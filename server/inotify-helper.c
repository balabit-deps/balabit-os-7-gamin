/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* inotify-helper.c - Gnome VFS Monitor based on inotify.

   Copyright (C) 2005 John McCutchan

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
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include "inotify-helper.h"
#include "inotify-missing.h"
#include "inotify-path.h"
#include "inotify-diag.h"

static gboolean		ih_debug_enabled = FALSE;
#define IH_W if (ih_debug_enabled) g_warning 

static void ih_event_callback (ik_event_t *event, ih_sub_t *sub);
static void ih_found_callback (ih_sub_t *sub);

/* We share this lock with inotify-kernel.c and inotify-missing.c
 *
 * inotify-kernel.c takes the lock when it reads events from
 * the kernel and when it processes those events
 *
 * inotify-missing.c takes the lock when it is scanning the missing
 * list.
 * 
 * We take the lock in all public functions 
 */
G_LOCK_DEFINE (inotify_lock);
static GList *sub_list = NULL;
static gboolean initialized = FALSE;
static event_callback_t user_ecb = NULL;
static found_callback_t user_fcb = NULL;

/**
 * Initializes the inotify backend.  This must be called before
 * any other functions in this module.
 *
 * @returns TRUE if initialization succeeded, FALSE otherwise
 */
gboolean
ih_startup (event_callback_t ecb,
	    found_callback_t fcb)
{
	static gboolean result = FALSE;

	G_LOCK(inotify_lock);
	
	if (initialized == TRUE) {
		G_UNLOCK(inotify_lock);
		return result;
	}

	result = ip_startup (ih_event_callback);
	if (!result) {
		g_warning( "Could not initialize inotify\n");
		G_UNLOCK(inotify_lock);
		return FALSE;
	}
	initialized = TRUE;
	user_ecb = ecb;
	user_fcb = fcb;
	im_startup (ih_found_callback);
	id_startup ();

	IH_W ("started gnome-vfs inotify backend\n");

	G_UNLOCK(inotify_lock);
	return TRUE;
}

gboolean
ih_running (void)
{
	return initialized;
}

/**
 * Adds a subscription to be monitored.
 */
gboolean
ih_sub_add (ih_sub_t * sub)
{
	G_LOCK(inotify_lock);
	
	g_assert (g_list_find (sub_list, sub) == NULL);

	// make sure that sub isn't on sub_list first.
	if (!ip_start_watching (sub))
	{
		im_add (sub);
	}

	sub_list = g_list_prepend (sub_list, sub);

	G_UNLOCK(inotify_lock);
	return TRUE;
}

/**
 * Cancels a subscription which was being monitored.
 */
gboolean
ih_sub_cancel (ih_sub_t * sub)
{
	G_LOCK(inotify_lock);


	if (!sub->cancelled)
	{
		IH_W("cancelling %s\n", sub->pathname);
		g_assert (g_list_find (sub_list, sub) != NULL);
		sub->cancelled = TRUE;
		im_rm (sub);
		ip_stop_watching (sub);
		sub_list = g_list_remove (sub_list, sub);
	}

	G_UNLOCK(inotify_lock);
	return TRUE;
}

static void
ih_sub_foreach_worker (void *callerdata, gboolean (*f)(ih_sub_t *sub, void *callerdata), gboolean free)
{
	GList *l = NULL;
	GList *next = NULL;

	G_LOCK(inotify_lock);

	for (l = sub_list; l; l = next)
	{
		ih_sub_t *sub = l->data;
		next = l->next;
		
		if (f(sub, callerdata))
		{
			ih_sub_cancel (sub); /* Removes sub from sub_list */
			if (free)
				ih_sub_free (sub);
		}
	}

	G_UNLOCK(inotify_lock);
}

void
ih_sub_foreach (void *callerdata, gboolean (*f)(ih_sub_t *sub, void *callerdata))
{
	ih_sub_foreach_worker (callerdata, f, FALSE);
}

void
ih_sub_foreach_free (void *callerdata, gboolean (*f)(ih_sub_t *sub, void *callerdata))
{
	ih_sub_foreach_worker (callerdata, f, TRUE);
}

static void ih_event_callback (ik_event_t *event, ih_sub_t *sub)
{
	gchar *fullpath;
	if (event->name)
	{
		fullpath = g_strdup_printf ("%s/%s", sub->dirname, event->name);
	} else {
		fullpath = g_strdup_printf ("%s/", sub->dirname);
	}

	user_ecb (fullpath, event->mask, sub->usersubdata);
	g_free(fullpath);
}

static void ih_found_callback (ih_sub_t *sub)
{
	gchar *fullpath;

	if (sub->filename)
	{
		fullpath = g_strdup_printf ("%s/%s", sub->dirname, sub->filename);
		if (!g_file_test (fullpath, G_FILE_TEST_EXISTS)) {
			g_free (fullpath);
			return;
		}
	} else {
		fullpath = g_strdup_printf ("%s/", sub->dirname);
	}

	user_fcb (fullpath, sub->usersubdata);
	g_free(fullpath);
}
