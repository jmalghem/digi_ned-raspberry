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
/*
 * This file contains lists of calls, used for all kinds of purposes,
 * like a list of calls that are blocked and a list of digi-owners.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "digicons.h"
#include "digitype.h"
#include "mac_if.h"
#include "genlib.h"
#include "read_ini.h"
#include "call.h"
#include "output.h"

/*
 * Here are the lists is used to store call signs.
 */
static call_t *digi_owner_p      = NULL; /* digi maintainers              */

static call_t *digi_block_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
                                         /* blocked calls for digipeating */
static call_t *digi_via_block_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
                                         /* blocked calls for digipeating */
static call_t *digi_allow_to_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
                                         /* allowed "to" calls for digi   */
static call_t *digi_allow_from_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
                                         /* allowed "from" calls for digi */
static call_t *digi_msg_block_p[MAX_PORTS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
                                         /* blocked calls for query       */

/*
 * Add call(s) from a command from the .ini file to a list.
 *
 * - The first argument is a string that contains a list of calls.
 * - The second argument is the the text for the type of command - used for
 *   error messages.
 * - The third argument is a pointer to the list pointer which points to the
 *   list on which the calls shall be added.
 *
 * Warning, the function uses strtok on calls_p!! (strtok modifies the
 * string)
 */
static void common_add_call(char *calls_p, char *type, call_t **call_list_pp)
{
    call_t *call_p;
    char   *call;
    char   *tmp_case;

    if(calls_p == NULL)
    {
        say("No calls found while handling %s: line at line %d\r\n",
                        type, linecount);
        return;
    }

    call = strtok(calls_p,", \t\r\n");

    while(call != NULL)
    {
        if(strlen(call) < CALL_LENGTH)
        {
            call_p = (call_t*) malloc(sizeof(call_t));
            if(call_p == NULL)
            {
                /*
                 * if out of memory then quit
                 */
                say("Out of memory while handling %s: line at line %d\r\n",
                        type, linecount);
                return;
            }

            /*
             * Convert the call to uppercase
             */
            tmp_case = call;

            while(*tmp_case != '\0')
            {
                *tmp_case = toupper(*tmp_case);
                tmp_case++;
            }

            /*
             * Store the call
             */
            strcpy(call_p->call, call);

            /*
             * Add to call-list
             */
            call_p->next    = (*call_list_pp);
            (*call_list_pp) = call_p;
        }
        else
        {
            say("Call too long in %s: at line %d, number > 0 expected\r\n", 
                    type, linecount);
        }
        call = strtok(NULL,", \t\r\n");
    }
}

/*
 * Add call(s) from a command from the .ini file to the specified list.
 * This list is used to store destination call signs for one or more specific
 * ports.
 *
 * <token>: 1 AP*,GPS*,DX*,ID*
 *         ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * token is already handled. The tokenizer is now at the point in the
 * string indicated by '^' above (the ':' was changed to '\0' by 'strtok').
 *
 * The first argument is the the text for the type of command - used
 * for error messages. The second argument is a pointer to an array of
 * pointers. The pointer in the array points to the list pointer for each
 * port which pointd to the list on which the calls shall be added.
 *
 * **call_list_pp[] -> *call_list_p[0] -> *call_list (for port 1)
 *                     *call_list_p[1] -> *call_list (for port 2)
 *                     *call_list_p[2] -> *call_list (for port 3)
 *                     *call_list_p[3] -> *call_list (for port 4)
 *                     *call_list_p[4] -> *call_list (for port 5)
 *                     *call_list_p[5] -> *call_list (for port 6)
 *                     *call_list_p[6] -> *call_list (for port 7)
 *                     *call_list_p[7] -> *call_list (for port 8)
 *
 * The third parameter specifies if the port specification is optional
 */
void common_add_port_call(char *type, call_t *(*call_list_pp[]), short option)
{
    char       *ports;
    char       *calls;
    char       *tmp;
    char        all_ports[4];
    char       *arg1, *arg2;
    short       port;

    arg1 = strtok(NULL," \t\r\n");
    if(arg1 != NULL)
    {
        /* Get next argument, if it exists */
        arg2 = strtok(NULL,"\r\n");
    }
    else
    {
        arg2 = NULL;
    }

    if( (arg2 == NULL) && (option == 1) )
    {
        /* Only one argument, only calls present */
        strcpy(all_ports, "all");
        ports = all_ports;
        calls = arg1;
    }
    else
    {
        /* Two arguments, ports and calls present */
        ports = arg1;
        calls = arg2;
    }

    if(ports == NULL)
    {
        say("Missing from-port on %s: at line %d\r\n", type, linecount);
        return;
    }

    if(check_ports(ports, "From-port", type, linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    if(calls == NULL)
    {
        say("No calls found while handling %s: line at line %d\r\n",
                        type, linecount);
        return;
    }

    /*
     * Add the calls to the applicable ports
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
        /*
         * Make a copy so strtok can be used to hack out the calls.
         */
        tmp = strdup(calls);
        if(tmp == NULL)
        {
            say("Out of memory while handling %s: line at line %d\r\n",
                    type, linecount);
            return;
        }

        /* add rule to the list (using 0 based port number) */
        common_add_call(tmp, type, &((*call_list_pp)[port - 1]));

        /* throw away out temporary copy */
        free(tmp);

        port = find_next_port();
        if(port == -1)
        {
            say("From-port out of range (1-%d) on %s: at line %d\r\n",
                    mac_ports(), type, linecount);
            return;
        }
    }
}

/*
 * Determine if a call is present in the supplied list
 *
 * Return -1 if the call is the digi's own call
 *         1 if the call is present in the call list
 *         0 if the call is not present
 */
static short common_call_in_list(char *call, call_t* bp)
{
    if(strcmp(call, digi_digi_call) == 0)
    {
        /* found digi's own call */
        return -1;
    }

    while(bp != NULL)
    {
        if(match_call(call, bp->call) != 0)
        {
            /* found call in call-list! */
            return 1;
        }
        bp = bp->next;
    }

    /* Okay, call not in call-list */
    return 0;
}

/*
 * Add call(s) in the owner command from the .ini file to the
 * digi_owner list. This list is used to store call signs that
 * are allowed to do remote maintenance on the node
 *
 * owner: PE1DNN,PE1DNN-7,PE1MEW
 *       ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'owner' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_owner()
{
    common_add_call(strtok(NULL,"\0"), "owner", &digi_owner_p);
}

/*
 * Determine if a call is allowed to do maintenance
 *
 * Return -1 if the call is the digi's own call
 *         1 if the call is in the owners list
 *         0 if the call is not in the owners list
 */
short is_owner(char *call)
{
    return common_call_in_list(call, digi_owner_p);
}

/*
 * Return the first call in the owner list which is the main owner
 * of the digipeater
 *
 * Return  pointer to call when present
 *         NULL if the owner list is empty
 */
char *digi_owner()
{
    call_t *p;

    if(digi_owner_p == NULL)
    {
        return NULL;
    }

    /*
     * The first defined call is at the end of the list...
     * we have to walk through the list until the end
     */
    p = digi_owner_p;

    while(p->next != NULL)
    {
        p = p->next;
    }

    /* return a pointer to the call */
    return &(p->call[0]);
}

/*
 * Add call(s) in the block command from the .ini file to the digi_block_p
 * list. This list is used to store call signs that are blocked for
 * digipeating via this digipeater (like N0CALL, MYCALL etc.)
 *
 * block: N0CALL,NOCALL,MYCALL
 *       ^
 * Or with an optional port-number:
 *
 * block: all N0CALL,NOCALL,MYCALL
 *       ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'block' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_block()
{
    call_t **block_pp;

    block_pp = &(digi_block_p[0]);

    common_add_port_call("block", &block_pp, 1);
}

/*
 * Determine if a call is blocked
 *
 * Return -1 if the call is the digi's own call
 *         1 if the call is in the blocked list
 *         0 if the call is not blocked
 */
short is_block(char *call, short port)
{
    call_t *block_p;

    block_p = digi_block_p[port - 1];

    return common_call_in_list(call, block_p);
}

/*
 * Add call(s) in the block command from the .ini file to the digi_via_block_p
 * list. This list is used to store call signs of digis, packets that passed
 * via that digipeater are blocked for digipeating via this digipeater
 * (like TCPIP, IGATE etc.)
 *
 * via_block: TCPIP,IGATE
 *           ^
 * Or with an optional port-number:
 *
 * via_block: all TCPIP,IGATE
 *           ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'via_block' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_via_block()
{
    call_t **via_block_pp;

    via_block_pp = &(digi_via_block_p[0]);

    common_add_port_call("via_block", &via_block_pp, 1);
}

/*
 * Determine if a digipeater in the uidata where uidata_p points to
 * is blocked for this port.
 *
 * Return  0 if the digi is not blocked
 *         1 if found blocking call in VIA list
 *         2 if found blocking call in 3rd party header
 */
short is_via_block(uidata_t *uidata_p)
{
    short   i;
    short   res;
    char    p[MAX_DATA_LENGTH + 1];
    char   *q;
    call_t *via_block_p;
    
    via_block_p = digi_via_block_p[uidata_p->port];

    for(i = 0; i < uidata_p->ndigi; i++)
    {
        if((uidata_p->digi_flags[i] & H_FLAG) != 0)
        {
            res = common_call_in_list(uidata_p->digipeater[i], via_block_p);

            if(res == 1)
            {
                /* found blocking call in VIA list */
                return 1;
            }
        }
        else
        {
            /* we passed all "used" digis, no need to check further */
            break;
        }
    }

    /*
     * Check 3rd party header. Only do this for UI frames and PID
     * 0xf0 since that are the only kind of packets that can have
     * a 3rd patry header. Als we can only check for a 3rd party
     * header if the packet has data.
     */
    if((uidata_p->primitive & ~0x10) == 0x03)
    {
        if( (uidata_p->pid == 0xf0)
            &&
            (uidata_p->size > 0)
          )
        {
            /*
             * We passed all "used" digis in the AX.25 header, look at the
             * third-part header if present
             */
            if(uidata_p->data[0] == '}')
            {
                memcpy(p, uidata_p->data, MAX_DATA_LENGTH);

                /* ensure presence of a terminator */
                p[uidata_p->size] = '\0';

                q = strtok(&p[1], ":");

                if(q != NULL)
                {
                    /* We found the header portion, present in q */

                    /* split in components, separators ">" "," and "*" */
                    q = strtok(q, ">,*");
                    while(q != NULL)
                    {
                        res = common_call_in_list(q, via_block_p);

                        if(res == 1)
                        {
                            /* 2 if found blocking call in 3rd party header */
                            return 2;
                        }

                        q = strtok(NULL, ">,*");
                    }
                }
            }
        }
    }

    return 0;
}

/*
 * Add call(s) in the msg_block command from the .ini file to the
 * digi_msg_block_p list. This list is used to store call signs that
 * are blocked for querying the digipeater for responses.
 *
 * msg_block: N0CALL,NOCALL,MYCALL
 *           ^
 * Or with an optional port-number:
 *
 * msg_block: all N0CALL,NOCALL,MYCALL
 *           ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'block' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_msg_block()
{
    call_t **msg_block_pp;

    msg_block_pp = &(digi_msg_block_p[0]);

    common_add_port_call("msg_block", &msg_block_pp, 1);
}

/*
 * Determine if a call is blocked for queries/messages.
 *
 * Return -1 if the call is the digi's own call
 *         1 if the call is in the message blocked list
 *         0 if the call is not blocked for messages
 */
short is_msg_block(char *call, short port)
{
    call_t *msg_block_p;

    msg_block_p = digi_msg_block_p[port - 1];

    return common_call_in_list(call, msg_block_p);
}

/*
 * Add call(s) in the allow_to command from the .ini file to the
 * digi_allow_to_p array of lists. This list is used to store destination
 * call signs that are allowed in a packet for digipeating via this
 * digipeater (like AP*, GPS*, DX*, ID* etc.)
 *
 * allow_to: 1,2 AP*,GPS*,DX*,ID*
 *          ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'allow_to' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_allow_to()
{
    call_t **allow_to_pp;

    allow_to_pp = &(digi_allow_to_p[0]);

    common_add_port_call("allow_to", &allow_to_pp, 0);
}

/*
 * Determine if a desitnation call is allowed
 *
 * Return -1 if the call is the digi's own call
 *         1 if the call is in the allowed_to list
 *         0 if the call is not in the allowed_to list
 */
short is_allowed_to(char *call, short port)
{
    call_t *allow_to_p;

    allow_to_p = digi_allow_to_p[port - 1];

    /* if not specified, allow everybody */
    if(allow_to_p == NULL)
    {
        return 1;
    }

    return common_call_in_list(call, allow_to_p);
}

/*
 * Add call(s) in the allow_from command from the .ini file to the
 * digi_allow_from_p array of lists. This list is used to store originator
 * call signs that are allowed in a packet for digipeating via this
 * digipeater (like only from a specific digipeater, for example PE1MEW-2
 * and PD0JEY-2 etc.)
 *
 * allow_from: 1 PE1MEW-2,PD0JEY-2
 *            ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'allow_from' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_allow_from()
{
    call_t **allow_from_pp;

    allow_from_pp = &(digi_allow_from_p[0]);

    common_add_port_call("allow_from", &allow_from_pp, 0);
}

/*
 * Determine if a originator call is allowed
 *
 * Return -1 if the call is the digi's own call
 *         1 if the call is in the allowed_from list
 *         0 if the call is not in the allowed_from list
 */
short is_allowed_from(char *call, short port)
{
    call_t *allow_from_p;

    allow_from_p = digi_allow_from_p[port - 1];

    /* if not specified, allow everybody */
    if(allow_from_p == NULL)
    {
        return 1;
    }

    return common_call_in_list(call, allow_from_p);
}
