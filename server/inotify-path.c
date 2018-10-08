/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* inotify-helper.c - Gnome VFS Monitor based on inotify.

   Copyright (C) 2006 John McCutchan

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: 
		 John McCutchan <john@johnmccutchan.com>
*/

#include "config.h"

#include <sys/inotify.h>

#include <errno.h>
#include <string.h>
#include <glib.h>
#include "inotify-kernel.h"
#include "inotify-path.h"
#include "inotify-missing.h"

#define IP_INOTIFY_MASK (IN_MODIFY|IN_ATTRIB|IN_MOVED_FROM|IN_MOVED_TO|IN_DELETE|IN_CREATE|IN_DELETE_SELF|IN_UNMOUNT|IN_MOVE_SELF)

typedef struct ip_watched_dir_s {
	char *path;
	/* TODO: We need to maintain a tree of watched directories
	 * so that we can deliver move/delete events to sub folders.
	 * Or the application could do it...
	 */
	struct ip_watched_dir_s *parent;
	GList *			 children;

	/* Inotify state */
	gint32 wd;

	/* List of inotify subscriptions */
	GList *subs;
} ip_watched_dir_t;

static gboolean     ip_debug_enabled = FALSE;
#define IP_W if (ip_debug_enabled) g_warning

/* path -> ip_watched_dir */
static GHashTable * path_dir_hash = NULL;
/* ih_sub_t * -> ip_watched_dir *
 *
 * Each subscription is attached to a watched directory or it is on
 * the missing list
 */
static GHashTable * sub_dir_hash = NULL;
/* This hash holds GLists of ip_watched_dir_t *'s
 * We need to hold a list because symbolic links can share
 * the same wd
 */
static GHashTable * wd_dir_hash = NULL;

static ip_watched_dir_t *	ip_watched_dir_new (const char *path, int wd);
static void 			ip_watched_dir_free (ip_watched_dir_t *dir);
static void 			ip_event_callback (ik_event_t *event);

static void (*event_callback)(ik_event_t *event, ih_sub_t *sub);

gboolean ip_startup (void (*cb)(ik_event_t *event, ih_sub_t *sub))
{
	static gboolean initialized = FALSE;
	static gboolean result = FALSE;

	if (initialized == TRUE) {
		return result;
	}

	event_callback = cb;
	result = ik_startup (ip_event_callback);

	if (!result) {
		return FALSE;
	}
	initialized = TRUE;

	path_dir_hash = g_hash_table_new(g_str_hash, g_str_equal);
	sub_dir_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	wd_dir_hash = g_hash_table_new(g_direct_hash, g_direct_equal);

	return TRUE;
}

static void
ip_map_path_dir (const char *path, ip_watched_dir_t *dir)
{
	g_assert (path && dir);
	g_hash_table_insert(path_dir_hash, dir->path, dir);
}

static void
ip_map_sub_dir (ih_sub_t *sub, ip_watched_dir_t *dir)
{
	/* Associate subscription and directory */
	g_assert (dir && sub);
	g_hash_table_insert (sub_dir_hash, sub, dir);
	dir->subs = g_list_prepend (dir->subs, sub);
}

static void
ip_map_wd_dir (gint32 wd, ip_watched_dir_t *dir)
{
	g_assert (wd >= 0 && dir);
	GList *dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER(wd));
	dir_list = g_list_prepend (dir_list, dir);
	g_hash_table_replace(wd_dir_hash, GINT_TO_POINTER(dir->wd), dir_list);
}

static gint32
ip_watch_parent (const char *path, guint mask, int *err, char **parent_path)
{
	gint32 wd;
	gchar *path_copy;
	gchar *p;

	path_copy = g_strdup (path);

        wd = -1;
        do
        {
                p = g_strrstr (path_copy, G_DIR_SEPARATOR_S);

                if (p == NULL)
                        return -1;

                if (p != &path_copy[0])
                        *p = '\0';

                wd = ik_watch (path_copy, mask, err);
        } 
        while (wd < 0 && errno == ENOENT);

        if (parent_path != NULL && wd >= 0)
                *parent_path = path_copy;
        else
                g_free (path_copy);

	return wd;
}

gboolean ip_start_watching (ih_sub_t *sub)
{
	gint32 wd;
	int err;
	ip_watched_dir_t *dir;

	g_assert (sub);
	g_assert (!sub->cancelled);
	g_assert (sub->dirname);

	IP_W("Starting to watch %s\n", sub->dirname);
	dir = g_hash_table_lookup (path_dir_hash, sub->dirname);
	if (dir)
	{
		IP_W("Already watching\n");
		goto out;
	}
	
	IP_W("Trying to add inotify watch ");
	wd = ik_watch (sub->dirname, IP_INOTIFY_MASK|IN_ONLYDIR|sub->extra_flags, &err);

        if (wd < 0 && errno == ENOENT)
        {
                char *parent_path;

		IP_W("Directory '%s' does not yet exist, finding and watching existing parent\n",
                     sub->dirname);

                parent_path = NULL;
		wd = ip_watch_parent (sub->dirname,
                                      IP_INOTIFY_MASK|IN_ONLYDIR|sub->extra_flags,
                                      &err, &parent_path);

		if (wd < 0)
		{
                        g_assert (parent_path == NULL);
			IP_W("Failed\n");
			return FALSE;
		}

		IP_W("Found parent '%s', will watch it for now until '%s' becomes available\n",
                     parent_path, sub->dirname);

                dir = ip_watched_dir_new (parent_path, wd);
                g_free (parent_path);

		ip_map_wd_dir (wd, dir);
		ip_map_path_dir (parent_path, dir);
        } else if (wd < 0) {
		IP_W("Failed\n");
		return FALSE;
	} else {
		/* Create new watched directory and associate it with the 
		 * wd hash and path hash
		 */
		IP_W("Success\n");
		dir = ip_watched_dir_new (sub->dirname, wd);
		ip_map_wd_dir (wd, dir);
		ip_map_path_dir (sub->dirname, dir);
	}

out:
	ip_map_sub_dir (sub, dir);

	return TRUE;
}

static void
ip_unmap_path_dir (const char *path, ip_watched_dir_t *dir)
{
	g_assert (path && dir);
	g_hash_table_remove (path_dir_hash, dir->path);
}

static void
ip_unmap_wd_dir (gint32 wd, ip_watched_dir_t *dir)
{
	GList *dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER(wd));
	if (!dir_list)
		return;
	g_assert (wd >= 0 && dir);
	dir_list = g_list_remove (dir_list, dir);
	if (dir_list == NULL) {
		g_hash_table_remove (wd_dir_hash, GINT_TO_POINTER(dir->wd));
	} else {
		g_hash_table_replace (wd_dir_hash, GINT_TO_POINTER(dir->wd), dir_list);
	}
}

static void
ip_unmap_wd (gint32 wd)
{
	GList *dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER(wd));
	if (!dir_list)
		return;
	g_assert (wd >= 0);
	g_hash_table_remove (wd_dir_hash, GINT_TO_POINTER(wd));
	g_list_free (dir_list);
}

static void
ip_unmap_sub_dir (ih_sub_t *sub, ip_watched_dir_t *dir)
{
	g_assert (sub && dir);
	g_hash_table_remove (sub_dir_hash, sub);
	dir->subs = g_list_remove (dir->subs, sub);
}

static void
ip_unmap_all_subs (ip_watched_dir_t *dir)
{
	GList *l = NULL;

	for (l = dir->subs; l; l = l->next)
	{
		ih_sub_t *sub = l->data;
		g_hash_table_remove (sub_dir_hash, sub);
	}
	g_list_free (dir->subs);
	dir->subs = NULL;
}

gboolean ip_stop_watching  (ih_sub_t *sub)
{
	ip_watched_dir_t *dir = NULL;

	dir = g_hash_table_lookup (sub_dir_hash, sub);
	if (!dir) {
		return TRUE;
	}

	ip_unmap_sub_dir (sub, dir);

	/* No one is subscribing to this directory any more */
	if (dir->subs == NULL) {
        ik_ignore (dir->path, dir->wd);
        ip_unmap_wd_dir (dir->wd, dir);
		ip_unmap_path_dir (dir->path, dir);
        ip_watched_dir_free (dir);
	}

	return TRUE;
}


static ip_watched_dir_t *
ip_watched_dir_new (const char *path, gint32 wd)
{
	ip_watched_dir_t *dir = g_new0(ip_watched_dir_t, 1);

	dir->path = g_strdup(path);
	dir->wd = wd;

	return dir;
}

static void
ip_watched_dir_free (ip_watched_dir_t * dir)
{
	g_assert (dir->subs == 0);
	g_free(dir->path);
	g_free(dir);
}

static void ip_wd_delete (gpointer data, gpointer user_data)
{
	ip_watched_dir_t *dir = data;
	GList *l = NULL;

	for (l = dir->subs; l; l = l->next)
	{
		ih_sub_t *sub = l->data;

		/* Add subscription to missing list */
		im_add (sub);
	}
	ip_unmap_all_subs (dir);
	/* Unassociate the path and the directory */
	ip_unmap_path_dir (dir->path, dir);
	ip_watched_dir_free (dir);
}

static void ip_event_dispatch (GList *dir_list, GList *pair_dir_list, ik_event_t *event)
{
	GList *dirl;
        GList *subl, *resubscription_list;

	if (!event)
		return;

	/* TODO:
	 *
	 * Figure out how we will deliver move events
	 */

        resubscription_list = NULL;
	for (dirl = dir_list; dirl; dirl = dirl->next)
	{
		ip_watched_dir_t *dir = dirl->data;
                char *event_path;

                event_path = g_build_filename (dir->path, event->name, NULL);



		for (subl = dir->subs; subl; subl = subl->next)
		{
			ih_sub_t *sub = subl->data;

			/* If the event and the subscription have a filename
			 * they need to match before the event could be delivered.
			 */
			if (event->name && sub->filename) {

                               if (strcmp (event->name, sub->filename))
                                {
					continue;
                                }

			/* If the event doesn't have a filename, but the subscription does
			 * we shouldn't deliever the event */
			} else if (sub->filename)
				continue;

                        if (g_str_has_prefix (sub->dirname, event_path)) {

                            IP_W("Adding directory %s to resubscription list (because of event %s)", 
                                 sub->dirname, event_path);

                            resubscription_list = g_list_prepend (resubscription_list, sub);

                            if (strcmp (sub->dirname, event_path) == 0) {
                                        char *subscription_dir;

                                        IP_W("directory '%s' is now available!", 
                                             sub->dirname);

                                        /* Normally, the subscription directory name,
                                         * matches the directory getting watched.  This 
                                         * isn't necessarily true, though, if we're watching
                                         * a parent since the directory we're actually interested 
                                         * in doesn't exist anymore.  When the directory we *are*
                                         * interested in shows up, we find out relative to the
                                         * directory we're watching.  We need to fudge our subscription
                                         * temporarily to account for that.
                                         *
                                         * FIXME: This is a hack, we should send the dirname that the 
                                         * event came from, along with the subscription, or store a 
                                         * full path in the event structure
                                         */
                                        subscription_dir = sub->dirname;
                                        sub->dirname = dir->path;
                                        event_callback (event, sub);
                                        sub->dirname = subscription_dir;
                            }

                            continue;
                        }

			event_callback (event, sub);
		}

                g_free (event_path);
	}

	if (event->pair)
	for (dirl = pair_dir_list; dirl; dirl = dirl->next)
	{
		ip_watched_dir_t *dir = dirl->data;

		for (subl = dir->subs; subl; subl = subl->next)
		{
			ih_sub_t *sub = subl->data;

			/* If the event and the subscription have a filename
			 * they need to match before the event could be delivered.
			 */
			if (event->pair->name && sub->filename) {
				if (strcmp (event->pair->name, sub->filename))
					continue;
			/* If the event doesn't have a filename, but the subscription does
			 * we shouldn't deliever the event */
			} else if (sub->filename)
				continue;

			event_callback (event->pair, sub);
		}
	}

        for (subl = resubscription_list; subl; subl = subl->next)
        {
            ip_stop_watching (subl->data);
            ip_start_watching (subl->data);
        }
        g_list_free (resubscription_list);
}

static void
ip_event_callback (ik_event_t *event)
{
	GList *dir_list = NULL;
	GList *pair_dir_list = NULL;

	dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER(event->wd));

	/* We can ignore IN_IGNORED events */
	if (event->mask & IN_IGNORED) {
		ik_event_free (event);
		return;
	}

	if (event->pair)
		pair_dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER(event->pair->wd));

	if (event->mask & IP_INOTIFY_MASK) {
		ip_event_dispatch (dir_list, pair_dir_list, event);
	        dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER(event->wd));
        }

	/* We have to manage the missing list when we get a DELETE event. */
	if (event->mask & IN_DELETE_SELF || event->mask & IN_MOVE_SELF)
	{
		/* Add all subscriptions to missing list */
		g_list_foreach (dir_list, ip_wd_delete, NULL);
		/* Unmap all directories attached to this wd */
		ip_unmap_wd (event->wd);
	}

	ik_event_free (event);
}
