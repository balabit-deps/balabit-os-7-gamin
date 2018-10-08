/*
    Copyright (C) 2004 Red Hat, Inc.  All Rights Reserved.

    This program is free software; you can redistribute it and/or modify it
    under the terms of version 2.1 of the GNU Lesser General Public License
    as published by the Free Software Foundation.

    This program is distributed in the hope that it would be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  Further, any
    license provided herein, whether implied or otherwise, is limited to
    this program in accordance with the express provisions of the GNU Lesser
    General Public License.  Patent licenses, if any, provided herein do not
    apply to combinations of this program with other product or programs, or
    any other product whatsoever. This program is distributed without any
    warranty that the program is delivered free of the rightful claim of any
    third person by way of infringement or the like.  See the GNU Lesser
    General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 59
    Temple Place - Suite 330, Boston MA 02111-1307, USA.
*/

#ifndef __GAM_ERROR_H__
#define __GAM_ERROR_H__ 1

#include <config.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GNUC__
#define __FUNCTION__   ""
#endif

/**
 * DEBUG_INFO:
 *
 * convenience macro providing informations
 */
#define DEBUG_INFO __FILE__, __LINE__, __FUNCTION__

void	gam_error(const char *file, int line, const char* function,
                  const char* format, ...);

int gam_errno(void);

#ifdef GAM_DEBUG_ENABLED

#ifdef GAMIN_DEBUG_API
extern int debug_reqno;
extern void *debug_userData;
#endif

/**
 * gam_debug_active:
 *
 * global variable indicating if debugging is activated.
 */
extern int gam_debug_active;

/**
 * GAM_DEBUG:
 *
 * debugging macro when debug is activated
 */
void	gam_debug(const char *file, int line, const char* function,
                  const char* format, ...);
#define GAM_DEBUG if (gam_debug_active) gam_debug

void	gam_error_init(void);
void	gam_error_check(void);

#else
/*
 * no debug, redefine the macro empty content
 */
#ifdef HAVE_ISO_VARARGS
/**
 * GAM_DEBUG:
 *
 * debugging macro when debug is not activated
 */
#define GAM_DEBUG(...)
#elif defined (HAVE_GNUC_VARARGS)
#define GAM_DEBUG(format...)
#else
#error "This compiler does not support varargs macros and thus debug messages can't be disabled"
#endif /* !HAVE_ISO_VARARGS && !HAVE_GNUC_VARARGS */
#endif /* GAM_DEBUG_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* __GAM_ERROR_H__ */

