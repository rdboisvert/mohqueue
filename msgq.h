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

#ifndef MSGQ_H
#define MSGQ_H

/**********
* definitions
**********/

#define URI_LEN 100

/* msgq_flag values */

#define MSGQF_ACT 0x01
#define MSGQF_CHK 0x02

/**********
* structures
**********/

typedef struct
  {
  char msgq_uri [URI_LEN + 1];
  char msgq_mohdir [101];
  char msgq_mohfile [101];
  int msgq_flag;
  int msgq_id;
  } msgq_lst;

typedef struct
  {
  str mohdir;
  str db_url;
  str db_ctable;
  str db_qtable;
  } mod_cfg;

typedef struct
  {
  mod_cfg pcfg [1];
  db_func_t pdb [1];
  struct tm_binds ptm [1];
  int msgq_cnt;
  msgq_lst *pmsgq_lst;
  cmd_function fn_rtp_answer;
  cmd_function fn_rtp_offer;
  cmd_function fn_rtp_stream2uac;
  } mod_data;

/**********
* global varb declarations
**********/

extern mod_data *pmod_data;

#endif /* MSGQ_H */