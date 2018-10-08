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

#ifndef __GAM_DATA_H__
#define __GAM_DATA_H__ 1

#include "fam.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GAMReqData:
 *
 * Internal structure associated to a request and exported only to allow
 * restarts.
 */
typedef struct GAMReqData GAMReqData;
typedef GAMReqData *GAMReqDataPtr;
struct GAMReqData {
    int reqno;                  /* the request number */
    int state;			/* the request state */
    int type;                   /* the type of events */
    char *filename;		/* the filename needed for restarts */
    void *userData;             /* the user data if any */
};

/**
 * Structure associated to a FAM connection
 */
typedef struct GAMData GAMData;
typedef GAMData *GAMDataPtr;

void		gamin_data_lock		(GAMDataPtr data);
void 		gamin_data_unlock	(GAMDataPtr data);

GAMDataPtr	gamin_data_new		(void);
void		gamin_data_free		(GAMDataPtr conn);
int		gamin_data_reset	(GAMDataPtr conn,
					 GAMReqDataPtr **requests);
int		gamin_data_get_data	(GAMDataPtr conn,
					 char **data,
					 int *size);
int		gamin_data_get_reqnum	(GAMDataPtr conn,
					 const char *filename,
					 int type,
					 void *userData);
int		gamin_data_get_request	(GAMDataPtr conn,
					 const char *filename,
					 int type,
					 void *userData,
					 int reqno);
int		gamin_data_del_req	(GAMDataPtr conn,
					 int reqno);
int		gamin_data_cancel	(GAMDataPtr conn,
					 int reqno);
int		gamin_data_conn_data	(GAMDataPtr conn,
					 int len);
int		gamin_data_need_auth	(GAMDataPtr conn);
int		gamin_data_done_auth	(GAMDataPtr conn);
int		gamin_data_read_event	(GAMDataPtr conn,
					 FAMEvent *event);
int		gamin_data_event_ready	(GAMDataPtr conn);
int		gamin_data_no_exists	(GAMDataPtr conn);
int		gamin_data_get_exists	(GAMDataPtr conn);

#ifdef __cplusplus
}
#endif

#endif /* __GAM_DATA_H__ */

