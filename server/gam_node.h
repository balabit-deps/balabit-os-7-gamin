
#ifndef __GAM_NODE_H__
#define __GAM_NODE_H__

#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "gam_event.h"
#include "gam_subscription.h"
#include "gam_fs.h"

G_BEGIN_DECLS

#define FLAG_NEW_NODE 1 << 5

/*
 * Special monitoring modes (pflags)
 */
#define MON_MISSING 1 << 0  /* The resource is missing */
#define MON_NOKERNEL    1 << 1  /* file(system) not monitored by the kernel */
#define MON_BUSY    1 << 2  /* Too busy to be monitored by the kernel */
#define MON_WRONG_TYPE  1 << 3  /* Expecting a directory and got a file */
#define MON_ALL_PFLAGS (MON_MISSING|MON_NOKERNEL|MON_BUSY|MON_WRONG_TYPE)

typedef struct _GamNode GamNode;

typedef gboolean (*GamSubFilterFunc) (GamSubscription *sub);

struct _GamNode {
        /* the node informations proper */
	char *path;		/* The file path */
	GList *subs;		/* the list of subscriptions */
	GNode *node;		/* pointer in the tree */
	gboolean is_dir;	/* is that a directory or expected to be one */
	int flags;		/* generic flags */
	int poll_time;		/* How often this node should be polled */
	gam_fs_mon_type mon_type; /* the type of notification that should be done */

        /* what used to be stored in a separate data structure */
	int checks;
	int pflags;		/* A combination of MON_xxx flags */
	time_t lasttime;	/* Epoch of last time checking was done */
	int flow_on_ticks;	/* Number of ticks while flow control is on */
	struct stat sbuf;	/* The stat() informations in last check */
};


GamNode               *gam_node_new                 (const char     *path,
						   GamSubscription *initial_sub,
						   gboolean        is_dir);

void                  gam_node_free                (GamNode         *node);

GamNode               *gam_node_parent              (GamNode         *node);

gboolean              gam_node_is_dir              (GamNode         *node);

void                  gam_node_set_is_dir          (GamNode         *node,
						   gboolean        is_dir);
	
G_CONST_RETURN char  *gam_node_get_path            (GamNode         *node);

GList                *gam_node_get_subscriptions   (GamNode         *node);

gboolean              gam_node_add_subscription    (GamNode         *node,
						   GamSubscription *sub);

gboolean              gam_node_remove_subscription (GamNode         *node,
						   GamSubscription *sub);

gboolean              gam_node_has_dir_subscriptions(GamNode * node);

void                  gam_node_set_node            (GamNode         *node,
						   GNode          *gnode);
GNode                *gam_node_get_node            (GamNode         *node);

void                  gam_node_set_data            (GamNode         *node,
						   gpointer        data,
						   GDestroyNotify  destroy);

gpointer              gam_node_get_data            (GamNode         *node);

void                  gam_node_set_flag            (GamNode         *node,
						   int             flag);
void                  gam_node_unset_flag          (GamNode         *node,
						   int             flag);
gboolean              gam_node_has_flag            (GamNode         *node,
						   int             flag);

void                  gam_node_set_pflag            (GamNode         *node,
						     int             flag);
void                  gam_node_unset_pflag          (GamNode         *node,
						     int             flag);
gboolean              gam_node_has_pflag            (GamNode         *node,
						     int             flag);
void                  gam_node_set_pflags           (GamNode         *node,
						     int             flags);
gboolean              gam_node_has_pflags           (GamNode         *node,
						     int             flags);


void	gam_node_emit_event (GamNode *node, GaminEventType event);


G_END_DECLS

#endif /* __GAM_NODE_H__ */
