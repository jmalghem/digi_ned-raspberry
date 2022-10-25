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
#ifdef _SERIAL_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
#include "serial.h"
#include "keep.h"
#include "timer.h"
#include "output.h"

/*
 * This file contains all code to interpret and execute 'serial' commands
 * from the .ini file.
 *
 * Example:
 * serial: 10 all com1 4800 GPSODN,WIDE,WIDE3-3
 * $GPRMC $GPGGA
 */

/*
 * Linked list containing the 'serial' command data
 */
static digi_serial_t *digi_serial_p = NULL;

/*
 * Restore the serial lines to a safe setting
 */
void serial_exit(void)
{
    digi_serial_t *ds_p;

    ds_p = digi_serial_p;
    while(ds_p != NULL)
    {
        serial_close(ds_p->fd);

        ds_p->fd = -1;

        ds_p = ds_p->next;
    }
}

/*
 * Change text to pick up from the serial line
 */
static void serial_serialtext(char *type, digi_serial_t *ds_p) 
{
    digi_serialtext_t  serialtext;
    digi_serialtext_t *serialtext_p;

    if (strlen(type) > 6)
    {
        /* Can't change settings, serialtext too long */
        say("Illegal type in serial: at line %d\r\n", linecount);
        return;
    }

    strcpy(serialtext.type, type);
    serialtext.sentence[0] = '\0';

    /* create storarage space for this serialtext */
    serialtext_p = (digi_serialtext_t*) malloc(sizeof(digi_serialtext_t));                                      
    /* 
     * if out of memory then ignore
     */
    if(serialtext_p == NULL)
    {   
        say("Out of memory on serial: at line %d\r\n", linecount);
        return;
    }
    
    /* 
     * Duplicate data
     */
    memcpy(serialtext_p, &serialtext, sizeof(digi_serialtext_t));
    
    /* 
     * Add to serialtext-list in the digi_serial structure
     */
    serialtext_p->next = ds_p->serialtext_p;
    ds_p->serialtext_p = serialtext_p;

    return;
}


/*
 * Initialize the serial line to poll for data
 *
 * If is fails just silently ignore.
 */
static void serial_init(digi_serial_t *ds_p) 
{
    ds_p->fd = serial_open(ds_p->comnr, ds_p-> baudrate); 

    ds_p->active_in[0] = '\0';

    return;
}

/*
 * Read data from the serial line
 */
static void serial_read(digi_serial_t *ds_p)
{
    int                i, j, sum;
    char               sum_string[5];
    char               data[2];
    digi_serialtext_t *st_p;

    if(ds_p->fd == -1)
    {
        /*
         * Not initialized, do nothing
         */
        return;
    }

    /* Loop until all data from the serial line is read */
    while(1)
    {
        i = serial_in(ds_p->fd, &(data[0]));

        if(i == -1)
        {
            /* No data read */
            return;
        }

        if(data[0] == '\n')
        {
            /* Ignore LF */
            continue;
        }

        if((data[0] == '\r') || (data[0] == '\0'))
        {
            i = strlen(ds_p->active_in);
            if(i > 4)
            {
                /*
                 * Move back to the '*' character. If present then this string
                 * has a checksum. Verify it.
                 */
                i = i - 3;

                if(ds_p->active_in[i] == '*')
                {
                    sum = 0;
                    for(j = 1; j < i; j++)
                    {
                        sum = sum ^ ds_p->active_in[j];
                    }

                    sprintf(sum_string, "*%02X", sum);

                    if(strcmp(sum_string, &(ds_p->active_in[i])) != 0)
                    {
                        /* Checksum error */

                        /* Start with next line */
                        ds_p->active_in[0] = '\0';
                        continue;
                    }
                }
            }

            /*
             * The string is now complete. If there was a checksum it has been
             * verfied and was okay. Now check if it was one of the strings we
             * were expecting.
             */
            st_p = ds_p->serialtext_p;
            while(st_p != NULL)
            {
                if(strncmp(st_p->type, ds_p->active_in, strlen(st_p->type)) == 0)
                {
                    /* Yes, a complete line of the correct type read! */
                    strcpy(st_p->sentence, ds_p->active_in);
                }

                /* advance to the next serialtext we are awaiting */
                st_p = st_p->next;
            }

            /* Start with next line */
            ds_p->active_in[0] = '\0';
            return;
        }

        if(data[0] < ' ')
        {
            /* Change control characters to dots */
            data[0] = '.';
        }

        if(strlen(ds_p->active_in) < 255)
        {
            /* make data a string by appending a '\0' */
            data[1] = '\0';
            strcat(ds_p->active_in, data);
        }
    }
}

/*
 * Add serial command from the .ini file to the internal administration.
 *
 * serial: 10 all com1 4800 GPSODN,WIDE,WIDE3-3
 *        ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'serial' token is already handled. The tokenizer is now at the point in
 * the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_serial(FILE *fd)
{
    digi_serial_t  serial;
    digi_serial_t *serial_p;
    char          *tmp;
    char          *tmp_case;
    char          *path;
    char           output[OUTPORTS_LENGTH];
    char           number[MAX_ITOA_LENGTH];
    char           line[MAX_LINE_LENGTH];
    short          port;

    tmp = strtok(NULL," \t");

    serial.interval = atoi(tmp);
    if(serial.interval == 0)
    {
        say("Error in interval in serial: at line %d, number > 0 expected\r\n", 
                 linecount);
        return;
    }

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        say("Missing to-port on serial: at line %d\r\n", linecount);
        return;
    }

    if(check_ports(tmp, "To-port", "serial", linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    strcpy(serial.output, tmp);

    tmp = strtok(NULL, " \t\r\n");

    if(tmp == NULL)
    {
        say("Missing COM port on serial: at line %d\r\n", linecount);
        return;
    }

    tmp_case = tmp;
    while(*tmp_case != '\0')
    {
        *tmp_case = tolower(*tmp_case);
        tmp_case++;
    }

    if((strlen(tmp) != 4) || (strncmp(tmp, "com", 3) != 0))
    {
        say("Expected COM port setting on serial: at line %d\r\n",
                linecount);
        return;
    }

    if((tmp[3] < '0') || (tmp[3] > '9'))
    {
        say("Wrong COM port number on serial: at line %d, expected 1..9\r\n",
                linecount);
    }

    serial.comnr = tmp[3] - '0';

    tmp = strtok(NULL, " \t\r\n");

    if(tmp == NULL)
    {
        say("Missing baudrate on serial: at line %d\r\n", linecount);
        return;
    }

    serial.baudrate = atoi(tmp);

    if( (serial.baudrate != 1200) && (serial.baudrate != 2400) &&
        (serial.baudrate != 4800) && (serial.baudrate != 9600) )
    {
        say("Wrong baudrate on serial: at line %d, only "
            "1200, 2400, 4800 or 9600 baud\r\n", linecount);
        return;
    }

    serial.path = NULL;

    path = strtok(NULL, "\r\n");

    if(path != NULL)
    {
        tmp_case = path;
        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        serial.path = strdup(path);

        if(serial.path == NULL)
        {
            say("Out of memory on serial: at line %d\r\n", linecount);
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
                say("Argument (%s) to long on serial: at line %d\r\n",
                        tmp, linecount);

                free(serial.path);
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

    port = find_first_port(serial.output);
    if(port == -1)
    {
        say("To-port out of range (1-%d) on serial: at line %d\r\n",
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
            say("To-port out of range (1-%d) on serial: at line %d\r\n",
                    mac_ports(), linecount);
            return;
        }
    }
    strcpy(serial.output, output);

    if(fgets(&line[0], MAX_LINE_LENGTH - 1, fd) != NULL)
    {
        serial.serialtext_p = NULL;

        /* store the types */
        tmp = strtok(&line[0], " \t\r\n");
        if(tmp == NULL)
        {
            /* Special case, empty line -> accept everything! */
            serial_serialtext("", &serial); 
        }
        while(tmp != NULL)
        {
            serial_serialtext(tmp, &serial); 
            tmp = strtok(NULL, " \t\r\n");
        }
        linecount++;
    }
    else
    {
        say("Early end-of-file on serial: at line %d\r\n", linecount);
        if(serial.path != NULL)
            free(serial.path);
        return;
    }

    serial_p = (digi_serial_t*) malloc(sizeof(digi_serial_t));

    /*
     * if out of memory then ignore
     */
    if(serial_p == NULL)
    {
        say("Out of memory while handling serial: line at line %d\r\n",
                linecount);
        return;
    }
    
    /* initialize the timer, start with transmission after 15 seconds */
    start_timer(&serial.timer, 15);

    /*
     * Duplicate data
     */
    memcpy(serial_p, &serial, sizeof(digi_serial_t));

    /*
     * Add to serialer-list
     */
    serial_p->next = digi_serial_p;
    digi_serial_p = serial_p;

    serial_init(serial_p) ;
}

/*
 * Transmit the data specified by the file which is specified 
 * in the digi_serial_t structure.
 */
static void transmit_serial(digi_serial_t *serial_p)
{
    digi_serialtext_t *st_p;
    uidata_t           uidata;
    short              ndigi;
    char              *tmp;
    char              *path;

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
    if(serial_p->path != NULL)
    {
        /* make copy of digipeater path for strtok */
        path = strdup(serial_p->path);
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

    /* get the transmission data from the collected sentences */
    st_p = serial_p->serialtext_p;

    /* Transmit each sentence */
    while(st_p != NULL)
    {
        /* Don't transmit if we never received this sentence */
        if(strlen(st_p->sentence) == 0)
        {
            /* move ahead to next sentence */
            st_p = st_p->next;
            continue;
        }

        strcpy(uidata.data, st_p->sentence);
        strcat(uidata.data, "\r");
        uidata.size = strlen(st_p->sentence) + 1;

        /* avoid digipeating my own data */
        keep_old_data(&uidata);

        /* check size of what we are about to transmit, kenwood mode */
        if(uidata_kenwood_mode(&uidata) == 0)
        {
            return; /* failure */
        }

        /* show what we will transmit */
        dump_uidata_to(serial_p->output, &uidata);

        /* now do the transmission for each port */
        uidata.port = find_first_port(serial_p->output);
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

        /* move ahead to next sentence */
        st_p = st_p->next;
    }
}

/*
 * Do serial command, see if there is any transmission to be done at this
 * time, if there is do the transmission and restart the timer for the
 * next time.
 */
void do_serial()
{
    digi_serial_t     *serial_p;

    /*
     * walk through the transmission list and start transmission if
     * needed
     */
    serial_p = digi_serial_p;

    /*
     * While there are still serial commands to consider
     */
    while(serial_p != NULL)
    {
        /*
         * Read data from the serial line
         */
        serial_read(serial_p);

        if(is_elapsed(&(serial_p->timer)) != 0)
        {
            /* it is time to serial */
            transmit_serial(serial_p);

            /* command handled, show separator */
            display_separator();

            /* restart the timer */
            start_timer(&(serial_p->timer), serial_p->interval * TM_MIN);
        }

        /* advanve to next serial command */
        serial_p = serial_p->next;
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
void swap_digicall_digidest_serial()
{
    digi_serial_t     *serial_p;
    char               path[MAX_DATA_LENGTH];
    char               oldpath[MAX_DATA_LENGTH];
    char              *call;
    short              replaced;

    /*
     * walk through the serial list, replace DIGI_CALL and DIGI_DEST
     */
    serial_p = digi_serial_p;
    while(serial_p != NULL)
    {
        if(serial_p->path != NULL)
        {
            replaced = 0;

            path[0] = '\0';

            /* make a copy, don't run tokenizer on original string */
            strcpy(oldpath, serial_p->path);
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
                strcpy(serial_p->path, path);
            }
        }

        serial_p = serial_p->next;
    }
}
#endif /* _SERIAL_ */
