#ifndef __GAM_EXCLUDES_H__
#define __GAM_EXCLUDES_H__ 1

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

int		gam_exclude_init	(void);
gboolean	gam_exclude_check	(const char *filename);
int		gam_exclude_add		(const char *pattern, int exclude);
void		gam_exclude_debug	(void);

#ifdef __cplusplus
}
#endif

#endif /* __GAM_EXCLUDES_H__ */

