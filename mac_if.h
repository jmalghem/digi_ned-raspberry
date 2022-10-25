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

#ifndef _MAC_IF_H_
#define _MAC_IF_H_

/* global data */
extern unsigned short mac_vector;

/* prototypes */
short mac_ports(void);
short uidata_kenwood_mode(uidata_t *);
void  dump_uidata_from(uidata_t *);
void  dump_uidata_to(char * to, uidata_t *);
void  dump_raw(frame_t *);
short get_uidata(uidata_t *);
void  put_uidata(uidata_t *);
#ifdef _LINUX_
int   add_digi_port(char *port_p);
#endif
short mac_init(void);
void  mac_exit(void);
void  mac_flush(void);

#endif /* _MAC_IF_H_ */
