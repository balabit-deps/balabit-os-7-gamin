/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* inotify-helper.c - Gnome VFS Monitor based on inotify.

   Copyright (C) 2006 John McCutchan

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: 
		 John McCutchan <john@johnmccutchan.com>
*/

#include "config.h"
#include <string.h>
#include <glib.h>
#include "gam_subscription.h"
#include "inotify-sub.h"

static gboolean     is_debug_enabled = FALSE;
#define IS_W if (is_debug_enabled) g_warning

static void ih_sub_setup (ih_sub_t *sub);

ih_sub_t *
ih_sub_new (const char *pathname, gboolean is_dir, guint32 flags, void *userdata)
{
	ih_sub_t *sub = NULL;

	sub = g_new0 (ih_sub_t, 1);
	sub->usersubdata = userdata;
	sub->is_dir = is_dir;
	sub->extra_flags = flags;
	sub->pathname = g_strdup (pathname);

	IS_W("new subscription for %s being setup\n", sub->pathname);

	ih_sub_setup (sub);
	return sub;
}

void
ih_sub_free (ih_sub_t *sub)
{
	if (sub->filename)
		g_free (sub->filename);
	if (sub->dirname)
	    g_free (sub->dirname);
	g_free (sub->pathname);
	g_free (sub);
}

static
gchar *ih_sub_get_dirname (gchar *pathname)
{
	return g_path_get_dirname (pathname);
}

static
gchar *ih_sub_get_filename (gchar *pathname)
{
	return g_path_get_basename (pathname);
}

static 
void ih_sub_fix_dirname (ih_sub_t *sub)
{
	size_t len = 0;
	
	g_assert (sub->dirname);

	len = strlen (sub->dirname);

	/* We need to strip a trailing slash
	 * to get the correct behaviour
	 * out of the kernel
	 */
	if (sub->dirname[len] == '/')
		sub->dirname[len] = '\0';
}

/*
 * XXX: Currently we just follow the gnome vfs monitor type flags when
 * deciding how to treat the path. In the future we could try
 * and determine whether the path points to a directory or a file but
 * that is racey.
 */
static void
ih_sub_setup (ih_sub_t *sub)
{
	if (sub->is_dir)
	{
		sub->dirname = g_strdup (sub->pathname);
		sub->filename = NULL;
	} else {
		sub->dirname = ih_sub_get_dirname (sub->pathname);
		sub->filename = ih_sub_get_filename (sub->pathname);
	}

	ih_sub_fix_dirname (sub);

	IS_W("sub->dirname = %s\n", sub->dirname);
	if (sub->filename)
	{
		IS_W("sub->filename = %s\n", sub->filename);
	}
}
