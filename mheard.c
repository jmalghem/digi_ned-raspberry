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
#include <time.h>
#ifdef _LINUX_
#include "linux.h"
#endif
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "read_ini.h"
#include "mheard.h"
#include "output.h"
#include "dx.h"
#include "call.h"

mheard_t *digi_mheard_p      = NULL;
mheard_t *digi_next_mheard_p = NULL;

mheard_t *next_mheard(short port)
{
    /*
     * Implementation warning: first_mheard/next_mheard shall be used
     * without having an update the mheard list inbetween!!
     */
    mheard_t *mheard_p;

    mheard_p = digi_next_mheard_p;
    while(mheard_p != NULL)
    {
        if((mheard_p->port == port) || (port == -1))
        {
            /* bingo! found one for this port */

            /* next time start with the next data-block */
            digi_next_mheard_p = mheard_p->next;

            /* return the matching mheard pointer */
            return mheard_p;
        }
        mheard_p = mheard_p->next;
    }

    /* mark that we are at the end */
    digi_next_mheard_p = NULL;

    /* no more matches, return NULL */
    return NULL;
}

mheard_t *first_mheard(short port)
{
    /*
     * Implementation warning: first_mheard/next_mheard shall be used
     * without having an update the mheard list inbetween!!
     */

    /* reset running pointer to the start */
    digi_next_mheard_p = digi_mheard_p;

    /* simply return the first match */
    return next_mheard(port);
}

void remove_mheard_call(char *call)
{
    mheard_t *mheard_p;
    mheard_t *previous_mheard_p;

    /*
     * Loop though mheards and unlink the call from the list if present
     */
    mheard_p          = digi_mheard_p;
    previous_mheard_p = NULL;
    while(mheard_p != NULL)
    {
        if(stricmp(call, mheard_p->call) == 0)
        {
            /* unlink this mheard data from the list */
            if(previous_mheard_p == NULL)
            {
                /* still at the beginning of the list */
                digi_mheard_p = mheard_p->next;

                /* Free this data */
                free(mheard_p);

                /* set mheard_p to next entry to check */
                mheard_p = digi_mheard_p;
            }
            else
            {
                /* not at the beginning of the list */
                previous_mheard_p->next = mheard_p->next;

                /* Free this data */
                free(mheard_p);

                /* set mheard_p to next entry to check */
                mheard_p = previous_mheard_p->next;
            }
        }
        else
        {
            /* keep this mheard, advance to next */
            previous_mheard_p = mheard_p;
            mheard_p = mheard_p->next;
        }
    }

    /* We scanned through the complete list, call was not found */
    return;
}

void remove_mheard_port(short port)
{
    mheard_t *mheard_p;
    mheard_t *previous_mheard_p;

    /*
     * Loop though the mheards and unlink the port infomation from the list
     */
    mheard_p          = digi_mheard_p;
    previous_mheard_p = NULL;

    while(mheard_p != NULL)
    {
        if(port == mheard_p->port)
        {
            /* unlink this mheard data from the list */
            if(previous_mheard_p == NULL)
            {
                /* still at the beginning of the list */
                digi_mheard_p = mheard_p->next;

                /* Free this data */
                free(mheard_p);

                /* set mheard_p to next entry to check */
                mheard_p = digi_mheard_p;
            }
            else
            {
                /* not at the beginning of the list */
                previous_mheard_p->next = mheard_p->next;

                /* Free this data */
                free(mheard_p);

                /* set mheard_p to next entry to check */
                mheard_p = previous_mheard_p->next;
            }
        }
        else
        {
            /* keep this mheard, advance to next */
            previous_mheard_p = mheard_p;
            mheard_p = mheard_p->next;
        }
    }

    /* We scanned through the complete list, removing entries along the way */
    return;
}

void remove_mheard()
{
    mheard_t *mheard_p;
    mheard_t *next_mheard_p;

    /*
     * Loop though the mheards and unlink each entry
     */
    mheard_p = digi_mheard_p;

    while(mheard_p != NULL)
    {
        /* remember next */
        next_mheard_p = mheard_p->next;

        /* Free this data */
        free(mheard_p);

        /* advance to next */
        mheard_p = next_mheard_p;
    }

    /* The list is empty now */
    digi_mheard_p = NULL;

    /* Finished */
    return;
}

mheard_t *find_mheard(char * call, short port)
{
    mheard_t *mheard_p;

    /* start from the start */
    mheard_p = digi_mheard_p;

    while(mheard_p != NULL)
    {
        if( (stricmp(call, mheard_p->call) == 0)
            &&
            (mheard_p->port == port)
          )
        {
            /* found! */
            return mheard_p;
        }

        mheard_p = mheard_p->next;
    }

    return NULL;
}

static mheard_t *lookup_mheard(char *call, short port)
{
    mheard_t *mheard_p;
    mheard_t *previous_mheard_p;
    short     count;

    /*
     * Loop though mheards to see if the call is present already
     *
     * If the call is present then unlink its data and return
     * the pointer to the data to the caller.
     */
    count             = 0;
    mheard_p          = digi_mheard_p;
    previous_mheard_p = NULL;
    while(mheard_p != NULL)
    {
        /* count the entries */
        count++;

        if( ( (stricmp(call, mheard_p->call) == 0)
              &&
              (mheard_p->port == port)
            )
            ||
            (count == digi_heard_size)
          )
        {
            /* unlink this mheard data from the list */
            if(previous_mheard_p == NULL)
            {
                /* still at the beginning of the list */
                digi_mheard_p = mheard_p->next;
            }
            else
            {
                /* not at the beginning of the list */
                previous_mheard_p->next = mheard_p->next;
            }

            /* some specific handling when re-using entries */
            if(count == digi_heard_size)
            {
                /*
                 * re-using the last entry in the list, clean distance
                 * and bearing
                 */
                mheard_p->distance = -1;
                mheard_p->bearing  = 0;
            }

            /* 
             * The mheard data for the call, or the last entry when the
             * list is full, is not in the list anymore, return for use.
             */
            return mheard_p;
        }
        else
        {
            /* keep this mheard */
            previous_mheard_p = mheard_p;
            mheard_p = mheard_p->next;
        }
    }

    /* 
     * If we end up here then the call is not in the heard-list yet and
     * the list is not full (otherwise the oldest mheard data block is
     * returned for reuse). Return a fresh data block or NULL when out
     * of memory.
     */
    mheard_p = malloc(sizeof(mheard_t));

    /* For new entries start with no knowledge of distance and bearing */
    mheard_p->distance = -1;
    mheard_p->bearing  = 0;

    /* Return the new entry */
    return mheard_p;
}

void do_mheard(uidata_t *uidata_p)
{
    mheard_t   *mheard_p;
    position_t  pos;
    short       res;
    long        distance;

    /* only support for normal AX.25 UI frames, if not, bail out */
    if(((uidata_p->primitive & ~0x10) != 0x03) || (uidata_p->pid != 0xf0))
    {
        /* not a normal AX.25 UI frame, bail out */
        return;
    }

    /* bail out if not received directly */
    if(uidata_p->distance != LOCAL)
    {
        return;
    }

    res = is_block(uidata_p->originator, uidata_p->port + 1);
    if(res == 1)
    {
        /* We don't handle blocked calls for mheard and DX */
        return;
    }

    if(res == -1)
    {
        /* And... we don't handle myself for mheard and DX */
        return;
    }

    /* the frame was received directly, get an entry from the mheard list */
    mheard_p = lookup_mheard(uidata_p->originator, uidata_p->port);

    if(mheard_p == NULL)
    {
        vsay("Out of memory on mheard handling\r\n");
        return;
    }

    /* fill in our data */
    strcpy(mheard_p->call, uidata_p->originator);
    mheard_p->port = uidata_p->port;
    mheard_p->when = (long) time(NULL);

    /*
     * Try to find out the distance and bearing to this station
     * (This will only work when the digipeater position is known)
     */
    res = aprs2position(uidata_p, &pos);
    if(res != 0)
    {
        /* That worked. Check if pos is not 0,0 */
        if((pos.lat != 0L) && (pos.lon  != 0L))
        {
            /* Measure distance and store answers in the mheard struct */
            distance = distance_and_bearing(&pos, &(mheard_p->bearing));

            /* Do a sanity check, insane distances are ignored */
            if(dx_ok(uidata_p, distance) == 1)
            {
                /* this distance looks fine, we will use it! */
                mheard_p->distance = distance;
            }
        }
    }

    /* find out if the station exceeds the threshold for the port */
    do_dx(uidata_p, mheard_p->distance, mheard_p->bearing);

    /* add to the front of the list */
    mheard_p->next = digi_mheard_p;
    digi_mheard_p  = mheard_p;

    /* finished */
    return;
}
