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
#ifndef _KISS_H_
#define _KISS_H_
#ifdef KISS

/* KISS constants */

#define FEND  ((char) 0xC0)
#define FESC  ((char) 0xDB)
#define TFEND ((char) 0xDC)
#define TFESC ((char) 0xDD)

/* prototypes */
void kiss_init(void);
void kiss_exit(void);

short kiss_avl(void);
short kiss_inp(frame_t *frame_p);
short kiss_out(frame_t *frame_p);

short kiss_param(char *param);

#endif /* KISS */
#endif /* _KISS_H_ */
