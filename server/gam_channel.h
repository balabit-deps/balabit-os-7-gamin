#ifndef __GAM_CHANNEL_H__
#define __GAM_CHANNEL_H__ 1

#include <glib.h>
#include "gam_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

GIOChannel*	gam_server_create	(const char *session);
GIOChannel*	gam_client_create	(GIOChannel* server);
void		gam_client_conn_shutdown(GIOChannel *source,
					 GamConnDataPtr conn);
gboolean	gam_conn_error		(GIOChannel *source,
					 GIOCondition condition,
					 gpointer data);

gboolean	gam_incoming_conn_read	(GIOChannel *source,
					 GIOCondition condition,
					 gpointer data);
gboolean	gam_client_conn_write	(GIOChannel *target,
					 int fd,
					 gpointer data,
					 size_t len);
void		gam_conn_shutdown	(const char *session);
#ifdef __cplusplus
}
#endif

#endif /* __GAM_CHANNEL_H__ */

