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

#ifndef MOHQ_FUNCS_H
#define MOHQ_FUNCS_H

/**********
* module function declarations
**********/

int mohq_count (sip_msg_t *, pv_elem_t *, char *);
int mohq_process (sip_msg_t *);
int mohq_redirect (sip_msg_t *, pv_elem_t *, pv_elem_t *);

#endif /* MOHQ_FUNCS_H */