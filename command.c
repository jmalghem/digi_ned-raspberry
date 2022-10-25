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
 * 2022-10 (F4GVY) : remove reference to function not usable in ARM version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _LINUX_
#include "linux.h"
#else
#include "digi_dos.h"
#endif /* _LINUX_ */
#include "digicons.h"
#include "digitype.h"
#include "read_ini.h"
#include "genlib.h"
#include "output.h"
#include "message.h"
#include "command.h"
#include "mheard.h"

/*
 * Static storage area for reply-strings from the commands.
 */
static char       reply_data[MAX_MESSAGE_LENGTH];
static ini_cmd_t *digi_ini_cmd_p = NULL; /* commands from .ini file       */

/*
 * Add a command specified in the .ini file in the command list,
 * to be executed after the .ini file has completely be processed
 *
 * command: !out 1 01101011
 *         ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'command' token is already handled. The tokenizer is now at the point
 * in the string indicated by '^' above (the ':' was changed to '\0' by
 * 'strtok').
 */
void add_ini_cmd()
{
    ini_cmd_t *ini_cmd_p;
    ini_cmd_t *ic_p;
    char      *command_p;

    command_p = strtok(NULL,"\r\n");

    if(command_p == NULL)
    {
        say("Command missing on command: line at line %d\r\n", linecount);
        return;
    }

    ini_cmd_p = (ini_cmd_t*) malloc(sizeof(ini_cmd_t));
    if(ini_cmd_p == NULL)
    {
        /*
         * if out of memory then quit
         */
        say("Out of memory while handling command: line at line %d\r\n",
                linecount);
        return;
    }

    ini_cmd_p->command_p = strdup(command_p);

    if(ini_cmd_p->command_p == NULL)
    {
        /*
         * if out of memory then quit
         */
        say("Out of memory while handling command: line at line %d\r\n",
                linecount);
        /* free earlier allocated memory */
        free(ini_cmd_p);
        return;
    }

    /* initialize the next pointer */
    ini_cmd_p->next = NULL;

    /*
     * Store the command, preserve the order!
     */
    ic_p = digi_ini_cmd_p;

    if(ic_p == NULL)
    {
        /* first */
        digi_ini_cmd_p = ini_cmd_p;
    }
    else
    {
        while(ic_p->next != NULL)
        {
            ic_p = ic_p->next;
        }

        /* let next point to the new command */
        ic_p->next = ini_cmd_p;
    }

    /* Finished */
    return;
}

/*
 * Execute a command previously read from the .ini file. After execution
 * the command is discarded. This proceeds until all commands have been
 * handled.
 */
void execute_ini_cmd()
{
    ini_cmd_t *ini_cmd_p;

    ini_cmd_p = digi_ini_cmd_p;

    if(ini_cmd_p == NULL)
    {
        /* No initital commands, finished! */
        return;
    }

    /*
     * set digi_ini_cmd_p to the next command for next time...
     */
    digi_ini_cmd_p = ini_cmd_p->next;

    /*
     * Execute the command, cleanup memory when this one is done
     */

    /* execute the command... */ 
    internal_message(ini_cmd_p->command_p);
    display_separator();

    /* cleanup */
    free(ini_cmd_p->command_p);
    free(ini_cmd_p);

    /* Finished */
    return;
}

#ifdef _OUTPORT_
/*
 * Execute an "!out" command
 *
 * On input it is assumed the command is being tokenized and only tokenized the
 * command. The tokenizer ("strtok") can be used to continue interpretation of
 * the command.
 *
 * Return: pointer to reply-string if command executed okay
 *         NULL if the command failed
 */
static char *do_command_out(void)
{
    char   *port;
    char   *lpt;
    char   *arg;
    short   res;
    short   i;

    port  = strtok(NULL," \t\r\n");
    if(port == NULL)
    {
        asay("Command rejected: missing lpt number on !out command\r\n");
        return NULL;
    }

    if(strlen(port) != 1)
    {
        asay("Command rejected: illegal lpt number (should be 1-3)\r\n");
        return NULL;
    }

    switch(port[0])
    {
    case '1':
                lpt = "lpt1";
                break;
    case '2':
                lpt = "lpt2";
                break;
    case '3':
                lpt = "lpt3";
                break;
    default:
                asay("Command rejected: illegal lpt number on !out  command"
                     "(should be 1-3)\r\n");
                return NULL;
    }

    arg  = strtok(NULL," \t\r\n");

    if(arg == NULL)
    {
        asay("Command rejected: value missing on !out %s command\r\n", port);
        return NULL;
    }

    if(strspn(arg,"01/xp") != strlen(arg))
    {
        asay("Command rejected: value not binary (0,1,/,x or p) "
             "on !out %s command\r\n", port);
        return NULL;
    }

    asay("Command accepted: !out %s %s\r\n", port, arg);

    /* res = out_lpt_data(lpt, arg); */

    if(res == -1)
    {
        asay("Command execution failed: !out %s %s\r\n", port, arg);
        return NULL;
    }

    for(i = 0; i < 8; i++)
    {
        if((res & 0x01) == 0)
        {
            reply_data[i] = '0';
        }
        else
        {
            reply_data[i] = '1';
        }
        res = res >> 1;
    }
    reply_data[8] = '\0';

    asay("Command execution success: !out %s %s, data now: %s\r\n",
                    port, arg, reply_data);

    return reply_data;
}
#endif /* _OUTPORT_ */

/*
 * Execute an "!clear" command
 *
 * On input it is assumed the command is being tokenized and only tokenized the
 * command. The tokenizer ("strtok") can be used to continue interpretation of
 * the command.
 *
 * Return: pointer to reply-string if command executed okay
 *         NULL if the command failed
 */
static char *do_command_clear(void)
{
    char *param;

    param = strtok(NULL," \t\r\n");

    if(param == NULL)
    {
        /* No parameter, clear all */
        remove_mheard();

        strcpy(reply_data, "MH cleard"); 
    }
    else
    {
        if(strlen(param) == 1)
        {
            if((param[0] >= '0') && (param[0] < '9'))
            {
                if(param[0] == '0')
                {
                    /* All ports, clear all */
                    remove_mheard();

                    strcpy(reply_data, "MH cleard"); 
                }
                else
                {
                    /* Clear data from the selected port */
                    remove_mheard_port(atoi(param) - 1);

                    strcpy(reply_data, "Port "); 
                    strcat(reply_data, param); 
                    strcat(reply_data, " Clrd"); 
                }
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            if(strlen(param) < 10)
            {
                /* Clear selected call from mheard */
                remove_mheard_call(param);
                strcpy(reply_data, param); 
                strcat(reply_data, " Clrd"); 
            }
            else
            {
                return NULL;
            }
        }
    }

    return reply_data;
}

#ifdef _PTT_
/*
 * Execute an "!ptt" command
 *
 * On input it is assumed the command is being tokenized and only tokenized the
 * command. The tokenizer ("strtok") can be used to continue interpretation of
 * the command.
 *
 * Return: pointer to reply-string if command executed okay
 *         NULL if the command failed
 */
static char *do_command_ptt(void)
{
    char   *arg;
    char    data[9];
    short   i;
    short   num;

    arg  = strtok(NULL," \t\r\n");

    if(arg == NULL)
    {
        asay("Command rejected: value missing on !ptt command\r\n");
        return NULL;
    }

    if(strspn(arg,"01/x") != strlen(arg))
    {
        asay("Command rejected: value not binary (0,1,/ or x) "
             "on !ptt command\r\n");
        return NULL;
    }

    asay("Command accepted: !ptt %s\r\n", arg);

    /*
     * Act on a copy so we can shift and manipulate it
     */
    num = digi_ptt_data;

    /*
     * Convert read data to data string
     */
    for(i = 0; i < 8; i++)
    {
        if((num & 0x01) == 0)
        {
            data[i] = '0';
        }
        else
        {
            data[i] = '1';
        }
        num = num >> 1;
    }
    data[8] = '\0';

    /*
     * Take over the not don't care values from arg into data
     */
    for(i = 0; i < 8; i++)
    {
        if(arg[i] == '\0')
        {
            /* no more changes, we can stop this loop */
            break;
        }

        if((arg[i] == '0') || (arg[i] == '1'))
        {
            /* this bit should be changed */
            data[i] = arg[i];
        }
    }

    /* Convert data to number */
    num = 0;
    for(i = 7; i >= 0; i--)
    {
        num = num << 1;

        if(data[i] == '1')
        {
            num = num + 1;
        }
    }

    /*
     * Activate the new value
     */
    digi_ptt_data = num;

    for(i = 0; i < 8; i++)
    {
        if((num & 0x01) == 0)
        {
            reply_data[i] = '0';
        }
        else
        {
            reply_data[i] = '1';
        }
        num = num >> 1;
    }
    reply_data[8] = '\0';

    asay("Command execution success: !ptt %s, ptt setting now: %s\r\n",
                    arg, reply_data);

    return reply_data;
}
#endif /* _PTT_ */

/*
 * Execute a remote command
 *
 * Return: pointer to reply-string if command is okay
 *         NULL if unknown command
 */
char *do_command(char *command)
{
    char    command_copy[256];
    char   *cmd;

    /*
     * Make a copy before letting strtok distroy the original string
     */
    strncpy(command_copy, command, 255);
    /*
     * Ensure a '\0' at the end
     */
    command_copy[255] = '\0';

    cmd = strtok(command_copy," \t\r\n");
    if(cmd == NULL)
    {
        asay("Command rejected: no command given\r\n");
        return NULL;
    }

#ifdef _OUTPORT_
    if((digi_enable_out_command != 0) && (strcmp(cmd,"!out") == 0))
    {
        return do_command_out();
    }
    else 
#endif /* _OUTPORT_ */
    if(strcmp(cmd,"!clear") == 0)
    {
        return do_command_clear();
    }
#ifdef _PTT_
    if((digi_enable_ptt_command != 0) && (strcmp(cmd,"!ptt") == 0))
    {
        return do_command_ptt();
    }
#endif /* _PTT_ */

    asay("Command rejected: unknown command %s\r\n", cmd);

    return NULL;
}
