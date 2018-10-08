#ifndef __MD_KQUEUE_H__
#define __MD_KQUEUE_H__

#include <glib.h>
#include "gam_subscription.h"

G_BEGIN_DECLS

gboolean   gam_kqueue_init                  (void);
gboolean   gam_kqueue_add_subscription      (GamSubscription *sub);
gboolean   gam_kqueue_remove_subscription   (GamSubscription *sub);
gboolean   gam_kqueue_remove_all_for        (GamListener *listener);

G_END_DECLS

#endif /* __MD_KQUEUE_H__ */
