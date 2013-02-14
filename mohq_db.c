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

#include "mohq_common.h"
#include "mohq.h"
#include "mohq_db.h"

/**********
* mohqueue definitions
**********/

str MOHQCSTR_ID = STR_STATIC_INIT ("id");
str MOHQCSTR_URI = STR_STATIC_INIT ("mohq_uri");
str MOHQCSTR_MDIR = STR_STATIC_INIT ("mohq_mohdir");
str MOHQCSTR_MFILE = STR_STATIC_INIT ("mohq_mohfile");
str MOHQCSTR_NAME = STR_STATIC_INIT ("mohq_name");

static str *mohq_columns [] =
  {
  &MOHQCSTR_ID,
  &MOHQCSTR_URI,
  &MOHQCSTR_MDIR,
  &MOHQCSTR_MFILE,
  &MOHQCSTR_NAME,
  NULL
  };

/**********
* mohqcalls definitions
**********/

str CALLCSTR_CALL = STR_STATIC_INIT ("call_id");
str CALLCSTR_CNTCT = STR_STATIC_INIT ("call_contact");
str CALLCSTR_FROM = STR_STATIC_INIT ("call_from");
str CALLCSTR_MOHQ = STR_STATIC_INIT ("mohq_id");
str CALLCSTR_STATE = STR_STATIC_INIT ("call_state");
str CALLCSTR_TAG = STR_STATIC_INIT ("call_tag");
str CALLCSTR_TIME = STR_STATIC_INIT ("mohq_time");
str CALLCSTR_VIA = STR_STATIC_INIT ("call_via");

static str *call_columns [] =
  {
  &CALLCSTR_MOHQ,
  &CALLCSTR_FROM,
  &CALLCSTR_CALL,
  &CALLCSTR_CNTCT,
  &CALLCSTR_VIA,
  &CALLCSTR_TAG,
  &CALLCSTR_STATE,
  &CALLCSTR_TIME,
  NULL
  };

/**********
* local function declarations
**********/

void set_call_key (db_key_t *, int, int);
void set_call_val (db_val_t *, int, int, void *);

/**********
* local functions
**********/

/**********
* Fill Call Keys
*
* INPUT:
*   Arg (1) = row pointer
* OUTPUT: none
**********/

void fill_call_keys (db_key_t *prkeys)

{
int nidx;
for (nidx = 0; nidx < CALL_COLCNT; nidx++)
  { set_call_key (prkeys, nidx, nidx); }
return;
}

/**********
* Fill Call Values
*
* INPUT:
*   Arg (1) = row pointer
*   Arg (2) = call struct pointer
* OUTPUT: none
**********/

void fill_call_vals (db_val_t *prvals, call_lst *pcall)

{
set_call_val (prvals, CALLCOL_MOHQ, CALLCOL_MOHQ, &pcall->mohq_id);
set_call_val (prvals, CALLCOL_FROM, CALLCOL_FROM, pcall->call_from);
set_call_val (prvals, CALLCOL_CALL, CALLCOL_CALL, pcall->call_id);
set_call_val (prvals, CALLCOL_CNTCT, CALLCOL_CNTCT, pcall->call_contact);
set_call_val (prvals, CALLCOL_VIA, CALLCOL_VIA, pcall->call_via);
set_call_val (prvals, CALLCOL_TAG, CALLCOL_TAG, pcall->call_tag);
set_call_val (prvals, CALLCOL_STATE, CALLCOL_STATE, &pcall->call_state);
set_call_val (prvals, CALLCOL_TIME, CALLCOL_TIME, 0);
return;
}

/**********
* Set Call Column Key
*
* INPUT:
*   Arg (1) = row pointer
*   Arg (2) = column number
*   Arg (3) = column id
* OUTPUT: none
**********/

void set_call_key (db_key_t *prkeys, int ncol, int ncolid)

{
prkeys [ncol] = call_columns [ncolid];
return;
}

/**********
* Set Call Column Value
*
* INPUT:
*   Arg (1) = row pointer
*   Arg (2) = column number
*   Arg (3) = column id
*   Arg (4) = value pointer
* OUTPUT: none
**********/

void set_call_val (db_val_t *prvals, int ncol, int ncolid, void *pdata)

{
/**********
* fill based on column
**********/

switch (ncolid)
  {
  case CALLCOL_MOHQ:
  case CALLCOL_STATE:
    prvals [ncol].val.int_val = *((int *)pdata);
    prvals [ncol].type = DB1_INT;
    prvals [ncol].nul = 0;
    break;
  case CALLCOL_CALL:
  case CALLCOL_CNTCT:
  case CALLCOL_FROM:
  case CALLCOL_TAG:
  case CALLCOL_VIA:
    prvals [ncol].val.string_val = (char *)pdata;
    prvals [ncol].type = DB1_STRING;
    prvals [ncol].nul = 0;
    break;
  case CALLCOL_TIME:
    time (&prvals [ncol].val.time_val);
    prvals [ncol].type = DB1_DATETIME;
    prvals [ncol].nul = 0;
    break;
  }
return;
}

/**********
* external functions
**********/

/**********
* Add Call Record
*
* INPUT:
*   Arg (1) = connection pointer
*   Arg (2) = call index
* OUTPUT: none
**********/

void add_call_rec (db1_con_t *pconn, int ncall_idx)

{
/**********
* o fill column names and values
* o insert new record
**********/

char *pfncname = "add_call_rec: ";
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_ctable);
db_key_t prkeys [CALL_COLCNT];
fill_call_keys (prkeys);
db_val_t prvals [CALL_COLCNT];
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
fill_call_vals (prvals, pcall);
if (pdb->insert (pconn, prkeys, prvals, CALL_COLCNT) < 0)
  {
  LM_WARN ("%sUnable to add new row to %s", pfncname,
    pmod_data->pcfg->db_qtable.s);
  }
pcall->call_dirty = 0;
return;
}

/**********
* Delete Call Record
*
* INPUT:
*   Arg (1) = connection pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void delete_call_rec (db1_con_t *pconn, call_lst *pcall)

{
/**********
* o setup to delete based on call ID
* o delete record
**********/

char *pfncname = "delete_call_rec: ";
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_ctable);
db_key_t prkeys [1];
set_call_key (prkeys, 0, CALLCOL_CALL);
db_val_t prvals [1];
set_call_val (prvals, 0, CALLCOL_CALL, pcall->call_id);
if (pdb->delete (pconn, prkeys, 0, prvals, 1) < 0)
  {
  LM_WARN ("%sUnable to delete row from %s", pfncname,
    pmod_data->pcfg->db_qtable.s);
  }
pcall->call_dirty = 0;
return;
}

/**********
* Connect to DB
*
* INPUT: none
* OUTPUT: DB connection pointer; NULL=failed
**********/

db1_con_t *mohq_dbconnect (void)

{
str *pdb_url = &pmod_data->pcfg->db_url;
db1_con_t *pconn = pmod_data->pdb->init (pdb_url);
if (!pconn)
  { LM_ERR ("Unable to connect to DB %s", pdb_url->s); }
return pconn;
}

/**********
* Disconnect from DB
*
* INPUT:
*   Arg (1) = connection pointer
* OUTPUT: none
**********/

void mohq_dbdisconnect (db1_con_t *pconn)

{
pmod_data->pdb->close (pconn);
return;
}

/**********
* Update Call Record
*
* INPUT:
*   Arg (1) = connection pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void update_call_rec (db1_con_t *pconn, call_lst *pcall)

{
/**********
* o setup to update based on call ID
* o update record
**********/

char *pfncname = "update_call_rec: ";
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_ctable);
db_key_t pqkeys [1];
set_call_key (pqkeys, 0, CALLCOL_CALL);
db_val_t pqvals [1];
set_call_val (pqvals, 0, CALLCOL_CALL, pcall->call_id);
db_key_t pukeys [CALL_COLCNT];
fill_call_keys (pukeys);
db_val_t puvals [CALL_COLCNT];
fill_call_vals (puvals, pcall);
if (pdb->update (pconn, pqkeys, 0, pqvals, pukeys, puvals, 1, CALL_COLCNT) < 0)
  {
  LM_WARN ("%sUnable to update row in %s", pfncname,
    pmod_data->pcfg->db_qtable.s);
  }
pcall->call_dirty = 0;
return;
}

/**********
* Update Message Queue List
*
* INPUT:
*   Arg (1) = connection pointer
* OUTPUT: none
**********/

void update_mohq_lst (db1_con_t *pconn)

{
/**********
* o reset checked flag on all queues
* o read queues from table
**********/

if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
mohq_lst *pqlst = pmod_data->pmohq_lst;
int nidx;
for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  { pqlst [nidx].mohq_flag &= ~MOHQF_CHK; }
pdb->use_table (pconn, &pmod_data->pcfg->db_qtable);
db_key_t prkeys [MOHQ_COLCNT];
for (nidx = 0; nidx < MOHQ_COLCNT; nidx++)
  { prkeys [nidx] = mohq_columns [nidx]; }
db1_res_t *presult = NULL;
if (pdb->query (pconn, 0, 0, 0, prkeys, 0, MOHQ_COLCNT, 0, &presult))
  {
  LM_ERR ("update_mohq_lst: table query (%s) failed",
    pmod_data->pcfg->db_qtable.s);
  return;
  }
db_row_t *prows = RES_ROWS (presult);
int nrows = RES_ROW_N (presult);
db_val_t *prowvals = NULL;
char *ptext, *puri;
mohq_lst *pnewlst;
for (nidx = 0; nidx < nrows; nidx++)
  {
  /**********
  * find matching queues
  **********/

  prowvals = ROW_VALUES (prows + nidx);
  puri = VAL_STRING (prowvals + MOHQCOL_URI);
  int bfnd = 0;
  int nidx2;
  for (nidx2 = 0; nidx2 < pmod_data->mohq_cnt; nidx2++)
    {
    if (!strcasecmp (pqlst [nidx2].mohq_uri, puri))
      {
      /**********
      * o data the same?
      * o mark as found
      **********/

      ptext = VAL_STRING (prowvals + MOHQCOL_MDIR);
      if (strcmp (pqlst [nidx2].mohq_mohdir, ptext))
        {
        strcpy (pqlst [nidx2].mohq_mohdir, ptext);
        LM_INFO ("Changed mohdir for queue (%s)", puri);
        }
      ptext = VAL_STRING (prowvals + MOHQCOL_MFILE);
      if (strcmp (pqlst [nidx2].mohq_mohfile, ptext))
        {
        strcpy (pqlst [nidx2].mohq_mohfile, ptext);
        LM_INFO ("Changed mohfile for queue (%s)", puri);
        }
      ptext = VAL_STRING (prowvals + MOHQCOL_NAME);
      if (strcmp (pqlst [nidx2].mohq_name, ptext))
        {
        strcpy (pqlst [nidx2].mohq_name, ptext);
        LM_INFO ("Changed name for queue (%s)", puri);
        }
      bfnd = -1;
      pqlst [nidx2].mohq_flag |= MOHQF_CHK;
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

    int nsize = pmod_data->mohq_cnt + 1;
    pnewlst = (mohq_lst *) shm_malloc (sizeof (mohq_lst) * nsize);
    if (!pnewlst)
      {
      LM_ERR ("Unable to allocate shared memory");
      return;
      }
    pmod_data->mohq_cnt = nsize;
    if (--nsize)
      { memcpy (pnewlst, pqlst, sizeof (mohq_lst) * nsize); }
    pnewlst [nsize].mohq_id = prowvals [MOHQCOL_ID].val.int_val;
    pnewlst [nsize].mohq_flag = MOHQF_CHK;
    strcpy (pnewlst [nsize].mohq_uri, puri);
    strcpy (pnewlst [nsize].mohq_mohdir,
      VAL_STRING (prowvals + MOHQCOL_MDIR));
    strcpy (pnewlst [nsize].mohq_mohfile,
      VAL_STRING (prowvals + MOHQCOL_MFILE));
    strcpy (pnewlst [nsize].mohq_name,
      VAL_STRING (prowvals + MOHQCOL_NAME));
    LM_INFO ("Adding new queue (%s)", pnewlst [nsize].mohq_uri);
    if (nsize)
      { shm_free (pmod_data->pmohq_lst); }
    pmod_data->pmohq_lst = pnewlst;
    pqlst = pnewlst;
    }
  }

/**********
* find deleted queues
**********/

for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  {
  /**********
  * o exists?
  * o has active calls?
  * o if not last, replace current with last queue
  **********/

  if (pqlst [nidx].mohq_flag & MOHQF_CHK)
    { continue; }
  if (0) /* ??? need to check */
    {
    LM_WARN ("Unable to remove queue (%s) because has active calls!",
      pqlst [nidx].mohq_uri);
    continue;
    }
  LM_INFO ("Removed queue (%s)", pqlst [nidx].mohq_uri);
  if (nidx != (pmod_data->mohq_cnt - 1))
    {
    memcpy (&pqlst [nidx], &pqlst [pmod_data->mohq_cnt - 1],
      sizeof (mohq_lst));
    }
  --pmod_data->mohq_cnt;
  --nidx;
  }
return;
}

/**********
* Wait Until DB Flushed
*
* INPUT:
*   Arg (1) = connection pointer
*   Arg (2) = call structure pointer
* OUTPUT: none
**********/

void wait_db_flush (db1_con_t *pconn, call_lst *pcall)

{
/**********
* o make sure data flushed to DB
* o set dirty flag
**********/

if (pconn)
  {
  while (pcall->call_dirty)
    { usleep (USLEEP_LEN); }
  }
pcall->call_dirty = 1;
return;
}