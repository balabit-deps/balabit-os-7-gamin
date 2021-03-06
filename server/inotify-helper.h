/* inotify-helper.h - GNOME VFS Monitor using inotify

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

   Author: John McCutchan <john@johnmccutchan.com>
*/


#ifndef __INOTIFY_HELPER_H
#define __INOTIFY_HELPER_H

#include "inotify-sub.h"
#include "inotify-kernel.h"

typedef void (*event_callback_t)(const char *fullpath, guint32 mask, void *subdata);
typedef void (*found_callback_t)(const char *fullpath, void *subdata);

gboolean	 ih_startup		(event_callback_t ecb,
					 found_callback_t fcb);
gboolean	 ih_running		(void);
gboolean	 ih_sub_add		(ih_sub_t *sub);
gboolean	 ih_sub_cancel		(ih_sub_t *sub);

/* Return FALSE from 'f' if the subscription should be cancelled */
void		 ih_sub_foreach		(void *callerdata, gboolean (*f)(ih_sub_t *sub, void *callerdata));

/* Return FALSE from 'f' if the subscription should be cancelled and free'd */
void		 ih_sub_foreach_free	(void *callerdata, gboolean (*f)(ih_sub_t *sub, void *callerdata));

#endif /* __INOTIFY_HELPER_H */
