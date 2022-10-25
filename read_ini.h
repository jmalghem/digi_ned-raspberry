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

#ifndef _READ_INI_H_
#define _READ_INI_H_

/* global data */
extern short digi_heard_size;
extern short digi_heard_show;
extern short digi_keep_time;
extern short digi_short_keep_time;
extern short digi_message_keep_time;
extern short digi_enable_exit;
extern short digi_enable_ptt_command;
extern short digi_enable_out_command;
extern short digi_max_msg_hops;
extern short digi_kenwood_mode;
extern short digi_opentrac_enable;
extern char  digi_digi_call[CALL_LENGTH];
extern char  digi_digi_dest[CALL_LENGTH];

extern char *digi_data_prefix;
extern char *digi_ssid_ign_prefix;
extern char *digi_logfile;
extern char *digi_tnclogfile;
extern char *digi_message_file;

extern float digi_utc_offset;
extern short digi_ptt_data;

#ifdef _SATTRACKER_
extern short digi_sat_active;
extern short digi_altitude;
extern short digi_use_local;
extern short digi_in_range_interval;
extern short digi_out_of_range_interval;
extern short digi_track_duration;
extern char *digi_sat_file;
extern char *digi_tle_file;
extern short digi_sat_obj_format;
#endif /* _SATTRACKER_ */

extern short linecount;

/* prototypes */
void  process_ini_line(char *);
short read_ini(char *);

#endif /* _READ_INI_H_ */
