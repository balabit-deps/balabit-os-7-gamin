#ifndef __GAM_INOTIFY_H__
#define __GAM_INOTIFY_H__

#include <glib.h>
#include "gam_subscription.h"

G_BEGIN_DECLS

gboolean   gam_inotify_init                  (void);
gboolean   gam_inotify_add_subscription      (GamSubscription *sub);
gboolean   gam_inotify_remove_subscription   (GamSubscription *sub);
gboolean   gam_inotify_remove_all_for        (GamListener *listener);
void       gam_inotify_debug                 (void); 
gboolean   gam_inotify_is_running            (void);

G_END_DECLS

#endif /* __GAM_INOTIFY_H__ */
