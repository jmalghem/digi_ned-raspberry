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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "read_ini.h"
#include "digipeat.h"
#include "send.h"
#include "call.h"
#include "output.h"
#include "message.h"
#include "preempt.h"
#include "dx.h"
#include "command.h"
#include "telemetr.h"
#include "serial.h"
#include "wx.h"

short        digi_heard_size;
short        digi_heard_show;
short        digi_keep_time;
short        digi_short_keep_time;
short        digi_message_keep_time;
short        digi_enable_exit;
short        digi_enable_ptt_command;
short        digi_enable_out_command;
short        digi_max_msg_hops;
short        digi_kenwood_mode;
short        digi_opentrac_enable;
char         digi_digi_call[CALL_LENGTH];
char         digi_digi_dest[CALL_LENGTH];
char        *digi_data_prefix;
char        *digi_ssid_ign_prefix;
char        *digi_logfile = "";    /* needs to be initialized at startup */
char        *digi_tnclogfile = ""; /* needs to be initialized at startup */
char        *digi_message_file;
float        digi_utc_offset;
short        digi_ptt_data;
#ifdef _SATTRACKER_
short        digi_sat_active;
short        digi_altitude;
short        digi_use_local;
short        digi_in_range_interval;
short        digi_out_of_range_interval;
short        digi_track_duration;
char        *digi_sat_file;
char        *digi_tle_file;
short        digi_sat_obj_format;
#endif /* _SATTRACKER_ */

short        linecount;
static short digi_version;
static FILE *ini_fd;

void process_ini_line(char *line)
{
    char *token;
    char *value;
    char **logfile_pp;
    char *p,*q;
    char *tmp_case;

    /* if line starts with ';' then it is a comment-line */
    if(line[0] == ';')
    {
        /* comment, ignore */
        return;
    }

    /* check if there is a colon after the first word */
    p = strchr(line, ':');
    if(p != NULL)
    {
        q = strchr(line,' ');

        if((q != NULL) && (p > q))
        {
            /* space occurs before the colon */
            p = NULL;
        }

        q = strchr(line,'\t');

        if((q != NULL) && (p > q))
        {
            /* tab occurs before the colon */
            p = NULL;
        }
    }

    /* 
     * if colon could not be found, or space or tab occured before the
     * colon then it is not a valid rule. do some checks.
     */
    if(p == NULL)
    {
        if(strspn(line," \t\r\n") == strlen(line))
        {
            /* empty line, ignore */
            return;
        }

        /* strip <cr> and <nl> */
        value = strtok(line,"\r\n");

        /* Junk */
        say("Can't understand \"%s\" at line %d\r\n", value, linecount);
        return;
    }

    /* get token */
    token = strtok(line," \t:\r\n");

    /* check if we got a token */
    if(token == NULL)
        return;

    /* convert to lowercase */
    tmp_case = token;
    while(*tmp_case != '\0')
    {
        *tmp_case = tolower(*tmp_case);
        tmp_case++;
    }

    if(strcmp(token, "send") == 0)
    {
        add_send(ini_fd, SEND);
        return;
    }

    if(strcmp(token, "beacon") == 0)
    {
        add_send(ini_fd, BEACON);
        return;
    }

    if(strcmp(token, "automessage") == 0)
    {
        add_send(ini_fd, AUTOMESSAGE);
        return;
    }

#ifdef _SERIAL_
    if(strcmp(token, "serial") == 0)
    {
        add_serial(ini_fd);
        return;
    }
#endif /* _SERIAL_ */

    if(strcmp(token, "digipeat") == 0)
    {
        add_digipeat();
        return;
    }

    if(strcmp(token, "digiend") == 0)
    {
        add_digiend();
        return;
    }

    if(strcmp(token, "digito") == 0)
    {
        add_digito();
        return;
    }

    if(strcmp(token, "digissid") == 0)
    {
        add_digissid();
        return;
    }

    if(strcmp(token, "digifirst") == 0)
    {
        add_digifirst();
        return;
    }

    if(strcmp(token, "diginext") == 0)
    {
        add_diginext();
        return;
    }

    if(strcmp(token, "local") == 0)
    {
        add_local();
        return;
    }

    if(strcmp(token, "block") == 0)
    {
        add_block();
        return;
    }

    if(strcmp(token, "via_block") == 0)
    {
        add_via_block();
        return;
    }

    if(strcmp(token, "allow_to") == 0)
    {
        add_allow_to();
        return;
    }

    if(strcmp(token, "allow_from") == 0)
    {
        add_allow_from();
        return;
    }

    if(strcmp(token, "msg_block") == 0)
    {
        add_msg_block();
        return;
    }

    if(strcmp(token, "digi_owner") == 0)
    {
        add_owner();
        return;
    }

    if(strcmp(token, "message_path") == 0)
    {
        add_message_path();
        return;
    }

    if(strcmp(token, "preempt") == 0)
    {
        add_preempt();
        return;
    }

    if(strcmp(token, "preempt_keep") == 0)
    {
        add_preempt_keep();
        return;
    }

    if(strcmp(token, "preempt_never_keep") == 0)
    {
        add_preempt_never_keep();
        return;
    }

    if(strcmp(token, "digi_pos") == 0)
    {
        add_digi_pos();
        return;
    }

    if(strcmp(token, "digi_pos_file") == 0)
    {
        add_digi_pos_file();
        return;
    }

    if(strcmp(token, "dx_times") == 0)
    {
        add_dx_times();
        return;
    }

    if(strcmp(token, "dx_metric") == 0)
    {
        add_dx_metric();
        return;
    }

    if(strcmp(token, "dx_level") == 0)
    {
        add_dx_level();
        return;
    }

    if(strcmp(token, "dx_path") == 0)
    {
        add_dx_path();
        return;
    }

    if(strcmp(token, "dx_portname") == 0)
    {
        add_dx_portname();
        return;
    }

#ifdef _TELEMETRY_
    if(strcmp(token, "telemetry") == 0)
    {
        add_telemetry();
        return;
    }

    if(strcmp(token, "tele_info") == 0)
    {
        add_tele_info();
        return;
    }
#endif /* _TELEMETRY_ */

#ifdef _WX_
    if(strcmp(token, "wx_var") == 0)
    {
        add_wx_var();
        return;
    }

    if(strcmp(token, "wx") == 0)
    {
        add_send(ini_fd, WX);
        return;
    }
#endif /* _WX_ */

    if(strcmp(token, "command") == 0)
    {
        add_ini_cmd();
        return;
    }

    value = strtok(NULL," \t\r\n");
    if(value == NULL)
    {
        /* if there is no data anymore, use an empty string */
        value = "";
    }

    if(strcmp(token, "size_heard_list") == 0)
    {
        digi_heard_size = atoi(value);
        return;
    }

    if(strcmp(token, "size_heard_show") == 0)
    {
        digi_heard_show = atoi(value);
        return;
    }

    if(strcmp(token, "keep_time") == 0)
    {
        digi_keep_time = atoi(value);
        return;
    }

    if(strcmp(token, "short_keep_time") == 0)
    {
        digi_short_keep_time = atoi(value);
        return;
    }

    if(strcmp(token, "message_keep_time") == 0)
    {
        digi_message_keep_time = atoi(value);
        return;
    }

    if(strcmp(token, "enable_exit") == 0)
    {
        digi_enable_exit = atoi(value);
        return;
    }

    if(strcmp(token, "enable_ptt_command") == 0)
    {
        digi_enable_ptt_command = atoi(value);
        return;
    }

    if(strcmp(token, "enable_out_command") == 0)
    {
        digi_enable_out_command = atoi(value);
        return;
    }

    if(strcmp(token, "max_msg_hops") == 0)
    {
        digi_max_msg_hops = atoi(value);
        return;
    }

    if(strcmp(token, "digi_utc_offset") == 0)
    {
        digi_utc_offset = atof(value);
        if (digi_utc_offset > 12.0
            ||
            digi_utc_offset < -12.0)
        {
            digi_utc_offset = 0.0;
        }

        return;
    }

#ifdef _SATTRACKER_
    if(strcmp(token, "satellite_file") == 0)
    {
        free(digi_sat_file);

        /* slash handling and file completion */
        digi_sat_file = digi_convert_file(value);

        return;
    }


    if(strcmp(token, "update_tle_file") == 0)
    {
        free(digi_tle_file);

        /* slash handling and file completion */
        digi_tle_file = digi_convert_file(value);

        return;
    }

    if(strcmp(token, "digi_altitude") == 0)
    {
        digi_altitude = atoi(value);
        /*
         * If you're at a level for which you need FAA clearance or
         * you live on a submarine, we assume you'd rather be
         * at sea level.
         */
        if (digi_altitude > 10000
            ||
            digi_altitude < -1000)
        {
            digi_altitude = 0;
        }

        return;
    }

    if(strcmp(token, "digi_use_local") == 0)
    {
        digi_use_local = atoi(value);
        if( (digi_use_local != 0)
            &&
            (digi_use_local != 1)
          )
        {
            digi_use_local = 0;
        }
        return;
    }

    if(strcmp(token, "track_duration") == 0)
    {
        digi_track_duration = atoi(value);
        if (digi_track_duration < 1)
        {
            digi_track_duration = 105;
        }
        return;
    }

    if(strcmp(token, "sat_in_range_interval") == 0)
    {
        digi_in_range_interval = atoi(value);
        if (digi_in_range_interval < 1)
        {
            digi_in_range_interval = 1;
        }
        return;
    }

    if(strcmp(token, "sat_out_of_range_interval") == 0)
    {
        digi_out_of_range_interval = atoi(value);
        if (digi_out_of_range_interval < 1)
        {
            digi_out_of_range_interval = 15;
        }
        return;
    }

    if(strcmp(token, "sat_obj_format") == 0)
    {
        digi_sat_obj_format = atoi(value);
        return;
    }
#endif /* _SATTRACKER_ */

    if(strcmp(token, "kenwood_mode") == 0)
    {
        digi_kenwood_mode = atoi(value);
        if( (digi_kenwood_mode != 1)
            &&
            (digi_kenwood_mode != 2)
          )
        {
            digi_kenwood_mode = 0; 
        }
        return;
    }

    if(strcmp(token, "opentrac_enable") == 0)
    {
        digi_opentrac_enable = atoi(value);
        if(digi_opentrac_enable != 1)
        {
            digi_opentrac_enable = 0; 
        }
        return;
    }

    if(strcmp(token, "version") == 0)
    {
        digi_version = atoi(value);
        return;
    }

    if(strcmp(token, "data_prefix") == 0)
    {
        free(digi_data_prefix);
        digi_data_prefix = strdup(value);
        return;
    }

    if(strcmp(token, "ssid_ignore_prefix") == 0)
    {
        free(digi_ssid_ign_prefix);
        digi_ssid_ign_prefix = strdup(value);
        return;
    }

    if((strcmp(token, "logfile") == 0) || (strcmp(token, "tnclogfile") == 0))
    {
        if(strcmp(token, "logfile") == 0)
        {
            logfile_pp = &digi_logfile;
        }
        else
        {
            logfile_pp = &digi_tnclogfile;
        }

        if(*logfile_pp != NULL)
        {
            free(*logfile_pp);
        }

        /* slash handling and file completion */
        *logfile_pp = digi_convert_file(value);

        return;
    }

    if(strcmp(token, "message_file") == 0)
    {
        free(digi_message_file);

        /* slash handling and file completion */
        digi_message_file = digi_convert_file(value);

        return;
    }

    /*
     * All what is left are tokens to specify source and destination calls
     */

    if(strlen(value) >= CALL_LENGTH)
    {
        say("Call (%s) not okay at token %s: at line %d\r\n", 
                value, token, linecount);
        return;
    }

    /* calls in uppercase */
    tmp_case = value;
    while(*tmp_case != '\0')
    {
        *tmp_case = toupper(*tmp_case);
        tmp_case++;
    }

    if(strcmp(token, "digi_call") == 0)
    {
        strcpy(digi_digi_call, value);
        return;
    }

    if(strcmp(token, "digi_dest") == 0)
    {
        strcpy(digi_digi_dest, value);
        return;
    }

    say("Unknown token: %s at line %d\r\n", token, linecount);
    return;
}

short read_ini(char *filename)
{
    char  line[256];

    vsay("Reading %s...\r\n",filename);

    digi_heard_size        = 4;   /* 4 calls = one message line response */
    digi_heard_show        = 32767; /* list is limited by digi_heard_size */
    digi_keep_time         = 300; /* 5 minutes */
    digi_short_keep_time   = 10;  /* 10 seconds */
    digi_message_keep_time = 900; /* 15 minutes */
    digi_enable_exit       = 0;   /* default: not enabled */
    digi_enable_ptt_command= 1;   /* default: enabled */
    digi_enable_out_command= 1;   /* default: enabled */
    digi_max_msg_hops      = 8;   /* maximum number of hops */
    digi_kenwood_mode      = 1;   /* truncate too long packets */
    digi_opentrac_enable   = 0;   /* digipeat opentrack packets */
    digi_version           = 0;   /* version of the .ini file */
    digi_digi_call[0]      = '\0';
    digi_digi_dest[0]      = '\0';
    digi_data_prefix       = strdup("");
    digi_ssid_ign_prefix   = strdup("");
    digi_logfile           = strdup("");
    digi_tnclogfile        = strdup("");
    digi_message_file      = strdup("");
    digi_utc_offset        = 0.0; /* default UTC offset */
    digi_ptt_data          = 0xffff;
#ifdef _SATTRACKER_
    digi_sat_active        = 0;   /* default not active */
    digi_altitude          = 0;   /* default altitude above sea level */
    digi_use_local         = 0;   /* use UTC as default */
    digi_track_duration    = 105; /* tracking lasts default for 1.5 hour at */
                                  /* request */
    digi_in_range_interval = 1;   /* send sat object once per minute when */
                                  /* in range */
    digi_out_of_range_interval = 15; /* send sat object once per 15 minutes */
                                     /* when satellite out of range */
    digi_sat_file          = strdup(""); /* satellite info file name */
    digi_tle_file          = strdup(""); /* satellite tle filename for upd. */
    digi_sat_obj_format    = 0;   /* simple satellite objects with E */
#endif /* _SATTRACKER_ */

    ini_fd = fopen(filename,"rt");
    if(ini_fd == NULL)
    {
        perror(filename);
        return 0;
    }
    linecount = 0;

    while(fgets(&line[0], 256, ini_fd) != NULL)
    {
        linecount++;
        process_ini_line(line);
    }

    fclose(ini_fd);

    if(digi_owner() == NULL)
    {
        say("Missing digi_owner, the callsign of the owner of the digi!\r\n");
        return 0;
    }

    if(digi_digi_call[0]  == '\0')
    {
        say("Missing digi_call, the callsign of this digi!\r\n");
        return 0;
    }

    if(digi_digi_dest[0]  == '\0')
    {
        say("Missing digi_dest, the callsign to which this digi "
               "should sends its beacons!\r\n");
        return 0;
    }

    if(digi_version != 2)
    {
        say("\r\nERROR: Missing or wrong version number in the .ini file.\r\n");
        say("The rules in the read .ini file may misbehave in this version ");
        say("of DIGI_NED!\r\nThis version expects version 2. The version ");
        say("number changes when incompatible\r\nchanges have been made to ");
        say("the interpretation of the rules from the .ini file!\r\n");
        return 0;
    }

#ifdef _SATTRACKER_
    /* See if there is enough informatio to use the satellite tracker */
    if( ((digi_pos.lon != 0L) || (digi_pos.lat != 0L))
        &&
        (digi_sat_file != NULL)
        &&
        (digi_sat_file[0] != '\0')
      )
    {
        /* enough information to use tracking */
        digi_sat_active = 1;
    }
#endif /* _SATTRACKER_ */

    swap_digicall_digidest_digi();
    swap_digicall_digidest_send();
    swap_digicall_digidest_preempt();

#ifdef _TELEMETRY_
    swap_digicall_digidest_telemetry();
#endif /* _TELEMETRY_ */

#ifdef _SERIAL_
    swap_digicall_digidest_serial();
#endif /* _SERIAL_ */

    vsay("%s Read, %d lines\r\n",filename, linecount);

    return 1;
}

