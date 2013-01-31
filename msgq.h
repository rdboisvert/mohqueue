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

/**********
* structures
**********/

/* msgq_flag values */
#define MSGQF_ACT 0x01
#define MSGQF_CHK 0x02

typedef struct
  {
  char msgq_uri [URI_LEN + 1];
  char msgq_mohdir [101];
  char msgq_mohfile [101];
  int msgq_flag;
  int msgq_id;
  } msgq_lst;

/* call_state values */
#define CLSTA_PRING     0x01
#define CLSTA_PRINGACK  0x02
#define CLSTA_RING      0x03
#define CLSTA_INVITED   0x04
#define CLSTA_ACKED     0x05
#define CLSTA_INQUEUE   0x06
#define CLSTA_ENDCALL   0x07
#define CLSTA_ERR       0xFF

typedef struct
  {
  int call_dirty;
  int call_active;
  char call_id [101];
  char call_from [URI_LEN + 1];
  char call_tag [101];
  int call_state;
  int call_rseq;
  int msgq_id;
  } call_lst;

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
  int msgq_cnt;
  msgq_lst *pmsgq_lst;
  int call_cnt;
  call_lst *pcall_lst;
  db_func_t pdb [1];
  tm_api_t ptm [1];
  sl_api_t psl [1];
  rr_api_t prr [1];
  cmd_function fn_rtp_answer;
  cmd_function fn_rtp_destroy;
  cmd_function fn_rtp_offer;
  cmd_function fn_rtp_stream2uac;
  } mod_data;

/**********
* global varb declarations
**********/

extern mod_data *pmod_data;

#endif /* MSGQ_H */