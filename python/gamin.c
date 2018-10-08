/*
 * gamin.c: glue to the gamin library for access from Python
 *
 * See Copyright for the status of this software.
 *
 * veillard@redhat.com
 */

#include <Python.h>
#include <fam.h>
#include "config.h"

#ifdef GAMIN_DEBUG_API
int FAMDebug(FAMConnection *fc, const char *filename, FAMRequest * fr,
             void *userData);
#endif

void init_gamin(void);

static FAMConnection **connections = NULL;
static int nb_connections = 0;
static int max_connections = 0;

static FAMRequest **requests = NULL;
static int nb_requests = 0;
static int max_requests = 0;

static int
get_connection(void) {
    int i;

    if (connections == NULL) {
        max_connections = 10;
        connections = (FAMConnection **) malloc(max_connections *
	                                        sizeof(FAMConnection *));
	if (connections == NULL) {
	    max_connections = 0;
	    return(-1);
	}
	memset(connections, 0, max_connections * sizeof(FAMConnection *));
    }
    for (i = 0;i < max_connections;i++)
        if (connections[i] == NULL)
	    break;
    if (i >= max_connections) {
        FAMConnection **tmp;

	tmp = (FAMConnection **) realloc(connections, max_connections * 2 *
	                                 sizeof(FAMConnection *));
	if (tmp == NULL)
	    return(-1);
	memset(&tmp[max_connections], 0,
	       max_connections * sizeof(FAMConnection *));
	max_connections *= 2;
	connections = tmp;
    }
    connections[i] = (FAMConnection *) malloc(sizeof(FAMConnection));
    if (connections[i] == NULL)
        return(-1);
    nb_connections++;
    return(i);
}

static int
release_connection(int no) {
    if ((no < 0) || (no >= max_connections))
        return(-1);
    if (connections[no] == NULL)
        return(-1);
    free(connections[no]);
    connections[no] = NULL;
    nb_connections--;
    return(0);
}

static FAMConnection *
check_connection(int no) {
    if ((no < 0) || (no >= max_connections))
        return(NULL);
    return(connections[no]);
}

static int
get_request(void) {
    int i;

    if (requests == NULL) {
        max_requests = 10;
        requests = (FAMRequest **) malloc(max_requests *
	                                        sizeof(FAMRequest *));
	if (requests == NULL) {
	    max_requests = 0;
	    return(-1);
	}
	memset(requests, 0, max_requests * sizeof(FAMRequest *));
    }
    for (i = 0;i < max_requests;i++)
        if (requests[i] == NULL)
	    break;
    if (i >= max_requests) {
        FAMRequest **tmp;

	tmp = (FAMRequest **) realloc(requests, max_requests * 2 *
	                                 sizeof(FAMRequest *));
	if (tmp == NULL)
	    return(-1);
	memset(&tmp[max_requests], 0,
	       max_requests * sizeof(FAMRequest *));
	max_requests *= 2;
	requests = tmp;
    }
    requests[i] = (FAMRequest *) malloc(sizeof(FAMRequest));
    if (requests[i] == NULL)
        return(-1);
    nb_requests++;
    return(i);
}

static int
release_request(int no) {
    if ((no < 0) || (no >= max_requests))
        return(-1);
    if (requests[no] == NULL)
        return(-1);
    free(requests[no]);
    requests[no] = NULL;
    nb_requests--;
    return(0);
}

static FAMRequest *
check_request(int no) {
    if ((no < 0) || (no >= max_requests))
        return(NULL);
    return(requests[no]);
}

static int fam_connect(void) {
    int ret;
    int no = get_connection();
    FAMConnection *conn;

    if (no < 0)
        return(-1);
    conn = connections[no];
    if (conn == NULL)
        return(-1);
    ret = FAMOpen2(conn, "gamin-python");
    if (ret < 0) {
        release_connection(no);
        return(ret);
    }
    return(no);
}

static int
call_internal_callback(PyObject *self, const char *filename, FAMCodes event) {
    PyObject *ret;

    if ((self == NULL) || (filename == NULL))
        return(-1);
/*    fprintf(stderr, "call_internal_callback(%p)\n", self); */
    ret = PyEval_CallMethod(self, (char *) "_internal_callback",
                            (char *) "(zi)", filename, (int) event);
    if (ret != NULL) {
        Py_DECREF(ret);
#if 0
    } else {
	fprintf(stderr, "call_internal_callback() failed\n");
#endif
    }
    return(0);
}

static PyObject *
gamin_Errno(PyObject *self, PyObject * args) {
    return(PyInt_FromLong(FAMErrno));
}

static PyObject *
gamin_MonitorConnect(PyObject *self, PyObject * args) {
    int ret;

    ret = fam_connect();
    if (ret < 0) {
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(ret));
}

static PyObject *
gamin_GetFd(PyObject *self, PyObject * args) {
    int no;
    FAMConnection *conn;

    if (!PyArg_ParseTuple(args, (char *)"i:GetFd", &no))
	return(NULL);

    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(FAMCONNECTION_GETFD(conn)));
}

static PyObject *
gamin_MonitorClose(PyObject *self, PyObject * args) {
    int no;
    int ret;

    if (!PyArg_ParseTuple(args, (char *)"i:MonitorClose", &no))
	return(NULL);

    ret = release_connection(no);
    return(PyInt_FromLong(ret));
}

static PyObject *
gamin_ProcessOneEvent(PyObject *self, PyObject * args) {
    int ret;
    FAMEvent fe;
    int no;
    FAMConnection *conn;

    if (!PyArg_ParseTuple(args, (char *)"i:ProcessOneEvent", &no))
	return(NULL);

    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }

    ret = FAMNextEvent(conn, &fe);
    if (ret < 0) {
	return(PyInt_FromLong(-1));
    }
    call_internal_callback(fe.userdata, &fe.filename[0], fe.code);

    return(PyInt_FromLong(ret));
}

static PyObject *
gamin_ProcessEvents(PyObject *self, PyObject * args) {
    int ret;
    int nb = 0;
    FAMEvent fe;
    int no;
    FAMConnection *conn;

    if (!PyArg_ParseTuple(args, (char *)"i:ProcessOneEvent", &no))
	return(NULL);

    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }

    do {
	ret = FAMPending(conn);
	if (ret < 0)
	    return(PyInt_FromLong(-1));
	if (ret == 0)
	    break;
	ret = FAMNextEvent(conn, &fe);
	if (ret < 0)
	    return(PyInt_FromLong(-1));
	call_internal_callback(fe.userdata, &fe.filename[0], fe.code);
	nb++;
    } while (ret >= 0);

    return(PyInt_FromLong(nb));
}

static PyObject *
gamin_EventPending(PyObject *self, PyObject * args) {
    int no;
    FAMConnection *conn;

    if (!PyArg_ParseTuple(args, (char *)"i:ProcessOneEvent", &no))
	return(NULL);

    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(FAMPending(conn)));
}

static PyObject *
gamin_MonitorNoExists(PyObject *self, PyObject * args) {
    int no;
    FAMConnection *conn;

    if (!PyArg_ParseTuple(args, (char *)"i:MonitorNoExists", &no))
	return(NULL);

    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(FAMNoExists(conn)));
}

static PyObject *
gamin_MonitorDirectory(PyObject *self, PyObject * args) {
    PyObject *userdata;
    char * filename;
    int ret;
    int noreq;
    int no;
    FAMConnection *conn;
    FAMRequest *request;

    if (!PyArg_ParseTuple(args, (char *)"izO:MonitorDirectory",
        &no, &filename, &userdata))
	return(NULL);
    
    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }
    noreq = get_request();
    if (noreq < 0) {
	return(PyInt_FromLong(-1));
    }
    request = check_request(noreq);

    ret = FAMMonitorDirectory(conn, filename, request, userdata);
    if (ret < 0) {
        release_request(noreq);
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(noreq));
}

static PyObject *
gamin_MonitorFile(PyObject *self, PyObject * args) {
    PyObject *userdata;
    char * filename;
    int ret;
    int noreq;
    int no;
    FAMConnection *conn;
    FAMRequest *request;

    if (!PyArg_ParseTuple(args, (char *)"izO:MonitorFile",
        &no, &filename, &userdata))
	return(NULL);
    
    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }
    noreq = get_request();
    if (noreq < 0) {
	return(PyInt_FromLong(-1));
    }
    request = check_request(noreq);

    ret = FAMMonitorFile(conn, filename, request, userdata);
    if (ret < 0) {
        release_request(noreq);
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(noreq));
}

static PyObject *
gamin_MonitorCancel(PyObject *self, PyObject * args) {
    int ret;
    int noreq;
    int no;
    FAMConnection *conn;
    FAMRequest *request;

    if (!PyArg_ParseTuple(args, (char *)"ii:MonitorCancel",
        &no, &noreq))
	return(NULL);
    
    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }
    request = check_request(noreq);
    if (request == NULL) {
	return(PyInt_FromLong(-1));
    }

    ret = FAMCancelMonitor(conn, request);
    if (ret < 0) {
        release_request(noreq);
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(ret));
}

#ifdef GAMIN_DEBUG_API
static PyObject *
gamin_MonitorDebug(PyObject *self, PyObject * args) {
    PyObject *userdata;
    char * filename;
    int ret;
    int noreq;
    int no;
    FAMConnection *conn;
    FAMRequest *request;

    if (!PyArg_ParseTuple(args, (char *)"izO:MonitorDebug",
        &no, &filename, &userdata))
	return(NULL);
    
    conn = check_connection(no);
    if (conn == NULL) {
	return(PyInt_FromLong(-1));
    }
    noreq = get_request();
    if (noreq < 0) {
	return(PyInt_FromLong(-1));
    }
    request = check_request(noreq);

    ret = FAMDebug(conn, filename, request, userdata);
    if (ret < 0) {
        release_request(noreq);
	return(PyInt_FromLong(-1));
    }
    return(PyInt_FromLong(noreq));
}
#endif

static PyMethodDef gaminMethods[] = {
    {(char *)"MonitorConnect", gamin_MonitorConnect, METH_VARARGS, NULL},
    {(char *)"MonitorDirectory", gamin_MonitorDirectory, METH_VARARGS, NULL},
    {(char *)"MonitorFile", gamin_MonitorFile, METH_VARARGS, NULL},
    {(char *)"MonitorCancel", gamin_MonitorCancel, METH_VARARGS, NULL},
    {(char *)"MonitorNoExists", gamin_MonitorNoExists, METH_VARARGS, NULL},
    {(char *)"EventPending", gamin_EventPending, METH_VARARGS, NULL},
    {(char *)"ProcessOneEvent", gamin_ProcessOneEvent, METH_VARARGS, NULL},
    {(char *)"ProcessEvents", gamin_ProcessEvents, METH_VARARGS, NULL},
    {(char *)"MonitorClose", gamin_MonitorClose, METH_VARARGS, NULL},
    {(char *)"GetFd", gamin_GetFd, METH_VARARGS, NULL},
    {(char *)"Errno", gamin_Errno, METH_VARARGS, NULL},
#ifdef GAMIN_DEBUG_API
    {(char *)"MonitorDebug", gamin_MonitorDebug, METH_VARARGS, NULL},
#endif
    {NULL, NULL, 0, NULL}
};

void
init_gamin(void)
{
    static int initialized = 0;

    if (initialized != 0)
        return;

    /* intialize the python extension module */
    Py_InitModule((char *) "_gamin", gaminMethods);

    initialized = 1;
}

