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

#ifndef _MHEARD_H_
#define _MHEARD_H_

/* prototypes */

/*
 * Implementation warning: first_mheard/next_mheard shall be used
 * without having an update the mheard list inbetween!!
 */
mheard_t *first_mheard(short);
mheard_t *next_mheard(short);
mheard_t *find_mheard(char *, short);

void remove_mheard_call(char *);
void remove_mheard_port(short);
void remove_mheard(void);

/* mheard function */
void do_mheard(uidata_t *);

#endif /* _MHEARD_H_ */
