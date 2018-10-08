/**
 * gam_data.c: implementation of the automatic launch of the server side
 *             if apparently missing
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "gam_fork.h"
#include "gam_error.h"

/**
 * gamin_find_server_path:
 *
 * Tries to find the path to the gam_server binary.
 * 
 * Returns path on success or NULL in case of error.
 */
static const char *
gamin_find_server_path(void)
{
    static const char *server_paths[] = {
        BINDIR "/gam_server",
        NULL
    };
    int i;
    const char *gamin_debug_server = getenv("GAMIN_DEBUG_SERVER");

    if (gamin_debug_server) {
        return gamin_debug_server;
    }

    for (i = 0; server_paths[i]; i++) {
        if (access(server_paths[i], X_OK | R_OK) == 0) {
            return server_paths[i];
        }
    }
    return NULL;
}

/**
 * gamin_fork_server:
 * @fam_client_id: the client ID string to use
 *
 * Forks and try to launch the server processing the requests for
 * libgamin under the current process id and using the given client ID
 *
 * Returns 0 in case of success or -1 in case of detected error.
 */
int
gamin_fork_server(const char *fam_client_id)
{
    const char *server_path = gamin_find_server_path();
    int ret, pid, status;

    if (!server_path) {
        gam_error(DEBUG_INFO, "failed to find gam_server\n");
    }


    GAM_DEBUG(DEBUG_INFO, "Asking to launch %s with client id %s\n",
              server_path, fam_client_id);
    /* Become a daemon */
    pid = fork();
    if (pid == 0) {
	int fd;
        long open_max;
	long i;

        /* don't hold open fd opened from the client of the library */
	open_max = sysconf (_SC_OPEN_MAX);
	for (i = 0; i < open_max; i++)
	    fcntl (i, F_SETFD, FD_CLOEXEC);

	/* /dev/null for stdin, stdout, stderr */
	fd = open ("/dev/null", O_RDONLY);
	if (fd != -1) {
	    dup2 (fd, 0);
	    close (fd);
	}
	
	fd = open ("/dev/null", O_WRONLY);
	if (fd != -1) {
	    dup2 (fd, 1);
	    dup2 (fd, 2);
	    close (fd);
	}
	
        setsid();
        if (fork() == 0) {
#ifdef HAVE_SETENV
            setenv("GAM_CLIENT_ID", fam_client_id, 0);
#elif HAVE_PUTENV
            char *client_id = malloc (strlen (fam_client_id) + sizeof "GAM_CLIENT_ID=");
              if (client_id)
              {
                strcpy (client_id, "GAM_CLIENT_ID=");
                strcat (client_id, fam_client_id);
                putenv (client_id);
              }
#endif /* HAVE_SETENV */
            execl(server_path, server_path, NULL);
            gam_error(DEBUG_INFO, "failed to exec %s\n", server_path);
        }
        /*
         * calling exit() generate troubles for termination handlers
         * for example if the client uses bonobo/ORBit
         */
        _exit(0);
    }

    /*
     * do a waitpid on the intermediate process to avoid zombies.
     */
retry_wait:
    ret = waitpid(pid, &status, 0);
    if (ret < 0) {
        if (errno == EINTR)
            goto retry_wait;
    }

    return (0);
}
