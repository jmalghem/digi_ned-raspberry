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
#include <math.h>
#include <time.h>
#include <ctype.h>
#ifdef _LINUX_
#include "linux.h"
#endif
#include "digicons.h"
#include "digitype.h"
#include "read_ini.h"
#include "mac_if.h"
#include "genlib.h"
#include "keep.h"
#include "output.h"
#include "mheard.h"
#include "dx.h"
#include "distance.h"

/*
 * The position of the digipeater is kept here
 */
position_t  digi_pos = { 0L, 0L };

/*
 * Global data, used in message.c to assemble responses for DX queries
 */
char        digi_dx_times[MAX_DX_TIMES + 1] = "";
char        digi_dx_metric[3] = "km";

/*
 * Linked lists containing the 'dx_path' information per port
 * Linked lists containing the 'dxlevel' data
 */
static path_t *port_dx_path_p[MAX_PORTS] = { 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static dxlevel_t *port_dxlevel_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/*
 * Here are the lists is used to store port names.
 */
static portname_t *digi_portname_p = NULL;     /* digi portnames */

/*
 * Add portnames from a dx_portname: command from the .ini file to the
 * list.
 *
 * dx_portname: 1 144.800 MHz
 *             ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * token of the "dx_portname" command is already handled. The tokenizer
 * is now at the point in the string indicated by '^' above (the ':' was
 * changed to '\0' by 'strtok').
 */
void add_dx_portname()
{
    portname_t *portname_p;
    char       *tmp;
    short       port;

    tmp = strtok(NULL,", \t\r\n");

    if(tmp == NULL)
    {
        say("Missing port number on dx_portname: line at line %d\r\n",
                linecount);
        return;
    }

    port = atoi(tmp);

    if(port < 1 || port > mac_ports())
    {
        say("Port out of range (1-%d) on dx_portname: at line %d\r\n",
                mac_ports(), linecount);
        return;
    }

    tmp = strtok(NULL,"\r\n");

    if(tmp == NULL)
    {
        say("Missing portname on dx_portname: line at line %d\r\n",
                linecount);
        return;
    }

    portname_p = (portname_t*) malloc(sizeof(portname_t));
    if(portname_p == NULL)
    {
        /*
         * if out of memory then quit
         */
        say("Out of memory while handling dx_portname: line at line %d\r\n",
                linecount);
        return;
    }

    port--; /* make base 0 numbers */

    portname_p->next = NULL;
    portname_p->port = port;
    portname_p->portname_p = strdup(tmp);

    if(portname_p->portname_p == NULL)
    {
        /*
         * if out of memory then quit
         */
        say("Out of memory while handling dx_portname: line at line %d\r\n",
                linecount);
        free(portname_p);
        return;
    }

    portname_p->next = digi_portname_p;
    digi_portname_p  = portname_p;
}

/*
 * Return portname based on port number.
 */
static char *get_portname(short port)
{
    portname_t  *portname_p;
    /* 4 is the length of "Port", space for '\0' already in MAX_ITOA_LENGTH */
    static char  portname[4 + MAX_ITOA_LENGTH]; 

    portname_p = digi_portname_p;

    while(portname_p != NULL)
    {
        if(portname_p->port == port)
        {
            return portname_p->portname_p;
        }
        portname_p = portname_p->next;
    }

    strcpy(portname, "Port");
    (void) digi_itoa(port + 1, &(portname[4]));

    return portname;
}

/*
 * Determine distance and bearing from the digipeater position
 */
long distance_and_bearing(position_t *pos_p, short *bearing_p)
{
    return dist_distance_and_bearing(&digi_pos, pos_p, bearing_p,
                                                    digi_dx_metric);
}

/*
 * Convert an APRS packet to a position
 *
 * Returns: 0 If a position is not returned, this happens when
 *            - the position of the digipeater is unknown (0,0)
 *            - the APRS packet does not contain a position
 *            - the APRS position has a formatting error
 *          1 Position returned in the storage referenced by pos_p
 */
short aprs2position(uidata_t *uidata_p, position_t *pos_p)
{
    if((digi_pos.lat == 0L) && (digi_pos.lon  == 0L))
    {
        /* Our own location is 0, return 0 to indicate failure */
        return 0;
    }

    return dist_aprs2position(uidata_p, pos_p);
}

/*
 * Add digi_pos command from the .ini file to the internal administration.
 *
 * digi_pos: 5213.61N 00600.00E
 *          ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digi_pos' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_digi_pos()
{
    position_t  mypos;
    char       *p;
    short       res;
    char        buf[19];

    /* Get lattitude from the tokenizer */
    p = strtok(NULL, " \t\r\n");
    if(p == NULL)
    {
        say("Missing lattitude on digi_pos: at line %d\r\n", linecount);
        return;
    }

    /*
     * Make shure this is lattitude (the conversion function takes both
     * so if you specified longitude here it would also work)
     */
    if(strlen(p) != 8)
    {
        say("Lattitude should be 8 characters on digi_pos: at line %d\r\n",
                                                                linecount);
        return;
    }

    /* Convert lattitude to long - this will also check the format */
    res = dist_ascii2long(p, &(mypos.lat));
    if(res == 0)
    {
        say("Can't convert lattitude on digi_pos: " 
            "at line %d (wrong format?)\r\n", linecount);
        return;
    }

    /* Get longitude from the tokenizer */
    p = strtok(NULL, " \t\r\n");
    if(p == NULL)
    {
        say("Missing longitude on digi_pos: at line %d\r\n", linecount);
        return;
    }

    /*
     * Make shure this is lattitude (the conversion function takes both
     * so if you specified longitude here it would also work)
     */
    if(strlen(p) != 9)
    {
        say("Longitude should be 9 characters on digi_pos: at line %d\r\n",
                                                                linecount);
        return;
    }

    /* Convert longitude to long - this will also check the format */
    res = dist_ascii2long(p, &(mypos.lon));
    if(res == 0)
    {
        say("Can't convert longitude on digi_pos: " 
            "at line %d (wrong format?)\r\n", linecount);
        return;
    }

    /* All went well, position read and converted, store it */
    digi_pos.lat = mypos.lat;
    digi_pos.lon = mypos.lon;

    vsay("Digipeater location: %s\r\n", dist_pos2ascii(&digi_pos, buf));

    return;
}

/*
 * Add digi_pos command from the beacon file specified in the .ini file to
 * the internal administration.
 *
 * digi_pos_file: digibcon.ini
 *               ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digi_pos_file' token is already handled. The tokenizer is now at the
 * point in the string indicated by '^' above (the ':' was changed to '\0'
 * by 'strtok').
 */
void add_digi_pos_file()
{
    char       *beaconfile;
    position_t  mypos;
    uidata_t    uidata;
    char        line[MAX_LINE_LENGTH];
    FILE       *fd;
    short       res;
    char        buf[19];

    beaconfile = strtok(NULL," \t\r\n");
    if(beaconfile == NULL)
    {
        say("Missing beacon filename on digi_pos_file: at line %d\r\n",
                    linecount);
        return;
    }

    /* slash handling and file completion */
    beaconfile = digi_convert_file(beaconfile);

    if(beaconfile == NULL)
    {
        say("Out of memory on digi_pos_file: at line %d\r\n", linecount);
        return;
    }

    /* get the transmission data from file */
    fd = fopen(beaconfile, "rt");
    if(fd == NULL)
    {
        /* not needed anymore */
        free(beaconfile);

        /* can't open file, no position to read! */
        say("Can't open file \"%s\" on digi_pos_file: at line %d\r\n",
                beaconfile, linecount);
        return;
    }

    /* get the first line from the beaconfile */
    if(fgets(line, MAX_LINE_LENGTH -2, fd) == NULL)
    {
        /* make line empty */
        line[0] = '\0';
    }

    /* don't need file anymore */
    fclose(fd);
    
    /*
     * Build a dummy UI APRS packet so we can use the normal disassembly
     * routines for positions
     *
     * Only the destination call and data are used for position information
     * the rest can be ignored safely.
     */

    strcpy(uidata.destination,"APRS");
    strcpy(uidata.data, line);
    uidata.size = strlen(line);

    res = dist_aprs2position(&uidata, &mypos);
    
    if(res == 0)
    {
        say("No position found in file \"%s\" on digi_pos_file: at "
            "line %d\r\n", beaconfile, linecount);
    }
    else
    {
        /* All went well, position read and converted, store it */
        digi_pos.lat = mypos.lat;
        digi_pos.lon = mypos.lon;

        vsay("Digipeater location: %s\r\n", dist_pos2ascii(&digi_pos, buf));
    }

    /* not needed anymore */
    free(beaconfile);

    return;
}

void find_best_dx(short port, mheard_t **dx_best_pp, mheard_t **dx_second_pp,
                                                time_t age, long limit)
{
    mheard_t *mheard_p;

    *dx_best_pp = NULL;
    *dx_second_pp = NULL;

    mheard_p = first_mheard(port);

    /*
     * loop until we are at the end of the mheard list
     * collect best and second best station along the way
     */
    while(mheard_p != NULL)
    {
        /* check if we should look at this */
        if((mheard_p->when >= age) && (mheard_p->distance != -1))
        {
            /* okay, not too old */

            if((*dx_best_pp) == NULL)
            {
                /* best pointers completely empty */
                (*dx_best_pp) = mheard_p;
            }
            else
            {
                /* Only proceed checking if the distance not above the limit */
                if(mheard_p->distance <= limit)
                {
                    /* look if we have to swap the best */
                    if((*dx_best_pp)->distance < mheard_p->distance)
                    {
                        /* this one is further away, put first */

                        /* best is now second best  */
                        (*dx_second_pp) = (*dx_best_pp);

                        /* mheard_p is the best now */
                        (*dx_best_pp)   = mheard_p;
                    }
                    else
                    {
                        /*
                         * best is still the best, look if we have to swap
                         * the second best
                         */
                        if((*dx_second_pp) == NULL)
                        {
                            /*
                             * there is no second best station,
                             * put mheard here
                             */
                            (*dx_second_pp) = mheard_p;
                        }
                        else
                        {
                            if((*dx_second_pp)->distance < mheard_p->distance)
                            {
                                /* mheard_p is the second best now */
                                (*dx_second_pp) = mheard_p;
                            }
                        }
                    }
                }
            }
        }

        mheard_p = next_mheard(port);
    }
    return;
}

/*
 * Add dx_times: command from the .ini file to the internal administration.
 *
 * dx_times: all,24,1
 *          ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'dx_times' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_dx_times()
{
    char  *p;
    short  len;
    char   line[MAX_DX_TIMES + 2];

    /* start with empty string */
    strcpy(line, "");
    p = strtok(NULL, ", \t\r\n");
    while(p != NULL)
    {
        if((strlen(line) + strlen(p)) > MAX_DX_TIMES)
        {
            say("Definition of dx_times: too long at line %d "
                "(max 80 characters)\r\n", linecount);
            return;
        }

        len = strspn(p, "0123456789");

        if(stricmp(p,"all") == 0)
        {
            strcat(line,p);
            strcat(line,",");
        }
        else if(len == strlen(p))
        {
            if(len > 5)
            {
                say("Number too big in dx_times: at line %d "
                    "(more than 5 digits)\r\n", linecount);
                return;
            }
            strcat(line,p);
            strcat(line,",");
        }
        else
        {
            say("Illegal definition of dx_times: at line %d "
                "(wrong format?)\r\n", linecount);
            return;
        }

        p = strtok(NULL, ", \t\r\n");
    }

    /* Remove the last comma in the string */
    len = strlen(line);
    if(len > 0)
    {
        line[len - 1] = '\0';
    }

    /* Copy the string */
    strcpy(digi_dx_times, line);

    return;
}

/*
 * Add dx_metic: command from the .ini file to the internal administration.
 *
 * dx_metric: km
 *           ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'dx_metric' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_dx_metric()
{
    char  *p;

    p = strtok(NULL, " \t\r\n");
    if( (strcmp(p, "km") != 0)
        &&
        (strcmp(p, "mi") != 0)
        &&
        (strcmp(p, "nm") != 0)
      )
    {
        say("Wrong value \"%s\" in dx_metric: at line %d, "
            "expected \"km\", \"mi\" or \"nm\"\r\n", p, linecount);
        return;
    }

    /* Copy the string */
    strcpy(digi_dx_metric, p);

    return;
}

/*
 * Add dxlevel stucture to a dxlevel linked list.
 */
static void add_to_dxlevel_list(dxlevel_t* dxlevel_p, dxlevel_t **list_pp)
{
    dxlevel_t *dl;

    /*
     * allocate a storage place for this dxlevel rule
     */
    dl = (dxlevel_t*) malloc(sizeof(dxlevel_t));

    /*
     * if out of memory then ignore
     */
    if(dl == NULL)
    {
        say("Out of memory while handling dx_level: line at line %d\r\n",
                 linecount);
        return;
    }

    /*
     * Duplicate data
     */
    memcpy(dl, dxlevel_p, sizeof(dxlevel_t));

    /*
     * Add the newly created dxlevel data to the port map of this port
     */
    dl->next = *list_pp;
    *list_pp = dl;
}

/*
 * Add dx_level: command from the .ini file to the internal administration.
 *
 * dx_level: all 50-100 1
 *          ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'dx_level' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_dx_level()
{
    dxlevel_t   dxlevel;
    char       *ports;
    char       *distance_min;
    char       *distance_max;
    char       *age;
    short       port;

    ports = strtok(NULL, " \t\r\n");
    if(ports == NULL)
    {
        say("Missing port on dx_level: at line %d\r\n", linecount);
        return;
    }

    if(check_ports(ports,"Port", "dx_level:", linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    distance_min = strtok(NULL, " \t\r\n");
    if(distance_min == NULL)
    {
        say("Missing distance on dx_level: at line %d\r\n", linecount);
        return;
    }

    distance_max = strchr(distance_min, '-');
    if(distance_max == NULL)
    {
        /*
         * Set max to a value bigger than the maximum distance on earth in
         * any metric scale.
         */
        distance_max = MAX_DX_DIST_STR;
    }
    else
    {
        /* Overwrite the '-' to terminate the minimum value */
        *distance_max = '\0';

        /* Advance 1 char to make distance_max point to the maximum distance */
        distance_max++;

        /* Handle maximum distance */
        if(strspn(distance_max,"0123456789") != strlen(distance_max))
        {
            say("Illegal maximum distance definition on dx_level: at line %d, "
                "numeric value expected\r\n", linecount);
            return;
        }
    }

    /* Handle minimum distance */
    if(strspn(distance_min,"0123456789") != strlen(distance_min))
    {
        say("Illegal distance definition on dx_level: at line %d, "
            "numeric value expected\r\n", linecount);
        return;
    }

    age =  strtok(NULL, " \t\r\n");
    if(age == NULL)
    {
        say("Missing age on dx_level: at line %d\r\n", linecount);
        return;
    }

    if(strspn(age,"0123456789") != strlen(age))
    {
        say("Illegal age definition on dx_level: at line %d, "
            "numeric value expected\r\n", linecount);
        return;
    }

    dxlevel.distance_min = (strtol(distance_min, NULL, 10) * 10L);
    dxlevel.distance_max = (strtol(distance_max, NULL, 10) * 10L);
    dxlevel.age          = (short) atoi(age);
    dxlevel.next         = NULL;

    /*
     * dxlevel structure was now setup without problems. Add it to
     * the applicable ports
     */
    port = find_first_port(ports);
    if(port == -1)
    {
        say("Port out of range (1-%d) on dx_level: at line %d\r\n",
                mac_ports(), linecount);
        return;
    }
    while(port != 0)
    {
        port--; /* make base 0 numbers */

        /* add rule to the list */
        add_to_dxlevel_list(&dxlevel, &(port_dxlevel_p[port]));

        port = find_next_port();
        if(port == -1)
        {
            say("Port out of range (1-%d) on dx_level: at line %d\r\n",
                    mac_ports(), linecount);
            return;
        }
    }
}

/*
 * Add dx_path command from the .ini file to the internal administration.
 *
 * dx_path: 1,2,3,4,5,6,7,8 DX,WIDE,TRACE6-6
 *         ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'dx_path' token is already handled. The tokenizer is now at the
 * point in the string indicated by '^' above (the ':' was changed to
 * '\0' by 'strtok').
 */
void add_dx_path()
{
    path_t  dx_path;
    path_t *dx_path_p;
    char   *tmp;
    char   *tmp_case;
    char   *path;
    char    output[OUTPORTS_LENGTH];
    short   port;

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        say("Missing to-port on dx_path: at line %d\r\n", linecount);
        return;
    }

    if(check_ports(tmp, "To-port", "dx_path:", 
                   linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    strcpy(output, tmp);

    dx_path.path = NULL;

    path = strtok(NULL, "\r\n");

    if(path != NULL)
    {
        tmp_case = path;
        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        dx_path.path = strdup(path);

        if(dx_path.path == NULL)
        {
            say("Out of memory on dx_path: at line %d\r\n", linecount);
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
                say("Argument (%s) to long on dx_path: at line %d\r\n",
                        tmp, linecount);

                free(dx_path.path);
                return;
            }
            tmp = strtok(NULL, " ,\t\r\n");
        }
    }

    /*
     * Store the dx_path in the port_dx_path list for each port
     */
    port = find_first_port(output);
    if(port == -1)
    {
        say("To-port out of range (1-%d) on dx_path: at line %d\r\n",
                mac_ports(), linecount);
        return;
    }
    while(port != 0)
    {
        port--; /* make base 0 numbers */

        /*
         * Allocate dynamic storage for this entry
         */
        dx_path_p = (path_t*) malloc(sizeof(path_t));

        /*
         * if out of memory then ignore
         */
        if(dx_path_p == NULL)
        {
            say("Out of memory while handling dx_path: line at line %d\r\n",
                    linecount);
            return;
        }
        
        /*
         * Duplicate data
         */
        memcpy(dx_path_p, &dx_path, sizeof(path_t));

        /*
         * Add to the dx_path list for this port
         */
        dx_path_p->next = port_dx_path_p[port];
        port_dx_path_p[port] = dx_path_p;

        port = find_next_port();
        if(port == -1)
        {
            say("To-port out of range (1-%d) on dx_path: at line %d\r\n",
                    mac_ports(), linecount);
            return;
        }
    }
}

/*
 * Transmit the DX data to the ports with paths specified in the path_t
 * structure.
 */
static void transmit_dx(uidata_t *uidata_p, long distance, short bearing)
{
    path_t   *dx_path_p;
    uidata_t  uidata;
    short     ndigi;
    char     *path;
    char     *tmp;
    char      output[MAX_ITOA_LENGTH];
    char      line[MAX_LINE_LENGTH];
    short     i;
    /*
     * string for distance, worse-case length
     * max long length: -2147483648 : 11 characters
     * string "Km", "Mi" or "Nm"    :  3 characters
     * terminating "\0"             :  1 character
     *                                ------------- +
     *            worse case length : 15 characters
     */
    char      distance_s[15];
    /*
     * string for call, worse-case length
     * max call length: PE1DNN-15: 9 characters
     * string ":"                : 1 character
     * terminating "\0"          : 1 character
     *                             ------------- +
     *        worse case length : 11 characters
     */
    /*
    char      call_s[11];
     */

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

    /*
     * fill in destination flag (desination call overwritten later,
     * see below)
     */
    strcpy(uidata.destination, digi_digi_dest);
    uidata.dest_flags = RR_FLAG;

    /* set the number of digis to a defined value, will be updated later */
    uidata.ndigi = 0;

    /* fill in DX message
     *
     * format:
     *
  DX de DXCALL-10>11111111112222222222***33333333334444444444***********55555
  <----><---9---> <---10---><---10---><3><---10---><---10---><---11----><-5->
     *
     * DX de is mandatory
     */
    sprintf(distance_s,"%3ld%s\r", distance / 10L, digi_dx_metric);
    sprintf(line,"DX de %9.9s>%8.8s  %-10.10s   %03d dg frm"
                 "%10.10s           %5.5s\r", digi_digi_call,
                 get_portname(uidata_p->port), uidata_p->originator,
                 bearing, digi_digi_call, distance_s);
 
    strcpy(uidata.data, line);
    uidata.size = strlen(line);

    /*
     * The template for the packet is finished, see if we already transmitted
     * this recently before filling in the destinations and sending it out
     */
    if(is_new_data(&uidata) == 0)
    {
        /*
         * We already transmitted this recently
         * Show the next message is DX tracing is enabled
         */
        dsay("DX message for this station was already sent recently\r\n");
        return;
    }

    /* avoid digipeating my own data */
    keep_old_data(&uidata);

    for(i = 0; i < mac_ports(); i++)
    {
        dx_path_p = port_dx_path_p[i];

        /* fill in the port */
        uidata.port = i;

        /* if no path is specified, no transmission takes place on that port */
        while(dx_path_p != NULL)
        {
            /* make copy of the path for strtok */
            ndigi=0;
            if(dx_path_p->path != NULL)
            {
                path = strdup(dx_path_p->path);
                if(path == NULL)
                {
                    /* no memory for transmission */
                    return;
                }

                /* fill dest. if it is in the path (first argument) */
                tmp = strtok(path, " ,\t\r\n");
                if(tmp != NULL)
                {
                    /*
                     * only need to set dest., dest_flags value was set
                     * above
                     */
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
                else
                {
                    /* fill in default destination */
                    strcpy(uidata.destination, digi_digi_dest);
                }

                /* don't need "path" anymore, free memory */
                free(path);
            }
            else
            {
                /* fill in default destination */
                strcpy(uidata.destination, digi_digi_dest);
            }

            uidata.ndigi = ndigi;

            /* check size of what we are about to transmit, kenwood mode */
            if(uidata_kenwood_mode(&uidata) != 0)
            {
                /* Okay, proceed */

                /* make output-number */
                (void) digi_itoa(uidata.port + 1, output);

                /* show what we will transmit */
                dump_uidata_to(output, &uidata);

                /* transmit */
                put_uidata(&uidata);
            }

            dx_path_p = dx_path_p->next;
        } /* while(dx_path_p != NULL) */
    } /* for(i = 1; i < mac_ports(); i++) */
}

/*
 * dx_ok
 *
 * Returns 1 when distance is within limits or 0 when not
 */
short dx_ok(uidata_t *uidata_p, long distance)
{
    dxlevel_t *dl;
    short      port;
    long       max;

    /* If distance is '-1' then it is unknown */
    if(distance == -1L)
    {
        /* Unknown distance, not okay */
        return 0;
    }

    /* retrieve the port number */
    port = uidata_p->port;

    max = 0L;

    /*
     * Walk through the DX list to get the maximum allowable distance
     * for this port
     */
    dl = port_dxlevel_p[port];
    while(dl != NULL)
    {
        if(max < dl->distance_max)
        {
            max = dl->distance_max;
        }
        dl = dl->next;
    }

    if(distance > max)
    {
        /*
         * This distance is above any maximum length specified, and therefore
         * bogus.
         */
        return 0;
    }

    /* distance seems to be possible on this port, accepted */
    return 1;
}

/*
 * Handle DX message generation
 */
void do_dx(uidata_t *uidata_p, long distance, short bearing)
{
    dxlevel_t *dl;
    mheard_t  *best_dx_p;
    mheard_t  *second_best_dx_p;
    short      port;
    short      new_dx;

    /* If distance is '-1' then it is unknown */
    if(distance == -1L)
    {
        /* Unknown distance */
        return;
    }

    /* retrieve the port number */
    port = uidata_p->port;

    new_dx = 0;

    /* Walk through the DX list to detect if we have a new DX */
    dl = port_dxlevel_p[port];
    while(dl != NULL)
    {
        if( (distance >= dl->distance_min)
            &&
            (distance <= dl->distance_max)
          )
        {
            /*
             * Between the DX distance theresholds, see if it is better then
             * stations over the last dl->age hours
             */
            find_best_dx(port, &best_dx_p, &second_best_dx_p, time(NULL) - 
                        (time_t) (((long) dl->age) * 3600L), dl->distance_max);
            if(best_dx_p == NULL)
            {
                new_dx = 1;
                /*
                 * Show the next message if DX tracing is enabled
                 *
Best DX of  1h at port 1: PE1DNN-12     0.0 km (only station in the DX list)
                 */
                dsay("Best DX of %2dh at port %d: %-9s %5ld.%ld %s "
                     "(only station in the list)\r\n", dl->age,
                     uidata_p->port + 1, uidata_p->originator,
                     distance / 10L, distance % 10L, digi_dx_metric);
                break;
            }
            else if(best_dx_p->distance < distance)
            {
                new_dx = 1;
                /*
                 * Show the next message if DX tracing is enabled
                 *
Best DX of  1h at port 1: PE1DNN-12     0.0 km (now the best in class)
                 */
                dsay("Best DX of %2dh at port %d: %-9s %5ld.%ld %s "
                     "(now the best in class)\r\n", dl->age,
                     uidata_p->port + 1, uidata_p->originator,
                     distance / 10L, distance % 10L, digi_dx_metric);
                break;
            }
            else
            {
                /*
                 * Show the next message if DX tracing is enabled
                 *
Best DX of  1h at port 1: PE1DNN-12     0.0 km (checked PE1DNN-12     0.0 km)
                 */
                dsay("Best DX of %2dh at port %d: %-9s %5ld.%ld %s "
                     "(checked %-9s %5ld.%ld %s)\r\n", dl->age,
                      uidata_p->port + 1, best_dx_p->call,
                      best_dx_p->distance / 10L, best_dx_p->distance % 10L,
                      digi_dx_metric, uidata_p->originator,
                      distance / 10L, distance % 10L, digi_dx_metric);
            }
        }

        dl = dl->next;
    }

    if(new_dx == 1)
    {
        transmit_dx(uidata_p, distance, bearing);
    }

    return;
}
