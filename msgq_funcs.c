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

/**********
* local constants
**********/

const str MTHD_ACK = STR_STATIC_INIT ("ACK");
const str MTHD_BYE = STR_STATIC_INIT ("BYE");
const str MTHD_INVITE = STR_STATIC_INIT ("INVITE");
const str MTHD_PRACK = STR_STATIC_INIT ("PRACK");

/**********
* local functions
**********/

/**********
* Process ACK Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: 0=unable to process; 1=processed
**********/

int ack_msg (sip_msg_t *pmsg, int msgq_idx)

{
/**********
* 
**********/

return (0);
}

/**********
* Process BYE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: 0=unable to process; 1=processed
**********/

int bye_msg (sip_msg_t *pmsg, int msgq_idx)

{
/**********
* 
**********/

return (0);
}

/**********
* Find msgq_id From URI
*
* INPUT:
*   Arg (1) = URI text pointer
* OUTPUT: queue index; -1 if unable to find
**********/

int find_msgq_id (char *uri)

{
int nidx;
char *pfnd;
msgq_lst *pqlst = pmod_data->pmsgq_lst;
do
  {
  /**********
  * o search for URI
  * o if uri parm, strip off and try again
  **********/

  for (nidx = 0; nidx < pmod_data->msgq_cnt; nidx++)
    {
    if (!strcmp (pqlst [nidx].msgq_uri, uri))
      { return (nidx); }
    }
  pfnd = strchr (uri, ';');
  if (!pfnd)
    { break; }
  *pfnd = '\0';
  }
while (1);
return (-1);
}

/**********
* Process First INVITE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: 0=unable to process; 1=processed
**********/

int first_invite_msg (sip_msg_t *pmsg, int msgq_idx)

{
/**********
* SDP exists?
**********/

if (!(pmsg->msg_flags & FL_SDP_BODY))
  { return (0); }
if (parse_sdp (pmsg))
  { return (0); }

/**********
* extract
* o Via
* o From
* o CSeq
* o Call-ID
* o Record-Route
* o Supported
* o Allow
**********/

/**********
* reply w/body
**********/

LM_INFO ("???INVITE has SDP");
/*
pmod_data->ptm->t_reply_with_body (ptrans, code, ptext, pbody, pnewhdr, ptotag);
*/
return (0);
}

/**********
* Process PRACK Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: 0=unable to process; 1=processed
**********/

int prack_msg (sip_msg_t *pmsg, int msgq_idx)

{
/**********
* 
**********/

return (0);
}

/**********
* Process reINVITE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
*   Arg (3) = tag string pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int reinvite_msg (sip_msg_t *pmsg, int msgq_idx, str *tag_value)

{
/**********
* 
**********/

return (0);
}

/**********
* Form Char Array from STR
*
* INPUT:
*   Arg (1) = str pointer
* OUTPUT: char pointer; NULL if unable to allocate
**********/

char *form_tmpstr (str *pstr)

{
char *pcstr = malloc (pstr->len + 1);
if (!pcstr)
  {
  LM_ERR ("Unable to allocate local memory!");
  return (NULL);
  }
memcpy (pcstr, pstr->s, pstr->len);
pcstr [pstr->len] = 0;
return (pcstr);
}

/**********
* Release Char Array
*
* INPUT:
*   Arg (1) = char pointer
* OUTPUT: none
**********/

void free_tmpstr (char *pcstr)

{
if (pcstr)
  { free (pcstr); }
return;
}

/**********
* external functions
**********/

/**********
* fixup functions
**********/

/**********
* count messages
**********/

int msgq_count_fixup (void **param, int param_no)

{
/*
LM_INFO ("???msgq_count_fixup ()");
*/
return (1);
}

/**********
* redirect messages
**********/

int msgq_redirect_fixup (void **param, int param_no)

{
/*
LM_INFO ("???msgq_redirect_fixup ()");
*/
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
/*
LM_INFO ("???msgq_count ()");
*/
return (1);
}

/**********
* Process Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = ???
*   Arg (3) = ???
* OUTPUT: none
**********/

int msgq_process (sip_msg_t *pmsg, char *p1, char *p2)

{
/*
LM_INFO ("???msgq_process ()");
*/
db1_con_t *pconn = msgq_dbconnect (); /* ??? */
if (pconn)
  {
  update_msgq_lst (pconn);
  msgq_dbdisconnect (pconn);
  }

/**********
* o parse headers
* o directed to message queue?
**********/

if (parse_headers (pmsg, HDR_EOH_F, 0) < 0)
  {
  LM_ERR ("Unable to parse header!");
  return (0);
  }
to_body_t *pto_body = get_to (pmsg);
char *tmpstr = form_tmpstr (&pto_body->uri);
if (!tmpstr)
  { return (0); }
int msgq_idx = find_msgq_id (tmpstr);
free_tmpstr (tmpstr);
if (msgq_idx < 0)
  { return (0); }

/**********
* check if
* o INVITE
* o PRACK
* o ACK
* o BYE
**********/

str smethod = REQ_LINE (pmsg).method;
LM_INFO ("???%.*s: [%d]%s", STR_FMT (&smethod),
  msgq_idx, pmod_data->pmsgq_lst [msgq_idx].msgq_uri);
if (!STR_EQ (smethod, MTHD_INVITE))
  {
  /**********
  * initial INVITE?
  **********/

  if (!pto_body->tag_value.len)
    { return (first_invite_msg (pmsg, msgq_idx)); }
  return (reinvite_msg (pmsg, msgq_idx, &pto_body->tag_value));
  }
if (!STR_EQ (smethod, MTHD_PRACK))
  { return (prack_msg (pmsg, msgq_idx)); }
if (!STR_EQ (smethod, MTHD_ACK))
  { return (ack_msg (pmsg, msgq_idx)); }
if (!STR_EQ (smethod, MTHD_BYE))
  { return (bye_msg (pmsg, msgq_idx)); }
return (0);
}

/**********
* redirect message
**********/

int msgq_redirect (sip_msg_t *msg, char *p1, char *p2)

{
/*
LM_INFO ("???msgq_redirect ()");
*/
return (1);
}