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

#ifndef _OUTPUT_H_
#define _OUTPUT_H_

void set_verbose(void);
void set_display_activity(void);
void set_display_dx(void);
void toggle_verbose(void);
void toggle_logging(void);
void toggle_tnclogging(void);
void toggle_display_activity(void);
void say(const char *,...);
void vsay(const char *,...);
void asay(const char *,...);
void dsay(const char *,...);

/* prototypes */
void tnc_log(uidata_t *, distance_t);
void dump_uidata_from(uidata_t *);
void dump_uidata_to(char *, uidata_t *);
void dump_uidata_preempt(uidata_t *);
void dump_raw(frame_t *);
void dump_rule(char *, short, digipeat_t *);
void dump_preempt(short, preempt_t *, preempt_keep_t *);
void display_help(void);
void display_key_help(void);
#ifdef _LINUX_
void display_help_root(void);
#endif
void display_separator(void);

#endif /* _OUTPUT_H_ */
