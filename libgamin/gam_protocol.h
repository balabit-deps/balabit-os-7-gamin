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

#ifndef __GAM_PROTOCOL_H__
#define __GAM_PROTOCOL_H__ 1

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MAXPATHLEN:
 *
 * Needed to check length of paths
 */
#ifndef MAXPATHLEN
#define MAXPATHLEN FILENAME_MAX
#endif

/**
 * GAM_PROTO_VERSION:
 *
 * versionning at the protocol level
 */
#define GAM_PROTO_VERSION 1

/**
 * GAMReqType:
 *
 * Type of FAM requests
 */
typedef enum {
    GAM_REQ_FILE = 1,	/* monitoring a file */
    GAM_REQ_DIR = 2,	/* monitoring a directory */
    GAM_REQ_CANCEL = 3,	/* cancelling a monitor */
    GAM_REQ_DEBUG = 4 	/* debugging request */
} GAMReqType;

/**
 * GAMReqOpts:
 *
 * Option for FAM requests
 */
typedef enum {
    GAM_OPT_NOEXISTS=16	/* don't send Exists on directory monitoting */
} GAMReqOpts;

/**
 * GAMPacket:
 *
 * Structure associated to a FAM survey request
 * propagates from client to server
 */
typedef struct GAMPacket GAMPacket;
typedef GAMPacket *GAMPacketPtr;

struct GAMPacket {
    /* header */
    unsigned short len;		/* the total lenght of the request */
    unsigned short version;	/* version of the protocol */
    unsigned short seq;		/* the sequence number */
    unsigned short type;	/* type of request GAMReqType | GAMReqOpts */
    unsigned short pathlen;	/* the length of the path or filename */
    /* payload */
    char path[MAXPATHLEN];	/* the path to the file */
};

/**
 * GAM_PACKET_HEADER_LEN:
 *
 * convenience macro to provide the length of the packet header.
 */
#define GAM_PACKET_HEADER_LEN (5 * (sizeof(unsigned short)))

#ifdef __cplusplus
}
#endif

#endif /* __GAMIN_FAM_H__ */

