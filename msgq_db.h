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
#define MSGQ_COLCNT   5
#define MSGQCOL_ID    0
#define MSGQCOL_URI   1
#define MSGQCOL_MDIR  2
#define MSGQCOL_MFILE 3
#define MSGQCOL_NAME  4

/* msgqcalls columns */
#define CALL_COLCNT   7
#define CALLCOL_MSGQ  0
#define CALLCOL_FROM  1
#define CALLCOL_CALL  2
#define CALLCOL_VIA   3
#define CALLCOL_TAG   4
#define CALLCOL_STATE 5
#define CALLCOL_TIME  6

/**********
* DB function declarations
**********/

void add_call_rec (db1_con_t *, int);
void delete_call_rec (db1_con_t *, call_lst *);
db1_con_t *msgq_dbconnect (void);
void msgq_dbdisconnect (db1_con_t *);
void update_call_rec (db1_con_t *, call_lst *);
void update_msgq_lst (db1_con_t *pconn);
void wait_db_flush (db1_con_t *, call_lst *);

#endif /* MSGQ_DB_H */