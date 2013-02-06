/*
 * $Id$
 *
 * Copyright (C) 2013 Robert Boisvert
 *
 * This file is part of the mohqueue module for sip-router, a free SIP server.
 *
 * The mohqueue module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The mohqueue module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef MOHQ_DB_H
#define MOHQ_DB_H

/**********
* DB definitions
**********/

/* table versions */
#define MOHQ_CTABLE_VERSION  1
#define MOHQ_QTABLE_VERSION  1

/* mohqueues columns */
#define MOHQ_COLCNT   5
#define MOHQCOL_ID    0
#define MOHQCOL_URI   1
#define MOHQCOL_MDIR  2
#define MOHQCOL_MFILE 3
#define MOHQCOL_NAME  4

/* mohqcalls columns */
#define CALL_COLCNT   7
#define CALLCOL_MOHQ  0
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
db1_con_t *mohq_dbconnect (void);
void mohq_dbdisconnect (db1_con_t *);
void update_call_rec (db1_con_t *, call_lst *);
void update_mohq_lst (db1_con_t *pconn);
void wait_db_flush (db1_con_t *, call_lst *);

#endif /* MOHQ_DB_H */