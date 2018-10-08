/**
 * gam_error.c: the debugging intrastructure
 *
 * Allow to use GAM_DEBUG environment variable or SIG_USR2 for
 * dynamic debugging of clients and server.
 *
 * Daniel Veillard <veillard@redhat.com>
 * See the Copyright file.
 */
#include <config.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include "gam_error.h"

typedef void (*signal_handler) (int);

extern void gam_show_debug(void);
extern void gam_got_signal (void);

int gam_debug_active = 0;
static int initialized = 0;
static int do_debug = 0;
static int got_signal = 0;
static FILE *debug_out = NULL;

static void
gam_error_handle_signal(void)
{
    if (got_signal == 0)
        return;

    got_signal = 0;

    if (do_debug == 0) {
        if (debug_out != stderr) {
            char path[50] = "/tmp/gamin_debug_XXXXXX";
            int fd = mkstemp(path);

            if (fd >= 0) {
                debug_out = fdopen(fd, "a");
                if (debug_out != NULL) {
                    do_debug = 1;
                    gam_debug_active = 1;
                    gam_show_debug();
                }
            }
        }
    } else {
        if (debug_out != stderr) {
            do_debug = 0;
            gam_debug_active = 0;
            if (debug_out != NULL) {
                fflush(debug_out);
                fclose(debug_out);
                debug_out = NULL;
            }
        }
    }
}


static void
gam_error_signal(int no)
{
    got_signal = !got_signal;
    gam_debug_active = -1;      /* force going into gam_debug() */
    gam_got_signal ();
}

/**
 * gam_error_init:
 *
 * Initialization routine for the error and debug handling.
 */
void
gam_error_init(void)
{
    if (initialized == 0) {
        struct sigaction oldact;

        initialized = 1;

        if (getenv("GAM_DEBUG") != NULL) {
	    debug_out = stderr;
	    gam_debug_active = 1;
	    do_debug = 1;
            /* Fake the signal */
            got_signal = 1;
            gam_error_handle_signal();
        }

	/* if there is already an handler, leave it as is to
	 * avoid disturbing the application's behaviour */
	if (sigaction (SIGUSR2, NULL, &oldact) == 0) {
	    if (oldact.sa_handler == NULL && oldact.sa_sigaction == NULL)
	        signal(SIGUSR2, gam_error_signal);
	}
    }
}

/**
 * gam_error_init:
 *
 * Checking routine to call from time to time to handle asynchronous
 * error debugging events.
 */
void
gam_error_check(void)
{
    if (initialized == 0)
        gam_error_init();

    if (got_signal)
        gam_error_handle_signal();
}

int
gam_errno(void)
{
    return (errno);
}

/**
 * gam_error:
 * @file: the filename where the error was detected
 * @line: the line where the error was detected
 * @function: the function where the error was detected
 * @format: *printf format
 * @...:  extra arguments
 *
 * Log an error, currently only stderr, but could go into syslog
 */
void
gam_error(const char *file, int line, const char *function,
          const char *format, ...)
{
    va_list args;

    if (initialized == 0)
        gam_error_init();

    if (got_signal)
        gam_error_handle_signal();

    if ((file == NULL) || (function == NULL) || (format == NULL))
        return;

    va_start(args, format);
    vfprintf((debug_out ? debug_out : stderr), format, args);
    va_end(args);

    if (debug_out)
        fflush(debug_out);
}

/**
 * gam_debug:
 * @file: the filename where the error was detected
 * @line: the line where the error was detected
 * @function: the function where the error was detected
 * @format: *printf format
 * @...:  extra arguments
 *
 * Log a debug message, fi those are activated by the GAM_DEBUG environment
 */
void
gam_debug(const char *file, int line, const char *function,
          const char *format, ...)
{
    va_list args;

    if (initialized == 0)
        gam_error_init();

    if (got_signal)
        gam_error_handle_signal();

    if ((do_debug == 0) || (gam_debug_active == 0))
        return;

    if ((file == NULL) || (function == NULL) || (format == NULL))
        return;

    va_start(args, format);
    vfprintf((debug_out ? debug_out : stdout), format, args);
    va_end(args);
    if (debug_out)
        fflush(debug_out);
}
