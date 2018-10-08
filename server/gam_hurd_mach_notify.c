/* 
 * Copyright (C) 2005 Neal H. Walfield
 * Loosely based on gam_inotify.c which is
 * Copyright (C) 2004 John McCutchan, James Willcox, Corey Bowers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Lesser Public License
 * as published by the Free Software Foundation; either version 2, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Lesser Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#define _GNU_SOURCE

#include <string.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>

#include <hurd/fs.h>
#include <hurd/fd.h>
#include <hurd/ports.h>
#include <cthreads.h>
#include "fs_notify.h"

#include <glib.h>
#include "gam_error.h"
#include "gam_hurd_mach_notify.h"
#include "gam_server.h"
#include "gam_event.h"
#include "gam_poll.h"

/* Hash from paths to monitor structures.  */
static GHashTable *path_hash;

/* Pending subscriptions.  */
static GList *new_subs;
/* Pending subscriptions to remove.  */
static GList *removed_subs;

/* The main lock: must be held when mucking with monitors, etc.  */
static struct mutex lock = MUTEX_INITIALIZER;

/* The monitor engine.  */

static struct port_bucket *port_bucket;
static struct port_class *monitor_portclass;

struct monitor
{
    /* The port info data (the notification receive right, etc.).  */
    struct port_info port_info;

    /* File or directory we are monitoring.  */
    char *path;

    /* And the file descriptor pointing to PATH.  If we close this
       after we request notifications, the server will drop the
       notification request when the last hard reference (i.e. the
       last file descriptor) to the in memory node disappears.

       If -1 then PATH doesn't exist and we are waiting for a creation
       event from the parent directory monitor.  */
    int fd;

    /* List of subscriptions waiting for events on this monitor.  Each
       subscription holds a reference to this monitor.  */
    GList *subs;

    /* A hash from file names to monitors.  PATH is a directory and
       entries on this list are listening for events on their
       respective paths.  */
    GHashTable *children;

    /* The parent monitor (i.e. the monitor of the directory
       containing PATH).  */
    struct monitor *parent;
};

/* Attempt to make a passive monitor active.  */
static error_t
monitor_watch (struct monitor *m)
{
    error_t err;

    assert (m->fd == -1);
    m->fd = open (m->path, O_RDONLY);
    if (m->fd < 0) {
	GAM_DEBUG (DEBUG_INFO, "%s: failed to open `%s': %s\n",
		   __FUNCTION__, m->path, strerror (errno));
	return errno;
    }

    err = HURD_FD_USE (m->fd,
		       (file_notice_changes (descriptor->port.port,
					     ports_get_right (m),
					     MACH_MSG_TYPE_MAKE_SEND)));
    if (err == EISDIR)
	err = 0;
    if (err)
	GAM_DEBUG (DEBUG_INFO, "%s: file_notice_changes (%s): %s\n",
		   __FUNCTION__, m->path, strerror (err));
    
    if (! err) {
	err = HURD_FD_USE (m->fd,
			   (dir_notice_changes (descriptor->port.port,
						ports_get_right (m),
						MACH_MSG_TYPE_MAKE_SEND)));
	if (err == ENOTDIR)
	    err = 0;
	if (err)
	    GAM_DEBUG (DEBUG_INFO, "%s: dir_notice_changes (%s): %s\n",
		       __FUNCTION__, m->path, strerror (err));
    }

    return err;
}

/* Attempt to make an active monitor passive.  */
static void
monitor_unwatch (struct monitor *m)
{
    if (m->fd != -1) {
	/* Destroy (and create anew) the receive right.  */
	ports_reallocate_port (m);

	/* And close the file descriptor.  */
	close (m->fd);
	m->fd = -1;
    }
}

/* Start monitoring PATH.  Returns NULL on failure.  LOCK should
   be held.  */
static struct monitor *
monitor_create (const char *path)
{
    error_t err;
    struct monitor *m;
    char *tail;
    char *filename;
    char *dir;

    assert (! mutex_try_lock (&lock));

    /* See if there is already a monitor watching PATH.  */
    m = g_hash_table_lookup (path_hash, path);
    if (m) {
	GAM_DEBUG (DEBUG_INFO, "%s: monitor on `%s' exists; reusing that\n",
		   __FUNCTION__, path);
	return m;
    }

    GAM_DEBUG (DEBUG_INFO, "%s: creating monitor for `%s'\n",
	       __FUNCTION__, path);

    if (path[0] != '/') {
	GAM_DEBUG (DEBUG_INFO, "%s: `%s' invalid: not an absolute path.\n",
		   __FUNCTION__, path);
	return NULL;
    }

    err = ports_create_port (monitor_portclass, port_bucket,
			     sizeof (struct monitor), &m);
    if (err) {
	GAM_DEBUG (DEBUG_INFO, "%s: failed to create port: %s\n",
		   __FUNCTION__, strerror (err));
	return NULL;
    }

    m->fd = -1;
    /* Compute the path without any trailing slashes.  */
    {
	/* strlen (path) is at least one: we've already verified that
	   the first character is a slash.  */
	const char *tail = path + strlen (path) - 1;

	/* This won't strip the leading slash.  */
	while (tail > path && *tail == '/')
	    tail --;

	m->path = g_malloc (tail - path + 1 + 1);
	memcpy (m->path, path, tail - path + 1);
	m->path[tail - path + 1] = '\0';
    }
    m->subs = 0;
    m->children = g_hash_table_new (g_str_hash, g_str_equal);

    err = monitor_watch (m);
    if (err && err != ENOENT && err != EPERM && errno != EACCES)
	goto die;

    /* Monitor the containing directory for creation and deletetion events.  */
    if (strcmp (m->path, "/") == 0) {
	/* Except / which has no parent... */
	m->parent = 0;
    } else {
	if (m->path[0] == '/' && m->path[1] == '\0') {
	    /* Failed to open `/'!!!  */
	    GAM_DEBUG (DEBUG_INFO,
		       "%s: `/' does not exist?  Nothing to monitor.\n",
		       __FUNCTION__, m->path);
	    goto die;
	}

	tail = strrchr (m->path, '/');
	/* There is at least the leading /.  */
	assert (tail);

	filename = tail + 1;

	/* Make TAIL point to the last character in the file name:
	   not the following slash.  */
	while (tail > m->path && *tail == '/')
	    tail --;

	dir = g_malloc (tail - m->path + 1 + 1);
	memcpy (dir, m->path, tail - m->path + 1);
	dir[tail - m->path + 1] = '\0';

	m->parent = monitor_create (dir);
	g_free (dir);
	if (! m->parent)
	    goto die;

	/* Insert it into the parent's children hash table.  */
	g_hash_table_insert (m->parent->children, filename, m);

	GAM_DEBUG (DEBUG_INFO,
		   "%s: Added `%s' to children list of `%s''s monitor.\n",
		   __FUNCTION__, filename, m->parent->path);
    }

    /* Insert the monitor into the path hash table.  */
    g_hash_table_insert (path_hash, m->path, m);

    return m;

 die:
    if (m->fd != -1)
	close (m->fd);
    g_hash_table_destroy (m->children);
#ifndef NDEBUG
    m->children = 0;
#endif
    g_free (m->path);
    /* We didn't manage to make a send right so we'll never get a dead
       name notification.  Force the descruction of the receive
       right.  */
    ports_destroy_right (m);
    ports_port_deref (m);
    return NULL;
}

/* If there are no references to M left clean it up.  Must be called
   whenever a subscription or child is removed.  */
static void
monitor_consider (struct monitor *m)
{
    assert (! mutex_try_lock (&lock));

    if (m->subs || g_hash_table_size (m->children) > 0)
	/* Still have subscribers or children.  */
	return;

    GAM_DEBUG(DEBUG_INFO, "%s: no more subscribers to `%s', killing\n",
	      __FUNCTION__, m->path);

    close (m->fd);
    if (m->parent) {
	const char *filename = strrchr (m->path, '/');
	assert (filename);
	filename ++;

	assert (g_hash_table_lookup (m->parent->children, filename));
	g_hash_table_remove (m->parent->children, filename);

	monitor_consider (m->parent);
    } else
	assert (strcmp (m->path, "/") == 0);
    g_hash_table_destroy (m->children);
#ifndef NDEBUG
    m->children = 0;
#endif

    assert (g_hash_table_lookup (path_hash, m->path));
    g_hash_table_remove (path_hash, m->path);

    ports_port_deref (m);

    /* Kill the port libports will take care of the rest.  */
    ports_destroy_right (m);
}

/* All references to monitor M have been dropped.  Clean it up.  */
static void
monitor_clean (void *p)
{
    struct monitor *m = p;

    GAM_DEBUG (DEBUG_INFO, "%s (%s)\n", __FUNCTION__, m->path);
    g_free (m->path);

    assert (! m->subs);
    assert (! m->children);
}

/* Emit an event.  */
static void
monitor_emit (struct monitor *m, const char *item, GaminEventType event)
{
    if (event == GAMIN_EVENT_UNKNOWN)
	return;

    GAM_DEBUG (DEBUG_INFO, "%s: emitting event %s for `%s' on `%s'\n",
	       __FUNCTION__, gam_event_to_string(event), item, m->path);

    gam_server_emit_event (item, 0, event, m->subs, 0);
}

/* Directory changed callback.  */
error_t
dir_changed (fs_notify_t notify_port,
	     natural_t tickno,
	     dir_changed_type_t change,
	     string_t name)
{
    error_t err;
    struct monitor *m;
    struct monitor *child;

    mutex_lock (&lock);

    m = ports_lookup_port (port_bucket, notify_port, monitor_portclass);
    if (! m) {
	mutex_unlock (&lock);
	return EINVAL;
    }

    switch (change) {
    case DIR_CHANGED_NULL:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): DIR_CHANGED_NULL\n",
		   __FUNCTION__, m->path);
	break;

    case DIR_CHANGED_NEW:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): DIR_CHANGED_NEW: `%s'\n",
		   __FUNCTION__, m->path, name);

	monitor_emit (m, name, GAMIN_EVENT_CREATED);

	child = g_hash_table_lookup (m->children, name);
	if (child) {
	    err = monitor_watch (child);
	    if (! err) {
		monitor_emit (child, child->path, GAMIN_EVENT_CREATED);
	    }
	}
	break;

    case DIR_CHANGED_UNLINK:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): DIR_CHANGED_UNLINK: `%s'\n",
		   __FUNCTION__, m->path, name);

	if (name[0] == '.' && (name[1] == '\0'
			       || (name[1] == '.' && name[2] == '\0')))
	    /* Ignore `.' and `..'.  */
	    break;

	monitor_emit (m, name, GAMIN_EVENT_DELETED);

	child = g_hash_table_lookup (m->children, name);
	if (child) {
	    monitor_unwatch (child);
	    monitor_emit (child, child->path, GAMIN_EVENT_DELETED);
	}
	break;

    case DIR_CHANGED_RENUMBER:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): DIR_CHANGED_RENUMBER: `%s'\n",
		   __FUNCTION__, m->path, name);
	monitor_emit (m, name, GAMIN_EVENT_CHANGED);
	break;

    default:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): unknown\n", __FUNCTION__, m->path);
	break;
    }

    mutex_unlock (&lock);

    ports_port_deref (m);
    return 0;
}

/* File changed callback.  */
error_t
file_changed (fs_notify_t notify_port,
	      natural_t tickno,
	      file_changed_type_t change,
	      loff_t start,
	      loff_t end)
{
    struct monitor *m;

    mutex_lock (&lock);

    m = ports_lookup_port (port_bucket, notify_port, monitor_portclass);
    if (! m) {
	mutex_unlock (&lock);
	return EINVAL;
    }

    switch (change) {
    case FILE_CHANGED_NULL:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): FILE_CHANGED_NULL\n",
		   __FUNCTION__, m->path);
	break;

    case FILE_CHANGED_WRITE:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): FILE_CHANGED_WRITE\n",
		   __FUNCTION__, m->path);
	monitor_emit (m, m->path, GAMIN_EVENT_CHANGED);
	break;

    case FILE_CHANGED_EXTEND:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): FILE_CHANGED_EXTEND\n",
		   __FUNCTION__, m->path);
	monitor_emit (m, m->path, GAMIN_EVENT_CHANGED);
	break;

    case FILE_CHANGED_TRUNCATE:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): FILE_CHANGED_TRUNCATE\n",
		   __FUNCTION__, m->path);
	monitor_emit (m, m->path, GAMIN_EVENT_CHANGED);
	break;

    case FILE_CHANGED_META:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): FILE_CHANGED_META\n",
		   __FUNCTION__, m->path);
	monitor_emit (m, m->path, GAMIN_EVENT_CHANGED);
	break;

    default:
	GAM_DEBUG (DEBUG_INFO, "%s (%s): unknown\n", __FUNCTION__, m->path);
	break;
    }

    mutex_unlock (&lock);

    ports_port_deref (m);
    return 0;
}

/* The server loop: waits for messages from the file systems.  */
static void *
server (void *bucket)
{
  extern boolean_t fs_notify_server (mach_msg_header_t *in,
				     mach_msg_header_t *out);

  int s (mach_msg_header_t *in, mach_msg_header_t *out)
      {
	  return fs_notify_server (in, out)
	      || ports_notify_server (in, out);
      }

  while (1)
    ports_manage_port_operations_one_thread (bucket, s, 0);
}

/* If ADD is true, add the subscription SUB, otherwise, remove
   it.  LOCK must be held.  */
static void
gam_hurd_notify_add_rm_handler(const char *path,
			       GamSubscription *sub, gboolean add)
{
    struct monitor *m;

    assert (! mutex_try_lock (&lock));

    if (add) {
	/* Add subscription SUB to the monitor watching PATH.  */

	DIR *dir;

	if (gam_subscription_is_dir (sub)) {
	    dir = opendir (path);
	    if (dir) {
		GAM_DEBUG(DEBUG_INFO, "%s: `%s' is a directory\n",
			  __FUNCTION__, path);
	    } else {
		/* What was the problem?  */
		if (errno != ENOTDIR && errno != ENOENT && errno != EPERM
		    && errno != EACCES) {
		    /* Fatal error.  */
		    GAM_DEBUG(DEBUG_INFO, "%s: opening `%s': %s\n",
			      __FUNCTION__, path, strerror (errno));
		    return;
		}
	    }
	} else
	    dir = NULL;

	/* Create a new monitor.  */
	m = monitor_create (path);
	if (m) {
	    /* Add the subscription.  The monitor comes with a
	       reference.  */
	    m->subs = g_list_prepend(m->subs, sub);

	    GAM_DEBUG(DEBUG_INFO, "%s: created monitor for `%s'\n",
		      __FUNCTION__, path);
	} else {
	    GAM_DEBUG(DEBUG_INFO,
		      "%s: failed to create monitor for `%s': %s\n",
		      __FUNCTION__, path, strerror (errno));
	    if (dir)
		closedir (dir);
	    return;
	}

	/* According to the FAM documentation: "[w]hen the application
	   requests that a file be monitored, FAM generates a
	   FAMExists event for that file (if it exists). When the
	   application requests that a directory be monitored, FAM
	   generates a FAMExists event for that directory (if it
	   exists) and every file contained in that directory."  */
	monitor_emit (m, m->path,
		      m->fd == -1 ? GAMIN_EVENT_DELETED : GAMIN_EVENT_EXISTS);
	if (dir) {
	    union
	    {
		struct dirent d;
		char b[offsetof (struct dirent, d_name) + NAME_MAX + 1];
	    } u;
	    struct dirent *res;

	    while (readdir_r (dir, &u.d, &res) == 0) {
		if (! res)
		    break;

		if (res->d_name[0] == '.'
		    && (res->d_name[1] == '\0'
			|| (res->d_name[1] == '.' && res->d_name[2] == '\0')))
		    /* Skip `.' and `..'.  */
		    continue;

		monitor_emit (m, res->d_name, GAMIN_EVENT_EXISTS);
	    }
	    closedir (dir);
	}
	monitor_emit (m, m->path, GAMIN_EVENT_ENDEXISTS);
    } else {
	/* Remove subscription SUB from the monitor watching PATH.  */

        m = g_hash_table_lookup (path_hash, path);
        if (!m) {
	    GAM_DEBUG(DEBUG_INFO, "%s: request to remove subscription to "
		      "`%s' but no monitor found!\n",
		      __FUNCTION__, path);
            return;
        }

	if (g_list_find (m->subs, sub)) {
	    GAM_DEBUG(DEBUG_INFO,
		      "%s: removed subscription from `%s' monitor (%d)\n",
		      __FUNCTION__, path, m->port_info.refcnt);

	    m->subs = g_list_remove_all (m->subs, sub);
	    monitor_consider (m);
	}
    }
}

static int have_consume_idler;

/* Called by glib's idle handler.  Add any subscriptions on NEW_SUBS
   and remove any subscriptions on REMOVED_SUBS.  */
static gboolean
gam_hurd_notify_consume_subscriptions_real(gpointer data)
{
    GList *subs, *l;
	
    mutex_lock (&lock);

    /* Pending additions.  */
    subs = new_subs;
    new_subs = NULL;
    for (l = subs; l; l = l->next) {
	GamSubscription *sub = l->data;
	GAM_DEBUG(DEBUG_INFO, "%s: adding `%s'\n",
		  __FUNCTION__, gam_subscription_get_path (sub));
	gam_hurd_notify_add_rm_handler (gam_subscription_get_path (sub),
					sub, TRUE);
    }

    /* Pending removals.  */
    subs = removed_subs;
    removed_subs = NULL;
    for (l = subs; l; l = l->next) {
	GamSubscription *sub = l->data;
	GAM_DEBUG(DEBUG_INFO, "%s: removing `%s'\n",
		  __FUNCTION__, gam_subscription_get_path (sub));
	gam_hurd_notify_add_rm_handler (gam_subscription_get_path (sub),
					sub, FALSE);
    }

    GAM_DEBUG(DEBUG_INFO, "%s done\n", __FUNCTION__);

    have_consume_idler = FALSE;

    mutex_unlock (&lock);
    return FALSE;
}

/* Register the real adder and remover of subscriptions callback to be
   called by glib's idle thread.  LOCK must be held.  */
static void
gam_hurd_notify_consume_subscriptions(void)
{
    if (! have_consume_idler) {
	GSource *source;

	have_consume_idler = TRUE;

	source = g_idle_source_new ();
	g_source_set_callback (source,
			       gam_hurd_notify_consume_subscriptions_real,
			       NULL, NULL);
	g_source_attach (source, NULL);
	g_source_unref (source);
    }
}

/**
 * @defgroup hurd notify backend
 * @ingroup Backends
 * @brief hurd notify backend API
 *
 * Since version 1991, the Hurd has included the fs_notify interface.
 * This backend uses fs_notify to know when files are
 * changed/created/deleted.
 *
 * @{
 */

extern gboolean gam_hurd_notify_add_subscription(GamSubscription * sub);
extern gboolean gam_hurd_notify_remove_subscription(GamSubscription * sub);
extern gboolean gam_hurd_notify_remove_all_for(GamListener * listener);

/**
 * Initializes the hurd notify system.  This must be called before any
 * other functions in this module.
 *
 * @returns TRUE if initialization succeeded, FALSE otherwise
 */
gboolean
gam_hurd_notify_init(void)
{
    path_hash = g_hash_table_new(g_str_hash, g_str_equal);

    port_bucket = ports_create_bucket ();
    if (! port_bucket) {
	GAM_DEBUG(DEBUG_INFO, "Could not create port bucket\n");
        return FALSE;
    }

    monitor_portclass = ports_create_class (monitor_clean, NULL);
    if (! monitor_portclass) {
	GAM_DEBUG(DEBUG_INFO, "Could not create port class\n");
        return FALSE;
    }

    /* Launch the monitor server.  */
    cthread_detach (cthread_fork (server, port_bucket));

    GAM_DEBUG (DEBUG_INFO, "hurd notify initialized\n");

    gam_poll_set_kernel_handler(NULL, NULL, GAMIN_K_MACH);
    gam_backend_add_subscription = gam_hurd_notify_add_subscription;
    gam_backend_remove_subscription = gam_hurd_notify_remove_subscription;
    gam_backend_remove_all_for = gam_hurd_notify_remove_all_for;

    return TRUE;
}
  
/**
 * Adds a subscription to be monitored.
 *
 * @param sub a #GamSubscription to be polled
 * @returns TRUE if adding the subscription succeeded, FALSE otherwise
 */
gboolean
gam_hurd_notify_add_subscription(GamSubscription * sub)
{
    gam_listener_add_subscription(gam_subscription_get_listener(sub), sub);

    mutex_lock (&lock);
    new_subs = g_list_prepend(new_subs, sub);

    GAM_DEBUG(DEBUG_INFO, "%s\n", __FUNCTION__);

    gam_hurd_notify_consume_subscriptions();
    mutex_unlock (&lock);

    return TRUE;
}

/**
 * Removes a subscription which was being monitored.
 *
 * @param sub a #GamSubscription to remove
 * @returns TRUE if removing the subscription succeeded, FALSE otherwise
 */
gboolean
gam_hurd_notify_remove_subscription(GamSubscription * sub)
{
    mutex_lock (&lock);
    if (g_list_find(new_subs, sub)) {
	GAM_DEBUG(DEBUG_INFO,
		  "%s: subscription `%s' removed from new_subs list\n",
		  __FUNCTION__, gam_subscription_get_path (sub));

	new_subs = g_list_remove_all (new_subs, sub);
	mutex_unlock (&lock);
	return TRUE;
    }
    mutex_unlock (&lock);

    GAM_DEBUG(DEBUG_INFO, "%s: adding `%s' to removed_subs for removal\n",
	      __FUNCTION__, gam_subscription_get_path (sub));

    gam_subscription_cancel (sub);

    mutex_lock (&lock);
    removed_subs = g_list_prepend (removed_subs, sub);
    gam_hurd_notify_consume_subscriptions();
    mutex_unlock (&lock);

    return TRUE;
}

/**
 * Stop monitoring all subscriptions for a given listener.
 *
 * @param listener a #GamListener
 * @returns TRUE if removing the subscriptions succeeded, FALSE otherwise
 */
gboolean
gam_hurd_notify_remove_all_for(GamListener * listener)
{
    GList *subs, *l;

    subs = gam_listener_get_subscriptions (listener);
    if (! subs)
	return FALSE;

    for (l = subs; l; l = l->next) {
	GamSubscription *sub = l->data;
	g_assert (sub != NULL);
	gam_hurd_notify_remove_subscription (sub);
    }

    if (subs) {
	g_list_free (subs);
	return TRUE;
    } else
	return FALSE;
}

/** @} */


/*
   Local Variables:
   c-basic-offset: 4
   End:
*/
