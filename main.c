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
#include <time.h>
#include <signal.h>
#ifndef _LINUX_
#include <conio.h>
#include <dos.h>
#include <direct.h>
#include <dir.h>
#else
#include <unistd.h>
#include "linux.h"
#endif /* _LINUX_ */
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "mac_if.h"
#include "read_ini.h"
#include "digipeat.h"
#include "send.h"
#include "message.h"
#include "output.h"
#include "kiss.h"
#include "mheard.h"
#include "command.h"
#include "telemetr.h"
#include "serial.h"
#include "predict.h"
#include "sat.h"
#include "wx.h"

#ifndef _LINUX_
#pragma argsused
#endif
void leave(int sig)
{
#ifdef _LINUX_
    kb_exit();
#endif
#ifdef _SERIAL_
    serial_exit();
#endif /* _SERIAL_ */
    fflush(stdout);
    mac_exit();
    exit(0);
}

#ifndef _LINUX_
void dos_shell(void)
{
    char    *p;
    char     cwd[MAXPATH + 1];
    short    disk;

    say("Shell to DOS, type exit to return to DIGI_NED\r\n");
    display_separator();

    getcwd(cwd, MAXPATH);
    disk = _getdrive();

    p = getenv("COMSPEC");

    system(p);

    /* change back to our own directory */
    chdir(cwd);
    _chdrive(disk);

    /*
     * flush pending input, we could have been away
     * for a long time, don't transmit old data
     */
    mac_flush();

    say("Welcome back to DIGI_NED, running\r\n");
    display_separator();
}
#endif /* _LINUX_ */

/*
 * Entry point of DIGI_NED. First parse parameters, then enter the
 * program-loop.
 */
int main(int argc, char *argv[])
{
    uidata_t    uidata;
    short       result;
    short       errflg;
    char       *p,*q;
    short       optind;
    short       c;
    short       verbose;
    short       display_activity;
    short       display_dx;
    char        kb_command[MAX_MESSAGE_LENGTH + 1];
    short       kb_pos;
#ifdef _LINUX_
    short       ports;
    short       enable_kb;
#else
    char        zone[40];
#endif

    /* index in keyboard command array to 0 */
    kb_pos = 0;

    /* display flag used for activity display */
    display_activity = 0;

    /* display flag used for dx display */
    display_dx = 0;

#ifdef _LINUX_
    /* flag if keyboard should be enabled */
    enable_kb = 0;

    /* number of ports to 0 */
    ports = 0;
#endif

#ifndef _LINUX_
    /* wether or not to use direct video access */
    directvideo = 1;
#endif

    /* assume no errors */
    errflg = 0;

    /* verbose flag used for startup message */
    verbose = 0;

    /* remember the path to the executable and the executable name */
    result = set_progname_and_path(argv[0]);

    if(result == 0)
    {
        say("Out of memory while copying program-path or name\r\n");
        return -1;
    }

    /*
     * I wanted to use 'getopt' but that function is not available in
     * BCC3.1
     */
    optind = 1;
    while( (optind < argc)
           && 
           (
             (argv[optind][0] == '-')
             ||
             (argv[optind][0] == '/')
           )
           && 
           (argv[optind][1] != '\0')
           && 
           (argv[optind][2] == '\0')
         )
    {
        c = argv[optind][1];

        switch (c) {
#ifdef _LINUX_
        case 'P':
        case 'p':
                    if((optind + 1) != argc)
                    {
                        optind++;

                        if(add_digi_port(argv[optind]) == 1)
                        {
                            ports++;
                        }
                        else
                        {
                            say("Port format or parameter error, see"
                                " documentation for correct format\r\n");
                            errflg++;
                        }
                    }
                    else
                    {
                        say("Option '-p' missing a port specification\r\n");
                    }
                    break;
        case 'K':
        case 'k':
                    enable_kb = 1;
                    break;
#endif /* _LINUX_ */
        case 'V':
        case 'v':
                    set_verbose();
                    verbose = 1;
                    break;
        case 'H':
        case 'h':
                    display_help();
                    return 0;
        case 'A':
        case 'a':
                    set_display_activity();
                    display_activity = 1;
                    break;
        case 'D':
        case 'd':
                    set_display_dx();
                    display_dx = 1;
                    break;
        default:
                    say("Invalid argument on commandline.\r\n");
                    errflg++;
                    break;
        }
        optind++;
    }
    if((optind + 1) < argc)
    {
        /*
         * more than 1 option still not parsed, there van only be one
         * option still left over at this time and that is the optional
         * specification of the .ini file. Now there are more than one,
         * which is wrong.
         */
        say("Commandline error.\r\n");
        errflg++;
    }

#ifdef _LINUX_
    if ((errflg != 0) || (ports == 0))
    {
        say("Use -h option for help about the startup parameters\r\n");
        return -1;
    }

    if(mac_init() == 0)
    {
        say("Not all AX25 ports seem to be available!\r\n");
        return -1;
    }
    else
    {
        vsay("AX25 ports initialized\r\n");
    }

    vsay("Using %d active ports\r\n", mac_ports());
#else
    if (errflg != 0)
    {
        say("Use -h option for help about the startup parameters\r\n");
        return -1;
    }

    if(mac_init() == 0)
    {
        say("Can't find AX25_MAC in memory!\r\n");
        return -1;
    }
    else
    {
        vsay("AX25_MAC found at interrupt vector 0x%02x\r\n", mac_vector);
    }

    vsay("AX25_MAC has %d active ports\r\n", mac_ports());
#endif /* _LINUX_ */

    if (argc == (optind + 1))
    {
        p = strdup(argv[optind]);
        if(p == NULL)
        {
            say("Out of memory while assembling path "
                            "to the .ini file\r\n");
            return -1;
        }
    }
    else
    {
        p = strdup("digi_ned.ini");
        if(p == NULL)
        {
            say("Out of memory while assembling path "
                            "to the .ini file\r\n");
            return -1;
        }
    }

    /* see if we have to add progpath to the filename */
    if( (strrchr(p, (int) '/') == NULL)
        &&
        (strrchr(p, (int) '\\') == NULL)
        &&
        (strrchr(p, (int) ':') == NULL)
      )
    {
        /* no path in name, add progpath in front  */
        q = malloc(strlen(get_progpath()) + strlen(p) + 1);
        if(q == NULL)
        {
            say("Out of memory while assembling path "
                            "to the .ini file\r\n");
            return -1;
        }
        strcpy(q, get_progpath());
        strcat(q, p);
        free(p);
        p = q;
    }

    /* read the INI file */
    result = read_ini(p);

    /* .ini filename not longer needed */
    free(p);

    if(result != 1)
    {
        say("Missing/wrong mandatory settings in .ini file\r\n");
        return -1;
    }

#ifndef _LINUX_
    /* For DOS environment make sure TZ is set */
    if (getenv("TZ") == NULL)
    {
        /*
         * Create TZ to make the time for the satellite functions
         * work right.
         */
        if(digi_utc_offset > 0)
        {
            sprintf(zone,"TZ=UTC-%3.1f", digi_utc_offset);
        }
        else
        {
            sprintf(zone,"TZ=UTC+%3.1f", -digi_utc_offset);
        }
        (void) putenv(zone);
        tzset();
    }
#endif /* _LINUX_ */

#ifdef _SATTRACKER_
    /* read the satellite database and load the QTH data from the .ini file*/
    if(digi_sat_active == 1)
    {
        result = sat_init();
        switch (result)
        {
            case 0:
                    say("Unable to load QTH data from the .ini "
                                    "file\r\n");
                    return -1;
            case 1:
                    say("Unable to load satellite database: "
                                    "%s\r\n", digi_sat_file);
                    return -1;
            case 2:
                    asay("QTH data and satellite database successfully "
                         "loaded.\r\n");
                    break;
        }
    }
#endif /* _SATTRACKER_ */

    /* initialize message system */
    init_message();

#ifdef _SATTRACKER_
    /* Initialize the satellite object list */
    if(digi_sat_active == 1)
    {
        init_satobject_list();
    }
#endif /* _SATTRACKER_ */

    /* program loop, exit with ALT-X or SIGINT */
    if(verbose == 0)
    {
#ifdef _LINUX_
        /*
         * For Linux the message differs for keyboard enabled and keyboard
         * not enabled. If not enabled ALT-X does not work. For DOS the
         * keyboard is always enabled (reason: you can't start DIGI_NED
         * in background in DOS...
         */
        if(enable_kb == 1)
        {
#endif /* _LINUX_ */
            say("DIGI_NED running in non-verbose mode, use ALT-X or send a "
                "SIGINT signal\r\n(usually Ctrl-C) to exit\r\n");
#ifdef _LINUX_
        }
        else
        {
            say("DIGI_NED running in non-verbose mode, send a "
                "SIGINT signal (usually Ctrl-C)\r\nto exit\r\n");
        }
#endif /* _LINUX_ */
    }
    else
    {
#ifdef _LINUX_
        if(enable_kb == 1)
        {
#endif /* _LINUX_ */
            say("DIGI_NED running, use ALT-X or send a SIGINT signal "
                "(usually Ctrl-C) to exit\r\n");
#ifdef _LINUX_
        }
        else
        {
            say("DIGI_NED running, send a SIGINT signal "
                "(usually Ctrl-C) to exit\r\n");
        }
#endif /* _LINUX_ */
    }

#ifdef _LINUX_
    /*
     * Bail out on signals like this
     */
    signal(SIGHUP, leave);
    signal(SIGTERM, leave);
    signal(SIGQUIT, leave);
    signal(SIGUSR1, leave);
    signal(SIGUSR2, leave);
#endif /* _LINUX_ */

    /*
     * Use SIGINT for DOS and Linux, DOS can be stopped by ^C now also
     */
    signal(SIGINT, leave);

#ifdef _LINUX_
    /*
     * Initialize keyboard input after the signal catchers are installed
     */
    if(enable_kb == 1)
    {
        kb_init();
    }
#endif /* _LINUX_ */

    if (display_activity == 1)
    {
       say("DIGI_NED will display honored and ignored requests\r\n");
    }

    if (display_dx == 1)
    {
       say("DIGI_NED will display its internal DX information\r\n");
    }

    display_separator();

    for (;;)
    {
        result = get_uidata(&uidata);

        switch(result) {
        case -1:
                exit(1);
                break;
        case 1:
                dump_uidata_from(&uidata);

                /* first check on messages */
                if(recv_message(&uidata) == 0)
                {
                    /* and then digipeat */
                    do_digipeat(&uidata);
                }

                do_mheard(&uidata);

                display_separator();
                break;
        case 2:
                /* unsuported PID, already dumped, no further action */
                display_separator();
                break;
        case 3:
                /* corrupt frame, do nothing */
                display_separator();
                break;
        default:
                break;
        }

        /*
         * Check is there are still initial commands to be executed, if
         * there are then this call will execute the command which is next
         * due.
         */
        execute_ini_cmd();

        if (kbhit ())
        {
            c = getch();

            if(c == 0)
            {
                /* ALT */
                if(kbhit())
                    c = getch();

                if (c == 0x2D)      /* ALT-X */
                    break;

                switch(c) {
                case 0x2f:          /* ALT-V */
                    toggle_verbose();
                    break;
                case 0x26:          /* ALT-L */
                    toggle_logging();
                    break;
                case 0x14:          /* ALT-T */
                    toggle_tnclogging();
                    break;
                case 0x1E:          /* ALT-A */
                    toggle_display_activity();
                    break;
                case 0x23:          /* ALT-H */
                    display_key_help();
                    break;
                case 0x30:          /* ALT-B */
                    send_all(BEACON);
                    break;
#ifndef _LINUX_
                case 0x20:          /* ALT-D */
                    dos_shell();
                    break;
#endif /* _LINUX_ */
                default:
                    /* do nothing */
                    break;
                }
            }
            else
            {
                /* no ALT */
                if((c >= ' ') && (c < 128))
                {
                    if(kb_pos < MAX_MESSAGE_LENGTH)
                    {
                        putchar(c);
                        fflush(stdout);
                        kb_command[kb_pos] = c;
                        kb_pos++;
                    }
                }
                else
                {
                    if(c == '\r')
                    {
                        putchar('\r');
                        putchar('\n');
                        fflush(stdout);
                        kb_command[kb_pos] = '\0';
                        kb_pos = 0;
                        internal_message(kb_command);
                        display_separator();
                    }
                    else if(c == '\b')
                    {
                        if(kb_pos > 0)
                        {
                            kb_pos--;
                            putchar('\b');
                            putchar(' ');
                            putchar('\b');
                            fflush(stdout);
                        }
                    }
                }
            }
        }

#ifdef _SATTRACKER_
        /* handle satellite transmissions */
        if(digi_sat_active == 1)
        {
            do_satellite();
        }
#endif /* _SATTRACKER_ */

        /* handle beacon transmissions */
        do_send();

#ifdef _TELEMETRY_
        /* handle telemetry transmissions */
        do_telemetry();
#endif /* _TELEMETRY_ */

#ifdef _WX_
        /* update accumulating WX variables */
        update_wx_var();
#endif /* _WX_ */

        /* handle message transmissions */
        do_messages();

        /* handle serial data ransmissions */
#ifdef _SERIAL_
        do_serial();
#endif /* _SERIAL_ */

#ifdef _LINUX_
       /* sleep for 25ms to prevent taking all CPU time while looping */
       usleep(25000);
#endif /* _LINUX_ */
    }

#ifdef _LINUX_
    kb_exit();
#endif

#ifdef _SERIAL_
    serial_exit();
#endif /* _SERIAL_ */

    fflush(stdout);
    mac_exit();
    return 0;
}
