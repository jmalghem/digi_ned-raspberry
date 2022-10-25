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
#ifdef _WX_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
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
#include "keep.h"
#include "timer.h"
#include "output.h"

/*
 * This file contains all code to interpret and execute 'wx_var'
 * commands from the .ini file.
 *
 * Example:
 * wx_var: p,lpt1_8/2,0,1,0
 */

/*
 * Data structure containing the 'wx' command data
 */
static wx_var_t *digi_wx_var_p = NULL;

static long format_time;

/*
 * Add telemetry command from the .ini file to the internal administration.
 *
 * wx_var: p,lpt1_8/2,0,1,0\r\n
 *        ^
 * wx_var: T,dmy,z
 *        ^
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'wx_var' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_wx_var(void)
{
    wx_var_t     wx_var;
    wx_var_t    *wx_var_p;
    char        *tmp;
    char        *tmp_square;
    char        *tmp_multiply;
    char        *tmp_constant;
    char        *addr_p;
    char        *tmp_case;
    short        i;
    short        timevar;

    tmp = strtok(NULL,", \t\r\n");

    if(tmp == NULL)
    {
        say("Variable name in wx_var: at line %d\r\n", linecount);
        return;
    }

    if(strlen(tmp) != 1)
    {
        say("Error in variable in wx_var: at line %d, "
            "only 1 character expected\r\n", linecount);
        return;
    }

    wx_var.variable = *tmp;
    if( (wx_var.variable >= '0') && (wx_var.variable <= '9') )
    {
        say("Error in variable in wx_var: at line %d, "
            "numbers not allowed\r\n", linecount);
        return;
    }

    /*
     * Read the kind of wx_var
     */
    tmp = strtok(NULL,",\r\n");

    if(tmp == NULL)
    {
        say("Missing variable kind in wx_var: at line %d\r\n", linecount);
        return;
    }

    /*
     * Convert to lowercase and check against the allowed kinds
     */
    tmp_case = tmp;
    while(*tmp_case != '\0')
    {
        *tmp_case = tolower(*tmp_case);
        tmp_case++;
    }

    /* variable kind is at least 3 characters */
    if(strlen(tmp) < 3)
    {
        say("Invalid variable kind in wx_var: at line %d\r\n", linecount);
        return;
    }

    /* Remeber if it is a "time" variable */
    timevar = 0;

    if(strncmp(tmp, "val", 3) == 0)
    {
        wx_var.kind = VAL;
    }
    else if(strncmp(tmp, "max", 3) == 0)
    {
        wx_var.kind = MAX;
    }
    else if(strncmp(tmp, "min", 3) == 0)
    {
        wx_var.kind = MIN;
    }
    else if(strncmp(tmp, "sum", 3) == 0)
    {
        wx_var.kind = SUM;
    }
    else if(strncmp(tmp, "avg", 3) == 0)
    {
        wx_var.kind = AVG;
    }
    else if(strncmp(tmp, "dhm", 3) == 0)
    {
        wx_var.kind = DHM;
        timevar = 1;
    }
    else if(strncmp(tmp, "hms", 3) == 0)
    {
        wx_var.kind = HMS;
        timevar = 1;
    }
    else if(strncmp(tmp, "ymd", 3) == 0)
    {
        wx_var.kind = YMD;
        timevar = 1;
    }
    else if(strncmp(tmp, "ydm", 3) == 0)
    {
        wx_var.kind = YDM;
        timevar = 1;
    }
    else if(strncmp(tmp, "dmy", 3) == 0)
    {
        wx_var.kind = DMY;
        timevar = 1;
    }
    else if(strncmp(tmp, "mdy", 3) == 0)
    {
        wx_var.kind = MDY;
        timevar = 1;
    }
    else if(strncmp(tmp, "mdhm", 4) == 0)
    {
        wx_var.kind = MDHM;
        timevar = 1;
    }
    else if(strncmp(tmp, "mdh", 3) == 0) /* ! don't put this before MDHM! */
    {
        wx_var.kind = MDH;
        timevar = 1;
    }
    else
    {
        say("Unknown variable kind \"%s\" in wx_var: at line %d\r\n", tmp,
                    linecount);
        return;
    }

    /*
     * Read the wx_var source port for non-time variables
     */
    if(timevar == 0)
    {
        if( strspn(&tmp[3],"0123456789") != strlen(&tmp[3]) )
        {
            say("Invalid time in variable kind in wx_var: at line %d, "
                "shall be a number\r\n", linecount);
            return;
        }

        /* If 0 then period is dayly */
        wx_var.period = atoi(&tmp[3]);
 
        tmp = strtok(NULL,",\r\n");

        if(tmp == NULL)
        {
            say("Missing source port name in wx_var: at line %d\r\n",
                                        linecount);
            return;
        }

        addr_p = strchr(tmp, '/');
        if(addr_p != NULL)
        {
            /* Split source and address part */
            *addr_p = '\0';
            addr_p++;
        }

        /*
         * Check if the source name is "lpt1", "lpt2" or "lpt3"
         * or "lpt1_8", "lpt2_8" or "lpt3_8".
         */
        tmp_case = tmp;
        while(*tmp_case != '\0')
        {
            *tmp_case = tolower(*tmp_case);
            tmp_case++;
        }

        if( (strcmp(tmp, "lpt1") != 0)
            &&
            (strcmp(tmp, "lpt2") != 0)
            &&
            (strcmp(tmp, "lpt3") != 0)
            &&
            (strcmp(tmp, "lpt1_8") != 0)
            &&
            (strcmp(tmp, "lpt2_8") != 0)
            &&
            (strcmp(tmp, "lpt3_8") != 0)
          )
        {
            say("Sourcename error in wx_var: at line %d, should be "
                "\"lpt(1..3[_8])\"\r\n", linecount);
            return;
        }

        strcpy(wx_var.source, tmp);

        if(addr_p != NULL)
        {
            /* Handle maximum portnumber */
            if( (strspn(addr_p,"01234567") != 1)
                ||
                (strlen(addr_p) != 1)
              )
            {
                say("Address error in wx_var: at line %d, only 0..7 "
                    "accepted, e.g. \"lpt2_8/4\"\r\n", linecount);
                return;
            }
            wx_var.address = atoi(addr_p);
        }
        else
        {
            wx_var.address = 0;
        }

        /* Retrieve equation values for the analog value */
        tmp_square = strtok(NULL,",\r\n");
        if(tmp_square != NULL)
        {
            tmp_multiply = strtok(NULL,",\r\n");
            if(tmp_multiply != NULL)
            {
                tmp_constant = strtok(NULL,",\r\n");
            }
            else
            {
                tmp_constant = NULL;
            }
        }
        else
        {
            tmp_multiply = NULL;
            tmp_constant = NULL;
        }

        if(tmp_square == NULL)
        {
            wx_var.a = 0.0;
        }
        else
        {
            if(strspn(tmp_square, "0123456789-.") != strlen(tmp_square))
            {
                say("Equation parameter error in wx_var: at line %d, only "
                    "numbers accepted\r\n", linecount);
                return;
            }

            sscanf(tmp_square,"%lf",&wx_var.a);
        }

        if(tmp_multiply == NULL)
        {
            wx_var.b = 1.0;
        }
        else
        {
            if(strspn(tmp_multiply, "0123456789-.") != strlen(tmp_multiply))
            {
                say("Equation parameter error in wx_var: at line %d, only "
                    "numbers accepted\r\n", linecount);
                return;
            }

            sscanf(tmp_multiply,"%lf",&wx_var.b);
        }

        if(tmp_constant == NULL)
        {
            wx_var.c = 0.0;
        }
        else
        {
            if(strspn(tmp_constant, "0123456789-.") != strlen(tmp_constant))
            {
                say("Equation parameter error in wx_var: at line %d, only "
                    "numbers accepted\r\n", linecount);
                return;
            }

            sscanf(tmp_constant,"%lf",&wx_var.c);
        }

        /* Initialize the current value of the variable */
        wx_var.value = get_lpt_status(wx_var.source, wx_var.address);
    }
    else
    {
        /* "time" variable, expect "local" or "zulu" */
        tmp = strtok(NULL,",\r\n");

        if(tmp == NULL)
        {
            say("Missing zone information in wx_var: at line %d, "
                "(\"zulu\" or \"local\")\r\n", linecount);
            return;
        }

        /*
         * Check if the zone name is "zulu" or "local"
         */
        tmp_case = tmp;
        while(*tmp_case != '\0')
        {
            *tmp_case = tolower(*tmp_case);
            tmp_case++;
        }

        if( (strcmp(tmp, "zulu") != 0)
            &&
            (strcmp(tmp, "local") != 0)
          )
        {
            say("Invalid zone information in wx_var: at line %d, "
                "(\"zulu\" or \"local\")\r\n", linecount);
            return;
        }

        strcpy(wx_var.source, tmp);

        /* Initialize unused parts */
        wx_var.period  = 0;
        wx_var.address = 0;

        /* Set equation to 1x, i.e. no modification */
        wx_var.a = 0.0;
        wx_var.b = 1.0;
        wx_var.c = 0.0;
    }

    /* Clean the cells for this variable */
    for(i = 0; i < NR_CELLS; i++)
    {
        wx_var.data[i].stamp = 0L;
        wx_var.data[i].value = 0;
    }
    /* Start filling cell 0 */
    wx_var.cell = 0;

    /* Store the result in the linked list */

    /* Allocate storage space */
    wx_var_p = (wx_var_t*) malloc(sizeof(wx_var_t));

    /*
     * if out of memory then ignore
     */
    if(wx_var_p == NULL)
    {
        say("Out of memory while handling wx_var: line at line %d\r\n",
             linecount);
        return;
    }
    
    /*
     * Duplicate data
     */
    memcpy(wx_var_p, &wx_var, sizeof(wx_var_t));

    /*
     * Add wx_var data
     */
    wx_var_p->next = digi_wx_var_p;
    digi_wx_var_p = wx_var_p;
}

/*
 * get_wx_var_value
 *
 * Return value of a WX variable, this is the raw value which still
 * needs to be scaled with the calculation settings
 */
static long get_wx_var_value(wx_var_t *wx_var_p)
{
    unsigned long age;
    long          value;
    short         i;
    struct tm    *tm_p;
    short         cell_count;

    if(wx_var_p->kind == VAL)
    {
        value = (long) get_lpt_status(wx_var_p->source, wx_var_p->address);

        wx_var_p->value = (short) value;

        /* This is the value of this variable */
        return value;
    }

    /*
     * Handle "time" type variables
     */
    if( (wx_var_p->kind == DHM)
        ||
        (wx_var_p->kind == HMS)
        ||
        (wx_var_p->kind == YMD)
        ||
        (wx_var_p->kind == YDM)
        ||
        (wx_var_p->kind == DMY)
        ||
        (wx_var_p->kind == MDY)
        ||
        (wx_var_p->kind == MDH)
        ||
        (wx_var_p->kind == MDHM)
      )
    {
        /* convert time to TM structure */
        if(strcmp(wx_var_p->source, "zulu") == 0)
        {
            tm_p = gmtime(&format_time); 
        }
        else
        {
            tm_p = localtime(&format_time); 
        }

        switch(wx_var_p->kind) {
        case DHM:   value = 10000L * (long) tm_p->tm_mday +
                              100L * (long) tm_p->tm_hour +
                                     (long) tm_p->tm_min;
                    break;
        case HMS:   value = 10000L * (long) tm_p->tm_hour +
                              100L * (long) tm_p->tm_min +
                                     (long) tm_p->tm_sec;
                    break;
        case YMD:   value = 10000L * (long) (tm_p->tm_year % 100) +
                              100L * (long) (tm_p->tm_mon + 1) +
                                     (long) tm_p->tm_mday;
                    break;
        case YDM:   value = 10000L * (long) (tm_p->tm_year % 100) +
                              100L * (long) tm_p->tm_mday +
                                     (long) (tm_p->tm_mon + 1);
                    break;
        case DMY:   value = 10000L * (long) tm_p->tm_mday +
                              100L * (long) (tm_p->tm_mon + 1) +
                                     (long) (tm_p->tm_year % 100);
                    break;
        case MDY:   value = 10000L * (long) (tm_p->tm_mon + 1) +
                              100L * (long) tm_p->tm_mday +
                                     (long) (tm_p->tm_year % 100);
                    break;
        case MDH:   value = 10000L * (long) (tm_p->tm_mon + 1) +
                              100L * (long) tm_p->tm_mday +
                                     (long) tm_p->tm_hour;
                    break;
        default: /* MDHM */
                    value = 1000000L * (long) (tm_p->tm_mon + 1) +
                              10000L * (long) tm_p->tm_mday +
                                100L * (long) tm_p->tm_hour +
                                       (long) tm_p->tm_min;
        }

        /* This is the value of this variable */
        return value;
    }

    /*
     * Accumulating variables follow here, retrieve data from the data-cells.
     */

    /*
     * Calculate allowable age for a cell, when the cell it too old
     * we proceed to the next cell.
     */
    if(wx_var_p->period == 0)
    {
        /* Since midnight, find out how long ago that was */
        tm_p = localtime(&format_time); 

        /* Set time to midnight */
        tm_p->tm_sec = 0;
        tm_p->tm_min = 0;
        tm_p->tm_hour = 0;

        /* Convert to back to long to get the time at midnight */
        age = mktime(tm_p);
    }
    else
    {
        /* convert period in minutes to seconds */
        age = format_time - (((long) wx_var_p->period) * 60L);
    }

    /* Make a start value before examining all the valid cells */
    if((wx_var_p->kind == SUM) || (wx_var_p->kind == AVG))
    {
        /* Start with 0 count */
        value = 0L;
    }
    else
    {
        /* For MIN and MAX start with the contents of the current cell */
        value = (long) wx_var_p->data[wx_var_p->cell].value;
    }

    /*
     * Start with 1 cell, the current cell. Increment for each cell
     * which is added to calculate the avarage.
     */
    cell_count = 1;

    /* Accumulate all the cell data of valid cells */
    for(i = 0; i < NR_CELLS; i++)
    {
        /* only considder cells newer or equal to age */
        if(wx_var_p->data[i].stamp >= age)
        {
            cell_count++;

            if((wx_var_p->kind == SUM) || (wx_var_p->kind == AVG))
            {
                value = value + (long) wx_var_p->data[i].value;
            }
            else if(wx_var_p->kind == MIN)
            {
                if(((long) wx_var_p->data[i].value) < value)
                {
                    value = (long) wx_var_p->data[i].value;
                }
            }
            else /* MAX */
            {
                if(((long) wx_var_p->data[i].value) > value)
                {
                    value = (long) wx_var_p->data[i].value;
                }
            }
        }
    }

    if(wx_var_p->kind == AVG)
    {
        /* Now calculate the avarage */
        value = value / cell_count;
    }

    /* This is the value of this variable */
    return value;
}

/*
 * wx_format
 *
 * Format a WX data string using the variables defined with "wx_var:" rules.
 *
 * The first variable contains the space for the formatted data, the second
 * parameter contains the format and the third paramter contains the available
 * space for the formatted the line.
 */
void wx_format(char *line, char *format, short size)
{
    short        len;
    char        *from;
    char        *to;
    char        *p;
    char         fmt[MAX_LINE_LENGTH + 1];
    char         v[MAX_LINE_LENGTH];
    char         var;
    double       val;
    short        i;
    short        s;
    wx_var_t    *wx_var_p;

    from = format;
    to   = line;
    len  = 0;

    /*
     * during formatting use the same timestamp for consistency
     *
     * "time" returns seconds since 00:00:00 UTC, January 1, 1970
     */
    format_time = (long) time(NULL);

    /* parse WX format stored in variable "format" */

    /* find formatting or escape character */
    p = strpbrk(from, "\\%");
    while((p != NULL) && (*p == '\\'))
    {
        *p = '\0';

        /* Copy segment before the \ sign, if any */
        if( p != from )
        {
            strncpy(to, from, size - len);

            line[size] = '\0';

            len = strlen(line);

            from = p + 1;
            to   = line + len;
        }
        
        /* restore the character */
        *p = '\\';
        
        /* advanvce to escaped character */
        p++;

        /* skip over the escaped character */
        if(*p != '\0') p++;

        /* find next formatting or escape character */
        p = strpbrk(p, "\\%");
    }

    /* copy segment per segment to the output */
    while(p != NULL)
    {
        *p = '\0';

        /* Copy segment before the % sign, if any */
        if( p != from)
        {
            strncpy(to, from, size - len);

            line[size] = '\0';

            len = strlen(line);

            from = p;
            to   = line + len;
        }

        *p = '%';

        /* Get the variable formatting segment */
        i = strspn(p + 1, "0123456789-.") + 2;

        if(i > strlen(p))
        {
            i = strlen(p);
        }

        strncpy(fmt, p, i);

        fmt[i] = '\0';

        /* character we are looking for */
        var = fmt[i - 1];

        /* see if we can find this one */
        wx_var_p = digi_wx_var_p;

        while(wx_var_p != NULL)
        {
            if(wx_var_p->variable == var)
            {
                break;
            }

            wx_var_p = wx_var_p->next;
        }

        /* if not NULL we found the variable data */
        if(wx_var_p != NULL)
        {
            /* get the most recent value and calculate the value */
            val = (double) get_wx_var_value(wx_var_p);

            val = wx_var_p->a * (val * val) + wx_var_p->b * val + wx_var_p->c;
        }
        else
        {
            /* variable not found, just use value zero */
            val = 0.0;

            /* give a warning about the missing variable */
            say("WARNING: variable \"%c\" used in wx: but not defined with "
                "wx_var:. Assuming zero.\r\n", var);
        }

        /* convert to %..ld */
        fmt[i - 1] = 'l';
        fmt[i]     = 'd';
        fmt[i+1]   = '\0';

        /*
         * format the variable, (snprintf would be better, but Borland C
         * does not have it unfortunatly. Lets hope "v" is never overrun...
         */
        sprintf(v, fmt, (long) val);
        
        /* if v is too big only copy only 's' characters */
        s = 0;

        /* get size from the variable */
        if(fmt[1] != '-')
        {
            /* right alligned */
            s = atoi(&fmt[1]);
        }
        else
        {
            /* left alligned */
            s = atoi(&fmt[2]);
        }

        if(s != 0)
        {
            if(strlen(v) > s)
            {
                /*
                 * copy tail 's' chars of value to the destination string
                 * we want to keep the least significant value
                 */
                strncpy(to, &v[strlen(v) - s], size - len);
            }
            else
            {
                /*
                 * it still fits, just copy the value to the destination
                 * string
                 */
                strncpy(to, v, size - len);
            }
        }
        else
        {
            /* no formatting, just copy */
            strncpy(to, v, size - len);
        }

        line[size] = '\0';

        len = strlen(line);

        from = p + i;
        to   = line + len;

        /* find next formatting or escape character */
        p = strpbrk(from, "\\%");
        while((p != NULL) && (*p == '\\'))
        {
            *p = '\0';

            /* Copy segment before the \ sign, if any */
            if( p != from )
            {
                strncpy(to, from, size - len);

                line[size] = '\0';

                len = strlen(line);

                from = p + 1;
                to   = line + len;
            }
            
            /* restore the character */
            *p = '\\';
            
            /* advanvce to escaped character */
            p++;

            /* skip over the escaped character */
            if(*p != '\0') p++;

            /* find next formatting or escape character */
            p = strpbrk(p, "\\%");
        }
    }

    /* No more formatting, copy the remainder */
    strncpy(to, from, size - len);

    /* The "line" now contains a completely formatted WX message, return */
    return;
}

/*
 * update_wx_var
 *
 * Update all WX variables which accumutate data (SUM, MAX, MIN, AVG)
 *
 * Data is accumulated in NR_CELLS cells, the "window" to look back
 * shifts in steps, each step is period/NR_CELLS minutes. This way
 * we don't have to keep all the measurements which would take a lot
 * of memory. The error this introduces is small unless the input
 * data is very dynamic. Since this is a sampling system there are
 * always sampling errors if the data changes faster than the sampling
 * rate. A sample is taken every 10 seconds, unless other tasks prevent
 * the main loop calling update_wx_var (for example shelling-out to DOS).
 */
void update_wx_var(void)
{
    wx_var_t            *wx_var_p;
    short                value;
    short                old_value;
    short                dif_value;
    static unsigned long old_t = 0L;
    unsigned long        now;
    unsigned long        age;
    unsigned long        period_sec;
    short                cell;

    /*
     * "time" returns seconds since 00:00:00 UTC, January 1, 1970
     */
    now = (long) time(NULL);

    /* Update every 10 seconds to keep the load down */
    if((old_t + 10L) > now)
    {
        return;
    }

    /* Due for update */
    old_t = now;

    /* Traverse through all the variables */
    wx_var_p = digi_wx_var_p;

    while(wx_var_p != NULL)
    {
        /* Skip other than SUM, MIN, MAX and AVG variables */
        if( (wx_var_p->kind != MIN)
            &&
            (wx_var_p->kind != MAX)
            &&
            (wx_var_p->kind != SUM)
            &&
            (wx_var_p->kind != AVG)
          )
        {
            /* proceed to next variable */
            wx_var_p = wx_var_p->next;

            continue;
        }

        /* retrieve the previous value, needed for some types */
        old_value = wx_var_p->value;

        /* get the current value from the interface */
        value = get_lpt_status(wx_var_p->source, wx_var_p->address);

        /* store the current value */
        wx_var_p->value = value;

        /* Get the current cell */
        cell = wx_var_p->cell;

        /*
         * Calculate allowable age for a cell, when the cell it too old
         * we proceed to the next cell.
         */
        if(wx_var_p->period == 0)
        {
            /* Since midnight, 24 hours span = 1440 minutes, 86400 sec */
            period_sec = 86400L;
        }
        else
        {
            /* convert period in minutes to seconds */
            period_sec = ((long) wx_var_p->period) * 60L;
        }
        age = now - (period_sec / ((long) NR_CELLS));

        /*
         * Check if we will add data to this cell or advance to the
         * next cell.
         */
        if(wx_var_p->data[cell].stamp < age)
        {
            /* This cell is too old, advance to next cell */
            cell = (cell + 1) % NR_CELLS;

            /* The cell new cell is in use from "now" */
            wx_var_p->data[cell].stamp = now;

            /* remember in which cell we are */
            wx_var_p->cell = cell;

            /*
             * For MIN and MAX just set the value, for SUM calculate
             * the difference with the previous readout
             */
            if(wx_var_p->kind == SUM)
            {
                /*
                 * Prevent wrong values if the counter counted
                 * backward or fast forward for some odd reason
                 *
                 * If the counter increased more than 32 since the
                 * last measurement we don't trust it. In that case
                 * just assume the difference is 0
                 */
                dif_value = (value - old_value) & 0x0ff;
                if(dif_value <= 32)
                {
                    wx_var_p->data[cell].value = dif_value;
                }
                else
                {
                    wx_var_p->data[cell].value = 0;
                }
            }
            else
            {
                /* MIN, MAX and AVG are all the same for the first value */
                wx_var_p->data[cell].value = value;
            }
        }
        else
        {
            /*
             * Handle SUM, MIN, MAX and AVG on the current CELL
             */
            if(wx_var_p->kind == SUM)
            {
                /* Add the new difference to the counter */

                /*
                 * Prevent wrong values if the counter counted
                 * backward or fast forward for some odd reason
                 *
                 * If the counter increased more than 32 since the
                 * last measurement we don't trust it. In that case
                 * don't add the difference.
                 */
                dif_value = (value - old_value) & 0x0ff;
                if(dif_value <= 32)
                {
                    wx_var_p->data[cell].value = dif_value +
                                        wx_var_p->data[cell].value;
                }
            }
            else if(wx_var_p->kind == MIN)
            {
                /* Exchange value if new value is lower */
                if(value < wx_var_p->data[cell].value)
                {
                    wx_var_p->data[cell].value = value;
                }
            }
            else if(wx_var_p->kind == MAX)
            {
                /* Exchange value if new value is higher */
                if(value > wx_var_p->data[cell].value)
                {
                    wx_var_p->data[cell].value = value;
                }
            }
            else /* AVG */
            {
                /*
                 * Use the avarage of the previous and current
                 * value for this cell.
                 */
                wx_var_p->data[cell].value = 
                    (value + wx_var_p->data[cell].value) / 2;
            }
        }

        /* proceed to next variable */
        wx_var_p = wx_var_p->next;
    }
}

/*
 * Dump wx_var administration
 */
void dump_wx_var(void)
{
    wx_var_t    *wx_var_p;

    wx_var_p = digi_wx_var_p;

    while(wx_var_p != NULL)
    {
        say("WX var: '%c' calculated\r\n", wx_var_p->variable);

        wx_var_p = wx_var_p->next;
    }
}

#endif /* _WX_ */
