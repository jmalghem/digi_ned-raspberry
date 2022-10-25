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
#include "preempt.h"
#include "send.h"
#include "keep.h"
#include "output.h"

/*
 * This file contains all code to interpret and execute the 'preempt',
 * , 'preempt_keep' and 'preemt_never_keep' commands from the .ini file.
 *
 * Examples:
 * preempt: all RELAY IGNORE
 * preempt: all DIGI_CALL
 *
 * preempt_keep: PA*,PE*,PD*,PI*
 * preempt_never_keep: RELAY*,WIDE*,TRACE*
 */

/*
 * Linked lists containing the 'preempt' and 'preempt_keep' data
 */
static preempt_t *port_preempt_p[MAX_PORTS] = { 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static preempt_keep_t *digi_preempt_keep_p = NULL;
static preempt_keep_t *digi_preempt_never_keep_p = NULL;

static void add_to_list(preempt_t* preempt_p, preempt_t **list_pp)
{
    preempt_t *pr;
    preempt_t *p;

    /*
     * allocate a storage place for this preempt rule
     */
    pr = (preempt_t*) malloc(sizeof(preempt_t));

    /*
     * if out of memory then ignore
     */
    if(pr == NULL)
    {
        say("Out of memory while handling preempt: line at line %d\r\n",
                 linecount);
        return;
    }

    /*
     * Duplicate data
     */
    memcpy(pr, preempt_p, sizeof(preempt_t));

    /*
     * Add the newly created preempt data to the port map of this port
     */
    if(*list_pp == NULL)
    {
        *list_pp = pr;
    }
    else
    {
        p = *list_pp;
        while(p->next != NULL)
        {
            p = p->next;
        }
        p->next = pr;
    }
}

static void add_to_keep_list(char *type, preempt_keep_t* preempt_keep_p,
                                preempt_keep_t **list_pp)
{
    preempt_keep_t *pr;
    preempt_keep_t *p;

    /*
     * allocate a storage place for this preempt_keep call
     */
    pr = (preempt_keep_t*) malloc(sizeof(preempt_keep_t));

    /*
     * if out of memory then ignore
     */
    if(pr == NULL)
    {
        say("Out of memory while handling %s: line at line %d\r\n",
                 type, linecount);
        return;
    }

    /*
     * Duplicate data
     */
    memcpy(pr, preempt_keep_p, sizeof(preempt_keep_t));

    /*
     * Add the newly created preempt_keep data to the list
     */
    if(*list_pp == NULL)
    {
        *list_pp = pr;
    }
    else
    {
        p = *list_pp;
        while(p->next != NULL)
        {
            p = p->next;
        }
        p->next = pr;
    }
}

/*
 * Add preempt command from the .ini file to the internal administration.
 *
 * preempt: all DIGI_CALL PREEMPT_CALL
 *         ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'preempt' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_preempt()
{
    preempt_t   preempt;
    char       *ports;
    char       *digi;
    char       *outd;
    short       port;
    char       *tmp_case;

    ports = strtok(NULL, " \t\r\n");
    if(ports == NULL)
    {
        say("Missing from-port on preempt: at line %d\r\n", linecount);
        return;
    }

    if(check_ports(ports, "From-port", "preempt:", linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    digi = strtok(NULL, " \t\r\n");
    if(digi == NULL)
    {
        say("Missing preempt-digi(s) on preempt: at line %d\r\n", linecount);
        return;
    }

    outd = strtok(NULL, " \t\r\n");
    if(outd != NULL)
    {
        tmp_case = outd;
        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        if(strlen(outd) > 9)
        {
            say("Preempt-digi replacement (%s) to long on preempt: at "
                "line %d\r\n", outd, linecount);
            return;
        }
        strcpy(preempt.outd, outd);
    }

    preempt.next = NULL;

    digi = strtok(digi, ",");
    while(digi != NULL)
    {
        tmp_case = digi;
        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        if(strlen(digi) > 9)
        {
            say("Preempt-digi (%s) to long on preempt: at line %d\r\n",
                        digi, linecount);
            return;
        }
        strcpy(preempt.digi, digi);

        /*
         * When no replacement digi was given use an empty call. This
         * will cause replacement by the call in the recieved packet.
         */
        if(outd == NULL)
        {
            strcpy(preempt.outd, "");
        }

        /*
         * preempt structure was now setup without problems. Add it to
         * the applicable ports
         */
        port = find_first_port(ports);
        if(port == -1)
        {
            say("From-port out of range (1-%d) on preempt: at line %d\r\n",
                    mac_ports(), linecount);
            return;
        }
        while(port != 0)
        {
            port--; /* make base 0 numbers */

            /* add rule to the list */
            add_to_list(&preempt, &(port_preempt_p[port]));

            port = find_next_port();
            if(port == -1)
            {
                say("From-port out of range (1-%d) on preempt: at line %d\r\n",
                        mac_ports(), linecount);
                return;
            }
        }
        digi = strtok(NULL, ",");
    }
}

/*
 * Add preempt_keep or preempt_never_keep command from the .ini file
 * to the internal administration.
 */
static void add_preempt_keep_common(char *type)
{
    preempt_keep_t  preempt_keep;
    char           *digi;
    char           *tmp_case;

    digi = strtok(NULL, " \t\r\n");
    if(digi == NULL)
    {
        say("Missing digi call(s) on %s: at line %d\r\n", type, linecount);
        return;
    }

    preempt_keep.next = NULL;

    preempt_keep.keep = NOT_KEPT;

    digi = strtok(digi, ",");
    while(digi != NULL)
    {
        tmp_case = digi;
        while(*tmp_case != '\0')
        {
            *tmp_case = tolower(*tmp_case);
            tmp_case++;
        }

        if(strlen(digi) > 9)
        {
            say("Digi call (%s) to long on %s: at line %d\r\n",
                        digi, type, linecount);
            return;
        }
        strcpy(preempt_keep.digi, digi);

        /* add call to the list */
        if(strcmp(type, "preempt_keep") == 0)
            add_to_keep_list(type, &preempt_keep, &digi_preempt_keep_p);
        else
            add_to_keep_list(type, &preempt_keep, &digi_preempt_never_keep_p);
 
        digi = strtok(NULL, ",");
    }
}

/*
 * Add preempt_keep command from the .ini file to the internal
 * administration.
 *
 * preempt_keep: CALL,CALL,CALL
 *              ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'preempt_keep' token is already handled. The tokenizer is now at the
 * point in the string indicated by '^' above (the ':' was changed
 * to '\0' by 'strtok').
 */
void add_preempt_keep()
{
    add_preempt_keep_common("preempt_keep");
}

/*
 * Add preempt_never_keep command from the .ini file to the internal
 * administration.
 *
 * preempt_never_keep: CALL,CALL,CALL
 *                    ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'preempt_never_keep' token is already handled. The tokenizer is now
 * at the point in the string indicated by '^' above (the ':' was 
 * changed to '\0' by 'strtok').
 */
void add_preempt_never_keep()
{
    add_preempt_keep_common("preempt_never_keep");
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
void swap_digicall_digidest_preempt()
{
    preempt_t       *pp;
    preempt_keep_t  *pk;
    short            i;

    for(i = 0; i < mac_ports(); i++)
    {
        /*
         * walk through the preempt list.
         */
        pp = port_preempt_p[i];
        while(pp != NULL)
        {
            if(strcmp(pp->digi,"DIGI_CALL") == 0)
            {
                strcpy(pp->digi, digi_digi_call);
            }
            else if(strcmp(pp->digi,"DIGI_DEST") == 0)
            {
                strcpy(pp->digi, digi_digi_dest);
            }

            if(strcmp(pp->outd,"DIGI_CALL") == 0)
            {
                strcpy(pp->outd, digi_digi_call);
            }
            else if(strcmp(pp->outd,"DIGI_DEST") == 0)
            {
                strcpy(pp->outd, digi_digi_dest);
            }
            pp = pp->next;
        }
    }

    /*
     * walk through the preempt_keep list.
     */
    pk = digi_preempt_keep_p;
    while(pk != NULL)
    {
        if(strcmp(pk->digi,"DIGI_CALL") == 0)
        {
            strcpy(pk->digi, digi_digi_call);
        }
        else if(strcmp(pk->digi,"DIGI_DEST") == 0)
        {
            strcpy(pk->digi, digi_digi_dest);
        }
        pk = pk->next;
    }

    /*
     * walk through the preempt_never_keep list.
     */
    pk = digi_preempt_never_keep_p;
    while(pk != NULL)
    {
        if(strcmp(pk->digi,"DIGI_CALL") == 0)
        {
            strcpy(pk->digi, digi_digi_call);
        }
        else if(strcmp(pk->digi,"DIGI_DEST") == 0)
        {
            strcpy(pk->digi, digi_digi_dest);
        }
        pk = pk->next;
    }
}

/*
 * Change "via" list
 *
 * Modify the "via" list to preempt a call. Keep the calls defined in
 * "preempt_keep:"
 * "used".
 */
void preempt_via_list(short hop, short due,
                      char *new_call_p, uidata_t *uidata_p)
{
    short              ndigi;
    char               digipeater[MAX_DIGI][CALL_LENGTH];
    char               digi_flags[MAX_DIGI];
    preempt_keep_t    *pk;
    preempt_keep_t    *pn;
    short              i;
    enum  {KEEP, SKIP} keep;

    ndigi = 0;

    /* copy all calls before the currently due call */
    for(i = 0; i < due; i++)
    {
        strcpy(digipeater[ndigi], uidata_p->digipeater[i]);
        digi_flags[ndigi] = uidata_p->digi_flags[i];
        ndigi++;
    }

    /* now store the preempted digi with the repacement call */
    strcpy(digipeater[ndigi], new_call_p);
    digi_flags[ndigi] = RR_FLAG;
    ndigi++;

    /*
     * reset all the flags so we can later see which calls are kept
     * and show this on verbose-output
     */
    pk = digi_preempt_keep_p;
    while(pk != NULL)
    {
        pk->keep = NOT_KEPT;
        pk = pk->next;
    }

    /* copy the preempted digis that are in the preempt_keep: list */
    for(i = due; i < hop; i++)
    {
        pk = digi_preempt_keep_p;
        while(pk != NULL)
        {
            if(match_call(uidata_p->digipeater[i], pk->digi) == 1)
            {
                /*
                 * assume we keep it, unless the call is found in
                 * the "never_keep" list
                 */
                keep = KEEP;

                /*
                 * Check if the call does not match a call in the
                 * "never_keep" list. If it matches we don't keep
                 * it.
                 */
                pn = digi_preempt_never_keep_p;
                while(pn != NULL)
                {
                    if(match_call(uidata_p->digipeater[i], pn->digi) == 1)
                    {
                        /*
                         * found that we sould never keep this call!
                         * skip it.
                         */
                        keep = SKIP;

                        /* no need to keep looking, terminate the loop */
                        pn = NULL;
                    }
                    else
                    {
                        pn = pn->next;
                    }
                }

                /* if we should still keep the call, do so */
                if(keep == KEEP)
                {
                    /* found in keep list, copy */
                    strcpy(digipeater[ndigi], uidata_p->digipeater[i]);
                    digi_flags[ndigi] = uidata_p->digi_flags[i];
                    ndigi++;

                    /*
                     * mark that we retained this call for verbose
                     * output later
                     */
                    pk->keep = KEPT;

                    /* handled this one, terminate the loop */
                    pk = NULL;
                }
                else
                {
                    pk = pk->next;
                }
            }
            else
            {
                pk = pk->next;
            }
        }
    }

    /* Copy the remaining digipeaters */
    for(i = hop + 1; i < uidata_p->ndigi; i++)
    {
        strcpy(digipeater[ndigi], uidata_p->digipeater[i]);
        digi_flags[ndigi] = uidata_p->digi_flags[i];
        ndigi++;
    }

    /*
     * The new "via" list has been build, copy it back to the
     * uidata_p structure
     */
    for(i = 0; i < ndigi; i++)
    {
        strcpy(uidata_p->digipeater[i], digipeater[i]);
        uidata_p->digi_flags[i] = digi_flags[i];
    }
    uidata_p->ndigi = ndigi;
}

/*
 * Start preempt handling
 *
 * If a call is found all digis before matched call are changed to
 * "used".
 *
 * Return: 0 if the digi at the indicated hop does not match with
 *           any preempt rule.
 *         1 if the digi at the indicated hop does match with
 *           a preempt rule. 
 */
short start_preempt_rule(short hop, short due, char *call_p, uidata_t *uidata_p)
{
    preempt_t  *pp;

    pp = port_preempt_p[uidata_p->port];

    while(pp != NULL)
    {
        if(match_call(call_p, pp->digi) == 1)
        {
            /*
             * if a match is found with the digi that was already next
             * due in the via-list, then leave the list untouched
             *
             * There will not be a dump of the preempt rule and there
             * is no further checking to preempt on other calls.
             */
            if(hop == due) return 1;

            /* reshuffle the "via" list */
            if(pp->outd[0] == '\0')
            {
                /* move preemted call */
                preempt_via_list(hop, due, call_p, uidata_p);
            }
            else
            {
                /* replace preemted call */
                preempt_via_list(hop, due, pp->outd, uidata_p);
            }

            /*
             * show preempt rule (have to do it after the shuffel because we
             * also want to show which calls are kept
             */
            vsay("\r\n===========\r\nPreempt: ");

            dump_preempt(uidata_p->port, pp, digi_preempt_keep_p);

            dump_uidata_preempt(uidata_p);

            /* Preempt only takes the first match */
            return 1;
        }

        /* proceed to next preempt rule */
        pp = pp->next;
    }
    
    /* no preempt rule found for the call on this hop */
    return 0;
}

void do_preempt(uidata_t *uidata_p)
{
    short       digi_index;
    char        digi_call[CALL_LENGTH];
    short       result;
    short       digi_due;

    result  = 0;
    digi_due = -1;

    /* check all active digis, first hit wins in the list */
    digi_index = 0;
    while(digi_index < uidata_p->ndigi)
    {
        if((uidata_p->digi_flags[digi_index] & H_FLAG) == 0)
        {
            if(digi_due == -1)
            {
                /*
                 * Remember the index of the first not-used digipeater
                 */
                digi_due = digi_index;
            }

            strcpy(digi_call, uidata_p->digipeater[digi_index]);

            /*
             * see if the digi at digi_index is in the preempt list
             * if it is, act on it. Returns '1' is preemption took place
             */
            result = start_preempt_rule(digi_index, digi_due,
                                            digi_call, uidata_p);

            if(result != 0)
            {
                /* Only the first found preempt rule is used */
                break;
            }
        }
        digi_index++;
    }
}
