/*
 * $Id$
 *
 * Copyright (C) 2013 Robert Boisvert
 *
 * This file is part of the msgqueue module for sip-router, a free SIP server.
 *
 * The msgqueue module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The msgqueue module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef MSGQ_DB_H
#define MSGQ_DB_H

/**********
* DB definitions
**********/

/* table versions */
#define MSGQ_CTABLE_VERSION  1
#define MSGQ_QTABLE_VERSION  1

/* msgqueues columns */
#define MSGQ_COLCNT   4
#define MSGQCOL_ID    0
#define MSGQCOL_URI   1
#define MSGQCOL_MDIR  2
#define MSGQCOL_MFILE 3

/**********
* DB function declarations
**********/

db1_con_t *msgq_dbconnect (void);
void msgq_dbdisconnect (db1_con_t *);
void update_msgq_lst (db1_con_t *pconn);

#if 0  /* ??? */
#define SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS    12

enum {
  SCA_DB_SUBS_SUBSCRIBER_COL = 0,
  SCA_DB_SUBS_AOR_COL = 1,
  SCA_DB_SUBS_EVENT_COL,
  SCA_DB_SUBS_EXPIRES_COL,
  SCA_DB_SUBS_STATE_COL,
  SCA_DB_SUBS_APP_IDX_COL,
  SCA_DB_SUBS_CALL_ID_COL,
  SCA_DB_SUBS_FROM_TAG_COL,
  SCA_DB_SUBS_TO_TAG_COL,
  SCA_DB_SUBS_RECORD_ROUTE_COL,
  SCA_DB_SUBS_NOTIFY_CSEQ_COL,
  SCA_DB_SUBS_SUBSCRIBE_CSEQ_COL,
  SCA_DB_SUBS_BOUNDARY,
  };

enum {
  SCA_DB_FLAG_NONE = 0,
  SCA_DB_FLAG_INSERT = 1,
  SCA_DB_FLAG_UPDATE,
  SCA_DB_FLAG_DELETE,
  };

#define SCA_DB_BIND_STR_VALUE (cv, ct, k, v, c) \
	((db_key_t *)(k))[ (c) ] = (str *)(ct); \
	((db_val_t *)(v))[ (c) ].type = DB1_STR; \
	((db_val_t *)(v))[ (c) ].nul = 0; \
	((db_val_t *)(v))[ (c) ].val.str_val = (str)(cv); \
	(c)++;

#define SCA_DB_BIND_INT_VALUE (cv, ct, k, v, c) \
	((db_key_t *)(k))[ (c) ] = (str *)(ct); \
	((db_val_t *)(v))[ (c) ].type = DB1_INT; \
	((db_val_t *)(v))[ (c) ].nul = 0; \
	((db_val_t *)(v))[ (c) ].val.int_val = (int)(cv); \
	(c)++;

/**********
* global constants declarations
**********/

extern const str SCA_DB_SUBSCRIBER_COL_NAME;
extern const str SCA_DB_AOR_COL_NAME;
extern const str SCA_DB_EVENT_COL_NAME;
extern const str SCA_DB_EXPIRES_COL_NAME;
extern const str SCA_DB_STATE_COL_NAME;
extern const str SCA_DB_APP_IDX_COL_NAME;
extern const str SCA_DB_CALL_ID_COL_NAME;
extern const str SCA_DB_FROM_TAG_COL_NAME;
extern const str SCA_DB_TO_TAG_COL_NAME;
extern const str SCA_DB_RECORD_ROUTE_COL_NAME;
extern const str SCA_DB_NOTIFY_CSEQ_COL_NAME;
extern const str SCA_DB_SUBSCRIBE_CSEQ_COL_NAME;

/**********
* global function declarations
**********/

str **sca_db_subscriptions_columns (void);
void sca_db_subscriptions_get_value_for_column (int, db_val_t *, void *);
void sca_db_subscriptions_set_value_for_column (int, db_val_t *, void *);
void sca_db_subscriptions_bind_value_for_column (int, db_val_t *, void *);
db1_con_t *sca_db_get_connection (void);
void msgq_db_disconnect (void);
#endif  /* ??? */

#endif /* MSGQ_DB_H */