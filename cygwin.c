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
#ifdef __CYGWIN32__

#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <string.h>
#include <process.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "digicons.h"
#include "digitype.h"
#include "cygwin.h"
#include "output.h"

#define MAXARGS 256

/*
 * Cygwin system replacement that starts command.com or cmd.exe
 * or whatever the envionment variable COMSPEC says instead of /bin/sh
 */
int cmd_system(char *command)
{
    char *start;             /* Windows command launcher                   */
    char *cmd;               /* Command processor from COMSPEC             */
    char *cmd_copy;          /* Copy for retrieval of basename             */
    char *command_copy;      /* Copy for strtok handling                   */
    int   exitcode;          /* Exitcode after execution                   */
    int   exitcode2;         /* Exitcode after dummy execution             */
    char *argv[MAXARGS + 1]; /* Argument array for up to MAXARGS arguments */
    int   argc;              /* Argument counter                           */
    char *p;                 /* Helper pointer for tokenizing command      */
    struct utsname buf;      /* Struct for uname                           */
    enum {M_HASSTART,
          M_DIRECT} method;  /* method how to start command                */

    /*
     * Cygwin cannot use system() since that will use /bin/sh,
     * we want to use COMMAND.COM or CMD.EXE in a Windows
     * environment.
     *
     * Using command.com is more complicated that it looks, fork/exec
     * or spawn of COMMAND.COM under Windows 98 crashes the application.
     * Even Cygwin's bash has this problem.
     *
     * The solution used here is to use the command "start" to launch the
     * command. However start is a program under Windows 95/98/ME
     * (normally in C:\WINDOWS\COMMAND) and is not present in Windows XP.
     * In Windows XP 'start' is buildin in CMD.EXE, but we can't launch it
     * via CMD.EXE because than redirection does not work anymore (CMD.EXE
     * redirects the output of start).
     *
     * So for Windows 95, 98 and ME 'start' is used, for XP 'CMD.EXE' is
     * launched directly.
     *
     * The command processor to start is actually taken from the
     * environment variable COMSPEC. If COMSPEC is not set COMMAND.COM
     * is assumed, the path is searched to find it in that case.
     *
     * Result is that the following is used for execution:
     *
     * Windows 95/98/ME:
     *
     * start /w /m C:\COMMAND.COM /c <command to execute>
     *
     * Windows XP (and Windows 2000/2003 Server):
     *
     * CMD.EXE /c <command to execute>
     */

    /* Set windows laucher, assume its somewhere on the path */
    start = "start";

    /* Get the command-processor path */
    cmd = getenv("COMSPEC");

    if(cmd == NULL)
    {
        /* COMSPEC not set, assume COMMAND.COM */
        cmd = "COMMAND.COM";
    }

    /* Duplicate for handling with "basename" */
    cmd_copy = strdup(cmd);
    if(cmd_copy == NULL)
    {
        /* out of memory, execution fails */
        return -1;
    }

    /* Duplicate for handling with "strtok" */
    command_copy = strdup(command);
    if(command_copy == NULL)
    {
        /* out of memory, execution fails */
        free(cmd_copy);

        return -1;
    }

    /*
     * Assume the command is directly invoked via CMD unless determined
     * otherwise below.
     */
    method = M_DIRECT;

    if(uname(&buf) == 0)
    {
        /* Uname succeded */
        if(strlen(buf.sysname) > 8)
        {
            /* Enough length to check */
            /*
             * Crude check for:
             *
             * CYGWIN_95
             * CYGWIN_98
             * CYGWIN_ME
             *        ^
             *        |
             *    character
             *     checked
             */
            if((buf.sysname[7] == '9') || (buf.sysname[7] == 'M'))
            {
                /* On Windows 95, 98 and ME use 'start' */
                method = M_HASSTART;
            }
        }
    }

    /* Split command up in arguments */

    argc = 0;

    if(method == M_HASSTART)
    {
        /*
         * First 4 arguments are fixed:
         * 1) program name 'start'
         * 2) a flag for start specifying it to wait for completion
         * 3) a flag for start specifying it start minimized
         * 4) a to the full path to the command processor
         * 5) the /c flag that causes execution of the command that follows.
         *
         * Layout:
         *
         * start /w /m C:\COMMAND.COM /c ARG1 ARG2
         * ----- -- -- -------------- -- ---- ----
         *  [0]  [1][2]      [3]      [4] [5]  [6]
         */
        argv[argc++] = start;
        argv[argc++] = "/w";
        argv[argc++] = "/m";
        argv[argc++] = cmd;
        argv[argc++] = "/c";
    }
    else /* M_DIRECT */
    {
        /*
         * First 2 arguments are fixed:
         * 1) basename of the command processor
         * 2) the /c flag that causes execution of the command that follows.
         *
         * Layout:
         *
         * CMD.EXE /c ARG1 ARG2
         * ------- -- ---- ----
         *   [0]  [1]  [2]  [3]
         */
        argv[argc++] = basename(cmd_copy);
        argv[argc++] = "/c";
    }

    /*
     * Tokenize the arguments, parts between double quotes
     * are treated as one argument. Otherwise arguments are split
     * on tab, space, cariage return or newline.
     *
     * Note that this is less advanced than a shell, CMD does not
     * treat singele quotes, nor do we, and also \ escape does not
     * work because those are also used in file-paths.
     *
     * Including a " by specifying "" doesn't work yet.
     */
    p = strtok(command_copy, "\0");
    while(p != NULL)
    {
        switch(*p) {
        case '"':   p = strtok(&p[1], "\"");
                    argv[argc++] = p;
                    p = strtok(NULL, "\0");
                    break;
        default:
                    p = strtok(p, " \t\r\n");
                    argv[argc++] = p;
                    p = strtok(NULL, "\0");
                    break;
        }

        /* If our pointer array argv is full, stop */
        if(argc == MAXARGS)
        {
            break;
        }
    }
    argv[argc] = NULL;

    if(fork() == 0)
    {
        /* Child side */

        /*
         * execute, will overlay our child process so
         * this code will end here...
         */
        if(method == M_HASSTART)
        {
            execvp(start, (char * const *) &(argv[0]));
        }
        else /* M_DIRECT */
        {
            execvp(cmd, (char * const *) &(argv[0]));
        }

        /* we should never get here... */
        exit(0);
    }
    else
    {
        /* Parent side */
        wait(&exitcode);
    }

    /*
     * Just fork and exit to reset the title on Windows 95/98/ME
     *
     * Eh, yes, it is indeed a very ugly hack, but it works :-)
     */
    if(fork() == 0)
    {
        /* We terminate immediately... */
        exit(0);
    }
    else
    {
        /* Parent side */
        wait(&exitcode2);
    }

    free(cmd_copy);
    free(command_copy);

    return exitcode;
}
#endif /* __CYGWIN32__ */
