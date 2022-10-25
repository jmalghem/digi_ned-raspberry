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
#ifndef _AGWPE_H_
#define _AGWPE_H_
#ifdef AGWPE

#define AGWPE_HEADER_SZ       36
#define AGWPE_PORT             0
#define AGWPE_KIND             4
#define AGWPE_PID              6
#define AGWPE_FROMCALL         8
#define AGWPE_TOCALL          18
#define AGWPE_DATALEN         28
#define AGWPE_USER            32
#define AGWPE_LOGON_NAME      36
#define AGWPE_LOGON_PWD      291 
#define AGWPE_LOGON_SZ       546

#define AGWPE_RX_BUFFER      100

/* prototypes */
short agwpe_init(void);
void  agwpe_reinit(void);
void  agwpe_exit(void);

short agwpe_avl(void);
short agwpe_inp(frame_t *frame_p);
short agwpe_out(frame_t *frame_p);

short agwpe_ports(void);
short agwpe_param(char *param);

#endif /* AGWPE */
#endif /* _AGWPE_H_ */
