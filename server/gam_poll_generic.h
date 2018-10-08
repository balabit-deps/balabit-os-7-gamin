#ifndef __GAM_POLL_GENERIC_H
#define __GAM_POLL_GENERIC_H

#include <glib.h>
#include "gam_server.h"
#include "gam_tree.h"

G_BEGIN_DECLS

gboolean	gam_poll_generic_init			(void);
void		gam_poll_generic_debug			(void);

void		gam_poll_generic_add_missing	(GamNode * node);
void		gam_poll_generic_remove_missing	(GamNode * node);
void		gam_poll_generic_add_busy	(GamNode * node);
void		gam_poll_generic_remove_busy	(GamNode * node);
void		gam_poll_generic_add		(GamNode * node);
void		gam_poll_generic_remove		(GamNode * node);

time_t		gam_poll_generic_get_time		(void);
void		gam_poll_generic_update_time	(void);
time_t		gam_poll_generic_get_delta_time	(time_t pt);

void		gam_poll_generic_trigger_handler(const char *path, pollHandlerMode mode, GamNode *node);

void		gam_poll_generic_scan_directory	(const char *path);
void		gam_poll_generic_scan_directory_internal (GamNode *dir_node);
void		gam_poll_generic_first_scan_dir	(GamSubscription * sub, GamNode * dir_node, const char *dpath);

GamTree *	gam_poll_generic_get_tree		(void);
GList *		gam_poll_generic_get_missing_list (void);
GList *		gam_poll_generic_get_busy_list (void);
GList *		gam_poll_generic_get_all_list (void);
GList *		gam_poll_generic_get_dead_list (void);

void		gam_poll_generic_unregister_node (GamNode * node);
void		gam_poll_generic_prune_tree (GamNode * node);

G_END_DECLS

#endif
