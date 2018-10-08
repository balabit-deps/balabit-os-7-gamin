/*
 * server_config.h: centralized header included by all server modules
 *
 * Author: Daniel Veillard <veillard@redhat.com>
 */

#ifndef __GAM_SERVER_CONFIG_H__
#define __GAM_SERVER_CONFIG_H__

#ifndef NO_LARGEFILE_SOURCE
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif

#include "config.h"

#endif
