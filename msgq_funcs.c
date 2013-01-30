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
#include "msgq_funcs.h"

/**********
* definitions
**********/

#define SIPEOL  "\r\n"
#define USRAGNT "Kamailio Message Queue"
#define CLENHDR "Content-Length"

/**********
* local constants
**********/

str MTHD_ACK = STR_STATIC_INIT ("ACK");
str MTHD_BYE = STR_STATIC_INIT ("BYE");
str MTHD_INVITE = STR_STATIC_INIT ("INVITE");
str MTHD_PRACK = STR_STATIC_INIT ("PRACK");

str p100rel [1] = {STR_STATIC_INIT ("100rel")};

str presp_ok [1] = {STR_STATIC_INIT ("OK")};
str presp_ring [1] = {STR_STATIC_INIT ("Ringing")};

/**********
* local function declarations
**********/

int send_prov_rsp (sip_msg_t *, str *, int);
int send_rtp_answer (sip_msg_t *, struct cell *, str *);
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
  for (npos = 1; npos < pstr->len - pos; npos++)
    {
    if (pstr->s [npos + pos] == '\r')
      { break; }
    }
  LM_INFO ("%.*s", npos, &pstr->s [pos]);
  pos += npos + 2;
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

void ack_msg (sip_msg_t *pmsg, int msgq_idx)

{
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

void bye_msg (sip_msg_t *pmsg, int msgq_idx)

{
/**********
* 
**********/

return;
}

/**********
* Create New Call from CallID
*
* INPUT:
*   Arg (1) = call ID str pointer
*   Arg (2) = from str pointer
*   Arg (3) = totag str pointer
*   Arg (4) = queue index
* OUTPUT: call index; -1 if unable to create
**********/

int create_call (str *pcallid, str *pfrom, str *ptotag, int msgq_idx)

{
/**********
* o find inactive slot
* o get more memory if needed
**********/

char *pfncname = "create_call: ";
int nidx = pmod_data->call_cnt;
for (nidx = 0; nidx < pmod_data->call_cnt; nidx++)
  {
  if (!pmod_data->pcall_lst [nidx].call_active)
    { break; }
  }
if (nidx == pmod_data->call_cnt)
  {
  if (!pmod_data->pcall_lst)
    { pmod_data->pcall_lst = shm_malloc (sizeof (call_lst)); }
  else
    {
    pmod_data->pcall_lst = shm_realloc (pmod_data->pcall_lst,
      sizeof (call_lst) * (nidx + 1));
    }
  if (!pmod_data->pcall_lst)
    {
    LM_ERR ("%sUnable to allocate shared memory", pfncname);
    pmod_data->call_cnt = 0;
    return -1;
    }
  nidx = pmod_data->call_cnt++;
  }

/**********
* add new entry to list
**********/

call_lst *pclst = pmod_data->pcall_lst;
pclst [nidx].call_active = 1;
pclst [nidx].msgq_id = msgq_idx;
pclst [nidx].call_state = 0;
strncpy (pclst [nidx].call_id, pcallid->s, pcallid->len);
pclst [nidx].call_id [pcallid->len] = '\0';
strncpy (pclst [nidx].call_from, pfrom->s, pfrom->len);
pclst [nidx].call_from [pfrom->len] = '\0';
strncpy (pclst [nidx].call_tag, ptotag->s, ptotag->len);
pclst [nidx].call_tag [ptotag->len] = '\0';

/**********
* update DB
**********/

add_call_rec (pconn, nidx);
return nidx;
}

/**********
* Delete Call
*
* INPUT:
*   Arg (1) = call index
* OUTPUT: none
**********/

void delete_call (int ncidx)

{
/**********
* o update DB
* o inactivate slot
**********/

delete_call_rec (pconn, ncidx);
pmod_data->pcall_lst [ncidx].call_active = 0;
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
  if (!STR_EQ (tmpstr, *pcallid))
    { return nidx; }
  }
return -1;
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
* Process First INVITE Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = queue index
* OUTPUT: none
**********/

void first_invite_msg (sip_msg_t *pmsg, int msgq_idx)

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
  }

/**********
* o create new transaction
* o SDP exists?
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
ncall_idx = create_call (&pmsg->callid->body, &pmsg->from->body, ptotag, msgq_idx);
if (ncall_idx != -1)
  { return; }

/**********
* o send working response
* o supports/requires PRACK? (RFC 3262 section 3)
* o exit if not ringing
**********/

if (ptm->t_reply (pmsg, 100, "Your call is important to us") < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE", pfncname);
  delete_call (ncall_idx);
  return;
  }
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
if (search_hdr_ext (pmsg->supported, p100rel)
  || search_hdr_ext (pmsg->require, p100rel))
  { send_prov_rsp (pmsg, ptotag, ncall_idx); }
else
  {
  if (ptm->t_reply (pmsg, 180, presp_ring->s) < 0)
    { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
  else
    {
    pcall->call_state = CLSTA_RING;
    update_call_rec (pconn, ncall_idx);
    }
  }
if (pcall->call_state != CLSTA_RING)
  {
  delete_call (ncall_idx);
  return;
  }

/**********
* since t_reply_with_body takes the shortcut of unreferencing the
* transaction we have to create a SIP body send it through rtpproxy
* and then use the result to build the final response
**********/

struct cell *ptrans = ptm->t_gett ();
if (!send_rtp_answer (pmsg, ptrans, ptotag))
  { pcall->call_state = CLSTA_ERR; }
else
  { pcall->call_state = CLSTA_INQUEUE; }
update_call_rec (pconn, ncall_idx);
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

void prack_msg (sip_msg_t *pmsg, int msgq_idx)

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
  LM_ERR ("%sNot part of existing transaction", pfncname);
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
update_call_rec (pconn, ncall_idx);
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

void reinvite_msg (sip_msg_t *pmsg, int msgq_idx, str *tag_value)

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
return 1;
}

/**********
* redirect messages
**********/

int msgq_redirect_fixup (void **param, int param_no)

{
/*
LM_INFO ("???msgq_redirect_fixup ()");
*/
return 1;
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
return 1;
}

/**********
* Process Message
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = ignore
*   Arg (3) = ignore
* OUTPUT: none
**********/

int msgq_process (sip_msg_t *pmsg, char *p1, char *p2)

{
/**********
* o parse headers
* o directed to message queue?
* o connect to database
**********/

if (parse_headers (pmsg, HDR_EOH_F, 0) < 0)
  {
  LM_ERR ("Unable to parse header!");
  return 0;
  }
to_body_t *pto_body = get_to (pmsg);
char *tmpstr = form_tmpstr (&pto_body->uri);
if (!tmpstr)
  { return 0; }
int msgq_idx = find_msgq_id (tmpstr);
free_tmpstr (tmpstr);
if (msgq_idx < 0)
  { return 0; }
pconn = msgq_dbconnect ();
if (pconn)
  { update_msgq_lst (pconn); }
else
  { LM_WARN ("Unable to connect to DB"); }

/**********
* process message
**********/

str smethod = REQ_LINE (pmsg).method;
LM_INFO ("???%.*s: [%d]%s", STR_FMT (&smethod),
  msgq_idx, pmod_data->pmsgq_lst [msgq_idx].msgq_uri);
switch (pmsg->REQ_METHOD)
  {
  case METHOD_INVITE:
    /**********
    * initial INVITE?
    **********/

    if (!pto_body->tag_value.len)
      { first_invite_msg (pmsg, msgq_idx); }
    else
      { reinvite_msg (pmsg, msgq_idx, &pto_body->tag_value); }
    break;
  case METHOD_PRACK:
    prack_msg (pmsg, msgq_idx);
    break;
  case METHOD_ACK:
    ack_msg (pmsg, msgq_idx);
    break;
  case METHOD_BYE:
    bye_msg (pmsg, msgq_idx);
    break;
  }
if (pconn)
  { msgq_dbdisconnect (pconn); }
return 1;
}

/**********
* redirect message
**********/

int msgq_redirect (sip_msg_t *msg, char *p1, char *p2)

{
/*
LM_INFO ("???msgq_redirect ()");
*/
return 1;
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
*   Arg (3) = call index
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_prov_rsp (sip_msg_t *pmsg, str *ptotag, int ncall_idx)

{
/**********
* o send ringing response with require
* o update record
**********/

char *pfncname = "send_prov_rsp: ";
tm_api_t *ptm = pmod_data->ptm;
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
struct cell *ptrans = ptm->t_gett ();
pcall->call_rseq = rand ();
char phdrtmp [200];
char phdrtmplt [] =
  "Accept-Language: en" SIPEOL
  "Allow-Events: conference,talk,hold" SIPEOL
  "Require: 100rel" SIPEOL
  "RSeq: %d" SIPEOL
  "User-Agent: " USRAGNT SIPEOL
  ;
sprintf (phdrtmp, phdrtmplt, pcall->call_rseq);
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
pcall->call_state = CLSTA_PRING;
update_call_rec (pconn, ncall_idx);

/**********
* wait until PRACK acknowledged
**********/

while (1)
  {
  usleep (200);
  if (pcall->call_state != CLSTA_PRING)
    { break; }
//need to resend RING if no response???
  }
if (pcall->call_state == CLSTA_PRINGACK)
  {
  pcall->call_state = CLSTA_RING;
  update_call_rec (pconn, ncall_idx);
  }
return 1;
}

/**********
* Send RTPProxy Answer
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = transaction pointer
*   Arg (3) = totag pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_rtp_answer (sip_msg_t *pmsg, struct cell *ptrans, str *ptotag)

{
/**********
* build response from request
**********/

char *pfncname = "send_rtp_answer: ";
tm_api_t *ptm = pmod_data->ptm;
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
memcpy (&pnmsg->rcv, &pmsg->rcv, sizeof (struct receive_info));

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
nrc = 1;

/**********
* free buffer and return
**********/

answer_done:
if (pbuf->s)
  { pkg_free (pbuf->s); }
return nrc;
}