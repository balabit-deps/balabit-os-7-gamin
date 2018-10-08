#ifndef __GAM_SERVER_H__
#define __GAM_SERVER_H__ 1

#include <glib.h>
#include "gam_connection.h"
#include "gam_subscription.h"
#include "gam_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	GAMIN_K_NONE = 0,
	GAMIN_K_DNOTIFY = 1,
	GAMIN_K_INOTIFY = 2,
	GAMIN_K_KQUEUE = 3,
	GAMIN_K_MACH = 4,
	GAMIN_K_INOTIFY2 = 5
} GamKernelHandler;

typedef enum {
	GAMIN_P_NONE = 0,
	GAMIN_P_DNOTIFY = 1,
	GAMIN_P_BASIC = 2
} GamPollHandler;

typedef enum pollHandlerMode {
	GAMIN_ACTIVATE = 1,         /* Activate kernel monitoring */
	GAMIN_DEACTIVATE = 2,       /* Deactivate kernel monitoring */
	GAMIN_FLOWCONTROLSTART = 3, /* Request flow control start */
	GAMIN_FLOWCONTROLSTOP = 4   /* Request flow control stop */
} pollHandlerMode;

gboolean        gam_init_subscriptions          (void);
gboolean        gam_server_use_timeout          (void);
gboolean        gam_add_subscription            (GamSubscription *sub);
gboolean        gam_remove_subscription         (GamSubscription *sub);
int             gam_server_num_listeners        (void);
void		gam_server_emit_one_event	(const char *path,
						 int is_dir_node,
						 GaminEventType event,
						 GamSubscription *sub,
						 int force);
void            gam_server_emit_event           (const char *path,
						 int is_dir_node,
						 GaminEventType event,
						 GList *subs,
						 int force);
void		gam_shutdown			(void);
void		gam_show_debug			(void);
void		gam_got_signal			(void);

void		gam_server_install_kernel_hooks	(GamKernelHandler name,
						 gboolean (*add)(GamSubscription *sub),
						 gboolean (*remove)(GamSubscription *sub),
						 gboolean (*remove_all)(GamListener *listener),
						 void (*dir_handler)(const char *path, pollHandlerMode mode),
						 void (*file_handler)(const char *path, pollHandlerMode mode));

void		gam_server_install_poll_hooks	(GamPollHandler name,
						 gboolean (*add)(GamSubscription *sub),
						 gboolean (*remove)(GamSubscription *sub),
						 gboolean (*remove_all)(GamListener *listener),
						 GaminEventType (*poll_file)(GamNode *node));


GamKernelHandler gam_server_get_kernel_handler	(void);
GamPollHandler	 gam_server_get_poll_handler	(void);

gboolean	gam_kernel_add_subscription	(GamSubscription *sub);
gboolean	gam_kernel_remove_subscription	(GamSubscription *sub);
gboolean	gam_kernel_remove_all_for	(GamListener *listener);
void		gam_kernel_dir_handler		(const char *path, pollHandlerMode mode);
void		gam_kernel_file_handler		(const char *path, pollHandlerMode mode);

gboolean	gam_poll_add_subscription	(GamSubscription *sub);
gboolean	gam_poll_remove_subscription	(GamSubscription *sub);
gboolean	gam_poll_remove_all_for		(GamListener *listener);
GaminEventType	gam_poll_file			(GamNode *node);

#ifdef __cplusplus
}
#endif

#endif /* __GAM_SERVER_H__ */


