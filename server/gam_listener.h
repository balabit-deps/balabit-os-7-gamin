
#ifndef __GAM_LISTENER_H__
#define __GAM_LISTENER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GamListener GamListener;

typedef struct _GamSubscription GamSubscription;


GamListener   *gam_listener_new                  (void *service, int pid);

void          gam_listener_free                  (GamListener *listener);

void         *gam_listener_get_service           (GamListener *listener);
int           gam_listener_get_pid               (GamListener *listener);
const char *  gam_listener_get_pidname		 (GamListener *listener);

void          gam_listener_add_subscription     (GamListener     *listener,
						  GamSubscription *sub);

void          gam_listener_remove_subscription  (GamListener     *listener,
						  GamSubscription *sub);

GamSubscription *gam_listener_get_subscription  (GamListener *listener,
						  const char *path);

GamSubscription *gam_listener_get_subscription_by_reqno   (GamListener *listener,
							   int reqno);

GList        *gam_listener_get_subscriptions    (GamListener *listener);

gboolean      gam_listener_is_subscribed        (GamListener *listener,
						 const char *path); 

void		gam_listener_debug		(GamListener * listener);
G_END_DECLS

#endif /* __GAM_LISTENER_H__ */
