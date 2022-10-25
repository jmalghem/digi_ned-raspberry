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
 *
 *  This module is donated to the DIGI_NED project by Alex Krist, KG4ECV.
 *  Many thanks for this nice contribution!
 */
#ifdef _SATTRACKER_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "mac_if.h"
#include "read_ini.h"
#include "send.h"
#include "keep.h"
#include "timer.h"
#include "output.h"
#include "predict.h"
#include "dx.h"
#include "message.h"

/* Linked list to store satellite objects to be send */

static satobject_t  *satobject_list_p;

/* 
 * Initialize the satellite object list 
 */
void init_satobject_list(void)
{
    satobject_list_p = NULL;
}

/*
 * Check if satellite is already in request list. If so,
 * then determine if it's being tracked or not.
 * Returns pointer to the satobject if in list, NULL if not in the list.
 */
static satobject_t *in_satobject_list(char *sat, short port, short track)
{
    satobject_t *satobject_p;

    /* start at beginning of the list */
    satobject_p = satobject_list_p;
    while(satobject_p != NULL)
    {
        if( (strcmp(satobject_p->sat, sat) == 0)
            &&
            (satobject_p->port == port)
            &&
            (satobject_p->track == track)
          )
        {
            /* found, return pointer to this object */
            return satobject_p;
        }

        /* advance to next satellite */
        satobject_p = satobject_p->next;
    }

    /* Scanned though the list but could not find this sat! */
    return NULL; 
}

/*
 * Remove a satellite from the request list 
 */
void remove_satobject(char *sat, short port, short track)
{
    satobject_t *satobject_p;
    satobject_t *next_satobject_p;
    satobject_t *previous_satobject_p;
    short       found;

    satobject_p = satobject_list_p;
    previous_satobject_p = NULL;
    found = 0;

    while(satobject_p != NULL && !found)
    {
        /* save next satobject */
        next_satobject_p = satobject_p->next;

        if( (strcmp(satobject_p->sat, sat) == 0)
            &&
            (satobject_p->port == port)
            &&
            (satobject_p->track == track) 
          )
        {
            /* delete satobject from list */
            if(previous_satobject_p == NULL)
            {
                /* still at the beginning of the list */
                satobject_list_p = next_satobject_p;
            }
            else
            {
                /* not at the beginning of the list */
                previous_satobject_p->next = next_satobject_p;
            }

            /* throw away this object */
            free(satobject_p);

            /* let satobject_p point to next satobject */
            satobject_p = next_satobject_p;

            /* indicate that satellite object has been found and removed */
            found = 1;
        }
        else
        {
            /* keep this object */
            previous_satobject_p = satobject_p;
            satobject_p = next_satobject_p;
        }
    }

    return;
}

/* 
 * Add a satellite object to the list of requested satellites
 */
void add_satobject(char *sat, uidata_t *uidata_p, short track, short interval)
{
    satobject_t   *satobject_p;
    unsigned long nextaos_l;

    /* check if the requested satellite is already in the request list */
    satobject_p = in_satobject_list(sat, uidata_p->port, track);

    /* 
     * if already in list then return. There's no sense in adding
     * the object twice with the same properties.
     */
    if(satobject_p != NULL)
    {
        /*
         * Somebody requested this sat, make it transmit by clearing the
         * timer. Also the duration starts from this point.
         */
        satobject_p->interval = 10 * interval;
    
        /* retransmit interval-time seconds from now */
        start_timer(&(satobject_p->timer), satobject_p->interval * TM_SEC);

        /* reinitialize time to stop tracking */
        start_timer(&(satobject_p->track_stop_time),
                    digi_track_duration * TM_MIN);
        return;
    }

    /*
     * At this point the satellite is not in the list and 
     * we can add it.
     */

    /* reserve memory for satellite object */
    satobject_p = malloc(sizeof(satobject_t));
    
    /* if out of memory then ignore */
    if(satobject_p == NULL)
    {
        say("Out of memory while adding satellite object.\r\n");
        return;
    }
    
    /* store the amsat designator of the requested satellite */
    strcpy(satobject_p->sat, sat);

    /* store the port number from which the request came from */
    satobject_p->port = uidata_p->port;

    /* indicate whether or not satellite needs to be tracked */
    satobject_p->track = track;
    
    /*
     * set the interval at which the first tracking object transmission
     * needs to take place
     *
     * transmission of messages takes place at t+15, t+25 etc (see message.c)
     *
     * interleave by having the satellites send at t+10 t+20 etc
     *
     * (when requesting multiple satellites interval has the value 1 for
     * the first satellite, 2 for the second satellite etc.).
     */
    satobject_p->interval = 10 * interval;

    /* Determine the next aos. */
    (void) get_aos(sat, &nextaos_l, 0);

    /* Let the timer expire at this time */
    start_utc_timer(&(satobject_p->nextaos), nextaos_l);

    /* start interval-time seconds from now */
    start_timer(&(satobject_p->timer), satobject_p->interval * TM_SEC);

    /* set time to stop tracking */
    start_timer(&(satobject_p->track_stop_time), digi_track_duration * TM_MIN);

    /* add new object in front of the list*/
    satobject_p->next = satobject_list_p;
    satobject_list_p = satobject_p;
}

/*
 * Do satellite command, see if there is any transmission to be done at this
 * time, if there is, do the transmission and restart the timer for the
 * next time.
 */
void do_satellite(void)
{
    satobject_t     *satobject_p;
    satobject_t     *previous_satobject_p;
    short           sat_object_result;
    short           longpass;
    char            satobject[MAX_MESSAGE_LENGTH+1];
    unsigned long   nextaos_l;
    
    /*
     * walk through the satobject list and start transmission if
     * needed
     */
    satobject_p = satobject_list_p;

    /*
     * While there are still satobjects to consider
     */
    while(satobject_p != NULL)
    {
        if( (satobject_p->track == 0)
            ||
            (is_elapsed(&(satobject_p->track_stop_time)) != 0)
          )
        {
            /*
             * If satellite object does not need to be tracked, 
             * or if maximum track time is expired,
             * then transmit once and delete from object list.
             */

            /* 
             * If tracking time expired, then indicate this on the 
             * activity list.
             */

            if(is_elapsed(&(satobject_p->track_stop_time)) != 0)
            {
                asay("Maximum track time reached for %s. Tracking stopped.\r\n", satobject_p->sat);
            }

            /* Get APRS satellite object. */
            sat_object_result = get_sat_object(satobject_p->sat, digi_use_local, satobject, digi_dx_metric, &longpass);

            /* transmit the satellite object */
            transmit_satobject(satobject, satobject_p->port);

            /*
             * no tracking needed, one time transmit only,
             * so remove object from list
             */
            previous_satobject_p = satobject_p;
            satobject_p = previous_satobject_p->next;
            remove_satobject(previous_satobject_p->sat, previous_satobject_p->port, previous_satobject_p->track);
        }
        else
        {
            /*
             * transmit satellite object if interval time has elapsed or
             * if the satellite came in range.
             */

            if( (is_elapsed(&(satobject_p->timer)) != 0)
                ||
                (is_elapsed(&(satobject_p->nextaos)) != 0)
              )
            {
                /* Get APRS satellite object. */
                sat_object_result = get_sat_object(satobject_p->sat, digi_use_local, satobject, digi_dx_metric, &longpass);

                /* transmit the satellite object */
                transmit_satobject(satobject, satobject_p->port);

                /* if the satellite is in range, then determine the next aos. */
                if(is_elapsed(&(satobject_p->nextaos)) != 0)
                {
                    /* indicate that the satellite is now in range */
                    asay("%s is now in range.\r\n", satobject_p->sat);

                    /* Determine the next aos. */
                    (void)get_aos(satobject_p->sat, &nextaos_l, 1);

                    /* Let the timer expire at this time */
                    start_utc_timer(&(satobject_p->nextaos), nextaos_l);
                }

                /*
                 * if the satellite is in range and it's a short pass,
                 * then transmit every sat_in_range_interval,
                 * else transmit every sat_out_of_range_interval.
                 */
                if((sat_object_result == 4) && (longpass == 0))
                {
                    /* start timer for next transmission */
                    satobject_p->interval = digi_in_range_interval;
                    start_timer(&(satobject_p->timer), satobject_p->interval * TM_MIN);
                }
                else
                {
                    /* start timer for next transmission */
                    satobject_p->interval = digi_out_of_range_interval;
                    start_timer(&(satobject_p->timer), satobject_p->interval * TM_MIN);
                }
            }
            /* advance to next satellite object */
            satobject_p = satobject_p->next;
        }
    }
}
#endif /* _SATTRACKER_ */
