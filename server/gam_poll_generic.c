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
#include <string.h>
#include <glib.h>
#include "fam.h"
#include "gam_error.h"
#include "gam_tree.h"
#include "gam_poll_generic.h"
#include "gam_event.h"
#include "gam_server.h"
#include "gam_protocol.h"
#include "gam_event.h"
#include "gam_excludes.h"

//#define VERBOSE_POLL
//#define VERBOSE_POLL2

#define DEFAULT_POLL_TIMEOUT 1

static GamTree *	tree = NULL;
static GList *		missing_resources = NULL;
static GList *		busy_resources = NULL;
static GList *		all_resources = NULL;
static GList *		dead_resources = NULL;
static time_t		current_time = 0;

gboolean
gam_poll_generic_init()
{
	tree = gam_tree_new ();
	gam_poll_generic_update_time ();
	return TRUE;
}

static void
gam_poll_debug_node(GamNode * node, gpointer user_data)
{
    if (node == NULL)
        return;

    GAM_DEBUG(DEBUG_INFO, "dir %d flags %d pflags %d nb subs %d : %s\n", node->is_dir, node->flags, node->pflags, g_list_length(node->subs), node->path);
}

void
gam_poll_generic_debug(void)
{
    if (missing_resources != NULL) {
        GAM_DEBUG(DEBUG_INFO, "Dumping poll missing resources\n");
        g_list_foreach(missing_resources, (GFunc) gam_poll_debug_node, NULL);
    } else {
        GAM_DEBUG(DEBUG_INFO, "No poll missing resources\n");
    }

    if (busy_resources != NULL) {
        GAM_DEBUG(DEBUG_INFO, "Dumping poll busy resources\n");
        g_list_foreach(busy_resources, (GFunc) gam_poll_debug_node, NULL);
    } else {
        GAM_DEBUG(DEBUG_INFO, "No poll busy resources\n");
    }

    if (all_resources != NULL) {
        GAM_DEBUG(DEBUG_INFO, "Dumping poll all resources\n");
        g_list_foreach(all_resources, (GFunc) gam_poll_debug_node, NULL);
    } else {
        GAM_DEBUG(DEBUG_INFO, "No poll all resources\n");
    }
}

/**
 * gam_poll_generic_add_missing:
 * @node: a missing node
 *
 * Add a missing node to the list for polling its creation.
 */
void
gam_poll_generic_add_missing(GamNode * node)
{
	if (g_list_find(missing_resources, node) == NULL) {
		missing_resources = g_list_prepend(missing_resources, node);
		GAM_DEBUG(DEBUG_INFO, "Poll: adding missing node %s\n", gam_node_get_path(node));
	}
}



/**
 * gam_poll_generic_remove_missing:
 * @node: a missing node
 *
 * Remove a missing node from the list.
 */
void
gam_poll_generic_remove_missing(GamNode * node)
{
	if (g_list_find (missing_resources, node))
	{
		GAM_DEBUG(DEBUG_INFO, "Poll: removing missing node %s\n", gam_node_get_path(node));
		missing_resources = g_list_remove_all(missing_resources, node);
	}
}

/**
 * gam_poll_generic_add_busy:
 * @node: a busy node
 *
 * Add a busy node to the list for polling its creation.
 */
void
gam_poll_generic_add_busy(GamNode * node)
{
	if (g_list_find(busy_resources, node) == NULL) {
		busy_resources = g_list_prepend(busy_resources, node);
		GAM_DEBUG(DEBUG_INFO, "Poll: adding busy node %s\n", gam_node_get_path(node));
	}
}

/**
 * gam_poll_generic_remove_busy:
 * @node: a busy node
 *
 * Remove a busy node from the list.
 */
void
gam_poll_generic_remove_busy(GamNode * node)
{
	if (!g_list_find (busy_resources, node))
		return;

	GAM_DEBUG(DEBUG_INFO, "Poll: removing busy node %s\n", gam_node_get_path(node));
	busy_resources = g_list_remove_all(busy_resources, node);
}

void
gam_poll_generic_add (GamNode * node)
{
	if (g_list_find (all_resources, node) == NULL)
	{
		all_resources = g_list_prepend(all_resources, node);
		GAM_DEBUG(DEBUG_INFO, "Poll: Adding node %s\n", gam_node_get_path (node));
	}
}

void
gam_poll_generic_remove (GamNode * node)
{
	g_assert (g_list_find (all_resources, node));
	GAM_DEBUG(DEBUG_INFO, "Poll: removing node %s\n", gam_node_get_path(node));
	all_resources = g_list_remove_all(all_resources, node);
}

time_t
gam_poll_generic_get_time()
{
	return current_time;
}

void
gam_poll_generic_update_time()
{
	current_time = time (NULL);
}

time_t
gam_poll_generic_get_delta_time(time_t pt)
{
	if (current_time >= pt)
		return current_time - pt;
	/* FIXME: We have wrapped */
	return 0;
}

static void
gam_poll_generic_trigger_file_handler (const char *path, pollHandlerMode mode, GamNode *node)
{
    if (node->mon_type != GFS_MT_KERNEL)
        return;

	if (gam_server_get_kernel_handler() == GAMIN_K_DNOTIFY || gam_server_get_kernel_handler() == GAMIN_K_INOTIFY) {
		if (gam_node_is_dir(node)) {
			gam_kernel_file_handler (path, mode);
		} else {
			const char *dir = NULL;
			GamNode *parent = gam_node_parent(node);

			if (!parent)
				return;

			dir = parent->path;
			switch (mode) {
			case GAMIN_ACTIVATE:
				GAM_DEBUG(DEBUG_INFO, "poll: Activating kernel monitoring on %s\n", dir);
				gam_kernel_dir_handler (dir, mode);
			break;
			case GAMIN_DEACTIVATE:
				GAM_DEBUG(DEBUG_INFO, "poll: Deactivating kernel monitoring on %s\n", dir);
				gam_kernel_dir_handler (dir, mode);
			break;
			case GAMIN_FLOWCONTROLSTART:
				if (!gam_node_has_pflag (parent, MON_BUSY)) {
					GAM_DEBUG(DEBUG_INFO, "poll: marking busy on %s\n", dir);
					gam_kernel_dir_handler (dir, mode);
					gam_poll_generic_add_busy(parent);
					gam_node_set_pflag (parent, MON_BUSY);
				}
			break;
			case GAMIN_FLOWCONTROLSTOP:
				if (gam_node_has_pflag (parent, MON_BUSY)) {
					GAM_DEBUG(DEBUG_INFO, "poll: unmarking busy on %s\n", dir);
					gam_kernel_dir_handler (dir, mode);
					gam_poll_generic_remove_busy(parent);
					gam_node_unset_pflag (parent, MON_BUSY);
				}
			break;
			}
		}
	} else {
		gam_kernel_file_handler (path, mode);
	}
}


static void
gam_poll_generic_trigger_dir_handler (const char *path, pollHandlerMode mode, GamNode *node)
{
	if (node->mon_type != GFS_MT_KERNEL)
		return;

	if (gam_server_get_kernel_handler() == GAMIN_K_DNOTIFY || gam_server_get_kernel_handler() == GAMIN_K_INOTIFY) {
		if (gam_node_is_dir(node)) {
			gam_kernel_dir_handler (path, mode);
		} else {
			gam_poll_generic_trigger_file_handler(path, mode, node);
		}
	} else {
		gam_kernel_dir_handler (path, mode);
	}
}


void
gam_poll_generic_trigger_handler(const char *path, pollHandlerMode mode, GamNode *node)
{
	if (gam_node_is_dir(node))
		gam_poll_generic_trigger_dir_handler(node->path, mode, node);
	else
		gam_poll_generic_trigger_file_handler(node->path, mode, node);
}

void
gam_poll_generic_scan_directory_internal (GamNode *dir_node)
{
	GDir *dir = NULL;
	const char *name = NULL, *dpath = NULL;
	char *path = NULL;
	GamNode *node = NULL;
	GaminEventType event = 0, fevent;
	GList *children = NULL, *l = NULL;
	unsigned int exists = 0;
	int is_dir_node;

	if (dir_node == NULL)
		return;

	dpath = gam_node_get_path(dir_node);

	if (dpath == NULL)
		return;

	if (!gam_node_get_subscriptions(dir_node))
		goto scan_files;

	if (dir_node->lasttime && gam_poll_generic_get_delta_time (dir_node->lasttime) < dir_node->poll_time)
		return;

	GAM_DEBUG(DEBUG_INFO, "poll-generic: scanning directory %s\n", dpath);

	event = gam_poll_file(dir_node);

	if (event != 0)
		gam_node_emit_event (dir_node, event);

	dir = g_dir_open(dpath, 0, NULL);

	if (dir == NULL) {
#ifdef VERBOSE_POLL
		GAM_DEBUG(DEBUG_INFO, "Poll: directory %s is not readable or missing\n", dpath);
#endif
		return;
	}

	exists = 1;

#ifdef VERBOSE_POLL
	GAM_DEBUG(DEBUG_INFO, "Poll: scanning directory %s\n", dpath);
#endif
	while ((name = g_dir_read_name(dir)) != NULL) {
		path = g_build_filename(gam_node_get_path(dir_node), name, NULL);
		node = gam_tree_get_at_path(tree, path);
		GAM_DEBUG(DEBUG_INFO, "poll-generic: scan dir - checking %s\n", dpath);

		if (!node) {
			if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
				node = gam_node_new(path, NULL, FALSE);
				gam_tree_add(tree, dir_node, node);
				gam_node_set_flag(node, FLAG_NEW_NODE);
			} else {
				node = gam_node_new(path, NULL, TRUE);
				gam_tree_add(tree, dir_node, node);
				gam_node_set_flag(node, FLAG_NEW_NODE);
			}
		}

		g_free(path);
	}

	g_dir_close(dir);

scan_files:
	/* FIXME: Shouldn't is_dir_node be assigned inside the loop? */
	children = gam_tree_get_children(tree, dir_node);
	for (l = children; l; l = l->next) {
		node = (GamNode *) l->data;
		is_dir_node = gam_node_is_dir(dir_node);

		fevent = gam_poll_file(node);

		if (gam_node_has_flag(node, FLAG_NEW_NODE)) {
			if (is_dir_node && gam_node_get_subscriptions(node)) {
				gam_node_unset_flag(node, FLAG_NEW_NODE);
				gam_poll_generic_scan_directory_internal(node);
			} else {
				gam_node_unset_flag(node, FLAG_NEW_NODE);
				fevent = GAMIN_EVENT_CREATED;
			}
		}

		if (fevent != 0) {
			gam_node_emit_event (node, fevent);
		} else {
			/* just send the EXIST events if the node exists */

			if (!gam_node_has_pflag (node, MON_MISSING))
			{
				gam_server_emit_event(gam_node_get_path(node),
				gam_node_is_dir(node),
				GAMIN_EVENT_EXISTS, NULL, 0);
			}
		}
	}

	g_list_free(children);
}

/**
 * Scans a directory for changes, and emits events if needed.
 *
 * @param path the path to the directory to be scanned
 */
void
gam_poll_generic_scan_directory(const char *path)
{
	GamNode *node;

	gam_poll_generic_update_time ();

	node = gam_tree_get_at_path(tree, path);
	if (node == NULL)
		node = gam_tree_add_at_path(tree, path, TRUE);

	if (node == NULL) {
		gam_error(DEBUG_INFO, "gam_tree_add_at_path(%s) returned NULL\n", path);
		return;
	}

	gam_poll_generic_scan_directory_internal(node);
}

/**
 * First dir scanning on a new subscription, generates the Exists EndExists
 * events.
 */
void
gam_poll_generic_first_scan_dir (GamSubscription * sub, GamNode * dir_node, const char *dpath)
{
	GDir *dir;
	char *path;
	GList *subs;
	int with_exists = 1;
	const char *name;
	GamNode *node;

	GAM_DEBUG(DEBUG_INFO, "Looking for existing files in: %s\n", dpath);

	if (gam_subscription_has_option(sub, GAM_OPT_NOEXISTS))
	{
		with_exists = 0;
		GAM_DEBUG(DEBUG_INFO, "   Exists not wanted\n");
	}

	subs = g_list_prepend(NULL, sub);

	if (!g_file_test(dpath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		GAM_DEBUG(DEBUG_INFO, "Monitoring missing dir: %s\n", dpath);

		gam_server_emit_event(dpath, 1, GAMIN_EVENT_DELETED, subs, 1);

		stat(dir_node->path, &(dir_node->sbuf));
		dir_node->lasttime = gam_poll_generic_get_time ();

		if (g_file_test(dpath, G_FILE_TEST_EXISTS)) {
			gam_node_set_pflags (dir_node, MON_WRONG_TYPE);
			dir_node->is_dir = 0;
		} else {
			gam_node_set_pflags (dir_node, MON_MISSING);
			gam_poll_generic_add_missing(dir_node);
		}
		goto done;
	}

	if (dir_node->lasttime == 0)
		gam_poll_file(dir_node);

	if (with_exists)
		gam_server_emit_event(dpath, 1, GAMIN_EVENT_EXISTS, subs, 1);


	dir = g_dir_open(dpath, 0, NULL);

	if (dir == NULL) {
		goto done;
	}

	while ((name = g_dir_read_name(dir)) != NULL)
	{
		path = g_build_filename(dpath, name, NULL);

		node = gam_tree_get_at_path(tree, path);

		if (!node)
		{
			GAM_DEBUG(DEBUG_INFO, "Unregistered node %s\n", path);
			if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
				node = gam_node_new(path, NULL, FALSE);
			} else {
				node = gam_node_new(path, NULL, TRUE);
			}
			stat(node->path, &(node->sbuf));
			gam_node_set_is_dir(node, (S_ISDIR(node->sbuf.st_mode) != 0));

			if (gam_exclude_check(path) || gam_fs_get_mon_type(path) != GFS_MT_KERNEL)
				gam_node_set_pflag (node, MON_NOKERNEL);

			node->lasttime = gam_poll_generic_get_time ();
			gam_tree_add(tree, dir_node, node);
		}

		if (with_exists)
			gam_server_emit_event(name, 1, GAMIN_EVENT_EXISTS, subs, 1);

		g_free(path);
	}

	g_dir_close(dir);

done:
	if (with_exists)
		gam_server_emit_event(dpath, 1, GAMIN_EVENT_ENDEXISTS, subs, 1);

	g_list_free(subs);

	GAM_DEBUG(DEBUG_INFO, "Done scanning %s\n", dpath);
}

GamTree *
gam_poll_generic_get_tree()
{
	return tree;
}

GList *
gam_poll_generic_get_missing_list (void)
{
	return missing_resources;
}

GList *
gam_poll_generic_get_busy_list (void)
{
	return busy_resources;
}

GList *
gam_poll_generic_get_all_list (void)
{
	return all_resources;
}

GList *
gam_poll_generic_get_dead_list (void)
{
	return dead_resources;
}

void
gam_poll_generic_unregister_node (GamNode * node)
{
	if (missing_resources != NULL) {
		gam_poll_generic_remove_missing(node);
	}

	if (busy_resources != NULL) {
		gam_poll_generic_remove_busy(node);
	}

	if (all_resources != NULL) {
		all_resources = g_list_remove(all_resources, node);
	}
}

void
gam_poll_generic_prune_tree(GamNode * node)
{
	/* don't prune the root */
	if (gam_node_parent(node) == NULL)
		return;

	if (!gam_tree_has_children(tree, node) && !gam_node_get_subscriptions(node)) 
	{
		GamNode *parent;

		GAM_DEBUG(DEBUG_INFO, "prune_tree: %s\n", gam_node_get_path(node));

		parent = gam_node_parent(node);
		gam_poll_generic_unregister_node(node);
		gam_tree_remove(tree, node);
		gam_poll_generic_prune_tree(parent);
	}
}

