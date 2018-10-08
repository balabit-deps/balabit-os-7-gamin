/* Marmot
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
#include <sys/types.h>
#include <string.h>
#include <glib.h>
#include "gam_event.h"
#include "gam_listener.h"
#include "gam_subscription.h"
#include "gam_protocol.h"
#include "gam_event.h"
#include "gam_error.h"

//#define GAM_SUB_VERBOSE

struct _GamSubscription {
    char *path;
    int events;
    int reqno;
    int pathlen;
    int options;

    gboolean is_dir;
    gboolean cancelled;

    GamListener *listener;
};


/**
 * @defgroup GamSubscription GamSubscription
 * @ingroup Daemon
 * @brief GamSubscription API.
 *
 * A #GamSubscription represents a single monitoring request (or "subscription").
 *
 * @{
 */

/**
 * Creates a new GamSubscription
 *
 * @param path the path to be monitored
 * @param events the events that are accepted
 * @param is_dir whether the subscription is for a directory or not
 * @returns the new GamSubscription
 */
GamSubscription *
gam_subscription_new(const char *path,
                     int events,
                     int reqno,
                     gboolean is_dir,
		     int options)
{
    GamSubscription *sub;

    sub = g_new0(GamSubscription, 1);
    sub->path = g_strdup(path);
    sub->events = events;
    sub->reqno = reqno;
    sub->pathlen = strlen(path);

    /* everyone accepts this */
    gam_subscription_set_event(sub, GAMIN_EVENT_EXISTS | GAMIN_EVENT_ENDEXISTS);

    sub->is_dir = is_dir;
    sub->options = options;

#ifdef GAM_SUB_VERBOSE
    GAM_DEBUG(DEBUG_INFO, "Created subscription for %s\n", path);
#endif
    return sub;
}

/**
 * Frees a GamSubscription
 *
 * @param sub the GamSubscription
 */
void
gam_subscription_free(GamSubscription * sub)
{
    if (sub == NULL)
        return;
#ifdef GAM_SUB_VERBOSE
    GAM_DEBUG(DEBUG_INFO, "Freeing subscription for %s\n", sub->path);
#endif

    g_free(sub->path);
    g_free(sub);
}

/**
 * Tells if a GamSubscription is for a directory or not
 *
 * @param sub the GamSubscription
 * @returns TRUE if the subscription is for a directory, FALSE otherwise
 */
gboolean
gam_subscription_is_dir(GamSubscription * sub)
{
    if (sub == NULL)
        return(FALSE);
    return sub->is_dir;
}

/**
 * Provide the path len for a GamSubscription
 *
 * @param sub the GamSubscription
 * @returns the path len for the subscription
 */
int
gam_subscription_pathlen(GamSubscription * sub)
{
    if (sub == NULL)
        return(-1);
    return sub->pathlen;
}

/**
 * Gets the path for a GamSubscription
 *
 * @param sub the GamSubscription
 * @returns The path being monitored.  It should not be freed.
 */
G_CONST_RETURN char *
gam_subscription_get_path(GamSubscription * sub)
{
    if (sub == NULL)
        return(NULL);
    return sub->path;
}

/**
 * Gets the request number for a GamSubscription
 *
 * @param sub the GamSubscription
 * @returns The request number
 */
int
gam_subscription_get_reqno(GamSubscription * sub)
{
    if (sub == NULL)
        return(-1);
    return sub->reqno;
}

/**
 * Gets the GamListener which owns this GamSubscription
 *
 * @param sub the GamSubscription
 * @returns the GamListener, or NULL
 */
GamListener *
gam_subscription_get_listener(GamSubscription * sub)
{
    if (sub == NULL)
        return(NULL);
    return sub->listener;
}

/**
 * Sets the GamListener which is owned by this GamSubscription
 *
 * @param sub the GamSubscription
 * @param listener the GamListener
 */
void
gam_subscription_set_listener(GamSubscription * sub,
                              GamListener * listener)
{
    if (sub == NULL)
        return;

	GAM_DEBUG(DEBUG_INFO, "%s listening for %s\n", gam_listener_get_pidname (listener), sub->path);
    sub->listener = listener;
}

/**
 * Set the events this GamSubscription is interested in
 *
 * @param sub the GamSubscription
 * @param event an ORed combination of the events desired
 */
void
gam_subscription_set_event(GamSubscription * sub, int event)
{
    if (sub == NULL)
        return;
    sub->events |= event;
}

/**
 * Removes an event from the set of acceptable events
 *
 * @param sub the GamSubscription
 * @param event the event to remove
 */
void
gam_subscription_unset_event(GamSubscription * sub, int event)
{
    if (sub == NULL)
        return;
    sub->events &= ~event;
}

/**
 *  
 * @param sub the GamSubscription
 * @param event the event to test for
 * @returns Whether or not this subscription accepts a given event
 */
gboolean
gam_subscription_has_event(GamSubscription * sub, int event)
{
    if (sub == NULL)
        return(FALSE);
    return((sub->events & event) != 0);
}

/**
 *  
 * @param sub the GamSubscription
 * @option option
 * @returns Whether or not this subscription has that option.
 */
gboolean
gam_subscription_has_option(GamSubscription * sub, int option)
{
    if (sub == NULL)
        return(FALSE);
    return((sub->options & option) != 0);
}

/**
 * Mark this GamSubscription as cancelled
 *
 * @param sub the GamSubscription
 */
void
gam_subscription_cancel(GamSubscription * sub)
{
    if (sub == NULL)
        return;
	GAM_DEBUG(DEBUG_INFO, "%s not listening for %s\n", gam_listener_get_pidname (sub->listener), sub->path);
    sub->cancelled = TRUE;
}

/**
 * Checks if the GamSubscription is cancelled or not
 *
 * @param sub the GamSubscription
 * @returns TRUE if the GamSubscription is cancelled, FALSE otherwise
 */
gboolean
gam_subscription_is_cancelled(GamSubscription * sub)
{
    if (sub == NULL)
        return(TRUE);
    return sub->cancelled == TRUE;
}

/**
 * gam_subscription_wants_event:
 * @sub: the GamSubscription
 * @name: file name (just the base name, not the complete path)
 * @is_dir_node: is the target a directory
 * @event: the event
 * @force: force the event as much as possible
 *
 * Checks if a given path/event combination is accepted by this GamSubscription
 *
 * Returns TRUE if the combination is accepted, FALSE otherwise
 */
gboolean
gam_subscription_wants_event(GamSubscription * sub,
                             const char *name, int is_dir_node, 
			     GaminEventType event, int force)
{
    int same_path = 0;

    if ((sub == NULL) || (name == NULL) || (event == 0))
        return(FALSE);
    if (sub->cancelled)
        return FALSE;

    if ((sub->options & GAM_OPT_NOEXISTS) &&
        ((event == GAMIN_EVENT_EXISTS) ||
	 (event == GAMIN_EVENT_ENDEXISTS)))
	return FALSE;

    /* only directory listening cares for other files */
    same_path = !strcmp(name, sub->path);
    if ((sub->is_dir == 0) && (!same_path))
        return(FALSE);

    if (!gam_subscription_has_event(sub, event)) {
        return FALSE;
    }

    if (force)
        return TRUE;
    if ((sub->is_dir) && (is_dir_node) && (same_path)) {
        if ((event == GAMIN_EVENT_EXISTS) ||
	    (event == GAMIN_EVENT_CHANGED) ||
	    (event == GAMIN_EVENT_ENDEXISTS))
	    return FALSE;
    }

    return TRUE;
}

/**
 * gam_subscription_debug:
 * @sub: the subscription
 *
 * Provide debug output for that subscription node/state
 */
void
gam_subscription_debug(GamSubscription *sub) {
#ifdef GAM_DEBUG_ENABLED
    if (sub == NULL) {
	GAM_DEBUG(DEBUG_INFO, "    Subscription is NULL\n");
        return;
    }
    GAM_DEBUG(DEBUG_INFO,
              "    Subscription %d reqno %d events %d dir %d: %s\n",
              sub->reqno, sub->events, sub->events, sub->is_dir, sub->path);
#endif
}

void
gam_subscription_shutdown ()
{
}

/** @} */
