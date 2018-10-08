/* John McCutchan <jmccutchan@novell.com> 2005 */

#include "server_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include "gam_protocol.h"
#include "gam_error.h"
#include "gam_eq.h"

// #define GAM_EQ_VERBOSE
typedef struct {
	int reqno;
	int event;
	char *path;
	int len;
} gam_eq_event_t;

static gam_eq_event_t *
gam_eq_event_new (int reqno, int event, const char *path, int len)
{
	gam_eq_event_t *eq_event = NULL;

	eq_event = g_new0(gam_eq_event_t, 1);
	eq_event->reqno = reqno;
	eq_event->event = event;
	eq_event->path = g_strdup (path);
	eq_event->len = len;

	return eq_event;
}

static void
gam_eq_event_free (gam_eq_event_t *event)
{
	if (!event)
		return;

	g_free (event->path);
	g_free (event);
}

struct _gam_eq {
	GQueue *event_queue;
};

gam_eq_t *
gam_eq_new (void)
{
	gam_eq_t *eq = NULL;

	eq = g_new0(struct _gam_eq, 1);
	eq->event_queue = g_queue_new ();

	return eq;
}

void
gam_eq_free (gam_eq_t *eq)
{
	if (!eq)
		return;

	while (!g_queue_is_empty (eq->event_queue))
	{
		gam_eq_event_t *event = g_queue_pop_head (eq->event_queue);
		g_assert (event);
		gam_eq_event_free (event);
	}
	g_queue_free (eq->event_queue);
	g_free (eq);
}

void
gam_eq_queue (gam_eq_t *eq, int reqno, int event, const char *path, int len)
{
	gam_eq_event_t *eq_event;

	if (!eq)
		return;

	eq_event = g_queue_peek_tail (eq->event_queue);

	/* Check if the last event in the event queue is the same as the one we are attempting to queue
	 * if it is, we can throw this new event away
	 */
	if (eq_event && eq_event->reqno == reqno &&
		eq_event->len == len &&
		eq_event->event == event &&
		!strcmp(eq_event->path, path))
	{
#ifdef GAM_EQ_VERBOSE
		GAM_DEBUG(DEBUG_INFO, "gam_eq: Didn't queue duplicate event\n");
#endif
		return;
	}
	eq_event = gam_eq_event_new (reqno, event, path, len);
	g_queue_push_tail (eq->event_queue, eq_event);
}

guint
gam_eq_size (gam_eq_t *eq)
{
	if (!eq)
		return 0;

	return g_queue_get_length (eq->event_queue);
}

static void
gam_eq_flush_callback (gam_eq_t *eq, gam_eq_event_t *event, GamConnDataPtr conn)
{
	gam_send_event (conn, event->reqno, event->event, event->path, event->len);
	gam_eq_event_free (event);
}

gboolean
gam_eq_flush (gam_eq_t *eq, GamConnDataPtr conn)
{
	gboolean done_work = FALSE;
	if (!eq)
		return;

#ifdef GAM_EQ_VERBOSE
	GAM_DEBUG(DEBUG_INFO, "gam_eq: Flushing event queue for %s\n", gam_connection_get_pidname (conn));
#endif
	while (!g_queue_is_empty (eq->event_queue))
	{
		done_work = TRUE;
		gam_eq_event_t *event = g_queue_pop_head (eq->event_queue);
		g_assert (event);
		gam_eq_flush_callback (eq, event, conn);
	}
	return done_work;
}
