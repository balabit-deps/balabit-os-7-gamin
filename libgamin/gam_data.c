/**
 * gam_data.c: implementation of the connection data handling of libgamin
 */

#include <stdlib.h>
#include <string.h>             /* for memset */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include "fam.h"
#include "gam_data.h"
#include "gam_protocol.h"
#include "gam_error.h"
#include "config.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>

static int is_threaded = -1;
#endif

/*
 * Use weak symbols on gcc/linux to avoid forcing link with pthreads for
 * non threaded apps using the gamin library.
 * patches to extends this to other platforms/compilers welcome, note that
 * this also affects configure.in
 * #pragma weak .... might be more portable but may not work from shared
 * libraries.
 */
#ifdef __GNUC__
#if defined(linux) || defined(__GLIBC__)
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || (__GNUC__ > 3)
extern int pthread_mutexattr_init(pthread_mutexattr_t *attr)
           __attribute((weak));
extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind)
           __attribute((weak));
extern int pthread_mutexattr_init (pthread_mutexattr_t *__attr)
           __attribute((weak));
extern int pthread_mutex_lock (pthread_mutex_t *__mutex)
           __attribute((weak));
extern int pthread_mutex_unlock (pthread_mutex_t *__mutex)
           __attribute((weak));
extern int pthread_mutexattr_destroy (pthread_mutexattr_t *__attr)
           __attribute((weak));
#endif
#endif /* linux */
#endif /* __GNUC__ */

#define FAM_EVENT_SIZE (sizeof(FAMEvent))

#ifdef GAMIN_DEBUG_API
extern int debug_reqno;
extern void *debug_userData;
#endif

typedef enum {
    REQ_NONE = 0,               /* not set */
    REQ_INIT = 1,               /* set but no event yet */
    REQ_CONFIRMED = 2,          /* already got an event */
    REQ_SUSPENDED = 3,          /* Suspended, ignore events */
    REQ_CANCELLED = 4           /* Cancelled, i.e. it was stopped */
} GAMReqState;

/**
 * GAMData:
 *
 * Structure associated to a connection
 */

struct GAMData {
    int reqno;                  /* counter for the requests */
    int auth;			/* did authentication took place */
    int restarted;		/* did authentication took place */
    int noexist;		/* no EXISTS activated */

    int evn_ready;              /* do we have a full event ready */
    int evn_read;               /* how many bytes were read for the event */
    GAMPacket event;            /* the next event being read */
    int evn_reqnum;             /* the reqnum for that event */
    void *evn_userdata;         /* the user data for that event */

    int req_nr;                 /* the number of running requests */
    int req_max;                /* the size of req_tab */
    GAMReqDataPtr *req_tab;     /* pointer to the array of requests */

#ifdef HAVE_PTHREAD_H
    pthread_mutex_t lock;	/* mutex protecting this structure,
				   it's connection, and everything related */
#endif
};

void gamin_data_lock(GAMDataPtr data)
{
#ifdef HAVE_PTHREAD_H
    if (is_threaded > 0)
	pthread_mutex_lock(&data->lock);
#endif
}

void gamin_data_unlock(GAMDataPtr data)
{
#ifdef HAVE_PTHREAD_H
    if (is_threaded > 0)
	pthread_mutex_unlock(&data->lock);
#endif
}
/************************************************************************
 *									*
 *		Request Data handling					*
 *									*
 * A connection can have a number of requests registered. Alls requests	*
 * are assigned an unique id (reqno) for this connection, and the 	*
 * fundamental operation is given a reqno to look up the asoociated	*
 * data. The list of active requests is then sorted by reqno.		*
 * We know that reqnos will only increase (from 0).			*
 *									*
 ************************************************************************/

/**
 * gamin_data_get_req_loc:
 * @conn:  a connection data structure
 * @reqno:  the request number
 *
 * Get the the index of the data associated to a request in the list
 * where it should be inserted if not present.
 *
 * Returns the index or -1 in case of error
 */
static int
gamin_data_get_req_loc(GAMDataPtr conn, int reqno)
{
    int min, max, cur = -1, tmp;

    if (conn == NULL)
        return (-1);

    /*
     * no entry
     */
    if (conn->req_nr == 0)
        return (0);

    /* Use the fact that conn->req_tab is sorted to do a binary lookup */
    min = 0;
    max = conn->req_nr - 1;
    while (max > min) {
        cur = (max + min) / 2;
        if (conn->req_tab[cur] == NULL) {
            gam_error(DEBUG_INFO,
                      "internal error req_tab[%d] is NULL, req_nr = %d \n",
                      cur, conn->req_nr);
            return (-1);
        }
        tmp = conn->req_tab[cur]->reqno;
        if (tmp == reqno) {
            gam_error(DEBUG_INFO,
                      "reqiest number %d already in use\n", reqno);
            return (-1);
	}
        if (tmp < reqno)
            min = cur + 1;
        else
            max = cur - 1;
    }
    if (reqno > conn->req_tab[min]->reqno)
        return(min + 1);
    return (min);
}

static GAMReqDataPtr
gamin_allocate_request(GAMDataPtr conn) {
    GAMReqDataPtr req;

    if (conn == NULL)
        return (NULL);
    if (conn->req_tab == NULL) {
        conn->req_max = 10;
        conn->req_nr = 0;
        conn->req_tab = (GAMReqDataPtr *) malloc(conn->req_max *
                                                 sizeof(GAMReqDataPtr));
        if (conn->req_tab == NULL) {
            gam_error(DEBUG_INFO, "out of memory\n");
            return (NULL);
        }
    } else if (conn->req_nr == conn->req_max) {
        GAMReqDataPtr *tmp;

        tmp = (GAMReqDataPtr *) realloc(conn->req_tab,
                                        2 * conn->req_max *
                                        sizeof(GAMReqDataPtr));
        if (tmp == NULL) {
            gam_error(DEBUG_INFO, "out of memory\n");
            return (NULL);
        }
        conn->req_max *= 2;
        conn->req_tab = tmp;
    } else if (conn->req_nr > conn->req_max) {
        gam_error(DEBUG_INFO,
                  "internal error conn->req_nr %d > conn->req_max %d\n",
                  conn->req_nr, conn->req_max);
        conn->req_nr = conn->req_max;
        return (NULL);
    }
    req = (GAMReqDataPtr) malloc(sizeof(GAMReqData));
    if (req == NULL) {
        gam_error(DEBUG_INFO, "out of memory\n");
        return (NULL);
    }
    memset(req, 0, sizeof(GAMReqData));
    return(req);
}

/**
 * gamin_data_add_req2:
 * @conn:  a connection data structure
 * @type:  the request type
 * @userData:  user data associated to the request
 * @reqno: a request number which must be unique
 *
 * add a new request to the connection
 *
 * Returns a pointer to the new request or NULL in case of error
 */
static GAMReqDataPtr
gamin_data_add_req2(GAMDataPtr conn, const char *filename, int type,
                    void *userData, int reqno)
{
    GAMReqDataPtr req;
    int idx;

    idx = gamin_data_get_req_loc(conn, reqno);
    if (idx < 0)
        return (NULL);

    if ((idx < conn->req_nr) && (conn->req_tab[idx] != NULL) &&
        (conn->req_tab[idx]->reqno == reqno)) {
        gam_error(DEBUG_INFO, "Request %d already exists\n", reqno);
        return (NULL);
    }

    req = gamin_allocate_request(conn);
    if (req == NULL)
        return (NULL);

    req->type = type;
    req->userData = userData;
    req->state = REQ_INIT;
    req->filename = strdup(filename);

    /*
     * insert the request at the indicated slot
     */
    req->reqno = reqno;
    if ((idx < conn->req_nr) && (conn->req_tab[idx] != NULL) &&
        (conn->req_tab[idx]->reqno < reqno)) idx++;
    if (idx < conn->req_nr)
	memmove(&(conn->req_tab[idx + 1]), &(conn->req_tab[idx]),
		(conn->req_nr - idx) * sizeof(GAMReqDataPtr));
    conn->req_tab[idx] = req;
    conn->req_nr++;

    GAM_DEBUG(DEBUG_INFO, "Allocated request %d\n", reqno);

    return (req);
}

/**
 * gamin_data_add_req:
 * @conn:  a connection data structure
 * @type:  the request type
 * @userData:  user data associated to the request
 *
 * add a new request to the connection
 *
 * Returns a pointer to the new request or NULL in case of error
 */
static GAMReqDataPtr
gamin_data_add_req(GAMDataPtr conn, const char *filename, int type,
                   void *userData)
{
    GAMReqDataPtr req;

    req = gamin_allocate_request(conn);
    if (req == NULL)
        return (NULL);

    req->type = type;
    req->userData = userData;
    req->state = REQ_INIT;
    req->filename = strdup(filename);

    /*
     * we can add at the end because we can garantee reqno is always
     * increasing.
     */
    req->reqno = conn->reqno++;
    conn->req_tab[conn->req_nr++] = req;

    return (req);
}

/**
 * gamin_data_get_req_idx:
 * @conn:  a connection data structure
 * @reqno:  the request number
 *
 * Get the the index of the data associated to a request in the list
 *
 * Returns the index or -1 in case of error
 */
static int
gamin_data_get_req_idx(GAMDataPtr conn, int reqno)
{
    int min, max, cur = -1, tmp;

    if (conn == NULL)
        return (-1);

    /* Use the fact that conn->req_tab is sorted to do a binary lookup */
    min = 0;
    max = conn->req_nr - 1;
    while (max >= min) {
        cur = (max + min) / 2;
        if (conn->req_tab[cur] == NULL) {
            gam_error(DEBUG_INFO,
                      "internal error req_tab[%d] is NULL, req_nr = %d \n",
                      cur, conn->req_nr);
            return (-1);
        }
        tmp = conn->req_tab[cur]->reqno;
        if (tmp == reqno)
            return (cur);
        if (tmp < reqno)
            min = cur + 1;
        else
            max = cur - 1;
    }
    GAM_DEBUG(DEBUG_INFO, "request %d not found\n", reqno);

    return (-1);
}

/**
 * gamin_data_del_req:
 * @conn:  a connection data structure
 * @reqno:  the request number
 *
 * Remove a request from the connection
 *
 * Returns -1 in case of error, and 0 in case of success
 */
int
gamin_data_del_req(GAMDataPtr conn, int reqno)
{
    int idx;
    GAMReqDataPtr data;

    idx = gamin_data_get_req_idx(conn, reqno);
    if (idx < 0)
        return (-1);
    data = conn->req_tab[idx];
    if (data->filename !=  NULL)
        free(data->filename);
    free(data);

    /*
     * remove the data and if needed recompact the array
     * removing an element keeps the order.
     */
    conn->req_nr--;
    if (conn->req_nr > idx) {
        memmove(&conn->req_tab[idx], &conn->req_tab[idx + 1],
                (conn->req_nr - idx) * sizeof(GAMReqDataPtr));
    }

    GAM_DEBUG(DEBUG_INFO, "Removed request %d\n", reqno);

    return (0);
}

/**
 * gamin_data_cancel:
 * @conn:  a connection data structure
 *
 * Cancel a connection
 *
 * Returns 0 or 1 in case or -1 in case of error.
 */
int
gamin_data_cancel(GAMDataPtr conn, int reqno)
{
    int idx;
    GAMReqDataPtr data;

    idx = gamin_data_get_req_idx(conn, reqno);
    if (idx < 0)
        return (-1);
    data = conn->req_tab[idx];
    if (data->state == REQ_CANCELLED)
        return(0);
    data->state = REQ_CANCELLED;
    return(1);
}

/**
 * gamin_data_get_req:
 * @conn:  a connection data structure
 * @reqno:  the request number
 *
 * Get the data associated to a request
 *
 * Returns a pointer to the request data or NULL in case of error
 */
static GAMReqDataPtr
gamin_data_get_req(GAMDataPtr conn, int reqno)
{
    int idx;

    if (conn == NULL)
        return (NULL);

    idx = gamin_data_get_req_idx(conn, reqno);
    if (idx < 0) {
        GAM_DEBUG(DEBUG_INFO, "Failed to find request %d\n", reqno);
        return (NULL);
    }
    return (conn->req_tab[idx]);
}

/************************************************************************
 *									*
 *		Connections Data handling				*
 *									*
 ************************************************************************/

/**
 * gamin_data_new:
 *
 * Allocates a new connection data structure
 *
 * Returns the new structure or NULL in case of error.
 */
GAMDataPtr
gamin_data_new(void)
{
    GAMDataPtr ret;
#ifdef HAVE_PTHREAD_H
    pthread_mutexattr_t attr;
#endif

    ret = (GAMDataPtr) malloc(sizeof(GAMData));
    if (ret == NULL)
        return (NULL);
    memset(ret, 0, sizeof(GAMData));
    
#ifdef HAVE_PTHREAD_H
    if (is_threaded == -1) {
        if ((pthread_mutexattr_init != NULL) &&
	    (pthread_mutexattr_settype != NULL) &&
	    (pthread_mutex_init != NULL) &&
	    (pthread_mutex_lock != NULL) &&
	    (pthread_mutex_unlock != NULL) &&
	    (pthread_mutexattr_destroy != NULL)) {
	    is_threaded = 1;
	    GAM_DEBUG(DEBUG_INFO, "Activating thread safety\n");
	} else {
	    GAM_DEBUG(DEBUG_INFO, "Not activating thread safety\n");
	}
    }
    if (is_threaded > 0) {
	pthread_mutexattr_init(&attr);
#if defined(linux) || defined(PTHREAD_MUTEX_RECURSIVE_NP)
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(&ret->lock, &attr);
	pthread_mutexattr_destroy(&attr);
    }
#endif

    ret->auth = 0;
    ret->reqno = 1;
    ret->evn_ready = 0;
    return (ret);
}

/**
 * gamin_data_free:
 * @conn:  a connection data structure
 *
 * Free a connection data structure
 */
void
gamin_data_free(GAMDataPtr conn)
{
    int i;

    if (conn == NULL)
        return;

    if (conn->req_tab != NULL) {
        for (i = 0; i < conn->req_nr; i++) {
            if (conn->req_tab[i] != NULL) {
	        if (conn->req_tab[i]->filename != NULL)
		    free(conn->req_tab[i]->filename);
                free(conn->req_tab[i]);
	    }
        }
        free(conn->req_tab);
    }

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&conn->lock);
    pthread_mutex_destroy(&conn->lock);
#endif

    free(conn);
}

/**
 * gamin_data_reset:
 * @conn:  a connection data structure
 * @requests:  return value for the array of requests pending
 *
 * Reset the state of the data when there is a reconnection.
 *
 * Returns the number of pending requests or -1 in case of error.
 */
int
gamin_data_reset(GAMDataPtr conn, GAMReqDataPtr **requests)
{
    if ((conn == NULL) || (requests == NULL))
        return(-1);

    *requests = &(conn->req_tab[0]);
    conn->auth = 0;
    conn->reqno = 1;
    conn->restarted = 1;
    conn->evn_ready = 0;
    conn->evn_read = 0;
    return(conn->req_nr);
}

/************************************************************************
 *									*
 *		Processing of Events					*
 *									*
 ************************************************************************/

/**
 * gamin_data_read_event:
 * @conn:  a connection data structure
 * @event:  pointer to the event structure to complete
 * 
 * Fills the event structure with the available data
 *
 * Returns 0 if the event is available or -1 in case of error
 */
int
gamin_data_read_event(GAMDataPtr conn, FAMEvent * event)
{
    GAMPacketPtr evn;

    if ((conn == NULL) || (conn->evn_ready != 1) || (event == NULL))
        return (-1);

    memset(event, 0, sizeof(FAMEvent));

    evn = &(conn->event);
    event->hostname = NULL;
    strncpy(&(event->filename[0]), &(evn->path[0]), evn->pathlen);
    event->filename[evn->pathlen] = 0;
    event->userdata = conn->evn_userdata;
    event->fr.reqnum = conn->evn_reqnum;
    event->code = evn->type;
    conn->evn_ready = 0;
    conn->evn_read -= evn->len;
    if (event->code == FAMAcknowledge) {
        /*
         * destroy the request internally
         */
        gamin_data_del_req(conn, evn->seq);
    }
    if (conn->evn_read > 0) {
        /*
         * there was other events piggy-backed on the same read,
         * preserve them
         */
        memmove(evn, &(evn->path[evn->pathlen]), conn->evn_read);
    }
    return (0);
}

/**
 * gamin_data_get_data:
 * @conn:  a connection data structure
 * @data: address to store data
 * @size: amount of storage available
 *
 * Get the address and length of the data store for the connection
 * This is called after authentication sucessed so that is reset too
 *
 * Returns 0 in case of success and -1 in case of failure
 */
int
gamin_data_get_data(GAMDataPtr conn, char **data, int *size)
{
    if ((conn == NULL) || (data == NULL) || (size == NULL))
        return (-1);
    *data = (char *) &conn->event;
    *size = sizeof(GAMPacket);
    *data += conn->evn_read;
    *size -= conn->evn_read;
    return (0);
}

/**
 * gamin_data_conn_event:
 * @conn:  a connection data structure
 * @evt:  the full event packet.
 *
 * Check that the event is okay and expected, if yes conn->evn_ready
 * is set up
 *
 * Returns 1 if the event must be processed, 0 if discarded, and -1 in
 *         case of error.
 */
static int
gamin_data_conn_event(GAMDataPtr conn, GAMPacketPtr evn)
{
    GAMReqDataPtr req;

    if ((conn == NULL) || (evn == NULL))
        return (-1);

#ifdef GAMIN_DEBUG_API
    if (evn->type >= 50) {
        GAM_DEBUG(DEBUG_INFO, "Got Debug Event: type %d, seq %d\n",
	          evn->type, evn->seq);
	conn->evn_ready = 1;
	conn->evn_reqnum = debug_reqno;
	conn->evn_userdata = debug_userData;
        return(1);
    }
#endif
    
    /* Check the event number */
    req = gamin_data_get_req(conn, evn->seq);
    if (req == NULL) {
        GAM_DEBUG(DEBUG_INFO, "Event: seq %d dropped, no request\n",
                  evn->seq);
        return (0);
    }

    switch (req->state) {
        case REQ_NONE:
        case REQ_SUSPENDED:
            GAM_DEBUG(DEBUG_INFO,
                      "Event: seq %d dropped, request type %d\n", evn->seq,
                      req->type);
            return (0);
        case REQ_CANCELLED:
	    if (evn->type == FAMAcknowledge)
	        break;
            GAM_DEBUG(DEBUG_INFO,
                      "Event: seq %d dropped, request type %d\n", evn->seq,
                      req->type);
            return (0);
        case REQ_INIT:
            req->state = REQ_CONFIRMED;
        case REQ_CONFIRMED:
            break;
    }

    /*
     * When a reconnection occurs, skip all events which are emitted
     * by the server to indicate the current state
     */
    if (conn->restarted) {
        if ((evn->type == FAMCreated) || (evn->type == FAMMoved) ||
	    (evn->type == FAMChanged))
	    conn->restarted = 0;
	if (conn->restarted != 0) {
	    if (evn->type == FAMEndExist)
	        conn->restarted = 0;
	    return(0);
	}
    }
    conn->evn_ready = 1;
    conn->evn_reqnum = evn->seq;
    conn->evn_userdata = req->userData;

    GAM_DEBUG(DEBUG_INFO, "accepted event: seq %d, type %d\n",
              evn->seq, evn->type);

    return (1);
}

/**
 * gamin_data_done_auth:
 * @conn: connection data structure.
 *
 * The current connection did authentication sucessfully
 *
 * Returns 0 in case success, -1 in case of failure
 */
int
gamin_data_done_auth(GAMDataPtr conn) {
    if (conn == NULL)
        return(-1);
    conn->auth = 1;
    return(0);
}

/**
 * gamin_data_need_auth:
 * @conn: connection data structure.
 *
 * Is the current connection needing authentication
 *
 * Returns 1 if true, 0 if not needed and -1 in case of error
 */
int
gamin_data_need_auth(GAMDataPtr conn) {
    if (conn == NULL)
        return(-1);
    if (conn->auth == 1)
        return(0);
    if (conn->auth == 0)
        return(1);
    return(-1);
}

/**
 * gamin_data_conn_data:
 * @conn: connection data structure.
 * @len: the event len
 *
 * Received some incoming data, check if there is complete incoming
 * event(s) and process it (them), otherwise make some sanity check
 * and keep the incomplete event in the structure, waiting for more.
 *
 * Returns 0 in case of success and -1 in case of error
 */
int
gamin_data_conn_data(GAMDataPtr conn, int len)
{
    GAMPacketPtr evn;

    if ((conn == NULL) || (len < 0) || (conn->evn_read < 0)) {
        gam_error(DEBUG_INFO, "invalid connection data\n");
        return (-1);
    }
    if ((len + conn->evn_read) > (int) sizeof(GAMPacket)) {
        gam_error(DEBUG_INFO,
                  "detected a data overflow or invalid size\n");
        return (-1);
    }
    conn->evn_read += len;
    evn = &conn->event;

    /*
     * loop processing all complete events available in conn->event
     */
    while (1) {
        if (conn->evn_read < (int) GAM_PACKET_HEADER_LEN) {
            /*
             * we don't have enough data to check the current event
             * keep it as a pending incomplete event and wait for more.
             */
            break;
        }
        /* check the packet total length */
        if (evn->len > sizeof(GAMPacket)) {
            gam_error(DEBUG_INFO, "invalid length %d\n", evn->len);
            return (-1);
        }
        /* check the version */
        if (evn->version != GAM_PROTO_VERSION) {
            gam_error(DEBUG_INFO, "unsupported version %d\n",
                      evn->version);
            return (-1);
        }
        /* double check pathlen and total length */
        if ((evn->pathlen <= 0) || (evn->pathlen > MAXPATHLEN)) {
            gam_error(DEBUG_INFO, "invalid path length %d\n",
                      evn->pathlen);
            return (-1);
        }
        if (evn->pathlen + GAM_PACKET_HEADER_LEN != evn->len) {
            gam_error(DEBUG_INFO, "invalid packet sizes: %d %d\n",
                      evn->len, evn->pathlen);
            return (-1);
        }

        /*
         * We can now decide if the event is complete, if not
         * keep it as a pending incomplete event and wait for more.
         */
        if (conn->evn_read < evn->len) {
            /*
             * we don't have enough data to process the current event
             * keep it as a pending incomplete event and wait for more.
             */
            break;
        }

        if (gamin_data_conn_event(conn, evn) < 0) {
            return (-1);
        }

        /*
         * if the packet was successfully scanned and accepted, break
         * until it got read
         */
        if (conn->evn_ready)
            break;

        /*
         * process any remaining event piggy-back'ed on the same packet
         */
        conn->evn_read -= evn->len;
        if (conn->evn_read == 0)
            break;
        memmove(evn, &(evn->path[evn->pathlen]), conn->evn_read);

    }

    return (0);
}

/**
 * gamin_data_event_ready:
 * @conn:  a connection data structure
 *
 * Is there a full event ready for processing
 *
 * Returns 1 if true, 0 if false and -1 in case of error
 */
int
gamin_data_event_ready(GAMDataPtr conn)
{
    if (conn == NULL)
        return (-1);
    if (conn->evn_ready)
        return (1);
    if (conn->evn_read != 0) {
        /*
         * check if there is a complete packet available
         */
        gamin_data_conn_data(conn, 0);
    }
    return (conn->evn_ready);
}

/**
 * gamin_data_get_reqnum:
 * @conn:  a connection data structure
 * @filename:  the filename associated to the request
 * @type:  the request type
 * @userData:  user data associated to the request
 *
 * Get a new request for a connection
 *
 * Returns the new number or -1 in case of error
 */
int
gamin_data_get_reqnum(GAMDataPtr conn, const char *filename, int type,
                      void *userData)
{
    GAMReqDataPtr req;

    if (conn == NULL)
        return (-1);
    req = gamin_data_add_req(conn, filename, type, userData);
    if (req == NULL)
        return (-1);
    return (req->reqno);
}

/**
 * gamin_data_get_request:
 * @conn:  a connection data structure
 * @filename:  the filename associated to the request
 * @type:  the request type
 * @userData:  user data associated to the request
 * @reqno:  the request number provided by the user
 *
 * Get a new request for a connection, where the request number is provided
 * by the user.
 *
 * Returns the new number or -1 in case of error
 */
int
gamin_data_get_request(GAMDataPtr conn, const char *filename, int type,
                       void *userData, int reqno)
{
    GAMReqDataPtr req;

    if (conn == NULL)
        return (-1);
    req = gamin_data_add_req2(conn, filename, type, userData, reqno);
    if (req == NULL)
        return (-1);
    return (req->reqno);
}

/**
 * gamin_data_no_exists:
 * @conn:  a connection data structure
 *
 * Switch the connection to a mode where no exists are sent on directory
 * monitoting startup.
 *
 * Returns 0 in case of success and -1 in case of error.
 */
int
gamin_data_no_exists(GAMDataPtr conn)
{
    if (conn == NULL)
        return (-1);
    conn->noexist = 1;
    return(0);
}

/**
 * gamin_data_get_exists:
 * @conn:  a connection data structure
 *
 * Get the EXISTS flag for the connection
 *
 * Returns 0 or 1 in case or -1 in case of error.
 */
int
gamin_data_get_exists(GAMDataPtr conn)
{
    if (conn == NULL)
        return (-1);
    if (conn->noexist)
        return(0);
    return(1);
}

