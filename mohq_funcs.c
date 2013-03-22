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

#include "mohq.h"
#include "mohq_db.h"
#include "mohq_funcs.h"

/**********
* definitions
**********/

#define SIPEOL  "\r\n"
#define USRAGNT "Kamailio Message Queue"
#define CLENHDR "Content-Length"
#define RTPMAPCNT (sizeof (prtpmap) / sizeof (rtpmap))

typedef struct
  {
  int ntype;
  char *pencode;
  } rtpmap;

/**********
* local constants
**********/

str p100rel [1] = {STR_STATIC_INIT ("100rel")};
str pbye [1] = {STR_STATIC_INIT ("BYE")};
str pinvite [1] = {STR_STATIC_INIT ("INVITE")};
str prefer [1] = {STR_STATIC_INIT ("REFER")};
str presp_noallow [1] = {STR_STATIC_INIT ("Method Not Allowed")};
str presp_ok [1] = {STR_STATIC_INIT ("OK")};
str presp_ring [1] = {STR_STATIC_INIT ("Ringing")};
str psipfrag [1] = {STR_STATIC_INIT ("message/sipfrag")};

rtpmap prtpmap [] =
  {
  {9, "G722/8000"},
  {0, "PCMU/8000"},
  {8, "PCMA/8000"},
  {18, "G729/8000"},
  {3, "GSM/8000"},
  {4, "G723/8000"},
  {15, "G728/8000"},
  {5, "DVI4/8000"},
  {7, "LPC/8000"},
  {12, "QCELP/8000"},
  {13, "CN/8000"},
  {16, "DVI4/11025"},
  {6, "DVI4/16000"},
  {17, "DVI4/22050"},
  {10, "L16/44100"},
  {11, "L16/44100"},
  {14, "MPA/90000"}
  };

char pbyemsg [] =
  {
  "Max-Forwards: 70" SIPEOL
  "Contact: <%s>" SIPEOL
  "User-Agent: " USRAGNT SIPEOL
  };

str pextrahdr [1] =
  {
  STR_STATIC_INIT (
  "Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, INFO, MESSAGE, SUBSCRIBE, NOTIFY, PRACK, UPDATE, REFER" SIPEOL
  "Supported: 100rel" SIPEOL
  "Accept-Language: en" SIPEOL
  "Content-Type: application/sdp" SIPEOL
  "User-Agent: " USRAGNT SIPEOL
  )
  };

char pinvitemsg [] =
  {
  "Max-Forwards: 70" SIPEOL
  "Contact: <%s>" SIPEOL
  "Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, INFO, MESSAGE, SUBSCRIBE, NOTIFY, PRACK, UPDATE, REFER" SIPEOL
  "Supported: 100rel" SIPEOL
  "User-Agent: " USRAGNT SIPEOL
  "Accept-Language: en" SIPEOL
  "Content-Type: application/sdp" SIPEOL
  };

char pinvitesdp [] =
  {
  "v=0" SIPEOL
  "o=- %d %d IN %s" SIPEOL
  "s=" USRAGNT SIPEOL
  "c=IN %s" SIPEOL
  "t=0 0" SIPEOL
  "a=send%s" SIPEOL
  "m=audio %d RTP/AVP "
  };

char prefermsg [] =
  {
  "Max-Forwards: 70" SIPEOL
  "Refer-To: <%s>" SIPEOL
  "Referred-By: <%.*s>" SIPEOL
  };

char prtpsdp [] =
  {
  "v=0" SIPEOL
  // IP address and audio port faked since they will be replaced
  "o=- 1 1 IN IP4 1.1.1.1" SIPEOL
  "s=" USRAGNT SIPEOL
  "c=IN IP4 1.1.1.1" SIPEOL
  "t=0 0" SIPEOL
  "a=sendrecv" SIPEOL
  "m=audio 1 RTP/AVP"
  };

/**********
* local function declarations
**********/

void delete_call (call_lst *);
int find_call (str *);
static void hold_cb (struct cell *, int, struct tmcb_params *);
static void invite_cb (struct cell *, int, struct tmcb_params *);
int refer_call (call_lst *);
static void refer_cb (struct cell *, int, struct tmcb_params *);
int send_prov_rsp (sip_msg_t *, call_lst *);
int send_rtp_answer (sip_msg_t *, call_lst *);
int search_hdr_ext (struct hdr_field *, str *);

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
struct cell *ptrans;
tm_api_t *ptm = pmod_data->ptm;
if (pcall->call_state != CLSTA_INVITED)
  { LM_ERR ("%sUnexpected ACK", pfncname); }
else
  {
  /**********
  * o release INVITE transaction
  * o save SDP address info
  * o put in queue
  **********/

  if (ptm->t_lookup_ident (&ptrans, pcall->call_hash, pcall->call_label) < 0)
    { LM_ERR ("%sINVITE transaction missing!", pfncname); }
  else
    {
    if (ptm->t_release (pcall->call_pmsg) < 0)
      { LM_ERR ("%sRelease transaction failed!", pfncname); }
    }
  pcall->call_hash = pcall->call_label = 0;
  sprintf (pcall->call_addr, "%s %s",
    pmsg->rcv.dst_ip.af == AF_INET ? "IP4" : "IP6",
    ip_addr2a (&pmsg->rcv.dst_ip));
  pcall->call_state = CLSTA_INQUEUE;
  update_call_rec (pcall);
  pcall->call_cseq = 1;
  }
return;
}

/**********
* BYE Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parms
* OUTPUT: none
**********/

static void bye_cb
  (struct cell *ptrans, int ntype, struct tmcb_params *pcbp)

{
/**********
* o error means must have hung after REFER
* o delete the call
**********/

call_lst *pcall = (call_lst *)*pcbp->param;
if (ntype == TMCB_ON_FAILURE)
  { LM_ERR ("Call did not respond to BYE"); }
else
  {
LM_INFO ("BYE reply=%d", pcbp->rpl->first_line.u.reply.statuscode);//???
  if (REPLY_CLASS (pcbp->rpl) != 2)
    { LM_ERR ("Call error on BYE"); }
  }
delete_call (pcall);
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

if (pmod_data->psl->freply (pmsg, 200, presp_ok) < 0)
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
* Change Hold
*
* INPUT:
*   Arg (1) = call pointer
*   Arg (2) = hold flag
* OUTPUT: 0 if failed
**********/

int change_hold (call_lst *pcall, int bhold)

{
/**********
* form INVITE header
* o find URI/tag in from
* o find target from contact or from header
* o calculate size
* o create buffer
**********/

char *pfncname = "change_hold: ";
tm_api_t *ptm = pmod_data->ptm;
int nret = 0;
struct to_body ptob [1];
parse_to (pcall->call_from, &pcall->call_from [strlen (pcall->call_from) + 1],
  ptob);
if (ptob->error != PARSE_OK)
  {
  // should never happen
  LM_ERR ("%sInvalid from URI (%s)!", pfncname, pcall->call_from);
  return 0;
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
    return 0;
    }
  if (pcontact->param_lst)
    { free_to_params (pcontact); }
  ptarget->s = pcontact->uri.s;
  ptarget->len = pcontact->uri.len;
  }
char *pquri = pcall->pmohq->mohq_uri;
int npos1 = sizeof (pinvitemsg) // INVITE template
  + strlen (pquri); // contact
dlg_t *pdlg = 0;
char *psdp = 0;
char *phdr = pkg_malloc (npos1);
if (!phdr)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  return 0;
  }
sprintf (phdr, pinvitemsg, pquri);
str phdrs [1];
phdrs->s = phdr;
phdrs->len = strlen (phdr);

/**********
* form INVITE body
* o calculate size
* o create buffer
**********/

npos1 = 1;
npos1 = sizeof (pinvitesdp) // INVITE template
  + 20 // session id + version
  + strlen (pcall->call_addr) * 2 // server IP address twice
  + 4  // send type
  + 5  // media port number
  + (npos1 * 40); // media types
psdp = pkg_malloc (npos1);
if (!psdp)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  goto hold_err;
  }
sprintf (psdp, pinvitesdp,
  time (0), time (0) + 1, // session id + version
  pcall->call_addr, pcall->call_addr, // server IP address
  bhold ? "only" : "recv", // hold type
  pcall->call_aport); // audio port
str pbody [1];
pbody->s = psdp;
pbody->len = strlen (psdp);
strcpy (&psdp [pbody->len], "8"); //???
pbody->len += 1;
strcpy (&psdp [pbody->len], SIPEOL);
pbody->len += 2;
strcpy (&psdp [pbody->len], "a=rtpmap:8 PCMA/8000" SIPEOL); //???
pbody->len += 22;

/**********
* create dialog
**********/

pdlg = (dlg_t *)pkg_malloc (sizeof (dlg_t));
if (!pdlg)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  goto hold_err;
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
pdlg->loc_uri.s = pquri;
pdlg->loc_uri.len = strlen (pquri);
pdlg->rem_uri.s = ptob->uri.s;
pdlg->rem_uri.len = ptob->uri.len;

/**********
* send INVITE request
**********/

uac_req_t puac [1];
set_uac_req (puac, pinvite, phdrs, pbody, pdlg,
  TMCB_LOCAL_COMPLETED | TMCB_ON_FAILURE, hold_cb, pcall);
pcall->call_state = bhold ? CLSTA_HLDSTRT : CLSTA_NHLDSTRT;
if (ptm->t_request_within (puac) < 0)
  { LM_ERR ("%sUnable to create HOLD request!", pfncname); }
else
  { nret = 1; }

/**********
* release memory
**********/

hold_err:
if (pdlg)
  { pkg_free (pdlg); }
if (psdp)
  { pkg_free (psdp); }
pkg_free (phdr);
return nret;
}

/**********
* Close the Call
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: none
**********/

void close_call (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* destroy proxy connection
**********/

char *pfncname = "close_call: ";
LM_INFO ("%s(%s)destoying rtpproxy", pfncname, pcall->call_from);//???
if (pmod_data->fn_rtp_destroy (pmsg, 0, 0) != 1)
  { LM_ERR ("%srtpproxy_destroy refused!", pfncname); }

/**********
* form BYE header
* o find URI/tag in from
* o find target from contact or from header
* o calculate size
* o create buffer
**********/

tm_api_t *ptm = pmod_data->ptm;
dlg_t *pdlg = 0;
char *phdr = 0;
int bsent = 0;
struct to_body ptob [1];
parse_to (pcall->call_from, &pcall->call_from [strlen (pcall->call_from) + 1],
  ptob);
if (ptob->error != PARSE_OK)
  {
  // should never happen
  LM_ERR ("%sInvalid from URI (%s)!", pfncname, pcall->call_from);
  goto bye_err;
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
    goto bye_err;
    }
  if (pcontact->param_lst)
    { free_to_params (pcontact); }
  ptarget->s = pcontact->uri.s;
  ptarget->len = pcontact->uri.len;
  }
char *pquri = pcall->pmohq->mohq_uri;
int npos1 = sizeof (pbyemsg) // BYE template
  + strlen (pquri); // contact
phdr = pkg_malloc (npos1);
if (!phdr)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  goto bye_err;
  }
sprintf (phdr, pbyemsg, pquri);
str phdrs [1];
phdrs->s = phdr;
phdrs->len = strlen (phdr);

/**********
* create dialog
**********/

pdlg = (dlg_t *)pkg_malloc (sizeof (dlg_t));
if (!pdlg)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  goto bye_err;
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
pdlg->loc_uri.s = pcall->pmohq->mohq_uri;
pdlg->loc_uri.len = strlen (pdlg->loc_uri.s);
pdlg->rem_uri.s = ptob->uri.s;
pdlg->rem_uri.len = ptob->uri.len;
pdlg->state = DLG_CONFIRMED;

/**********
* send BYE request
**********/

uac_req_t puac [1];
set_uac_req (puac, pbye, phdrs, 0, pdlg,
  TMCB_LOCAL_COMPLETED | TMCB_ON_FAILURE, bye_cb, pcall);
pcall->call_state = CLSTA_BYE;
if (ptm->t_request_within (puac) < 0)
  {
  LM_ERR ("%sUnable to create BYE request!", pfncname);
  goto bye_err;
  }
LM_INFO ("%sSent BYE request!", pfncname);//???
bsent = 1;

/**********
* o free memory
* o delete call
**********/

bye_err:
if (pdlg)
  { pkg_free (pdlg); }
if (phdr)
  { pkg_free (phdr); }
if (!bsent)
  { delete_call (pcall); }
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
* o lock calls
* o find inactive slot
**********/

char *pfncname = "create_call: ";
int ncall_idx = pmod_data->call_cnt;
if (!mohq_lock_set (pmod_data->pcall_lock, 1, 2000))
  {
  LM_ERR ("%sUnable to lock calls!", pfncname);
  return -1;
  }
for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
  {
  if (!pmod_data->pcall_lst [ncall_idx].call_active)
    { break; }
  }
if (ncall_idx == pmod_data->call_cnt)
  {
  mohq_lock_release (pmod_data->pcall_lock);
  LM_ERR ("%sNo call slots available!", pfncname);
  return -1;
  }

/**********
* o release call lock
* o add values to new entry
**********/

call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
pcall->call_active = 1;
mohq_lock_release (pmod_data->pcall_lock);
pcall->pmohq = &pmod_data->pmohq_lst [mohq_idx];
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
* o update DB
* o lock MOH queue
**********/

pcall->call_state = CLSTA_ENTER;
add_call_rec (ncall_idx);
mohq_lock_set (pmod_data->pmohq_lock, 0, 0);
LM_INFO ("%s(%s)Received INVITE", pfncname, pcall->call_from);//???
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
* o release MOH queue
**********/

LM_INFO ("(%s)Deleting call", pcall->call_from);//???
delete_call_rec (pcall);
pcall->call_active = 0;
mohq_lock_release (pmod_data->pmohq_lock);
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
/**********
* o lock calls
* o find call
**********/

int nidx;
str tmpstr;
char *pfncname = "find_call: ";
if (!mohq_lock_set (pmod_data->pcall_lock, 1, 2000))
  {
  LM_ERR ("%sUnable to lock calls!", pfncname);
  return -1;
  }
for (nidx = 0; nidx < pmod_data->call_cnt; nidx++)
  {
  if (!pmod_data->pcall_lst [nidx].call_active)
    { continue; }
  tmpstr.s = pmod_data->pcall_lst [nidx].call_id;
  tmpstr.len = strlen (tmpstr.s);
  if (STR_EQ (tmpstr, *pcallid))
    {
    mohq_lock_release (pmod_data->pcall_lock);
    return nidx;
    }
  }
mohq_lock_release (pmod_data->pcall_lock);
return -1;
}

/**********
* Find mohq_id From URI
*
* INPUT:
*   Arg (1) = SIP message pointer
* OUTPUT: queue index; -1 if unable to find
**********/

int find_mohq_id (sip_msg_t *pmsg)

{
/**********
* o find current RURI
* o strip off parms or headers
* o search MOH queue
**********/

str *pruri =
  pmsg->new_uri.s ? &pmsg->new_uri : &pmsg->first_line.u.request.uri;
int nidx;
str pstr [1];
pstr->s = pruri->s;
pstr->len = pruri->len;
for (nidx = 0; nidx < pruri->len; nidx++)
  {
  if (pstr->s [nidx] == ';' || pstr->s [nidx] == '?')
    {
    pstr->len = nidx;
    break;
    }
  }
mohq_lst *pqlst = pmod_data->pmohq_lst;
for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  {
  str pmohstr [1];
  pmohstr->s = pqlst [nidx].mohq_uri;
  pmohstr->len = strlen (pmohstr->s);
  if (STR_EQ (*pmohstr, *pstr))
    { return nidx; }
  }
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
if (!mohq_lock_set (pmod_data->pmohq_lock, 0, 500))
  {
  LM_ERR ("Unable to lock queues!");
  return -1;
  }
for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  {
  tmpstr.s = pmod_data->pmohq_lst [nidx].mohq_name;
  tmpstr.len = strlen (tmpstr.s);
  if (STR_EQ (tmpstr, *pqname))
    { break; }
  }
if (nidx == pmod_data->mohq_cnt)
  {
  LM_ERR ("Unable to find queue (%.*s)!", STR_FMT (pqname));
  nidx = -1;
  }
mohq_lock_release (pmod_data->pmohq_lock);
return nidx;
}

/**********
* Find Referred Call
*
* INPUT:
*   Arg (1) = referred-by value
* OUTPUT: call index; -1 if unable to find
**********/

int find_referred_call (str *pvalue)

{
/**********
* get URI
**********/

struct to_body pref [1];
parse_to (pvalue->s, &pvalue->s [pvalue->len + 1], pref);
if (pref->error != PARSE_OK)
  {
  // should never happen
  LM_ERR ("Invalid Referred-By URI (%.*s)!", STR_FMT (pvalue));
  return -1;
  }
if (pref->param_lst)
  { free_to_params (pref); }

/**********
* search calls for matching
**********/

int nidx;
str tmpstr;
struct to_body pfrom [1];
for (nidx = 0; nidx < pmod_data->call_cnt; nidx++)
  {
  if (!pmod_data->pcall_lst [nidx].call_active)
    { continue; }
  tmpstr.s = pmod_data->pcall_lst [nidx].call_from;
  tmpstr.len = strlen (tmpstr.s);
  parse_to (tmpstr.s, &tmpstr.s [tmpstr.len + 1], pfrom);
  if (pfrom->error != PARSE_OK)
    {
    // should never happen
    LM_ERR ("Invalid From URI (%.*s)!", STR_FMT (&tmpstr));
    continue;
    }
  if (pfrom->param_lst)
    { free_to_params (pfrom); }
  if (STR_EQ (pfrom->uri, pref->uri))
    { return nidx; }
  }
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
call_lst *pcall;
int ncall_idx = find_call (&pmsg->callid->body);
if (ncall_idx != -1)
  {
  //??? need to do something
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

str ptotag [1];
if (ptm->t_get_reply_totag (pmsg, ptotag) != 1)
  {
  LM_ERR ("%sUnable to create totag", pfncname);
  return;
  }
ncall_idx = create_call (mohq_idx, pmsg, ptotag);
if (ncall_idx == -1)
  { return; }

/**********
* o catch failures
* o send working response
* o add contact to reply
* o supports/requires PRACK? (RFC 3262 section 3)
* o exit if not ringing
**********/

pcall = &pmod_data->pcall_lst [ncall_idx];
pcall->call_cseq = 1;
if (ptm->register_tmcb (pmsg, 0, TMCB_ON_FAILURE | TMCB_DESTROY,
  invite_cb, pcall, 0) < 0)
  {
  LM_ERR ("unable to set callback");
  delete_call (pcall);
  return;
  }
if (ptm->t_reply (pmsg, 100, "Your call is important to us") < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE", pfncname);
  delete_call (pcall);
  return;
  }
str pcontact [1];
char *pcontacthdr = "Contact: <%s>" SIPEOL;
pcontact->s = pkg_malloc (strlen (pmod_data->pmohq_lst [mohq_idx].mohq_uri)
  + strlen (pcontacthdr));
if (!pcontact->s)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  delete_call (pcall);
  return;
  }
sprintf (pcontact->s, pcontacthdr, pmod_data->pmohq_lst [mohq_idx].mohq_uri);
pcontact->len = strlen (pcontact->s);
if (!add_lump_rpl2 (pmsg, pcontact->s, pcontact->len, LUMP_RPL_HDR))
  { LM_ERR ("%sUnable to add contact", pfncname); }
pkg_free (pcontact->s);
pcall->call_pmsg = pmsg;
struct cell *ptrans = ptm->t_gett ();
pcall->call_hash = ptrans->hash_index;
pcall->call_label = ptrans->label;
if (search_hdr_ext (pmsg->supported, p100rel)
  || search_hdr_ext (pmsg->require, p100rel))
  { send_prov_rsp (pmsg, pcall); }
else
  {
  if (ptm->t_reply (pmsg, 180, presp_ring->s) < 0)
    { LM_ERR ("%sUnable to reply to INVITE", pfncname); }
  else
    { pcall->call_state = CLSTA_RINGING; }
  }

/**********
* since t_reply_with_body takes the shortcut of unreferencing the
* transaction we have to create a SIP body send it through rtpproxy
* and then use the result to build the final response
**********/

send_rtp_answer (pmsg, pcall);
return;
}

/**********
* Form RTP SDP String
*
* INPUT:
*   Arg (1) = string pointer
*   Arg (2) = call pointer
* OUTPUT: 0 if failed
**********/

int form_rtp_SDP (str *pstr, call_lst *pcall)

{
/**********
* form file name
**********/

char *pfncname = "form_rtp_SDP: ";
char pfile [MOHDIRLEN + MOHFILELEN + 6];
strcpy (pfile, pcall->pmohq->mohq_mohdir);
int nflen = strlen (pfile);
pfile [nflen++] = '/';
strcpy (&pfile [nflen], pcall->pmohq->mohq_mohfile);
nflen += strlen (&pfile [nflen]);
pfile [nflen++] = '.';

/**********
* find available files based on RTP payload type
**********/

int nidx;
int pfound [RTPMAPCNT];
int nfound = 0;
int nsize = strlen (prtpsdp) + 2;
for (nidx = 0; nidx < RTPMAPCNT; nidx++)
  {
  /**********
  * o form file name based on payload type
  * o exists?
  * o save index and count chars
  **********/

  sprintf (&pfile [nflen], "%d", prtpmap [nidx].ntype);
  struct stat psb [1];
  if (lstat (pfile, psb))
    { continue; }
  pfound [nfound++] = nidx;
  nsize += strlen (prtpmap [nidx].pencode) // encode length
    + 19; // space, type number, "a=rtpmap:%d ", EOL
  }
if (!nfound)
  {
  LM_ERR ("%sUnable to find any MOH files for queue (%s)!", pfncname,
    pcall->pmohq->mohq_name);
  return 0;
  }

/**********
* o allocate memory
* o form SDP
**********/

pstr->s = pkg_malloc (nsize + 1);
if (!pstr->s)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  return 0;
  }
strcpy (pstr->s, prtpsdp);
nsize = strlen (pstr->s);
for (nidx = 0; nidx < nfound; nidx++)
  {
  /**********
  * add payload types to media description
  **********/

  sprintf (&pstr->s [nsize], " %d", prtpmap [pfound [nidx]].ntype);
  nsize += strlen (&pstr->s [nsize]);
  }
strcpy (&pstr->s [nsize], SIPEOL);
nsize += 2;
for (nidx = 0; nidx < nfound; nidx++)
  {
  /**********
  * add rtpmap attributes
  **********/

  sprintf (&pstr->s [nsize], "a=rtpmap:%d %s %s",
    prtpmap [pfound [nidx]].ntype, prtpmap [pfound [nidx]].pencode, SIPEOL);
  nsize += strlen (&pstr->s [nsize]);
  }
pstr->len = nsize;
return 1;
}

/**********
* Hold Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parms
* OUTPUT: none
**********/

static void
  hold_cb (struct cell *ptrans, int ntype, struct tmcb_params *pcbp)

{
char *pfncname = "hold_cb: ";
call_lst *pcall = (call_lst *)*pcbp->param;
switch (ntype)
  {
  case TMCB_ON_FAILURE:
    /**********
    * if hold off, return call to queue
    **********/

    LM_ERR ("%sHold failed!", pfncname);
    if (pcall->call_state == CLSTA_NHLDSTRT)
      {
      pcall->call_state = CLSTA_INQUEUE;
      update_call_rec (pcall);
      return;
      }
    break;
  case TMCB_LOCAL_COMPLETED:
    /**********
    * if hold off, return call to queue
    **********/

LM_INFO ("hold callback COMPLETE, reply=%d", pcbp->rpl->first_line.u.reply.statuscode);//???
    if (REPLY_CLASS (pcbp->rpl) != 2)
      {
      LM_ERR ("%sHold failed; status=%d!", pfncname,
        pcbp->rpl->first_line.u.reply.statuscode);
      }
    if (pcall->call_state == CLSTA_NHLDSTRT)
      {
      pcall->call_state = CLSTA_INQUEUE;
      update_call_rec (pcall);
      return;
      }
    break;
  }

/**********
* o send refer
* o take call off hold
**********/

if (refer_call (pcall))
  { return; }
LM_ERR ("%sUnable to refer call!", pfncname);
if (!change_hold (pcall, 0))
  {
  pcall->call_state = CLSTA_INQUEUE;
  update_call_rec (pcall);
  }
return;
}

/**********
* Invite Callback
*
* INPUT:
*   Arg (1) = cell pointer
*   Arg (2) = callback type
*   Arg (3) = callback parms
* OUTPUT: none
**********/

static void
  invite_cb (struct cell *ptrans, int ntype, struct tmcb_params *pcbp)

{
call_lst *pcall = (call_lst *)*pcbp->param;
switch (pcall->call_state)
  {
  case CLSTA_PRACKSTRT:
    LM_ERR ("No provisional response received!");
    pcall->call_state = CLSTA_ERR;
    break;
  case CLSTA_INVITED:
    LM_ERR ("INVITE failed");
    delete_call (pcall);
    break;
  }
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
* o add CRLF so parser can go beyond first line
**********/

if (!search_hdr_ext (pmsg->content_type, psipfrag))
  {
  LM_ERR ("%sNot a sipfrag type", pfncname);
  return;
  }
char *pfrag = get_body (pmsg);
if (!pfrag)
  {
  LM_ERR ("%ssipfrag body missing", pfncname);
  return;
  }
str pbody [1];
pbody->len = pmsg->len - (int)(pfrag - pmsg->buf);
pbody->s = pkg_malloc (pbody->len + 2);
if (!pbody->s)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  return;
  }
strncpy (pbody->s, pfrag, pbody->len);
if (pbody->s [pbody->len - 1] != '\n')
  {
  strncpy (&pbody->s [pbody->len], SIPEOL, 2);
  pbody->len += 2;
  }
struct msg_start pstart [1];
parse_first_line (pbody->s, pbody->len + 1, pstart);
pkg_free (pbody->s);
if (pstart->type != SIP_REPLY)
  {
  LM_ERR ("%sreply missing", pfncname);
  return;
  }

/**********
* o send OK
* o REFER done?
**********/

if (pmod_data->psl->freply (pmsg, 200, presp_ok) < 0)
  {
  LM_ERR ("%sUnable to create reply", pfncname);
  return;
  }
LM_INFO ("%sReply=%d", pfncname, pstart->u.reply.statuscode);
switch (pstart->u.reply.statuscode / 100)
  {
  case 1:
    break;
  case 2:
    close_call (pmsg, pcall);
    break;
  default:
    LM_ERR ("%sUnable to redirect call", pfncname);
    if (!change_hold (pcall, 0))
      {
      pcall->call_state = CLSTA_INQUEUE;
      update_call_rec (pcall);
      }
    break;
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
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
if (pcall->call_state != CLSTA_PRACKSTRT)
  {
  LM_ERR ("%sUnexpected PRACK", pfncname);
  return;
  }

/**********
* check RAck???
**********/

/**********
* accept PRACK
**********/

if (ptm->t_newtran (pmsg) < 0)
  {
  LM_ERR ("%sUnable to create new transaction", pfncname);
  pcall->call_state = CLSTA_ERR;
  return;
  }
if (ptm->t_reply (pmsg, 200, "OK") < 0)
  {
  LM_ERR ("%sUnable to reply to PRACK", pfncname);
  pcall->call_state = CLSTA_ERR;
  return;
  }
pcall->call_state = CLSTA_PRACKRPLY;
return;
}

/**********
* Refer Call
*
* INPUT:
*   Arg (1) = call pointer
* OUTPUT: 0 if failed
**********/

int refer_call (call_lst *pcall)

{
/**********
* form REFER message
* o find URI/tag in from
* o find target from contact or from header
* o calculate basic size
* o add Via size
* o create buffer
**********/

char *pfncname = "refer_call: ";
struct to_body ptob [1];
parse_to (pcall->call_from, &pcall->call_from [strlen (pcall->call_from) + 1],
  ptob);
if (ptob->error != PARSE_OK)
  {
  // should never happen
  LM_ERR ("%sInvalid from URI (%s)!", pfncname, pcall->call_from);
  return 0;
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
    return 0;
    }
  if (pcontact->param_lst)
    { free_to_params (pcontact); }
  ptarget->s = pcontact->uri.s;
  ptarget->len = pcontact->uri.len;
  }
str puri [1];
puri->s = pcall->call_referto;
puri->len = strlen (puri->s);
int npos1 = sizeof (prefermsg) // REFER template
  + puri->len // Refer-To
  + ptob->uri.len; // Referred-By
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
dlg_t *pdlg = 0;
char *pbuf = pkg_malloc (npos1);
if (!pbuf)
  {
  LM_ERR ("%sNo more memory!", pfncname);
  return 0;
  }
sprintf (pbuf, prefermsg,
  puri->s, // Refer-To
  STR_FMT (&ptob->uri)); // Referred-By

/**********
* create dialog
**********/

int nret = 0;
pdlg = (dlg_t *)pkg_malloc (sizeof (dlg_t));
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
pdlg->loc_uri.s = pcall->pmohq->mohq_uri;
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
  TMCB_LOCAL_COMPLETED | TMCB_ON_FAILURE, refer_cb, pcall);
pcall->call_state = CLSTA_REFER;
update_call_rec (pcall);
if (ptm->t_request_within (puac) < 0)
  {
  pcall->call_state = CLSTA_INQUEUE;
  LM_ERR ("%sUnable to create REFER request!", pfncname);
  update_call_rec (pcall);
  goto refererr;
  }
LM_INFO ("%s(%s)Sent REFER request!", pfncname, pcall->call_from);//???
nret = -1;

refererr:
if (pdlg)
  { pkg_free (pdlg); }
pkg_free (pbuf);
return nret;
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
call_lst *pcall = (call_lst *)*pcbp->param;
if (ntype == TMCB_ON_FAILURE)
  {
  pcall->call_state = CLSTA_INQUEUE;
  LM_ERR ("REFER failed!");
  update_call_rec (pcall);
  return;
  }
LM_INFO ("Referral reply=%d", pcbp->rpl->first_line.u.reply.statuscode);
if (REPLY_CLASS (pcbp->rpl) == 2)
  { pcall->call_state = CLSTA_RFRWAIT; }
else
  {
  pcall->call_state = CLSTA_INQUEUE;
  update_call_rec (pcall);
  }
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
*   Arg (2) = call pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_prov_rsp (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* o send ringing response with require
* o update record
**********/

char *pfncname = "send_prov_rsp: ";
tm_api_t *ptm = pmod_data->ptm;
pcall->call_cseq = rand ();
char phdrtmp [200];
char *phdrtmplt =
  "Accept-Language: en" SIPEOL
  "Allow-Events: conference,talk,hold" SIPEOL
  "Require: 100rel" SIPEOL
  "RSeq: %d" SIPEOL
  "User-Agent: " USRAGNT SIPEOL
  ;
sprintf (phdrtmp, phdrtmplt, pcall->call_cseq);
struct lump_rpl **phdrlump = add_lump_rpl2 (pmsg, phdrtmp,
  strlen (phdrtmp), LUMP_RPL_HDR);
if (!phdrlump)
  {
  LM_ERR ("%sUnable to reply with new header", pfncname);
  return 0;
  }
if (ptm->t_reply (pmsg, 180, presp_ring->s) < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE", pfncname);
  return 0;
  }
pcall->call_state = CLSTA_PRACKSTRT;

/**********
* o wait until PRACK
* o remove header lump
**********/

while (1)
  {
  usleep (USLEEP_LEN);
  if (pcall->call_state != CLSTA_PRACKSTRT)
    { break; }
  }
unlink_lump_rpl (pmsg, *phdrlump);
if (pcall->call_state != CLSTA_PRACKRPLY)
  { return 0; }
return 1;
}

/**********
* Send RTPProxy Answer
*
* INPUT:
*   Arg (1) = SIP message pointer
*   Arg (2) = call pointer
* OUTPUT: 0=unable to process; 1=processed
**********/

int send_rtp_answer (sip_msg_t *pmsg, call_lst *pcall)

{
/**********
* build response from request
**********/

char *pfncname = "send_rtp_answer: ";
tm_api_t *ptm = pmod_data->ptm;
struct cell *ptrans = ptm->t_gett ();
str ptotag [1];
ptotag->s = pcall->call_tag;
ptotag->len = strlen (pcall->call_tag);
str pbuf [1];
struct bookmark pBM [1];
pbuf->s = build_res_buf_from_sip_req (200, presp_ok, ptotag, ptrans->uas.request,
  (unsigned int *)&pbuf->len, pBM);
if (!pbuf->s || !pbuf->len)
  {
  LM_ERR ("%sUnable to build response", pfncname);
  return 0;
  }

/**********
* parse out first line and headers
**********/

char *pclenhdr = CLENHDR;
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
* recreate buffer with extra headers and SDP
* o form SDP
* o count hdrs, extra hdrs, content-length hdr, SDP
* o alloc new buffer
* o form new buffer
* o replace orig buffer
**********/

str pSDP [1] = {STR_NULL};
if (!form_rtp_SDP (pSDP, pcall))
  { goto answer_done; }
for (npos1 = npos2 = 0; npos2 < nhdrcnt; npos2++)
  { npos1 += pparse [npos2].len; }
char pbodylen [30];
sprintf (pbodylen, "%s: %d\r\n\r\n", pclenhdr, pSDP->len);
npos1 += pextrahdr->len + strlen (pbodylen) + pSDP->len + 1;
char *pnewbuf = pkg_malloc (npos1);
if (!pnewbuf)
  {
  LM_ERR ("%sNo more memory!", pfncname);
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
build_sip_msg_from_buf (pnmsg, pbuf->s, pbuf->len, 0);
memcpy (&pnmsg->rcv, &pmsg->rcv, sizeof (struct receive_info));

/**********
* o send rtpproxy answer
* o form stream file
* o send stream
**********/

if (pmod_data->fn_rtp_answer (pnmsg, 0, 0) != 1)
  {
  LM_ERR ("%srtpproxy_answer refused", pfncname);
  goto answer_done;
  }
char pfile [MOHDIRLEN + MOHFILELEN + 2];
strcpy (pfile, pcall->pmohq->mohq_mohdir);
npos1 = strlen (pfile);
pfile [npos1++] = '/';
strcpy (&pfile [npos1], pcall->pmohq->mohq_mohfile);
npos1 += strlen (&pfile [npos1]);
str pMOH [1] = {{pfile, npos1}};
pv_elem_t *pmodel;
pv_parse_format (pMOH, &pmodel);
if (pmod_data->fn_rtp_stream2 (pnmsg, (char *)pmodel, (char *)-1) != 1)
  {
  LM_ERR ("%srtpproxy_stream2 refused", pfncname);
  goto answer_done;
  }

/**********
* o create buffer from response
* o find SDP
**********/

pbuf->s = build_res_buf_from_sip_res (pnmsg, (unsigned int *)&pbuf->len);
pkg_free (pnewbuf);
free_sip_msg (pnmsg);
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
* o save media port number
* o send adjusted reply
**********/

char *pfnd = strstr (pnewSDP->s, "m=audio ");
if (!pfnd)
  { LM_ERR ("%sUnable to find audio port!", pfncname); } // should not happen
else
  { pcall->call_aport = strtol (pfnd + 8, NULL, 10); }
if (!add_lump_rpl2 (pmsg, pextrahdr->s, pextrahdr->len, LUMP_RPL_HDR))
  {
  LM_ERR ("%sUnable to reply with new header", pfncname);
  goto answer_done;
  }
if (!add_lump_rpl2 (pmsg, pnewSDP->s, pnewSDP->len, LUMP_RPL_BODY))
  {
  LM_ERR ("%sUnable to reply with new body", pfncname);
  goto answer_done;
  }
if (ptm->t_reply (pmsg, 200, presp_ok->s) < 0)
  {
  LM_ERR ("%sUnable to reply to INVITE", pfncname);
  goto answer_done;
  }
pcall->call_state = CLSTA_INVITED;
return 1;

/**********
* free buffer and return
**********/

answer_done:
if (pSDP->s)
  { pkg_free (pSDP->s); }
pkg_free (pbuf->s);
return 0;
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
  LM_ERR ("No more memory!");
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
* o lock calls
* o count items in queue
**********/

int nq_idx = find_queue (pqname);
int ncount = 0;
call_lst *pcalls = pmod_data->pcall_lst;
int ncall_idx, mohq_id;
if (!mohq_lock_set (pmod_data->pcall_lock, 1, 200))
  { LM_ERR ("%sUnable to lock calls!", pfncname); }
else
  {
  if (nq_idx != -1)
    {
    mohq_id = pmod_data->pmohq_lst [nq_idx].mohq_id;
    for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
      {
      if (!pcalls [ncall_idx].call_active)
        { continue; }
      if (pcalls [ncall_idx].pmohq->mohq_id == mohq_id
        && pcalls [ncall_idx].call_state == CLSTA_INQUEUE)
        { ncount++; }
      }
    }
  mohq_lock_release (pmod_data->pcall_lock);
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
* o lock MOH queue
* o directed to message queue?
* o connect to database
**********/

if (parse_headers (pmsg, HDR_EOH_F, 0) < 0)
  {
  LM_ERR ("Unable to parse header!");
  return -1;
  }
if (!mohq_lock_set (pmod_data->pmohq_lock, 0, 2000))
  { return -1; }
int mohq_idx = find_mohq_id (pmsg);
db1_con_t *pconn = mohq_dbconnect ();
if (pconn)
  {
  /**********
  * o last update older than 1 minute?
  * o exclusively lock MOH queue
  * o update queue
  **********/

  if (pmod_data->mohq_update + 60 < time (0))
    {
    if (mohq_lock_change (pmod_data->pmohq_lock, 1))
      {
      update_mohq_lst (pconn);
      mohq_lock_change (pmod_data->pmohq_lock, 0);
      pmod_data->mohq_update = time (0);
      }
    }
  mohq_dbdisconnect (pconn);
  }
if (mohq_idx < 0)
  {
  mohq_lock_release (pmod_data->pmohq_lock);
  return -1;
  }

/**********
* o process message
* o release MOH queue
**********/

to_body_t *pto_body;
str smethod = REQ_LINE (pmsg).method;
LM_INFO ("???%.*s[%d]: [%d]%s", STR_FMT (&smethod), getpid (),
  mohq_idx, pmod_data->pmohq_lst [mohq_idx].mohq_uri);
switch (pmsg->REQ_METHOD)
  {
  case METHOD_INVITE:
    /**********
    * initial INVITE?
    **********/

    pto_body = get_to (pmsg);
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
  default:
    if (pmod_data->psl->freply (pmsg, 405, presp_noallow) < 0)
      { LM_ERR ("Unable to create reply to %.*s", STR_FMT (&smethod)); }
    break;
  }
mohq_lock_release (pmod_data->pmohq_lock);
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
if (puri->len > URI_LEN)
  {
  LM_ERR ("%sURI too long!", pfncname);
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
* o lock calls
* o find oldest call
**********/

int nq_idx = find_queue (pqname);
if (nq_idx == -1)
  { return -1; }
if (!mohq_lock_set (pmod_data->pcall_lock, 1, 200))
  {
  LM_ERR ("%sUnable to lock calls!", pfncname);
  return -1;
  }
call_lst *pcall = 0;// avoids complaint from compiler about uninitialized
int ncall_idx;
time_t ntime = 0;
int nfound = -1;
for (ncall_idx = 0; ncall_idx < pmod_data->call_cnt; ncall_idx++)
  {
  pcall = &pmod_data->pcall_lst [ncall_idx];
  if (!pcall->call_active || pcall->call_state != CLSTA_INQUEUE)
    { continue; }
  if (!ntime)
    { nfound = ncall_idx; }
  else
    {
    if (pcall->call_time < ntime)
      { nfound = ncall_idx; }
    }
  }
if (nfound == -1)
  {
  LM_WARN ("%sNo calls in queue (%.*s)", pfncname, STR_FMT (pqname));
  mohq_lock_release (pmod_data->pcall_lock);
  return -1;
  }
pcall = &pmod_data->pcall_lst [nfound];

/**********
* o save refer-to URI
* o put call on hold
* o send refer
* o take call off hold
**********/

strncpy (pcall->call_referto, puri->s, puri->len);
pcall->call_referto [puri->len] = '\0';
if (change_hold (pcall, 1))
  {
  mohq_lock_release (pmod_data->pcall_lock);
  return -1;
  }
mohq_lock_release (pmod_data->pcall_lock);
LM_ERR ("%sUnable to put call on hold!", pfncname);
if (refer_call (pcall))
  { return -1; }
LM_ERR ("%sUnable to refer call!", pfncname);
if (!change_hold (pcall, 0))
  {
  pcall->call_state = CLSTA_INQUEUE;
  update_call_rec (pcall);
  }
return -1;
}