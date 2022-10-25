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

#ifndef _DX_H_
#define _DX_H_

/* global data */
extern char       digi_dx_times[];
extern char       digi_dx_metric[];
extern position_t  digi_pos;

/* prototypes */
long  distance_and_bearing(position_t *, short *);
short aprs2position(uidata_t *, position_t *);
void  find_best_dx(short, mheard_t **, mheard_t **, time_t, long);
void  add_digi_pos(void);
void  add_digi_pos_file(void);
void  add_dx_times(void);
void  add_dx_metric(void);
void  add_dx_level(void);
void  add_dx_path(void);
void  add_dx_portname(void);
short dx_ok(uidata_t *, long);
void  do_dx(uidata_t *, long, short);

#endif /* _DX_H_ */
