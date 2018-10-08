/* Gamin
 * Copyright (C) 2003 James Willcox, Corey Bowers
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
#include <glib.h>
#include "gam_listener.h"
#include "gam_subscription.h"
#include "gam_server.h"
#include "gam_error.h"
#include "gam_pidname.h"
#ifdef ENABLE_INOTIFY
#include "gam_inotify.h"
#endif

//#define GAM_LISTENER_VERBOSE
/* private struct representing a single listener */
struct _GamListener {
    void *service;
    int pid;
    char *pidname;
    GList *subs;
};

/**
 * @defgroup GamListener GamListener
 * @ingroup Daemon
 * @brief GamListener API.
 *
 * @{
 */

/**
 * gam_listener_new:
 *
 * @service: service structure used to communicate with #GamListener
 * @pid: the unique ID for this listener
 *
 * Creates a #GamListener
 *
 * Returns a new #GamListener on success, NULL otherwise
 */
GamListener *
gam_listener_new(void *service, int pid)
{
    GamListener *listener;

    g_assert(service);
    g_assert(pid != 0);

    listener = g_new0(GamListener, 1);
    listener->service = service;
    listener->pid = pid;
    listener->pidname = gam_get_pidname (pid);
    listener->subs = NULL;

#ifdef GAM_LISTENER_VERBOSE
    GAM_DEBUG(DEBUG_INFO, "Created listener for %d\n", pid);
#endif

    return listener;
}

/**
 * gam_listener_free_subscription:
 *
 * @listener: the #GamListener
 * @sub: the subscription to remove
 *
 * Frees a listener's subscription
 */
static void
gam_listener_free_subscription(GamListener *listener,
			       GamSubscription *sub)
{
    char *path;
    
    g_assert(listener);
    g_assert(sub);
    g_assert(g_list_find(listener->subs, sub));
    path = g_strdup(gam_subscription_get_path(sub));
    
    gam_remove_subscription(sub);
#ifdef ENABLE_INOTIFY
    if (gam_inotify_is_running() && (!gam_exclude_check(path))) {
	gam_fs_mon_type type;

	type = gam_fs_get_mon_type (path);
	if (type != GFS_MT_POLL)
	    gam_subscription_free(sub);
    }
#endif
    g_free(path);
}

/**
 * gam_listener_free:
 *
 * @listener: the #GamListener to free
 *
 * Frees a #GamListener returned by #gam_listener_new
 */
void
gam_listener_free(GamListener *listener)
{
    GList *cur;

    g_assert(listener);

    while ((cur = g_list_first(listener->subs)) != NULL) {
        GamSubscription * sub = cur->data;
	gam_listener_free_subscription(listener, sub);
	listener->subs = g_list_delete_link(listener->subs, cur);
    }
	g_free(listener->pidname);
    g_free(listener);
}

/**
 * gam_listener_get_service:
 *
 * @listener: the #GamListener
 *
 * Gets the service associated with a #GamListener
 *
 * Returns the service associated with the #GamListener.  The result
 * is owned by the #GamListener and must not be freed by the caller.
 */
void *
gam_listener_get_service(GamListener *listener)
{
    return listener->service;
}

/**
 * gam_listener_get_pid:
 *
 * @listener: the #GamListener
 *
 * Gets the unique process ID associated with a #GamListener
 *
 * Returns the pid associated with the #GamListener.
 */
int
gam_listener_get_pid(GamListener *listener)
{
    return listener->pid;
}

/**
 * gam_listener_get_pidname:
 *
 * @listener: the #GamListener
 *
 * Gets the process name associated with a #GamListener
 *
 * Returns the process name associated with the #GamListener.
 */
const char *
gam_listener_get_pidname(GamListener *listener)
{
    return listener->pidname;
}

/**
 * gam_listener_get_subscription:
 *
 * @listener: the #GamListener
 * @path: a path to a file or directory
 *
 * Gets the subscription to a path
 *
 * Returns the #GamSubscription to path, or NULL if there is none
 */
GamSubscription *
gam_listener_get_subscription(GamListener *listener, const char *path)
{
    GList *l;

    for (l = listener->subs; l; l = l->next) {
        GamSubscription *sub = l->data;

        if (strcmp(gam_subscription_get_path(sub), path) == 0)
            return sub;
    }

    return NULL;
}

/**
 * gam_listener_get_subscription_by_reqno:
 *
 * @listener: the #GamListener
 * @reqno: a subscription request number
 *
 * Gets the subscription represented by the given reqno
 *
 * Returns a #GamSubscription, or NULL if it wasn't found
 */
GamSubscription *
gam_listener_get_subscription_by_reqno(GamListener * listener, int reqno)
{
    GList *l;

    for (l = listener->subs; l; l = l->next) {
        GamSubscription *sub = l->data;

        if (gam_subscription_get_reqno(sub) == reqno)
            return sub;
    }

    return NULL;
}

/**
 * gam_listener_is_subscribed:
 *
 * @listener: the #GamListener
 * @path: the path to the file or directory
 *
 * Returns whether a #GamListener is subscribed to a file or directory
 *
 * Returns TRUE if listener has a subscription to the path, FALSE
 * otherwise.
 */
gboolean
gam_listener_is_subscribed(GamListener *listener, const char *path)
{
    return gam_listener_get_subscription(listener, path) != NULL;
}

/**
 * gam_listener_add_subscription:
 *
 * @listener: the #GamListener
 * @sub: the #GamSubscription to add
 *
 * Adds a subscription to the #GamListener
 */
void
gam_listener_add_subscription(GamListener *listener,
                              GamSubscription *sub)
{
    g_assert(listener);
    g_assert(sub);
    g_assert(!g_list_find(listener->subs, sub));

    listener->subs = g_list_prepend(listener->subs, sub);
    GAM_DEBUG(DEBUG_INFO, "Adding sub %s to listener %s\n", gam_subscription_get_path (sub), listener->pidname);
}

/**
 * gam_listener_remove_subscription:
 *
 * @listener: the #GamListener
 * @sub: the #GamSubscription to remove
 *
 * Removes a subscription from the #GamListener.
 */
void
gam_listener_remove_subscription(GamListener *listener,
                                 GamSubscription *sub)
{
    g_assert(listener);
    g_assert(sub);
    g_assert(g_list_find(listener->subs, sub));

    listener->subs = g_list_remove(listener->subs, sub);
    GAM_DEBUG(DEBUG_INFO, "Removing sub %s from listener %s\n", gam_subscription_get_path (sub), listener->pidname);
    /* There should only be one.  */
    g_assert(!g_list_find(listener->subs, sub));
}

/**
 * gam_listener_get_subscriptions:
 *
 * @listener: the #GamListener
 *
 * Gets all the subscriptions a given listener has
 *
 * Returns a new list containing all of listener's subscriptions.  It
 * is the responsibility of the caller to free the list.
 */
GList *
gam_listener_get_subscriptions(GamListener *listener)
{
    g_assert(listener);
    return g_list_copy(listener->subs);
}

/**
 * gam_listener_debug:
 *
 * @listener: the #GamListener
 *
 * Print debugging information about a listener
 */
void
gam_listener_debug(GamListener *listener)
{
#ifdef GAM_DEBUG_ENABLED
    GList *cur;

    if (listener == NULL) {
	GAM_DEBUG(DEBUG_INFO, "  Listener is NULL\n");
        return;
    }

    GAM_DEBUG(DEBUG_INFO, "  Listener %s has %d subscriptions registered\n", listener->pidname, 
              g_list_length(listener->subs));
    for (cur = listener->subs; cur; cur = g_list_next(cur)) {
	gam_subscription_debug((GamSubscription *) cur->data);
    }
#endif
}

/** @} */
