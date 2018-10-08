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
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "gam_tree.h"
#include "gam_node.h"


#define NODE_DATA(x) (GamNode *)x->data

struct _GamTree {
    GNode *root;                /* The root of the tree, which is always a node
                                 * representing /
                                 */
    GHashTable *node_hash;      /* a hash table that maps path->node */
};

typedef struct {
    GamListener *listener;
    GList *list;
} SubSearchData;

static GNode *
new_node(GamNode * node)
{
    GNode *gnode;

    gnode = g_node_new(node);
    node->node = gnode;

    return gnode;
}


/**
 * @defgroup GamTree GamTree
 * @ingroup Daemon
 * @brief GamTree API
 *
 * @{
 */

/**
 * Creates a new GamTree object.
 * 
 * The GamTree is useful for attaching data to files/directories in
 * a filesystem.
 *
 * @returns a new #GamTree
 */
GamTree *
gam_tree_new(void)
{
    GamTree *tree;

    tree = g_new0(GamTree, 1);
    tree->root = new_node(gam_node_new("/", NULL, TRUE));
    tree->node_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            g_free, NULL);

    return tree;
}

/**
 * Destroys a previously created tree.
 *
 * @param tree the tree
 */
void
gam_tree_free(GamTree * tree)
{
    g_node_destroy(tree->root);
    g_hash_table_destroy(tree->node_hash);

    g_free(tree);
}

/**
 * Adds a node to the tree
 *
 * @param tree the tree
 * @param parent the parent of the node to be inserted
 * @param child the node to insert
 * @returns TRUE if the node was added, FALSE otherwise
 */
gboolean
gam_tree_add(GamTree * tree, GamNode * parent, GamNode * child)
{
    GNode *node;


    if (g_hash_table_lookup(tree->node_hash, gam_node_get_path(child)))
        return FALSE;           /* lock ??? */

    node = new_node(child);
    g_node_append(parent->node, node);
    g_hash_table_insert(tree->node_hash,
                        g_strdup(gam_node_get_path(child)), node);


    return TRUE;
}

/**
 * Removes a node from the tree
 *
 * @param tree the tree
 * @param node the node to remove
 * @returns TRUE if the node was removed, FALSE otherwise
 */
gboolean
gam_tree_remove(GamTree * tree, GamNode * node)
{
    gboolean ret = FALSE;


    if (g_node_is_ancestor(tree->root, node->node)) {

        g_assert(g_node_first_child(node->node) == NULL);

        g_hash_table_remove(tree->node_hash, gam_node_get_path(node));
        g_node_unlink(node->node);
        g_node_destroy(node->node);
        gam_node_free(node);
        ret = TRUE;
    }


    return ret;
}

/**
 * Gets a node given a filesystem path.
 *
 * @param tree the tree
 * @param path the path to lookup
 * @returns the #GamNode corresponding to the provided path, or NULL if not
 * found
 */
GamNode *
gam_tree_get_at_path(GamTree * tree, const char *path)
{
    GNode *node;

    g_return_val_if_fail(tree != NULL, NULL);
    g_return_val_if_fail(path != NULL, NULL);

    node = g_hash_table_lookup(tree->node_hash, path);
    if (node)
        return NODE_DATA(node);
    else
        return NULL;
}

/**
 * Adds a node given a filesystem path.
 *
 * This is like #gam_tree_add, except all parents of the path
 * are created if they don't exist.  For instance, calling this function
 * on "/tmp/foo/bar/blah.txt" will create the directories "tmp", "foo",
 * and "bar" if they don't exist and insert "blah.txt" under "bar".
 *
 * @param tree the tree
 * @param path the path to add
 * @param is_dir TRUE if the node is supposed to be a directory
 * @returns the new #GamNode
 */
GamNode *
gam_tree_add_at_path(GamTree * tree, const char *path, gboolean is_dir)
{
    GamNode *parent;
    GamNode *node;
    unsigned int i;
    char *path_cpy;

    g_return_val_if_fail(strlen(path) > 0, NULL);

    if ((node = gam_tree_get_at_path(tree, path)) != NULL)
        return node;

    if (g_file_test(path, G_FILE_TEST_EXISTS))
        is_dir = g_file_test(path, G_FILE_TEST_IS_DIR);

    path_cpy = g_strdup(path);

    parent = NODE_DATA(tree->root);
    g_assert(parent != NULL);

    for (i = 1; i < strlen(path_cpy); i++) {
        GamNode *new_parent;

        if (path_cpy[i] == '/') {
            path_cpy[i] = '\0';
            new_parent = gam_tree_get_at_path(tree, path_cpy);

            if (new_parent) {
                parent = new_parent;
            } else {
                new_parent = gam_node_new(path_cpy, NULL, TRUE);
                gam_tree_add(tree, parent, new_parent);
                parent = new_parent;
            }

            path_cpy[i] = '/';
        }
    }

    node = gam_node_new(path, NULL, is_dir);
    gam_tree_add(tree, parent, node);

    g_free(path_cpy);

    return node;
}

/**
 * Gets the immediate children below a given node
 *
 * @param tree the tree
 * @param root where to start from, NULL if you want to start from the root
 * @returns a new list of #GamNode
 */
GList *
gam_tree_get_children(GamTree * tree, GamNode * root)
{
    GList *list = NULL;
    GNode *node, *child;
    unsigned int i, n;
    void *data;

    if ((tree == NULL) && (root == NULL))
        return(NULL);

    node = root ? root->node : tree->root;
    if (node == NULL)
        return(NULL);
    n = g_node_n_children(node);

    for (i = 0; i < n; i++) {
        child = g_node_nth_child(node, i);
	if (child == NULL) break;
        data = NODE_DATA(child);
	if (data == NULL) break;
        list = g_list_prepend(list, data);
    }

    return list;
}

/**
 * Tells whether a given node has any children
 *
 * @param tree the tree
 * @param node the node
 * @returns TRUE if the node has children, FALSE otherwise
 */
gboolean
gam_tree_has_children(GamTree * tree, GamNode * node)
{
    return(g_node_first_child(node->node) != NULL);
}

/**
 * Gets the number of nodes in the tree
 *
 * @param tree the tree
 * @returns the size of the tree
 */
guint
gam_tree_get_size(GamTree * tree)
{
    return g_hash_table_size(tree->node_hash);
}

/** @} */
