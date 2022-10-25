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

#ifndef _DIGIPEAT_H_
#define _DIGIPEAT_H_

/* prototypes */

/* functions for reading .ini file */
void  add_digipeat(void);
void  add_digiend(void);
void  add_digito(void);
void  add_digissid(void);
void  add_digifirst(void);
void  add_diginext(void);
void  add_local(void);
short port_is_local(short);
void  swap_digicall_digidest_digi(void);

/* digipeating function */
void  do_digipeat(uidata_t *);

#endif /* _DIGIPEAT_H_ */
