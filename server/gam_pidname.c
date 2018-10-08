#include <config.h>
#include <stdio.h>
#include <glib.h>
#include "gam_error.h"

#include "gam_pidname.h"

char *gam_get_pidname (int pid)
{
    gchar *pidname = NULL;
#ifdef HAVE_LINUX
    gchar *procname;
    FILE *fp;
#endif

#ifdef HAVE_LINUX
    procname = g_strdup_printf ("/proc/%d/cmdline", pid);
    fp = fopen(procname, "r");
    g_free (procname);
    if (!fp) {
            pidname = g_strdup_printf ("%d", pid);
    } else {
            gchar *name = g_malloc (128);
            int i = 0;
            while (i < 128) {
                    int ch = fgetc (fp);

                    if (ch == EOF)
                            break;

                    name[i++] = ch;

                    if (ch == '\0')
                            break;
            }
            name[127] = '\0';
            pidname = g_strdup (name);
            g_free (name);
            fclose (fp);
    }
#else
    pidname = g_strdup_printf ("%d", pid);
#endif

    return pidname;
}
