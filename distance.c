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
#include "digicons.h"
#include "digitype.h"
#include "distance.h"

/*
 * Convert 2 position plots to distance and bearing
 * 
 * The first parameter is my position, the second the spot towards which
 * distance and bearing shall be calculated.
 *
 * returns bearing in a short addressed by bearing_p and distance
 *         in hecto-meters, 10ths of statue miles or 10ths of nautical
 *         miles (detemined by metric) as return value.
 */
long dist_distance_and_bearing(position_t *mypos_p, position_t *otherpos_p,
                             short *bearing_p, char *metric)
{
    double A, B, L, L1, L2, D, C;
    double sinA, cosA, sinB, cosB, sinL, cosL, sinD, cosD, cosC;
    double tmp;

    /* 
     * From the 1982 ARRL antenna book.
     *
     * 1.  cos(D) = (sin(A) * sin(B)) + (cos(A) * cos(B) * cos(L))
     *
     * 2.  cos(C) = (sin(B) - (sin(A) * cos(D))) / (cos(A) * sin(D))
     *
     * WHERE:
     *
     * A = YOUR latitude in degrees.
     * B = latitude of the other location in degrees.
     * L = YOUR longitude minus that of the other location.
     *     (Algebraic difference.)
     * D = Distance along path in degrees of arc.
     * C = True bearing from north if the value for sin(L) is positive.
     *     If sin(L) is negative, true bearing is 360 - C.
     *
     * Not that this calculation is in degrees but we have to use radials
     * The distance is also in radials.
     *
     * One gotcha:
     *
     * The lattitudes used in this formula are positive for West of
     * Greenwich, but in our case these are negative. So we need to do
     * swap L1 and L2 in the subtraction for L to get it right...
     */
   
    /*
     * Great Circle calculation.
     *
     * Convert X in hundredths of minutes to radials since that it 
     * what the "C" math library uses.
     *
     * Formula: ((X / 6000.0) / 180.0) * M_PI, but the divisions are
     * combined. This gives: (X / 1080000) * M_PI;
     */

    /* A and L1 are used for my position */
    A  = (((double) mypos_p->lat) / 1080000.0) * M_PI;
    L1 = (((double) mypos_p->lon) / 1080000.0) * M_PI;

    /* B and L2 are used for the other position */
    B  = (((double) otherpos_p->lat) / 1080000.0) * M_PI;
    L2 = (((double) otherpos_p->lon) / 1080000.0) * M_PI;

    /* L is the difference between L2 and L1 */
    L = L2 - L1;

    /*
     * The positions are is in radials now, suitable for the math
     * libraries sin cos and acos functions. Lets do the calculation
     *
     * Calculate the geometric values we need for the distance and
     * bearing functions
     */
    sinA = sin(A);
    cosA = cos(A);
    sinB = sin(B);
    cosB = cos(B);
    sinL = sin(L);
    cosL = cos(L);

    /*
     * Calculate cosD, the cosinus of the distance-arc
     *
     * 1.  cos(D) = (sin(A) * sin(B)) + (cos(A) * cos(B) * cos(L))
     */
    cosD = (sinA * sinB) + (cosA * cosB * cosL);

    /*
     * cosD should never become bigger than 1 or smaller than -1. Trap here
     * if it is bigger or smaller to avoid math errors. (apply paranoia
     * mode coding style, this might be possible due to rounding errors)
     */
    if(cosD >  1.0) cosD =  1.0;
    if(cosD < -1.0) cosD = -1.0;

    /* calculate D (in radials) */
    D = acos(cosD);

    /* we need sinD for the bearing function */
    sinD = sin(D);

    /*
     * Answer D is in radials now, convert back to hundereths of minutes.
     * Multiply by 180 * 6000 = 1080000 (recognize the number? see above!).
     */
    D = (D * 1080000.0) / M_PI; 

    /*
     * Convert the answer to to correct metric
     *
     * Since minutes of arc and NM are the same, the calculated vector
     * in now in hundereths of nautical miles.
     */
    if(strcmp(metric, "km") == 0)
    {
        /*
         * Convert to tenths of km; i.e. hectometers. D is now in
         * hundereths of miles.
         *
         * One NM = 1.852 km. (10 hm = 1 km)
         *
         * To do this is the following conversions need to take place.
         * Devide by 100 to get NM
         * Multiply by 1.852 to get km.
         * multiply by 10 to get hecto meters.
         *
         * This gives a correction factor K which is 0.1852 to get an
         * answer in hectometers
         */
        D = D * 0.1852;
    }
    else if(strcmp(metric, "mi") == 0)
    {
        /*
         * Convert to tenths of miles; D is now in hundereths of nautical
         * miles.
         *
         * One NM = 1.15078 miles.
         *
         * To do this is the following conversions need to take place.
         * Devide by 100 to get NM
         * Multiply by 1.15078 to get miles.
         * multiply by 10 to get tenths of miles.
         *
         * This gives a correction factor K which is 0.115078 to get an
         * answer in tenths of miles.
         */
        D = D * 0.115078;
    }
    else if(strcmp(metric, "nm") == 0)
    {
        /*
         * Convert to tenths of mi; D is now in hundereths of nautical
         * miles.
         *
         * Just needs multiplication by 0.1!
         */
        D = D * 0.1;
    }
    else
    {
        /* No idea what this is, I just convert to and ISO standard value */
        D = D * 0.1852;
    }

    /*
     * Now caluclate cosC, the cosinus of the bearing:
     *
     * 2.  cos(C) = (sin(B) - (sin(A) * cos(D))) / (cos(A) * sin(D))
     */
    /*
     * We do this in two steps to avoid division by zero problems.
     *
     * This happens when you are at the North or South-pole or when
     * The distance is 0. If zero or near zero we do therefore some
     * special handling, avoid very large numbers that do no makes
     * sense anyway.
     */
    tmp = (cosA * sinD);
    if(tmp > 0.0000001)
    {
        cosC = (sinB - (sinA * cosD)) / tmp; 
    }
    else
    {
        if(D < 0.01)
        {
            /*
             * Within 1 meter of your station!
             *
             * You are calculation the bearing with youself
             * Just say North (must be something...)
             */
            cosC = 1; 
        }
        else
        {
            /*
             * You are on one of the north-poles or exactly on the other
             * side of the globe. If you are above the equator then point
             * south, if you are below the equator point North.
             */    
            if(A > 0.0)
            {
                /* Above the equator - set bearing to South */
                cosC = -1;
            }
            else
            {
                /* Below the equator - set bearing to North */
                cosC = 1;
            }
        }
    }

    /*
     * cosC should never become bigger than 1 or smaller than -1. Trap here
     * if it is bigger or smaller to avoid math errors. (apply paranoia
     * mode coding style, this might be possible due to rounding errors)
     */
    if(cosC >  1.0) cosC =  1.0;
    if(cosC < -1.0) cosC = -1.0;

    /* calculate C (in radials) */
    C = acos(cosC);

    /*
     * Convert C in radials to C in degrees
     */
    C = (short) ((C * 180) / M_PI);

    /* if sinL is positive then bearing = 360 - C */
    if(sinL < 0)
    {
        C = 360.0 - C;
    }

    /*
     * store (rounded) bearing
     */
    *bearing_p = (short) (C + 0.5);

    /*
     * Avoid bearings of 360 degrees due to rounding, make them 0
     */
    if(*bearing_p == 360)
    {
        *bearing_p = 0;
    }

    /*
     * return distance
     */
    return (long) D;
}

/*
 * Convert an ASCII position string to decimal hundredths of degrees
 *
 * returns: position in long pointed to by pos
 *          1 if conversion went okay
 *          0 if conversion failed
 */
short dist_ascii2long(const char *src, long *pos)
{
    char  degree[4];
    char  hundred[5];
    char *p;
    long  sign;

    /*
     * Position is either lattitude: 5213.61N
     *             or     longitude: 00600.00E
     * Spaces can be used for position ambiguity, will be replaced
     * by '5' to get to the middle of the ambiguity area.
     */
    /*
     * This is input from outside, do some sanity checks.
     *
     * This is also needed too weed out other types of APRS messages
     * that uses the same identifier, i.e. raw weather data.
     */
    switch(strlen(src)) {
    case 8:
            /* lattitude */
            /* check some placeholders */
            if((src[4] != '.') || ((src[7] != 'N') && (src[7] != 'S')))
            {
                /* expected '.' and 'N' or 'S' at specific places */
                return 0;
            }
            /* check digits, allow space for posititon ambiguity */
            if( (strspn(src, "0123456789 ") != 4)
                ||
                (strspn(&src[5], "0123456789 ") != 2)
              )
            {
                /* illegal character in lattitude */
                return 0;
            }
            /* replace spaces with '5' */
            while((p = strchr(src,' ')) != NULL)
            {
                *p = '5';
            }
            degree[0] = src[0];
            degree[1] = src[1];
            degree[2] = '\0';
            hundred[0] = src[2];
            hundred[1] = src[3];
            /* src[4] is the '.' */
            hundred[2] = src[5];
            hundred[3] = src[6];
            hundred[4] = '\0';
            if(src[7] == 'N')
            {
                sign = 1;
            }
            else
            {
                sign = -1;
            }
            break;
    case 9:
            /* longitude */
            /* check some placeholders */
            if((src[5] != '.') || ((src[8] != 'E') && (src[8] != 'W')))
            {
                /* expected '.' and 'E' or 'W' at specific places */
                return 0;
            }
            /* check digits, allow space for posititon ambiguity */
            if( (strspn(src, "0123456789") != 5)
                ||
                (strspn(&src[6], "0123456789") != 2)
              )
            {
                /* illegal character in longitude */
                return 0;
            }
            /* replace spaces with '5' */
            while((p = strchr(src,' ')) != NULL)
            {
                *p = '5';;
            }
            degree[0] = src[0];
            degree[1] = src[1];
            degree[2] = src[2];
            degree[3] = '\0';
            hundred[0] = src[3];
            hundred[1] = src[4];
            /* src[5] is the '.' */
            hundred[2] = src[6];
            hundred[3] = src[7];
            hundred[4] = '\0';
            if(src[8] == 'E')
            {
                sign = 1;
            }
            else
            {
                sign = -1;
            }
            break;
    default:
            /* invalid length */
            return 0;
    }

    *pos = ((6000 * strtol(degree, NULL, 10)) + strtol(hundred, NULL, 10));
    *pos = *pos * sign;

    return 1;
}

/*
 * Convert a compressed position string to decimal hundredths of degrees
 *
 * returns: position in long pointed to by pos
 *          1 if conversion went okay
 *          0 if conversion failed
 */
static short dist_compressed2long(const char *src, long *pos, latlon_t latlon)
{
    int    i;
    long   comp[4];
    long   val;
    double position;

    /*
     * Position is compressed in 4 digits.
     *
     * This is input from outside, do some sanity checks.
     *
     * This is also needed too weed out other types of APRS messages
     * that uses the same identifier, i.e. raw weather data.
     */
    if(strlen(src) != 4)
    {
        /* invalid length */
        return 0;
    }

    /*
     * Check if all characters are valid base 91 characters
     * (character codes from 33..124)
     */
    for(i = 0; i < 4; i++)
    {
        /*
         * Lower the character value by 33 and put it in the long array
         * 'comp'
         */
        comp[i] = (long) (src[i] - 33);

        /*
         * Check if it is base 91, i.e. value between 0..90
         */
        if( (comp[i] < 0L) || (comp[i] > 90L) )
        {
            /* invalid value */
            return 0;
        }
    }

    /*
     * Convert base 91 number to base 10 value
     *
     * The constants used here are:
     *
     * 91**3 = 753571
     * 91**2 = 8281
     * 91**1 = 91
     *
     * The maximum value that can come out of this is 68574960 which
     * easily fits into a long value.
     */
    val = (comp[0] * 753571L) + (comp[1] * 8281L) + (comp[2] * 91L) + comp[3];

    /*
     * Unfortunatly we have to do some floating point here...
     */
    if(latlon == LAT)
    {
        /* lattitude calculation */
        /*
         * Magic numbers 90.0 and 380926.0 are from the APRS Specification
         * revision 1.0
         */
        position = 90.0 - (val / 380926.0);
        if( (position < -90.0) || (position > 90.0) )
        {
            /* overflow, invalid position */
            return 0;
        }
    }
    else
    {
        /* longitude calculation */
        /*
         * Magic numbers -180.0 and 190463.0 are from the APRS Specification
         * revision 1.0
         */
        position = -180 + (val / 190463.0);
        if( (position < -180.0) || (position > 180.0) )
        {
            /* overflow, invalid position */
            return 0;
        }
    }

    /* convert to hundereths of minutes, round and drop fraction */
    *pos = (long) ((position * 6000.0) + 0.5);

    return 1;
}

/*
 * Convert MIC-E destination character to digit.
 *
 * returns: lattitude character derrived from destination call character
 */
static char dist_mice_dest2digit(char ch)
{
    if((ch >= '0') && (ch <= '9'))
    {
        /* return character as is */
        return ch;
    }

    if((ch >= 'A') && (ch <= 'J'))
    {
        /* return character minus offset between 'A' and '0' */
        return ch - ('A' - '0');
    }

    if((ch >= 'P') && (ch <= 'Y'))
    {
        /* return character minus offset between 'P' and '0' */
        return ch - ('P' - '0');
    }

    /* left-over: 'K', 'L' and 'Z', all converted to space */
    return ' ';
}

/*
 * Convert an UI frame with APRS data to position data
 *
 * returns: position in data areas referenced by "lat_p" and "lon_p"
 *          1 if data could be converted
 *          0 if data could not be converted (no position information)
 */
static short dist_mice2position(uidata_t *uidata_p, char *lat_p, char *lon_p)
{
    char  *p;
    short  i;
    short  j;
    short  twodigit;

    /*
     * Recover lattitude from destination address
     */
    p = &(uidata_p->destination[0]);

    /*
     * Check for incomplete destination code, only the SSID is optional
     * the 6 digits of the call are completely in use for MIC-E
     *
     * Only the mentioned digits shall appear in MIC-E destination calls
     * (this sould always deliver 6, the 7th byte, if present, will be
     * a '-' which is not included in the set which is tested)
     */
    if(strspn(p,"0123456789ABCDEFGHIJKLPQRSTUVWXYZ") != 6)
    {
        /* incomplete or wrong destination call */
        return 0;
    }

    /*
     * On the last 3 digits of the destination address characters
     * 'A'..'K' are not allowed.
     */
    for(i = 3; i < 6; i++)
    {
        if((p[i] >= 'A') && (p[i] <= 'K'))
        {
            /* Illigal character on this position */
            return 0; 
        }
    }

    /*
     * Copy the longitude from the destination to the lat_p array
     */
    j = 0;
    for(i = 0; i < 6; i++)
    {
        lat_p[j++] = dist_mice_dest2digit(p[i]);
        if(j == 4)
        {
            /* patch a '.' at the 5rd position (i.e. at index 4) */
            lat_p[j++] = '.';
        }
    }
    
    /*
     * All codes below 'P' are South
     */
    if(p[3] < 'P')
    {
        /* Below 'P', this is South */
        lat_p[7] = 'S';
    }
    else
    {
        /* Above or equal to 'P', this is North */
        lat_p[7] = 'N';
    }
    
    /*
     * Terminate with '\0'
     */
    lat_p[8] = '\0';

    /*
     * now recover longitude
     */

    /*
     * All codes above or equal to 'P' are +100
     */
    if(p[4] < 'P')
    {
        /* Below 'P', this is below 100 */
        lon_p[0] = '0';
    }
    else
    {
        /*
         * Above or equal to 'P', this is above 100
         *
         * <note that below this might be changed to '0' again for
         * the 0..9 degrees special case>
         */
        lon_p[0] = '1';
    }

    /*
     * All codes below 'P' are East
     */
    if(p[5] < 'P')
    {
        /* Below 'P', this is East */
        lon_p[8] = 'E';
    }
    else
    {
        /* Above or equal to 'P', this is West */
        lon_p[8] = 'W';
    }

    /*
     * The rest of the longitude digits are from the payload
     * They are base 100 coded (2 digits in each character)
     */
    p = &(uidata_p->data[0]);

    /*
     * Data at index 1,2 and 3 contains the longitude
     *
     * Recover one by one
     */
    twodigit = ((short) p[1]) - 28;
    if((twodigit < 10) || (twodigit > 99))
    {
        /* out of range for first longitude digit */
        return 0;
    }

    /*
     * Special case, if twodigit > 89 but +100 is set it means 0..9
     */
    if((twodigit > 89) && (lon_p[0] == '1'))
    {
        twodigit = twodigit - 90; /* modify to 0..9        */
        lon_p[0] = '0';           /* remove +100 indicator */
    }

    /*
     * Store the two degree digits
     */
    lon_p[1] = (twodigit / 10) + '0';
    lon_p[2] = (twodigit % 10) + '0';

    /*
     * take the next two digits
     */
    twodigit = ((short) p[2]) - 28;
    if((twodigit < 10) || (twodigit > 69))
    {
        /* out of range for second longitude digit */
        return 0;
    }

    /*
     * Special case, if twodigit > 59 it means 0..9
     */
    if(twodigit > 59)
    {
        twodigit = twodigit - 60; /* modify to 0..9        */
    }

    /*
     * Store the two degree digits
     */
    lon_p[3] = (twodigit / 10) + '0';
    lon_p[4] = (twodigit % 10) + '0';

    /*
     * patch a '.' at the 6rd position (i.e. at index 5)
     */
    lon_p[5] = '.';

    /*
     * take the last two digits
     */
    twodigit = ((short) p[3]) - 28;
    if((twodigit < 0) || (twodigit > 99))
    {
        /* out of range for third longitude digit */
        return 0;
    }

    /*
     * Store the two degree digits
     */
    lon_p[6] = (twodigit / 10) + '0';
    lon_p[7] = (twodigit % 10) + '0';

    /*
     * Terminate with '\0'
     */
    lon_p[9] = '\0';

    return 1;
}

/*
 * Convert data with a Maidenhead grid to position data
 *
 * returns: position in structure addressed by pos_p
 *          1 if the gird could be converted
 *          0 if the gird could not be converted (no position information)
 */
static short dist_grid2pos(char *maid, position_t *pos_p)
{
    long  lat;
    long  lon;

    if(maid == NULL)
    {
        /* maidenhead NULL pointer */
        return 0;
    }

    if(strlen(maid) < 8)
    {
        /* maidenhead too short to use */
        return 0;
    }

    if(maid[0] != '[')
    {
        /* locator should be enclosed in [] */
        return 0;
    }

    if((maid[1] < 'A') || (maid[1] > 'S'))
    {
        /* first letter A..S out of range */
        return 0;
    }

    if((maid[2] < 'A') || (maid[2] > 'S'))
    {
        /* second letter A..S out of range */
        return 0;
    }

    if((maid[3] < '0') || (maid[3] > '9'))
    {
        /* third digit 0..9 out of range */
        return 0;
    }

    if((maid[4] < '0') || (maid[4] > '9'))
    {
        /* fourth digit 0..9 out of range */
        return 0;
    }

    if((maid[5] < 'A') || (maid[5] > 'X'))
    {
        /* fourth letter A..X out of range */
        return 0;
    }

    if((maid[6] < 'A') || (maid[6] > 'X'))
    {
        /* fifth letter A..X out of range */
        return 0;
    }

    if(maid[7] != ']')
    {
        /* locator should be enclosed in [] */
        return 0;
    }

    /* valid locator */

    lon = ((long) (maid[1] - 'A')) * 120000L;
    lat = ((long) (maid[2] - 'A')) * 60000L;

    lon = lon + ((long) (maid[3] - '0')) * 12000L;
    lat = lat + ((long) (maid[4] - '0')) * 6000L;

    lon = lon + ((long) (maid[5] - 'A')) * 500L;
    lat = lat + ((long) (maid[6] - 'A')) * 250L;

    pos_p->lon = lon - 1080000L + 250L; /* add rounding */
    pos_p->lat = lat - 540000L  + 125L; /* add rounding */

    return 1;
}

/*
 * Convert an UI frame with APRS data to position data
 *
 * returns: position in structure addressed by pos_p
 *          1 if data could be converted
 *          0 if data could not be converted (no position information)
 */
short dist_aprs2position(uidata_t *uidata_p, position_t *pos_p)
{

    char       sum_string[5];
    short      sum;
    char       lat [9];
    char       lon [10];
    short      res;
    short      compressed;
    char      *lat_p;
    char      *lon_p;
    char      *p,*q;
    short      i;

    /*
     * Assume not compressed data
     */
    compressed = 0;

    /*
     * we initialily use some low indexes of the data array. As soon as
     * more is known another length check will be done for the found
     * format
     *
     * The highest index used for checking is 8 which means that there should
     * be at least 9 characters in the string.
     */
    if(uidata_p->size < 9)
    {
        return 0;
    }

    /*
     * Add '\0' terminator at the end of the data (just outside the "real"
     * data so it does not change the contents mind you!)
     */
    uidata_p->data[uidata_p->size] = '\0';

    /*
     * Check APRS data type. Note that this skips third party messages
     * they are not handled since we are only interested in the RF
     * transmitted distances and not those via some gate.
     */
    switch(uidata_p->data[0]) {
    case '!':   /* fall through */
    case '=': 
                /* Check destination, should start with "AP" */
                if(strncmp(uidata_p->destination, "AP", 2) != 0)
                {
                    return 0;
                }

                if(uidata_p->data[1] == '!')
                {
                    /*
                     * An Ultimeter station string starts with !! but is
                     * not a position. '!' is not a valid character for
                     * the symboltable identifier anyway.
                     */
                    return 0;
                }

                if( (uidata_p->data[1] >= '0')
                    &&
                    (uidata_p->data[1] <= '9')
                  )
                {
                    /*
                     * uncompressed format:
                     * 0123456789012345678901234567890123456789
                     * !4903.50N/07201.75W-Test 01234
                     * =4903.50N/07201.75W-Test 01234
                     *
                     * The string should be at least 20 characters
                     */
                    if(uidata_p->size < 20)
                    {
                        return 0;
                    }
                    strncpy(lat, &(uidata_p->data[1]), 8);
                    lat[8]  = '\0';
                    strncpy(lon, &(uidata_p->data[10]), 9);
                    lon[9] = '\0';
                }
                else
                {
                    /*
                     * compressed format:
                     * 0123456789012345678901234567890123456789
                     * !/5L!!<*e7>7P[Test 01234
                     * =/5L!!<*e7>7P[Test 01234
                     *
                     * The string should be at least 14 characters
                     */
                    if(uidata_p->size < 14)
                    {
                        return 0;
                    }
                    strncpy(lat, &(uidata_p->data[2]), 4);
                    lat[4]  = '\0';
                    strncpy(lon, &(uidata_p->data[6]), 4);
                    lon[4]  = '\0';
                    compressed = 1;
                }
                break;
    case '/':   /* fall through */
    case '@': 
                /* Check destination, should start with "AP" */
                if(strncmp(uidata_p->destination, "AP", 2) != 0)
                {
                    return 0;
                }

                if( (uidata_p->data[8] >= '0')
                    &&
                    (uidata_p->data[8] <= '9')
                  )
                {
                    /*
                     * uncompressed format:
                     * 0123456789012345678901234567890123456789
                     * /092345z4903.50N/07201.75W>Test 01234
                     * @092345z4903.50N/07201.75W>Test 01234
                     *
                     * The string should be at least 27 characters
                     */
                    if(uidata_p->size < 27)
                    {
                        return 0;
                    }
                    strncpy(lat, &(uidata_p->data[8]), 8);
                    lat[8]  = '\0';
                    strncpy(lon, &(uidata_p->data[17]), 9);
                    lon[9] = '\0';
                }
                else
                {
                    /*
                     * compressed format:
                     * 0123456789012345678901234567890123456789
                     * /092345z/5L!!<*e7>{?!Test 01234
                     * @092345z/5L!!<*e7>{?!Test 01234
                     *
                     * The string should be at least 21 characters
                     */
                    if(uidata_p->size < 21)
                    {
                        return 0;
                    }
                    strncpy(lat, &(uidata_p->data[9]), 4);
                    lat[4]  = '\0';
                    strncpy(lon, &(uidata_p->data[13]), 4);
                    lon[4]  = '\0';
                    compressed = 1;
                }
                break;
    case 0x1c:  /* fall through */
    case 0x1d:  /* fall through */
    case '\'':  /* fall through */
    case '`':   /* MIC-E format: old, new, whatever */
                res = dist_mice2position(uidata_p, lat, lon);
                if(res == 0)
                {
                    /* Can't recover MIC-E data */
                    return 0;
                }
                /*
                 * Now lat and lon are not compressed anymore! do not
                 * set the compressed flag in this case
                 */
                break;
    case '$':   /*
                 * Raw data, possibly GPS data
                 */

                /*
                 * If there is a checksum, check it
                 */
                p = strchr(uidata_p->data, '*');
                if(p != NULL)
                {
                    /*
                     * There is a checksum. Start summing just after the
                     * '$' up to, but not including, the '*'
                     */
                    q = &(uidata_p->data[1]);

                    sum = 0;
                    while(q < p)
                    {
                        sum = sum ^ *q;
                        q++;
                    }

                    /*
                     * Convert our sum to string
                     */
                    sprintf(sum_string, "*%02X", sum);

                    /*
                     * Compare our string with the sum in the data
                     */
                    if(strncmp(sum_string, p, 3) != 0)
                    {
                        /*
                         * Checksum error, do not attempt to decode the
                         * position
                         */
                        return 0;
                    }
                }

                /* Initialize some variables */
                p = uidata_p->data;

                /* initialize to avoid warnings */
                lat_p = "";
                lon_p = "";

                if(strncmp("GPGGA", &(uidata_p->data[1]), 5) == 0)
                {
/* $GPGGA,190440,5213.6104,N,00600.0057,E,1,03,4.1,-9.3,M,46.7,M,,*6C */

                    /* Parse up to the data item following the longitude */
                    for(i = 1; i < 7; i++)
                    {
                        switch(i) {
                        case 3: lat_p = p;
                                break;
                        case 5: lon_p = p;
                                break;
                        }

                        /* skip to next item in the string */
                        p = strchr(p,',');

                        /* exit on failure strategy */
                        if(p == NULL)
                        {
                            return 0;
                        }
                        p++; /* p points to the data behind the comma */
                    }
                }
                else if(strncmp("GPRMC", &(uidata_p->data[1]), 5) == 0)
                {
/* $GPRMC,201626,A,5213.6001,N,00600.0196,E,0.0,197.9,250401,0.6,W,A*00 */

                    /* Parse up to the data item following the longitude */
                    for(i = 1; i < 8; i++)
                    {
                        switch(i) {
                        case 4: lat_p = p;
                                break;
                        case 6: lon_p = p;
                                break;
                        }

                        /* skip to next item in the string */
                        p = strchr(p,',');

                        /* exit on failure strategy */
                        if(p == NULL)
                        {
                            return 0;
                        }
                        p++; /* p points to the data behind the comma */
                    }
                }
                else if(strncmp("GPGLL", &(uidata_p->data[1]), 5) == 0)
                {
/* $GPGLL,5213.5909,N,00559.9998,E,214532,A,A*4C */

                    /* Parse up to the data item following the longitude */
                    for(i = 1; i < 6; i++)
                    {
                        switch(i) {
                        case 2: lat_p = p;
                                break;
                        case 4: lon_p = p;
                                break;
                        }

                        /* skip to next item in the string */
                        p = strchr(p,',');

                        /* exit on failure strategy */
                        if(p == NULL)
                        {
                            return 0;
                        }
                        p++; /* p points to the data behind the comma */
                    }
                }
                else
                {
                    /* not position information, failure */
                    return 0;
                }

                p = lat_p;
                
                for(i = 0; i < 7; i++)
                {
                    if(*p != ',')
                        lat[i] = *p++;
                    else
                        lat[i] = ' ';
                }
                /* Note check already done above... */
                p = strchr(p, ',');
                p++;
                lat[7] = *p;
                lat[8] = '\0';

                p = lon_p;
                
                for(i = 0; i < 8; i++)
                {
                    if(*p != ',')
                        lon[i] = *p++;
                    else
                        lon[i] = ' ';
                }
                /* Note check already done above... */
                p = strchr(p, ',');
                p++;
                lon[8] = *p;
                lon[9] = '\0';

                break;
    case '[':   /*
                 * Try if it is a maidenhead locator
                 */
                res = dist_grid2pos(uidata_p->data, pos_p);
                
                /*
                 * grid2pos returns 1 on success and 0 on failure, we
                 * can return that rightaway!
                 */
                return res;
    default:    /* not position information, failure */
                return 0;
    }

    if(compressed == 0)
    {
        res = dist_ascii2long(lat, &(pos_p->lat));
    }
    else
    {
        res = dist_compressed2long(lat, &(pos_p->lat), LAT);
    }

    if(res == 0)
    {
        /* Can't convert lattitude */
        return 0;
    }

    if(compressed == 0)
    {
        res = dist_ascii2long(lon, &(pos_p->lon));
    }
    else
    {
        res = dist_compressed2long(lon, &(pos_p->lon), LON);
    }

    if(res == 0)
    {
        /* Can't convert longitude */
        return 0;
    }

    return 1;
}

/*
 * Convert position to an ASCII string. "posstring" shall be at least
 * 19 charaters long to avoid buffer overflow.
 */
char *dist_pos2ascii(position_t *pos, char *posstring)
{
    long   n;
    short  tmp;
    short  lat_deg;
    short  lat_min;
    short  lat_hund;
    char   lat_place;
    short  lon_deg;
    short  lon_min;
    short  lon_hund;
    char   lon_place;

    n = pos->lat;
    if(n < 0)
    {
        n = -n;
        lat_place = 'S';
    }
    else
    {
        lat_place = 'N';
    }

    lat_deg  = (short) (n / 6000);
    tmp      = (short) (n % 6000);
    lat_min  = (short) (tmp / 100);
    lat_hund = (short) (tmp % 100);

    n = pos->lon;
    if(n < 0)
    {
        n = -n;
        lon_place = 'W';
    }
    else
    {
        lon_place = 'E';
    }

    lon_deg  = n / 6000;
    tmp      = n % 6000;
    lon_min  = tmp / 100;
    lon_hund = tmp % 100;

    /*
     * Size: "5213.61N 00600.00E" -> 19 including terminating 0
     */

    sprintf(posstring,"%02d%02d.%02d%c %03d%02d.%02d%c",
                       lat_deg, lat_min, lat_hund, lat_place,
                       lon_deg, lon_min, lon_hund, lon_place);

    return posstring;
}
