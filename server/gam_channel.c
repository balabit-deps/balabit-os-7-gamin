#include "server_config.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/uio.h>
#include "gam_error.h"
#include "gam_connection.h"
#include "gam_channel.h"
#include "gam_protocol.h"

/* #define CHANNEL_VERBOSE_DEBUGGING */
/************************************************************************
 *									*
 *			Connection socket handling			*
 *									*
 ************************************************************************/

/**
 * gam_client_conn_send_cred:
 *
 * The write read on the connection send a NUL byte to allow the client
 * to check the server credentials early on.
 */
static gboolean
gam_client_conn_send_cred(int fd)
{
    char data[2] = { 0, 0 };
    int written;
#if defined(HAVE_CMSGCRED) && !defined(LOCAL_CREDS)
    struct {
	    struct cmsghdr hdr;
	    struct cmsgcred cred;
    } cmsg;
    struct iovec iov;
    struct msghdr msg;

    iov.iov_base = &data[0];
    iov.iov_len = 1;

    memset (&msg, 0, sizeof (msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = &cmsg;
    msg.msg_controllen = sizeof (cmsg);
    memset (&cmsg, 0, sizeof (cmsg));
    cmsg.hdr.cmsg_len = sizeof (cmsg);
    cmsg.hdr.cmsg_level = SOL_SOCKET;
    cmsg.hdr.cmsg_type = SCM_CREDS;
#endif

retry:
#if defined(HAVE_CMSGCRED) && !defined(LOCAL_CREDS)
    written = sendmsg(fd, &msg, 0);
#else
    written = write(fd, &data[0], 1);
#endif
    if (written < 0) {
        if (errno == EINTR)
            goto retry;
	gam_error(DEBUG_INFO,
		  "Failed to write credential bytes to socket %d\n", fd);
	return (-1);
    }
    if (written != 1) {
	gam_error(DEBUG_INFO, "Wrote %d credential bytes to socket %d\n",
		  written, fd);
	return (-1);
    }
#ifdef CHANNEL_VERBOSE_DEBUGGING
    GAM_DEBUG(DEBUG_INFO, "Wrote credential bytes to socket %d\n", fd);
#endif
    return (written);
}

/**
 * gam_client_conn_check_cred:
 *
 * The first read on the connection gathers credentials from the client
 * and checks them. Parts directly borrowed from DBus code.
 */
static gboolean
gam_client_conn_check_cred(GIOChannel * source, int fd,
                           GamConnDataPtr conn)
{
    struct msghdr msg;
    struct iovec iov;
    char buf;
    pid_t c_pid;
    uid_t c_uid, s_uid;
    gid_t c_gid;

#ifdef HAVE_CMSGCRED
    struct {
	    struct cmsghdr hdr;
	    struct cmsgcred cred;
    } cmsg;
#endif

    s_uid = getuid();

#if defined(LOCAL_CREDS) && defined(HAVE_CMSGCRED)
    /* Set the socket to receive credentials on the next message */
    {
        int on = 1;

        if (setsockopt(fd, 0, LOCAL_CREDS, &on, sizeof(on)) < 0) {
            gam_error(DEBUG_INFO, "Unable to set LOCAL_CREDS socket option\n");
            return FALSE;
        }
    }
#endif

    iov.iov_base = &buf;
    iov.iov_len = 1;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

#ifdef HAVE_CMSGCRED
    memset(&cmsg, 0, sizeof(cmsg));
    msg.msg_control = &cmsg;
    msg.msg_controllen = sizeof(cmsg);
#endif

  retry:
    if (recvmsg(fd, &msg, 0) < 0) {
        if (errno == EINTR)
            goto retry;

        GAM_DEBUG(DEBUG_INFO, "Failed to read credentials byte on %d\n", fd);
        goto failed;
    }

    if (buf != '\0') {
        GAM_DEBUG(DEBUG_INFO, "Credentials byte was not nul on %d\n", fd);
        goto failed;
    }
#ifdef HAVE_CMSGCRED
    if (cmsg.hdr.cmsg_len < sizeof(cmsg) || cmsg.hdr.cmsg_type != SCM_CREDS) {
        GAM_DEBUG(DEBUG_INFO,
                  "Message from recvmsg() was not SCM_CREDS\n");
        goto failed;
    }
#endif

    GAM_DEBUG(DEBUG_INFO, "read credentials byte\n");

    {
#ifdef SO_PEERCRED
        struct ucred cr;
        socklen_t cr_len = sizeof(cr);

        if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &cr_len) ==
            0 && cr_len == sizeof(cr)) {
            c_pid = cr.pid;
            c_uid = cr.uid;
            c_gid = cr.gid;
        } else {
            GAM_DEBUG(DEBUG_INFO,
                      "Failed to getsockopt() credentials on %d, returned len %d/%d\n",
                      fd, cr_len, (int) sizeof(cr));
            goto failed;
        }
#elif defined(HAVE_CMSGCRED)
	c_pid = cmsg.cred.cmcred_pid;
	c_uid = cmsg.cred.cmcred_euid;
	c_gid = cmsg.cred.cmcred_groups[0];
#else /* !SO_PEERCRED && !HAVE_CMSGCRED */
        GAM_DEBUG(DEBUG_INFO,
                  "Socket credentials not supported on this OS\n");
        goto failed;
#endif
    }

    if (s_uid != c_uid) {
        GAM_DEBUG(DEBUG_INFO,
                  "Credentials check failed: s_uid %d, c_uid %d\n",
                  (int) s_uid, (int) c_uid);
        goto failed;
    }
    GAM_DEBUG(DEBUG_INFO,
              "Credentials: s_uid %d, c_uid %d, c_gid %d, c_pid %d\n",
              (int) s_uid, (int) c_uid, (int) c_gid, (int) c_pid);

    if (gam_connection_set_pid(conn, c_pid) < 0) {
        GAM_DEBUG(DEBUG_INFO, "Failed to save PID\n");
        goto failed;
    }

    if (!gam_client_conn_send_cred(fd)) {
        GAM_DEBUG(DEBUG_INFO, "Failed to send credential byte to client\n");
        goto failed;
    }

    return TRUE;

failed:
    gam_client_conn_shutdown(source, conn);
    return (FALSE);
}

/**
 * gam_client_conn_read:
 *
 * Incoming data on the socket.
 */
static gboolean
gam_client_conn_read(GIOChannel * source, GIOCondition condition,
                     gpointer info)
{
    char *data; /* actually a GAMPacketPtr */
    int size;
    int fd;
    int ret;
    GamConnDataPtr conn = (GamConnDataPtr) info;

    if ((condition == G_IO_HUP) || (condition == G_IO_NVAL) ||
        (condition == G_IO_ERR)) {
        return (gam_conn_error(source, condition, info));
    }
    if (conn == NULL) {
        GAM_DEBUG(DEBUG_INFO, "lost informations\n");
        return (FALSE);
    }
    GAM_DEBUG(DEBUG_INFO, "gam_client_conn_read called\n");
    fd = gam_connection_get_fd(conn);
    if (fd < 0) {
        GAM_DEBUG(DEBUG_INFO, "failed to get file descriptor\n");
        return (FALSE);
    }

    switch (gam_connection_get_state(conn)) {
        case GAM_STATE_AUTH:
            if (!gam_client_conn_check_cred(source, fd, conn))
	        return (FALSE);
	    return (TRUE);
        case GAM_STATE_ERROR:
            GAM_DEBUG(DEBUG_INFO, "connection in error state\n");
            return (FALSE);
        case GAM_STATE_CLOSED:
            GAM_DEBUG(DEBUG_INFO, "connection is closed\n");
            return (FALSE);
        case GAM_STATE_OKAY:
            break;
    }

    if (gam_connection_get_data(conn, &data, &size) < 0) {
        GAM_DEBUG(DEBUG_INFO, "connection data error, disconnecting\n");
        gam_client_conn_shutdown(source, conn);
        return (FALSE);
    }

retry:
    ret = read(fd, data, size);
    if (ret < 0) {
        if (errno == EINTR)
            goto retry;
        GAM_DEBUG(DEBUG_INFO, "failed to read() from client connection\n");
        gam_client_conn_shutdown(source, conn);
        return (FALSE);
    }
    if (ret == 0) {
        GAM_DEBUG(DEBUG_INFO, "end from client connection\n");
        gam_client_conn_shutdown(source, conn);
        return (FALSE);
    }
#ifdef CHANNEL_VERBOSE_DEBUGGING
    GAM_DEBUG(DEBUG_INFO, "read %d bytes from client\n", ret);
#endif

    /* 
     * there is no garantee of alignment, that the request is complete
     * we may also get multiple requests in a single packet, so make no
     * assumption on data contant at this point, though in most case it
     * will be a complete, fully aligned request.
     */
    if (gam_connection_data(conn, ret) < 0) {
        GAM_DEBUG(DEBUG_INFO,
                  "error in client data, closing client connection\n");
        gam_client_conn_shutdown(source, conn);
        return (FALSE);
    }
    return (TRUE);
}

/**
 * gam_incoming_conn_read:
 *
 * Incoming data on the socket.
 */
gboolean
gam_incoming_conn_read(GIOChannel * source, GIOCondition condition,
                       gpointer data)
{
    GMainLoop *loop;
    GIOChannel *socket = NULL;
    GamConnDataPtr conn;

    GAM_DEBUG(DEBUG_INFO, "gam_incoming_conn_read called\n");

    loop = (GMainLoop *) data;

    socket = gam_client_create(source);
    if (socket == NULL)
        goto failed;
    conn = gam_connection_new(loop, socket);
    if (conn == NULL)
        goto failed;

    g_io_add_watch(socket, G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR,
                   gam_client_conn_read, conn);
    return (TRUE);

  failed:
    if (socket != NULL)
        g_io_channel_unref(socket);
    return (FALSE);
}

// #define GAM_CHANNEL_VERBOSE

/************************************************************************
 *									*
 *			Path for the socket connection			*
 *									*
 ************************************************************************/

/**
 * gam_get_socket_path:
 * @session: the session name or NULL
 *
 * Get the file path to the socket to connect the FAM server.
 * The fam server interface is available though a socket whose
 * id is available though an environment variable GAM_CLIENT_ID
 * or passed as the @session argument though the command line.
 *
 * Returns a new string or NULL in case of error.
 */
static gchar *
gam_get_socket_path(const char *session)
{
    const char *gam_client_id;
    const gchar *user;
    gchar *ret;

    if (session == NULL) {
        gam_client_id = g_getenv("GAM_CLIENT_ID");
        if (gam_client_id == NULL) {
            GAM_DEBUG(DEBUG_INFO, "Error getting GAM_CLIENT_ID\n");
	    gam_client_id = "";
        }
    } else {
        gam_client_id = session;
    }
    user = g_get_user_name();

    if (user == NULL) {
        GAM_DEBUG(DEBUG_INFO, "Error getting user informations\n");
        return (NULL);
    }
#ifdef HAVE_ABSTRACT_SOCKETS
    ret = g_strconcat("/tmp/fam-", user, "-", gam_client_id, NULL);
#else
    ret = g_strconcat("/tmp/fam-", user, "/fam-", gam_client_id, NULL);
#endif
    return(ret);
}


#ifndef HAVE_ABSTRACT_SOCKETS
/**
 * gam_get_socket_dir:
 *
 * Get the directory path to the socket to connect the FAM server.
 *
 * Returns a new string or NULL in case of error.
 */
static gchar *
gam_get_socket_dir(void)
{
    const gchar *user;
    gchar *ret;

    user = g_get_user_name();

    if (user == NULL) {
        GAM_DEBUG(DEBUG_INFO, "Error getting user informations\n");
        return (NULL);
    }
    ret = g_strconcat("/tmp/fam-", user, NULL);
    return(ret);
}



/************************************************************************
 *									*
 *		Security for OSes without abstract sockets		*
 *									*
 ************************************************************************/
/**
 * gam_check_secure_dir:
 *
 * Tries to create or ensure that the directory used to hold the socket used
 * for communicating with is a safe directory to avoid possible attacks.
 *
 * Returns TRUE if safe and FALSE otherwise.
 */
static gboolean
gam_check_secure_dir(void)
{
    gchar *dir;
    struct stat st;
    int ret;
    int tries = 0;

    dir = gam_get_socket_dir();
    if (dir == NULL) {
	gam_error(DEBUG_INFO, "Failed to get path to socket directory\n");
        return(FALSE);
    }
create:
    ret = mkdir(dir, 0700);
    if (ret >= 0) {
	GAM_DEBUG(DEBUG_INFO, "Created socket directory %s\n", dir);
	g_free(dir);
        return(TRUE);
    }

    switch (errno) {
        case EEXIST:
	    ret = stat(dir, &st);
	    if (ret < 0) {
		gam_error(DEBUG_INFO, "Failed to stat socket directory %s\n",
		          dir);
		g_free(dir);
		return(FALSE);
	    }
	    if (st.st_uid != getuid()) {
		gam_error(DEBUG_INFO,
		          "Socket directory %s has different owner\n",
		          dir);
	        break;
	    }
	    if (!S_ISDIR (st.st_mode)) {
		gam_error(DEBUG_INFO, "Socket path %s is not a directory\n",
		          dir);
	        break;
	    }
	    if (st.st_mode & (S_IRWXG|S_IRWXO)) {
		gam_error(DEBUG_INFO,
		          "Socket directory %s has wrong permissions\n",
		          dir);
	        break;
	    }
	    if (((st.st_mode & (S_IRWXU)) != S_IRWXU)) {
		gam_error(DEBUG_INFO,
		          "Socket directory %s has wrong permissions\n",
		          dir);
	        break;
	    }
	    /*
	     * all checks on existing dir seems okay
	     */
	    GAM_DEBUG(DEBUG_INFO, "Reusing socket directory %s\n", dir);
	    g_free(dir);
	    return(TRUE);
        case EACCES:
	    gam_error(DEBUG_INFO, 
	              "No permission to create socket directory %s\n",
	              dir);
	    g_free(dir);
	    return(FALSE);
        case ENAMETOOLONG:
	    gam_error(DEBUG_INFO, "Socket directory %s name too long\n",
	              dir);
	    g_free(dir);
	    return(FALSE);
        default:
	    gam_error(DEBUG_INFO,
	              "Failed to create socket directory %s\n",
		      dir);
	    g_free(dir);
	    return(FALSE);
    }

    /*
     * The path to the directory is considered unsafe
     * try to remove the given path to rebuild the directory.
     */
    ret = rmdir(dir);
    if (ret < 0) {
	ret = unlink(dir);
	if (ret < 0) {
	    gam_error(DEBUG_INFO, "Failed to remove unsafe path %s\n",
	              dir);
	    g_free(dir);
	    return(FALSE);
	}
    }
#ifdef GAM_CHANNEL_VERBOSE
    GAM_DEBUG(DEBUG_INFO, "Removed %s\n", dir);
#endif
    tries++;
    if (tries < 5)
        goto create;
    g_free(dir);
    return(FALSE);
}

/**
 * gam_check_secure_path:
 * @path: path to the (possibly abstract) socket
 *
 * Tries to create or ensure that the socket used for communicating with
 * the clients are in a safe directory to avoid possible attacks.
 *
 * Returns the socket file descriptor or -1 in case of error.
 */
static gboolean
gam_check_secure_path(const char *path)
{
    struct stat st;
    int ret;

    if (!gam_check_secure_dir())
        return(FALSE);
    /*
     * Check the existing socket if any
     */
    ret = stat(path, &st);
    if (ret < 0)
	return(TRUE);
    
    if (st.st_uid != getuid()) {
	gam_error(DEBUG_INFO,
		  "Socket %s has different owner\n",
		  path);
	goto cleanup;
    }
#ifdef S_ISSOCK
    if (!S_ISSOCK (st.st_mode)) {
	gam_error(DEBUG_INFO, "Socket path %s is not a socket\n",
		  path);
	goto cleanup;
    }
#endif
    if (st.st_mode & (S_IRWXG|S_IRWXO)) {
	gam_error(DEBUG_INFO,
		  "Socket %s has wrong permissions\n",
		  path);
	goto cleanup;
    }
    /*
     * Looks good though binding may fail due to an existing server
     */
    return(TRUE);

cleanup:
    /*
     * the existing file at the socket location seems strange, try to remove it
     */
    ret = unlink(path);
    if (ret < 0) {
	gam_error(DEBUG_INFO, "Failed to remove %s\n", path);
	return(FALSE);
    }
    return(TRUE);
}
#endif /* ! HAVE_ABSTRACT_SOCKETS */

/************************************************************************
 *									*
 *			Connection socket shutdown			*
 *									*
 ************************************************************************/

/**
 * gam_conn_shutdown:
 * @session: the session name or NULL
 *
 * Shutdown the connection socket if any
 */
void
gam_conn_shutdown(const char *session) {
#ifndef HAVE_ABSTRACT_SOCKETS
    gchar *path;
    int ret;

    path = gam_get_socket_path(session);

    ret = unlink(path);
    if (ret < 0) {
	gam_error(DEBUG_INFO, "Failed to remove %s\n", path);
    }
    g_free(path);
#endif
}

/************************************************************************
 *									*
 *			Connection socket handling			*
 *									*
 ************************************************************************/

/**
 * gam_listen_unix_socket:
 * @path: path to the (possibly abstract) socket

 * Returns the socket file descriptor or -1 in case of error.
 */
static int
gam_listen_unix_socket(const char *path)
{
    int fd;
    struct sockaddr_un addr;

    fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        GAM_DEBUG(DEBUG_INFO, "Failed to create unix socket");
        return (-1);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
#ifdef HAVE_ABSTRACT_SOCKETS
    /*
     * Abstract socket do not hit the filesystem
     */
    addr.sun_path[0] = '\0';
    strncpy(&addr.sun_path[1], path, (sizeof(addr) - 4) - 2);
#else
    /*
     * if the socket is exposed at the filesystem level we need to take
     * some extra protection checks. Also make sure the socket is created
     * with restricted mode
     */
    if (!gam_check_secure_path(path)) {
	return (-1);
    }
    strncpy(&addr.sun_path[0], path, (sizeof(addr) - 4) - 1);
    umask(0077);
#endif

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        GAM_DEBUG(DEBUG_INFO, "Failed to bind to socket %s\n", path);
        close(fd);
        return (-1);
    }
    if (listen(fd, 30 /* backlog */ ) < 0) {
        GAM_DEBUG(DEBUG_INFO, "Failed to listen to socket %s\n", path);
        close(fd);
        return (-1);
    }
    GAM_DEBUG(DEBUG_INFO, "Ready listening to socket %s : %d\n", path, fd);

    return (fd);
}

/************************************************************************
 *									*
 *			General channel interface			*
 *									*
 ************************************************************************/

/**
 * gam_client_conn_shutdown:
 *
 * shutdown a Glib I/O channel initiated by the server
 */
void
gam_client_conn_shutdown(GIOChannel * source, GamConnDataPtr conn)
{
    GError *error = NULL;

    if (conn != NULL) {
        if (gam_connection_exists(conn)) {
            GAM_DEBUG(DEBUG_INFO, "Shutting down client socket %d\n",
                      g_io_channel_unix_get_fd(source));
            g_io_channel_shutdown(source, FALSE, &error);
            gam_connection_close(conn);
        } else {
            GAM_DEBUG(DEBUG_INFO,
                      "could not found connection on socket %d\n",
                      g_io_channel_unix_get_fd(source));
        }
    } else {
        GAM_DEBUG(DEBUG_INFO, "Shutting down server socket %d\n",
                  g_io_channel_unix_get_fd(source));
        g_io_channel_shutdown(source, FALSE, &error);
        g_io_channel_unref(source);
    }
}

/**
 * gam_incoming_conn_error:
 *
 * shutdown a Glib I/O channel initiated by an error on the socket
 */
gboolean
gam_conn_error(GIOChannel * source, GIOCondition condition, gpointer data)
{
    GError *error = NULL;
    GamConnDataPtr conn = (GamConnDataPtr) data;

    if (conn != NULL) {
        if (gam_connection_exists(conn)) {
            GAM_DEBUG(DEBUG_INFO,
                      "Error condition raised on client socket %d\n",
                      g_io_channel_unix_get_fd(source));
            g_io_channel_shutdown(source, FALSE, &error);
            gam_connection_close(conn);
        } else {
            GAM_DEBUG(DEBUG_INFO,
                      "could not found connection on socket %d\n",
                      g_io_channel_unix_get_fd(source));
        }
    } else {
        GAM_DEBUG(DEBUG_INFO, "Error condition raised on server socket\n");
        g_io_channel_shutdown(source, FALSE, &error);
        g_io_channel_unref(source);
    }

    return (FALSE);
}

/**
 * gam_channel_create:
 * @session: the session name or NULL
 *
 * Creation of a channel on which the server waits for clients
 *
 * Returns the new GIOChannel or NULL in case of error.
 */
GIOChannel *
gam_server_create(const char *session)
{
    GIOChannel *socket;
    gchar *path = NULL;
    int fd = -1;

    path = gam_get_socket_path(session);
    if (path == NULL)
        return (NULL);
    fd = gam_listen_unix_socket(path);
    g_free(path);
    if (fd == -1)
        return (NULL);
    socket = g_io_channel_unix_new(fd);
    if (socket == NULL)
        close(fd);
    else
        g_io_channel_set_close_on_unref(socket, TRUE);
    return (socket);
}

/**
 * gam_client_create:
 *
 * Creation of a channel on which the server connects to the client
 *
 * Returns the new GIOChannel or NULL in case of error.
 */
GIOChannel *
gam_client_create(GIOChannel * server)
{
    GIOChannel *socket = NULL;
    int sock;
    int client = -1;
    socklen_t client_addrlen;
    struct sockaddr client_addr;

    sock = g_io_channel_unix_get_fd(server);
    if (sock < 0) {
        GAM_DEBUG(DEBUG_INFO, "failed to get incoming socket\n");
        return (NULL);
    }
  retry:
    client_addrlen = sizeof(client_addr);
    client = accept(sock, &client_addr, &client_addrlen);
    if (client < 0) {
        if (errno == EINTR)
            goto retry;
        GAM_DEBUG(DEBUG_INFO, "failed to accept() incoming connection\n");
        return (NULL);
    }
    socket = g_io_channel_unix_new(client);
    if (socket == NULL) {
        if (client != -1)
            close(client);
        return (NULL);
    }
    g_io_channel_set_close_on_unref(socket, TRUE);
    GAM_DEBUG(DEBUG_INFO, "accepted incoming connection: %d\n", client);
    return (socket);
}

/**
 * gam_client_conn_write:
 *
 * Incoming data on the socket.
 */
gboolean
gam_client_conn_write(GIOChannel * source, int fd, gpointer data,
                      size_t len)
{
    int written;
    int remaining;

    /**
     * Todo: check if write will block, or use non-blocking options
     */
    if (fd < 0) {
        fd = g_io_channel_unix_get_fd(source);
    }
    if (fd < 0)
        return (FALSE);

    remaining = len;
    do {
	written = write(fd, data, remaining);
	if (written < 0) {
	    if (errno == EINTR)
		continue;

	    GAM_DEBUG(DEBUG_INFO,
		      "%s: Failed to write bytes to socket %d: %s\n",
		      __FUNCTION__, fd, strerror (errno));
	    return (FALSE);
	}

	data += written;
	remaining -= written;
    } while (remaining > 0);

#ifdef CHANNEL_VERBOSE_DEBUGGING
    GAM_DEBUG(DEBUG_INFO, "Wrote %d bytes to socket %d\n", len, fd);
#endif
    return (TRUE);
}
