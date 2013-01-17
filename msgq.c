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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "msgq_common.h"
#include "msgq.h"
#include "msgq_db.h"
#include "msgq_funcs.h"

MODULE_VERSION

/**********
* local function declarations
**********/

static int mod_child_init (int);
static void mod_destroy (void);
static int mod_init (void);

/**********
* global varbs
**********/

mod_data *pmod_data;

/**********
* module exports
**********/

/* EXPORTED COMMANDS */
static cmd_export_t mod_cmds [] = {
  { "msgq_count", (cmd_function) msgq_count, 1, msgq_count_fixup, 0,
    REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
  { "msgq_process", (cmd_function) msgq_process, 0, NULL, 0, REQUEST_ROUTE },
  { "msgq_redirect", (cmd_function) msgq_redirect, 2, msgq_redirect_fixup, 0,
    REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
  { NULL, NULL, -1, 0, 0 },
};

/* EXPORTED PARAMETERS */
static char *mohdir = "";
static char *db_url = DEFAULT_DB_URL;
static char *db_ctable = "msgqcalls";
static char *db_qtable = "msgqueues";

static param_export_t mod_parms [] = {
  { "mohdir", STR_PARAM, &mohdir },
  { "db_url", STR_PARAM, &db_url },
  { "db_ctable", STR_PARAM, &db_ctable },
  { "db_ctable", STR_PARAM, &db_qtable },
  { NULL, 0, NULL },
};

/* MODULE EXPORTS */
struct module_exports exports = {
  "msgqueue",       /* module name */
  DEFAULT_DLFLAGS,  /* dlopen flags */
  mod_cmds,         /* exported functions */
  mod_parms,        /* exported parameters */
  0,                /* statistics */
  0,                /* MI functions */
  0,                /* exported pseudo-variables */
  0,                /* extra processes */
  mod_init,         /* module initialization function */
  0,                /* response handling function */
  mod_destroy,      /* destructor function */
  mod_child_init,   /* per-child initialization function */
};

/**********
* local functions
**********/

/**********
* Configuration Initialization
*
* INPUT:
*   pmod_data memory allocated
*   configuration values set
* OUTPUT: 0 if failed; else pmod_data has config values
**********/

static int init_cfg (void)

{
/**********
* db_url, db_ctable, db_qtable exist?
**********/

if (!*db_url)
  {
  LM_ERR ("db_url parameter not set!");
  return (0);
  }
pmod_data->pcfg->db_url.s = db_url;
pmod_data->pcfg->db_url.len = strlen (db_url);
if (!*db_ctable)
  {
  LM_ERR ("db_ctable parameter not set!");
  return (0);
  }
pmod_data->pcfg->db_ctable.s = db_ctable;
pmod_data->pcfg->db_ctable.len = strlen(db_ctable);
if (!*db_qtable)
  {
  LM_ERR ("db_qtable parameter not set!");
  return (0);
  }
pmod_data->pcfg->db_qtable.s = db_qtable;
pmod_data->pcfg->db_qtable.len = strlen(db_qtable);

/**********
* mohdir
* o exists?
* o directory?
**********/

if (!*mohdir)
  {
  LM_ERR ("mohdir parameter not set!");
  return (0);
  }
pmod_data->pcfg->mohdir.s = mohdir;
pmod_data->pcfg->mohdir.len = strlen (mohdir);
int bfnd = 0;
struct stat psb [1];
if (!lstat (mohdir, psb))
  {
  if ((psb->st_mode & S_IFMT) == S_IFDIR)
    { bfnd = 1; }
  }
if (!bfnd)
  {
  LM_ERR ("mohdir is not a directory!");
  return (0);
  }
return (-1);
}

/**********
* DB Initialization
*
* INPUT:
*   pmod_data memory allocated and cfg values set
* OUTPUT: 0 if failed; else pmod_data has db_api
**********/

static int init_db (void)

{
/**********
* o bind to DB
* o check capabilities
* o init DB
**********/

str *pdb_url = &pmod_data->pcfg->db_url;
if (db_bind_mod (pdb_url, pmod_data->pdb))
  {
  LM_ERR ("Unable to bind DB API using %s", pdb_url->s);
  return (0);
  }
db_func_t *pdb = pmod_data->pdb;
if (!DB_CAPABILITY ((*pdb), DB_CAP_ALL))
  {
  LM_ERR ("Selected database %s lacks required capabilities", pdb_url->s);
  return (0);
  }
db1_con_t *pconn = msgq_dbconnect ();
if (!pconn)
  { return (0); }

/**********
* o check schema
* o load queue list
**********/

if (db_check_table_version (pdb, pconn,
  &pmod_data->pcfg->db_ctable, MSGQ_CTABLE_VERSION) < 0)
  {
  LM_ERR ("%s table in DB %s not at version %d",
    pmod_data->pcfg->db_ctable.s, pdb_url->s, MSGQ_CTABLE_VERSION);
  goto dberr;
  }
if (db_check_table_version (pdb, pconn,
  &pmod_data->pcfg->db_qtable, MSGQ_QTABLE_VERSION) < 0)
  {
  LM_ERR ("%s table in DB %s not at version %d",
    pmod_data->pcfg->db_qtable.s, pdb_url->s, MSGQ_QTABLE_VERSION);
  goto dberr;
  }
update_msgq_lst (pconn);
msgq_dbdisconnect (pconn);
return (-1);

/**********
* close DB
**********/

dberr:
pdb->close (pconn);
return (0);
}

/**********
* Child Module Initialization
*
* INPUT:
*   Arg (1) = child type
* OUTPUT: -1 if db_api not ready; else 0
**********/

int mod_child_init (int rank)

{
/**********
* make sure DB initialized
**********/

if (rank == PROC_INIT || rank == PROC_TCP_MAIN || rank == PROC_MAIN)
  { return (0); }
if (!pmod_data->pdb->init)
  {
  LM_CRIT ("DB API not loaded!");
  return (-1);
  }
return (0);
}

/**********
* Module Teardown
*
* INPUT: none
* OUTPUT: none
**********/

void mod_destroy (void)

{
LM_INFO ("???module destroy");
}

/**********
* Module Initialization
*
* INPUT: none
* OUTPUT: -1 if failed; 0 if success
**********/

int mod_init (void)

{
/**********
* o allocate shared mem and init
* o init configuration data
* o init DB
**********/

pmod_data = (mod_data *) shm_malloc (sizeof (mod_data));
if (!pmod_data)
  {
  LM_ERR ("Unable to allocate shared memory");
  return (-1);
  }
memset (pmod_data, 0, sizeof (mod_data));
if (!init_cfg ())
  { goto initerr; }
if (!init_db ())
  { goto initerr; }

/**********
* o bind to TM/RR modules
* o bind to RTPPROXY functions
**********/

if (load_tm_api (pmod_data->ptm))
  {
  LM_ERR ("Unable to load TM module");
  goto initerr;
  }
if (load_rr (pmod_data->prr))
  {
  LM_ERR ("Unable to load RR module");
  goto initerr;
  }
pmod_data->fn_rtp_answer = find_export ("rtpproxy_answer", 0, 0);
if (!pmod_data->fn_rtp_answer)
  {
  LM_ERR ("Unable to load rtpproxy_answer");
  goto initerr;
  }
pmod_data->fn_rtp_offer = find_export ("rtpproxy_offer", 0, 0);
if (!pmod_data->fn_rtp_offer)
  {
  LM_ERR ("Unable to load rtpproxy_offer");
  goto initerr;
  }
pmod_data->fn_rtp_stream2uac = find_export ("rtpproxy_stream2uac", 2, 0);
if (!pmod_data->fn_rtp_stream2uac)
  {
  LM_ERR ("Unable to load rtpproxy_stream2uac");
  goto initerr;
  }
LM_INFO ("module initialized");
return (0);

/**********
* o release shared mem
* o exit with error
**********/

initerr:
if (pmod_data->msgq_cnt)
  { shm_free (pmod_data->pmsgq_lst); }
shm_free (pmod_data);
pmod_data = NULL;
return (-1);
}