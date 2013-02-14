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
#include "mohq_funcs.h"

/**********
* definitions
**********/

#define SIPEOL  "\r\n"
#define USRAGNT "Kamailio Message Queue"
#define CLENHDR "Content-Length"

/**********
* local constants
**********/

str p100rel [1] = {STR_STATIC_INIT ("100rel")};
str pinvite [1] = {STR_STATIC_INIT ("INVITE")};
str prefer [1] = {STR_STATIC_INIT ("REFER")};
str presp_ok [1] = {STR_STATIC_INIT ("OK")};
str presp_ring [1] = {STR_STATIC_INIT ("Ringing")};
str psipfrag [1] = {STR_STATIC_INIT ("message/sipfrag")};

char prefermsg [] =
  {
  "Max-Forwards: 70" SIPEOL
  "Refer-To: <%.*s>" SIPEOL
  "Referred-By: <%.*s>" SIPEOL
  };

/**********
* local function declarations
**********/

void delete_call (call_lst *);
int find_call (str *);
static void refer_cb (struct cell *, int, struct tmcb_params *);
int send_prov_rsp (sip_msg_t *, str *, call_lst *);
int send_rtp_answer (sip_msg_t *, str *, call_lst *);
int search_hdr_ext (struct hdr_field *, str *);

/**********
* local variables
**********/

db1_con_t *pconn;

void print_lumps (char *hdr, char *lmpname, sip_msg_t *pmsg, struct lump *plump)//???

{
LM_INFO ("%s: [%s] type=%d, op=%d", hdr, lmpname, plump->type, plump->op);
if (plump->op == LUMP_ADD)
  { LM_INFO ("ADD %.*s", plump->len, plump->u.value); }
if (plump->op == LUMP_DEL)
  { LM_INFO ("DEL %.*s", plump->len, &pmsg->buf [plump->u.offset]); }
if (plump->before)
  { print_lumps (hdr, "BEFORE", pmsg, plump->before); }
if (plump->after)
  { print_lumps (hdr, "AFTER", pmsg, plump->after); }
if (plump->next)
  { print_lumps (hdr, "NEXT", pmsg, plump->next); }
}

void print_rpl_lumps (char *hdr, char *lmpname, sip_msg_t *pmsg, struct lump_rpl *plump)//???

{
LM_INFO ("%s: [%s] flags=%d, %.*s", hdr, lmpname, plump->flags, STR_FMT (&plump->text));
if (plump->next)
  { print_rpl_lumps (hdr, "NEXT", pmsg, plump->next); }
}

void print_sip_lumps (char *hdr, sip_msg_t *pmsg)//???

{
if (pmsg->add_rm)
  { print_lumps (hdr, "add_rm", pmsg, pmsg->add_rm); }
if (pmsg->body_lumps)
  { print_lumps (hdr, "body_lumps", pmsg, pmsg->body_lumps); }
if (pmsg->reply_lump)
  { print_rpl_lumps (hdr, "reply_lump", pmsg, pmsg->reply_lump); }
}

void print_crlf_str (char *hdr, str *pstr)

{
int pos = 0;
int npos;
LM_INFO ("%s", hdr);
while (pos < pstr->len)
  {
  for (npos = pos; npos < pstr->len; npos++)
    {
    if (pstr->s [npos] == '\r')
      { break; }
    }
  LM_INFO ("%.*s", npos - pos, &pstr->s [pos]);
  pos = npos + 2;
  }
}

/**********
* local functions
**********/

/**********
* Process ACK Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: none
**********/

void ack_msg (sip_msg_t *pmsg, int mohq_idx)

{
/**********
* o get call ID
* o record exists?
* o part of INVITE?
**********/

char *pfncname = "ack_msg: ";
int ncall_idx = find_call (&pmsg->callid->body);
if (ncall_idx == -1)
  {
  LM_ERR ("%sNot part of existing dialog", pfncname);
  return;
  }
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
if (pcall->call_state != CLSTA_INVITED)
  { LM_ERR ("%sNot part of existing dialog", pfncname); }
else
  {
  wait_db_flush (pconn, pcall);
  pcall->call_state = CLSTA_ACKED;
  update_call_rec (pconn, pcall);
  }
return;
}

/**********
* Process BYE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: none
**********/

void bye_msg (sip_msg_t *pmsg, int mohq_idx)

{
/**********
* o get call ID
* o record exists?
**********/

char *pfncname = "bye_msg: ";
int ncall_idx = find_call (&pmsg->callid->body);
if (ncall_idx == -1)
  {
  LM_ERR ("%sNot part of existing dialog", pfncname);
  return;
  }

/**********
* o send OK
* o destroy proxy
* o teardown queue
**********/

sl_api_t *psl = pmod_data->psl;
if (psl->freply (pmsg, 200, presp_ok) < 0)
  { LM_ERR ("%sUnable to create reply", pfncname); }
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
if (pcall->call_state < CLSTA_INQUEUE)
  { LM_WARN ("%sEnding call before queue setup", pfncname); }
else
  {
  if (pmod_data->fn_rtp_destroy (pmsg, 0, 0) != 1)
    { LM_ERR ("%srtpproxy_destroy refused", pfncname); }
  }
delete_call (pcall);
return;
}

/**********
* Create New Call Record
*
* INPUT:
*   Arg (1) = queue index
*   Arg (2) = SIP message pointer
*   Arg (3) = totag str pointer
* OUTPUT: call index; -1 if unable to create
**********/

int create_call (int mohq_idx, sip_msg_t *pmsg, str *ptotag)

{
/**********
* o find inactive slot
* o get more memory if needed
**********/

char *pfncname = "create_call: ";
int ncall_idx = pmod_data->call_cnt;
for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
  {
  if (!pmod_data->pcall_lst [ncall_idx].call_active)
    { break; }
  }
if (ncall_idx == pmod_data->call_cnt)
  {
  if (!pmod_data->pcall_lst)
    { pmod_data->pcall_lst = shm_malloc (sizeof (call_lst)); }
  else
    {
    pmod_data->pcall_lst = shm_realloc (pmod_data->pcall_lst,
      sizeof (call_lst) * (ncall_idx + 1));
    }
  if (!pmod_data->pcall_lst)
    {
    LM_ERR ("%sUnable to allocate shared memory", pfncname);
    pmod_data->call_cnt = 0;
    return -1;
    }
  ncall_idx = pmod_data->call_cnt++;
  }

/**********
* add values to new entry
**********/

call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
pcall->call_active = 1;
pcall->mohq_id = pmod_data->pmohq_lst [mohq_idx].mohq_id;
pcall->call_state = 0;
str *pstr = &pmsg->callid->body;
strncpy (pcall->call_id, pstr->s, pstr->len);
pcall->call_id [pstr->len] = '\0';
pstr = &pmsg->from->body;
strncpy (pcall->call_from, pstr->s, pstr->len);
pcall->call_from [pstr->len] = '\0';
strncpy (pcall->call_tag, ptotag->s, ptotag->len);
pcall->call_tag [ptotag->len] = '\0';
if (!pmsg->contact)
  { *pcall->call_contact = '\0'; }
else
  {
  pstr = &pmsg->contact->body;
  strncpy (pcall->call_contact, pstr->s, pstr->len);
  pcall->call_contact [pstr->len] = '\0';
  }

/**********
* extract Via
**********/

hdr_field_t *phdr;
struct via_body *pvia;
int npos1 = 0;
int npos2;
char *pviabuf;
int nvia_max = sizeof (pcall->call_via);
int bovrflow = 0;
for (phdr = pmsg->h_via1; phdr; phdr = next_sibling_hdr (phdr))
  {
  for (pvia = (struct via_body *)phdr->parsed; pvia; pvia = pvia->next)
    {
    /**********
    * o skip trailing whitespace
    * o check if overflow
    **********/

    npos2 = pvia->bsize;
    pviabuf = pvia->name.s;
    while (npos2)
      {
      --npos2;
      if (pviabuf [npos2] == ' ' || pviabuf [npos2] == '\r'
        || pviabuf [npos2] == '\n' || pviabuf [npos2] == '\t' || pviabuf [npos2] == ',')
        { continue; }
      break;
      }
    if (npos2 + npos1 >= nvia_max)
      {
      LM_WARN ("%sVia buffer overflowed!", pfncname);
      bovrflow = 1;
      break;
      }

    /**********
    * copy via
    **********/

    if (npos1)
      { pcall->call_via [npos1++] = ','; }
    strncpy (&pcall->call_via [npos1], pviabuf, npos2);
    npos1 += npos2;
    pcall->call_via [npos1] = '\0';
    }
  if (bovrflow)
    { break; }
  }

/**********
* update DB
**********/

add_call_rec (pconn, ncall_idx);
return ncall_idx;
}

/**********
* Delete Call
*
* INPUT:
*   Arg (1) = call pointer
* OUTPUT: none
**********/

void delete_call (call_lst *pcall)

{
/**********
* o update DB
* o inactivate slot
**********/

wait_db_flush (pconn, pcall);
delete_call_rec (pconn, pcall);
pcall->call_active = 0;
return;
}

/**********
* Find Call from CallID
*
* INPUT:
*   Arg (1) = call ID str pointer
* OUTPUT: call index; -1 if unable to find
**********/

int find_call (str *pcallid)

{
int nidx;
str tmpstr;
for (nidx = 0; nidx < pmod_data->call_cnt; nidx++)
  {
  if (!pmod_data->pcall_lst [nidx].call_active)
    { continue; }
  tmpstr.s = pmod_data->pcall_lst [nidx].call_id;
  tmpstr.len = strlen (tmpstr.s);
  if (STR_EQ (tmpstr, *pcallid))
    { return nidx; }
  }
return -1;
}

/**********
* Find mohq_id From URI
*
* INPUT:
*   Arg (1) = URI text pointer
* OUTPUT: queue index; -1 if unable to find
**********/

int find_mohq_id (char *uri)

{
int nidx;
char *pfnd;
mohq_lst *pqlst = pmod_data->pmohq_lst;
do
  {
  /**********
  * o search for URI
  * o if uri parm, strip off and try again
  **********/

  for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
    {
    if (!strcmp (pqlst [nidx].mohq_uri, uri))
      { return nidx; }
    }
  pfnd = strchr (uri, ';');
  if (!pfnd)
    { break; }
  *pfnd = '\0';
  }
while (1);
return -1;
}

/**********
* Find Queue
*
* INPUT:
*   Arg (1) = queue name str pointer
* OUTPUT: queue index; -1 if unable to find
**********/

int find_queue (str *pqname)

{
int nidx;
str tmpstr;
for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  {
  tmpstr.s = pmod_data->pmohq_lst [nidx].mohq_name;
  tmpstr.len = strlen (tmpstr.s);
  if (STR_EQ (tmpstr, *pqname))
    { return nidx; }
  }
LM_ERR ("Unable to find queue (%.*s)!", STR_FMT (pqname));
return -1;
}

/**********
* Process First INVITE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: none
**********/

void first_invite_msg (sip_msg_t *pmsg, int mohq_idx)

{
/**********
* o get call ID
* o record exists?
**********/

char *pfncname = "first_invite_msg: ";
tm_api_t *ptm = pmod_data->ptm;
int ncall_idx = find_call (&pmsg->callid->body);
if (ncall_idx != -1)
  {
  //??? need to destroy
  return;
  }

/**********
* o create new transaction
* o SDP exists?
* o accepts REFER?
* o send rtpproxy offer
**********/

if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction", pfncname);
  return;
  }
if (!(pmsg->msg_flags & FL_SDP_BODY))
  {
  if (parse_sdp (pmsg))
    {
    LM_ERR ("%sINVITE lacks SDP", pfncname);
    if (ptm->t_reply (pmsg, 603, "INVITE lacks SDP") < 0)
      { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
    return;
    }
  }
if (!search_hdr_ext (pmsg->allow, prefer))
  {
  LM_ERR ("%sINVITE lacks ability to use REFER", pfncname);
  if (ptm->t_reply (pmsg, 603, "INVITE lacks REFER") < 0)
    { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
  return;
  }
if (pmod_data->fn_rtp_offer (pmsg, 0, 0) != 1)
  {
  LM_ERR ("%srtpproxy_offer refused", pfncname);
  if (ptm->t_reply (pmsg, 503, "Unable to proxy INVITE") < 0)
    { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
  return;
  }

/**********
* create call record
**********/

str ptotag [1] = {STR_NULL};
if (ptm->t_get_reply_totag (pmsg, ptotag) != 1)
  {
  LM_ERR ("%sUnable to create totag", pfncname);
  return;
  }
ncall_idx = create_call (mohq_idx, pmsg, ptotag);
if (ncall_idx == -1)
  { return; }

/**********
* o send working response
* o supports/requires PRACK? (RFC 3262 section 3)
* o exit if not ringing
**********/

call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
pcall->call_cseq = 1;
if (ptm->t_reply (pmsg, 100, "Your call is important to us") < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE", pfncname);
  delete_call (pcall);
  return;
  }
if (search_hdr_ext (pmsg->supported, p100rel)
  || search_hdr_ext (pmsg->require, p100rel))
  { send_prov_rsp (pmsg, ptotag, pcall); }
else
  {
  if (ptm->t_reply (pmsg, 180, presp_ring->s) < 0)
    { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
  else
    {
    wait_db_flush (pconn, pcall);
    pcall->call_state = CLSTA_RING;
    update_call_rec (pconn, pcall);
    }
  }
if (pcall->call_state != CLSTA_RING)
  {
  delete_call (pcall);
  return;
  }

/**********
* since t_reply_with_body takes the shortcut of unreferencing the
* transaction we have to create a SIP body send it through rtpproxy
* and then use the result to build the final response
**********/

send_rtp_answer (pmsg, ptotag, pcall);
pcall->call_cseq = 1;
return;
}

/**********
* Process NOTIFY Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: none
**********/

void notify_msg (sip_msg_t *pmsg, int mohq_idx)

{
/**********
* o get call ID
* o record exists?
* o waiting on REFER?
**********/

char *pfncname = "notify_msg: ";
int ncall_idx = find_call (&pmsg->callid->body);
if (ncall_idx == -1)
  {
  LM_ERR ("%sNot part of existing dialog", pfncname);
  return;
  }
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
if (pcall->call_state != CLSTA_RFRWAIT)
  {
  LM_ERR ("%sNot waiting on a REFER", pfncname);
  return;
  }

/**********
* o sipfrag?
* o get status from body
**********/

if (!search_hdr_ext (pmsg->content_type, psipfrag))
  {
  LM_ERR ("%sNot a sipfrag type", pfncname);
  return;
  }
str pbody [1];
pbody->s = get_body (pmsg);
if (pbody->s)
  { pbody->len = pmsg->len - (int)(pbody->s - pmsg->buf); }
/*
* tricky code!
* we go past the buffer end because parse_first_line needs more than the
* first line but doesn't actually touch the data beyond it
*/
if (!pbody->len++)
  {
  LM_ERR ("%ssipfrag body missing", pfncname);
  return;
  }
struct msg_start pstart [1];
parse_first_line (pbody->s, pbody->len, pstart);
if (pstart->type != SIP_REPLY)
  {
  LM_ERR ("%sreply missing", pfncname);
  return;
  }

/**********
* o send OK
* o respond to REFER status
**********/

if (pmod_data->psl->freply (pmsg, 200, presp_ok) < 0)
  {
  LM_ERR ("%sUnable to create reply", pfncname);
  return;
  }
switch (pstart->u.reply.statuscode / 100)
  {
  case 1:
    break;
  case 2:
    pcall->call_state = CLSTA_RFRDONE;
    break;
  default:
    pcall->call_state = CLSTA_RFRFAIL;
  }
return;
}

/**********
* Process PRACK Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: none
**********/

void prack_msg (sip_msg_t *pmsg, int mohq_idx)

{
/**********
* o get call ID
* o record exists?
**********/

char *pfncname = "prack_msg: ";
tm_api_t *ptm = pmod_data->ptm;
int ncall_idx = find_call (&pmsg->callid->body);
if (ncall_idx == -1)
  {
  LM_ERR ("%sNot part of existing dialog", pfncname);
  return;
  }

/**********
* check RAck???
**********/

/**********
* o accept PRACK
* o update call state
**********/

call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
wait_db_flush (pconn, pcall);
if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction", pfncname);
  pcall->call_state = CLSTA_ERR;
  }
else if (!ptm->t_reply (pmsg, 200, "OK"))
  {
  LM_ERR ("%sUnable to reply to PRACK", pfncname);
  pcall->call_state = CLSTA_ERR;
  }
else
  { pcall->call_state = CLSTA_PRINGACK; }
update_call_rec (pconn, pcall);
return;
}

/**********
* Process reINVITE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
*   Arg (3) = tag string pointer
* OUTPUT: none
**********/

void reinvite_msg (sip_msg_t *pmsg, int mohq_idx, str *tag_value)

{
/**********
* 
**********/

return;
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
  return NULL;
  }
memcpy (pcstr, pstr->s, pstr->len);
pcstr [pstr->len] = 0;
return pcstr;
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
* Count Messages
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue name
*   Arg (3) = pv result name
* OUTPUT: -1 if no items in queue; else result = count
**********/

int mohq_count (sip_msg_t *pmsg, pv_elem_t *pqueue, char *presult)

{
/**********
* get queue and pv names
**********/

char *pfncname = "mohq_count: ";
str pavp [1], pqname [1];
if (!pqueue || !presult)
  {
  LM_ERR ("%sParameters missing!", pfncname);
  return -1;
  }
if (pv_printf_s (pmsg, pqueue, pqname))
  {
  LM_ERR ("%sUnable to extract queue name!", pfncname);
  return -1;
  }

/**********
* o pv provided?
* o create pv if not exist
**********/

pavp->s = presult;
pavp->len = strlen (presult);
if (!pavp->len)
  {
  LM_ERR ("%sResult pv name missing!", pfncname);
  return -1;
  }
pv_spec_t *pavp_spec = pv_cache_get (pavp);
if (!pavp_spec)
  {
  LM_ERR ("%sUnable to create pv (%.*s)!", pfncname, STR_FMT (pavp));
  return -1;
  }

/**********
* o find queue
* o count items in queue
**********/

int nq_idx = find_queue (pqname);
int ncount = 0;
call_lst *pcalls = pmod_data->pcall_lst;
int ncall_idx, mohq_id;
if (nq_idx != -1)
  {
  mohq_id = pmod_data->pmohq_lst [nq_idx].mohq_id;
  for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
    {
    if (!pcalls [ncall_idx].call_active)
      { continue; }
    if (pcalls [ncall_idx].mohq_id == mohq_id
      && pcalls [ncall_idx].call_state == CLSTA_INQUEUE)
      { ncount++; }
    }
  }

/**********
* o set pv result
* o exit with result
**********/

pv_value_t pavp_val [1];
memset (pavp_val, 0, sizeof (pv_value_t));
pavp_val->ri = ncount;
pavp_val->flags = PV_TYPE_INT | PV_VAL_INT;
if (pv_set_spec_value (0, pavp_spec, 0, pavp_val) < 0)
  {
  LM_ERR ("%sUnable to set pv value (%.*s)!", pfncname, STR_FMT (pavp));
  return -1;
  }
return 1;
}

/**********
* Process Message
*
* INPUT:
*   Arg (1) = SIP message pointer
* OUTPUT: -1=not directed to queue; 0=exit script
**********/

int mohq_process (sip_msg_t *pmsg)

{
/**********
* o parse headers
* o directed to message queue?
* o connect to database
**********/

if (parse_headers (pmsg, HDR_EOH_F, 0) < 0)
  {
  LM_ERR ("Unable to parse header!");
  return -1;
  }
to_body_t *pto_body = get_to (pmsg);
char *tmpstr = form_tmpstr (&pto_body->uri);
if (!tmpstr)
  { return -1; }
int mohq_idx = find_mohq_id (tmpstr);
free_tmpstr (tmpstr);
if (mohq_idx < 0)
  { return -1; }
pconn = mohq_dbconnect ();
if (pconn)
  { update_mohq_lst (pconn); }
else
  { LM_WARN ("Unable to connect to DB"); }

/**********
* process message
**********/

str smethod = REQ_LINE (pmsg).method;
LM_INFO ("???%.*s: [%d]%s", STR_FMT (&smethod),
  mohq_idx, pmod_data->pmohq_lst [mohq_idx].mohq_uri);
switch (pmsg->REQ_METHOD)
  {
  case METHOD_INVITE:
    /**********
    * initial INVITE?
    **********/

    if (!pto_body->tag_value.len)
      { first_invite_msg (pmsg, mohq_idx); }
    else
      { reinvite_msg (pmsg, mohq_idx, &pto_body->tag_value); }
    break;
  case METHOD_NOTIFY:
    notify_msg (pmsg, mohq_idx);
    break;
  case METHOD_PRACK:
    prack_msg (pmsg, mohq_idx);
    break;
  case METHOD_ACK:
    ack_msg (pmsg, mohq_idx);
    break;
  case METHOD_BYE:
    bye_msg (pmsg, mohq_idx);
    break;
  }
if (pconn)
  { mohq_dbdisconnect (pconn); }
return 0;
}

/**********
* redirect message
**********/

/**********
* Redirect Oldest Queued Call
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue name
*   Arg (3) = redirect URI
* OUTPUT: -1 if no items in queue; else redirects oldest call
**********/

int mohq_redirect (sip_msg_t *pmsg, pv_elem_t *pqueue, pv_elem_t *pURI)

{
/**********
* o get queue name and URI
* o check URI
**********/

char *pfncname = "mohq_redirect: ";
str puri [1], pqname [1];
if (!pqueue || !pURI)
  {
  LM_ERR ("%sParameters missing!", pfncname);
  return -1;
  }
if (pv_printf_s (pmsg, pqueue, pqname))
  {
  LM_ERR ("%sUnable to extract queue name!", pfncname);
  return -1;
  }
if (pv_printf_s (pmsg, pURI, puri))
  {
  LM_ERR ("%sUnable to extract URI!", pfncname);
  return -1;
  }
struct sip_uri puri_parsed [1];
if (parse_uri (puri->s, puri->len, puri_parsed))
  {
  LM_ERR ("%sInvalid URI (%.*s)!", pfncname, STR_FMT (puri));
  return -1;
  }

/**********
* o find queue
* o find oldest call
**********/

//??? need to lock queue while processing
int nq_idx = find_queue (pqname);
if (nq_idx == -1)
  { return -1; }
call_lst *pcall = 0;// avoids complaint from compiler about uninitialized
int ncall_idx;
for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
  {
  pcall = &pmod_data->pcall_lst [ncall_idx];
  if (pcall->call_active)
    { break; }
//??? need to really find
  }
if (ncall_idx == pmod_data->call_cnt)
  {
  LM_ERR ("%sNo calls in queue (%.*s)", pfncname, STR_FMT (pqname));
  return -1;
  }

/**********
* o find URI/tag in from
* o find target from contact or from header
**********/

struct to_body ptob [1];
parse_to (pcall->call_from,
  &pcall->call_from [strlen (pcall->call_from) + 1], ptob);
if (ptob->error != PARSE_OK)
  {
  // should never happen
  LM_ERR ("%sInvalid from URI (%s)!", pfncname, pcall->call_from);
  return -1;
  }
if (ptob->param_lst)
  { free_to_params (ptob); }
struct to_body pcontact [1];
str ptarget [1];
if (!*pcall->call_contact)
  {
  ptarget->s = ptob->uri.s;
  ptarget->len = ptob->uri.len;
  }
else
  {
  parse_to (pcall->call_contact,
    &pcall->call_contact [strlen (pcall->call_contact) + 1], pcontact);
  if (pcontact->error != PARSE_OK)
    {
    // should never happen
    LM_ERR ("%sInvalid contact (%s)!", pfncname, pcall->call_contact);
    return -1;
    }
  if (pcontact->param_lst)
    { free_to_params (pcontact); }
  ptarget->s = pcontact->uri.s;
  ptarget->len = pcontact->uri.len;
  }

/**********
* form REFER headers
* o calculate basic size
* o add Via size
* o create buffer
**********/

int npos1 = sizeof (prefermsg) // REFER template
  + puri->len // redirect URI
  + (strlen (pcall->call_from) * 2); // inqueue URI twice
#if 0 //???
char *pvia [3];
if (!pcall->call_via [0])
  { pvia [0] = pvia [1] = pvia [2] = ""; }
else
  {
  npos1 += strlen (pcall->call_via) + 7;
  pvia [0] = "Via: ";
  pvia [1] = pcall->call_via;
  pvia [2] = SIPEOL;
  }
#endif //???
char *pbuf = pkg_malloc (npos1);
if (!pbuf)
  {
  LM_ERR ("%sNo more memory", pfncname);
  return -1;
  }
sprintf (pbuf, prefermsg,
  STR_FMT (puri), // Refer-To
  STR_FMT (&ptob->uri)); // Referred-By

/**********
* create dialog
**********/

dlg_t *pdlg = (dlg_t *)pkg_malloc (sizeof (dlg_t));
if (!pdlg)
  {
  LM_ERR ("%sNo more memory", pfncname);
  goto refererr;
  }
memset (pdlg, 0, sizeof (dlg_t));
pdlg->loc_seq.value = pcall->call_cseq++;
pdlg->loc_seq.is_set = 1;
pdlg->id.call_id.s = pcall->call_id;
pdlg->id.call_id.len = strlen (pcall->call_id);
pdlg->id.loc_tag.s = pcall->call_tag;
pdlg->id.loc_tag.len = strlen (pcall->call_tag);
pdlg->id.rem_tag.s = ptob->tag_value.s;
pdlg->id.rem_tag.len = ptob->tag_value.len;
pdlg->rem_target.s = ptarget->s;
pdlg->rem_target.len = ptarget->len;
pdlg->loc_uri.s = pmod_data->pmohq_lst [nq_idx].mohq_uri;
pdlg->loc_uri.len = strlen (pdlg->loc_uri.s);
pdlg->rem_uri.s = ptob->uri.s;
pdlg->rem_uri.len = ptob->uri.len;
pdlg->state = DLG_CONFIRMED;

/**********
* send REFER request
**********/

tm_api_t *ptm = pmod_data->ptm;
uac_req_t puac [1];
str phdrs [1];
phdrs->s = pbuf;
phdrs->len = strlen (pbuf);
set_uac_req (puac, prefer, phdrs, 0, pdlg,
  TMCB_LOCAL_COMPLETED, refer_cb, pcall);
pcall->call_state = CLSTA_REFER;
if (ptm->t_request_within (puac) < 0)
  {
  pcall->call_state = CLSTA_INQUEUE;
  LM_ERR ("%sUnable to create REFER request!", pfncname);
  goto refererr;
  }

/**********
* o wait for dialog to complete
* o destroy proxy
* o teardown queue
**********/

while (1)
  {
  usleep (USLEEP_LEN);
  if (pcall->call_state > CLSTA_RFRWAIT)
    { break; }
  }
if (pcall->call_state == CLSTA_RFRFAIL)
  { pcall->call_state = CLSTA_INQUEUE; }
else
  {
  if (pmod_data->fn_rtp_destroy (pmsg, 0, 0) != 1)
    { LM_ERR ("%srtpproxy_destroy refused", pfncname); }
  delete_call (pcall);
//??? send BYE?
  }

refererr:
//???pkg_free (pdlg);
//???pkg_free (pbuf);
return 1;
}

/**********
* REFER Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parms
* OUTPUT: none
**********/

static void refer_cb
  (struct cell *ptrans, int ntype, struct tmcb_params *pcbp)

{
LM_INFO ("Referral reply=%d", pcbp->rpl->first_line.u.reply.statuscode);
call_lst *pcall = (call_lst *)*pcbp->param;
if (REPLY_CLASS (pcbp->rpl) == 2)
  { pcall->call_state = CLSTA_RFRWAIT; }
else
  { pcall->call_state = CLSTA_INQUEUE; }
return;
}

/**********
* Search Header for Extension
*
* INPUT:
*   Arg (1) = header field pointer
*   Arg (2) = extension str pointer
* OUTPUT: 0=not found
**********/

int search_hdr_ext (struct hdr_field *phdr, str *pext)

{
if (!phdr)
  { return 0; }
str *pstr = &phdr->body;
int npos1, npos2;
for (npos1 = 0; npos1 < pstr->len; npos1++)
  {
  /**********
  * o find non-space
  * o search to end, space or comma
  * o same size?
  * o same name?
  **********/

  if (pstr->s [npos1] == ' ')
    { continue; }
  for (npos2 = npos1++; npos1 < pstr->len; npos1++)
    {
    if (pstr->s [npos1] == ' ' || pstr->s [npos1] == ',')
      { break; }
    }
  if (npos1 - npos2 != pext->len)
    { continue; }
  if (!strncasecmp (&pstr->s [npos2], pext->s, pext->len))
    { return 1; }
  }
return 0;
}

/**********
* Send Provisional Response
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = totag str pointer
*   Arg (3) = call pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_prov_rsp (sip_msg_t *pmsg, str *ptotag, call_lst *pcall)

{
/**********
* o send ringing response with require
* o update record
**********/

char *pfncname = "send_prov_rsp: ";
tm_api_t *ptm = pmod_data->ptm;
struct cell *ptrans = ptm->t_gett ();
pcall->call_cseq = rand ();
char phdrtmp [200];
char phdrtmplt [] =
  "Accept-Language: en" SIPEOL
  "Allow-Events: conference,talk,hold" SIPEOL
  "Require: 100rel" SIPEOL
  "RSeq: %d" SIPEOL
  "User-Agent: " USRAGNT SIPEOL
  ;
sprintf (phdrtmp, phdrtmplt, pcall->call_cseq);
str phdr [1];
phdr->s = phdrtmp;
phdr->len = strlen (phdrtmp);
str pbody [1] = {STR_NULL};
if (ptm->t_reply_with_body
  (ptrans, 180, presp_ring, pbody, phdr, ptotag) < 0)
  {
  LM_ERR ("%sUnable to create reply", pfncname);
  return 0;
  }
wait_db_flush (pconn, pcall);
pcall->call_state = CLSTA_PRING;
update_call_rec (pconn, pcall);

/**********
* wait until PRACK acknowledged
**********/

while (1)
  {
  usleep (USLEEP_LEN);
  if (pcall->call_state != CLSTA_PRING)
    { break; }
//need to resend RING if no response???
  }
if (pcall->call_state == CLSTA_PRINGACK)
  {
  wait_db_flush (pconn, pcall);
  pcall->call_state = CLSTA_RING;
  update_call_rec (pconn, pcall);
  }
return 1;
}

/**********
* Send RTPProxy Answer
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = totag str pointer
*   Arg (3) = call pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_rtp_answer (sip_msg_t *pmsg, str *ptotag, call_lst *pcall)

{
/**********
* build response from request
**********/

char *pfncname = "send_rtp_answer: ";
tm_api_t *ptm = pmod_data->ptm;
struct cell *ptrans = ptm->t_gett ();
str pbuf [1] = {STR_NULL};
struct bookmark pBM [1];
int nrc = 0;
pbuf->s = build_res_buf_from_sip_req (200, presp_ok, ptotag, ptrans->uas.request,
  (unsigned int *)&pbuf->len, pBM);
if (!pbuf->s || !pbuf->len)
  {
  LM_ERR ("%sUnable to build response", pfncname);
  goto answer_done;
  }

/**********
* parse out first line and headers
**********/

char pclenhdr [] = CLENHDR;
str pparse [20];
int npos1, npos2;
int nhdrcnt = 0;
for (npos1 = 0; npos1 < pbuf->len; npos1++)
  {
  /**********
  * find EOL
  **********/

  for (npos2 = npos1++; npos1 < pbuf->len; npos1++)
    {
    /**********
    * o not EOL? (CRLF assumed)
    * o next line a continuation? (RFC 3261 section 7.3.1)
    **********/

    if (pbuf->s [npos1] != '\n')
      { continue; }
    if (npos1 + 1 == pbuf->len)
      { break; }
    if (pbuf->s [npos1 + 1] == ' '
      || pbuf->s [npos1 + 1] == '\t')
      { continue; }
    break;
    }

  /**********
  * o blank is end of header (RFC 3261 section 7)
  * o ignore Content-Length (assume followed by colon)
  * o save header
  **********/

  if (npos1 - npos2 == 1)
    { break; }
  if (npos1 - npos2 > 14)
    {
    if (!strncasecmp (&pbuf->s [npos2], pclenhdr, 14))
      { continue; }
    }
  pparse [nhdrcnt].s = &pbuf->s [npos2];
  pparse [nhdrcnt++].len = npos1 - npos2 + 1;
  }

/**********
* define SDP body and headers
*
* NOTES:
* o IP address is faked since it will be replaced
* o all audio streams are offered but only available will be used
**********/

str pextrahdr [1] =
  {
  STR_STATIC_INIT (
  "Allow: INVITE, ACK, BYE, CANCEL, MESSAGE, SUBSCRIBE, NOTIFY, PRACK, UPDATE, REFER" SIPEOL
  "Accept-Language: en" SIPEOL
  "Content-Type: application/sdp" SIPEOL
  "User-Agent: " USRAGNT SIPEOL
  )
  };

str pSDP [1] =
  {
  STR_STATIC_INIT (
  "v=0" SIPEOL
  "o=- 1167618058 1167618058 IN IP4 1.1.1.1" SIPEOL
  "s=" USRAGNT SIPEOL
  "c=IN IP4 1.1.1.1" SIPEOL
  "t=0 0" SIPEOL
  "a=sendrecv" SIPEOL
  "m=audio 2230 RTP/AVP 9 0 8 18 127" SIPEOL
  "a=rtpmap:9 G722/8000" SIPEOL
  "a=rtpmap:0 PCMU/8000" SIPEOL
  "a=rtpmap:8 PCMA/8000" SIPEOL
  "a=rtpmap:18 G729/8000" SIPEOL
  "a=rtpmap:127 telephone-event/8000" SIPEOL
  )
  };

/**********
* recreate buffer with extra headers and SDP
* o count hdrs, extra hdrs, content-length hdr, SDP
* o alloc new buffer
* o form new buffer
* o replace orig buffer
**********/

for (npos1 = npos2 = 0; npos2 < nhdrcnt; npos2++)
  { npos1 += pparse [npos2].len; }
char pbodylen [30];
sprintf (pbodylen, "%s: %d\r\n\r\n", pclenhdr, pSDP->len);
npos1 += pextrahdr->len + strlen (pbodylen) + pSDP->len;
char *pnewbuf = pkg_malloc (npos1);
if (!pnewbuf)
  {
  LM_ERR ("%sNo more memory", pfncname);
  goto answer_done;
  }
for (npos1 = npos2 = 0; npos2 < nhdrcnt; npos2++)
  {
  memcpy (&pnewbuf [npos1], pparse [npos2].s, pparse [npos2].len);
  npos1 += pparse [npos2].len;
  }
npos2 = pextrahdr->len;
memcpy (&pnewbuf [npos1], pextrahdr->s, npos2);
npos1 += npos2;
npos2 = strlen (pbodylen);
memcpy (&pnewbuf [npos1], pbodylen, npos2);
npos1 += npos2;
npos2 = pSDP->len;
memcpy (&pnewbuf [npos1], pSDP->s, npos2);
npos1 += npos2;
pkg_free (pbuf->s);
pbuf->s = pnewbuf;
pbuf->len = npos1;

/**********
* build SIP msg
**********/

struct sip_msg pnmsg [1];
build_sip_msg_from_buf (pnmsg, pbuf->s, pbuf->len, 1);
memcpy (&pnmsg->rcv, &pmsg->rcv, sizeof (struct receive_info)); //??? cheating

/**********
* send rtpproxy answer
**********/

if (pmod_data->fn_rtp_answer (pnmsg, 0, 0) != 1)
  {
  LM_ERR ("%srtpproxy_answer refused", pfncname);
  goto answer_done;
  }
str pMOH [1] = {STR_STATIC_INIT ("/var/build/music_on_hold")};
pv_elem_t *pmodel;
pv_parse_format (pMOH, &pmodel);
if (pmod_data->fn_rtp_stream2uac (pnmsg, (char *)pmodel, (char *)-1) != 1) /* ??? */
  {
  LM_ERR ("%srtpproxy_stream2uac refused", pfncname);
  goto answer_done;
  }

/**********
* o create buffer from response
* o find SDP
**********/

pkg_free (pbuf->s);
pbuf->s = build_res_buf_from_sip_res (pnmsg, (unsigned int *)&pbuf->len);
if (!pbuf->s || !pbuf->len)
  {
  LM_ERR ("%sUnable to build new response", pfncname);
  goto answer_done;
  }
str pnewSDP [1];
for (npos1 = 0; npos1 < pbuf->len; npos1++)
  {
  if (pbuf->s [npos1] != '\n')
    { continue; }
  if (pbuf->s [npos1 - 3] == '\r')
    { break; }
  }
pnewSDP->s = &pbuf->s [npos1 + 1];
pnewSDP->len = pbuf->len - npos1 - 1;

/**********
* send adjusted reply
**********/

if (ptm->t_reply_with_body
  (ptrans, 200, presp_ok, pnewSDP, pextrahdr, ptotag) < 0)
  {
  LM_ERR ("%sUnable to create reply", pfncname);
  goto answer_done;
  }
wait_db_flush (pconn, pcall);
pcall->call_state = CLSTA_INVITED;
update_call_rec (pconn, pcall);

/**********
* wait for ACK
**********/

while (1)
  {
  usleep (USLEEP_LEN);
  if (pcall->call_state != CLSTA_INVITED)
    { break; }
//need to resend OK if no response???
  }
if (pcall->call_state != CLSTA_ACKED)
  {
  //??? something needs to be done
  }
wait_db_flush (pconn, pcall);
pcall->call_state = CLSTA_INQUEUE;
update_call_rec (pconn, pcall);
nrc = 1;

/**********
* free buffer and return
**********/

answer_done:
if (pbuf->s)
  { pkg_free (pbuf->s); }
return nrc;
}