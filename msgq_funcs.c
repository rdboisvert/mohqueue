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

#define PRXDLG_NEW    1
#define PRXDLG_RING   2
#define PRXDLG_PRACK  3

/**********
* local constants
**********/

const str MTHD_ACK = STR_STATIC_INIT ("ACK");
const str MTHD_BYE = STR_STATIC_INIT ("BYE");
const str MTHD_INVITE = STR_STATIC_INIT ("INVITE");
const str MTHD_PRACK = STR_STATIC_INIT ("PRACK");

str presp_ok [1] = {STR_STATIC_INIT ("OK")};
str presp_ring [1] = {STR_STATIC_INIT ("Ringing")};

/**********
* local function declarations
**********/

void invite_response_cb (struct cell *, int, struct tmcb_params *);
int send_rtp_answer (sip_msg_t *, struct cell *, str *);

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
* OUTPUT: 0=unable to process; 1=processed
**********/

int ack_msg (sip_msg_t *pmsg, int msgq_idx)

{
/**********
* o get transaction hash
* o SDP exists?
**********/

tm_api_t *ptm = pmod_data->ptm;
char *pfncname = "ack_msg: ";
unsigned int hash_index;
unsigned int hash_label;
/* ??? respond with valid error replies */
if (ptm->t_get_trans_ident (pmsg, &hash_index, &hash_label) < 0)
  {
  LM_ERR ("%sUnable to get transaction hash", pfncname);
  return (0);
  }
LM_INFO ("???%shash=%d, %d", pfncname, hash_index, hash_label);
if (!(pmsg->msg_flags & FL_SDP_BODY))
  {
  if (parse_sdp (pmsg))
    {
    LM_ERR ("%sACK lacks SDP", pfncname);
    return (0);
    }
  }

/**********
* rtpproxy_answer and UAC stream
**********/

if (pmod_data->fn_rtp_answer (pmsg, NULL, NULL) != 1)
  {
  LM_ERR ("%srtpproxy_answer refused", pfncname);
  return (0);
  }
if (pmod_data->fn_rtp_stream2uac (pmsg, "/var/build/music_on_hold", (char *)-1) != 1) /* ??? */
  {
  LM_ERR ("%srtpproxy_stream2uac refused", pfncname);
  return (0);
  }
return (1);
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
* o create new transaction
* o SDP exists?
* o send rtpproxy offer
**********/

char *pfncname = "first_invite_msg: ";
tm_api_t *ptm = pmod_data->ptm;
if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction", pfncname);
  return (1);
  }
if (!(pmsg->msg_flags & FL_SDP_BODY))
  {
  if (parse_sdp (pmsg))
    {
    LM_ERR ("%sINVITE lacks SDP", pfncname);
    if (ptm->t_reply (pmsg, 603, "INVITE lacks SDP") < 0)
      { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
    return (1);
    }
  }
if (pmod_data->fn_rtp_offer (pmsg, 0, 0) != 1)
  {
  LM_ERR ("%srtpproxy_offer refused", pfncname);
  if (ptm->t_reply (pmsg, 503, "Unable to proxy INVITE") < 0)
    { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
  return (1);
  }

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

#if 0 /* ??? */
if (pmod_data->prr->record_route (pmsg, 0)) /* ??? not working */
  {
  LM_ERR ("%sUnable to add record route", pfncname);
  return (0);
  }
#endif /* ??? */

/**********
* o send working response
* o init dialog
* o create totag, SDP and headers
**********/

struct cell *ptrans = ptm->t_gett ();
if (ptm->t_reply (pmsg, 100, "Your call is important to us") < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE", pfncname);
  return (1);
  }
pmod_data->pmsgq_lst [msgq_idx].dlg = PRXDLG_NEW;
LM_INFO ("%sdialog state=NEW", pfncname);//???
str ptotag [1] = {STR_NULL};
if (ptm->t_get_reply_totag (pmsg, ptotag) != 1)
  {
  LM_ERR ("%sUnable to create totag", pfncname);
  return (1);
  }
str pbody [1] = {STR_STATIC_INIT ("v=0" SIPEOL
"o=- 1167618058 1167618058 IN IP4 10.211.64.5" SIPEOL
"s=" USRAGNT SIPEOL
"c=IN IP4 10.211.64.12" SIPEOL
"t=0 0" SIPEOL
"a=sendrecv" SIPEOL
"m=audio 2230 RTP/AVP 9 127" SIPEOL
"a=rtpmap:9 G722/8000" SIPEOL
"a=rtpmap:127 telephone-event/8000" SIPEOL)};
str pnewhdr [1] = {STR_STATIC_INIT (
//"Allow: INVITE, ACK, BYE, CANCEL, MESSAGE, SUBSCRIBE, NOTIFY, REFER" SIPEOL
"Accept-Language: en" SIPEOL
"Allow-Events: conference,talk,hold" SIPEOL
//"Content-Type: application/sdp" SIPEOL
"Require: 100rel" SIPEOL
"RSeq: 8193" SIPEOL
"User-Agent: " USRAGNT SIPEOL)};

/**********
* o register callback to intercept reply
* o send ringing reply
* o update dialog state
**********/

#if 0 //???
if (ptm->register_tmcb (0, ptrans, TMCB_E2EACK_IN | TMCB_ON_FAILURE,
  invite_response_cb, 0, 0) < 0)
  {
  LM_ERR ("%sUnable to create callback", pfncname);
  return (1);
  }
#endif //???
pmod_data->pmsgq_lst [msgq_idx].hash_index = ptrans->hash_index;
pmod_data->pmsgq_lst [msgq_idx].hash_label = ptrans->label;
if (ptm->t_reply_with_body
  (ptrans, 180, presp_ring, pbody, pnewhdr, ptotag) < 0)
  {
  LM_ERR ("%sUnable to create reply", pfncname);
  return (1);
  }
pmod_data->pmsgq_lst [msgq_idx].dlg = PRXDLG_RING;
LM_INFO ("%sdialog state=RING", pfncname);//???

/**********
* wait until dialog finished
**********/

while (1)
  {
  sleep (1);
  if (pmod_data->pmsgq_lst [msgq_idx].dlg != PRXDLG_RING)
    { break; }
  LM_INFO ("%sStill waiting", pfncname);
  }
if (pmod_data->pmsgq_lst [msgq_idx].dlg != PRXDLG_PRACK)
  { LM_ERR ("%sConnection failed", pfncname); }
else
  {
  /**********
  * since t_reply_with_body takes the shortcut of unreferencing the
  * transaction we have to create a SIP body send it through rtpproxy
  * and then use the result to build the final response
  **********/

  if (!send_rtp_answer (pmsg, ptrans, ptotag))
    { LM_ERR ("%sUnable to final reply to INVITE", pfncname); }
  else
    { LM_INFO ("%sdialog state=CONFIRMED", pfncname); } //???
  }
pmod_data->pmsgq_lst [msgq_idx].hash_index = 0;
pmod_data->pmsgq_lst [msgq_idx].hash_label = 0;
pmod_data->pmsgq_lst [msgq_idx].dlg = 0;
return (1);
}

/**********
* Invite Response Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parameters
* OUTPUT: none
**********/

void invite_response_cb (struct cell *ptrans, int type, struct tmcb_params *ptparms)

{
/**********
* rtpproxy_answer and UAC stream
**********/

char *pfncname = "invite_response_cb: ";
char *ptype;
switch (type)
  {
  case TMCB_REQUEST_IN:
    ptype = "REQUEST_IN";
    break;
  case TMCB_RESPONSE_IN:
    ptype = "RESPONSE_IN";
    break;
  case TMCB_E2EACK_IN:
    ptype = "E2EACK_IN";
    break;
  case TMCB_REQUEST_PENDING:
    ptype = "REQUEST_PENDING";
    break;
  case TMCB_REQUEST_FWDED:
    ptype = "REQUEST_FWDED";
    break;
  case TMCB_RESPONSE_FWDED:
    ptype = "RESPONSE_FWDED";
    break;
  case TMCB_ON_FAILURE_RO:
    ptype = "ON_FAILURE_RO";
    break;
  case TMCB_ON_FAILURE:
    ptype = "ON_FAILURE";
    break;
  case TMCB_REQUEST_OUT:
    ptype = "REQUEST_OUT";
    break;
  case TMCB_RESPONSE_OUT:
    ptype = "RESPONSE_OUT";
    break;
  case TMCB_LOCAL_COMPLETED:
    ptype = "LOCAL_COMPLETED";
    break;
  case TMCB_LOCAL_RESPONSE_OUT:
    ptype = "LOCAL_RESPONSE_OUT";
    break;
  case TMCB_ACK_NEG_IN:
    ptype = "ACK_NEG_IN";
    break;
  case TMCB_REQ_RETR_IN:
    ptype = "REQ_RETR_IN";
    break;
  case TMCB_LOCAL_RESPONSE_IN:
    ptype = "LOCAL_RESPONSE_IN";
    break;
  case TMCB_LOCAL_REQUEST_IN:
    ptype = "LOCAL_REQUEST_IN";
    break;
  case TMCB_DLG:
    ptype = "DLG";
    break;
  case TMCB_DESTROY:
    ptype = "DESTROY";
    break;
  case TMCB_E2ECANCEL_IN:
    ptype = "E2ECANCEL_IN";
    break;
  case TMCB_E2EACK_RETR_IN:
    ptype = "E2EACK_RETR_IN";
    break;
  case TMCB_RESPONSE_READY:
    ptype = "RESPONSE_READY";
    break;
  case TMCB_REQUEST_SENT:
    ptype = "REQUEST_SENT";
    break;
  case TMCB_RESPONSE_SENT:
    ptype = "RESPONSE_SENT";
    break;
  default:
    ptype = "UNKNOWN";
  }
LM_INFO ("???%sresponse=%x, %s", pfncname, type, ptype);
#if 0 //???
sip_msg_t *pmsg = ptparms->rpl;
if (pmod_data->fn_rtp_answer (pmsg, NULL, NULL) != 1)
  {
  LM_ERR ("%srtpproxy_answer refused", pfncname);
  return;
  }
if (pmod_data->fn_rtp_stream2uac (pmsg, "/var/build/music_on_hold", (char *)-1) != 1) /* ??? */
  {
  LM_ERR ("%srtpproxy_stream2uac refused", pfncname);
  return;
  }
LM_INFO ("???rtp answer and stream2uac");
#endif //???
return;
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
* part of existing dialog?
**********/

char *pfncname = "prack_msg: ";
tm_api_t *ptm = pmod_data->ptm;
unsigned int hash_index = pmod_data->pmsgq_lst [msgq_idx].hash_index;
unsigned int hash_label = pmod_data->pmsgq_lst [msgq_idx].hash_label;
if (!hash_index && !hash_label)
  {
  LM_ERR ("%sNot part of existing transaction", pfncname);
  return (1);
  }

/**********
* o accept PRACK
* o update dialog state
**********/

if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction", pfncname);
  return (1);
  }
if (!ptm->t_reply (pmsg, 200, "OK"))
  {
  LM_ERR ("%sUnable to reply to PRACK", pfncname);
  return (1);
  }
pmod_data->pmsgq_lst [msgq_idx].dlg = PRXDLG_PRACK;
LM_INFO ("%sdialog state=PRACK OK", pfncname);//???
return (1);
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
if (STR_EQ (smethod, MTHD_INVITE))
  {
  /**********
  * initial INVITE?
  **********/

  if (!pto_body->tag_value.len)
    { return (first_invite_msg (pmsg, msgq_idx)); }
  return (reinvite_msg (pmsg, msgq_idx, &pto_body->tag_value));
  }
if (STR_EQ (smethod, MTHD_PRACK))
  { return (prack_msg (pmsg, msgq_idx)); }
if (STR_EQ (smethod, MTHD_ACK))
  { return (ack_msg (pmsg, msgq_idx)); }
if (STR_EQ (smethod, MTHD_BYE))
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
str pparse [20]; /* should be enough */
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
pbuf->s = build_res_buf_from_sip_res (pnmsg, &(unsinged int)pbuf->len);
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
return (nrc);
}