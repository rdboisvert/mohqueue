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

#include "msgq_common.h"
#include "msgq.h"
#include "msgq_db.h"

/**********
* msgqueue definitions
**********/

const str MSGQCSTR_ID = STR_STATIC_INIT ("id");
const str MSGQCSTR_URI = STR_STATIC_INIT ("msgq_uri");
const str MSGQCSTR_MDIR = STR_STATIC_INIT ("msgq_mohdir");
const str MSGQCSTR_MFILE = STR_STATIC_INIT ("msgq_mohfile");

static str *msgq_columns [] =
  {
  &MSGQCSTR_ID,
  &MSGQCSTR_URI,
  &MSGQCSTR_MDIR,
  &MSGQCSTR_MFILE,
  NULL
  };

/**********
* functions
**********/

/**********
* Connect to DB
*
* INPUT: none
* OUTPUT: DB connection pointer; NULL=failed
**********/

db1_con_t *msgq_dbconnect (void)

{
str *pdb_url = &pmod_data->pcfg->db_url;
db1_con_t *pconn = pmod_data->pdb->init (pdb_url);
if (!pconn)
  { LM_ERR ("Unable to connect to DB %s", pdb_url->s); }
return (pconn);
}

/**********
* Disconnect from DB
*
* INPUT:
*   Arg (1) = connection pointer
* OUTPUT: none
**********/

void msgq_dbdisconnect (db1_con_t *pconn)

{
pmod_data->pdb->close (pconn);
return;
}

/**********
* Update Message Queue List
*
* INPUT:
*   Arg (1) = DB pointer
*   Arg (2) = connection pointer
* OUTPUT: none
**********/

void update_msgq_lst (db_func_t *pdb, db1_con_t *pconn)

{
/**********
* o reset checked flag on all queues
* o read queues from table
**********/

msgq_lst *pqlst = pmod_data->pmsgq_lst;
int nidx;
for (nidx = 0; nidx < pmod_data->msgq_cnt; nidx++)
  { pqlst [nidx].msgq_flag &= ~MSGQF_CHK; }
pdb->use_table (pconn, &pmod_data->pcfg->db_qtable);
db_key_t prcols [MSGQ_COLCNT];
for (nidx = 0; nidx < MSGQ_COLCNT; nidx++)
  { prcols [nidx] = msgq_columns [nidx]; }
db1_res_t *presult = NULL;
if (pdb->query (pconn, NULL, NULL, NULL, prcols, 0, MSGQ_COLCNT, 0, &presult))
  {
  LM_ERR ("update_msgq_lst: table query (%s) failed",
    pmod_data->pcfg->db_qtable.s);
  return;
  }
db_row_t *prows = RES_ROWS (presult);
int nrows = RES_ROW_N (presult);
db_val_t *prowvals = NULL;
for (nidx = 0; nidx < nrows; nidx++)
  {
  /**********
  * find matching queues
  **********/

  prowvals = ROW_VALUES (prows + nidx);
  char *puri = VAL_STRING (prowvals + MSGQCOL_URI);
  int bfnd = 0;
  int nidx2;
  for (nidx2 = 0; nidx2 < pmod_data->msgq_cnt; nidx2++)
    {
    if (!strcasecmp (pqlst [nidx2].msgq_uri, puri))
      {
      /**********
      * o data the same?
      * o mark as found
      **********/

      char *ptext = VAL_STRING (prowvals + MSGQCOL_MDIR);
      if (strcmp (pqlst [nidx2].msgq_mohdir, ptext))
        {
        strcpy (pqlst [nidx2].msgq_mohdir, ptext);
        LM_INFO ("Changed mohdir for queue (%s)", puri);
        }
      ptext = VAL_STRING (prowvals + MSGQCOL_MFILE);
      if (strcmp (pqlst [nidx2].msgq_mohfile, ptext))
        {
        strcpy (pqlst [nidx2].msgq_mohfile, ptext);
        LM_INFO ("Changed mohfile for queue (%s)", puri);
        }
      bfnd = -1;
      pqlst [nidx2].msgq_flag |= MSGQF_CHK;
      break;
      }
    }

  /**********
  * add new queue
  **********/

  if (!bfnd)
    {
    /**********
    * o allocate new list
    * o copy old list
    * o add new row
    * o release old list
    * o adjust pointers to new list
    **********/

    int nsize = pmod_data->msgq_cnt + 1;
    msgq_lst *pnewlst = (msgq_lst *) shm_malloc (sizeof (msgq_lst) * nsize);
    if (!pnewlst)
      {
      LM_ERR ("Unable to allocate shared memory");
      return;
      }
    pmod_data->msgq_cnt = nsize;
    if (--nsize)
      { memcpy (pnewlst, pqlst, sizeof (msgq_lst) * nsize); }
    pnewlst [nsize].msgq_id = prowvals [MSGQCOL_ID].val.int_val;
    pnewlst [nsize].msgq_flag = MSGQF_CHK;
    strcpy (pnewlst [nsize].msgq_uri, puri);
    strcpy (pnewlst [nsize].msgq_mohdir,
      VAL_STRING (prowvals + MSGQCOL_MDIR));
    strcpy (pnewlst [nsize].msgq_mohfile,
      VAL_STRING (prowvals + MSGQCOL_MFILE));
    LM_INFO ("Adding new queue (%s)", pnewlst [nsize].msgq_uri);
    if (nsize)
      { shm_free (pmod_data->pmsgq_lst); }
    pmod_data->pmsgq_lst = pnewlst;
    pqlst = pnewlst;
    }
  }

/**********
* find deleted queues
**********/

for (nidx = 0; nidx < pmod_data->msgq_cnt; nidx++)
  {
  /**********
  * o exists?
  * o has active calls?
  * o if not last, replace current with last queue
  **********/

  if (pqlst [nidx].msgq_flag &= MSGQF_CHK)
    { continue; }
  if (0) /* ??? need to check */
    {
    LM_WARN ("Unable to remove queue (%s) because has active calls!",
      pqlst [nidx].msgq_uri);
    continue;
    }
  LM_INFO ("Removed queue (%s)", pqlst [nidx].msgq_uri);
  if (nidx != (pmod_data->msgq_cnt - 1))
    {
    memcpy (&pqlst [nidx], &pqlst [pmod_data->msgq_cnt - 1],
      sizeof (msgq_lst));
    }
  --pmod_data->msgq_cnt;
  --nidx;
  }
return;
}

#if 0  /* ??? */
db1_con_t *sca_db_con = NULL;

const str SCA_DB_SUBSCRIBER_COL_NAME = STR_STATIC_INIT ("subscriber");
const str SCA_DB_AOR_COL_NAME  = STR_STATIC_INIT ("aor");
const str SCA_DB_EVENT_COL_NAME = STR_STATIC_INIT ("event");
const str SCA_DB_EXPIRES_COL_NAME = STR_STATIC_INIT ("expires");
const str SCA_DB_STATE_COL_NAME = STR_STATIC_INIT ("state");
const str SCA_DB_APP_IDX_COL_NAME = STR_STATIC_INIT ("app_idx");
const str SCA_DB_CALL_ID_COL_NAME = STR_STATIC_INIT ("call_id");
const str SCA_DB_FROM_TAG_COL_NAME = STR_STATIC_INIT ("from_tag");
const str SCA_DB_TO_TAG_COL_NAME = STR_STATIC_INIT ("to_tag");
const str SCA_DB_RECORD_ROUTE_COL_NAME = STR_STATIC_INIT ("record_route");
const str SCA_DB_NOTIFY_CSEQ_COL_NAME = STR_STATIC_INIT ("notify_cseq");
const str SCA_DB_SUBSCRIBE_CSEQ_COL_NAME = STR_STATIC_INIT ("subscribe_cseq");

void sca_db_subscriptions_get_value_for_column (int column, db_val_t *row_values,
  void *column_value)
{
assert (column_value != NULL);
assert (row_values != NULL);
assert (column >= 0 && column < SCA_DB_SUBS_BOUNDARY);

switch (column) {
    case SCA_DB_SUBS_SUBSCRIBER_COL:
    case SCA_DB_SUBS_AOR_COL:
    case SCA_DB_SUBS_CALL_ID_COL:
    case SCA_DB_SUBS_FROM_TAG_COL:
    case SCA_DB_SUBS_TO_TAG_COL:
    case SCA_DB_SUBS_RECORD_ROUTE_COL:
	((str *)column_value)->s = (char *)row_values[ column ].val.string_val;
	((str *)column_value)->len = strlen(((str *)column_value)->s);
	break;

    case SCA_DB_SUBS_EXPIRES_COL:
	*((time_t *)column_value) = row_values[ column ].val.time_val;
	break;

    case SCA_DB_SUBS_EVENT_COL:
    case SCA_DB_SUBS_STATE_COL:
    case SCA_DB_SUBS_NOTIFY_CSEQ_COL:
    case SCA_DB_SUBS_SUBSCRIBE_CSEQ_COL:
	*((int *)column_value) = row_values[ column ].val.int_val;
	break;

    default:
	column_value = NULL;
    }
}

void sca_db_subscriptions_set_value_for_column (int column, db_val_t *row_values,
  void *column_value)
{
assert (column >= 0 && column < SCA_DB_SUBS_BOUNDARY);
assert (column_value != NULL);
assert (row_values != NULL);
switch (column) {
  case SCA_DB_SUBS_SUBSCRIBER_COL:
  case SCA_DB_SUBS_AOR_COL:
  case SCA_DB_SUBS_CALL_ID_COL:
  case SCA_DB_SUBS_FROM_TAG_COL:
  case SCA_DB_SUBS_TO_TAG_COL:
  case SCA_DB_SUBS_RECORD_ROUTE_COL:
    row_values[ column ].val.str_val = *((str *)column_value);
    row_values[ column ].type = DB1_STR;
    row_values[ column ].nul = 0;
    break;

    case SCA_DB_SUBS_EXPIRES_COL:
	row_values[ column ].val.int_val = (int)(*((time_t *)column_value));
	row_values[ column ].type = DB1_INT;
	row_values[ column ].nul = 0;
	break;

    case SCA_DB_SUBS_APP_IDX_COL:
	/* for now, don't save appearance index associated with subscriber */
	row_values[ column ].val.int_val = 0;
	row_values[ column ].type = DB1_INT;
	row_values[ column ].nul = 0;
	break;

    default:
	LM_WARN ("sca_db_subscriptions_set_value_for_column: unrecognized "
		 "column index %d, treating as INT", column);
	/* fall through */

    case SCA_DB_SUBS_EVENT_COL:
    case SCA_DB_SUBS_STATE_COL:
    case SCA_DB_SUBS_NOTIFY_CSEQ_COL:
    case SCA_DB_SUBS_SUBSCRIBE_CSEQ_COL:
	row_values[ column ].val.int_val = *((int *)column_value);
	row_values[ column ].type = DB1_INT;
	row_values[ column ].nul = 0;
	break;
    }
}

str ** sca_db_subscriptions_columns (void)

{
static str *subs_columns [] = {
  (str *)&SCA_DB_SUBSCRIBER_COL_NAME,
  (str *)&SCA_DB_AOR_COL_NAME,
  (str *)&SCA_DB_EVENT_COL_NAME,
  (str *)&SCA_DB_EXPIRES_COL_NAME,
  (str *)&SCA_DB_STATE_COL_NAME,
  (str *)&SCA_DB_APP_IDX_COL_NAME,
  (str *)&SCA_DB_CALL_ID_COL_NAME,
  (str *)&SCA_DB_FROM_TAG_COL_NAME,
  (str *)&SCA_DB_TO_TAG_COL_NAME,
  (str *)&SCA_DB_RECORD_ROUTE_COL_NAME,
  (str *)&SCA_DB_NOTIFY_CSEQ_COL_NAME,
  (str *)&SCA_DB_SUBSCRIBE_CSEQ_COL_NAME,
  NULL
  };
return (subs_columns);
}

db1_con_t *sca_db_get_connection (void)

{
assert (sca && sca->cfg->db_url);
assert (sca->db_api && sca->db_api->init);
if (!sca_db_con)
  {
  sca_db_con = pmod_data->db_api->init (sca->cfg->db_url);
/* catch connection error in caller */
  }
return (sca_db_con);
}

void msgq_db_disconnect (void)

{
if (sca_db_con)
  {
  pmod_data->db_api->close (sca_db_con);
  sca_db_con = NULL;
  }
}
#endif  /* ??? */