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

#ifndef MSGQ_FUNCS_H
#define MSGQ_FUNCS_H

/**********
* module function declarations
**********/

int msgq_count (sip_msg_t *, char *, char *);
int msgq_count_fixup (void **, int);
int msgq_process (sip_msg_t *, char *, char *);
int msgq_redirect (sip_msg_t *, char *, char *);
int msgq_redirect_fixup (void **, int);

#endif /* MSGQ_FUNCS_H */