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
#include "gam_fs.h"

#define DEFAULT_POLL_TIMEOUT 0

typedef struct _gam_fs_properties {
	char * 		fsname;
	gam_fs_mon_type mon_type;
	int		poll_timeout;
} gam_fs_properties;

typedef struct _gam_fs {
	char *path;
	char *fsname;
} gam_fs;

static gboolean initialized = FALSE;
static GList *filesystems = NULL;
static GList *fs_props = NULL;
static struct stat mtab_sbuf;

static void
gam_fs_free_filesystems (void)
{
	GList *iterator = NULL;
	gam_fs *fs = NULL;

	iterator = filesystems;

	while (iterator) 
	{
		fs = iterator->data;

		iterator = g_list_next (iterator);

		filesystems = g_list_remove (filesystems, fs);

		g_free (fs->path);
		g_free (fs->fsname);
		g_free (fs);
	}
}


static const gam_fs *
gam_fs_find_fs (const char *path)
{
	GList *iterator = NULL;
	gam_fs *fs = NULL;

	gam_fs_init ();

	iterator = filesystems;

	while (iterator)
	{
		fs = iterator->data;

		if (g_str_has_prefix (path, fs->path)) {
			return fs;
		}
		iterator = g_list_next (iterator);
	}

	return NULL;
}

static const gam_fs_properties *
gam_fs_find_fs_props (const char *path)
{
	const gam_fs *fs = NULL;
	gam_fs_properties *props = NULL;
	GList *iterator = NULL;

	fs = gam_fs_find_fs (path);
	if (!fs)
		return NULL;

	iterator = fs_props;

	while (iterator) 
	{
		props = iterator->data;

		if (!strcmp (props->fsname, fs->fsname)) {
			return props;
		}
		iterator = g_list_next (iterator);
	}

	return NULL;
}


static gint
gam_fs_filesystem_sort_cb (gconstpointer a, gconstpointer b)
{
	const gam_fs *fsa = a;
	const gam_fs *fsb = b;

	return strlen(fsb->path) - strlen (fsa->path);
}

static void
gam_fs_scan_mtab (void)
{
	gchar *contents, **lines, *line, **words;
	gsize len;
	GList *new_filesystems = NULL;
	gam_fs *fs = NULL;
	int i;

	g_file_get_contents ("/etc/mtab", &contents, &len, NULL);
	if (contents == NULL)
		return;

	lines = g_strsplit (contents, "\n", 0);
	if (lines != NULL)
	{
		for (i = 0; lines[i] != NULL; i++)
		{
			line = lines[i];

			if (line[0] == '\0')
				continue;

			words = g_strsplit (line, " ", 0);

			if (words == NULL)
				continue;

			if (words[0] == NULL || words[1] == NULL || words[2] == NULL) 
			{
				g_strfreev (words);
				continue;
			}
			if (words[1][0] == '\0' || words[2][0] == '\0')
			{
				g_strfreev (words);
				continue;
			}

			fs = g_new0 (gam_fs, 1);
			fs->path = g_strdup (words[1]);
			fs->fsname = g_strdup (words[2]);

			g_strfreev (words);

			new_filesystems = g_list_prepend (new_filesystems, fs);
		}
		g_strfreev (lines);
	}
	g_free (contents);

	/* Replace the old file systems list with the new one */
	gam_fs_free_filesystems ();
	filesystems = g_list_sort (new_filesystems, gam_fs_filesystem_sort_cb);
}

void
gam_fs_init (void)
{
	if (initialized == FALSE)
	{
		initialized = TRUE;
		gam_fs_set ("ext3", GFS_MT_DEFAULT, 0);
		gam_fs_set ("ext2", GFS_MT_DEFAULT, 0);
		gam_fs_set ("reiser4", GFS_MT_DEFAULT, 0);
		gam_fs_set ("reiserfs", GFS_MT_DEFAULT, 0);
		gam_fs_set ("novfs", GFS_MT_POLL, 30);
		gam_fs_set ("nfs", GFS_MT_POLL, 5);
		if (stat("/etc/mtab", &mtab_sbuf) != 0)
		{
			GAM_DEBUG(DEBUG_INFO, "Could not stat /etc/mtab\n");
		}
		gam_fs_scan_mtab ();
	} else {
		struct stat sbuf;

		if (stat("/etc/mtab", &sbuf) != 0)
		{
			GAM_DEBUG(DEBUG_INFO, "Could not stat /etc/mtab\n");
		}

		/* /etc/mtab has changed */
		if (sbuf.st_mtime != mtab_sbuf.st_mtime) {
			GAM_DEBUG(DEBUG_INFO, "Updating list of mounted filesystems\n");
			gam_fs_scan_mtab ();
		}

		mtab_sbuf = sbuf;
	}
}

gam_fs_mon_type
gam_fs_get_mon_type (const char *path)
{
	const gam_fs_properties *props = NULL;
	gam_fs_init ();

	props = gam_fs_find_fs_props (path);

	if (!props)
		return GFS_MT_DEFAULT;

	return props->mon_type;
}

int
gam_fs_get_poll_timeout (const char *path)
{
	const gam_fs_properties *props = NULL;
	gam_fs_init ();

	props = gam_fs_find_fs_props (path);

	if (!props)
		return DEFAULT_POLL_TIMEOUT;

	return props->poll_timeout;
}

void
gam_fs_set (const char *fsname, gam_fs_mon_type type, int poll_timeout)
{
	GList *iterator = NULL;
	gam_fs_properties *prop = NULL;

	gam_fs_init ();
	iterator = fs_props;

	while (iterator) 
	{
		prop = iterator->data;

		if (!strcmp (prop->fsname, fsname)) {
			prop->mon_type = type;
			if (poll_timeout >= 0)
				prop->poll_timeout = poll_timeout;
			return;
		}

		iterator = g_list_next (iterator);
	}

	prop = g_new0(gam_fs_properties, 1);

	prop->fsname = g_strdup (fsname);
	prop->mon_type = type;
	if (poll_timeout >= 0)
		prop->poll_timeout = poll_timeout;
	else
		prop->poll_timeout = DEFAULT_POLL_TIMEOUT;

	fs_props = g_list_prepend (fs_props, prop);
}

void
gam_fs_unset (const char *fsname)
{
	GList *iterator = NULL;
	gam_fs_properties *prop = NULL;

	gam_fs_init ();

	iterator = fs_props;

	while (iterator) 
	{
		prop = iterator->data;

		if (!strcmp (prop->fsname, fsname)) {
			fs_props = g_list_remove (fs_props, prop);
			g_free (prop->fsname);
			g_free (prop);
			return;
		}
		iterator = g_list_next (iterator);
	}
}

void
gam_fs_debug (void)
{
	GList *iterator = NULL;
	gam_fs_properties *prop = NULL;
	gam_fs *fs = NULL;

	gam_fs_init ();

	iterator = filesystems;

	GAM_DEBUG (DEBUG_INFO, "Dumping mounted file systems\n");
	while (iterator)
	{
		fs = iterator->data;
		GAM_DEBUG (DEBUG_INFO, "%s filesystem mounted at %s\n", fs->fsname, fs->path);
		iterator = g_list_next (iterator);
	}

	iterator = fs_props;
	GAM_DEBUG (DEBUG_INFO, "Dumping file system properties\n");
	while (iterator)
	{
		prop = iterator->data;
		GAM_DEBUG (DEBUG_INFO, "fstype %s monitor %s poll timeout %d\n", prop->fsname, (prop->mon_type == GFS_MT_KERNEL) ? "kernel" : (prop->mon_type == GFS_MT_POLL) ? "poll" : "none", prop->poll_timeout);
		iterator = g_list_next (iterator);
	}
}
