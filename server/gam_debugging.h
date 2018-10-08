#ifndef __GAM_DEBUGGING_H__
#define __GAM_DEBUGGING_H__

#include <glib.h>
#include "gam_event.h"
#include "gam_connection.h"

G_BEGIN_DECLS

typedef enum {
    GAMDnotifyCreate=1,
    GAMDnotifyDelete=2,
    GAMDnotifyChange=3,
    GAMDnotifyFlowOn=4,
    GAMDnotifyFlowOff=5
} GAMDebugEvent;

void gam_debug_add(GamConnDataPtr conn, const char *value, int options);
void gam_debug_release(GamConnDataPtr conn);
void gam_debug_report(GAMDebugEvent event, const char *value, int extra);

G_END_DECLS

#endif /* __GAM_DEBUGGING_H__ */
