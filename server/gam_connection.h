#ifndef __GAM_CONNECTION_H__
#define __GAM_CONNECTION_H__ 1

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Data associated to connections in the gamin server
 */
typedef struct GamConnData GamConnData;
typedef GamConnData *GamConnDataPtr;

/**
 * the different states the connection can be in
 */
typedef enum {
    GAM_STATE_ERROR = -1,	/* error condition */
    GAM_STATE_AUTH,		/* authenthication needed */
    GAM_STATE_OKAY,		/* normal state */
    GAM_STATE_CLOSED		/* the connection was closed by server */
} GamConnState;

int		gam_connections_init	(void);
int		gam_connections_close	(void);
void            gam_schedule_server_timeout (void);

GamConnDataPtr	gam_connection_new	(GMainLoop *loop,
					 GIOChannel *source);
int		gam_connection_set_pid	(GamConnDataPtr conn,
					 int pid);
int		gam_connection_exists	(GamConnDataPtr conn);
int		gam_connection_close	(GamConnDataPtr conn);

int		gam_connection_get_fd	(GamConnDataPtr conn);
int		gam_connection_get_pid  (GamConnDataPtr conn);
gchar *		gam_connection_get_pidname (GamConnDataPtr conn);
GamConnState	gam_connection_get_state(GamConnDataPtr conn);
int		gam_connection_get_data	(GamConnDataPtr conn,
					 char **data,
					 int *size);
int		gam_connection_data	(GamConnDataPtr conn,
					 int len);
int		gam_send_event		(GamConnDataPtr conn,
					 int reqno,
					 int event,
					 const char *path,
					 int len);
void		gam_queue_event		(GamConnDataPtr conn,
					 int reqno,
					 int event,
					 const char *path,
					 int len);
int		gam_send_ack		(GamConnDataPtr conn,
					 int reqno,
					 const char *path,
					 int len);
void		gam_connections_debug	(void);
#ifdef __cplusplus
}
#endif

#endif /* __GAM_CONNECTION_H__ */
