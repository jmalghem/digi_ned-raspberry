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
 *
 *  Further modifications by PE1DNN.
 */
#ifdef _SATTRACKER_
/****************************************************************************
 * Note:                                                                    *
 *       We would like to thank John Magliancane for the development of his *
 *       product Predict and for making it available through GPL. Without   *
 *       his this, the satellite module would not have been possible.       *
 *       You can find John's excellent satellite tracking program for DOS   *
 *       and Linux at:  http://www.qsl.net/kd2bd/predict.html               *
 *                                                                          *
 ****************************************************************************
 *         PREDICT: A satellite tracking/orbital prediction program         *
 *             Copyright John A. Magliacane, KD2BD 1991-2001                *
 *                      Project started: 26-May-1991                        *
 *                       Last update: 05-Jan-2001                           *
 ****************************************************************************/


#include <stdio.h>
#include <math.h>
#include <sys/timeb.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#ifndef PI
#define PI 3.141592653589793
#endif

#include "digicons.h"
#include "digitype.h"
#include "read_ini.h"
#include "genlib.h"
#include "keep.h"
#include "output.h"
#include "mheard.h"
#include "dx.h"
#include "distance.h"

/* Global Variables */

typedef struct sat_s {
    char name[25];
    char amsat_des[5];
    char uplink[8];
    char downlink[8];
    char mode[3];
    long catnum;
    short year;
    double refepoch;
    double incl;
    double raan;
    double eccn;
    double argper;
    double meanan;
    double meanmo;
    double drag;
    long orbitnum;
} sat_t;

typedef struct qth_s {
    char callsign[10];
    double stnlat;
    double stnlong;
    short stnalt;
} qth_t;

static sat_t sat[24];
static qth_t qth;

static short numsats = 0;

static char *tlefile;

static short object_format;

static unsigned short val[256];

static short   indx, ma256, iaz, iel, isplat, isplong;

static long    rv, irk;

static double
        age, daynum, epoch, sma, range, t1, se, e1, e2, n0, c[4][3],
        k2, s1, c1, l8, s9, c9, s8, c8, r9, z9, x9, y9, o, w, q, s0, s2,
        c2, q0, m, e,  s3, c3, c0, r3, m1, m5, x0, yzero, x1, yone, r,
        z1, g7, s7, c7, x,  y,  z, x5, y5, z5, z8, x8, y8, df, aostime,
        lostime, apogee, perigee, azimuth, ssplat, ssplong, elevation,
        vk, vm, rm, rk, ak, am, fk, fm, yr, TP=6.283185307179586,
        deg2rad=1.74532925199e-02, R0=6378.16, FF=3.35289186924e-03,
        KM=1.609344;

/*
 * This function returns the nearest integer of the input value.
 * The original Predict program uses this function which is now standard
 * in C99. This version of the function is only to facilitate compilers
 * that are not C99 compliant.
 */
static long my_rint(double to_rint)
{

    if (to_rint < 0.0)
        to_rint = to_rint - 0.5;
    else
        to_rint = to_rint + 0.5;

    return (long)to_rint;

}

#define SEC_IN_HOUR    (86400.0) /* number of seconds in an hour           */
#define DAYS1970TO1980 (3651.0)  /* nr of days between 01Jan70 and 31Dec79 */

/*
 * Coverts days since 31Dec79 00:00:00 UTC (daynum 0) to seconds since
 * 01Jan70 00:00:00 UTC (Unix epoch).
 */
static time_t Daynum2Time(double daynum)
{
    time_t t;

    t = (time_t) my_rint(SEC_IN_HOUR * (daynum + DAYS1970TO1980));

    return t;
}

static double Time2Daynum(time_t t)
{
    double d;

    d =((t / SEC_IN_HOUR) - DAYS1970TO1980);

    return d;
}

/*
 * This function returns a substring based on the starting and ending
 * positions provided.  It is used heavily in the AutoUpdate function
 * when parsing 2-line element data.
 */
static char *SubString(char *string, unsigned char start, unsigned char end)
{
    static char temp[80];
    unsigned char x,y;

    if (end>=start)
    {
        for (x=start, y=0; x<=end && string[x]!=0; x++)
            if (string[x]!=' ')
            {
                temp[y]=string[x];
                y++;
            }

        temp[y]=0;
        return temp;
    }
    else
        return NULL;
}

/*
 * This function copies elements of the string "source" bounded by "start"
 * and "end" into the string "destination".
 */
static void CopyString(char *source, char *destination, unsigned char start,
                       unsigned char end)
{
    unsigned char j, k=0;

    for (j=start; j<=end; j++)
        if (source[k]!=0)
        {
            destination[j]=source[k];
            k++;
        }
}

/*
 * This function scans line 1 and line 2 of a NASA 2-Line element
 * set and returns a 1 if the element set appears to be valid or
 * a 0 if it does not.  If the data survives this torture test,
 * it's a pretty safe bet we're looking at a valid 2-line
 * element set and not just some random garbage that might pass
 * as orbital data based on a simple checksum calculation alone.
 */
static char KepCheck(char *line1, char *line2)
{
    short    x;
    unsigned sum1, sum2;

    /* Compute checksum for each line */

    for (x=0, sum1=0, sum2=0; x<=67; sum1+=val[(short)line1[x]],
         sum2+=val[(short)line2[x]], x++);

    /* Perform a "torture test" on the data */
    /* Split in 2 so it is also accepted by TurboC++ 1.0 */

    x=(val[(short)line1[68]]^(sum1%10)) | (val[(short)line2[68]]^(sum2%10)) |
      (line1[0]^'1')  | (line1[1]^' ')  | (line1[7]^'U')  |
      (line1[8]^' ')  | (line1[17]^' ') | (line1[23]^'.') |
      (line1[32]^' ') | (line1[34]^'.') | (line1[43]^' ') |
      (line1[52]^' ') | (line1[61]^' ') | (line1[63]^' ') |
      (line2[0]^'2')  | (line2[1]^' ')  | (line2[7]^' ')  |
      (line2[11]^'.') | (line2[16]^' ') | (line2[20]^'.');
    x = x | (line2[25]^' ') | (line2[33]^' ') | (line2[37]^'.') |
      (line2[42]^' ') | (line2[46]^'.') | (line2[51]^' ') |
      (line2[54]^'.') | (line1[2]^line2[2]) | (line1[3]^line2[3]) |
      (line1[4]^line2[4]) | (line1[5]^line2[5]) | (line1[6]^line2[6]) |
      (isdigit(line1[68]) ? 0 : 1) | (isdigit(line2[68]) ? 0 : 1) |
      (isdigit(line1[18]) ? 0 : 1) | (isdigit(line1[19]) ? 0 : 1) |
      (isdigit(line2[31]) ? 0 : 1) | (isdigit(line2[32]) ? 0 : 1);

    return (x ? 0 : 1);
}

/*
 * This function reads the digi_sat_file (specified in *.ini) file
 * into memory.  Return values are as follows:
 *
 * 0 : No satellite file was loaded
 * 1 : Satellite file was loaded successfully
 */
static char ReadDataFiles(void)
{
    short x=0, y;
    FILE *fd;
    char  name[80], line0[80], line1[80], line2[80];

    fd = fopen(tlefile,"r");

    if (fd == NULL)
    {
        return 0;
    }

    while (x<24 && feof(fd)==0)
    {
        /* Read element set */

        if(fgets(line0,75,fd) == NULL)
        {   /* Reading failed, fill with dummy */
            strcpy(line0, "000000000000000000000000000000000000000000000000000000000000000000000000000");
        }
        if(fgets(line1,75,fd) == NULL)
        {   /* Reading failed, fill with dummy */
            strcpy(line1, "000000000000000000000000000000000000000000000000000000000000000000000000000");
        }
        if(fgets(line2,75,fd) == NULL)
        {   /* Reading failed, fill with dummy */
            strcpy(line2, "000000000000000000000000000000000000000000000000000000000000000000000000000");
        }

        if (KepCheck(line1,line2))
        {
            /* We found a valid TLE! */

            /* Add decimal to eccentricity value */

            line2[25]='.';

            /* The first 24 byte are reserved for the name. */

            strncpy(name, line0, 24);
            name[24] = 0;

            /* Some TLE sources left justify the sat
               name in a 24-byte field that is padded
               with blanks.  The following lines cut
               out the blanks as well as the line feed
               character read by the fgets() function. */

            y=strlen(name);

            while ( name[y]==32 || name[y]==0 || 
                    name[y]==10 || name[y]==13 || y==0)
            {
                name[y]=0;
                y--;
            }

            /* Copy TLE data into the sat data structure */

            strncpy(sat[x].name,name,24);
            strcpy(sat[x].amsat_des, SubString(line0, 24, 27));
            strcpy(sat[x].uplink, SubString(line0, 29, 35));
            strcpy(sat[x].downlink, SubString(line0, 37, 43));
            strcpy(sat[x].mode, SubString(line0, 45, 46));
            sat[x].catnum=atol(SubString(line1,2,6));
            sat[x].year=atoi(SubString(line1,18,19));
            sat[x].refepoch=atof(SubString(line1,20,31));
            sat[x].incl=atof(SubString(line2,8,15));
            sat[x].raan=atof(SubString(line2,17,24));
            sat[x].eccn=atof(SubString(line2,25,32));
            sat[x].argper=atof(SubString(line2,34,41));
            sat[x].meanan=atof(SubString(line2,43,50));
            sat[x].meanmo=atof(SubString(line2,52,62));
            sat[x].drag=atof(SubString(line1,33,42));
            sat[x].orbitnum=atof(SubString(line2,63,67));
            x++;
        }
    }

    /* file read, close since it is no longer needed */
    fclose(fd);

    /* save the number of satellites that have been read */
    numsats = x;

    /* successfull read */
    return 1;
}

/* This function calculates the day number for the start of a year */
static long DayNumYear(short y)
{
    long   dn;
    double yy;
    double df1;
    double df2;
    double df3;

    /*
     * Leap year does not take effect at the start of the year. Do so a
     * simpler calculation use one year less and add a constant nr of days
     * to make up for the lost year
     */
    y--; 

    /* Correct for Y2K, this lasts up to 2079... */

    if (y < 79)
        y += 100;

    yy = (double)y;

    /* Base year is 1980, floor will have the effect of modulo 4 */
    df1 = floor(365.25 * (yy - 80.0));

    /* Every 100 year we loose 1 leapyear... */
    df2 = floor(yy / 100.0);

    /* ...except every 400 years, we gain 1 leapyear */
    df3 = floor(0.75 + yy / 400.0);

    /*
     * Add toghether, also add 366 days for the year we lost plus 1 day
     * round-down in the caluclation of df1.
     */
    dn=(long)(df1 - df2 + df3 + 366.0);

    return dn;
}

static double CurrentDaynum(void)
{
    /* Read the system clock and return the number
       of days since 31Dec79 00:00:00 UTC (daynum 0) */
    double local_clock;
    struct timeb tptr;

    ftime(&tptr);

    local_clock = Time2Daynum((double)tptr.time+0.001*(double)tptr.millitm);

    return local_clock;
}

static double PreCalc(double daynum)
{
    /* This function performs preliminary calculations
       prior to tracking or prediction. */

    epoch=DayNumYear(sat[indx].year)+sat[indx].refepoch;
    age=daynum-epoch;
    yr=(float)sat[indx].year;

    /* Do the Y2K thing... */

    if (yr<=50.0)
        yr+=100.0;

    t1=yr-1.0;
    df=366.0+floor(365.25*(t1-80.0))-floor(t1/100.0)+floor(t1/400.0+0.75);
    t1=(df+29218.5)/36525.0;
    t1=6.6460656+t1*(2400.051262+t1*2.581e-5);
    se=t1/24.0-yr;
    n0=sat[indx].meanmo+(age*sat[indx].drag);
    sma=331.25*pow((1440.0/n0),(2.0/3.0));
    e2=1.0-(sat[indx].eccn*sat[indx].eccn);
    e1=sqrt(e2);
    k2=9.95*(exp(log(R0/sma)*3.5))/e2*e2;
    s1=sin(sat[indx].incl*deg2rad);
    c1=cos(sat[indx].incl*deg2rad);
    l8=qth.stnlat*deg2rad;
    s9=sin(l8);
    c9=cos(l8);
    s8=sin(-qth.stnlong*deg2rad);
    c8=cos(qth.stnlong*deg2rad);
    r9=R0*(1.0+(FF/2.0)*(cos(2.0*l8)-1.0))+(qth.stnalt/1000.0);
    l8=atan((1.0-FF)*(1.0-FF)*s9/c9);
    z9=r9*sin(l8);
    x9=r9*cos(l8)*c8;
    y9=r9*cos(l8)*s8;
    apogee=sma*(1.0+sat[indx].eccn)-R0;
    perigee=sma*(1.0-sat[indx].eccn)-R0;

    return daynum;
}

static void Calc(void)
{
    /* This is the stuff we need to do repetitively. */

    age=daynum-epoch;
    o=deg2rad*(sat[indx].raan-(age)*k2*c1);
    s0=sin(o);
    c0=cos(o);
    w=deg2rad*(sat[indx].argper+(age)*k2*(2.5*c1*c1-0.5));
    s2=sin(w);
    c2=cos(w);
    c[1][1]=c2*c0-s2*s0*c1;
    c[1][2]=-s2*c0-c2*s0*c1;
    c[2][1]=c2*s0+s2*c0*c1;
    c[2][2]=-s2*s0+c2*c0*c1;
    c[3][1]=s2*s1;
    c[3][2]=c2*s1;
    q0=(sat[indx].meanan/360.0)+sat[indx].orbitnum;
    q=n0*age+q0;
    rv=(long)floor(q);
    q=q-floor(q);
    m=q*TP;
    e=m+sat[indx].eccn*(sin(m)+0.5*sat[indx].eccn*sin(m*2.0));

    do   /* Kepler's Equation */
    {
        s3=sin(e);
        c3=cos(e);
        r3=1.0-sat[indx].eccn*c3;
        m1=e-sat[indx].eccn*s3;
        m5=m1-m;
        e=e-m5/r3;

    } while (fabs(m5)>=1.0e-6);

    x0=sma*(c3-sat[indx].eccn);
    yzero=sma*e1*s3;
    r=sma*r3;
    x1=x0*c[1][1]+yzero*c[1][2];
    yone=x0*c[2][1]+yzero*c[2][2];
    z1=x0*c[3][1]+yzero*c[3][2];
    g7=(daynum-df)*1.0027379093+se;
    g7=TP*(g7-floor(g7));
    s7=-sin(g7);
    c7=cos(g7);
    x=x1*c7-yone*s7;
    y=x1*s7+yone*c7;
    z=z1;
    x5=x-x9;
    y5=y-y9;
    z5=z-z9;
    range=x5*x5+y5*y5+z5*z5;
    z8=x5*c8*c9+y5*s8*c9+z5*s9;
    x8=-x5*c8*s9-y5*s8*s9+z5*c9;
    y8=y5*c8-x5*s8;
    ak=r-R0;
    elevation=atan(z8/sqrt(range-z8*z8))/deg2rad;
    azimuth=atan(y8/x8)/deg2rad;

    if (x8<0.0)
        azimuth+=180.0;

    if (azimuth<0.0)
        azimuth+=360.0;

    ma256=(short)256.0*q;

    am=ak/KM;
    rk=sqrt(range);
    rm=rk/KM;
    vk=3.6*sqrt(3.98652e+14*((2.0/(r*1000.0))-1.0/(sma*1000.0)));
    vm=vk/KM;
    fk=12756.33*acos(R0/r);
    fm=fk/KM;
    ssplat=atan(z/sqrt(r*r-z*z))/deg2rad;
    ssplong=-atan(y/x)/deg2rad;

    if (x<0.0)
        ssplong+=180.0;

    if (ssplong<0.0)
        ssplong+=360.0;

    irk=(long)my_rint(rk);
    isplat=(short)my_rint(ssplat);
    isplong=(short)my_rint(ssplong);
    iaz=(short)my_rint(azimuth);
    iel=(short)my_rint(elevation);


}

static char AosHappens(short x)
{
    /* This function returns a 1 if the satellite pointed to by
       "x" can ever rise above the horizon of the ground station. */

    double lin, sma, apogee;

    lin=sat[x].incl;

    if (lin >= 90.0)
        lin=180.0-lin;

    sma=331.25*exp(log(1440.0/sat[x].meanmo)*(2.0/3.0));
    apogee=sma*(1.0+sat[x].eccn)-R0;

    if ((acos(R0/(apogee+R0))+(lin*deg2rad)) > fabs(qth.stnlat*deg2rad))
        return 1;
    else
        return 0;
}

static char Decayed(short x)
{
    /* This function returns a 1 if it appears that the
       satellite pointed to by 'x' has decayed at the
       time of 'daynum' */

    double satepoch;

    satepoch=DayNumYear(sat[x].year)+sat[x].refepoch;

    if (satepoch+((16.666666-sat[x].meanmo)/(10.0*fabs(sat[x].drag))) < daynum)
        return 1;
    else
        return 0;
}

static char Geostationary(short x)
{
    /* This function returns a 1 if the satellite pointed
       to by "x" appears to be in a geostationary orbit */

    if (fabs(sat[x].meanmo-1.0027)<0.0002)
        return 1;
    else
        return 0;
}

static double FindAOS(void)
{
    /* This function finds and returns the time of AOS (aostime). */

    aostime=0.0;

    if (AosHappens(indx) && Geostationary(indx)==0 && Decayed(indx)==0)
    {
        Calc();

        /* Get the satellite in range */

        while (elevation < -1.0)
        {
            daynum-=0.00035*(elevation*(((ak/8400.0)+0.46))-2.0);

            /* Technically, this should be:

               daynum-=0.0007*(elevation*(((ak/8400.0)+0.46))-2.0);

               but it sometimes skipped passes for
               satellites in highly eccentric orbits. */

            Calc();
        }

        /* Find AOS */

        /** Users using Keplerian data to track the Sun MAY find
            this section goes into an infinite loop when tracking
            the Sun if their QTH is below 30 deg N! **/

        while (aostime==0.0)
        {
            if (fabs(elevation) < 0.03)
                aostime=daynum;
            else
            {
                daynum-=elevation*sqrt(ak)/530000.0;
                Calc();
            }
        }
    }

    return aostime;
}

static double FindLOS(void)
{
    lostime=0.0;

    if (Geostationary(indx)==0 && AosHappens(indx)==1 && Decayed(indx)==0)
    {
        Calc();

        do
        {
            daynum+=elevation*sqrt(ak)/502500.0;
            Calc();

            if (fabs(elevation) < 0.03)
                lostime=daynum;

        } while (lostime==0.0);
    }

    return lostime;
}

static double FindLOS2(void)
{
    /* This function steps through the pass to find LOS.
       FindLOS() is called to "fine tune" and return the result. */

    do
    {
        daynum+=cos((elevation-1.0)*deg2rad)*sqrt(ak)/25000.0;
        Calc();

    } while (elevation>=0.0);

    return(FindLOS());
}

static double NextAOS(void)
{
    /* This function finds and returns the time of the next
       AOS for a satellite that is currently in range. */

    aostime=0.0;

    if (AosHappens(indx) && Geostationary(indx)==0 && Decayed(indx)==0)
        daynum=FindLOS2()+0.014;  /* Move to LOS + 20 minutes */

    return (FindAOS());
}


static void SaveTLE(void)
{
    FILE *fd;
    short x, y, sum;
    short spaces;
    short nspaces;
    char filler_s1[25];
    char filler_s2[25];
    char filler_s3[25];
    char line1[80], line2[80], string[20];

    /* Save orbital data in the form of NASA TLEs */

    fd = fopen(tlefile, "w");

    for (y=0; y<24; y++)
    {
        /* Fill lines with blanks */

        for (x=0; x<70; line1[x]=32, line2[x]=32, x++);

        line1[69]=0;
        line2[69]=0;

        /* Insert static characters */

        line1[0]='1';
        line1[7]='U';
        line2[0]='2';

        line1[51]='0';
        line1[62]='0';

        strcpy(string,"00000-0");
        CopyString(string,line1,54,60);
        CopyString(string,line1,45,51);

        /* Insert orbital data */

        sprintf(string,"%05ld",sat[y].catnum);
        CopyString(string,line1,2,6);
        CopyString(string,line2,2,6);

        sprintf(string,"%12.8f",sat[y].refepoch);
        CopyString(string,line1,20,32);

        sprintf(string,"%02d",sat[y].year);
        CopyString(string,line1,18,19);

        sprintf(string,"%9.4f",sat[y].incl);
        CopyString(string,line2,7,15);

        sprintf(string,"%9.4f",sat[y].raan);
        CopyString(string,line2,16,24);

        sprintf(string,"%13.12f",sat[y].eccn);

        /* Erase eccentricity's decimal point */

        for (x=2; x<=9; string[x-2]=string[x], x++);

        CopyString(string,line2,26,32);

        sprintf(string,"%9.4f",sat[y].argper);
        CopyString(string,line2,33,41);

        sprintf(string,"%9.5f",sat[y].meanan);
        CopyString(string,line2,43,50);

        sprintf(string,"%12.9f",sat[y].meanmo);
        CopyString(string,line2,52,62);

        sprintf(string,"%.9f",fabs(sat[y].drag));

        CopyString(string,line1,33,42);

        if (sat[y].drag < 0.0)
            line1[33]='-';
        else
            line1[33]=32;

        sprintf(string,"%5lu",sat[y].orbitnum);
        CopyString(string,line2,63,67);

        /* Compute and insert checksum for line 1 and line 2 */

        for (x=0, sum=0; x<=67; sum+=val[(short)line1[x]], x++);
        line1[68]=(sum%10)+'0';

        for (x=0, sum=0; x<=67; sum+=val[(short)line2[x]], x++);
        line2[68]=(sum%10)+'0';

        /* Need to construct some filler space to pad the satname to 24 chars. */
        strcpy(filler_s1, "");
        nspaces = 24 - strlen(sat[y].name);
        for (spaces = 0;  spaces < nspaces; spaces++)
            strcat(filler_s1, " ");

        /* Need to construct some filler space to pad the amsat designator to 4 chars. */
        strcpy(filler_s2, "");
        nspaces = 4 - strlen(sat[y].amsat_des);
        for (spaces = 0;  spaces < nspaces; spaces++)
            strcat(filler_s2, " ");

        /* Need to construct some filler space to pad mode to 2 chars. */

        strcpy(filler_s3, "");
        nspaces = 2 - strlen(sat[y].mode);
        for (spaces = 0;  spaces < nspaces; spaces++)
            strcat(filler_s3, " ");

        /* Write name, line 1, line 2 to predict.tle */

        fprintf(fd,"%s%s%s%s %s %s %s%s\n", sat[y].name, filler_s1, sat[y].amsat_des, filler_s2, sat[y].uplink, sat[y].downlink, sat[y].mode, filler_s3);
        fprintf(fd,"%s\n", line1);
        fprintf(fd,"%s\n", line2);
    }

    fclose(fd);
}


/*
 * This function updates PREDICT's orbital datafile from a NASA
 * 2-line element file.
 *
 * Return values are:
 *
 * 0 : Update successful
 * 1 : No file name specified
 * 2 : Invalid file name specified
 * 3 : Unable to open specified file
 */
short AutoUpdate(char *filename)
{
    char line1[80], line2[80], str0[80], str1[80], str2[80],
         saveflag=0;

    float database_epoch, tle_epoch, database_year, tle_year;
    short i;
    FILE *fd;
    struct stat statbuf;

    if (strlen(filename) == 0)
    {
        return (1);
    }
    else
    {
        /*
         * Prevent a directory from being used as a filename otherwise
         * strange things happen.
         */
        if(stat(filename, &statbuf) != 0)
        {
            /* Can't stat, invalid file, can't be openen either */
            return (3);
        }

        /* directories and block devices are not accepted */
        if( ((statbuf.st_mode & S_IFDIR) != 0)
            ||
            ((statbuf.st_mode & S_IFBLK) != 0)
          )
        {
            /* given file is a directory */
            return (2);
        }
    }

    fd = fopen(filename, "r");

    if (fd == NULL)
    {
        return (3);
    }
    else
    {
        if(fgets(str0,75,fd) == NULL)
        {   /* Reading failed, fill with dummy */
            strcpy(str0, "000000000000000000000000000000000000000000000000000000000000000000000000000");
        }
        if(fgets(str1,75,fd) == NULL)
        {   /* Reading failed, fill with dummy */
            strcpy(str1, "000000000000000000000000000000000000000000000000000000000000000000000000000");
        }
        if(fgets(str2,75,fd) == NULL)
        {   /* Reading failed, fill with dummy */
            strcpy(str2, "000000000000000000000000000000000000000000000000000000000000000000000000000");
        }

        /* Check if we already reached EOF (e.g. on an empty .TLE file) */
        if(feof(fd) != 0)
        {
            fclose(fd);
            return (0);
        }

        do
        {
            if (KepCheck(str1,str2))
            {
                /* We found a valid TLE!
                   Copy strings str1 and
                   str2 into line1 and line2
                */

                   strncpy(line1,str1,75);
                   strncpy(line2,str2,75);

                   /* Scan for object number in datafile to see
                      if this is something we're interested in
                   */

                   for (i=0; (i<24 && sat[i].catnum!=atol(SubString(line1,2,6))); i++);

                   if (i!=24)
                   {
                       /* We found it!  Check to see if it's more
                          recent than the data we already have.
                       */

                       if (sat[i].year<=50)
                           database_year=365.25*(100.0+(float)sat[i].year);
                       else
                           database_year=365.25*(float)sat[i].year;

                       database_epoch=(float)sat[i].refepoch+database_year;

                       tle_year=(float)atof(SubString(line1,18,19));

                       if (tle_year<=50.0)
                           tle_year+=100.0;

                       tle_epoch=(float)atof(SubString(line1,20,31))+(tle_year*365.25);

                       /* Update only if TLE epoch > = epoch in data file
                          so we don't overwrite current data with older
                          data.
                       */

                       if (tle_epoch>=database_epoch)
                       {
                           if (saveflag == 0)
                           {
                               saveflag = 1;
                           }

                           /* Add decimal point to eccentricity value */

                           line2[25]='.';

                           /* Copy TLE data into the sat data structure */

                           sat[i].year=atoi(SubString(line1,18,19));
                           sat[i].refepoch=atof(SubString(line1,20,31));
                           sat[i].incl=atof(SubString(line2,8,15));
                           sat[i].raan=atof(SubString(line2,17,24));
                           sat[i].eccn=atof(SubString(line2,25,32));
                           sat[i].argper=atof(SubString(line2,34,41));
                           sat[i].meanan=atof(SubString(line2,43,50));
                           sat[i].meanmo=atof(SubString(line2,52,62));
                           sat[i].drag=atof(SubString(line1,33,42));
                           sat[i].orbitnum=atof(SubString(line2,63,67));
                       }
                }

                if(fgets(str0,75,fd) == NULL)
                {   /* Reading failed, fill with dummy */
                    strcpy(str0, "000000000000000000000000000000000000000000000000000000000000000000000000000");
                }
                if(fgets(str1,75,fd) == NULL)
                {   /* Reading failed, fill with dummy */
                    strcpy(str1, "000000000000000000000000000000000000000000000000000000000000000000000000000");
                }
                if(fgets(str2,75,fd) == NULL)
                {   /* Reading failed, fill with dummy */
                    strcpy(str2, "000000000000000000000000000000000000000000000000000000000000000000000000000");
                }
            }
            else
            {
                strcpy(str0,str1);
                strcpy(str1,str2);
                if(fgets(str2,75,fd) == NULL)
                {   /* Reading failed, fill with dummy */
                    strcpy(str2, "000000000000000000000000000000000000000000000000000000000000000000000000000");
                }
            }
        } while (feof(fd)==0);

        fclose(fd);
    }

    if (saveflag)
        SaveTLE();

    return (0);
}

/*
 * Read the QTH data. Tracking will be based on the latitude, longitude,
 * and altitude provided in the .ini file.
 * Return value 0 : Unable to retrieve latitude or longitude data
 */
static short convert_qth_data(void)
{
    long   qth_lat;
    long   qth_lon;

    strcpy(qth.callsign, digi_digi_call);
    qth_lat = digi_pos.lat;
    qth_lon = digi_pos.lon;

    qth.stnlat  = (float) qth_lat / 6000;
    qth.stnlong = (float) qth_lon / 6000;

    /*
     * For some reason the Predict code defines East as negative while
     * DIGI_NED code assumes West is negative. We have to reverse the
     * latter to make it work for the Predict code.
     */
    qth.stnlong = -qth.stnlong;

    qth.stnalt = digi_altitude;

    return (1);
}

/*
 * This function initializes some values and reads the required input files.
 * It will return one of the following values:
 *
 *       0 : Unable to load QTH data
 *       1 : QTH data was loaded but reading of satellite file failed
 *       2 : All data was successfully loaded
 *       3 : TLE file not provided, sattracker not used
 */
short sat_init(void)
{
    short result;
    short x;

    /* Set up translation table for computing TLE checksums */
    for (x = 0; x <= 255; val[x] = 0, x++);
    for (x = '0'; x <= '9'; val[x] = x - '0', x++);
    val['-'] = 1;

    /* Get the QTH data */
    result = convert_qth_data();
    if (result == 0)
    {
        return (0);
    }

    /* Set up tlefile to point to the name from the rule-file (*.ini) */
    tlefile = digi_sat_file;

    /* Set up the object format from the rule-file (*.ini) */
    object_format = digi_sat_obj_format;

    /* Read the satellite data file */
    result = ReadDataFiles();
    if (result == 0)
    {
        return (1);
    }

    return(2);
}

/*
 * This function determines the index number for the requested satellite which
 * is passed to this function as a 4 letter Amsat designator (i.e. AO40).
 *
 * The requested satellite index is returned:
 *
 *     0 - 24 : this is an index number that corresponds with
 *              the 3 line satellite information contained in
 *              digi_ned.sat.
 *
 * If the index is 25 then that means that the satellite was not found.
 *    Return value is:
 *
 *     0 - 24 : if satellite is found
 *     -1     : satellite is not found
 */
static signed short get_sat_index(char *satellite)
{
    short found, index;

    /* Check the length of requested Amsat designator. */
    if (strlen(satellite) != 4)
        return (-1);

    /* Make sure Amsat designator is in upper case. */
    for(index = 0; index < strlen(satellite); index++)
    {
        satellite[index] = toupper(satellite[index]);
    }

    found = 0;
    index = 0;
    /* Search the database for requested satellite. */
    do
    {
        if(!strncmp(sat[index].amsat_des, satellite, 4))
            found = 1;
        index++;
    }
    while (found == 0 && index < 25);

    /* If satellite found in database then return correct index, else return
       -1. */
    if (found)
    {
        return (index - 1);
    }
    else
    {
        return (-1);
    }
}


/*
 * This function will return the time of AOS for the specified satellite.
 *
 * The return value is:
 *                      0 : AOS successfully calculated
 *                      1 : No passes can occur for specified satellite
 *                      2 : Specified satellite is a geostationary satellite
 *                      3 : Satellite not in database
 */
short get_aos(char *satellite, time_t *nextaos_l, short next)
{
    signed short index;

    /* Try to find the requested satellite in the database. */
    index = get_sat_index(satellite);

    /* If satellite not in database then return with error code 3. */
    if (index == -1)
        return (3);

    daynum = PreCalc(CurrentDaynum());

    /*
     * If a pass can occur and satellite is not a geo stationary one, then
     * find AOS.
     */
    if (AosHappens(indx) && Geostationary(indx)==0 && Decayed(indx)==0)
    {
        if(next == 1)
        {
            /* We want the next AOS */
            daynum = NextAOS();
        }
        else
        {
            /* We want the upcomming AOS */
            daynum = FindAOS();
        }

        /* Convert daynum to Unix time (seconds since 01-Jan-70) */
        *nextaos_l = Daynum2Time(daynum);
    }
    else
    {
        if (AosHappens(indx)==0 || Decayed(indx)==1)
            return (1);          /* Pass can not occur. */

        if (Geostationary(indx)==1)
            return (2);          /* Satellite is geo stationary. */
    }

    return (0);
}

/*
 * This function will calculate the doppler effect for the uplink and
 * downlink and correct the frequency parameters uplink and downlink. It
 * needs the current daynumber (dayn), a previous clock value (old_clock)
 * en a previous range (old_range) to calculate the doppler effect.
 */
static void doppler(double dayn, double old_clock, double old_range,
                    char *uplink, char *downlink)
{
    double       dp, dt, fraction;
    signed short doppler146;
    signed short doppler435;
    long         uplink_khz, downlink_khz;
    char         uplink_khz_s[8], downlink_khz_s[8];

    /* Get the uplink and downlink in KHz. */

    uplink_khz   = my_rint( atof(uplink) * 1000);
    downlink_khz = my_rint( atof(downlink) * 1000);

    /* Calculate the doppler effect for 2m (146) and 70cm (435) */

    dt = Daynum2Time(dayn) - old_clock;
    dp = (rk * 1000.0)  - old_range;
    fraction = -((dp / dt) / 299792458.0);
    doppler146 = (signed short)my_rint((fraction * 146.0e6) / 1000);
    doppler435 = (signed short)my_rint((fraction * 435.0e6) / 1000);

    /* Add the doppler effect to the correct uplink and downlink frequencies. */

    if (uplink[0] == '1')
        uplink_khz = uplink_khz - doppler146;
    if (uplink[0] == '4')
        uplink_khz = uplink_khz - doppler435;
    if (downlink[0] == '1')
        downlink_khz = downlink_khz + doppler146;
    if (downlink[0] == '4')
        downlink_khz = downlink_khz + doppler435;
    /*
     * Convert the corrected frequencies back to string values in MHz,
     * but only if the frequency was not 000.000 (unknown or unspecified)
     * or < 100 MHz (i.e 029.000 for some of the RS sats).
     */
    if (uplink[0] != '0')
    {
        sprintf(uplink_khz_s, "%ld", uplink_khz);
        strcpy(uplink, SubString(uplink_khz_s, 0, 2));
        strcat(uplink, ".");
        strcat(uplink, SubString(uplink_khz_s, 3, 5));
    }
    if (downlink[0] != '0')
    {
        sprintf(downlink_khz_s, "%ld", downlink_khz);
        strcpy(downlink, SubString(downlink_khz_s, 0, 2));
        strcat(downlink, ".");
        strcat(downlink, SubString(downlink_khz_s, 3, 5));
    }
}

/*
 * This function determines if the the current pass is a long one or not. If
 * a satellite is in range for a half hour or longer, we'll call it a long
 * pass.
 * The function returns:
 *                       0 : short pass
 *                       1 : long pass
 *
 * We check to see if the satellite was in range either half an hour ago
 * or a half hour in the future. Of course this is not technically correct
 * since theoretically we could have a satellite that orbits the earth in less
 * than half an hour. These sats are beyond the scope of this program
 * however.
 */
static short long_pass(void)
{
    double half_hour = 0.0207;
    double elevation1, elevation2;

    /* Calculate the position of the satellite half an hour ago. */
    daynum = CurrentDaynum() - half_hour;
    PreCalc(daynum);
    Calc();
    elevation1 = elevation;

    /* Calculate the position of the satellite half hour into the future. */
    daynum = CurrentDaynum() + half_hour;
    PreCalc(daynum);
    Calc();
    elevation2 = elevation;

    /* Now check if the satellite was in range half an hour ago or half hour
       into the future. */
    if (elevation1 > 0.0 || elevation2 > 0.0)
    {
        return (1); /* This pass is a long pass. */
    }
    else
    {
        return (0); /* This pass is a short pass. */
    }

}

/*
 * Create an APRS satellite object for the requested satellite.
 *
 * The requested satellite is indicated by the satellite parameter which
 * is a 4 letter Amsat designator.
 *
 * If the satellite is in range then it will return an object which includes the
 * doppler corrected uplink and downlink frequencies, elevation and azimuth,
 * and the mode the satellite is operinting in. If the satellite is out of
 * range it will return AOS (next pass).
 *
 * Except for the object creation time, which is always in UTC per the APRS
 * specs, all times can be displayed in either UTC or local time, depending
 * on the uselocal parameter:
 *
 *     1 : local time is displayed
 *     0 : UTC time is displayed
 *
 * The satellite APRS object is returned through satobject.
 *
 * This function will return one of the following values:
 *
 *     0 : unable to create object, satellite not found in database
 *     1 : No passes can occur for specified satellite
 *     2 : Specified satellite is a geostationary satellite
 *     3 : satellite is in not range
 *     4 : satellite is in range
 */
short get_sat_object(char *satellite, short uselocal, char *satobject,
                            char metric[3], short *longpass)
{
    position_t   sat_pos, prev_sat_pos;
    short        heading,spaces, nspaces, result = 0;
    signed short index;
    char         string[80], sat_object[MAX_MESSAGE_LENGTH+1], heading_s[4];
    char         sat_pos_string[19],freq_up[8], freq_down[8];
    char         cur_time[80], AOStime_s[25], kepepoch[25];
    double       curr_pos_lon, prev_pos_lon, old_clock, old_range;
    double       one_second = 0.0000115;
    time_t       now;
    time_t       aos;
    time_t       kep;

    /* Try to find the requested satellite in the database. */
    index = get_sat_index(satellite);

    /* If satellite not in database then return with error code 0. */
    if (index == -1)
        return (0);

    /* indx is defined by the predict code as a global index variable */
    indx = index;

    /*
     * Determine if it is possible to calculate an orbit and pass for the
     * requested satellite.
     */
    daynum = PreCalc(CurrentDaynum());
    if (AosHappens(indx) && Geostationary(indx)==0 && Decayed(indx)==0)
    {
        /* it is possible to calculate a pass for this satellite. */
        daynum = FindAOS();

        /* Convert daynum to Unix time (seconds since 01-Jan-70) */
        aos = Daynum2Time(daynum);
    }
    else
    {
        /*
         * It is not possible to calculate a pass for this satellite. Now
         * determine why not.
         */

        if (AosHappens(indx)==0 || Decayed(indx)==1)
            result = 1;

        if (Geostationary(indx)==1)
            result = 2;
    }

    /*
     * No need to continue if satellite is a geo stationary one or if it's not
     * possible to calculate a pass.
     */
    if (result == 1 || result == 2)
        return (result);

    /* Determine if the current pass is a long pass. */
    *longpass = long_pass();

    /*
     * Find position of satellite one second ago. This is needed to calculate
     * the heading of thesatellite and the doppler effect.
     */

    daynum = CurrentDaynum() - one_second;
    PreCalc(daynum);
    Calc();

    /* Convert the latitude and longitude in APRS format. */

    prev_pos_lon = -ssplong;
    if (prev_pos_lon < -180.0)
    {
        prev_pos_lon = prev_pos_lon +360;
    }
    prev_sat_pos.lat = ssplat * 6000;
    prev_sat_pos.lon = prev_pos_lon * 6000;

    /* Save the time and range for doppler calculations. */

    old_clock = Daynum2Time(daynum);
    old_range = rk * 1000.0;

    /* Now find the current position of the satellite. */

    daynum = CurrentDaynum();
    PreCalc(daynum);
    Calc();

    /* Convert the latitude and longitude in APRS format. */

    curr_pos_lon = -ssplong;
    if (curr_pos_lon < -180.0)
    {
        curr_pos_lon = curr_pos_lon +360;
    }
    sat_pos.lat = ssplat * 6000;
    sat_pos.lon = curr_pos_lon * 6000;

    sprintf(string,"%s %-7.2f %+-6.2f %-6.2f %-7.2f ", sat[indx].name,
                                        azimuth,elevation, ssplat,ssplong);

    /* Calculate the heading of the satellite. */

    dist_distance_and_bearing(&prev_sat_pos, &sat_pos, &heading, metric);
    (void) digi_itoa(heading, heading_s);

    /* Determine the doppler corrected uplink and downlink frequencies. */

    strcpy(freq_up, sat[indx].uplink);
    strcpy(freq_down, sat[indx].downlink);
    doppler(daynum, old_clock, old_range, freq_up, freq_down);

    /* Assemble the APRS satellite object. */

    /* ; is the APRS object indicator */

    strcpy(sat_object, ";");

    /* Add the Amsat designator and pad with spaces up to 9 characters. */

    strcat(sat_object, sat[indx].amsat_des);
    nspaces = 5 - strlen(sat[indx].amsat_des);
    for (spaces = 0; spaces < nspaces; spaces++)
        strcat(sat_object, " ");


    /*
     * The number specifies the format of the sat-object:
     * 0) Show as "AO40    E" (where E is for the Elevation column)
     *
     * 1) Show as "AO40 126E" (where 123 is the epoch of the used kepler
     *                         data and E is for the Elevation column)
     * 2) Show as "AO40 0805" (where 0805 is the ddmm date of the used
     *                         kepler data)
     * 3) Show as "AO40 0508" (where 0508 is the mmdd date of the used
     *                         kepler data)
     * Default is 0.
     */
    switch(object_format) {
    case 1: 
            /*
             * Add kepler epoch and "E" to the object
             */

            /* Append Kepler-daynumber/epoch to the object */
            sprintf(kepepoch, "%03dE", (int) sat[indx].refepoch);
            strcat(sat_object, kepepoch);
            break;

    case 2:
            /*
             * Add kepler date "ddmm" to the object
             */

            /* Convert daynum to Unix time (seconds since 01-Jan-70) */
            kep = Daynum2Time(DayNumYear(sat[indx].year) + sat[indx].refepoch);

            /* Append Kepler day and month to object */
            strftime(kepepoch, 5, "%d%m", gmtime(&kep));
            strcat(sat_object, kepepoch);
            break;

    case 3:
            /*
             * Add kepler date "mmdd" to the object
             */

            /* Convert daynum to Unix time (seconds since 01-Jan-70) */
            kep = Daynum2Time(DayNumYear(sat[indx].year) + sat[indx].refepoch);

            /* Append Kepler day and month to object */
            strftime(kepepoch, 5, "%m%d", gmtime(&kep));
            strcat(sat_object, kepepoch);
            break;

    default:
            /*
             * Append 'E' mark to the object
             */
            strcat(sat_object, "   E");
            break;
    }

    /* Add object creation time. This time is always in UTC. */

    time(&now);
    strftime(cur_time, 80, "*%d%H%M", gmtime(&now));
    strcat(sat_object, cur_time);
    strcat(sat_object, "z");

    /* Add the position of the satellite. */

    strcat(sat_object, dist_pos2ascii(&sat_pos, sat_pos_string));

    /* Insert and add the satellite APRS icon. */

    sat_object[26] = '\\';
    strcat(sat_object, "S");

    /* Add heading and speed to the object. Note that maximum speed is
       used and that the heading can have leading zeors per APRS spec.
       Maximum speed for imperial system is 999 mph and for the metric
       system this is 539 kmh. */

    if (strlen(heading_s) == 2)
    {
        strcat(sat_object, "0");
    }
    else
    {
        if (strlen(heading_s) == 1)
        {
            strcat(sat_object, "00");
        }
    }
    strcat(sat_object, heading_s);
    if (strcmp(metric, "km"))
        strcat(sat_object, "/999");
    else
        strcat(sat_object, "/539");

    if (elevation < -0.03 )
    {
        /*
         * If the satellite is not in range then add
         * the next pass (AOS) to the object.
         */

        /* Indicate whether local or UTC time is being displayed. */
        if (uselocal == 1)
        {
            strftime(AOStime_s, 25, " AOS@%d-%m %H:%M LOC", localtime(&aos));
        }
        else
        {
            strftime(AOStime_s, 25, " AOS@%d-%m %H:%M UTC", gmtime(&aos));
        }
        strcat(sat_object, AOStime_s);

        /* Indicate that satellite is not in range. */
        result = 3;
    }
    else
    {
        /*
         * If the satellite is in range, then add the doppler corrected uplink
         * and downlink frequencies, the elevation and azimuth, and the mode
         * of operation to the object.
         */
        strcat(sat_object, " ");
        strcat(sat_object, freq_up);
        strcat(sat_object, "MHz");
        strcat(sat_object, " ");
        strcat(sat_object, freq_down);
        strcat(sat_object, " up ");
        strcat(sat_object, sat[indx].amsat_des);
        if(my_rint(elevation) >= 20)
        {
            strcat(sat_object, " HI");
        }
        else
        {
            strcat(sat_object, " LOW");
        }

        /* Indicate that satellite is in range. */
        result = 4;
    }

    /* Return the object. */
    strcpy(satobject, sat_object);

    /* Return whether or not satellite is in range. */
    return (result);
}

/*
 * Return a pointer to the Amsat designator string at index "index". If the
 * index is too high or too low return NULL. This is used to produce a
 * satellite list
 */
char *get_amsat_des_at_index(short index)
{
    if((index >= numsats) || (index < 0))
        return NULL;

    return sat[index].amsat_des;
}
#endif
