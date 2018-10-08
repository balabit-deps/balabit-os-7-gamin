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
#include "gam_event.h"
#include "gam_node.h"
#include "gam_error.h"

/**
 * Create a new node
 *
 * @param path the path the node will represent
 * @param sub an initial GamSubscription for the node, could be NULL
 * @param is_dir whether the node is a directory or not
 * @returns the new node
 */
GamNode *
gam_node_new(const char *path, GamSubscription * sub, gboolean is_dir)
{
    GamNode *node;
    node = g_new0(GamNode, 1);

    node->path = g_strdup(path);
    if (sub)
        node->subs = g_list_prepend(NULL, sub);
    else
        node->subs = NULL;

    node->is_dir = is_dir;
    node->flags = 0;
	node->checks = 0;

    node->poll_time = gam_fs_get_poll_timeout (path);
    node->mon_type = gam_fs_get_mon_type (path);
	GAM_DEBUG(DEBUG_INFO, "g_n_n: node for %s using %s with poll timeout of %d\n", path, node->mon_type == GFS_MT_KERNEL ? "kernel" : "poll", node->poll_time);
    return node;
}

/**
 * Frees a node
 *
 * @param node the node
 */
void
gam_node_free(GamNode * node)
{
    g_assert(node != NULL);
    g_assert(node->subs == NULL);

    g_free(node->path);
    g_free(node);
}

/**
 * Retrieves the parent of a given node
 *
 * @param node the node
 * @returns the parent, or NULL if node has no parent
 */
GamNode *
gam_node_parent(GamNode * node)
{
    GamNode *ret = NULL;

    g_assert(node);

    if (node->node && node->node->parent)
        ret = (GamNode *) node->node->parent->data;

    return ret;
}

/**
 * Returns whether a node is a directory or not
 *
 * @param node the node
 * @returns TRUE if the node is a directory, FALSE otherwise
 */
gboolean
gam_node_is_dir(GamNode * node)
{
    g_assert(node);
    return(node->is_dir);
}

/**
 * Sets whether a node is a directory
 *
 * @param node the node
 * @param is_dir whether the node is a directory
 */
void
gam_node_set_is_dir(GamNode * node, gboolean is_dir)
{
    g_assert(node);
    node->is_dir = is_dir;
}

/**
 * Returns the path associated with a node
 *
 * @param node the node
 * @returns the path.  The caller must keep a reference to node until
 * it has finished with the string.  If it must keep it longer, it
 * should makes its own copy.  The returned string must not be freed.
 */
G_CONST_RETURN char *
gam_node_get_path(GamNode * node)
{
    g_assert(node);
    return node->path;
}

/**
 * Returns the list of subscriptions to a node
 *
 * @param node the node
 * @returns the list of #GamSubscription to the node
 */
GList *
gam_node_get_subscriptions(GamNode * node)
{
    g_assert(node);
    return node->subs;
}

/**
 * Returns whether a node has any directory subscriptions
 *
 * @param node the node
 * @returns TRUE if the node has directory subscriptions, FALSE otherwise
 */
gboolean
gam_node_has_dir_subscriptions(GamNode * node)
{
    GList *s;

    g_assert (node);
    if (!(node->is_dir))
        return(FALSE);
    for (s = node->subs;s != NULL;s = s->next) {
        if (gam_subscription_is_dir((GamSubscription *) s->data))
	    return(TRUE);
    }
    return(FALSE);
}

/**
 * Adds a subscription to a node
 *
 * @param node the node
 * @param sub the subscription
 * @returns TRUE on success, FALSE otherwise
 */
gboolean
gam_node_add_subscription(GamNode * node, GamSubscription * sub)
{
    g_assert(node);
    g_assert(sub);
    g_assert(!g_list_find(node->subs, sub));

    node->subs = g_list_prepend(node->subs, sub);

    return TRUE;
}

/**
 * Removes a subscription from a node
 *
 * @param node the node
 * @param sub the subscription to remove
 * @returns TRUE if the subscription was removed, FALSE otherwise
 */
gboolean
gam_node_remove_subscription(GamNode * node, GamSubscription * sub)
{
    g_assert(node);
    g_assert(g_list_find (node->subs, sub));

    node->subs = g_list_remove_all(node->subs, sub);

    return TRUE;
}

/**
 * Sets the GNode associated with a node.  Should only be used by MdTree
 *
 * @param node the node
 * @param gnode the GNode
 */
void
gam_node_set_node(GamNode * node, GNode * gnode)
{
    g_assert(node);
    node->node = gnode;
}

/**
 * Gets the GNode associated with a node.  Should only be used by MdTree
 *
 * @param node the node
 * @returns the GNode
 */
GNode *
gam_node_get_node(GamNode * node)
{
    g_assert(node);
    return node->node;
}

/**
 * Set a flag on a node
 *
 * @param node the node
 * @param flag the flag to set
 */
void
gam_node_set_flag(GamNode * node, int flag)
{
    g_assert(node);
    /* Make sure we set exactly one flag.  */
    g_assert((flag & (flag - 1)) == 0);
    node->flags |= flag;
}

/**
 * Clears a flag from a node
 *
 * @param node the node
 * @param flag the flag
 */
void
gam_node_unset_flag(GamNode * node, int flag)
{
    g_assert(node);
    /* Make sure we clear exactly one flag.  */
    g_assert((flag & (flag - 1)) == 0);
    node->flags &= ~flag;
}

/**
 * Checks whether a flag is set on a node
 *
 * @param node the node
 * @param flag the flag
 * @returns TRUE if the flag is set, FALSE otherwise
 */
gboolean
gam_node_has_flag(GamNode * node, int flag)
{
    g_assert(node);
    /* Check exactly one flag.  */
    g_assert((flag & (flag - 1)) == 0);
    return node->flags & flag;
}

/**
 * Set a pflag on a node
 *
 * @param node the node
 * @param flag the flag to set
 */
void
gam_node_set_pflag(GamNode * node, int flag)
{
    g_assert(node);
    /* Make sure we set exactly one flag.  */
    g_assert((flag & (flag - 1)) == 0);
    node->pflags |= flag;
}

/**
 * Clears a pflag from a node
 *
 * @param node the node
 * @param flag the flag
 */
void
gam_node_unset_pflag(GamNode * node, int flag)
{
    g_assert(node);
    /* Make sure we clear exactly one flag.  */
    g_assert((flag & (flag - 1)) == 0);
    node->pflags &= ~flag;
}

/**
 * Checks whether a pflag is set on a node
 *
 * @param node the node
 * @param flag the flag
 * @returns TRUE if the flag is set, FALSE otherwise
 */
gboolean
gam_node_has_pflag(GamNode * node, int flag)
{
    g_assert(node);
    /* Check exactly one flag.  */
    g_assert((flag & (flag - 1)) == 0);
    return node->pflags & flag;
}

/**
 * Set the pflags on a node
 *
 * @param node the node
 * @param flags the flags
 */
void
gam_node_set_pflags(GamNode * node, int flags)
{
    g_assert(node);
    node->pflags = flags;
}


/**
 * Checks whether some pflags are set on a node
 *
 * @param node the node
 * @param flags the flags
 * @returns TRUE if the flag is set, FALSE otherwise
 */
gboolean
gam_node_has_pflags(GamNode * node, int flags)
{
    g_assert(node);
    return node->pflags & flags;
}

void
gam_node_emit_event (GamNode *node, GaminEventType event)
{
	GList *l;
	GamNode *parent;
	GList *subs;
	int is_dir_node = gam_node_is_dir(node);

#ifdef VERBOSE_POLL
	GAM_DEBUG(DEBUG_INFO, "Poll: emit events %d for %s\n", event, gam_node_get_path(node));
#endif
	subs = gam_node_get_subscriptions(node);

	if (subs)
		subs = g_list_copy(subs);

	parent = gam_node_parent(node);
	if (parent) {
		GList *parent_subs = gam_node_get_subscriptions(parent);

		for (l = parent_subs; l; l = l->next) {
			if (!g_list_find(subs, l->data))
				subs = g_list_prepend(subs, l->data);
		}
	}

	gam_server_emit_event(gam_node_get_path(node), is_dir_node, event, subs, 0);

	g_list_free(subs);
}

/** @} */
