
#ifndef __GAM_SUBSCRIPTION_H__
#define __GAM_SUBSCRIPTION_H__

#include <glib.h>
#include "gam_event.h"
#include "gam_listener.h"

G_BEGIN_DECLS

GamSubscription     *gam_subscription_new          (const char *path,
						    int         events,
						    int         reqno,
						    gboolean    is_dir,
						    int         options);

void                 gam_subscription_free         (GamSubscription *sub);

gboolean             gam_subscription_is_dir       (GamSubscription *sub);
int                  gam_subscription_pathlen      (GamSubscription *sub);

int                  gam_subscription_get_reqno    (GamSubscription *sub);

G_CONST_RETURN char *gam_subscription_get_path     (GamSubscription *sub);

GamListener         *gam_subscription_get_listener (GamSubscription *sub);

void                 gam_subscription_set_listener (GamSubscription *sub,
						    GamListener     *listener);

void                 gam_subscription_set_event    (GamSubscription *sub,
						    int              event);
void                 gam_subscription_unset_event  (GamSubscription *sub,
						    int              event);
gboolean             gam_subscription_has_event    (GamSubscription *sub,
						    int              event);

gboolean             gam_subscription_has_option   (GamSubscription * sub,
						    int              option);
void                 gam_subscription_cancel       (GamSubscription *sub);
gboolean             gam_subscription_is_cancelled (GamSubscription *sub);

gboolean             gam_subscription_wants_event  (GamSubscription *sub,
						    const char      *name,
						    int          is_dir_node,
						    GaminEventType   event,
						    int force);
void                 gam_subscription_debug        (GamSubscription *sub);

void				gam_subscription_shutdown ();

G_END_DECLS

#endif /* __GAM_SUBSCRIPTION_H__ */
