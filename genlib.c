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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _LINUX_
#include "linux.h"
#endif
#include "digicons.h"
#include "digitype.h"
#include "output.h"
#include "genlib.h"
#include "mac_if.h"

/* used for the port iterator */
static char *iter_ports_p = NULL;
static short iter_allbut;
static char  iter_ports[OUTPORTS_LENGTH];

static char *progpath;
static char *progname;

/*
 * Retrieve program path and name from the first commandline argument
 *
 * Returns: 1 on succes
 *          0 on failure, out of memory
 */
short set_progname_and_path(char *argv0)
{
    char *p;

    p = strdup(argv0);

    if(p == NULL)
    {
        /* Out of memory while copying program-path */
        return 0;
    }

    progpath = p;

    p = strrchr(progpath, (int) '/');

    if(p == NULL)
    {
        /* no forward slashes in name, maybe backward slashes? */
        p = strrchr(progpath, (int) '\\');
        if(p == NULL)
        {
            /* no path */
            progname = progpath;
            progpath = "";
        }
        else
        {
            p++;
            progname = strdup(p);
            if(progname == NULL)
            {
                /* Out of memory while copying program-name */
                return 0;
            }

            /* patch end of path to '\0' */
            *p = '\0';
        }
    }
    else
    {
        p++;
        progname = strdup(p);
        if(progname == NULL)
        {
            /* Out of memory while copying program-name */
            return 0;
        }

        /* patch end of path to '\0' */
        *p = '\0';
    }

    /* success, progpath and progname set */
    return 1;
}

/*
 * Return pointer to a string containing the path to the program
 */
char *get_progpath()
{
    return progpath;
}

/*
 * Return pointer to a string containing the name of the program
 */
char *get_progname()
{
    return progname;
}

/*
 * Compare call with rule-call.
 *
 * The rule-call can contain special characters:
 *
 * '@' match any letter
 * '!' match any digit
 * '?' match any character
 * '*' match 0 or more characters, any character
 * 
 */
short match_call(const char *call, const char *rule_call)
{
    const char *p, *q;

    p = call;
    q = rule_call;

    while(*q != '\0')
    {
        if(*q == '*')
        {
            /* advance to next character */
            q++;

            /* the following constuct is to match rule-calls of either */
            /* "*" or "*-<SSID>", this enables to-call selection on    */
            /* SSID to enable digipeating on SSID destination.         */

            /* now proceed until the character '*q' matches with */
            /* '*p' again                                        */
            while(*p != *q)
            {
                if(*p == '\0')
                {
                    /*
                     * *p finished while *q is not, no match, unless the
                     * the remainer of '*q' happens do be "-0" (the call does
                     * not have a SSID, otherwise '-' should have been found)
                     */
                    if(strcmp(q, "-0") == 0)
                    {
                        /* match for call without an SSID */
                        return 1;
                    }
                    else
                    {
                        /* no match */
                        return 0;
                    }
                }
                p++;
            }

            /* *p and *q are in sync now */
            if(*q == '\0')
            {
                /* the end is reached on both strings, match is completed */
                return 1;
            }
        }
        else
        {
            if(*p == '\0')
            {
                    /*
                     * *p finished while *q is not, no match, unless the
                     * the remainer of '*q' happens do be "-0" (the call does
                     * not have a SSID, otherwise '-' should have been found)
                     */
                    if(strcmp(q, "-0") == 0)
                    {
                        /* match for call without an SSID */
                        return 1;
                    }
                    else
                    {
                        /* no match */
                        return 0;
                    }
            }

            switch(*q) {
            case '?':
                    /* okay, whatever it is... */
                    break;
            case '@':
                    /* when not alphanumeric, quit */
                    if(!isalpha(*p))
                        return 0;
                    break;
            case '!':
                    /* when not a digit, quit */
                    if(!isdigit(*p))
                        return 0;
                    break;
            default:
                    if(*p != *q)
                    {
                        /* not the same, quit */
                        return 0;
                    }
                    break;
            }
            p++;
            q++;
        }
    }

    /* check if p is also finished */
    if(*p == '\0')
        return 1;

    return 0;
}

/*
 * Compare string with wild-string.
 *
 * The wild-string can contain special characters:
 *
 * '@' match any letter
 * '!' match any digit
 * '?' match any character
 * '*' match 0 or more characters, any character
 * 
 */
short match_string(const char *string, const char *wild_string)
{
    const char *p, *q;

    p = string;
    q = wild_string;

    while(*q != '\0')
    {
        if(*q == '*')
        {
            /* advance to next character */
            q++;

            /* now proceed until the character '*q' matches with */
            /* '*p' again                                        */
            while(*p != *q)
            {
                if(*p == '\0')
                {
                    /* *p finished while *q is not, no match */
                    return 0;
                }
                p++;
            }

            /* *p and *q are in sync now */
            if(*q == '\0')
            {
                /* the end is reached on both strings, match is completed */
                return 1;
            }
        }
        else
        {
            if(*p == '\0')
            {
                /* *p finished while *q is not, no match */
                return 0;
            }

            switch(*q) {
            case '?':
                    /* okay, whatever it is... */
                    break;
            case '@':
                    /* when not alphanumeric, quit, unless it is '@' */
                    if((!isalpha(*p)) && (*p != '@'))
                        return 0;
                    break;
            case '!':
                    /* when not a digit, quit, unless it is '!' */
                    if((!isdigit(*p)) && (*p != '!'))
                        return 0;
                    break;
            default:
                    if(*p != *q)
                    {
                        /* not the same, quit */
                        return 0;
                    }
                    break;
            }
            p++;
            q++;
        }
    }

    /* check if p is also finished */
    if(*p == '\0')
        return 1;

    return 0;
}

/*
 * Check if ports parameter contains correct characters
 *
 * Returns: 0 if not okay
 *          1 if okay for portnumbers and "all"
 *
 * Note: ports string will be converted to lowercase
 */
short check_ports(char *ports, const char *purpose, const char *place,
                    short line, porttype_t porttype)
{
    if(strlen(ports) >= OUTPORTS_LENGTH)
    {
        say("%s (%s) to long on %s: at line %d\r\n",
                purpose, ports, place, line);
        return 0;
    }

    if(stricmp(ports, "all") == 0)
    {
        /* "all" is accepted */
        return 1;
    }

    if((porttype == INCL_ALLBUT) && (stricmp(ports, "allbut") == 0))
    {
        /* "allbut" is accepted */
        return 1;
    }

    if(strspn(ports, "0123456789,") == strlen(ports))
    {
        /* no illegal characters, accepted */
        return 1;
    }

    say("%s (%s) has illegal format on %s: at line %d\r\n",
            purpose, ports, place, line);
    return 0;
}

/*
 * Check if the input string is a valid call
 *
 * Returns: 0 if not okay
 *          1 if okay
 */
short is_call(const char *c)
{
    short n;

    if(*c == '\0')
    {
        /* Missing call */
        return 0;
    }

    /* call-size counter to 0 */
    n = 0;

    do
    {
        /* count call size */
        n++;

        if(n == 7)
        {
            /* Illegal call length */
            return 0;
        }

        /*
         * Before the dash can be either a digit
         * or an uppercase alpha character
         */
        if(!isdigit(*c) && !isupper(*c))
        {
            /* Illegal character in call */
            return 0;
        }

        /* advance to next character */
        c++;

        /* if end of string, checking completed successfully */
        if(*c == '\0')
        {
            /* okay, call without SSID */
            return 1;
        }
    } while(*c != '-');

    /* if we end up here then a '-' was found */

    /* advance to first character of SSID */
    c++;

    if(*c == '\0')
    {
        /* Missing SSID */
        return 0;
    }

    if(*c == '1')
    {
        /* can be a 2 digit SSID */

        /* next character can be '\0' */
        c++;

        if(*c == '\0')
        {
            /* SSID is '1', okay */
            return 1;
        }

        /* remaining character shall be 0..5 */
        if((*c < '0') || (*c > '5'))
        {
            /* Impossible digit value */
            return 0;
        }
    }
    else
    {
        /* single digit SSID */
        if(!isdigit(*c))
        {
            /* Illegal character in SSID */
            return 0;
        }
    }

    /* next character shall be '\0' */
    c++;

    if(*c != '\0')
    {
        /* characters following the SSID */
        return 0;
    }
    else
    {
        /* checking completed successfully */
        return 1;
    }
}

/*
 * Determine the number of hops from the originator
 *
 * Returns: 0 if station has been heard directly
 *          n the number of hops to get here
 */
short number_of_hops(uidata_t *uidata_p)
{
    short hops;
    short i;

    hops = 0;
   
    if (uidata_p->distance != LOCAL)
    {
        for (i = 0; i < uidata_p->ndigi; i++)
        {
            if ((uidata_p->digi_flags[i] & H_FLAG) != 0)
            {
                hops++;
            }
        }
    }
    return hops;
}

/*
 * Interate through a list of ports.
 *
 * It is assumed that the ports_p array is already checked by check_ports
 * This function can not be nested!
 *
 * Function can itterate through port lists or "all". "allbut" is not
 * supported here.
 *
 * Port is in range 1..mac_ports()
 * 0  if there are no more ports
 * -1 if there are is an error in the port array
 */
short find_first_port(char *ports_p)
{
    char  number[MAX_ITOA_LENGTH];
    short i;

    /*
     * Default setup first, than handle the exceptions
     */
    iter_ports_p = ports_p;
    iter_allbut  = 0; 

    /* if "all" to all the ports we have */
    if(stricmp(ports_p, "all") == 0)
    {
        iter_ports[0] = '\0';
        for(i = 1; i <= mac_ports(); i++)
        {
            if(iter_ports[0] != '\0')
                strcat(iter_ports, ",");
            strcat(iter_ports, digi_itoa(i, number));
        }
        iter_ports_p = iter_ports;
    }

    return find_next_port();
}

/*
 * Get the next port from the iterator
 *
 * It is assumed that the ports_p array is already checked by check_ports
 * find_first_port is supposed to be called first. These functions can
 * not be nested!
 *
 * Port is in range 1..mac_ports()
 * 0  if there are no more ports
 * -1 if there are is an error in the port array
 */
short find_next_port()
{
    char *num_end_p;
    short i;

    if(iter_ports_p == NULL)
    {
        return 0;
    }

    i = atoi(iter_ports_p);

    num_end_p = strchr(iter_ports_p, (int) ',');

    if(num_end_p != NULL)
    {
        num_end_p++;
    }
    iter_ports_p = num_end_p;

    if((i <= 0) || (i > mac_ports()))
    {
        return -1; /* Error */
    }

    return i; 
}

/*
 * digi_convert_file
 *
 * The next function will:
 *
 * Convert forward or back-slashes to the correct directory separator
 * based on the platform. All slashes in the string will be converted.
 *
 * Add the program path to the filename if needed.
 *
 * On return the completed file will be returned in a allocated part of
 * memory. When out of memory the function will return NULL.
 */
char *digi_convert_file(char *filename)
{
    char *tmp;
    char *p;
    char *newfilename;

    /* see if we have to add progpath to the filename */
    tmp = filename;
    if( (strrchr(tmp, (int) '/') == NULL)
        &&
        (strrchr(tmp, (int) '\\') == NULL)
        &&
        (strrchr(tmp, (int) ':') == NULL)
      )
    {
        /* no path in name, add progpath in front  */
        p = malloc(strlen(get_progpath()) + strlen(tmp) + 1);
        if(p != NULL)
        {
            strcpy(p, get_progpath());
            strcat(p, tmp);
        }
        newfilename = p;
    }
    else
    {
        /* name includes path */
        newfilename = strdup(tmp);
    }

    tmp = newfilename;
    if(tmp != NULL)
    {
        while(*tmp != '\0')
        {
#ifndef _LINUX_
            if(*tmp == '/' )
            {
                *tmp = '\\';
            }
#else
            if(*tmp == '\\')
            {
                *tmp = '/';
            }
#endif /* _LINUX_ */
            if((*tmp == '\r') || (*tmp == '\n'))
            {
                *tmp = '\0';
            }
            tmp++;
        }
    }

    return newfilename;
}

extern char *tnc_string(uidata_t *uidata_p, distance_t distance)
{
    unsigned char c;
    short         i;
    static char   tnc_string[512];

    /*
     * Input has been checked before. Needed buffer-space for string:
     *
     * Name                    Example               Size
     * -----------------------------------------------------
     * Originator-worst case   "PE1DNN-15"           9 bytes
     * Separator               ">"                   1 byte
     * Destination worst case  "APRSXX-15"           9 bytes
     * Separator 8x            ","                   8 bytes
     * DIGI-1-worst case 8x    "NLDIGI-10"          72 bytes
     * 'digipeated' flag       "*"                   1 byte
     * End of header           ":"                   1 byte
     * Data                    "Hello, World!"     256 bytes
     * Null terminator         "\0"                  1 byte
     *                                           ----------- +
     * Total needed space                          358 bytes
     *
     * Allocated 512 bytes so this must be sufficient in call cases
     */

    /* clear string */
    tnc_string[0] = '\0';

    /* from */
    sprintf(&tnc_string[strlen(tnc_string)], "%s>", uidata_p->originator);
    /* to */
    sprintf(&tnc_string[strlen(tnc_string)],"%s", uidata_p->destination);

    for(i = 0; i < uidata_p->ndigi; i++)
    {
        /*
         * When distance is REMOTE all digis are dumped, otherwise
         * only digis with the H_FLAG set are dumped.
         */
        if( (distance == REMOTE)
            ||
            ((uidata_p->digi_flags[i] & H_FLAG) != 0)
          )
        {
            sprintf(&tnc_string[strlen(tnc_string)],",%s",
                    uidata_p->digipeater[i]);

            if((uidata_p->digi_flags[i] & H_FLAG) != 0)
            {
                /* only put an '*' on the last digi */
                if(i == (uidata_p->ndigi - 1))
                {
                    /* last call in digi-list */
                    sprintf(&tnc_string[strlen(tnc_string)],"*");
                }
                else
                {
                    if((uidata_p->digi_flags[i + 1] & H_FLAG) == 0)
                    {
                        /* next digi has no H_FLAG set */
                        sprintf(&tnc_string[strlen(tnc_string)],"*");
                    }
                }
            }
        }
    }
    sprintf(&tnc_string[strlen(tnc_string)],":");

    if(uidata_p->size > 0)
    {
        for(i = 0; i < uidata_p->size; i++)
        {
            c = uidata_p->data[i];
            if(iscntrl((short) c))
            {
                /* remove '\r' characters*/
                if(c != '\r')
                {
                    /*
                     * Print above char 27 as normal, don't
                     * damage MIC-E
                     */
                    if(c > 27)
                    {
                        sprintf(&tnc_string[strlen(tnc_string)],".");
                    }
                    else
                    {
                        sprintf(&tnc_string[strlen(tnc_string)],"%c", c);
                    }
                }
            }
            else
            {
                sprintf(&tnc_string[strlen(tnc_string)],"%c", c);
            }
        }
    }

    return tnc_string;
}

char *digi_itoa(int value, char *storage)
{
    sprintf(storage, "%d", value);
    return storage;
}

