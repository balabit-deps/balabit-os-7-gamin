/* John McCutchan <ttb@tentacle.dhs.org> 2005 */

#include "server_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include "gam_error.h"
#include "gam_conf.h"
#include "gam_fs.h"
#include "gam_excludes.h"

static gam_fs_mon_type
gam_conf_string_to_mon_type (const char *method)
{
	if (!strcasecmp(method, "kernel"))
		return GFS_MT_KERNEL;
	if (!strcasecmp(method, "poll"))
		return GFS_MT_POLL;

	return GFS_MT_NONE;
}

static void
gam_conf_read_internal (const char *filename)
{
	gchar *path;
	gchar *contents, **lines, *line, **words;
	gsize len;
	int x, y;
	int exclude = 1;

	g_file_get_contents(filename, &contents, &len, NULL);
	if (contents == NULL)
		return;
	lines = g_strsplit(contents, "\n", 0);
	if (lines != NULL) {
		for (x = 0; lines[x] != NULL ; x++) {
			line = lines[x];
			if ((line[0] == 0) || (line[0] == '#'))
				continue;
			words = g_strsplit(line, " ", 0);
			if (words == NULL)
				continue;

			if (!strcmp(words[0], "fsset")) {
				gam_fs_mon_type mon_type = GFS_MT_KERNEL;
				gint poll_timeout = 0;
				/* We need: fsset <fsname> <method> [poll timeout] */
				/* fsname */
				if (!words[1] || !words[1][0]) {
					g_strfreev(words);
					continue;
				}
				/* method name */
				if (!words[2] || !words[2][0]) {
					g_strfreev(words);
					continue;
				}
				mon_type = gam_conf_string_to_mon_type (words[2]);
				/* The poll timeout value is optional, if it isn't provided, the default value will be used */
				if (!words[3] || !words[3][0]) 
					poll_timeout = -1;
				else
					poll_timeout = atoi (words[3]);
				gam_fs_set (words[1], mon_type, poll_timeout);
				g_strfreev(words);
				continue;
			} 
			if (!strcmp(words[0], "poll")) {
				exclude = 1;
			} else if (!strcmp(words[0], "notify")) {
				exclude = 0;
			} else {
				g_strfreev(words);
				continue;
			}

			for (y = 1; words[y] != NULL ; y++) {
				if (words[y][0] == 0)
						continue;
				if (words[y][0] == '#')
					break;
				if (words[y][0] == '~') {
					path = g_strconcat(g_get_home_dir(), &(words[y][1]), NULL);
					if (path != NULL) {
						gam_exclude_add (path, exclude);
						g_free(path);
					}
					continue;
				}
				if (words[y][0] != '/')
					continue;
				gam_exclude_add (words[y], exclude);
			}
			g_strfreev(words);
		}
		g_strfreev(lines);
	}
	g_free(contents);
}

void
gam_conf_read (void)
{
	const char *globalconf = "/etc/gamin/gaminrc";
	const char *mandatory = "/etc/gamin/mandatory_gaminrc";
	gchar *userconf = NULL;
	userconf = g_strconcat(g_get_home_dir(), "/.gaminrc", NULL);
	if (userconf == NULL) {
		gam_conf_read_internal (globalconf);
		return;
	}

	/* We read three config files in this order,
	 * 1) System
	 * 2) User config
	 * 3) System mandatory 
	 *
	 * We read the system mandatory last, so that the system administrator 
	 * can override potentially dangerous options
	 */
	gam_conf_read_internal (globalconf);
	gam_conf_read_internal (userconf);
	gam_conf_read_internal (mandatory);

	g_free (userconf);
}
