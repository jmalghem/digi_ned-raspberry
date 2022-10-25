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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef _LINUX_
#include "linux.h"
#else
#include "digi_dos.h"
#endif /* _LINUX_ */
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "mac_if.h"
#include "read_ini.h"
#include "keep.h"
#include "timer.h"
#include "output.h"

/*
 * This file contains all code to interpret and execute 'telemetry'
 * commands from the .ini file.
 *
 * Example:
 * telemetry: 20 all lpt2_8/0 DIGI_DEST,WIDE,TRACE6-6\r\n
 */

/*
 * Data structure containing the 'telemetry' command data
 */
static telemetry_t *digi_telemetry_p = NULL;
static char        *digi_tele_info_p = NULL;

/*
 * Add telemetry command from the .ini file to the internal administration.
 *
 * telemetry: 20 all off,off,off,off,off,lpt2_8 DIGI_DEST,WIDE,TRACE6-6\r\n
 *           ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'telemetry' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_telemetry(void)
{
    telemetry_t  telemetry;
    telemetry_t  *telemetry_p;
    char         *tmp;
    char         *tmp_case;
    char         *save;
    char         *path;
    char         *addr_p;
    char          output[OUTPORTS_LENGTH];
    char          number[MAX_ITOA_LENGTH];
    short         i;
    short         port;
    signed short  time_to_next_tx;
    signed short  time_sec_now;
    time_t        now;
    struct tm    *tm_p;

    tmp = strtok(NULL," \t");

    if(tmp == NULL)
    {
        say("Interval missing in telemetry: at line %d\r\n", linecount);
        return;
    }

    if(tmp[0] == '@')
    {
        telemetry.interval = atoi(&tmp[1]);
        telemetry.absolute = 1;
    }
    else
    {
        telemetry.interval = atoi(tmp);
        telemetry.absolute = 0;
    }
    if(telemetry.interval < 0)
    {
        say("Error in interval in telemetry: at line %d, "
            "number >= 0 expected\r\n", linecount);
        return;
    }

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        say("Missing to-port on telemetry: at line %d\r\n", linecount);
        return;
    }

    if(check_ports(tmp, "To-port", "telemetry", linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    strcpy(telemetry.output, tmp);

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        say("Missing source port(s) on telemetry: at line %d\r\n", linecount);
        return;
    }

    /*
     * save the rest of the command so we can use strtok to retrieve
     * the source names
     */
    save = strtok(NULL, "\0");

    /*
     * Read the telemetry source ports
     */
    tmp = strtok(tmp,",");

    i = 0;

    while(tmp != NULL)
    {
        /*
         * Check if we don't overrun the number of sources  in the
         * command.
         */
        if(i == SOURCE_COUNT)
        {
            say("Too many sources in telemetry: at line %d, only %d "
                "sources allowed\r\n", linecount, SOURCE_COUNT);
            return;
        }

        addr_p = strchr(tmp, '/');
        if(addr_p != NULL)
        {
            /* Split source and address part */
            *addr_p = '\0';
            addr_p++;
        }

        /*
         * Check if the source name is "lpt1", "lpt2" or "lpt3"
         * or "lpt1_8", "lpt2_8" or "lpt3_8" or "off".
         */
        tmp_case = tmp;
        while(*tmp_case != '\0')
        {
            *tmp_case = tolower(*tmp_case);
            tmp_case++;
        }

        if( (strcmp(tmp, "lpt1") != 0)
            &&
            (strcmp(tmp, "lpt2") != 0)
            &&
            (strcmp(tmp, "lpt3") != 0)
            &&
            (strcmp(tmp, "lpt1_8") != 0)
            &&
            (strcmp(tmp, "lpt2_8") != 0)
            &&
            (strcmp(tmp, "lpt3_8") != 0)
            &&
            (strcmp(tmp, "off") != 0)
          )
        {
            say("Sourcename error in telemetry: at line %d, should be "
                "\"lpt(1..3[_8])\" or \"off\"\r\n", linecount);
            return;
        }

        strcpy(telemetry.source[i], tmp);

        if(addr_p != NULL)
        {
            /* Handle maximum portnumber */
            if( (strspn(addr_p,"01234567") != 1)
                ||
                (strlen(addr_p) != 1)
              )
            {
                say("Address error in telemetry: at line %d, only 0..7 "
                    "accepted, e.g. \"lpt2_8/4\"\r\n", linecount);
                return;
            }
            telemetry.address[i] = atoi(addr_p);
        }
        else
        {
            telemetry.address[i] = i;
        }

        /*
         * Advance to next source
         */
        tmp = strtok(NULL,",");

        /*
         * Increment counter which counts the number of sources.
         */
        i++;
    }

    /*
     * Store the number of sources.
     */
    telemetry.count = i;

    telemetry.path = NULL;

    path = strtok(save, "\r\n");

    if(path != NULL)
    {
        tmp_case = path;
        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        telemetry.path = strdup(path);

        if(telemetry.path == NULL)
        {
            say("Out of memory on telemetry: at line %d\r\n", linecount);
            return;
        }

        /*
         * Check for illegal digipeaters (too long)
         */
        tmp = strtok(path, " ,\t\r\n");
        while(tmp != NULL)
        {
            if(strlen(tmp) > 9)
            {
                say("Argument (%s) to long on telemetry: at line %d\r\n",
                        tmp, linecount);

                free(telemetry.path);
                return;
            }
            tmp = strtok(NULL, " ,\t\r\n");
        }
    }

    /*
     * convert output shortcuts to full-lists
     */

    /* start with empty output string, add the digits we find */
    output[0] = '\0';

    port = find_first_port(telemetry.output);
    if(port == -1)
    {
        say("To-port out of range (1-%d) on telemetry: at line %d\r\n",
                mac_ports(), linecount);
        return;
    }
    while(port != 0)
    {
        if(output[0] != '\0')
            strcat(output, ",");
        strcat(output, digi_itoa(port, number));

        port = find_next_port();
        if(port == -1)
        {
            say("To-port out of range (1-%d) on telemetry: at line %d\r\n",
                    mac_ports(), linecount);
            return;
        }
    }
    strcpy(telemetry.output, output);

    telemetry.sequence = 0;

    telemetry_p = (telemetry_t*) malloc(sizeof(telemetry_t));

    /*
     * if out of memory then ignore
     */
    if(telemetry_p == NULL)
    {
        say("Out of memory while handling telemetry: line at line %d\r\n",
                linecount);
        return;
    }
    
    /* initialize the timer */

    /* Calculate time to go */
    if(telemetry.absolute == 0)
    {
        /* Relative time */

        /* start with transmission after 15 seconds */
        time_to_next_tx = 15;
    }
    else
    {
        /* Absolute time */

        /* Convert time to seconds */
        time_to_next_tx = telemetry.interval * TM_MIN;

        /* Get current minute and second count */
        now = time(NULL);
        tm_p = gmtime(&now);
        time_sec_now = (signed short) (60 * tm_p->tm_min + tm_p->tm_sec);

        /* Calculate seconds to go */
        time_to_next_tx = 3600 + time_to_next_tx - time_sec_now;

        if(time_to_next_tx > 3600)
        {
            time_to_next_tx = time_to_next_tx - 3600;
        }
    }

    /* restart the timer */
    start_timer(&telemetry.timer, time_to_next_tx);

    /*
     * Duplicate data
     */
    memcpy(telemetry_p, &telemetry, sizeof(telemetry_t));

    /*
     * Add or replace telemetry data
     */
    if(digi_telemetry_p != NULL)
    {
        free(digi_telemetry_p);
    }
    digi_telemetry_p = telemetry_p;
}

/*
 * Add tele_info command from the .ini file to the internal administration.
 *
 * tele_info: <telemetry-beacon-file>
 *           ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'tele_info' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_tele_info(void)
{
    char *filename;

    filename = strtok(NULL," \t");

    if(filename == NULL)
    {
        say("Beacon filename missing in tele_info: at line %d\r\n", linecount);
        return;
    }

    /* slash handling and file completion */
    filename = digi_convert_file(filename);

    if(filename == NULL)
    {
        say("Out of memory while handling tele_info: line at line %d\r\n",
                linecount);
        return;
    }
    
    /*
     * Add or replace telemetry information file
     */
    if(digi_tele_info_p != NULL)
    {
        free(digi_tele_info_p);
    }

    digi_tele_info_p = filename;
}

/*
 * Get the telemetry data, read data from the specified source
 * Needs a zero based source number.
 *
 * Returns value from the telemetry port or '-1' if it could not
 * be read.
 */
static short get_telemetry(short source, telemetry_t *telemetry_p)
{
    short    value;

    if((source < 0) || (source >= telemetry_p->count))
    {
        /* Illegal port */
        return -1;
    }

    /* get the transmission data from the lpt port */
    if(strcmp(telemetry_p->source[source], "off") != 0)
    {
        /* not 'off' must be 'lptx' or 'lptx_8' then... */
        value = get_lpt_status(telemetry_p->source[source],
                               telemetry_p->address[source]);
    }
    else
    {
        /*
         * Dummy port, fill with 999 on analog ports, 255 for digital
         */
        if(source != 5)
        {
            value = 999;
        }
        else
        {
            value = 255;
        }
    }

    return value;
}

/*
* Transmit the telemetry data, read data from the specified sources
*/
static void transmit_telemetry(telemetry_t *telemetry_p)
{
    uidata_t uidata;
    short    ndigi;
    char    *tmp;
    char    *path;
    char     line[MAX_LINE_LENGTH];
    short    n;
    short    seq;
    short    data[SOURCE_COUNT];
    char     number[MAX_ITOA_LENGTH];
    short    value;
    short    count;

    /* prepare transmission */
    uidata.port = 0;

    /* transmit remotely */
    uidata.distance = REMOTE;

    /* primitive UI with no poll bit */
    uidata.primitive = 0x03;

    /* PID = 0xF0, normal AX25*/
    uidata.pid = 0xF0;

    /* fill in originator */
    strcpy(uidata.originator, digi_digi_call);
    uidata.orig_flags = (char) (RR_FLAG | C_FLAG);

    /* fill in destination */
    strcpy(uidata.destination, digi_digi_dest);
    uidata.dest_flags = RR_FLAG;

    /* make copy of digipeater path for strtok */
    ndigi=0;
    path = strdup(telemetry_p->path);
    if(path == NULL)
    {
        /* no memory for transmission */
        return;
    }

    /* overwrite destination if it is in the path (first argument) */
    tmp = strtok(path, " ,\t\r\n");
    if(tmp != NULL)
    {
        /* only need to set destination, dest_flags value was set above */
        strcpy(uidata.destination, tmp);
    }

    /* add digipeater path */
    tmp = strtok(NULL, " ,\t\r\n");
    while(tmp != NULL)
    {
        /* if there are more that MAX_DIGI digi's we ignore the rest */
        if(ndigi < MAX_DIGI)
        {
            strcpy(uidata.digipeater[ndigi], tmp);
            uidata.digi_flags[ndigi] = RR_FLAG;
            ndigi++;
        }
        tmp = strtok(NULL, " ,\t\r\n");
    }
    uidata.ndigi = ndigi;

    /* don't need "path" anymore, free memory */
    free(path);

    /* get the transmission data from the sources */
    for(count = 0; count < telemetry_p->count; count++)
    {
        value = get_telemetry(count, telemetry_p);

        if(value == -1)
        {
            say("Cannot read \"%s/%d\" to read telemetry data for "
                "transmission\r\n", telemetry_p->source[count],
                telemetry_p->address[count]);
            return;
        }
        data[count] = value;
    }

    /* read the sequence number to use */
    seq = telemetry_p->sequence;

    /*
     * Build telemetry line:
     *
     * T#<seqn>,A1,A2,A3,A4,A5,DDDDDDDD Comment
     */
    sprintf(line,"T#%03d,", seq);

    for(count = 0; count < telemetry_p->count; count++)
    {
        if(count != (SOURCE_COUNT - 1))
        {
            /* All sources except the last one are just values */
            sprintf(number,"%03d", data[count]);

            strcat(line, number);

            if((count + 1) < telemetry_p->count)
            {
                strcat(line, ",");
            }
        }
        else
        {
            /* Last source is digial, change value to bit pattern */
            value = data[count];

            /* Build Digital Value */
            for(n = 0; n < 8; n++)
            {
                if((value & 0x080) != 0)
                {
                    strcat(line, "1");
                }
                else
                {
                    strcat(line, "0");
                }
                value = value << 1;
            }
        }
    }

    /* add comment and line terminator */
    strcat(line, " From: ");
    for(count = 0; count < telemetry_p->count; count++)
    {
        strcat(line, telemetry_p->source[count]);
        if((count + 1) < telemetry_p->count)
        {
            strcat(line, ",");
        }
    }
    strcat(line, "\r");

    /* increment sequence number */
    seq = (seq + 1) % 1000;
    telemetry_p->sequence = seq;

    strcpy(uidata.data, line);
    uidata.size = strlen(uidata.data);

    /* avoid digipeating my own data */
    keep_old_data(&uidata);

    /* show what we will transmit */
    dump_uidata_to(telemetry_p->output, &uidata);

    /* now do the transmission for each port */
    uidata.port = find_first_port(telemetry_p->output);
    if(uidata.port == -1)
    {
        return;
    }
    while(uidata.port != 0)
    {
        /*
         * Make port numbers base start from 0 instead of 1
         */
        uidata.port--;

        /* transmit */
        put_uidata(&uidata);

        /* get next port to transmit on */
        uidata.port = find_next_port();
        if(uidata.port == -1)
        {
            return;
        }
    }
}

/*
 * Do telemetry command, see if there is any transmission to be done at
 * this time, if there is, do the transmission and restart the timer for
 * the next time.
 */
void do_telemetry(void)
{
    signed short  time_to_next_tx;
    signed short  time_sec_now;
    telemetry_t  *telemetry_p;
    time_t        now;
    struct tm    *tm_p;

    /*
     * walk through the transmission list and start transmission if
     * needed
     */
    telemetry_p = digi_telemetry_p;

    /*
     * Transmit telemetry if it is specified and it is time for transmission
     */

    if(telemetry_p != NULL)
    {
        if( (is_elapsed(&(telemetry_p->timer)) != 0)
            &&
            ((telemetry_p->interval != 0) || (telemetry_p->absolute == 1))
          )
        {
            /* it is time to send telemetry */
            transmit_telemetry(telemetry_p);

            /* command handled, show separator */
            display_separator();

            /* Calculate time to go */
            if(telemetry_p->absolute == 0)
            {
                /* Relative time */
                time_to_next_tx = telemetry_p->interval * TM_MIN;
            }
            else
            {
                /* Absolute time */

                /* Convert time to seconds */
                time_to_next_tx = telemetry_p->interval * TM_MIN;

                /* Get current minute and second count */
                now = time(NULL);
                tm_p = gmtime(&now);
                time_sec_now = (signed short) (60 * tm_p->tm_min +
                                tm_p->tm_sec);

                /* Calculate seconds to go */
                time_to_next_tx = 3600 + time_to_next_tx - time_sec_now;

                if(time_to_next_tx > 3600)
                {
                    time_to_next_tx = time_to_next_tx - 3600;
                }
            }

            /* restart the timer */
            start_timer(&(telemetry_p->timer), time_to_next_tx);
        }
    }
}

/*
 * Swap occurences of DIGI_CALL and DIGI_DEST with the calls
 * specified in digi_digi_call and digi_digi_dest.
 *
 * This is done with a seperate routine since at the time the rules
 * were put in the list structure "digi_digi_call" and "digi_digi_dest"
 * did not need to have a defined value. When we call this routine
 * this is the case and the calls have been checked (on size).
 */
void swap_digicall_digidest_telemetry(void)
{
    telemetry_t *telemetry_p;
    char         path[MAX_DATA_LENGTH];
    char         oldpath[MAX_DATA_LENGTH];
    char        *call;
    short        replaced;

    /*
     * Look at the telemetry data, replace DIGI_CALL and DIGI_DEST
     */
    telemetry_p = digi_telemetry_p;
    if(telemetry_p != NULL)
    {
        if(telemetry_p->path != NULL)
        {
            replaced = 0;

            path[0] = '\0';
            /* make a copy, don't run tokenizer on original string */
            strcpy(oldpath, telemetry_p->path);
            call = strtok(oldpath, " ,\t\r\n");
            while(call != NULL)
            {
                if(path[0] != '\0')
                    strcat(path, ",");

                if(strcmp(call, "DIGI_CALL") == 0)
                {
                    strcat(path, digi_digi_call);
                    replaced = 1;
                }
                else if(strcmp(call, "DIGI_DEST") == 0)
                {
                    strcat(path, digi_digi_dest);
                    replaced = 1;
                }
                else
                {
                    strcat(path, call);
                }
                call = strtok(NULL, " ,\t\r\n");
            }

            if(replaced == 1)
            {
                /*
                 * !we can just overwrite since DIGI_CALL and DIGI_DEST
                 * have the worse-case length!! (PE1DNN-15 and DIGI_CALL
                 * have the same length!
                 */
                strcpy(telemetry_p->path, path);
            }
        }
    }
}

/*
 * Return the number of configured sources
 */
short telemetry_sources(void)
{
    if(digi_telemetry_p == NULL)
    {
        return 0;
    }
    else
    {
        return digi_telemetry_p->count;
    }
}

static void telemetry_response_analog(short source, char *parm, char *unit,
                                         char *eqns, char *message)
{
    short  i;
    char  *tmp;
    char  *tmp_square;
    char  *tmp_multiply;
    char  *tmp_constant;
    char   parm_in[8]; /* longest possible analog label is 7 chars */
    char   unit_in[8]; /* longest possible analog unit is 7 chars */
    double eqns_square;
    double eqns_multiply;
    double eqns_constant;
    double eqns_value;
    double d_value;
    short  value;

    /*
     * Specification:
     *
     * parm = None,None,None,None,None,Busy,Ack,PE,Sel,Err,No,No,No
     * unit = None,None,None,None,None,Bit,Bit,Bit,Bit,Bit,No,No,No
     * eqns = 0,1,0,0,1,0,0,1,0,0,1,0,0,1,0
     *
     * 'source' input has the range 1..5 where 1 if the first analog source
     */

    /*
     * Get the parameters for the requested analog port
     */
    tmp = strtok(parm,",");
    for(i = 1; i < source; i++)
    {
        if(tmp != NULL)
        {
            tmp = strtok(NULL,",");
        }
    }

    /* If this port has a name, keep it, otherwise just call it 'A<n>' */
    if(tmp != NULL)
    {
        /* copy at most 7 characters */
        strncpy(parm_in, tmp, 7);

        /* ensure a trailing '\0' */
        parm_in[7] = '\0';
    }
    else
    {
        sprintf(parm_in,"A%d", source);
    }

    /*
     * Get the unit of measurement for the requested analog port
     */
    tmp = strtok(unit,",");
    for(i = 1; i < source; i++)
    {
        if(tmp != NULL)
        {
            tmp = strtok(NULL,",");
        }
    }

    /* If this bit has a name, keep is, otherwise just call it 'units' */
    if(tmp != NULL)
    {
        /* copy at most 7 characters */
        strncpy(unit_in, tmp, 7);

        /* ensure a trailing '\0' */
        unit_in[7] = '\0';
    }
    else
    {
        strcpy(unit_in,"units");
    }

    /* Retrieve equation values for the analog value */
    tmp_square = strtok(eqns,",");
    if(tmp_square != NULL)
    {
        tmp_multiply = strtok(NULL,",");
        if(tmp_multiply != NULL)
        {
            tmp_constant = strtok(NULL,",");
        }
        else
        {
            tmp_constant = NULL;
        }
    }
    else
    {
        tmp_multiply = NULL;
        tmp_constant = NULL;
    }

    for(i = 1; i < source; i++)
    {
        tmp_square = strtok(NULL,",");
        if(tmp_square != NULL)
        {
            tmp_multiply = strtok(NULL,",");
            if(tmp_multiply != NULL)
            {
                tmp_constant = strtok(NULL,",");
            }
            else
            {
                tmp_constant = NULL;
            }
        }
        else
        {
            tmp_multiply = NULL;
            tmp_constant = NULL;
        }
    }

    if(tmp_square == NULL)
    {
        eqns_square = 0.0;
    }
    else
    {
        sscanf(tmp_square,"%lf",&eqns_square);
    }

    if(tmp_multiply == NULL)
    {
        eqns_multiply = 1.0;
    }
    else
    {
        sscanf(tmp_multiply,"%lf",&eqns_multiply);
    }

    if(tmp_constant == NULL)
    {
        eqns_constant = 0.0;
    }
    else
    {
        sscanf(tmp_constant,"%lf",&eqns_constant);
    }

    /* Read the analog port */
    value = get_telemetry(source - 1, digi_telemetry_p);
    if(value == -1)
    {
        sprintf(message, "Reading src \"%s\" failed",
            digi_telemetry_p->source[source - 1]);
        return;
    }

    asay("Telemetry value: %u read from source \"%s\"\r\n", value,
            digi_telemetry_p->source[source - 1]);

    /*
     * Calculate the result
     */
    d_value = (double) value;
    eqns_value = eqns_square * (d_value * d_value) +
                 eqns_multiply * d_value + eqns_constant;

    sprintf(message, "%s %g %s", parm_in, eqns_value, unit_in);
}

static void telemetry_response_digital(short bitnr, char *parm, char *unit,
                                       char *bits, char *message)
{
    short  i, j;
    char  *tmp;
    char   parm_bit[6]; /* longest possible digital label is 5 chars */
    char   unit_bit[6]; /* longest possible digital unit is 5 chars */
    short  bits_bit;
    short  value;

    /*
     * Specification of the supplied parameters:
     *
     * parm = None,None,None,None,None,Busy,Ack,PE,Sel,Err,No,No,No
     * unit = None,None,None,None,None,Bit,Bit,Bit,Bit,Bit,No,No,No
     * bits = 11111111,DIGI_NET Telemetry
     */

    /* read source 5, which is the digital port */
    value = get_telemetry(5, digi_telemetry_p);
    if(value == -1)
    {
        sprintf(message, "Reading src \"%s\" failed",
            digi_telemetry_p->source[5]);
        return;
    }

    asay("Telemetry value: 0x%02x read from source \"%s\"\r\n", value, 
            digi_telemetry_p->source[5]);

    /*
     * If bitnr is 0 the binary value is returned, the 'bits' value
     * is evaluated to detemine if a bit is active.
     */
    if(bitnr == 0)
    {
        /*
         * Return an overview of all bits
         *
         * Line numbr 1Line numbr 2Line numbr 3Line numb  <- THD7 screen
         * B1-B8 value 01010101
         */
        strcpy(message, "B1-B8 value ");

        for(i = 0; i < 8; i++)
        {
            /* build the string backwards */
            j = 7 - i;
            bits_bit = 0;

            if((bits == NULL) || (j >= strlen(bits)) || (bits[j] != '0'))
            {
                bits_bit = 1;
            }

            if((value & 0x01) == bits_bit)
            {
                    message[j+12] = '1';
            }
            else
            {
                    message[j+12] = '0';
            }
            value = value >> 1;
        }
        message[8+12] = '\0';
    }
    else
    {
        /*
         * 'bitnr' input has the range 1..8  where 1 is MSB and 8 is LSB
         *
         * Return the bit with the appropriate name and unit name
         */

        /* make bitnr a 0 based value */
        bitnr--;

        /*
         * Skip over the analog parm values, then get the parameters for
         * the requested bit.
         */
        tmp = strtok(parm,",");
        for(i = 0; i < 5; i++)
        {
            if(tmp != NULL)
            {
                tmp = strtok(NULL,",");
            }
        }
        /* Skip to the correct bit */
        for(i = 0; i < bitnr; i++)
        {
            if(tmp != NULL)
            {
                tmp = strtok(NULL,",");
            }
        }

        /* If this bit has a name, keep is, otherwise just call it 'B<n>' */
        if(tmp != NULL)
        {
            /* copy at most 5 characters */
            strncpy(parm_bit, tmp, 5);

            /* ensure a trailing '\0' */
            parm_bit[5] = '\0';
        }
        else
        {
            sprintf(parm_bit,"B%d", bitnr);
        }

        /*
         * Skip over the analog unit values, then get the unit name for
         * the requested bit.
         */
        tmp = strtok(unit,",");
        for(i = 0; i < 5; i++)
        {
            if(tmp != NULL)
            {
                tmp = strtok(NULL,",");
            }
        }
        /* Skip to the correct bit */
        for(i = 0; i < bitnr; i++)
        {
            if(tmp != NULL)
            {
                tmp = strtok(NULL,",");
            }
        }

        /* If this bit has a name, keep is, otherwise just call it 'on' */
        if(tmp != NULL)
        {
            /* copy at most 5 characters */
            strncpy(unit_bit, tmp, 5);

            /* ensure a trailing '\0' */
            unit_bit[5] = '\0';
        }
        else
        {
            strcpy(unit_bit,"on");
        }

        /* Retrieve active value for bit */
        bits_bit = 0;
        if((bits == NULL) || (bitnr >= strlen(bits)) || (bits[bitnr] != '0'))
        {
            bits_bit = 1;
        }

        /* on APRS bit B1 is the MSB, B8 the LSB */
        if(((value >> (7 - bitnr)) & 0x01) == bits_bit)
        {
            sprintf(message, "%s is %s", parm_bit, unit_bit);
        }
        else
        {
            sprintf(message, "%s not %s", parm_bit, unit_bit);
        }
    }
}

void  telemetry_response(short source, short bitnr, char *message)
{
    FILE    *fd;
    char     line[MAX_LINE_LENGTH];
    char    *p;
    char    *parm = NULL;
    char    *unit = NULL;
    char    *eqns = NULL;
    char    *bits = NULL;

    if(digi_tele_info_p == NULL)
    {
        /* No telemetry info file specified */
        strcpy(message, "No Telemetry info, no   beacon file specified");
        return;
    }

    /* get the transmission data from file */
    fd = fopen(digi_tele_info_p, "rt");
    if(fd == NULL)
    {
        vsay("Can't open file \"%s\"\r\n", digi_tele_info_p);
        /* can't open file, nothing to do! */
        strcpy(message, "No Telemetry info, can't open beacon file");
        return;
    }

    /* Retrieve all lines from the beacon text */
    while(fgets(line, MAX_LINE_LENGTH -2, fd) != NULL)
    {
        /*
         * Remove '\r' and '\n' characters, truncate at first occurance.
         */
        p = strchr(line, '\r');
        if(p != NULL) { *p = '\0'; };
        p = strchr(line, '\n');
        if(p != NULL) { *p = '\0'; };

        /*
         * Interpret this kind of beacons
         *
012345678901234567890123456789012345678901234567890123456789012345678901

:PE1DNN-2 :PARM.None,None,None,None,None,Busy,Ack,PE,Sel,Err,No,No,No
:PE1DNN-2 :UNIT.None,None,None,None,None,Bit,Bit,Bit,Bit,Bit,No,No,No
:PE1DNN-2 :EQNS.0,1,0,0,1,0,0,1,0,0,1,0,0,1,0
:PE1DNN-2 :BITS.11111111,DIGI_NET Telemetry
         */
        if(strlen(line) <= 17)
        {
            continue;
        }

        if( (line[0] != ':')
            ||
            (line[10] != ':')
          )
        {
            continue;
        }

        if(strncmp(&line[1], digi_digi_call, strlen(digi_digi_call)) != 0)
        {
            continue;
        }

        if(strncmp(&line[11], "PARM.", 5) == 0)
        {
            if(parm != NULL)
            {
                free(parm);
            }
            parm = strdup(&line[16]);
        }
        else if(strncmp(&line[11], "UNIT.", 5) == 0)
        {
            if(unit != NULL)
            {
                free(unit);
            }
            unit = strdup(&line[16]);
        }
        else if(strncmp(&line[11], "EQNS.", 5) == 0)
        {
            if(eqns != NULL)
            {
                free(eqns);
            }
            eqns = strdup(&line[16]);
        }
        else if(strncmp(&line[11], "BITS.", 5) == 0)
        {
            if(bits != NULL)
            {
                free(bits);
            }
            bits = strdup(&line[16]);
        }
    }

    /* don't need file anymore */
    fclose(fd);

    /* Digital values need different processing then analog values */
    if(source < 6)
    {
        telemetry_response_analog(source, parm, unit, eqns, message);
    }
    else
    {
        telemetry_response_digital(bitnr, parm, unit, bits, message);
    }

    /* Cleanup allocated memory */
    if(parm != NULL) { free(parm); }
    if(unit != NULL) { free(unit); }
    if(eqns != NULL) { free(eqns); }
    if(bits != NULL) { free(bits); }

    asay("Telemetry response: %s\r\n", message);

    /* Finished */
    return;
}

#endif /* _TELEMETRY_ */
