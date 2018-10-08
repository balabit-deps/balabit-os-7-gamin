#ifndef __GAM_EQ_H
#define __GAM_EQ_H

#include "gam_connection.h"

typedef struct _gam_eq gam_eq_t;

gam_eq_t *		gam_eq_new	(void);
void			gam_eq_free	(gam_eq_t *eq);
void			gam_eq_queue	(gam_eq_t *eq, int reqno, int event, const char *path, int len); 
guint			gam_eq_size	(gam_eq_t *eq);
gboolean		gam_eq_flush	(gam_eq_t *eq, GamConnDataPtr conn);

#endif
