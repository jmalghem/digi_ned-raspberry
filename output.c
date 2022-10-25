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
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "version.h"
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "read_ini.h"
#include "digipeat.h"
#include "output.h"

static FILE *l_fd             = NULL;
static FILE *tl_fd            = NULL;
static short log_active       = 1;
static short tnclog_active    = 1;
static short verbose          = 0;
static short display_activity = 0;
static short display_dx       = 0;

/*
 * This will switch on verbose output
 */
void set_verbose(void)
{
    verbose = 1;
}

/*
 * This will switch on massage repsonse tracing.
 */
void set_display_activity(void)
{
    display_activity = 1;
}

/*
 * This will switch on DX status tracing.
 */
void set_display_dx(void)
{
    display_dx = 1;
}

void toggle_verbose(void)
{
    if(verbose == 1)
    {
        say("Verbose switched off\r\n");
        verbose = 0;
    }
    else
    {
        say("Verbose switched on\r\n");
        verbose = 1;
    }
    display_separator();
}

void toggle_logging(void)
{
    char       stamp[40];
    time_t     curtime;
    struct tm *loctime;

    if(digi_logfile[0] == '\0')
    {
        /* we are not logging, do nothing */
        return;
    }

    /* Make a timestamp */

    /* Determine current time and convert into local representation. */
    curtime = time (NULL);
    loctime = localtime (&curtime);

    /* Format the time */
    strftime(stamp, MAX_MESSAGE_LENGTH- 1, "%b %d %Y - %H:%M:%S", loctime);

    if(log_active == 1)
    {
        say("Logging to file switched off [%s]\r\n", stamp);
        log_active = 0;

        /* close the log-file */
        if(l_fd != NULL)
        {
            /* put visble separator in logfile */
            fprintf(l_fd,"======================================="
                         "=======================================\r\n");
            fflush(l_fd);
            fclose(l_fd);
            l_fd = NULL;
        }
    }
    else
    {
        log_active = 1;

        /* (re)open the logfile to append to the end */
        if(l_fd == NULL)
        {
            l_fd = fopen(digi_logfile, "ab");
        }
        say("Logging to file switched on  [%s]\r\n", stamp);
    }
    display_separator();
}

void toggle_tnclogging(void)
{
    char       stamp[40];
    time_t     curtime;
    struct tm *loctime;

    if(digi_tnclogfile[0] == '\0')
    {
        /* we are not logging, do nothing */
        return;
    }

    /* Make a timestamp */

    /* Determine current time and convert into local representation. */
    curtime = time (NULL);
    loctime = localtime (&curtime);

    /* Format the time */
    strftime(stamp, MAX_MESSAGE_LENGTH- 1, "%b %d %Y - %H:%M:%S", loctime);

    if(tnclog_active == 1)
    {
        say("TNC logging to file switched off [%s]\r\n", stamp);
        tnclog_active = 0;

        /* close the log-file */
        if(tl_fd != NULL)
        {
            fflush(tl_fd);
            fclose(tl_fd);
            tl_fd = NULL;
        }
    }
    else
    {
        tnclog_active = 1;

        /* (re)open the tnclogfile to append to the end */
        if(tl_fd == NULL)
        {
            tl_fd = fopen(digi_tnclogfile, "a");
        }
        say("TNC logging to file switched on  [%s]\r\n", stamp);
    }
    display_separator();
}

void toggle_display_activity(void)
{
    if(display_activity == 1)
    {
        say("Display activity switched off\r\n");
        display_activity = 0;
    }
    else
    {
        say("Display activity switched on\r\n");
        display_activity = 1;
    }
    display_separator();
}

extern void common_say(short verbose, const char *s)
{
    static time_t reopen_time = 0UL;

    if(verbose != 0)
    {
        printf("%s", s);
        fflush(stdout);
    }

    if(log_active == 1)
    {
        if(digi_logfile[0] != '\0')
        {
            if(l_fd == NULL)
            {
                l_fd = fopen(digi_logfile, "wb");
                reopen_time = time(NULL) + 60UL;
            }

            if(l_fd != NULL)
            {
                fprintf(l_fd,"%s", s);
                fflush(l_fd);

                if(time(NULL) > reopen_time)
                {
                    fclose(l_fd);
                    l_fd = fopen(digi_logfile, "ab");
                    reopen_time = time(NULL) + 60UL;
                }
            }
        }
    }
}

extern void say(const char *fmt, ...)
{
    va_list args;
    char    s[1024];

    va_start(args, fmt);
    vsprintf(s, fmt, args);
    va_end(args);

    common_say(1, s);
}

extern void vsay(const char *fmt, ...)
{
    va_list args;
    char    s[1024];

    va_start(args, fmt);
    vsprintf(s, fmt, args);
    va_end(args);

    common_say(verbose, s);
}

extern void asay(const char *fmt, ...)
{
    va_list args;
    char    s[1024];

    va_start(args, fmt);
    vsprintf(s, fmt, args);
    va_end(args);

    common_say(display_activity, s);
}

extern void dsay(const char *fmt, ...)
{
    va_list args;
    char    s[1024];

    va_start(args, fmt);
    vsprintf(s, fmt, args);
    va_end(args);

    common_say(display_dx, s);
}

extern void tnc_log(uidata_t *uidata_p, distance_t distance)
{
    static time_t  reopen_time = 0UL;
    char   *s;

    if(tnclog_active == 1)
    {
        /* only log normal AX.25 UI frames, if not, return */
        if(((uidata_p->primitive & ~0x10) != 0x03) || (uidata_p->pid != 0xf0))
        {
            /* not a normal AX.25 UI frame, return */
            return;
        }

        if(digi_tnclogfile[0] != '\0')
        {
            if(tl_fd == NULL)
            {
                tl_fd = fopen(digi_tnclogfile, "a");
                reopen_time = time(NULL) + 60UL;
            }

            if(tl_fd != NULL)
            {
                s = tnc_string(uidata_p, distance);

                /* dump the line to the logfile */
                fprintf(tl_fd, "%s\n", s);
                fflush(tl_fd);

                if(time(NULL) > reopen_time)
                {
                    fclose(tl_fd);
                    tl_fd = fopen(digi_tnclogfile, "a");
                    reopen_time = time(NULL) + 60UL;
                }
            }
        }
    }
}

static void dump_uidata_common(uidata_t *uidata_p, distance_t distance)
{
    short         i;
    short         did_digi;
    unsigned char c;

    vsay("%s > %s", uidata_p->originator, uidata_p->destination);

    /* remember if we already displayed a digi, used for " via" text */
    did_digi = 0;

    for(i = 0; i < uidata_p->ndigi; i++)
    {
        /* dump digi:
         *
         * When distance is REMOTE all digis are dumped, otherwise
         * only digis with the H_FLAG set are dumped.
         */
        if( (distance == REMOTE)
            ||
            ((uidata_p->digi_flags[i] & H_FLAG) != 0)
          )
        {
            if(did_digi == 0)
            {
                /* first digi in output, add " via" text */
                vsay(" via");
                did_digi = 1;
            }
            vsay(" %s", uidata_p->digipeater[i]);
            if((uidata_p->digi_flags[i] & H_FLAG) != 0)
            {
                vsay("*");
            }
            if((uidata_p->digi_flags[i] & RR_FLAG) != RR_FLAG)
            {
                vsay("!");
            }
        }
    }

    /* get primitive in temp variable, strip poll bit */
    i = uidata_p->primitive & ~0x10;

    if((i & 0x01) == 0)
    {
        /* I frame */
        vsay(" I%d,%d", (i >> 5) & 0x07, (i >> 1) & 0x07);
    }
    else if((i & 0x02) == 0)
    {
        /* S frame */
        switch((i >> 2) & 0x03) {
        case 0:
                vsay(" RR");
                break;
        case 1:
                vsay(" RNR");
                break;
        case 2:
                vsay(" REJ");
                break;
        default:
                vsay(" ???");
                break;
        }
        vsay("%d", (i >> 5) & 0x07);
    }
    else
    {
        /* U frame */
        switch((i >> 2) & 0x3f) {
        case 0x00:
                vsay(" UI");
                break;
        case 0x03:
                vsay(" DM");
                break;
        case 0x0B:
                vsay(" SABM");
                break;
        case 0x10:
                vsay(" DISC");
                break;
        case 0x18:
                vsay(" UA");
                break;
        case 0x21:
                vsay(" FRMR");
                break;
        default:
                vsay(" ???");
                break;
        }
    }

    if((uidata_p->primitive & 0x10) == 0)
    {
        if((uidata_p->dest_flags & C_FLAG) != 0)
            vsay("^");
        else
            vsay("v");
    }
    else
    {
        if((uidata_p->dest_flags & C_FLAG) != 0)
            vsay("+");
        else
            vsay("-");
    }

    /* if we have a PID */
    if(uidata_p->pid != 0xffff)
        vsay(" PID=%X", ((short) uidata_p->pid) & 0x00ff);

    if(uidata_p->size > 0)
    {
        vsay(" %d bytes\r\n", uidata_p->size);

        for(i = 0; i < uidata_p->size; i++)
        {
            c = uidata_p->data[i];
            if(iscntrl((short) c))
            {
                if(c == '\r')
                {
                    vsay("\r\n");
                }
                else
                    vsay(".");
            }
            else
            {
                vsay("%c", c);
            }
        }
    }
    vsay("\r\n");
}

void dump_uidata_from(uidata_t *uidata_p)
{
    char          stamp[MAX_MESSAGE_LENGTH + 1];
    struct tm    *t;
    time_t        now;

    /* show timestamp */
    now = time(NULL);
    tzset();
    t = localtime(&now);
    strftime(stamp, MAX_MESSAGE_LENGTH- 1, "[%b %d %Y - %H:%M:%S]", t);
    vsay(stamp);

    vsay("\r\nfrom:%d ", (short) uidata_p->port + 1);

    dump_uidata_common(uidata_p, REMOTE);

    if(uidata_p->distance == LOCAL)
    {
        vsay("This is a local station\r\n");
    }
}

void dump_uidata_to(char *to, uidata_t *uidata_p)
{
    char  output[OUTPORTS_LENGTH];
    char *p,*q;
    short i;

    if(uidata_p->distance == LOCAL)
    {
        /* dump output for remote and local ports */
        for(i = 0; i < 2; i++)
        {
            p = to;
            q = &(output[0]);
            while(*p != '\0')
            {
                if((*p >= '1') && (*p <= '8'))
                {
                    if(port_is_local((short) (*p - '1')) == i)
                    {
                        /* remote port, add it */
                        *q = *p;
                        q++;
                        *q = ',';
                        q++;
                    }
                }
                p++;
            }
            /* check if any ports have been added */
            if(q != &(output[0]))
            {
                /* overwrite the last ',' with a '\0' */
                q--;
                *q = '\0';

                vsay("to:%s ", output);

                if(i == 0)
                {
                    dump_uidata_common(uidata_p, REMOTE);
                }
                else
                {
                    dump_uidata_common(uidata_p, LOCAL);
                }
            }
        }
    }
    else
    {
        /* dump one time with all ports */
        vsay("to:%s ", to);

        dump_uidata_common(uidata_p, REMOTE);
    }
}

void dump_uidata_preempt(uidata_t *uidata_p)
{
    vsay("\r\nmodified:%d ", (short) uidata_p->port + 1);

    dump_uidata_common(uidata_p, REMOTE);
}

void dump_raw(frame_t *frame_p)
{
    short i, c;

    vsay("Received from port %d\r\n", (short) frame_p->port + 1);

    for(i = 0; i < frame_p->len; i++)
    {
        c = ((short) frame_p->frame[i]) & 0x00ff;
        if(iscntrl((short) c))
        {
            if(c == '\r')
            {
                vsay("\r\n");
            }
            else
            {
                vsay(".");
            }
        }
        else
        {
            vsay("%c", c);
        }
    }
    vsay("\r\n");

    /* one more time in HEX */
    for(i = 0; i < frame_p->len; i++)
    {
        c = ((short) frame_p->frame[i]) & 0x00ff;
        vsay("%02x ", c);
    }
    vsay("\r\n");
}

void dump_rule(char *type, short port, digipeat_t *dp)
{
    vsay("Process rule: %s: %d %s %s",
            type, port + 1, dp->digi, dp->output);

    if((strcmp(type, "digito") == 0) ||(strcmp(type, "digissid") == 0))
    {
        vsay(" %d", dp->ssid);
    }

    if(dp->operation != NONE)
    {
        switch(dp->operation) {
        case ADD:
                vsay(" add");
                break;
        case REPLACE:
                vsay(" replace");
                break;
        case NEW:
                vsay(" new");
                break;
        case SWAP:
                vsay(" swap");
                break;
        case HIJACK:
                vsay(" hijack");
                break;
        case ERASE:
                vsay(" erase");
                break;
        case KEEP:
                vsay(" keep");
                break;
        case SHIFT:
                vsay(" shift");
                break;
        default:
                vsay(" ???");
                break;
        }

        if(dp->use != 1)
        {
            vsay("%d", dp->use);
        }

        if((dp->operation != ERASE) && (dp->operation != KEEP))
        {
            vsay(" %s", dp->mapping);
        }
    }
    vsay("\r\n");
}

void dump_preempt(short port, preempt_t *pp, preempt_keep_t *pk)
{
    short first;

    vsay("preprocessing frame for out of order digi handling\r\n");

    if(strcmp(pp->digi, pp->outd) ==  0)
    {
        vsay("Modify: preempt: %d %s", port + 1, pp->digi);
    }
    else
    {
        vsay("Modify: preempt: %d %s %s", port + 1, pp->digi, pp->outd);
    }

    /*
     * Show which calls to keep
     */
    first = 1;
    while(pk != NULL)
    {
        if(pk->keep == KEPT)
        {
            if(first == 1)
            {
                first = 0;
                vsay(", keep: %s", pk->digi);
            }
            else
            {
                vsay(", %s", pk->digi);
            }
        }
        pk = pk->next;
    }
    vsay("\r\n");
}

#ifndef _LINUX_
/* Help for DOS version */
void display_help(void)
{
    say("DIGI_NED %s - %s\r\n\r\n", VERSION, __DATE__);
    say("Usage:\r\n\r\ndigi_ned [-h] | [-v] [-a] [-d] [<digi_ned.ini>]");
    say("\r\n\r\nTo use this digipeater you must load 'ax25_mac' ");
    say("first.\r\n\r\n");
    say("The '-v' option wil switch on verbose output.\r\n");
    say("The '-a' option will turn on display of activity.\r\n");
    say("The '-d' option will turn on display of DX handling.\r\n");
    say("\r\nAn optional last argument for 'digi_ned' is the ");
    say("filename of the .ini file with\r\ndigipeating rules. ");
    say("Default is 'digi_ned.ini' in the current working ");
    say("directory.\r\n\r\n");
    say("The '-h' option will give this help text.\r\n\r\n");
    say("Good luck - Henk, PE1DNN\r\n\r\n");
}

void display_key_help(void)
{
    say("\r\nDIGI_NED Keyboard keys\r\n----------------------\r\n");
    say("Commands: hold ALT key and press letter\r\n");
    say("ALT-A ............ Toggle activity output\r\n");
    say("ALT-B ............ Send out all beacons\r\n");
    say("ALT-D ............ Start Shell\r\n");
    say("ALT-H ............ Key Help\r\n");
    say("ALT-L ............ Toggle logging\r\n");
    say("ALT-T ............ Toggle TNC-logging\r\n");
    say("ALT-V ............ Toggle verbose output\r\n");
    say("ALT-X ............ Exit digipeater\r\n");
    say("Text input followed by CR is taken as keyboard message ");
    say("to the digipeater\r\n");

    display_separator();
}
#else /* ifndef _LINUX_ */
/* Help for Unix type systems */

/* Both old and new AX.25 tools use AX.25 via the kernel */
#ifdef OLD_AX25
#define KERNEL_AX25
#endif /* OLD_AX25 */
#ifdef NEW_AX25
#define KERNEL_AX25
#endif /* NEW_AX25 */

#ifdef KERNEL_AX25
/* Help for Unix type systems using the AX.25 kernel support */

void display_help(void)
{
    say("DIGI_NED %s - %s\r\n\r\n", VERSION, __DATE__);
    say("Usage:\r\n\r\ndigi_ned [-h] | -p <port> [-p <port>...] [-v] [-a] ");
    say("[-d] [-k] [<digi_ned.ini>]\r\n\r\n");
    say("To use this digipeater you must specify the ports using the ");
    say("'-p' option.\r\nThis option takes AX.25 ports which are ");
    say("specified in the 'axports'\r\nfile in /etc/ax25. At least one ");
    say("port needs to be specified at startup.\r\n\r\n");
    say("The '-v' option wil switch on verbose output.\r\n");
    say("The '-a' option will turn on display of activity.\r\n");
    say("The '-d' option will turn on display of DX handling.\r\n");
    say("The '-k' option will enable keyboard input.\r\n");
    say("\r\nAn optional last argument for 'digi_ned' is the ");
    say("filename of the .ini file with\r\ndigipeating rules. ");
    say("Default is 'digi_ned.ini' in the current working ");
    say("directory.\r\n\r\n");
    say("The '-h' option will give this help text.\r\n\r\n");
    say("Good luck - Henk, PE1DNN\r\n\r\n");
}

void display_help_root(void)
{
    say("\r\nDIGI_NED startup error\r\n\r\n");
    say("You must run DIGI_NED with root previlege. For security ");
    say("reasons only root\r\ncan get raw socket access in Linux. ");
    say("Either run DIGI_NED when logged in as\r\nroot (or after ");
    say("\"su\") or do\r\n\r\n  $ chown root digi_ned\r\n");
    say("  $ chmod 6755 digi_ned\r\n\r\nDoing this also requires root ");
    say("previlege too but after that you can run\r\nDIGI_NED as ");
    say("any user\r\n\r\nI hope this helps -  Henk, PE1DNN\r\n\r\n");
}

#else /* OLD_AX25 || NEW_AX25 */
#ifdef KISS

/* Help for Unix type systems using KISS */

void display_help(void)
{
    say("DIGI_NED %s - %s\r\n\r\n", VERSION, __DATE__);
    say("Usage:\r\n\r\ndigi_ned [-h] | -p <port> [-v] [-a] ");
    say("[-d] [-k] [<digi_ned.ini>]\r\n\r\n");
    say("To use this digipeater you must specify the port using the '-p' ");
    say("option.\r\nThis option takes a string that defindes the port in the ");
    say("following\r\nform:\r\n\r\n");
    say("-p <tty-device>:<baudrate>:<nr-of-ports>\r\n\r\n");
    say("The KISS device needs to have at least one port, ");
    say("more ports follow the\r\nMKISS standaard. There can be only one ");
    say("tty-device.\r\n\r\n");
    say("The '-v' option wil switch on verbose output.\r\n");
    say("The '-a' option will turn on display of activity.\r\n");
    say("The '-d' option will turn on display of DX handling.\r\n");
    say("The '-k' option will enable keyboard input.\r\n");
    say("\r\nAn optional last argument for 'digi_ned' is the ");
    say("filename of the .ini file with\r\ndigipeating rules. ");
    say("Default is 'digi_ned.ini' in the current working ");
    say("directory.\r\n\r\n");
    say("The '-h' option will give this help text.\r\n\r\n");
    say("Good luck - Henk, PE1DNN\r\n\r\n");
}

#else /* KISS */
#ifdef AGWPE
/* Help for Unix type systems using AGWPE via TCP/IP */

void display_help(void)
{
    say("DIGI_NED %s - %s\r\n\r\n", VERSION, __DATE__);
    say("Usage:\r\n\r\ndigi_ned [-h] | -p <port> [-v] [-a] ");
    say("[-d] [-k] [<digi_ned.ini>]\r\n\r\n");
    say("To use this digipeater you must specify the port using the '-p' ");
    say("option.\r\nThis option takes a string that defindes the port in the ");
    say("following\r\nform:\r\n\r\n");
    say("-p <hostname>:<port>\r\n\r\n");
    say("The AGWPE engine needs to have at least one port. There can be only ");
    say("\r\nbe one port specification\r\n\r\n");
    say("The '-v' option wil switch on verbose output.\r\n");
    say("The '-a' option will turn on display of activity.\r\n");
    say("The '-d' option will turn on display of DX handling.\r\n");
    say("The '-k' option will enable keyboard input.\r\n");
    say("\r\nAn optional last argument for 'digi_ned' is the ");
    say("filename of the .ini file with\r\ndigipeating rules. ");
    say("Default is 'digi_ned.ini' in the current working ");
    say("directory.\r\n\r\n");
    say("The '-h' option will give this help text.\r\n\r\n");
    say("Good luck - Henk, PE1DNN\r\n\r\n");
}
#endif /* AGWPE */
#endif /* KISS */
#endif /* KERNEL_AX25 */

void display_key_help(void)
{
    say("\r\nDIGI_NED Keyboard keys\r\n----------------------\r\n");
    say("Commands: hold ALT key and press letter\r\n");
    say("          or press ESC key followed by letter\r\n");
    say("ALT-A ............ Toggle activity output\r\n");
    say("ALT-B ............ Send out all beacons\r\n");
    say("ALT-H ............ Key Help\r\n");
    say("ALT-L ............ Toggle logging\r\n");
    say("ALT-T ............ Toggle TNC-logging\r\n");
    say("ALT-V ............ Toggle verbose output\r\n");
    say("ALT-X ............ Exit digipeater\r\n");
    say("Text input followed by CR is taken as keyboard message ");
    say("to the digipeater\r\n");

    display_separator();
}
#endif /* _LINUX_ */

void display_separator(void)
{
    vsay("---------------------------------------"
         "---------------------------------------\r\n");
}
