/* Gamin
 * Copyright (C) 2003 James Willcox, Corey Bowers
 * Copyright (C) 2004 Daniel Veillard
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
#include "gam_poll_dnotify.h"
#include "gam_event.h"
#include "gam_server.h"
#include "gam_protocol.h"
#include "gam_event.h"
#include "gam_excludes.h"

#define VERBOSE_POLL
#define VERBOSE_POLL2

static gboolean gam_poll_dnotify_add_subscription(GamSubscription * sub);
static gboolean gam_poll_dnotify_remove_subscription(GamSubscription * sub);
static gboolean gam_poll_dnotify_remove_all_for(GamListener * listener);
static GaminEventType gam_poll_dnotify_poll_file(GamNode * node);
static gboolean gam_poll_dnotify_scan_callback(gpointer data);


gboolean
gam_poll_dnotify_init ()
{
	gam_poll_generic_init ();
	gam_server_install_poll_hooks (GAMIN_P_DNOTIFY,
				       gam_poll_dnotify_add_subscription,
				       gam_poll_dnotify_remove_subscription,
				       gam_poll_dnotify_remove_all_for,
				       gam_poll_dnotify_poll_file);

	g_timeout_add(1000, gam_poll_dnotify_scan_callback, NULL);
	return TRUE;
}

/**
 * gam_poll_delist_node:
 * @node: the node to delist
 *
 * This function is called when kernel monitoring for a node should
 * be turned off.
 */
static void
gam_poll_dnotify_delist_node(GamNode * node)
{
	GList *subs;
	const char *path;

	path = gam_node_get_path(node);

	if (gam_exclude_check(path) || gam_fs_get_mon_type (path) != GFS_MT_KERNEL)
		return;

	GAM_DEBUG(DEBUG_INFO, "poll-dnotify: Disabling kernel monitoring for %s\n", path);

	subs = gam_node_get_subscriptions(node);
	while (subs != NULL) {
		gam_poll_generic_trigger_handler (path, GAMIN_DEACTIVATE, node);
		subs = subs->next;
	}
}

/**
 * gam_poll_relist_node:
 * @node: the node to delist
 *
 * This function is called when kernel monitoring for a node should
 * be turned on (again).
 */
static void
gam_poll_dnotify_relist_node(GamNode * node)
{
	GList *subs;
	const char *path;

	path = gam_node_get_path(node);
	GAM_DEBUG(DEBUG_INFO, "poll-dnotify: Enabling kernel monitoring for %s\n", path);

	if (gam_exclude_check(path) || gam_fs_get_mon_type(path) != GFS_MT_KERNEL)
		return;

	subs = gam_node_get_subscriptions(node);

	while (subs != NULL) {
		gam_poll_generic_trigger_handler (path, GAMIN_ACTIVATE, node);
		subs = subs->next;
	}
}

/**
 * gam_poll_flowon_node:
 * @node: the node to delist
 *
 * This function is called when kernel monitoring flow control for a
 * node should be started
 */
static void
gam_poll_dnotify_flowon_node(GamNode * node)
{
	const char *path;

	path = gam_node_get_path(node);

	if (gam_exclude_check(path) || gam_fs_get_mon_type(path) != GFS_MT_KERNEL)
		return;

	GAM_DEBUG(DEBUG_INFO, "poll-dnotify: Enabling flow control for %s\n", path);

	gam_poll_generic_trigger_handler (path, GAMIN_FLOWCONTROLSTART, node);
}

/**
 * gam_poll_flowoff_node:
 * @node: the node to delist
 *
 * This function is called when kernel monitoring flow control for a
 * node should be started
 */
static void
gam_poll_dnotify_flowoff_node(GamNode * node)
{
	const char *path;

	path = gam_node_get_path(node);

	if (gam_exclude_check(path) || gam_fs_get_mon_type(path) != GFS_MT_KERNEL)
		return;

	GAM_DEBUG(DEBUG_INFO, "poll-dnotify: Disabling flow control for %s\n", path);

	gam_poll_generic_trigger_handler (path, GAMIN_FLOWCONTROLSTOP, node);
}

static GaminEventType
gam_poll_dnotify_poll_file(GamNode * node)
{
    GaminEventType event;
    struct stat sbuf;
    int stat_ret;
    const char *path;

    /* If not enough time has passed since the last time we polled this node, stop here */
    if (node->lasttime && gam_poll_generic_get_delta_time (node->lasttime) < node->poll_time)
        return 0;

    path = gam_node_get_path(node);
#ifdef VERBOSE_POLL
    GAM_DEBUG(DEBUG_INFO, "Poll: poll_file for %s called\n", path);
#endif

    memset(&sbuf, 0, sizeof(struct stat));
    if (node->lasttime == 0) {
        GAM_DEBUG(DEBUG_INFO, "Poll: file is new\n");
        stat_ret = stat(node->path, &sbuf);
        if (stat_ret != 0)
            gam_node_set_pflag (node, MON_MISSING);
        else
            gam_node_set_is_dir(node, (S_ISDIR(sbuf.st_mode) != 0));

        if (gam_exclude_check(path) || gam_fs_get_mon_type (path) != GFS_MT_KERNEL)
            gam_node_set_pflag (node, MON_NOKERNEL);

        memcpy(&(node->sbuf), &(sbuf), sizeof(struct stat));
        node->lasttime = gam_poll_generic_get_time ();

        if (stat_ret == 0)
            return 0;
        else
            return GAMIN_EVENT_DELETED;
    }

#ifdef VERBOSE_POLL
    GAM_DEBUG(DEBUG_INFO, " at %d delta %d : %d\n", gam_poll_generic_get_time(), gam_poll_generic_get_time() - node->lasttime, node->checks);
#endif

    event = 0;

    stat_ret = stat(node->path, &sbuf);
    if (stat_ret != 0) {
        if ((gam_errno() == ENOENT) && (!gam_node_has_pflag(node, MON_MISSING))) {
            /* deleted */
            gam_node_set_pflags (node, MON_MISSING);

            gam_poll_generic_remove_busy(node);
            if (gam_node_get_subscriptions(node) != NULL) {
                gam_poll_dnotify_delist_node(node);
                gam_poll_generic_add_missing(node);
            }
            event = GAMIN_EVENT_DELETED;
        }
    } else if (gam_node_has_pflag (node, MON_MISSING)) {
        /* created */
        gam_node_unset_pflag (node, MON_MISSING);
        event = GAMIN_EVENT_CREATED;
#ifdef ST_MTIM_NSEC
    } else if ((node->sbuf.st_mtim.tv_sec != sbuf.st_mtim.tv_sec) ||
           (node->sbuf.st_mtim.tv_nsec != sbuf.st_mtim.tv_nsec) ||
           (node->sbuf.st_size != sbuf.st_size) ||
           (node->sbuf.st_ctim.tv_sec != sbuf.st_ctim.tv_sec) ||
           (node->sbuf.st_ctim.tv_nsec != sbuf.st_ctim.tv_nsec))
    {
        event = GAMIN_EVENT_CHANGED;
    } else {
#ifdef VERBOSE_POLL
        GAM_DEBUG(DEBUG_INFO, "Poll: poll_file %s unchanged\n", path);
        GAM_DEBUG(DEBUG_INFO, "%d %d : %d %d\n", node->sbuf.st_mtim.tv_sec, node->sbuf.st_mtim.tv_nsec, sbuf.st_mtim.tv_sec, sbuf.st_mtim.tv_nsec);
#endif
#else
    } else if ((node->sbuf.st_mtime != sbuf.st_mtime) ||
           (node->sbuf.st_size != sbuf.st_size) ||
           (node->sbuf.st_ctime != sbuf.st_ctime))
    {
        event = GAMIN_EVENT_CHANGED;
#ifdef VERBOSE_POLL
        GAM_DEBUG(DEBUG_INFO, "%d : %d\n", node->sbuf.st_mtime, sbuf.st_mtime);
#endif
#endif
    }

    /*
    * TODO: handle the case where a file/dir is removed and replaced by
    *       a dir/file
    */
    if (stat_ret == 0)
        gam_node_set_is_dir(node, (S_ISDIR(sbuf.st_mode) != 0));

    memcpy(&(node->sbuf), &(sbuf), sizeof(struct stat));
    node->sbuf.st_mtime = sbuf.st_mtime; // VALGRIND!

    /*
    * if kernel monitoring prohibited, stop here
    */
    if (gam_node_has_pflag (node, MON_NOKERNEL))
        return (event);

    /*
    * load control, switch back to poll on very busy resources
    * and back when no update has happened in 5 seconds
    */
    if (gam_poll_generic_get_time() == node->lasttime) {
        if (!gam_node_has_pflag (node, MON_BUSY)) {
            if (node->sbuf.st_mtime == gam_poll_generic_get_time())
                node->checks++;
        }
    } else {
        node->lasttime = gam_poll_generic_get_time();
        if (gam_node_has_pflag (node, MON_BUSY)) {
            if (event == 0)
                node->checks++;
        } else {
            node->checks = 0;
        }
    }

    if ((node->checks >= 4) && (!gam_node_has_pflag (node, MON_BUSY))) {
        if ((gam_node_get_subscriptions(node) != NULL) &&
            (!gam_exclude_check(node->path) && gam_fs_get_mon_type (node->path) == GFS_MT_KERNEL))
        {
            GAM_DEBUG(DEBUG_INFO, "switching %s back to polling\n", path);
            gam_node_set_pflag (node, MON_BUSY);
            node->checks = 0;
            gam_poll_generic_add_busy(node);
            gam_poll_dnotify_flowon_node(node);
            /*
            * DNotify can be nasty here, we will miss events for parent dir
            * if we are not careful about it
            */
            if (!gam_node_is_dir(node)) {
                GamNode *parent = gam_node_parent(node);

                if ((parent != NULL) &&
                    (gam_node_get_subscriptions(parent) != NULL))
                {
                    gam_poll_generic_add_busy(parent);
                    /* gam_poll_flowon_node(parent); */
                }
            }
        }
    }

    if ((event == 0) && gam_node_has_pflag (node, MON_BUSY) && (node->checks > 5))
    {
        if ((gam_node_get_subscriptions(node) != NULL) &&
            (!gam_exclude_check(node->path) && gam_fs_get_mon_type (node->path) == GFS_MT_KERNEL))
        {
            GAM_DEBUG(DEBUG_INFO, "switching %s back to kernel monitoring\n", path);
            gam_node_unset_pflag (node, MON_BUSY);
            node->checks = 0;
            gam_poll_generic_remove_busy(node);
            gam_poll_dnotify_flowoff_node(node);
        }
    }

    return (event);
}

/**
 * node_add_subscription:
 * @node: the node tree pointer
 * @sub: the pointer to the subscription
 *
 * register a subscription for this node
 *
 * Returns 0 in case of success and -1 in case of failure
 */
static int
node_add_subscription(GamNode * node, GamSubscription * sub)
{
    if ((node == NULL) || (sub == NULL))
        return (-1);

    if ((node->path == NULL) || (node->path[0] != '/'))
        return (-1);

    GAM_DEBUG(DEBUG_INFO, "node_add_subscription(%s)\n", node->path);
    gam_node_add_subscription(node, sub);

    if (gam_exclude_check(node->path) || gam_fs_get_mon_type (node->path) == GFS_MT_POLL) {
        GAM_DEBUG(DEBUG_INFO, "  gam_exclude_check: true\n");
        if (node->lasttime == 0)
            gam_poll_dnotify_poll_file(node);

        gam_poll_generic_add_missing(node);
        return (0);
    }

	gam_poll_generic_trigger_handler (node->path, GAMIN_ACTIVATE, node);

    return (0);
}

/**
 * node_remove_subscription:
 * @node: the node tree pointer
 * @sub: the pointer to the subscription
 *
 * Removes a subscription for this node
 *
 * Returns 0 in case of success and -1 in case of failure
 */

static int
node_remove_subscription(GamNode * node, GamSubscription * sub)
{
    const char *path;

    if ((node == NULL) || (sub == NULL))
        return (-1);

    if ((node->path == NULL) || (node->path[0] != '/'))
        return (-1);

    GAM_DEBUG(DEBUG_INFO, "node_remove_subscription(%s)\n", node->path);

    gam_node_remove_subscription(node, sub);

    path = node->path;
    if (gam_exclude_check(path) || gam_fs_get_mon_type (path) == GFS_MT_POLL) {
        GAM_DEBUG(DEBUG_INFO, "  gam_exclude_check: true\n");
        return (0);
    }

    if (node->pflags == MON_BUSY) {
        GAM_DEBUG(DEBUG_INFO, "  node is busy\n");
    } else if (gam_node_has_pflags (node, MON_ALL_PFLAGS)) {
        GAM_DEBUG(DEBUG_INFO, "  node has flag %d\n", node->pflags);
        return (0);
    }

    /* DNotify makes our life miserable here */
	gam_poll_generic_trigger_handler (node->path, GAMIN_DEACTIVATE, node);

    return (0);
}

static gboolean
node_remove_directory_subscription(GamNode * node, GamSubscription * sub)
{
    GList *children, *l;
    gboolean remove_dir;

    GAM_DEBUG(DEBUG_INFO, "remove_directory_subscription %s\n",
              gam_node_get_path(node));

    node_remove_subscription(node, sub);

    remove_dir = (gam_node_get_subscriptions(node) == NULL);

    children = gam_tree_get_children(gam_poll_generic_get_tree(), node);
    for (l = children; l; l = l->next) {
        GamNode *child = (GamNode *) l->data;

        if ((!gam_node_get_subscriptions(child)) && (remove_dir) &&
            (!gam_tree_has_children(gam_poll_generic_get_tree(), child))) {
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
    remove_dir = ((gam_node_get_subscriptions(node) == NULL) &&
                  (!gam_node_has_dir_subscriptions
                   (gam_node_parent(node))));

    if (remove_dir) {
        GAM_DEBUG(DEBUG_INFO, "  => remove_dir %s\n",
                  gam_node_get_path(node));
    }
    return remove_dir;
}


/**
 * Adds a subscription to be polled.
 *
 * @param sub a #GamSubscription to be polled
 * @returns TRUE if adding the subscription succeeded, FALSE otherwise
 */
static gboolean
gam_poll_dnotify_add_subscription(GamSubscription * sub)
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

    if (node_add_subscription(node, sub) < 0)
    {
        gam_error(DEBUG_INFO, "Failed to add subscription for: %s\n", path);
        return FALSE;
    }

    node_is_dir = gam_node_is_dir(node);
    if (node_is_dir)
    {
        gam_poll_generic_first_scan_dir(sub, node, path);
    } else {
        GaminEventType event;

        event = gam_poll_dnotify_poll_file (node);
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
    if (gam_node_has_pflag (node, MON_MISSING) || gam_node_has_pflag (node, MON_NOKERNEL))
        gam_poll_generic_add_missing(node);

    if (!node_is_dir) {
        char *parent;
        parent = g_path_get_dirname(path);
        node = gam_tree_get_at_path(gam_poll_generic_get_tree(), parent);
        if (!node)
        {
            node = gam_tree_add_at_path(gam_poll_generic_get_tree(), parent, gam_subscription_is_dir (sub));
        }
        g_free(parent);
    }

	gam_poll_generic_add (node);

    GAM_DEBUG(DEBUG_INFO, "Poll: added subscription\n");
    return TRUE;
}

/**
 * gam_poll_remove_subscription_real:
 * @sub: a subscription
 *
 * Implements the removal of a subscription, including
 * trimming the tree and deactivating the kernel back-end if needed.
 */
static void
gam_poll_dnotify_remove_subscription_real(GamSubscription * sub)
{
    GamNode *node;

    node = gam_tree_get_at_path(gam_poll_generic_get_tree(), gam_subscription_get_path(sub));

    if (node != NULL) {
        if (!gam_node_is_dir(node)) {
            GAM_DEBUG(DEBUG_INFO, "Removing node sub: %s\n",
                      gam_subscription_get_path(sub));
            node_remove_subscription(node, sub);

            if (!gam_node_get_subscriptions(node)) {
                GamNode *parent;

                gam_poll_generic_unregister_node (node);
                if (gam_tree_has_children(gam_poll_generic_get_tree(), node)) {
                    fprintf(stderr,
                            "node %s is not dir but has children\n",
                            gam_node_get_path(node));
                } else {
                    parent = gam_node_parent(node);
                    if ((parent != NULL) &&
                        (!gam_node_has_dir_subscriptions(parent))) {
                        gam_tree_remove(gam_poll_generic_get_tree(), node);

                        gam_poll_generic_prune_tree(parent);
                    }
                }
            }
        } else {
            GAM_DEBUG(DEBUG_INFO, "Removing directory sub: %s\n",
                      gam_subscription_get_path(sub));
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
    }

    gam_subscription_free(sub);
}

/**
 * Removes a subscription which was being polled.
 *
 * @param sub a #GamSubscription to remove
 * @returns TRUE if removing the subscription succeeded, FALSE otherwise
 */
static gboolean
gam_poll_dnotify_remove_subscription(GamSubscription * sub)
{
    GamNode *node;

    node = gam_tree_get_at_path(gam_poll_generic_get_tree(), gam_subscription_get_path(sub));
    if (node == NULL) {
        /* free directly */
        gam_subscription_free(sub);
        return TRUE;
    }

    gam_subscription_cancel(sub);

    GAM_DEBUG(DEBUG_INFO, "Tree has %d nodes\n", gam_tree_get_size(gam_poll_generic_get_tree()));
    gam_poll_dnotify_remove_subscription_real(sub);
    GAM_DEBUG(DEBUG_INFO, "Tree has %d nodes\n", gam_tree_get_size(gam_poll_generic_get_tree()));

    GAM_DEBUG(DEBUG_INFO, "Poll: removed subscription\n");
    return TRUE;
}

/**
 * Stop polling all subscriptions for a given #GamListener.
 *
 * @param listener a #GamListener
 * @returns TRUE if removing the subscriptions succeeded, FALSE otherwise
 */
static gboolean
gam_poll_dnotify_remove_all_for(GamListener * listener)
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
gam_poll_dnotify_scan_callback(gpointer data)
{
    int idx;

#ifdef VERBOSE_POLL
	GAM_DEBUG(DEBUG_INFO, "gam_poll_scan_callback(): %d missing, %d busy\n", g_list_length(gam_poll_generic_get_missing_list()), g_list_length(gam_poll_generic_get_busy_list()));
#endif

	gam_poll_generic_update_time ();


	/*
	 * do not simply walk the list as it may be modified in the callback
	 */
	for (idx = 0;; idx++)
	{
		GamNode *node = g_list_nth_data(gam_poll_generic_get_missing_list(), idx);

		if (node == NULL) 
			break;

		g_assert (node);

#ifdef VERBOSE_POLL
		GAM_DEBUG(DEBUG_INFO, "Checking missing file %s", node->path);
#endif
		if (node->is_dir) {
			gam_poll_generic_scan_directory_internal(node);
		} else {
			GaminEventType event = gam_poll_dnotify_poll_file (node);
			gam_node_emit_event(node, event);
		}

		/*
		* if the resource exists again and is not in a special monitoring
		* mode then switch back to dnotify for monitoring.
		*/
		if (!gam_node_has_pflags (node, MON_ALL_PFLAGS) && 
		    !gam_exclude_check(node->path) && 
		    gam_fs_get_mon_type (node->path) == GFS_MT_KERNEL)
		{
			gam_poll_generic_remove_missing(node);
			if (gam_node_get_subscriptions(node) != NULL) {
				gam_poll_dnotify_relist_node(node);
			}
		}
	}

	for (idx = 0;; idx++)
	{
		GamNode *node = (GamNode *) g_list_nth_data(gam_poll_generic_get_busy_list(), idx);
		/*
		 * do not simply walk the list as it may be modified in the callback
		 */
		if (node == NULL)
			break;

		g_assert (node);
#ifdef VERBOSE_POLL
		GAM_DEBUG(DEBUG_INFO, "Checking busy file %s", node->path);
#endif
		if (node->is_dir) {
			gam_poll_generic_scan_directory_internal(node);
		} else {
			GaminEventType event = gam_poll_dnotify_poll_file (node);
			gam_node_emit_event(node, event);
		}

		/*
		* if the resource exists again and is not in a special monitoring
		* mode then switch back to dnotify for monitoring.
		*/
		if (!gam_node_has_pflags (node, MON_ALL_PFLAGS) && 
		    !gam_exclude_check(node->path) && 
		    gam_fs_get_mon_type (node->path) == GFS_MT_KERNEL)
		{
			gam_poll_generic_remove_busy(node);
			if (gam_node_get_subscriptions(node) != NULL) {
				gam_poll_dnotify_flowoff_node(node);
			}
		}
	}
	return TRUE;
}
