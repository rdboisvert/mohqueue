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
#include "msgq.h" /* ??? */
#include "msgq_db.h" /* ??? */
#include "msgq_funcs.h"

#include "../../modules/tm/tm_load.h"

/**********
* local functions
**********/

/**********
* fixup functions
**********/

/**********
* count messages
**********/

int msgq_count_fixup (void **param, int param_no)

{
LM_INFO ("msgq_count_fixup ()");
return (1);
}

/**********
* redirect messages
**********/

int msgq_redirect_fixup (void **param, int param_no)

{
LM_INFO ("msgq_redirect_fixup ()");
return (1);
}

/**********
* module functions
**********/

/**********
* count messages
**********/

int msgq_count (sip_msg_t *msg, char *p1, char *p2)

{
LM_INFO ("msgq_count ()");
return (1);
}

/**********
* process message
**********/

int msgq_process (sip_msg_t *msg, char *p1, char *p2)

{
LM_INFO ("msgq_process ()");
db1_con_t *pconn = msgq_dbconnect (); /* ??? */
if (pconn)
  {
  update_msgq_lst (pmod_data->pdb, pconn);
  msgq_dbdisconnect (pconn);
  }
return (1);
}

/**********
* redirect message
**********/

int msgq_redirect (sip_msg_t *msg, char *p1, char *p2)

{
LM_INFO ("msgq_redirect ()");
return (1);
}