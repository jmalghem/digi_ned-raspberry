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
#include "mac_if.h"
#include "read_ini.h"
#include "send.h"
#include "keep.h"
#include "timer.h"
#include "output.h"
#include "wx.h"
#include "message.h"

/*
 * This file contains all code to interpret and execute 'send', 'beacon'
 * and 'wx' commands from the .ini file.
 *
 * Example:
 * send: 20 all DIGI_DEST,WIDE,TRACE6-6\r\n
 * or
 * beacon: 20 all DIGI_DEST,WIDE,TRACE6-6\r\n
 * or
 * wx: 20 all DIGI_DEST,WIDE,TRACE6-6\r\n
 */

/*
 * Linked list containing the 'send', 'beacon', 'wx' or 'automessage' command
 * data
 */
static send_t *digi_send_p = NULL;

/*
 * Add send command from the .ini file to the internal administration.
 *
 * send: 20 all DIGI_DEST,WIDE,WIDE,TRACE6-6\r\n
 *      ^
 * or
 * beacon: 20 all DIGI_DEST,WIDE,WIDE,TRACE6-6\r\n
 *        ^
 * or
 * wx: 20 all DIGI_DEST,WIDE,WIDE,TRACE6-6\r\n
 *    ^
 * or
 * automessage: 100 all\r\n
 *             ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * token is already handled. The tokenizer is now at the point in
 * the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_send(FILE *fd, send_type_t kind)
{
    send_t        send;
    send_t       *send_p;
    char         *tmp;
    char         *tmp_case;
    char         *path;
    char         *command_p;
    char          output[OUTPORTS_LENGTH];
    char          number[MAX_ITOA_LENGTH];
    char          line[MAX_LINE_LENGTH];
    short         port;
    signed short  time_to_next_tx;
    signed short  time_sec_now;
    time_t        now;
    struct tm    *tm_p;
    /*
     * Remember the kind of command
     */
    send.kind = kind;

    /*
     * convert kind to text for use in messages
     */
    switch(kind) {
    case BEACON:
            command_p = "beacon";
            break;
#ifdef _WX_
    case WX:
            command_p = "wx";
            break;
#endif /* _WX_ */
    case AUTOMESSAGE:
            command_p = "automessage";
            break;
    default:
            /* The only possibility left... */
            command_p = "send";
            break;
    }

    tmp = strtok(NULL," \t");

    if(tmp[0] == '@')
    {
        send.interval = atoi(&tmp[1]);
        send.absolute = 1;
    }
    else
    {
        send.interval = atoi(tmp);
        send.absolute = 0;
    }
    if(send.interval < 0)
    {
        say("Error in interval in %s: at line %d, number >= 0 expected\r\n", 
                 command_p, linecount);
        return;
    }

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        say("Missing to-port on %s: at line %d\r\n", command_p, linecount);
        return;
    }

    if(check_ports(tmp, "To-port", command_p, linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    strcpy(send.output, tmp);

    send.path = NULL;

    path = strtok(NULL, "\r\n");

    if(path != NULL)
    {
        tmp_case = path;
        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        send.path = strdup(path);

        if(send.path == NULL)
        {
            say("Out of memory on %s: at line %d\r\n", command_p, linecount);
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
                say("Argument (%s) to long on %s: at line %d\r\n",
                        tmp, command_p, linecount);

                free(send.path);
                return;
            }
            tmp = strtok(NULL, " ,\t\r\n");
        }
    }

    /*
     * convert output shortcuts to full-lists
     */
    /*
     * Assemble the output string
     */

    /* start with empty output string, add the digits we find */
    output[0] = '\0';

    port = find_first_port(send.output);
    if(port == -1)
    {
        say("To-port out of range (1-%d) on %s: at line %d\r\n",
                mac_ports(), command_p, linecount);
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
            say("To-port out of range (1-%d) on %s: at line %d\r\n",
                    mac_ports(), command_p, linecount);
            return;
        }
    }
    strcpy(send.output, output);

    if(fgets(&line[0], MAX_LINE_LENGTH - 1, fd) != NULL)
    {
        if((kind == SEND) || (kind == BEACON))
        {
            /* slash handling and file completion */
            send.info = digi_convert_file(&line[0]);
        }
        else
        {
            /* Not a path to a file, just truncate at '\r' or '\n' */
            tmp = strchr(&line[0], '\r');
            if(tmp != NULL) *tmp = '\0';

            tmp = strchr(&line[0], '\n');
            if(tmp != NULL) *tmp = '\0';

            /* Just copy the line completely for later use */
            send.info = strdup(&line[0]);
        }

        if(send.info == NULL)
        {
            if(send.path != NULL)
                free(send.path);
            say("Out of memory on %s: at line %d\r\n", command_p, linecount);
            return;
        }
        linecount++;
    }
    else
    {
        say("Early end-of-file on %s: at line %d\r\n", command_p, linecount);
        if(send.path != NULL)
            free(send.path);
        return;
    }

    send_p = (send_t*) malloc(sizeof(send_t));

    /*
     * if out of memory then ignore
     */
    if(send_p == NULL)
    {
        say("Out of memory while handling %s: line at line %d\r\n",
                command_p, linecount);
        return;
    }
    
    /* initialize the timer */

    /* Calculate time to go */
    if(send.absolute == 0)
    {
        /* Relative time */

        /* start with transmission after 15 seconds */
        time_to_next_tx = 15;
    }
    else
    {
        /* Absolute time */

        /* Convert time to seconds */
        time_to_next_tx = send.interval * TM_MIN;

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
    start_timer(&send.timer, time_to_next_tx);

    /*
     * Duplicate data
     */
    memcpy(send_p, &send, sizeof(send_t));

    /*
     * Add to sender-list
     */
    send_p->next = digi_send_p;
    digi_send_p = send_p;
}

/*
 * Transmit the data specified by the file which is specified 
 * in the send_t structure.
 */
static void transmit_send(send_t *send_p)
{
    uidata_t uidata;
    short    ndigi;
    char    *tmp;
    char    *tmp2;
    char    *path;
    char     line[MAX_LINE_LENGTH];
    FILE    *fd;
    short    n;

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

    ndigi=0;
    if(send_p->path != NULL)
    {
        /* make copy of digipeater path for strtok */
        path = strdup(send_p->path);
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

            /* add digipeater path */
            tmp = strtok(NULL, " ,\t\r\n");
            while(tmp != NULL)
            {
                /*
                 * if there are more that MAX_DIGI digi's we ignore
                 * the rest
                 */
                if(ndigi < MAX_DIGI)
                {
                    strcpy(uidata.digipeater[ndigi], tmp);
                    uidata.digi_flags[ndigi] = RR_FLAG;
                    ndigi++;
                }
                tmp = strtok(NULL, " ,\t\r\n");
            }
        }

        /* don't need "path" anymore, free memory */
        free(path);
    }
    uidata.ndigi = ndigi;

#ifdef _WX_
    if(send_p->kind != WX)
    {
#endif /* _WX_ */
        if(send_p->kind != AUTOMESSAGE)
        {

            /* SEND and BEACON data transmission */

            /* get the transmission data from file */
            fd = fopen(send_p->info, "rt");
            if(fd == NULL)
            {
                vsay("Can't open file \"%s\"\r\n", send_p->info);
                /* can't open file, nothing to transmit! */
                return;
            }

            /* Transmit each line in the file as beacon */
            while(fgets(line, MAX_LINE_LENGTH - 2, fd) != NULL)
            {
                /* remove away '\r' characters */
                tmp  = line;
                tmp2 = line;
                while(*tmp != '\0')
                {
                    if(*tmp != '\r')
                    {
                        *tmp2 = *tmp;
                        tmp2++;
                    }
                    tmp++;
                }
                *tmp2 = '\0';

                /* convert newline to cariage-return characters */
                tmp = line;
                while(*tmp != '\0')
                {
                    if(*tmp == '\n')
                    {
                        *tmp = '\r';
                    }
                    tmp++;
                }

                /* check if there is a '\r' at the end, if not then add it */
                n = strlen(line);
                if((n == 0) || (line[n - 1] != '\r'))
                {
                    /* add a <CR> at the end */
                    strcat(line,"\r");
                }

                if(strlen(line) == 1)
                {
                    /*
                     * Empty line, it doesn't make sense to transmit these
                     * Go around to fetch the next line (if any)
                     */
                    continue;
                }

                strcpy(uidata.data, line);
                uidata.size = strlen(line);

                /* avoid digipeating my own data */
                keep_old_data(&uidata);

                /* check size of what we are about to transmit, kenwood mode */
                if(uidata_kenwood_mode(&uidata) == 0)
                {
                    return; /* failure */
                }

                /* show what we will transmit */
                dump_uidata_to(send_p->output, &uidata);

                /* now do the transmission for each port */
                uidata.port = find_first_port(send_p->output);
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

            /* don't need file anymore */
            fclose(fd);
        }
        else
        {
            /* AUTOMESSAGE transmission */

            vsay("Auto message: %s\r\n", send_p->info);

            /* Data part not used, the command is supplied explicitly */
            strcpy(uidata.data, "\0");
            uidata.size = 0;

            /*
             * if verbose is on then display response that goes back to
             * the "originator". This is only done for the first port,
             * for subsequent ports the information is surpressed, its the
             * same anyway.
             *
             * 'n' is a counter counting the number of times the command
             * is executed.
             */
            n = 0;

            /* now do the transmission for each port */
            uidata.port = find_first_port(send_p->output);
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

                /*
                 * Increment counter
                 */
                n++;

                /* fake command comming from this port */
                auto_message(send_p->info, &uidata, n);

                /* get next port to minic the command from */
                uidata.port = find_next_port();
                if(uidata.port == -1)
                {
                    return;
                }
            }
        }
#ifdef _WX_
    }
    else
    {
        /* WX data transmission */

        /* format WX data */
        wx_format(line, send_p->info, MAX_LINE_LENGTH - 2);

        /* check if there is a '\r' at the end, if not then add it */
        n = strlen(line);
        if((n == 0) || (line[n - 1] != '\r'))
        {
            /* add a <CR> at the end */
            strcat(line,"\r");
        }

        strcpy(uidata.data, line);
        uidata.size = strlen(line);

        /* avoid digipeating my own data */
        keep_old_data(&uidata);

        /* check size of what we are about to transmit, kenwood mode */
        if(uidata_kenwood_mode(&uidata) == 0)
        {
            return; /* failure */
        }

        /* show what we will transmit */
        dump_uidata_to(send_p->output, &uidata);

        /* now do the transmission for each port */
        uidata.port = find_first_port(send_p->output);
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
#endif /* _WX_ */
}

/*
 * Send all beacons. Used for the "?aprs?" broadcast handling.
 * note that the timers are not restarted.
 */
void send_all(send_type_t kind)
{
    send_t     *send_p;

    /*
     * walk through the transmission list and start transmission.
     */
    send_p = digi_send_p;

    /*
     * While there are still send commands to consider
     */
    while(send_p != NULL)
    {
        /*
         * Only commands of the propper type shall be send
         */
        if(send_p->kind == kind)
        {
            /* it is time to send this beacon */
            transmit_send(send_p);

            /* separator between all transmissions, not after the last */
            if(send_p->next != NULL)
            {
                /* command handled, show separator */
                display_separator();
            }
        }

        /* advanve to next send command */
        send_p = send_p->next;
    }
}

/*
 * Do send command, see if there is any transmission to be done at this
 * time, if there is do the transmission and restart the timer for the
 * next time.
 */
void do_send()
{
    send_t       *send_p;
    signed short  time_to_next_tx;
    signed short  time_sec_now;
    time_t        now;
    struct tm    *tm_p;

    /*
     * walk through the transmission list and start transmission if
     * needed
     */
    send_p = digi_send_p;

    /*
     * While there are still send commands to consider
     */
    while(send_p != NULL)
    {
        if( (is_elapsed(&(send_p->timer)) != 0)
            &&
            ((send_p->interval != 0) || (send_p->absolute == 1))
          )
        {
            /* it is time to send */
            transmit_send(send_p);

            /* command handled, show separator */
            display_separator();

            /* Calculate time to go */
            if(send_p->absolute == 0)
            {
                /* Relative time */
                time_to_next_tx = send_p->interval * TM_MIN;
            }
            else
            {
                /* Absolute time */

                /* Convert time to seconds */
                time_to_next_tx = send_p->interval * TM_MIN;

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
            start_timer(&(send_p->timer), time_to_next_tx);
        }

        /* advanve to next send command */
        send_p = send_p->next;
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

void swap_digicall_digidest_send()
{
    send_t     *send_p;
    char        path[MAX_DATA_LENGTH];
    char        oldpath[MAX_DATA_LENGTH];
    char       *call;
    short       replaced;

    /*
     * walk through the send list, replace DIGI_CALL and DIGI_DEST
     */
    send_p = digi_send_p;
    while(send_p != NULL)
    {
        if(send_p->path != NULL)
        {
            replaced = 0;

            path[0] = '\0';

            /* make a copy, don't run tokenizer on original string */
            strcpy(oldpath, send_p->path);
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
                strcpy(send_p->path, path);
            }
        }

        send_p = send_p->next;
    }
}
