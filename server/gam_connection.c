#include "server_config.h"
#include <string.h>             /* for memmove */
#include <stdlib.h>             /* for exit() */
#include <time.h>
#include "gam_connection.h"
#include "gam_subscription.h"
#include "gam_listener.h"
#include "gam_server.h"
#include "gam_event.h"
#include "gam_protocol.h"
#include "gam_channel.h"
#include "gam_error.h"
#include "gam_pidname.h"
#include "gam_eq.h"
#ifdef GAMIN_DEBUG_API
#include "gam_debugging.h"
#endif
#ifdef ENABLE_INOTIFY
#include "gam_inotify.h"
#endif
#include "fam.h"

/************************************************************************
 *									*
 *			Connection data handling			*
 *									*
 ************************************************************************/

static GList *gamConnList;

struct GamConnData {
    GamConnState state;         /* the state for the connection */
    int fd;                     /* the file descriptor */
    int pid;                    /* the PID of the remote process */
    gchar *pidname;		/* The name of the process */
    GMainLoop *loop;            /* the Glib loop used */
    GIOChannel *source;         /* the Glib I/O Channel used */
    int request_len;            /* how many bytes of request are valid */
    GAMPacket request;          /* the next request being read */
    GamListener *listener;      /* the listener associated with the connection */
    gam_eq_t *eq;               /* the event queue */
    guint eq_source;            /* the event queue GSource id */
};

static void gam_cancel_server_timeout (void);


static const char *
gam_reqtype_to_string (GAMReqType type)
{
	switch (type)
	{
	case GAM_REQ_FILE:
		return "MONFILE";
	case GAM_REQ_DIR:
		return "MONDIR";
	case GAM_REQ_CANCEL:
		return "CANCEL";
	case GAM_REQ_DEBUG:
		return "4";
	}

	return "";
}

/**
 * gam_connections_init:
 *
 * Initialize the connections data layer
 *
 * Returns 0 on success; -1 on failure
 */
int
gam_connections_init(void)
{
    return (0);
}

/**
 * gam_connection_exists:
 * @conn: the connection
 *
 * Routine to chech whether a connection still exists
 *
 * Returns 1 if still registered, 0 otherwise
 */
int
gam_connection_exists(GamConnDataPtr conn)
{
    g_assert(conn);
    return g_list_find(gamConnList, (gconstpointer) conn) != NULL;
}

/**
 * gam_connection_close:
 * @conn: the connection
 *
 * Routine to close a connection and discard the associated data
 *
 * Returns 0 on success; -1 on error
 */
int
gam_connection_close(GamConnDataPtr conn)
{
    g_assert (conn);
    /* A valid connection is on gamConnList.  */
    g_assert(g_list_find(gamConnList, (gconstpointer) conn));
    g_assert(conn->source);

    /* Kill the queue event source */
    if (conn->eq_source != 0)
      g_source_remove (conn->eq_source);
    /* Flush the event queue */
    gam_eq_flush (conn->eq, conn);
    /* Kill the event queue */
    gam_eq_free (conn->eq);

    if (conn->listener != NULL) {
        gam_listener_free(conn->listener);
    }

#ifdef GAMIN_DEBUG_API
    gam_debug_release(conn);
#endif
    GAM_DEBUG(DEBUG_INFO, "Closing connection %d\n", conn->fd);

    g_io_channel_unref(conn->source);
    gamConnList = g_list_remove(gamConnList, conn);
    g_assert (!g_list_find(gamConnList, conn));
    g_free(conn->pidname);
    g_free(conn);

    if (gamConnList == NULL && gam_server_use_timeout ())
      gam_schedule_server_timeout ();

    return (0);
}

/**
 * gam_connections_close:
 *
 * Close all the registered connections
 *
 * Returns 0 on success; -1 if at least one connection failed to close
 */
int
gam_connections_close(void)
{
    int ret = 0;
    GList *cur;

    while ((cur = g_list_first(gamConnList)) != NULL) {
        if (gam_connection_close((GamConnDataPtr) cur->data) < 0)
	    ret = -1;
    }
    return (ret);
}

/**
 * gam_connection_eq_flush:
 *
 * Flushes the connections event queue
 *
 * returns TRUE
 */
static gboolean
gam_connection_eq_flush (gpointer data)
{
	gboolean work;
	GamConnDataPtr conn = (GamConnDataPtr)data;
	if (!conn)
		return FALSE;

	work = gam_eq_flush (conn->eq, conn);
	if (!work)
		conn->eq_source = 0;
	return work;
}

/**
 * gam_connection_new:
 * @loop: the Glib loop
 * @source: the  Glib I/O Channel 
 *
 * Create a new connection data structure.
 *
 * Returns a new connection structure on success; NULL on error.
 */
GamConnDataPtr
gam_connection_new(GMainLoop *loop, GIOChannel *source)
{
    GamConnDataPtr ret;

    g_assert(loop);
    g_assert(source);

    ret = g_malloc0(sizeof(GamConnData));
    if (ret == NULL)
        return (NULL);

    ret->state = GAM_STATE_AUTH;
    ret->fd = g_io_channel_unix_get_fd(source);
    ret->loop = loop;
    ret->source = source;
    ret->eq = gam_eq_new ();
    ret->eq_source = g_timeout_add (100 /* 100 milisecond */, gam_connection_eq_flush, ret);
    gamConnList = g_list_prepend(gamConnList, ret);

    gam_cancel_server_timeout ();
    
    GAM_DEBUG(DEBUG_INFO, "Created connection %d\n", ret->fd);

    return (ret);
}

/**
 * gam_connection_get_fd:
 * @conn: a connection data structure.
 *
 * Get the file descriptor associated with a connection
 *
 * Returns the file descriptor or -1 in case of error.
 */
int
gam_connection_get_fd(GamConnDataPtr conn)
{
    g_assert(conn);
    return (conn->fd);
}

/**
 * gam_connection_get_pid:
 * @conn: a connection data structure.
 *
 * accessor for the pid associated to the connection
 *
 * Returns the process identifier or -1 in case of error.
 */
int
gam_connection_get_pid(GamConnDataPtr conn)
{
    g_assert(conn);
    return (conn->pid);
}

gchar *
gam_connection_get_pidname(GamConnDataPtr conn)
{
	g_assert (conn);
	return conn->pidname;
}

/**
 * gam_connection_set_pid:
 * @conn: a connection data structure.
 * @pid: the client process id
 *
 * Set the client process id, this also indicate that authentication was done.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int
gam_connection_set_pid(GamConnDataPtr conn, int pid)
{
    g_assert(conn);

    if (conn->state != GAM_STATE_AUTH) {
        GAM_DEBUG(DEBUG_INFO, "Connection in unexpected state: "
		  "not waiting for authentication\n");
        conn->state = GAM_STATE_ERROR;
        return (-1);
    }

    conn->state = GAM_STATE_OKAY;
    conn->pid = pid;
    conn->pidname = gam_get_pidname (pid);
    conn->listener = gam_listener_new(conn, pid);
    if (conn->listener == NULL) {
        GAM_DEBUG(DEBUG_INFO, "Failed to create listener\n");
        conn->state = GAM_STATE_ERROR;
        return (-1);
    }
    return (0);
}

/**
 * gam_connection_get_state:
 * @conn: a connection
 *
 * Accessor for the connection state
 *
 * Returns the connection's connection state 
 */
GamConnState
gam_connection_get_state(GamConnDataPtr conn)
{
    g_assert(conn);
    return (conn->state);
}

/**
 * gam_connection_get_data:
 * @conn: a connection
 * @data: address to store pointer to data
 * @size: amount of data available
 *
 * Get the address and length of the data store for the connection
 *
 * Returns 0 on success; -1 on failure
 */
int
gam_connection_get_data(GamConnDataPtr conn, char **data, int *size)
{
    g_assert(conn);
    g_assert(data);
    g_assert(size);

    *data = (char *) &conn->request + conn->request_len;
    *size = sizeof(GAMPacket) - conn->request_len;

    return (0);
}

/**
 * gam_connection_request:
 *
 * @conn: connection data structure.
 * @req: the request
 *
 * Process a complete request.
 *
 * Returns 0 on success; -1 on error
 */
static int
gam_connection_request(GamConnDataPtr conn, GAMPacketPtr req)
{
    GamSubscription *sub;
    int events;
    gboolean is_dir = TRUE;
    char byte_save;
    int type;
    int options;

    g_assert(conn);
    g_assert(req);
    g_assert(conn->state == GAM_STATE_OKAY);
    g_assert(conn->fd >= 0);
    g_assert(conn->listener);

    type = req->type & 0xF;
    options = req->type & 0xFFF0;
    GAM_DEBUG(DEBUG_INFO, "%s request: from %s, seq %d, type %x options %x\n",
              gam_reqtype_to_string (type), conn->pidname, req->seq, type, options);

    if (req->pathlen >= MAXPATHLEN)
        return (-1);

    /*
     * zero-terminate the string in the buffer, but keep the byte as
     * it may be the first one of the next request.
     */
    byte_save = req->path[req->pathlen];
    req->path[req->pathlen] = 0;

    switch (type) {
        case GAM_REQ_FILE:
        case GAM_REQ_DIR:
            events = GAMIN_EVENT_CHANGED | GAMIN_EVENT_CREATED |
                GAMIN_EVENT_DELETED | GAMIN_EVENT_MOVED |
                GAMIN_EVENT_EXISTS;

	    is_dir = (type == GAM_REQ_DIR);
            sub = gam_subscription_new(req->path, events, req->seq,
	                               is_dir, options);
            gam_subscription_set_listener(sub, conn->listener);
            gam_add_subscription(sub);
            break;
        case GAM_REQ_CANCEL: {
            char *path;
            int pathlen;

            sub = gam_listener_get_subscription_by_reqno(conn->listener,
							 req->seq);
            if (sub == NULL) {
                GAM_DEBUG(DEBUG_INFO,
                          "Cancel: no subscription with reqno %d found\n",
                          req->seq);
		goto error;
	    }

	    GAM_DEBUG(DEBUG_INFO, "Cancelling subscription with reqno %d\n",
		      req->seq);
	    /* We need to make a copy of sub's path as gam_send_ack
	       needs it but gam_listener_remove_subscription frees
	       it.  */
	    path = g_strdup(gam_subscription_get_path(sub));
	    pathlen = gam_subscription_pathlen(sub);

	    gam_listener_remove_subscription(conn->listener, sub);
	    gam_remove_subscription(sub);
#ifdef ENABLE_INOTIFY
	    if ((gam_inotify_is_running()) && (!gam_exclude_check(path))) {
		gam_fs_mon_type type;

                type = gam_fs_get_mon_type (path);
		if (type != GFS_MT_POLL)
		    gam_subscription_free(sub);
	    }
#endif

	    if (gam_send_ack(conn, req->seq, path, pathlen) < 0) {
		GAM_DEBUG(DEBUG_INFO, "Failed to send cancel ack to PID %d\n",
			  gam_connection_get_pid(conn));
	    }
	    g_free(path);
	    break;
        }   
        case GAM_REQ_DEBUG:
#ifdef GAMIN_DEBUG_API
	    gam_debug_add(conn, req->path, options);
#else
            GAM_DEBUG(DEBUG_INFO, "Unhandled debug request for %s\n",
		      req->path);
#endif
            break;
        default:
            GAM_DEBUG(DEBUG_INFO, "Unknown request type %d for %s\n",
                      type, req->path);
            goto error;
    }

    req->path[req->pathlen] = byte_save;
    return (0);
error:
    req->path[req->pathlen] = byte_save;
    return (-1);
}

/**
 * gam_connection_data:
 * @conn: the connection data structure
 * @len: the amount of data added to the request buffer
 *
 * When receiving data, it should be read into an internal buffer
 * retrieved using gam_connection_get_data.  After receiving some
 * incoming data, call this to process the data.
 *
 * Returns 0 in case of success, -1 in case of error
 */
int
gam_connection_data(GamConnDataPtr conn, int len)
{
    GAMPacketPtr req;

    g_assert(conn);
    g_assert(len >= 0);
    g_assert(conn->request_len >= 0);
    g_assert(len + conn->request_len <= (int) sizeof(GAMPacket));

    conn->request_len += len;
    req = &conn->request;

    /*
     * loop processing all complete requests available in conn->request
     */
    while (1) {
        if (conn->request_len < (int) GAM_PACKET_HEADER_LEN) {
            /*
             * we don't have enough data to check the current request
             * keep it as a pending incomplete request and wait for more.
             */
            break;
        }
        /* check the packet total length */
        if (req->len > sizeof(GAMPacket)) {
            GAM_DEBUG(DEBUG_INFO, "malformed request: invalid length %d\n",
		      req->len);
            return (-1);
        }
        /* check the version */
        if (req->version != GAM_PROTO_VERSION) {
            GAM_DEBUG(DEBUG_INFO, "unsupported version %d\n", req->version);
            return (-1);
        }
	if (GAM_REQ_CANCEL != req->type) {
    	    /* double check pathlen and total length */
    	    if ((req->pathlen <= 0) || (req->pathlen > MAXPATHLEN)) {
        	GAM_DEBUG(DEBUG_INFO,
			  "malformed request: invalid path length %d\n",
                	  req->pathlen);
        	return (-1);
    	    }
	}
        if (req->pathlen + GAM_PACKET_HEADER_LEN != req->len) {
            GAM_DEBUG(DEBUG_INFO,
		      "malformed request: invalid packet sizes: %d %d\n",
                      req->len, req->pathlen);
            return (-1);
        }
        /* Check the type of the request: TODO !!! */

        if (conn->request_len < req->len) {
            /*
             * the current request is incomplete, wait for the rest.
             */
            break;
        }

        if (gam_connection_request(conn, req) < 0) {
            GAM_DEBUG(DEBUG_INFO, "gam_connection_request() failed\n");
            return (-1);
        }

        /*
         * process any remaining request piggy-back'ed on the same packet
         */
        conn->request_len -= req->len;
        if (conn->request_len == 0)
            break;

#if defined(__i386__) || defined(__x86_64__)
	req = (void *) req + req->len;
#else
        memmove(&conn->request, (void *)req + req->len, conn->request_len);
#endif
    }

    if ((conn->request_len > 0) && (req != &conn->request))
	memmove(&conn->request, req, conn->request_len);

    return (0);
}


/**
 * gam_send_event:
 * @conn: the connection
 * @event: the event type
 * @path: the path
 *
 * Send an event over a connection
 *
 * Returns 0 on success; -1 on failure
 */
int
gam_send_event(GamConnDataPtr conn, int reqno, int event,
               const char *path, int len)
{
    GAMPacket req;
    size_t tlen;
    int ret;
    int type;

    g_assert(conn);
    g_assert(conn->fd >= 0);
    g_assert(path);
    g_assert(path[len] == '\0');

    if (len >= MAXPATHLEN) {
        GAM_DEBUG(DEBUG_INFO, "File path too long %s\n", path);
        return (-1);
    }

    /*
     * Convert between Gamin/Marmot internal values and FAM ones.
     */
    switch (event) {
        case GAMIN_EVENT_CHANGED:
            type = FAMChanged;
            break;
        case GAMIN_EVENT_CREATED:
            type = FAMCreated;
            break;
        case GAMIN_EVENT_DELETED:
            type = FAMDeleted;
            break;
        case GAMIN_EVENT_MOVED:
            type = FAMMoved;
            break;
        case GAMIN_EVENT_EXISTS:
            type = FAMExists;
            break;
        case GAMIN_EVENT_ENDEXISTS:
            type = FAMEndExist;
            break;
#ifdef GAMIN_DEBUG_API
	case 50:
	    type = 50 + reqno;
	    break;
#endif
        default:
            GAM_DEBUG(DEBUG_INFO, "Unknown event type %d\n", event);
            return (-1);
    }

    GAM_DEBUG(DEBUG_INFO, "Event to %s : %d, %d, %s %s\n", conn->pidname,
              reqno, type, path, gam_event_to_string(event));
    /*
     * prepare the packet
     */
    tlen = GAM_PACKET_HEADER_LEN + len;
    /* We use only local socket so no need for network byte order conversion */
    req.len = (unsigned short) tlen;
    req.version = GAM_PROTO_VERSION;
    req.seq = reqno;
    req.type = (unsigned short) type;
    req.pathlen = len;
    memcpy(req.path, path, len);
    ret = gam_client_conn_write(conn->source, conn->fd, (gpointer) &req, tlen);
    if (!ret) {
        GAM_DEBUG(DEBUG_INFO, "Failed to send event to %s\n", conn->pidname);
        return (-1);
    }
    return (0);
}

/**
 * gam_queue_event:
 * @conn: the connection
 * @event: the event type
 * @path: the path
 *
 * Queue an event to be sent over a connection within the next second.
 * If an identical event is found at the tail of the event queue 
 * no event will be queued.
 */
void
gam_queue_event(GamConnDataPtr conn, int reqno, int event,
                const char *path, int len)
{
	g_assert (conn);
	g_assert (conn->eq);

	gam_eq_queue (conn->eq, reqno, event, path, len);
	if (!conn->eq_source)
	    conn->eq_source = g_timeout_add (100 /* 100 milisecond */, gam_connection_eq_flush, conn);
}


/**
 * gam_send_ack:
 * @conn: the connection data
 * @path: the file/directory path
 *
 * Emit an acknowledge event on the connection
 *
 * Returns 0 on success; -1 on failure
 */
int
gam_send_ack(GamConnDataPtr conn, int reqno,
             const char *path, int len)
{
    GAMPacket req;
    size_t tlen;
    int ret;

    g_assert(conn);
    g_assert(conn->fd >= 0);
    g_assert(path);
    g_assert(len > 0);
    g_assert(path[len] == '\0');

    if (len >= MAXPATHLEN) {
        GAM_DEBUG(DEBUG_INFO,
		  "path (%s)'s length (%d) exceeds MAXPATHLEN (%d)\n",
		  path, len, MAXPATHLEN);
        return (-1);
    }

    GAM_DEBUG(DEBUG_INFO, "Event to %s: %d, %d, %s\n", conn->pidname,
              reqno, FAMAcknowledge, path);

    /*
     * prepare the packet
     */
    tlen = GAM_PACKET_HEADER_LEN + len;
    /* We only use local sockets so no need for network byte order
       conversion */
    req.len = (unsigned short) tlen;
    req.version = GAM_PROTO_VERSION;
    req.seq = reqno;
    req.type = FAMAcknowledge;
    req.pathlen = len;
    memcpy(req.path, path, len);

    ret = gam_client_conn_write(conn->source, conn->fd, (gpointer) &req, tlen);
    if (!ret) {
        GAM_DEBUG(DEBUG_INFO, "Failed to send event to %s\n", conn->pidname);
        return (-1);
    }
    return (0);
}

/************************************************************************
 *									*
 *			Automatic exit handling				*
 *									*
 ************************************************************************/

#define MAX_IDLE_TIMEOUT_MSEC (30*1000) /* 30 seconds */

static guint server_timeout_id = 0;

/**
 * gam_connections_check:
 *
 * This function can be called periodically by e.g. g_timeout_add and
 * shuts the server down if there have been no outstanding connections
 * for a while.
 */
static gboolean
gam_connections_check(void)
{
    server_timeout_id = 0;
    
    if (gamConnList == NULL) {
        GAM_DEBUG(DEBUG_INFO, "Exiting on timeout\n");
	gam_shutdown();
        exit(0);
    }
    return (FALSE);
}

static void
gam_cancel_server_timeout (void)
{
  if (server_timeout_id)
    g_source_remove (server_timeout_id);
  server_timeout_id = 0;
}

void
gam_schedule_server_timeout (void)
{
  gam_cancel_server_timeout ();
  server_timeout_id =
    g_timeout_add(MAX_IDLE_TIMEOUT_MSEC, (GSourceFunc) gam_connections_check, NULL);
}

/**
 * gam_connections_debug:
 *
 * Calling this function generate debugging informations about the set
 * of existing connections.
 */
void
gam_connections_debug(void)
{
#ifdef GAM_DEBUG_ENABLED
    GamConnDataPtr conn;
    GList *cur;

    if (!gam_debug_active)
	return;
    if (gamConnList == NULL) {
	GAM_DEBUG(DEBUG_INFO, "No active connections\n");
	return;
    }

    for (cur = gamConnList; cur; cur = g_list_next(cur)) {
        conn = (GamConnDataPtr) cur->data;
	if (conn == NULL) {
	    GAM_DEBUG(DEBUG_INFO, "Error: connection with no data\n");
	} else {
	    const char *state = "unknown";

	    switch (conn->state) {
	        case GAM_STATE_ERROR:
		    state = "error";
		    break;
	        case GAM_STATE_AUTH:
		    state = "need auth";
		    break;
	        case GAM_STATE_OKAY:
		    state = "okay";
		    break;
	        case GAM_STATE_CLOSED:
		    state = "closed";
		    break;
	    }
	    GAM_DEBUG(DEBUG_INFO, 
	              "Connection fd %d to %s: state %s, %d read\n",
		      conn->fd, conn->pidname, state, conn->request_len);
	    gam_listener_debug(conn->listener);
	}
    }
#endif
}
