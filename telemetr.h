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
#ifdef _TELEMETRY_

#ifndef _TELEMETR_H_
#define _TELEMETR_H_

/* prototypes */
void  add_telemetry(void);
void  add_tele_info(void);
void  do_telemetry(void);
void  swap_digicall_digidest_telemetry(void);
short telemetry_sources(void);
void  telemetry_response(short, short, char *);

#endif /* _TELEMETR_H_ */

#endif /* _TELEMETRY_ */
