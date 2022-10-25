/*
 *  Copyright (C) 2000-2009  Henk de Groot - PE1DNN
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 */

#ifndef _CALL_H_
#define _CALL_H_

/* prototypes */
void  add_owner(void);
short is_owner(char *);
char *digi_owner(void);
void  add_block(void);
short is_block(char *, short);
void  add_via_block(void);
short is_via_block(uidata_t*);
void  add_allow_to(void);
short is_allowed_to(char *, short);
void  add_allow_from(void);
short is_allowed_from(char *, short);
void  add_msg_block(void);
short is_msg_block(char *, short);

#endif /* _CALL_H_ */
