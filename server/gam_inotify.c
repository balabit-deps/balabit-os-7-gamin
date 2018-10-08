/* gamin inotify backend
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
#include <string.h>
#include <sys/inotify.h>
#include "inotify-sub.h"
#include "inotify-helper.h"
#include "inotify-diag.h"
#ifdef GAMIN_DEBUG_API
#include "gam_debugging.h"
#endif
#include "gam_error.h"
#include "gam_event.h"
#include "gam_server.h"
#include "gam_subscription.h"
#include "gam_inotify.h"

/* Transforms a inotify event to a gamin event. */
static GaminEventType
ih_mask_to_EventType (guint32 mask)
{
        mask &= ~IN_ISDIR;
        switch (mask)
        {
        case IN_MODIFY:
                return GAMIN_EVENT_CHANGED;
        break;
        case IN_ATTRIB:
                return GAMIN_EVENT_CHANGED;
        break;
        case IN_MOVE_SELF:
        case IN_MOVED_FROM:
        case IN_DELETE:
        case IN_DELETE_SELF:
                return GAMIN_EVENT_DELETED;
        break;
        case IN_CREATE:
        case IN_MOVED_TO:
                return GAMIN_EVENT_CREATED;
        break;
        case IN_Q_OVERFLOW:
        case IN_OPEN:
        case IN_CLOSE_WRITE:
        case IN_CLOSE_NOWRITE:
        case IN_UNMOUNT:
        case IN_ACCESS:
        case IN_IGNORED:
        default:
                return -1;
	break;
	}
}

static void
gam_inotify_send_initial_events (const char *pathname, GamSubscription *sub, gboolean is_dir, gboolean was_missing)
{
	GaminEventType gevent;

	if (was_missing) {
	  gevent = GAMIN_EVENT_CREATED;
	} else {
	  if (g_file_test (pathname, G_FILE_TEST_EXISTS))
	    gevent = GAMIN_EVENT_EXISTS;
	  else
	    gevent = GAMIN_EVENT_DELETED;
	}

	gam_server_emit_one_event (pathname, is_dir ? 1 : 0, gevent, sub, 1);

	if (is_dir) 
	{
		GDir *dir;
		GError *err = NULL;
		dir = g_dir_open (pathname, 0, &err);
		if (dir)
		{
			const char *filename;

			while ((filename = g_dir_read_name (dir)))
			{
				gchar *fullname = g_strdup_printf ("%s/%s", pathname, filename);
				gboolean file_is_dir = FALSE;
				struct stat fsb;
				memset(&fsb, 0, sizeof (struct stat));
				lstat(fullname, &fsb);
				file_is_dir = (fsb.st_mode & S_IFDIR) != 0 ? TRUE : FALSE;
				gam_server_emit_one_event (fullname, file_is_dir ? 1 : 0, gevent, sub, 1);
				g_free (fullname);
			}

			g_dir_close (dir);
		} else {
			GAM_DEBUG (DEBUG_INFO, "unable to open directory %s: %s\n", pathname, err->message);
			g_error_free (err);
		}

	}

	if (!was_missing) 
	{
		gam_server_emit_one_event (pathname, is_dir ? 1 : 0, GAMIN_EVENT_ENDEXISTS, sub, 1);
	}

}

static void
gam_inotify_event_callback (const char *fullpath, guint32 mask, void *subdata)
{
	GamSubscription *sub = (GamSubscription *)subdata;
	GaminEventType gevent;

	gevent = ih_mask_to_EventType (mask);

	gam_server_emit_one_event (fullpath, gam_subscription_is_dir (sub), gevent, sub, 1);
}

static void
gam_inotify_found_callback (const char *fullpath, void *subdata)
{
	GamSubscription *sub = (GamSubscription *)subdata;

	gam_inotify_send_initial_events (gam_subscription_get_path (sub), sub, gam_subscription_is_dir (sub), TRUE);
}


gboolean
gam_inotify_init (void)
{
	gam_poll_basic_init ();
	gam_server_install_kernel_hooks (GAMIN_K_INOTIFY2, 
					 gam_inotify_add_subscription,
					 gam_inotify_remove_subscription,
					 gam_inotify_remove_all_for,
					 NULL, NULL);
	
	return ih_startup (gam_inotify_event_callback,
			   gam_inotify_found_callback);
}

gboolean
gam_inotify_add_subscription (GamSubscription *sub)
{
	ih_sub_t *isub = NULL;

	gam_listener_add_subscription(gam_subscription_get_listener(sub), sub);
	
	isub = ih_sub_new (gam_subscription_get_path (sub), gam_subscription_is_dir (sub), 0, sub);

	if (!ih_sub_add (isub))
	{
		ih_sub_free (isub);
		return FALSE;
	}

	gam_inotify_send_initial_events (gam_subscription_get_path (sub), sub, gam_subscription_is_dir (sub), FALSE);

	return TRUE;
}

static gboolean
gam_inotify_remove_sub_pred (ih_sub_t *sub, void *callerdata)
{
	return sub->usersubdata == callerdata;
}

gboolean
gam_inotify_remove_subscription (GamSubscription *sub)
{
	ih_sub_foreach_free (sub, gam_inotify_remove_sub_pred);

	return TRUE;
}

static gboolean
gam_inotify_remove_listener_pred (ih_sub_t *sub, void *callerdata)
{
	GamSubscription *gsub = (GamSubscription *)sub->usersubdata;

	return gam_subscription_get_listener (gsub) == callerdata;
}

gboolean
gam_inotify_remove_all_for (GamListener *listener)
{
	ih_sub_foreach_free (listener, gam_inotify_remove_listener_pred);

	return TRUE;
}

void
gam_inotify_debug (void)
{
	id_dump (NULL);
}

gboolean
gam_inotify_is_running (void)
{
	return ih_running ();
}
