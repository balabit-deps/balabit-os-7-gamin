/* Gamin
 * Copyright (C) 2003 James Willcox, Corey Bowers
 * Copyright (C) 2004 Daniel Veillard
 * Copyright (C) 2005 John McCutchan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "server_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include "fam.h"
#include "gam_error.h"
#include "gam_tree.h"
#include "gam_poll_basic.h"
#include "gam_event.h"
#include "gam_server.h"
#include "gam_protocol.h"
#include "gam_event.h"
#include "gam_excludes.h"

#define VERBOSE_POLL

static gboolean gam_poll_basic_add_subscription(GamSubscription * sub);
static gboolean gam_poll_basic_remove_subscription(GamSubscription * sub);
static gboolean gam_poll_basic_remove_all_for(GamListener * listener);
static GaminEventType gam_poll_basic_poll_file(GamNode * node);
static gboolean gam_poll_basic_scan_callback(gpointer data);

static gboolean scan_callback_running = FALSE;

gboolean
gam_poll_basic_init ()
{
	gam_poll_generic_init ();
	gam_server_install_poll_hooks (GAMIN_P_BASIC,
				       gam_poll_basic_add_subscription,
				       gam_poll_basic_remove_subscription,
				       gam_poll_basic_remove_all_for,
				       gam_poll_basic_poll_file);

	GAM_DEBUG(DEBUG_INFO, "basic poll backend initialized\n");
	return TRUE;
}

/**
 * Adds a subscription to be polled.
 *
 * @param sub a #GamSubscription to be polled
 * @returns TRUE if adding the subscription succeeded, FALSE otherwise
 */
static gboolean
gam_poll_basic_add_subscription(GamSubscription * sub)
{
	const char *path = gam_subscription_get_path (sub);
	GamNode *node = gam_tree_get_at_path (gam_poll_generic_get_tree(), path);
	int node_is_dir = FALSE;

	gam_listener_add_subscription(gam_subscription_get_listener(sub), sub);
	gam_poll_generic_update_time ();

	if (!node)
	{
		node = gam_tree_add_at_path(gam_poll_generic_get_tree(), path, gam_subscription_is_dir(sub));
	}

	gam_node_add_subscription(node, sub);
	node_is_dir = gam_node_is_dir(node);

	if (node_is_dir)
	{
		gam_poll_generic_first_scan_dir(sub, node, path);
	} else {
		GaminEventType event;

		event = gam_poll_basic_poll_file (node);
		GAM_DEBUG(DEBUG_INFO, "New file subscription: %s event %d\n", path, event);

		if ((event == 0) || (event == GAMIN_EVENT_EXISTS) ||
		    (event == GAMIN_EVENT_CHANGED) ||
		    (event == GAMIN_EVENT_CREATED))
		{
			if (gam_subscription_is_dir(sub)) {
				/* we are watching a file but requested a directory */
				gam_server_emit_one_event(path, node_is_dir, GAMIN_EVENT_DELETED, sub, 0);
			} else {
				gam_server_emit_one_event(path, node_is_dir, GAMIN_EVENT_EXISTS, sub, 0);
			}
		} else if (event != 0) {
			gam_server_emit_one_event(path, node_is_dir, GAMIN_EVENT_DELETED, sub, 0);
		}

		gam_server_emit_one_event(path, node_is_dir, GAMIN_EVENT_ENDEXISTS, sub, 0);
	}

	if (gam_node_has_pflag (node, MON_MISSING))
		gam_poll_generic_add_missing(node);

	gam_poll_generic_add (node);

	if (!scan_callback_running)
	{
	  scan_callback_running = TRUE;
	  g_timeout_add (1000, gam_poll_basic_scan_callback, NULL);
	}
	
	GAM_DEBUG(DEBUG_INFO, "Poll: added subscription for %s\n", path);
	return TRUE;
}

static gboolean
node_remove_directory_subscription(GamNode * node, GamSubscription * sub)
{
	GList *children, *l;
	gboolean remove_dir;

	GAM_DEBUG(DEBUG_INFO, "remove_directory_subscription %s\n", gam_node_get_path(node));

	gam_node_remove_subscription(node, sub);

	remove_dir = (gam_node_get_subscriptions(node) == NULL);

	children = gam_tree_get_children(gam_poll_generic_get_tree(), node);
	for (l = children; l; l = l->next) {
		GamNode *child = (GamNode *) l->data;

		if ((!gam_node_get_subscriptions(child)) && (remove_dir) &&
		    (!gam_tree_has_children(gam_poll_generic_get_tree(), child))) 
		{
			gam_poll_generic_unregister_node (child);

			gam_tree_remove(gam_poll_generic_get_tree(), child);
		} else {
			remove_dir = FALSE;
		}
	}

	g_list_free(children);

	/*
	* do not remove the directory if the parent has a directory subscription
	*/
	remove_dir = ((gam_node_get_subscriptions(node) == NULL) && (!gam_node_has_dir_subscriptions(gam_node_parent(node))));

	if (remove_dir) {
		GAM_DEBUG(DEBUG_INFO, "  => remove_dir %s\n",
		gam_node_get_path(node));
	}
	return remove_dir;
}

/**
 * Removes a subscription which was being polled.
 *
 * @param sub a #GamSubscription to remove
 * @returns TRUE if removing the subscription succeeded, FALSE otherwise
 */
static gboolean
gam_poll_basic_remove_subscription(GamSubscription * sub)
{
	const char *path = gam_subscription_get_path (sub);
	GamNode *node = gam_tree_get_at_path (gam_poll_generic_get_tree(), path);

	if (node == NULL) {
		/* free directly */
		gam_subscription_free(sub);
		return TRUE;
	}

	gam_subscription_cancel(sub);

#ifdef VERBOSE_POLL
	GAM_DEBUG(DEBUG_INFO, "Tree has %d nodes\n", gam_tree_get_size(gam_poll_generic_get_tree()));
#endif
	if (!gam_node_is_dir(node)) {
		GAM_DEBUG(DEBUG_INFO, "Removing node %s\n", gam_subscription_get_path(sub));
		gam_node_remove_subscription(node, sub);

		if (!gam_node_get_subscriptions(node)) 
		{
			GamNode *parent;
			gam_poll_generic_unregister_node (node);
			g_assert (!gam_tree_has_children(gam_poll_generic_get_tree(), node));
			parent = gam_node_parent(node);
			if ((parent != NULL) && (!gam_node_has_dir_subscriptions(parent))) {
				gam_tree_remove(gam_poll_generic_get_tree(), node);
				gam_poll_generic_prune_tree(parent);
			}
		}
	} else {
		GAM_DEBUG(DEBUG_INFO, "Removing node %s\n", gam_subscription_get_path(sub));
		if (node_remove_directory_subscription(node, sub)) {
			GamNode *parent;

			gam_poll_generic_unregister_node (node);
			parent = gam_node_parent(node);
			if (!gam_tree_has_children(gam_poll_generic_get_tree(), node)) {
				gam_tree_remove(gam_poll_generic_get_tree(), node);
			}

			gam_poll_generic_prune_tree(parent);
		}
	}

#ifdef VERBOSE_POLL
	GAM_DEBUG(DEBUG_INFO, "Tree has %d nodes\n", gam_tree_get_size(gam_poll_generic_get_tree()));
#endif

	GAM_DEBUG(DEBUG_INFO, "Poll: removed subscription for %s\n", path);

	gam_subscription_free(sub);
	return TRUE;
}

/**
 * Stop polling all subscriptions for a given #GamListener.
 *
 * @param listener a #GamListener
 * @returns TRUE if removing the subscriptions succeeded, FALSE otherwise
 */
static gboolean
gam_poll_basic_remove_all_for(GamListener * listener)
{
	GList *subs, *l = NULL;

	subs = gam_listener_get_subscriptions(listener);
	for (l = subs; l; l = l->next) {
		GamSubscription *sub = l->data;
		g_assert(sub != NULL);
		gam_poll_remove_subscription(sub);
	}

	if (subs) {
		g_list_free(subs);
		return TRUE;
	} else
		return FALSE;
}

static gboolean
gam_poll_generic_node_changed (GamNode *node, struct stat sbuf)
{
	g_assert(node);
#ifdef ST_MTIM_NSEC
	return ((node->sbuf.st_mtim.tv_sec != sbuf.st_mtim.tv_sec) ||
		(node->sbuf.st_mtim.tv_nsec != sbuf.st_mtim.tv_nsec) ||
		(node->sbuf.st_size != sbuf.st_size) ||
		(node->sbuf.st_ctim.tv_sec != sbuf.st_ctim.tv_sec) ||
		(node->sbuf.st_ctim.tv_nsec != sbuf.st_ctim.tv_nsec));
#else
	return ((node->sbuf.st_mtime != sbuf.st_mtime) ||
		(node->sbuf.st_size != sbuf.st_size) ||
		(node->sbuf.st_ctime != sbuf.st_ctime));
#endif
}

static GaminEventType
gam_poll_basic_poll_file(GamNode * node)
{
	GaminEventType event;
	struct stat sbuf;
	int stat_ret;
	const char *path = NULL;

	g_assert (node);

	path = gam_node_get_path(node);
	/* If not enough time has passed since the last time we polled this node, stop here */
	if (node->lasttime && gam_poll_generic_get_delta_time (node->lasttime) < node->poll_time)
	{
		GAM_DEBUG(DEBUG_INFO, "poll-basic: not enough time passed for %s\n", path);
		return 0;
	} else {
		GAM_DEBUG(DEBUG_INFO, "poll-basic: %d && %d < %d\n", node->lasttime, gam_poll_generic_get_delta_time (node->lasttime), node->poll_time);
	}

#ifdef VERBOSE_POLL
	GAM_DEBUG(DEBUG_INFO, "Poll: poll_file for %s called\n", path);
#endif
	memset(&sbuf, 0, sizeof(struct stat));
	if (node->lasttime == 0) 
	{
#ifdef VERBOSE_POLL
		GAM_DEBUG(DEBUG_INFO, "Poll: file is new\n");
#endif
		stat_ret = stat(node->path, &sbuf);

		if (stat_ret != 0)
			gam_node_set_pflag (node, MON_MISSING);
		else
			gam_node_set_is_dir(node, (S_ISDIR(sbuf.st_mode) != 0));

		memcpy(&(node->sbuf), &(sbuf), sizeof(struct stat));

		node->lasttime = gam_poll_generic_get_time ();

		if (stat_ret == 0)
			return 0;
		else
			return GAMIN_EVENT_DELETED;
	}

	event = 0;
	node->lasttime = gam_poll_generic_get_time ();

	stat_ret = stat(node->path, &sbuf);
	if (stat_ret != 0) {
		if ((gam_errno() == ENOENT) && (!gam_node_has_pflag(node, MON_MISSING))) {
			/* deleted */
			gam_node_set_pflags (node, MON_MISSING);

			gam_poll_generic_remove_busy(node);
			gam_poll_generic_add_missing(node);
			event = GAMIN_EVENT_DELETED;
		}
	} else if (gam_node_has_pflag (node, MON_MISSING)) {
		/* created */
		gam_node_unset_pflag (node, MON_MISSING);
		event = GAMIN_EVENT_CREATED;
		gam_poll_generic_remove_missing (node);
	} else if (gam_poll_generic_node_changed (node, sbuf)) {
		event = GAMIN_EVENT_CHANGED;
	} else {
#ifdef VERBOSE_POLL
		GAM_DEBUG(DEBUG_INFO, "Poll: poll_file %s unchanged\n", path);
#ifdef ST_MTIM_NSEC
		GAM_DEBUG(DEBUG_INFO, "%d %d : %d %d\n", node->sbuf.st_mtim.tv_sec, node->sbuf.st_mtim.tv_nsec, sbuf.st_mtim.tv_sec, sbuf.st_mtim.tv_nsec);
#else
		GAM_DEBUG(DEBUG_INFO, "%d : %d\n", node->sbuf.st_mtime, sbuf.st_mtim.tv_nsec);
#endif /* ST_MTIM_NSEC */
#endif /* VERBOSE_POLL */
	}

	/*
	* TODO: handle the case where a file/dir is removed and replaced by
	*       a dir/file
	*/
	if (stat_ret == 0)
		gam_node_set_is_dir(node, (S_ISDIR(sbuf.st_mode) != 0));

	memcpy(&(node->sbuf), &(sbuf), sizeof(struct stat));
	node->sbuf.st_mtime = sbuf.st_mtime; // VALGRIND!

	return event;
}

static gboolean
gam_poll_basic_scan_callback(gpointer data)
{
	int idx;
	gboolean did_something = FALSE;

	gam_poll_generic_update_time ();

	for (idx = 0;; idx++) 
	{
		/*
		 * do not simply walk the list as it may be modified in the callback
		 */
		GamNode *node = (GamNode *) g_list_nth_data(gam_poll_generic_get_all_list(), idx);

		if (node == NULL)
			break;

		g_assert (node);

		did_something = TRUE;
		
		if (node->is_dir) {
			gam_poll_generic_scan_directory_internal(node);
		} else {
			GaminEventType event = gam_poll_basic_poll_file (node);
			gam_node_emit_event(node, event);
		}
	}

	/*
	* do not simply walk the list as it may be modified in the callback
	*/
	for (idx = 0;; idx++)
	{
		GamNode *node = g_list_nth_data(gam_poll_generic_get_missing_list(), idx);

		if (node == NULL)
			break;

		g_assert (node);

		did_something = TRUE;
		
#ifdef VERBOSE_POLL
		GAM_DEBUG(DEBUG_INFO, "Checking missing file %s\n", node->path);
#endif
		if (node->is_dir) {
			gam_poll_generic_scan_directory_internal(node);
		} else {
			GaminEventType event = gam_poll_basic_poll_file (node);
			gam_node_emit_event(node, event);
		}

		/*
		* if the resource exists again and is not in a special monitoring
		* mode then switch back to dnotify for monitoring.
		*/
		if (!gam_node_has_pflags (node, MON_MISSING)) 
		{
			gam_poll_generic_remove_missing(node);
			gam_poll_generic_add (node);
		}
	}

	if (!did_something) {
	  scan_callback_running = FALSE;
	  return FALSE;
	}
	
	return TRUE;
}
