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

#ifndef MOHQ_H
#define MOHQ_H

/**********
* definitions
**********/

#define URI_LEN 100
#define USLEEP_LEN  100

/**********
* structures
**********/

/* mohq_flag values */
#define MOHQF_ACT 0x01
#define MOHQF_CHK 0x02

typedef struct
  {
  char mohq_name [26];
  char mohq_uri [URI_LEN + 1];
  char mohq_mohdir [101];
  char mohq_mohfile [101];
  int mohq_flag;
  int mohq_id;
  } mohq_lst;

/* call_state values */
#define CLSTA_PRACKSTRT 100
#define CLSTA_PRACKRPLY 101
#define CLSTA_RINGING   102
#define CLSTA_INVITED   103
#define CLSTA_INQUEUE   200
#define CLSTA_REFER     301
#define CLSTA_RFRWAIT   302
#define CLSTA_HOLDSTRT  401
#define CLSTA_HOLDFAIL  402
#define CLSTA_HOLDOK    403
#define CLSTA_ERR       900

typedef struct
  {
  int call_dirty;
  int call_active;
  char call_id [101];
  char call_from [URI_LEN + 1];
  char call_contact [URI_LEN + 1];
  char call_tag [101];
  char call_via [1024];
  int call_state;
  int call_cseq;
  int call_aport;
  int mohq_id;
  unsigned int call_hash;
  unsigned int call_label;
  sip_msg_t *call_pmsg;
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
  int mohq_cnt;
  mohq_lst *pmohq_lst;
  int call_cnt;
  call_lst *pcall_lst;
  db_func_t pdb [1];
  tm_api_t ptm [1];
  sl_api_t psl [1];
  rr_api_t prr [1];
  cmd_function fn_rtp_answer;
  cmd_function fn_rtp_destroy;
  cmd_function fn_rtp_offer;
  cmd_function fn_rtp_stream2;
  } mod_data;

/**********
* global varb declarations
**********/

extern mod_data *pmod_data;

#endif /* MOHQ_H */