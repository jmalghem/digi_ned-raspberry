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

#ifndef _PREEMPT_H_
#define _PREEMPT_H_

/* prototypes */

/* functions for reading .ini file */
void  add_preempt(void);
void  add_preempt_keep(void);
void  add_preempt_never_keep(void);
short port_is_local(short);
void  swap_digicall_digidest_preempt(void);

/* preempting function */
void  do_preempt(uidata_t *);

#endif /* _PREEMPT_H_ */
