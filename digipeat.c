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
#include <ctype.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "mac_if.h"
#include "read_ini.h"
#include "digipeat.h"
#include "send.h"
#include "keep.h"
#include "call.h"
#include "output.h"
#include "preempt.h"
#ifdef _LINUX_
#include "linux.h"
#endif

/*
 * This file contains all code to interpret and execute the 'digipeat',
 * 'digiend', 'digito' and 'digissid' commands from the .ini file.
 *
 * Examples:
 * digipeat: all wide7-7 all swap0 wide7-6
 * digifirst: all wide7-7 all swap DIGI_CALL,wide7-6
 * diginext: all wide7-7 all swap0 wide7-6
 * digiend: all wide*,trace* 2 add LOCAL
 * digito: 1 AP????,BEACON,ID,WX,CQ 1 0 add DIGI_CALL,WIDE
 * digissid: 1 AP????,BEACON,ID,WX,CQ 1 0 add DIGI_CALL,WIDE
 */

/*
 * Linked lists containing the 'digipeat' command data
 */
static digipeat_t *port_digipeat_p[MAX_PORTS] = { 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static digipeat_t *port_digiend_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static digipeat_t *port_digito_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static digipeat_t *port_digissid_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static digipeat_t *port_digifirst_p[MAX_PORTS] = { 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static digipeat_t *port_diginext_p[MAX_PORTS] = { 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/*
 * Assignment if a port is used for LOCAL or REMOTE distances
 */
static distance_t distance[MAX_PORTS] = { 
    REMOTE, REMOTE, REMOTE, REMOTE, REMOTE, REMOTE, REMOTE, REMOTE };

static void add_to_list(const char *type, digipeat_t* digipeat_p,
                        digipeat_t **list_pp)
{
    digipeat_t *dm;
    digipeat_t *d;

    /*
     * allocate a storage place for this digipeat rule
     */
    dm = (digipeat_t*) malloc(sizeof(digipeat_t));

    /*
     * if out of memory then ignore
     */
    if(dm == NULL)
    {
        say("Out of memory while handling %s: line at line %d\r\n",
                 type, linecount);
        return;
    }

    /*
     * Duplicate data
     */
    memcpy(dm, digipeat_p, sizeof(digipeat_t));

    /*
     * Add the newly ceated digipeat to the port map of this port
     */
    if(*list_pp == NULL)
    {
        *list_pp = dm;
    }
    else
    {
        d = *list_pp;
        while(d->next != NULL)
        {
            d = d->next;
        }
        d->next = dm;
    }
}

/*
 * Add digipeat, digiend, digito, digissid, digifirst and diginext command
 * from the .ini file to the internal administration.
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * command token is already handled.
 */
static void common_add_digi(command_t command)
{
    digipeat_t  digipeat;
    char       *ports;
    char       *mapping;
    char       *tmp;
    char       *tmp_case;
    char       *from;
    short       port;
    short       i;
    short       allbut;
    char        number[MAX_ITOA_LENGTH];
    char        output[OUTPORTS_LENGTH];
    const char *type;

    switch(command) {
    case DIGIPEAT:
                type = "digipeat";
                break;
    case DIGIEND:
                type = "digiend";
                break;
    case DIGITO:
                type = "digito";
                break;
    case DIGIFIRST:
                type = "digifirst";
                break;
    case DIGINEXT:
                type = "diginext";
                break;
    default:
                /* DIGISSID */
                type = "digissid";
                break;
    }

    ports = strtok(NULL, " \t\r\n");
    if(ports == NULL)
    {
        say("Missing from-port on %s: at line %d\r\n", type, linecount);
        return;
    }

    if(check_ports(ports, "From-port", type, linecount, INCL_ALLBUT) == 0)
    {
        return;
    }

    from = strtok(NULL, " \t\r\n");
    if(from == NULL)
    {
        if((command == DIGIPEAT) || (command == DIGIEND) ||
           (command == DIGIFIRST) || (command == DIGINEXT))
        {
            say("Missing from-digi(s) on %s: at line %d\r\n", type, linecount);
        }
        else
        {
            say("Missing dest-call(s) on %s: at line %d\r\n", type, linecount);
        }
        return;
    }

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        say("Missing to-port on %s: at line %d\r\n", type, linecount);
        return;
    }

    if(check_ports(tmp, "To-port", type, linecount, INCL_ALLBUT) == 0)
    {
        return;
    }

    strcpy(digipeat.output, tmp);

    if((command == DIGITO) || (command == DIGISSID))
    {
        tmp = strtok(NULL, " \t\r\n");

        if(strspn(tmp, "0123456789") != strlen(tmp))
        {
            say("SSID (%s) must be a number on %s: at line %d\r\n",
                        tmp, type, linecount);
            return;
        }

        i = atoi(tmp);

        if((i < 0) || (i > 15))
        {
            say("SSID (%s) out of range (0-15) on %s: at line %d\r\n",
                        tmp, type, linecount);
            return;
        }

        digipeat.ssid = i;
    }
    else
    {
        digipeat.ssid = 0; /* not used, set to some defined value */
    }

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        digipeat.operation = NONE;
        digipeat.use = 1;
        digipeat.mapping   = NULL;
    }
    else
    {
        tmp_case = tmp;
        while(*tmp_case != '\0')
        {
            *tmp_case = tolower(*tmp_case);
            tmp_case++;
        }

        /*
         * First get the number of how many digi's shall be used up
         * by the operation.
         */
        i = strlen(tmp);
        if(i > 1)
        {
            /* 
             * Check for a number between 0 and MAX_DIGI. Note that if
             * the number is out of range the 'unknown operation'
             * message below will be given, no extra message needed
             * here!
             */
            if((tmp[i-1] >= '0') && (tmp[i-1] <= '8'))
            {
                digipeat.use = (short) (tmp[i-1] - '0');
                tmp[i-1] = '\0'; /* truncate the 'use' number */
            }
            else
            {
                digipeat.use = 1; /* default use one digipeater call */
            }
        }

        if(strcmp(tmp,"add") == 0)
        {
            digipeat.operation = ADD;
        }
        else if(strcmp(tmp,"replace") == 0)
        {
            digipeat.operation = REPLACE;
        }
        else if(strcmp(tmp,"new") == 0)
        {
            digipeat.operation = NEW;
        }
        else if(strcmp(tmp,"swap") == 0)
        {
            digipeat.operation = SWAP;
        }
        else if(strcmp(tmp,"hijack") == 0)
        {
            digipeat.operation = HIJACK;
        }
        else if(strcmp(tmp,"erase") == 0)
        {
            digipeat.operation = ERASE;
        }
        else if(strcmp(tmp,"keep") == 0)
        {
            digipeat.operation = KEEP;
        }
        else if(strcmp(tmp,"shift") == 0)
        {
            digipeat.operation = SHIFT;
        }
        else
        {
            say("Unknown operation (%s) on %s: at line %d\r\n",
                    tmp, type, linecount);
            return;
        }

        if((digipeat.operation != ERASE) && (digipeat.operation != KEEP))
        {
            /* All operations except erase and keep need arguments */
            mapping = strtok(NULL, "\r\n");

            if(mapping == NULL)
            {
                say("Operation without argument on %s: at line %d\r\n",
                        type, linecount);
                return;
            }

            tmp_case = mapping;
            while(*tmp_case != '\0')
            {
                *tmp_case = toupper(*tmp_case);
                tmp_case++;
            }

            digipeat.mapping = strdup(mapping);

            if(digipeat.mapping == NULL)
            {
                say("Out of memory on %s: at line %d\r\n",
                        type, linecount);
                return;
            }

            /*
             * Check for illegal digipeaters (too long)
             */
            tmp = strtok(mapping, " ,\t\r\n");
            while(tmp != NULL)
            {
                if(strlen(tmp) >= CALL_LENGTH)
                {
                    say("Argument (%s) to long on %s: at line %d\r\n",
                            tmp, type, linecount);

                    free(digipeat.mapping);
                    return;
                }
                tmp = strtok(NULL, " ,\t\r\n");
            }
        }
        else
        {
            /* for erase and keep no mapping is used */
            digipeat.mapping = NULL;

            tmp = strtok(NULL, " \t\r\n");
            if(tmp != NULL)
            {
                say("Warning operation doesn't need arguments on %s: "
                        "at line %d\r\n", type, linecount);
            }
        }
    }

    digipeat.next = NULL;

    /*
     * convert output shortcuts to full-lists
     * 'allbut' output ports are handled later on a per-port basis
     */
    allbut = 0;
    if(strcmp(digipeat.output,"allbut") == 0)
    {
        allbut = 1;
    }
    else
    {
        /*
         * Assemble the output string
         */

        /* start with empty output string, add the digits we find */
        output[0] = '\0';

        port = find_first_port(digipeat.output);
        if(port == -1)
        {
            say("To-port out of range (1-%d) on %s: at line %d\r\n",
                    mac_ports(), type, linecount);
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
                        mac_ports(), type, linecount);
                return;
            }
        }
        strcpy(digipeat.output, output);
    }

    from = strtok(from, ",");
    while(from != NULL)
    {
        tmp_case = from;

        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        if(strlen(from) >= CALL_LENGTH)
        {
            if((command == DIGIPEAT) || (command == DIGIEND) ||
               (command == DIGIFIRST) || (command == DIGINEXT))
            {
                say("From-digi (%s) to long on %s: at line %d\r\n",
                        from, type, linecount);
            }
            else
            {
                say("Dest-call (%s) to long on %s: at line %d\r\n",
                        from, type, linecount);
            }
            return;
        }
        strcpy(digipeat.digi, from);

        /*
         * digipeat was now setup without problems. Add it to the applicable
         * ports
         */
        port = find_first_port(ports);
        if(port == -1)
        {
            say("From-port out of range (1-%d) on %s: at line %d\r\n",
                    mac_ports(), type, linecount);
            return;
        }
        while(port != 0)
        {
            if(allbut != 0)
            {
                digipeat.output[0] = '\0';
                for(i = 1; i <= mac_ports(); i++)
                {
                    if(i != port)
                    {
                        if(digipeat.output[0] != '\0')
                            strcat(digipeat.output, ",");
                        strcat(digipeat.output, digi_itoa(i, number));
                    }
                }
            }

            /* make base 0 number */
            port--; 

            /* add rule to the correct list */
            switch(command) {
            case DIGIPEAT:
                add_to_list(type, &digipeat, &(port_digipeat_p[port]));
                break;
            case DIGIEND:
                add_to_list(type, &digipeat, &(port_digiend_p[port]));
                break;
            case DIGITO:
                add_to_list(type, &digipeat, &(port_digito_p[port]));
                break;
            case DIGISSID:
                add_to_list(type, &digipeat, &(port_digissid_p[port]));
                break;
            case DIGIFIRST:
                add_to_list(type, &digipeat, &(port_digifirst_p[port]));
                break;
            default:
            /* DIGINEXT: */
                add_to_list(type, &digipeat, &(port_diginext_p[port]));
                break;
            }

            port = find_next_port();
            if(port == -1)
            {
                say("From-port out of range (1-%d) on %s: at line %d\r\n",
                        mac_ports(), type, linecount);
                return;
            }
        }

        from = strtok(NULL, ",");
    }
}

/*
 * Add digipeat command from the .ini file to the internal administration.
 *
 * digipeat: all wide7-7 all swap0 wide7-6
 *          ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digipeat' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_digipeat()
{
    common_add_digi(DIGIPEAT);
}

/*
 * Add digiend command from the .ini file to the internal administration.
 *
 * digiend: all wide*,trace* 2 add LOCAL
 *         ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digiend' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_digiend()
{
    common_add_digi(DIGIEND);
}

/*
 * Add digito command from the .ini file to the internal administration.
 *
 * digito: 1 *-4 1 3 add WIDE
 *        ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digito' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_digito()
{
    common_add_digi(DIGITO);
}

/*
 * Add digissid command from the .ini file to the internal administration.
 *
 * digissid: 1 *-4 1 3 add WIDE
 *          ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digissid' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_digissid()
{
    common_add_digi(DIGISSID);
}

/*
 * Add digifirst command from the .ini file to the internal administration.
 *
 * digifirst: all wide7-7 all swap PE1DNN,wide7-6
 *           ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digipeat' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_digifirst()
{
    common_add_digi(DIGIFIRST);
}

/*
 * Add diginext command from the .ini file to the internal administration.
 *
 * diginext: all wide7-7 all swap0 wide7-6
 *          ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'digipeat' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_diginext()
{
    common_add_digi(DIGINEXT);
}

/*
 * Add local command from the .ini file to the internal administration.
 *
 * local: 1,2,3,4,5,6,7,8
 *       ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'local' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_local()
{
    char       *ports;
    short       port;

    ports = strtok(NULL, " \t\r\n");
    if(ports == NULL)
    {
        say("Missing local-port on local: at line %d\r\n", linecount);
        return;
    }

    if(check_ports(ports, "Local-port", "local", linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    /*
     * Fill in the distance array to mark local ports
     */
    port = find_first_port(ports);
    if(port == -1)
    {
        say("Local-port out of range (1-%d) on local: at line %d\r\n",
                mac_ports(), linecount);
        return;
    }
    while(port != 0)
    {
        /*
         * Make port numbers base start from 0 instead of 1
         */
        port--;

        /*
         * Mark port as local
         */
        distance[port] = LOCAL;

        port = find_next_port();
        if(port == -1)
        {
            say("Local-port out of range (1-%d) on local: at line %d\r\n",
                    mac_ports(), linecount);
            return;
        }
    }
}

short port_is_local(short port)
{
    if(distance[port] == LOCAL)
    {
        return 1;
    }
    return 0;
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
void swap_digicall_digidest_digi()
{
    short       i;
    digipeat_t *dp;
    char        map[MAX_DATA_LENGTH];
    char        oldmap[MAX_DATA_LENGTH];
    char       *call;
    short       replaced;
    short       digiend_done;
    short       digito_done;
    short       digissid_done;
    short       digifirst_done;
    short       diginext_done;

    for(i = 0; i < mac_ports(); i++)
    {
        /*
         * Start with the digipeat list. When done do the digiend list
         * then do the digito list and finally the digissid list.
         */
        digiend_done = 0;
        digito_done = 0;
        digissid_done = 0;
        digifirst_done = 0;
        diginext_done = 0;
        dp = port_digipeat_p[i];
        while(dp != NULL)
        {
            if(strcmp(dp->digi,"DIGI_CALL") == 0)
            {
                strcpy(dp->digi, digi_digi_call);
            }
            else if(strcmp(dp->digi,"DIGI_DEST") == 0)
            {
                strcpy(dp->digi, digi_digi_dest);
            }

            if( (dp->operation != NONE)
                &&
                (dp->operation != ERASE)
                &&
                (dp->operation != KEEP)
              )
            {
                replaced = 0;

                map[0] = '\0';
                /* make a copy, don't run tokenizer on original string */
                strcpy(oldmap, dp->mapping);
                call = strtok(oldmap, " ,\t\r\n");
                while(call != NULL)
                {
                    if(map[0] != '\0')
                        strcat(map, ",");

                    if(strcmp(call, "DIGI_CALL") == 0)
                    {
                        strcat(map, digi_digi_call);
                        replaced = 1;
                    }
                    else if(strcmp(call, "DIGI_DEST") == 0)
                    {
                        strcat(map, digi_digi_dest);
                        replaced = 1;
                    }
                    else
                    {
                        strcat(map, call);
                    }
                    call = strtok(NULL, " ,\t\r\n");
                }

                if(replaced == 1)
                {
                    /*
                     * !we can just overwrite since DIGI_CALL and DIGI_DEST
                     * have the worse-case length!! (PE1DNN-15 and DIGI_CALL
                     * have the same length!)
                     */
                    strcpy(dp->mapping, map);
                }
            }

            dp = dp->next;
            if((dp == NULL) && (digiend_done == 0))
            {
                /*
                 * we are finished with the digipeat list.
                 * now do the digiend list
                 */
                digiend_done = 1;
                dp = port_digiend_p[i];
            }

            if((dp == NULL) && (digito_done == 0))
            {
                /*
                 * we are finished with the digiend list.
                 * now do the digito list
                 */
                digito_done = 1;
                dp = port_digito_p[i];
            }

            if((dp == NULL) && (digissid_done == 0))
            {
                /*
                 * we are finished with the digito list.
                 * now do the digissid list
                 */
                digissid_done = 1;
                dp = port_digissid_p[i];
            }

            if((dp == NULL) && (digifirst_done == 0))
            {
                /*
                 * we are finished with the digissid list.
                 * now do the digifirst list
                 */
                digifirst_done = 1;
                dp = port_digifirst_p[i];
            }

            if((dp == NULL) && (diginext_done == 0))
            {
                /*
                 * we are finished with the digifirst list.
                 * now do the diginext list
                 */
                diginext_done = 1;
                dp = port_diginext_p[i];
            }
        }
    }
}

static void digipeat_mark(short digi_index, uidata_t *uidata_p, digipeat_t *dp)
{
    short i;

    /* set 'repeated' bits on the callsigns to be used up */
    for(i = 0; i < dp->use; i++)
    {
        if(digi_index < MAX_DIGI)
        {
            uidata_p->digi_flags[digi_index] = (char) (H_FLAG | RR_FLAG);
            digi_index++;
        }
    }
}

static void digipeat_none(short digi_index, uidata_t *uidata_p, digipeat_t *dp)
{
    /* do callsign substitution */
    strcpy(uidata_p->digipeater[digi_index], digi_digi_call);
    uidata_p->digi_flags[digi_index] = RR_FLAG;

    /* set 'repeated' bits on the callsigns to be used up */
    digipeat_mark(digi_index, uidata_p, dp);
}

static void add_digis(short ndigi, uidata_t *uidata_p, digipeat_t *dp)
{
    char *tmp;
    char  map[MAX_DATA_LENGTH];

    /* copy mapping for handling by strtok */
    strcpy(map, dp->mapping);

    /* get first digi from the map */
    tmp = strtok(map, ",");

    /* add more digipeaters to the end */
    while((tmp != NULL) && (ndigi < MAX_DIGI))
    {
        strcpy(uidata_p->digipeater[ndigi], tmp);

        /* set the reserved flags */
        uidata_p->digi_flags[ndigi] = RR_FLAG;

        /* increment digi counter */
        ndigi++;

        /* get next digi to add */
        tmp = strtok(NULL, ",");
    }
    uidata_p->ndigi = ndigi;
}

static void digipeat_add(short digi_index, uidata_t *uidata_p,
                         digipeat_t *dp, substitute_t subst)
{
    if(subst == SUBSTITUTE)
    {
        /* do callsign substitution */
        strcpy(uidata_p->digipeater[digi_index], digi_digi_call);
        uidata_p->digi_flags[digi_index] = RR_FLAG;
    }

    /* now add the digis from the digipeating at the end */
    add_digis(uidata_p->ndigi, uidata_p, dp);

    /* set 'repeated' bits on the callsigns to be used up */
    digipeat_mark(digi_index, uidata_p, dp);
}

static void digipeat_replace(short digi_index, uidata_t *uidata_p,
                                 digipeat_t *dp)
{
    /* add the digis from the digi list from the current digipeater */
    add_digis(digi_index, uidata_p, dp);

    /* set 'repeated' bits on the callsigns to be used up */
    digipeat_mark(digi_index, uidata_p, dp);
}

static void digipeat_swap(short digi_index, uidata_t *uidata_p, digipeat_t *dp,
                          short hijack)
{
    char  digipeater[MAX_DIGI][CALL_LENGTH];
    char  digi_flags[MAX_DIGI];
    short cdigi;
    short ndigi;
    short i;

    /* save digis after our digi to a local list */
    cdigi = 0;
    for(i = digi_index + 1; i < uidata_p->ndigi; i++)
    {
        strcpy(digipeater[cdigi], uidata_p->digipeater[i]);
        digi_flags[cdigi] = uidata_p->digi_flags[i];
        cdigi++;
    }

    /* now add the digis from the digipeating from here! */
    add_digis(digi_index - hijack, uidata_p, dp);
    
    /* get current amount of digis */
    ndigi = uidata_p->ndigi;

    /* now copy the saved digis after this */
    for(i = 0; i < cdigi; i++)
    {
        if(ndigi < MAX_DIGI)
        {
            strcpy(uidata_p->digipeater[ndigi], digipeater[i]);
            uidata_p->digi_flags[ndigi] = digi_flags[i];
            ndigi++;
        }
    }

    /* store new amount of digis */
    uidata_p->ndigi = ndigi;

    /* set 'repeated' bits on the callsigns to be used up */
    digipeat_mark(digi_index - hijack, uidata_p, dp);
}

static void digipeat_keep(short digi_index, uidata_t *uidata_p, digipeat_t *dp)
{
    /* only set 'repeated' bits on the callsigns to be used up */
    digipeat_mark(digi_index, uidata_p, dp);
}

static void digipeat_shift(short digi_index, uidata_t *uidata_p, digipeat_t *dp)
{
    /* 
     * copy digi at digi_index - 1 to first position
     *
     * note if digi_index == 0 then there is no previously used digi in the
     * path, if digi_index == 1 then the previously used digi is already at
     * the first position.
     */
    if(digi_index > 1)
    {
        strcpy(uidata_p->digipeater[0], uidata_p->digipeater[digi_index - 1]);
        uidata_p->digi_flags[0] = uidata_p->digi_flags[digi_index - 1];

        digi_index = 1;
    }

    /* now add the digis from the digipeating rule from here! */
    add_digis(digi_index, uidata_p, dp);

    /* set 'repeated' bits on the callsigns to be used up */
    digipeat_mark(digi_index, uidata_p, dp);
}

void send_to_ports(uidata_t *uidata_p, digipeat_t *dp)
{
    distance_t local_distance;
    short      res;

    /* check size of what we are about to transmit, kenwood mode */
    if(uidata_kenwood_mode(uidata_p) == 0)
    {
        return; /* failure */
    }

    /* show what we will transmit */
    dump_uidata_to(dp->output, uidata_p);

    /* remember how to transmit the packet on local ports */
    local_distance = uidata_p->distance;

    /* transmit to all named ports */
    uidata_p->port = find_first_port(dp->output);
    if(uidata_p->port == -1)
    {
        return;
    }
    while(uidata_p->port != 0)
    {
        /*
         * Make port numbers base start from 0 instead of 1
         */
        uidata_p->port--;

        /* 
         * if local_distance is LOCAL and we are now handling a local port
         * then change the distance to LOCAL, otherwise use REMOTE distance
         */
        if((local_distance == LOCAL) && (distance[uidata_p->port] == LOCAL))
        {
            uidata_p->distance = LOCAL;
        }
        else
        {
            uidata_p->distance = REMOTE;
        }

        /* transmit, if not blocked by via_block: */
        res = is_via_block(uidata_p);

        if(res != 0) /* only 0 is okay to tranmsit */
        {
            vsay("Not send to port %d due to via_block:\r\n",
                     uidata_p->port + 1);
        }
        else
        {
            put_uidata(uidata_p);
        }

        /* get next port to transmit on */
        uidata_p->port = find_next_port();
        if(uidata_p->port == -1)
        {
            return;
        }
    }

    /* remember that we handled this */
    keep_old_data(uidata_p);
}

static void process_rules(short digi_index, uidata_t *uidata_p,
                          digipeat_t *dp, substitute_t subst)
{
    /*
     * If received locally, always transmit remotely. If received remotely
     * transmit only locally on "local" ports
     */
    if(uidata_p->distance == LOCAL)
    {
        /* transmit LOCAL received packets into the REMOTE area */
        uidata_p->distance = REMOTE;
    }
    else
    {
        /*
         * transmit REMOTE received packets in the LOCAL area 
         * on 'local' ports
         */
        uidata_p->distance = LOCAL;
    }

    if(dp->operation == NONE)
    {
        digipeat_none(digi_index, uidata_p, dp);
        send_to_ports(uidata_p, dp);
    }
    else
    {
        switch(dp->operation) {
        case ADD:
                digipeat_add(digi_index, uidata_p, dp, subst);
                send_to_ports(uidata_p, dp);
                break;
        case REPLACE:
                digipeat_replace(digi_index, uidata_p, dp);
                send_to_ports(uidata_p, dp);
                break;
        case NEW:
                /* this is a replace from the beginning of the list */
                digipeat_replace(0, uidata_p, dp);
                send_to_ports(uidata_p, dp);
                break;
        case SWAP:
                digipeat_swap(digi_index, uidata_p, dp, 0);
                send_to_ports(uidata_p, dp);
                break;
        case HIJACK:
                if(digi_index > 0)
                    digipeat_swap(digi_index, uidata_p, dp, 1);
                else
                    digipeat_swap(digi_index, uidata_p, dp, 0);
                send_to_ports(uidata_p, dp);
                break;
        case ERASE:
                /* just remove all digipeaters */
                uidata_p->ndigi = 0;
                send_to_ports(uidata_p, dp);
                break;
        case KEEP:
                digipeat_keep(digi_index, uidata_p, dp);
                send_to_ports(uidata_p, dp);
                break;
        case SHIFT:
                digipeat_shift(digi_index, uidata_p, dp);
                send_to_ports(uidata_p, dp);
                break;
        default:
                break;
        }
    }
}

static void execute_digito_digissid_rule(uidata_t *uidata_p, digipeat_t *dp)
{
    char *p;
    char  number[MAX_ITOA_LENGTH];
    char  dest_call[CALL_LENGTH + 3];

    /* change destination SSID */
    strcpy(dest_call, uidata_p->destination);

    p = strrchr(dest_call, '-');
    if(p != NULL)
    {
        /* found old SSID, remove */
        *p = '\0';
    }

    /* now catinate a new SSID if needed */
    if(dp->ssid != 0)
    {
        strcat(dest_call, "-");
        strcat(dest_call, digi_itoa(dp->ssid, number));
    }

    if(strlen(dest_call) < CALL_LENGTH )
    {
        /* okay, did not overrun maximum lenght        */
        /* patch the destination call to the new value */
        strcpy(uidata_p->destination, dest_call);
    }

    /* do normal digipeating */
    process_rules(0, uidata_p, dp, NOSUBSTITUTE);
}

static void execute_digipeat_rule(short hop, uidata_t *uidata_p, digipeat_t *dp)
{
    /* do normal digipeating */
    process_rules(hop, uidata_p, dp, SUBSTITUTE);
}

static void execute_digiend_rule(short hop, uidata_t *uidata_p, digipeat_t *dp)
{
    /* add our own call to the end, then do the rule processing */
    hop++;
    if(hop >= MAX_DIGI)
    {
        /* doesn't fit, nothing to append anymore */
        return;
    }

    /* Add own call to the end, then perform digipeat: rules */
    strcpy(uidata_p->digipeater[hop], digi_digi_call);
    uidata_p->digi_flags[hop] = RR_FLAG;

    /* one digipeater more (already checked for room above) */
    uidata_p->ndigi++;

    /* do normal digipeating */
    process_rules(hop, uidata_p, dp, SUBSTITUTE);
}

short start_digipeat_rule(command_t command, short hop,
                                char *call_p, uidata_t *uidata_p)
{
    digipeat_t *dp;
    uidata_t    uidata;
    short       hit;

    hit = 0;

    switch(command) {
    case DIGIPEAT:
            dp = port_digipeat_p[uidata_p->port];
            break;
    case DIGIEND:
            dp = port_digiend_p[uidata_p->port];
            break;
    case DIGITO:
            dp = port_digito_p[uidata_p->port];
            break;
    case DIGISSID:
            dp = port_digissid_p[uidata_p->port];
            break;
    case DIGIFIRST:
            dp = port_digifirst_p[uidata_p->port];
            break;
    default:
            /* DIGINEXT */
            dp = port_diginext_p[uidata_p->port];
            break;
    }

    while(dp != NULL)
    {
        /*
         * PE1DNN: Fixed. A strtok iterrated through dp->digi, but
         * dp->digi always contains only one call! Removed the
         * strtok and associated variables.
         */
        if(match_call(call_p, dp->digi) == 1)
        {
            if(hit == 0)
            {
                /*
                 * Display header for each type of command. The header
                 * is only displayed on the first hit. If there are no
                 * hits then the header is not displayed. If it is still
                 * wanted it shall be done after this function returned.
                 * 
                 */
                switch(command) {
                case DIGIPEAT:
                    vsay("\r\n===========\r\nDigi: ");
                    vsay("Digipeater next due '%s', hop %d\r\n",call_p,hop+1);
                    break;
                case DIGIEND:
                    vsay("\r\n===========\r\nDigiEnd: ");
                    vsay("Digipeater last done '%s', hop %d\r\n",call_p,hop+1);
                    break;
                case DIGITO:
                    vsay("\r\n===========\r\nDigiTo:\r\n");
                    break;
                case DIGISSID:
                    vsay("\r\n===========\r\nDigiSsid:\r\n");
                    break;
                case DIGIFIRST:
                    vsay("\r\n===========\r\nDigiFirst: ");
                    vsay("Digipeater next due '%s', hop %d\r\n",call_p,hop+1);
                    break;
                default:
                    /* DIGINEXT */
                    vsay("\r\n===========\r\nDigiNext: ");
                    vsay("Digipeater next due '%s', hop %d\r\n",call_p,hop+1);
                    break;
                }
                hit = 1;
            }

            /* make a copy for further handling */
            memcpy(&uidata, uidata_p, sizeof(uidata_t));

            switch(command) {
            case DIGIPEAT:
                dump_rule("digipeat", uidata.port, dp);
                execute_digipeat_rule(hop, &uidata, dp);
                break;
            case DIGIEND:
                dump_rule("digiend", uidata.port, dp);
                execute_digiend_rule(hop, &uidata, dp);
                break;
            case DIGITO:
                dump_rule("digito", uidata.port, dp);
                execute_digito_digissid_rule(&uidata, dp);
                break;
            case DIGISSID:
                dump_rule("digissid", uidata.port, dp);
                execute_digito_digissid_rule(&uidata, dp);
                break;
            case DIGIFIRST:
                dump_rule("digifirst", uidata.port, dp);
                execute_digipeat_rule(hop, &uidata, dp);
                break;
            default:
                /* DIGINEXT */
                dump_rule("diginext", uidata.port, dp);
                execute_digipeat_rule(hop, &uidata, dp);
                break;
            }
        }

        /* proceed to next rule */
        dp = dp->next;
    }
    
    return hit;
}

static void do_digito_rules(uidata_t *uidata_p)
{
    char  digi_call[CALL_LENGTH];
    short result;

    /* search rule for this destination */
    strcpy(digi_call, uidata_p->destination);

    result= start_digipeat_rule(DIGITO, 0, digi_call, uidata_p);

    if(result == 0)
    {
        vsay("\r\n===========\r\nDigiTo:\r\n");
        vsay("No matching rule: ignore frame\r\n");
    }
}

static short do_digissid_rules(uidata_t *uidata_p)
{
    char        digi_call[CALL_LENGTH];
    short       result;

    /* search rule for this destination */
    strcpy(digi_call, uidata_p->destination);

    result= start_digipeat_rule(DIGISSID, 0, digi_call, uidata_p);

    return result;
}

static void do_digipeat_rules(uidata_t *uidata_p)
{
    short       digi_index;
    char        digi_call[CALL_LENGTH];
    short       result_normal;
    short       result_first;
    short       result_next;

    /* find the next active digi in the list */
    digi_index = 0;
    while(digi_index < uidata_p->ndigi)
    {
        if((uidata_p->digi_flags[digi_index] & H_FLAG) == 0)
        {
           break;
        }

        digi_index++;
    }

    if(digi_index == uidata_p->ndigi)
    {
        vsay("\r\n===========\r\nDigi: ");
        vsay("Error: no active digipeater found!\r\n");
        return;
    }

    strcpy(digi_call, uidata_p->digipeater[digi_index]);
 
    result_normal = 0;
    result_first = 0;
    result_next = 0;

    result_normal = start_digipeat_rule(DIGIPEAT, digi_index, digi_call,
                                        uidata_p);

    /* check if the first digipeater is due */
    if(digi_index == 0)
    {
        /* execute rules for first digi in the list */
        result_first = start_digipeat_rule(DIGIFIRST, digi_index, digi_call,
                                           uidata_p);
    }
    else
    {
        /* execute rules for digis not first in the list */
        result_next = start_digipeat_rule(DIGINEXT, digi_index, digi_call,
                                          uidata_p);
    }

    if ( (result_normal == 0)
         &&
         (result_first == 0)
         &&
         (result_next == 0)
       )
    {
        vsay("\r\n===========\r\nDigi: ");
        vsay("No matching rule: ignore frame\r\n");
    }
}

static void do_digiend_rules(uidata_t *uidata_p)
{
    short digi_index;
    char  digi_call[CALL_LENGTH];
    short result;

    /* find the next active digi in the list */
    digi_index = uidata_p->ndigi - 1;

    if(digi_index < 0)
    {
        vsay("\r\n===========\r\nDigiEnd: ");
        vsay("Error: no digipeater found!\r\n");
        return;
    }

    strcpy(digi_call, uidata_p->digipeater[digi_index]);

    result = start_digipeat_rule(DIGIEND, digi_index, digi_call, uidata_p);

    if(result == 0)
    {
        vsay("\r\n===========\r\nDigiEnd: ");
        vsay("No matching rule: ignore frame\r\n");
    }
}

/*
 * Used for the "?aprs?" broadcast handling.
 */
static void do_aprs_broadcast(uidata_t *uidata_p)
{
    char bc_string[APRS_BC_SIZE + 1];

    /* first check lenght, expect APRS_BC_SIZE bytes */
    if(uidata_p->size < APRS_BC_SIZE)
    {
        /* too small */
        return;
    }

    /* copy the string */
    memcpy(bc_string, uidata_p->data, APRS_BC_SIZE);
  
    /* add '\0' at the end */
    bc_string[APRS_BC_SIZE] = '\0';

    /* compare the data with the APRS_BC_STRING */
    if(stricmp(bc_string, APRS_BC_STRING) != 0)
    {
        /* not the APRS broadcast string */
        return;
    }

    vsay("Found ?APRS? broadcast, transmitting beacons\r\n");

    /* command handled, show separator */
    display_separator();

    /* send our beacons */
    send_all(BEACON);
}

/*
 * Used for the "?wx?" broadcast handling.
 */
static void do_wx_broadcast(uidata_t *uidata_p)
{
    char bc_string[APRS_WX_SIZE + 1];

    /* first check lenght, expect APRS_WX_SIZE bytes */
    if(uidata_p->size < APRS_WX_SIZE)
    {
        /* too small */
        return;
    }

    /* copy the string */
    memcpy(bc_string, uidata_p->data, APRS_WX_SIZE);
  
    /* add '\0' at the end */
    bc_string[APRS_WX_SIZE] = '\0';

    /* compare the data with the APRS_WX_STRING */
    if(stricmp(bc_string, APRS_WX_STRING) != 0)
    {
        /* not the APRS broadcast string */
        return;
    }

    vsay("Found ?WX? broadcast, transmitting beacons\r\n");

    /* command handled, show separator */
    display_separator();

    /* send our beacons */
    send_all(WX);
}

/*
 * Analyse broadcasts and if needed send beacons.
 */
void do_broadcast(uidata_t *uidata_p)
{
    do_aprs_broadcast(uidata_p);
    do_wx_broadcast(uidata_p);
}

void do_digipeat(uidata_t *uidata_p)
{
    short res;
    short portnr;
    short allow_digito_digissid;

    portnr = uidata_p->port + 1;

    res = is_block(uidata_p->originator, portnr);

    if(res == 1)
    {
        vsay("\r\n===========\r\nCheck: ");
        vsay("Originator blocked on port %d: ignore frame\r\n", portnr);
        return;
    }

    if(res == -1)
    {
        vsay("\r\n===========\r\nCheck: ");
        vsay("I will not repeat my own frame\r\n");
        return;
    }

    res = is_via_block(uidata_p);

    if(res == 1) /* found in VIA header */
    {
        vsay("\r\n===========\r\nCheck: ");
        vsay("Via digipeater blocked on port %d: ignore frame\r\n",
                 portnr);
        return;
    }
    else if(res == 2) /* found in 3rd party header */
    {
        vsay("\r\n===========\r\nCheck: ");
        vsay("Via block on 3rd party header on port %d: "
                 "ignore frame\r\n", portnr);
        return;
    }

    res = is_allowed_to(uidata_p->destination, portnr);

    if(res == 0)
    {
        vsay("\r\n===========\r\nCheck: ");
        vsay("Destination call not allowed on port %d: ignore frame\r\n",
                    portnr);
        return;
    }

    res = is_allowed_from(uidata_p->originator, portnr);

    if(res == 0)
    {
        vsay("\r\n===========\r\nCheck: ");
        vsay("Originator call not allowed on port %d: ignore frame\r\n",
                    portnr);
        return;
    }

    if(is_new_data(uidata_p) == 0)
    {
        vsay("\r\n===========\r\nCheck: ");
        vsay("Old data, ignore frame\r\n");
        return;
    }

    /*
     * Check if the digito: and digissid: rules are allowed
     *
     * These rules shall only be used on UI frames with PID=0xf0
     * and when when the first character of the data does not
     * appear in the "ssid_ignore_prefix:" rule
     * OR
     * when opentrac is enabled and PID=0x77 (Opentrac frame)
     */
    if((uidata_p->primitive & ~0x10) == 0x03)
    {
        if( ( 
              (uidata_p->pid == 0xf0)
              &&
              ( (uidata_p->size == 0)
                || 
                (strchr(digi_ssid_ign_prefix, uidata_p->data[0]) == NULL)
              )
            )
            ||
            (
              (digi_opentrac_enable == 1)
              &&
              (uidata_p->pid == 0x77)
            )
          )
        {
            /* Okay to use the digito: and digissid: rules */
            allow_digito_digissid = 1;
        }
        else
        {
            /* Wrong PID, not allowed to use digito: and digissid: now */
            allow_digito_digissid = 0;
        }
    }
    else
    {
        /* Not a UI frame, not allowed to use digito: and digissid: now */
        allow_digito_digissid = 0;
    }

    if(uidata_p->ndigi == 0)
    {
        /*
         * No digipeaters, check digito: rules
         */
        if( allow_digito_digissid == 1)
        {
            do_digito_rules(uidata_p);
        }
        else
        {
            vsay("\r\n===========\r\nCheck: ");
            if (digi_opentrac_enable == 1)
            {
                vsay("Digito: not used for non UI/PID F0 or PID 77 frames, "
                    "ignore frame\r\n");
            }
            else
            {
                vsay("Digito: not used for non UI/PID F0 frames, "
                    "ignore frame\r\n");
            }
        }
    }
    else
    {
        if((uidata_p->digi_flags[uidata_p->ndigi - 1] & H_FLAG) == 0)
        {
            /* remember if "digissid:" executed */
            res = 0;

            /* check if there were digipeaters already used */
            if((uidata_p->digi_flags[0] & H_FLAG) == 0)
            {
                /*
                 * None of the digipeaters are marked as "used". This is a
                 * possible SSID digipeating case, check digissid: rules
                 */
                if( allow_digito_digissid == 1)
                {
                    res = do_digissid_rules(uidata_p);
                }
            }

            /*
             * if "digissid:" was not tried or did not do anything then
             * try normal digipeating
             */
            if(res == 0)
            {
                /* Still digipeaters to do, check if we have to preempt */
                do_preempt(uidata_p);

                /* Still digipeaters to do, check digipeat: rules */
                do_digipeat_rules(uidata_p);
            }
        }
        else
        {
            /* No digipeaters, check digiend: rules */
            do_digiend_rules(uidata_p);
        }
    }

    /*
     * Handle broadcasts after generic digipeating
     */
    do_broadcast(uidata_p);
}
