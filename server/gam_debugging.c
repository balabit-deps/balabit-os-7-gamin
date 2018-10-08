#include "server_config.h"
#include <string.h>
#include "gam_debugging.h"
#include "gam_connection.h"
#include "gam_error.h"
#include "fam.h"

#ifdef GAMIN_DEBUG_API
static GList *gamDebugNotify = NULL;

/**
 * gam_debug_release:
 * @conn: the connection data
 *
 * The connection is being destroyed, release any debugging hook to it
 */
void
gam_debug_release(GamConnDataPtr conn) {
    gamDebugNotify = g_list_remove_all(gamDebugNotify, conn);
}

/**
 * gam_debug_add:
 * @conn: the connection data
 * @value: string describing the debug informations
 * @options: options values, unused yet
 *
 * Register a debugging request for the connection
 *
 * Returns TRUE if succeeded and FALSE otherwise
 */
void
gam_debug_add(GamConnDataPtr conn, const char *value, int options) {
    GAM_DEBUG(DEBUG_INFO, "Got debug request for %s %d\n", value, options);
    if (!strcmp(value, "notify")) {
        gamDebugNotify = g_list_prepend(gamDebugNotify, conn);
    } 
}

/**
 * gam_debug_report:
 * @event: the event
 * @value: the string associated usually a name
 * @extra: an extra integer value
 *
 * Internal callback if compiled with debug, it will then raise debug event
 * on the associated connections.
 */
void
gam_debug_report(GAMDebugEvent event, const char *value, int extra) {
    GList *connlist = NULL, *l;

    switch(event) {
        case GAMDnotifyCreate:
        case GAMDnotifyDelete:
        case GAMDnotifyChange:
        case GAMDnotifyFlowOn:
        case GAMDnotifyFlowOff:
	    connlist = gamDebugNotify;
	    break;
	default:
	    return;
    }
    for (l = connlist; l; l = l->next) {
        GamConnDataPtr conn = l->data;

	gam_send_event(conn, event, 50, value, strlen(value));
    }
}
#endif
