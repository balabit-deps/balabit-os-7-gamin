/* Gamin
 * Copyright (C) 2003 James Willcox, Corey Bowers
 * Copyright (C) 2004 Daniel Veillard, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "server_config.h"
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/stat.h>
#include "gam_error.h"
#include "gam_protocol.h"
#include "gam_event.h"
#include "gam_listener.h"
#include "gam_server.h"
#include "gam_channel.h"
#include "gam_subscription.h"
#include "gam_poll_generic.h"
#ifdef ENABLE_INOTIFY
#include "gam_inotify.h"
#endif
#ifdef ENABLE_DNOTIFY
#include "gam_dnotify.h"
#endif
#ifdef ENABLE_KQUEUE
#include "gam_kqueue.h"
#endif
#ifdef ENABLE_HURD_MACH_NOTIFY
#include "gam_hurd_mach_notify.h"
#endif
#include "gam_excludes.h"
#include "gam_fs.h"
#include "gam_conf.h" 

static int poll_only = 0;
static const char *session;

static GamKernelHandler __gam_kernel_handler = GAMIN_K_NONE;
static gboolean (*__gam_kernel_add_subscription) (GamSubscription *sub) = NULL;
static gboolean (*__gam_kernel_remove_subscription) (GamSubscription *sub) = NULL;
static gboolean (*__gam_kernel_remove_all_for) (GamListener *listener) = NULL;
static void (*__gam_kernel_dir_handler) (const char *path, pollHandlerMode mode) = NULL;
static void (*__gam_kernel_file_handler) (const char *path, pollHandlerMode mode) = NULL;

static GamPollHandler __gam_poll_handler = GAMIN_P_NONE;
static gboolean (*__gam_poll_add_subscription) (GamSubscription *sub) = NULL;
static gboolean (*__gam_poll_remove_subscription) (GamSubscription *sub) = NULL;
static gboolean (*__gam_poll_remove_all_for) (GamListener *listener) = NULL;
static GaminEventType (*__gam_poll_file) (GamNode *node) = NULL;

#ifndef ENABLE_INOTIFY
/**
 * gam_inotify_is_running
 *
 * Unless built with inotify support, always
 * return false.
 */
gboolean
gam_inotify_is_running(void)
{
	return FALSE;
}
#endif


/**
 * gam_exit:
 *
 * Call the shutdown routine, then just exit.
 * This function is designed to be called from a
 * signal handler.
 */
static void
gam_exit(int signo) {
	gam_shutdown();
	exit(0);
}

/**
 * gam_shutdown:
 *
 * Shutdown routine called when the server exits
 */
void
gam_shutdown(void) {
    gam_conn_shutdown(session);
}

/**
 * gam_debug:
 *
 * Debug routine called when the debugging starts
 */
void
gam_show_debug(void) {
	gam_exclude_debug ();
    gam_fs_debug ();
    gam_connections_debug();
#ifdef ENABLE_INOTIFY
    gam_inotify_debug ();
#endif
#ifdef ENABLE_DNOTIFY
    gam_dnotify_debug ();
#endif
    gam_poll_generic_debug();
}

/**
 * gam_init_subscriptions:
 *
 * Initialize the subscription checking backend, on Linux we will use
 * the DNotify kernel support, otherwise the polling module.
 *
 * Return TRUE in case of success and FALSE otherwise
 */
gboolean
gam_init_subscriptions(void)
{
	gam_conf_read ();
	gam_exclude_init();

	if (!poll_only) {
#ifdef ENABLE_INOTIFY
		if (!getenv("GAM_TEST_DNOTIFY") && gam_inotify_init()) {
			GAM_DEBUG(DEBUG_INFO, "Using inotify as backend\n");
			return(TRUE);
		}
#endif
#ifdef ENABLE_DNOTIFY
		if (gam_dnotify_init()) {
			GAM_DEBUG(DEBUG_INFO, "Using dnotify as backend\n");
			return(TRUE);
		}
#endif
#ifdef ENABLE_KQUEUE
		if (gam_kqueue_init()) {
			GAM_DEBUG(DEBUG_INFO, "Using kqueue as backend\n");
			return(TRUE);
		}
#endif
#ifdef ENABLE_HURD_MACH_NOTIFY
		if (gam_hurd_notify_init()) 
		{
			GAM_DEBUG(DEBUG_INFO, "Using hurd notify as backend\n");
			return(TRUE);
		}
#endif	
	}

	if (gam_poll_basic_init()) {
		GAM_DEBUG(DEBUG_INFO, "Using poll as backend\n");
		return(TRUE);
	}

	GAM_DEBUG(DEBUG_INFO, "Cannot initialize any backend\n");

	return(FALSE);
}

/**
 * gam_add_subscription:
 *
 * Register a subscription to the checking backend, on Linux we will use
 * the DNotify kernel support, otherwise the polling module.
 *
 * Return TRUE in case of success and FALSE otherwise
 */
gboolean
gam_add_subscription(GamSubscription * sub)
{
	const char *path = NULL;

	if (sub == NULL)
		return(FALSE);

	path = gam_subscription_get_path (sub);

	if (gam_exclude_check (path)) 
	{
		GAM_DEBUG(DEBUG_INFO, "g_a_s: %s excluded\n", path);
#if ENABLE_INOTIFY
		if (gam_inotify_is_running())
			return gam_poll_add_subscription (sub);
		else
#endif
			return gam_kernel_add_subscription (sub);
	} else {
		gam_fs_mon_type type;
		type = gam_fs_get_mon_type (path);
		if (type == GFS_MT_KERNEL) 
		{
			GAM_DEBUG(DEBUG_INFO, "g_a_s: %s using kernel monitoring\n", path);
			return gam_kernel_add_subscription(sub);
		}
		else if (type == GFS_MT_POLL)
		{
			GAM_DEBUG(DEBUG_INFO, "g_a_s: %s using poll monitoring\n", path);
			return gam_poll_add_subscription (sub);
		}
	}

	return FALSE;
}

/**
 * gam_remove_subscription:
 *
 * Remove a subscription from the checking backend.
 *
 * Return TRUE in case of success and FALSE otherwise
 */
gboolean
gam_remove_subscription(GamSubscription * sub)
{
	const char *path = NULL;

	if (sub == NULL)
		return(FALSE);

	path = gam_subscription_get_path (sub);

	if (gam_exclude_check (path)) 
	{
#if ENABLE_INOTIFY
		if (gam_inotify_is_running())
			return gam_poll_remove_subscription (sub);
		else
#endif
			return gam_kernel_remove_subscription(sub);
	} else {
		gam_fs_mon_type type;
		type = gam_fs_get_mon_type (path);
		if (type == GFS_MT_KERNEL)
			return gam_kernel_remove_subscription(sub);
		else if (type == GFS_MT_POLL)
			return gam_poll_remove_subscription (sub);
	}

	return FALSE;
}

/**
 * @defgroup Daemon Daemon
 *
 */

/**
 * @defgroup Backends Backends
 * @ingroup Daemon
 *
 * One of the goals for Gamin is providing a uniform and consistent
 * monitoring solution, which works even across different platforms.  Different
 * platforms have different kernel-level monitoring systems available (or
 * none at all).  A "backend" simply takes advantage of the services available
 * on a given platform and makes them work with the rest of Gamin.
 * 
 *
 */

static int no_timeout = 0;
static GHashTable *listeners = NULL;
static GIOChannel *socket = NULL;

/**
 * gam_server_use_timeout:
 *
 * Returns TRUE if idle server should exit after a timeout.
 */
gboolean
gam_server_use_timeout (void)
{
  return !no_timeout;
}

/**
 * gam_server_emit_one_event:
 * @path: the file/directory path
 * @event: the event type
 * @sub: the subscription for this event
 * @force: try to force the event though as much as possible
 *
 * Checks which subscriptions are interested in this event and
 * make sure the event are sent to the associated clients.
 */
void
gam_server_emit_one_event(const char *path, int node_is_dir,
                          GaminEventType event, GamSubscription *sub,
			  int force)
{
    int pathlen, len;
    const char *subpath;
    GamListener *listener;
    GamConnDataPtr conn;
    int reqno;


    pathlen = strlen(path);

    if (!gam_subscription_wants_event(sub, path, node_is_dir, event, force))
	return;

    listener = gam_subscription_get_listener(sub);
    if (listener == NULL)
	return;
    conn = (GamConnDataPtr) gam_listener_get_service(listener);
    if (conn == NULL)
	return;

    /*
     * When sending directory related entries, for items in the
     * directory the FAM protocol removes the common direcory part.
     */
    subpath = path;
    len = pathlen;
    if (gam_subscription_is_dir(sub)) {
	int dlen = gam_subscription_pathlen(sub);

	if ((pathlen > dlen + 1) && (path[dlen] == '/')) {
	    subpath += dlen + 1;
	    len -= dlen + 1;
	}
    }

    reqno = gam_subscription_get_reqno(sub);

#ifdef ENABLE_INOTIFY
	if (gam_inotify_is_running())
	{
		gam_queue_event(conn, reqno, event, subpath, len);
	} else
#endif
	{
		if (gam_send_event(conn, reqno, event, subpath, len) < 0) {
		    GAM_DEBUG(DEBUG_INFO, "Failed to send event to PID %d\n",
			  gam_connection_get_pid(conn));
		}
	}
}

/**
 * gam_server_emit_event:
 * @path: the file/directory path
 * @is_dir_node: is the target a directory
 * @event: the event type
 * @subs: the list of subscription for this event
 * @force: force the emission of the events
 *
 * Checks which subscriptions are interested in this event and
 * make sure the event are sent to the associated clients.
 */
void
gam_server_emit_event(const char *path, int is_dir_node, GaminEventType event,
                      GList * subs, int force)
{
    GList *l;
    int pathlen;

    if ((path == NULL) || (subs == NULL))
        return;
    pathlen = strlen(path);

    for (l = subs; l; l = l->next) {
        GamSubscription *sub = l->data;
	gam_server_emit_one_event (path, is_dir_node, event, sub, force);
    }
}

int
gam_server_num_listeners(void)
{
    return g_hash_table_size(listeners);
}

static GamKernelHandler __gam_kernel_handler;
static gboolean (*__gam_kernel_add_subscription) (GamSubscription *sub);
static gboolean (*__gam_kernel_remove_subscription) (GamSubscription *sub);
static gboolean (*__gam_kernel_remove_all_for) (GamListener *listener);

static GamPollHandler __gam_poll_handler;
static gboolean (*__gam_poll_add_subscription) (GamSubscription *sub);
static gboolean (*__gam_poll_remove_subscription) (GamSubscription *sub);
static gboolean (*__gam_poll_remove_all_for) (GamListener *listener);


void
gam_server_install_kernel_hooks (GamKernelHandler name,
				 gboolean (*add)(GamSubscription *sub),
				 gboolean (*remove)(GamSubscription *sub),
				 gboolean (*remove_all)(GamListener *listener),
				 void (*dir_handler)(const char *path, pollHandlerMode mode),
				 void (*file_handler)(const char *path, pollHandlerMode mode))
{
	__gam_kernel_handler = name;
	__gam_kernel_add_subscription = add;
	__gam_kernel_remove_subscription = remove;
	__gam_kernel_remove_all_for = remove_all;
	__gam_kernel_dir_handler = dir_handler;
	__gam_kernel_file_handler = file_handler;
}

void
gam_server_install_poll_hooks (GamPollHandler name,
				gboolean (*add)(GamSubscription *sub),
				gboolean (*remove)(GamSubscription *sub),
				gboolean (*remove_all)(GamListener *listener),
				GaminEventType (*poll_file)(GamNode *node))
{
	__gam_poll_handler = name;
	__gam_poll_add_subscription = add;
	__gam_poll_remove_subscription = remove;
	__gam_poll_remove_all_for = remove_all;
	__gam_poll_file = poll_file;
}

GamKernelHandler
gam_server_get_kernel_handler (void)
{
	return __gam_kernel_handler;
}

GamPollHandler
gam_server_get_poll_handler (void)
{
	return __gam_kernel_handler;
}

gboolean
gam_kernel_add_subscription (GamSubscription *sub)
{
	if (__gam_kernel_add_subscription)
		return __gam_kernel_add_subscription (sub);

	return FALSE;
}

gboolean
gam_kernel_remove_subscription (GamSubscription *sub)
{
	if (__gam_kernel_remove_subscription)
		return __gam_kernel_remove_subscription (sub);

	return FALSE;
}

gboolean
gam_kernel_remove_all_for (GamListener *listener)
{
	if (__gam_kernel_remove_all_for)
		return __gam_kernel_remove_all_for (listener);

	return FALSE;
}

void
gam_kernel_dir_handler(const char *path, pollHandlerMode mode)
{
	if (__gam_kernel_dir_handler)
		__gam_kernel_dir_handler (path, mode);
}

void
gam_kernel_file_handler(const char *path, pollHandlerMode mode)
{
	if (__gam_kernel_file_handler)
		__gam_kernel_file_handler (path, mode);
}

gboolean
gam_poll_add_subscription (GamSubscription *sub)
{
	if (__gam_poll_add_subscription)
		return __gam_poll_add_subscription (sub);

	return FALSE;
}

gboolean
gam_poll_remove_subscription (GamSubscription *sub)
{
	if (__gam_poll_remove_subscription)
		return __gam_poll_remove_subscription (sub);

	return FALSE;
}

gboolean
gam_poll_remove_all_for (GamListener *listener)
{
	if (__gam_poll_remove_all_for)
		return __gam_poll_remove_all_for (listener);

	return FALSE;
}

GaminEventType
gam_poll_file (GamNode *node)
{
	if (__gam_poll_file)
		return __gam_poll_file (node);

	return 0;
}

#ifdef GAM_DEBUG_ENABLED

static GIOChannel *pipe_read_ioc = NULL;
static GIOChannel *pipe_write_ioc = NULL;

static gboolean
gam_error_signal_pipe_handler(gpointer user_data)
{
  char buf[5000];

  if (pipe_read_ioc)
    g_io_channel_read_chars(pipe_read_ioc, buf, sizeof(buf), NULL, NULL);

  gam_error_check();
}  

static void
gam_setup_error_handler (void)
{
  int signal_pipe[2];
  GSource *source;
  
  if (pipe(signal_pipe) != -1) {
    pipe_read_ioc = g_io_channel_unix_new(signal_pipe[0]);
    pipe_write_ioc = g_io_channel_unix_new(signal_pipe[1]);
    
    g_io_channel_set_flags(pipe_read_ioc, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_flags(pipe_write_ioc, G_IO_FLAG_NONBLOCK, NULL);
    
    source = g_io_create_watch(pipe_read_ioc, G_IO_IN | G_IO_HUP | G_IO_ERR);
    g_source_set_callback(source, gam_error_signal_pipe_handler, NULL, NULL);
    
    g_source_attach(source, NULL);
    g_source_unref(source);
  }
}
#endif

void
gam_got_signal()
{
#ifdef GAM_DEBUG_ENABLED
  /* Wake up main loop */
  if (pipe_write_ioc) {
    g_io_channel_write_chars(pipe_write_ioc, "a", 1, NULL, NULL);
    g_io_channel_flush(pipe_write_ioc, NULL);
  }
#endif
}



/**
 * gam_server_init:
 * @loop:  the main event loop of the daemon
 * @session: the session name or NULL
 *
 * Initialize the gamin server
 *
 * Returns TRUE in case of success and FALSE in case of error
 */
static gboolean
gam_server_init(GMainLoop * loop, const char *session)
{
    if (socket != NULL) {
        return (FALSE);
    }
    socket = gam_server_create(session);
    if (socket == NULL)
        return (FALSE);
    g_io_add_watch(socket, G_IO_IN, gam_incoming_conn_read, loop);
    g_io_add_watch(socket, G_IO_HUP | G_IO_NVAL | G_IO_ERR, gam_conn_error,
                   NULL);

    /*
     * Register the timeout checking function
     */
    if (no_timeout == 0)
      gam_schedule_server_timeout ();
#ifdef GAM_DEBUG_ENABLED
    gam_setup_error_handler ();
#endif
    
    return TRUE;
}

int
main(int argc, const char *argv[])
{
    GMainLoop *loop;
    int i;

    if (argc > 1) {
        for (i = 1;i < argc;i++) {
	    if (!strcmp(argv[i], "--notimeout"))
		no_timeout = 1;
            else if (!strcmp(argv[i], "--pollonly"))
	        poll_only = 1;
	    else
		session = argv[i];
	}
    }

    gam_error_init();
    signal(SIGHUP, gam_exit);
    signal(SIGINT, gam_exit);
    signal(SIGQUIT, gam_exit);
    signal(SIGTERM, gam_exit);
    signal(SIGPIPE, SIG_IGN);

    if (!gam_init_subscriptions()) {
	GAM_DEBUG(DEBUG_INFO, "Could not initialize the subscription system.\n");
        exit(0);
    }

    loop = g_main_loop_new(NULL, FALSE);
    if (loop == NULL) {
        g_error("Failed to create the main loop.\n");
        exit(1);
    }

    if (!gam_server_init(loop, session)) {
        GAM_DEBUG(DEBUG_INFO, "Couldn't initialize the server.\n");
        exit(0);
    }

    g_main_loop_run(loop);

    gam_shutdown();

    return (0);
}
