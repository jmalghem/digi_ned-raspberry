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

#ifndef _GENLIB_H_
#define _GENLIB_H_

/* 
 * Switch on debugging code
 *
 * (also switches on stdio, redirect to file to avoid double output!)
 */
/* #define debug */

/* prototypes */
short  set_progname_and_path(char *);
char  *get_progpath(void);
char  *get_progname(void);
short  match_call(const char *, const char *);
short  match_string(const char *, const char *);
short  check_ports(char *, const char *, const char *, short, porttype_t );
short  is_call(const char *);
short  number_of_hops(uidata_t *);
short  find_first_port(char *);
short  find_next_port(void);
char  *digi_convert_file(char *);
char  *tnc_string(uidata_t *, distance_t);
char  *digi_itoa(int, char *);

#endif /* _GENLIB_H_ */
