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

#include <time.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "timer.h"

/*
 * Returns time in seconds since 00:00:00 LOC, January 1, 1970
 *
 * When at statup the timezone information is adjusted after staring
 * the beacons (under DOS) the value returned by time() changes,
 * causing the timer functions to fail which causes the beacons to be
 * transmitted at the wrong time.
 *
 * To counteract the zone information change local-time is calculated.
 * This works because PC time is set to local time so in effect the
 * calculation of UTC time is exactly reversed giving the stable
 * local-time again. This dirty hack is only needed for DOS.
 */
static unsigned long my_time(void)
{
#ifndef _LINUX_
    unsigned long t;
    struct timeb  tptr;

    ftime(&tptr);

    /*
     * The Borland time functions calculated UTC from the local time
     * using the currenlty available timezone information. Since the
     * timezone information could be wrong we are reversing this
     * calculation. That way the timezone setting doesn't matter
     * to the timestamp.
     */
    t = tptr.time;

    t = t - 60 * tptr.timezone;

    /* When Daylight saving the local clock is advanced one hour extra */
    if(tptr.dstflag == 1)
    {
        t = t + 3600;
    }

    /* Return the local-time timestamp */
    return t;
#else
    /* Get the time */
    return time(NULL);
#endif
}

/*
 * Start timer to expire at a specific absolute time; time in UTC
 *
 * In DOS the UTC time is converted back to local-time to counteract
 * timezone problems. This dirty hack is only needed for DOS.
 */
void start_utc_timer(digi_timer_t *tm, unsigned long alarm_time)
{
#ifndef _LINUX_
    struct timeb  tptr;

    ftime(&tptr);

    /*
     * The Borland time functions calculated UTC from the local time
     * using the currenlty available timezone information. Since the
     * timezone information could be wrong we are reversing this
     * calculation. That way the timezone setting doesn't matter
     * to the timestamp.
     */
    alarm_time = alarm_time - 60 * tptr.timezone;

    /* When Daylight saving the local clock is advanced one hour extra */
    if(tptr.dstflag == 1)
    {
        alarm_time = alarm_time + 3600;
    }

    /* Set timestamp to calculated local-time */
    *tm = alarm_time;
#else
    /* Set timestamp directly to this time */
    *tm = alarm_time;
#endif
}

void start_timer(digi_timer_t *tm, signed short timer_time)
{
    /*
     * Now this is realy simple!
     */
    *tm = my_time() + (unsigned long) timer_time;
}

void clear_timer(digi_timer_t *tm)
{
    /*
     * Now this is realy simple, just set it to the current time!
     */
    *tm = my_time();
}

short is_elapsed(digi_timer_t *tm)
{
    unsigned long now;

    /*
     * Get current time
     */
    now = my_time();

    /*
     * Compare to see if we are passed the set time
     *
     * This will fail when 'now' passed '0' again, but then a restart of
     * the digi is all it needs. Hey, it is in 21xx when this will happen...
     */
    if(now < *tm)
    {
        /* Not expired */
        return 0;
    }
    else
    {
        /* Expired */
        return 1;
    }
}
