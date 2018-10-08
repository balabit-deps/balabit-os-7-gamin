
#ifndef __MD_DNOTIFY_H__
#define __MD_DNOTIFY_H__

#include <glib.h>
#include "gam_subscription.h"

G_BEGIN_DECLS

gboolean   gam_dnotify_init                  (void);
gboolean   gam_dnotify_add_subscription      (GamSubscription *sub);
gboolean   gam_dnotify_remove_subscription   (GamSubscription *sub);
gboolean   gam_dnotify_remove_all_for        (GamListener *listener);
void       gam_dnotify_debug                 (void);

G_END_DECLS

#endif /* __MD_DNOTIFY_H__ */
