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
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _LINUX_
#include "linux.h"
#endif /* _LINUX_ */
#ifdef __CYGWIN32__
#include "cygwin.h"
#endif /* __CYGWIN32__ */
#include "version.h"
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "mac_if.h"
#include "read_ini.h"
#include "message.h"
#include "keep.h"
#include "call.h"
#include "timer.h"
#include "output.h"
#include "mheard.h"
#include "send.h"
#include "dx.h"
#include "command.h"
#include "telemetr.h"
#include "serial.h"
#include "predict.h"
#include "sat.h"

/*
 * Linked lists containing the 'message_path' information per port
 */
static path_t *port_message_path_p[MAX_PORTS] = { 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static char        message_id[MAX_NR_OF_IDS];
static message_t  *msg_list_p;
static time_t      boot_time;
static exit_info_t exit_info;
static query_t    *digi_query_cur_p;
static query_t    *digi_query = NULL;
static struct stat digi_query_stat;
static short       internal_command;
static short       auto_command;

/*
 * Add message_path command from the .ini file to the internal administration.
 *
 * message_path: 1,2,3,4,5,6,7,8 WIDE,TRACE6-6
 *              ^
 *
 * On entry it is assumed that 'strtok' is tokenizing the line and the
 * 'message_path' token is already handled. The tokenizer is now at the
 * point in the string indicated by '^' above (the ':' was changed to
 * '\0' by 'strtok').
 */
void add_message_path()
{
    path_t  message_path;
    path_t *message_path_p;
    char   *tmp;
    char   *tmp_case;
    char   *path;
    char    output[OUTPORTS_LENGTH];
    short   port;

    tmp = strtok(NULL, " \t\r\n");
    if(tmp == NULL)
    {
        say("Missing to-port on message_path: at line %d\r\n", linecount);
        return;
    }

    if(check_ports(tmp, "To-port", "message_path:", 
                   linecount, EXCL_ALLBUT) == 0)
    {
        return;
    }

    strcpy(output, tmp);

    message_path.path = NULL;

    path = strtok(NULL, "\r\n");

    if(path != NULL)
    {
        tmp_case = path;
        while(*tmp_case != '\0')
        {
            *tmp_case = toupper(*tmp_case);
            tmp_case++;
        }

        message_path.path = strdup(path);

        if(message_path.path == NULL)
        {
            say("Out of memory on message_path: at line %d\r\n", linecount);
            return;
        }

        /*
         * Check for illegal digipeaters (too long)
         */
        tmp = strtok(path, " ,\t\r\n");
        while(tmp != NULL)
        {
            if(strlen(tmp) > 9)
            {
                say("Argument (%s) to long on message_path: at line %d\r\n",
                        tmp, linecount);

                free(message_path.path);
                return;
            }
            tmp = strtok(NULL, " ,\t\r\n");
        }
    }

    /*
     * Store the message_path in the port_message_path list for each port
     */
    port = find_first_port(output);
    if(port == -1)
    {
        say("To-port out of range (1-%d) on message_path: at line %d\r\n",
                mac_ports(), linecount);
        return;
    }
    while(port != 0)
    {
        /*
         * Allocate dynamic storage for this entry
         */
        message_path_p = (path_t*) malloc(sizeof(path_t));

        /*
         * if out of memory then ignore
         */
        if(message_path_p == NULL)
        {
            say("Out of memory while handling message_path: "
                    "line at line %d\r\n", linecount);
            return;
        }

        /*
         * Duplicate data
         */
        memcpy(message_path_p, &message_path, sizeof(path_t));

        /*
         * Make port numbers base start from 0 instead of 1
         */
        port--;

        /*
         * Add to the message_path list for this port
         */
        message_path_p->next = port_message_path_p[port];
        port_message_path_p[port] = message_path_p;

        port = find_next_port();
        if(port == -1)
        {
            say("To-port out of range (1-%d) on message_path: at line %d\r\n",
                    mac_ports(), linecount);
            return;
        }
    }
}

/*
 * Read in the message file
 */
static void read_queries(void)
{
    FILE      *fd;
    struct tm *t;
    char       query[MAX_MESSAGE_LENGTH + 1];
    char       answer[MAX_DATA_LENGTH + 1];
    char       buf[MAX_DATA_LENGTH + 1];
    char       tmp[40];
    char      *p;
    char      *q;
    char      *tmp_case;
    short      ports;
    query_t   *query_p;
    query_t   *query_next_p;

    digi_query = NULL;

    if(digi_message_file[0] == '\0')
    {
        /* query response file not used */
        return;
    }

    if(stat(digi_message_file, &digi_query_stat) != 0)
    {
        say("Can't find message query/response file \"%s\"\r\n",
                digi_message_file);
        return;
    }

    fd = fopen(digi_message_file, "r");

    if(fd == NULL)
    {
        say("Can't read message query/response file \"%s\"\r\n",
                digi_message_file);
        return;
    }

    while(fgets(buf, MAX_DATA_LENGTH, fd) != NULL)
    {
        /* truncate at maximum allowable length */
        buf[MAX_MESSAGE_LENGTH] = '\0';

        /* truncate at the first CR or LF character, if any */
        p = strchr(buf, '\r');
        if(p != NULL) *p = '\0';
        p = strchr(buf, '\n');
        if(p != NULL) *p = '\0';

        /* ignore comment-lines */
        if(buf[0] ==  '#')
            continue;

        /* ignore empty-lines */
        if(strlen(buf) == 0)
            continue;

        if(buf[0] ==  '?')
        {
            strcpy(query, buf);

            tmp_case = query;
            while(*tmp_case != '\0')
            {
                *tmp_case = tolower(*tmp_case);
                tmp_case++;
            }

            answer[0] = '\0';
        }
        else
        {
            p = buf;

            q = answer;

            while(*p != '\0')
            {
                if(*p == '%')
                {
                    p++;
                    switch(*p)
                    {
                    case 'd':
                        strcat(answer, digi_digi_call);
                        p++;
                        break;
                    case 'o':
                        strcat(answer, digi_owner());
                        p++;
                        break;
                    case 'v':
                        sprintf(tmp, "%s: %s - %s", VERSION,__DATE__,__TIME__);
                        strcat(answer, tmp);
                        p++;
                        break;
                    case 'b':
                        tzset();
                        t = localtime(&boot_time);
                        strftime(tmp, 39, "%b %d %Y - %H:%M:%S", t);
                        strcat(answer, tmp);
                        p++;
                        break;
                    case 'p':
                        ports = mac_ports();
                        sprintf(tmp, "%d", ports);
                        strcat(answer, tmp);
                        p++;
                        break;
                    default:
                        *q++ = '%';
                        *q = '\0';
                        break;
                    }
                    q = &(answer[strlen(answer)]);
                }
                else
                {
                    *q++ = *p++;
                    *q = '\0';
                }
                if(strlen(answer) >= MAX_MESSAGE_LENGTH)
                    break;
            }

            answer[MAX_MESSAGE_LENGTH] = '\0';

            query_p = malloc(sizeof(query_t));
            if(query_p != NULL)
            {
                strcpy(query_p->query, query);
                strcpy(query_p->answer, answer);
                
                query_p->next = digi_query;
                digi_query = query_p;
            }
            else
            {
                say("Out of memory while reading message query/answers\r\n");
            }
        }
    }
    
    fclose(fd);

    /* Queries are now stored in reverse order, swap the order */
    query_p = digi_query;
    digi_query = NULL;
    while(query_p != NULL)
    {
        query_next_p = query_p->next;
        
        query_p->next = digi_query;
        digi_query = query_p;

        query_p = query_next_p;
    }

    return;
}

/*
 * Erase the queries
 */
static void erase_queries(void)
{
    query_t  *query_p;
    query_t  *query_next_p;

    /* set query_p to the start of the list */
    query_p = digi_query;

    /* This will erase the queries */
    digi_query = NULL;

    /* Free all allocated memory */
    while(query_p != NULL)
    {
        query_next_p = query_p->next;
        
        free(query_p);

        query_p = query_next_p;
    }

    return;
}

/*
 * Check if the queries are changed and need to be re-read
 */
static void check_queries(void)
{
    struct stat s;

    if(stat(digi_message_file, &s) != 0)
    {
        /* Can't find, can't update, go with what we have */
        return;
    }

    if(digi_query_stat.st_mtime == s.st_mtime)
    {
        /* same modification time, not changed, go with what we have */
        return;
    }

    /* File changed */

    /* Remove the old message query/responses */
    erase_queries();

    /* Read the new message query/responses from the digi_message_file */
    read_queries();

    return;
}

/*
 * Transmit message
 */
static void transmit_message(message_t *message_p)
{
    uidata_t  uidata;
    path_t   *message_path_p;
    short     ndigi;
    char     *tmp;
    char     *path;
    char      output[MAX_ITOA_LENGTH];

    /* prepare transmission */
    uidata.port = message_p->port;

    /* transmit remotely */
    uidata.distance = REMOTE;

    /* primitive UI with no poll bit */
    uidata.primitive = 0x03;

    /* PID = 0xF0, normal AX25*/
    uidata.pid = 0xF0;

    /* fill in originator */
    strcpy(uidata.originator, digi_digi_call);
    uidata.orig_flags = (char) (RR_FLAG | C_FLAG);

    /* fill in destination */
    strcpy(uidata.destination, digi_digi_dest);

    uidata.dest_flags = RR_FLAG;

    message_path_p = port_message_path_p[uidata.port];

    /* handle all the paths for this port */
    while(message_path_p != NULL)
    {
        ndigi=0;
        if(message_path_p->path != NULL)
        {
            /* make copy of digipeater path for strtok */
            path = strdup(message_path_p->path);
            if(path == NULL)
            {
                /* no memory for transmission */
                return;
            }

            /* add digipeater path */
            tmp = strtok(path, " ,\t\r\n");
            while(tmp != NULL)
            {
                if(ndigi < MAX_DIGI)
                {
                    strcpy(uidata.digipeater[ndigi], tmp);
                    uidata.digi_flags[ndigi] = RR_FLAG;
                    ndigi++;
                }
                tmp = strtok(NULL, " ,\t\r\n");
            }

            /* don't need "path" anymore, free memory */
            free(path);
        }
        uidata.ndigi = ndigi;

        /* a shutdown message should not have any paths */
        if(exit_info.state == CLOSING_DOWN)
        {
            if(message_p->id == exit_info.id)
            {
                /* 
                 * this is the shutdown message, should have no digis
                 * eliminate the digis by setting the count to 0
                 */
                uidata.ndigi = 0;
            }
        }

        /* set the data to transmit */
        strcpy(uidata.data, message_p->message);
        uidata.size = strlen(uidata.data);

        /* avoid digipeating my own data */
        keep_old_data(&uidata);

        /* convert port to string for data dump */
        (void) digi_itoa(message_p->port + 1, output);

        vsay("Message transmission:\r\n");

        /* check size of what we are about to transmit, kenwood mode */
        if(uidata_kenwood_mode(&uidata) == 0)
        {
            return; /* failure */
        }

        /* show what we will transmit */
        dump_uidata_to(output, &uidata);

        /* transmit */
        put_uidata(&uidata);

        /* next path */
        message_path_p = message_path_p->next;
    }
}

/*
 * Retrieve message ID, as close following the last returned number
 *
 * returns: 0-99 message id to use with the next message (allocated)
 *          -1 if there are no free message id's anymore (all waiting for ack)
 */
static short get_message_id(void)
{
    static short last = -1;
    short        i;

    if(last == -1)
    {
        /* first time to retrieve an ID, start somewhere between 0 and 99 */
        last = (short) (((unsigned short) time(NULL)) % MAX_NR_OF_IDS);
    }

    /* try to find an unallocated message id */
    for(i = 0; i < MAX_NR_OF_IDS; i++)
    {
        last = (last + 1) % MAX_NR_OF_IDS;
        if(message_id[last] == 0)
        {
            /* allocate */
            message_id[last] = 1;
            return last;
        }
    }
    return -1;
}

/*
 * Clear a message from the message-queue
 */
static void remove_message(short id)
{
    message_t *message_p;

    message_p = msg_list_p;
    while(message_p != NULL)
    {
        if(message_p->id == id)
        {
            /* mark for destruction */
            message_p->count = 0;

            /* expire the timer */
            clear_timer(&(message_p->timer));

            return;
        }
        message_p = message_p->next;
    }
    return;
}

/*
 * send an ack on a received message
 */
static void send_ack(char *id, uidata_t *uidata_p)
{
    message_t *message_p;

    /* create a new message */
    message_p = malloc(sizeof(message_t));

    if(message_p == NULL)
    {
        /* no memory, more luck next time! */
        return;
    }

    /* make message header */
    sprintf(message_p->message, ":         :ack%s", id);

    /* patch in destination addres */
    strncpy(&(message_p->message[1]), uidata_p->originator,
                                strlen(uidata_p->originator));

    /* send back wher the message came from */
    message_p->port = uidata_p->port;

    /* no message id */
    message_p->id = -1;

    /* interval of 4 seconds to start with */
    message_p->interval = 4;

    /* send the ack only 1 time for each reception */
    message_p->count = 1;

    /* start interval-time seconds after reception */
    start_timer(&(message_p->timer), message_p->interval * TM_SEC);

    /* add to msg_list_p, new message in front */
    message_p->next = msg_list_p;
    msg_list_p = message_p;
}

/*
 * send a message specified in a message_p structure addressed by
 * message_p.
 *
 * transmission is not done here, but the message is added to a
 * list of messages which are scheduled for transmission (until acked or
 * the count value reaches 0)
 */
static void send_message_data(ack_t need_ack, short number,
                              message_t *message_p)
{
    char       output[MAX_ITOA_LENGTH];
    char       message_prefix[MSG_PREFIX_LENGTH + 1];

    /* auto command is never not transmitted if addressed to myself */
    if(auto_command != 0)
    {
        /* prepare a message prefix with the digi-call for comparison */
        /* prepare 2 colons with space for a full call ":PE1DNN-12:"  */
        strcpy(message_prefix, ":         :");

        /* patch in the digi-call */
        strncpy(&(message_prefix[1]), digi_digi_call, strlen(digi_digi_call));

        if(strncmp(message_p->message, message_prefix, MSG_PREFIX_LENGTH) == 0)
        {
            /*
             * Ignoring message responses sent to myself, no need to go on
             * air!
             */
            if(auto_command == 1)
            {
                /*
                 * Show verbose for the first set of responses which are
                 * addressed to myself so they appear on the screen.
                 */
                vsay("automessage Reply: %s\r\n", &(message_p->message[11]));
            }

            return;
        }
    }

    if(need_ack == NOACK)
    {
        /* no message id */
        message_p->id = -1;
    }
    else /* ACK or SHUTDOWN */
    {
        /* get message id */
        message_p->id = get_message_id();

        if((need_ack == SHUTDOWN) && (message_p->id != -1))
        {
            /* cleanup old shutdown message if present */
            if(exit_info.state == CLOSING_DOWN)
            {
                /*
                 * We were already closing down, remove old pending
                 * shutdown message
                 */
                remove_message(exit_info.id);
            }

            /* remember the new id */
            exit_info.id = message_p->id;

            /* mark that we are closing down now */
            exit_info.state = CLOSING_DOWN;
        }
    }

    /* add indicator to message when id is used */
    if(message_p->id != -1)
    {
        /* add indicator to the message */
        strcat(message_p->message, "{");

        /* convert port to string for data dump */
        (void) digi_itoa(message_p->id, output);

        /* add id to the message */
        strcat(message_p->message, output);

        if(need_ack == SHUTDOWN)
        {
            /* repeat shutdown message 3 times */
            message_p->count = 3;
        }
        else
        {
            /* repeat normal message 10 times */
            message_p->count = 10;
        }
    }
    else
    {
        /* message without ack, repeat the message 1 time */
        message_p->count = 1;
    }

    /* add '\r' to the message */
    strcat(message_p->message, "\r");

    /*
     * Interval of 5 secondes before retry of the message (will be
     * doubled on each retry)
     */
    message_p->interval = 5;

    /*
     * Initial deposit is delayed if there are more messages, separation
     * of 10 seconds between subsequent messages is used.
     */
    /* start (10 * number) seconds from now */
    start_timer(&(message_p->timer), (10 * number) * TM_SEC);

    /* add to msg_list_p, new message to the front */
    message_p->next = msg_list_p;
    msg_list_p = message_p; 
}

/*
 * send a message to originator indicated in the uidata_p frame
 *
 * transmission is not done here, but the message is added to a
 * list of messages which are scheduled for transmission (until acked or
 * the count value reaches 0)
 */
static void send_message(ack_t need_ack, short number, char *message,
                                                    uidata_t *uidata_p)
{
    message_t *message_p;

    if(internal_command == 1)
    {
        say("Reply %d: %s\r\n", number, message);
        return;
    }

    /* create a new message */
    message_p = malloc(sizeof(message_t));

    if(message_p == NULL)
    {
        /* no memory, more luck next time! */
        return;
    }

    /* make message header */
    strcpy(message_p->message, ":         :");

    /* patch in destination addres */
    strncpy(&(message_p->message[1]), uidata_p->originator,
                                strlen(uidata_p->originator));
    
    /* 
     * Copy message in. Maximum length of a message is = MAX_MESSAGE_LENGTH
     * bytes.
     */
    strncpy(&(message_p->message[MSG_PREFIX_LENGTH]), 
            message, MAX_MESSAGE_LENGTH);

    /* make shure there is a null terminator at the end */
    message_p->message[MSG_PREFIX_LENGTH + MAX_MESSAGE_LENGTH] = '\0';

    /* send back where the message came from */
    message_p->port = uidata_p->port;

    /* send the message which is formatted in the message structure */
    send_message_data(need_ack, number, message_p);
}

/*
 * send a message, where the complete message, including addres, is
 * specified in a string. 
 *
 * transmission is not done here, but the message is added to a
 * list of messages which are scheduled for transmission (until acked or
 * the count value reaches 0)
 */
static void send_message_string(short number, char *message,
                                                    uidata_t *uidata_p)
{
    message_t *message_p;
    short      i;

    /*
     * The message sould be at least a full addres on one payload
     * character
     */
    if(strlen(message) < (MSG_PREFIX_LENGTH + 1))
    {
        say("Message string to short! %s\r\n", message);
        return;
    }

    /*
     * Make the address (between the ':' characters of a message)
     * uppercase. We already checked the length.
     */
    for(i = 0; i < MSG_PREFIX_LENGTH; i++)
    {
        message[i] = toupper(message[i]);
    }

    if(internal_command == 1)
    {
        say("Send %d: %s\r\n", number, message);
        return;
    }

    /* create a new message */
    message_p = malloc(sizeof(message_t));

    if(message_p == NULL)
    {
        /* no memory, more luck next time! */
        return;
    }

    /* 
     * Copy message in. Maximum length of a message is = MSG_PREFIX_LENGTH +
     * MAX_MESSAGE_LENGTH bytes.
     */
    message_p->message[0] = '\0';
    strncpy(message_p->message, message, MAX_MESSAGE_LENGTH +
            MSG_PREFIX_LENGTH);

    /* make shure there is a null terminator at the end */
    message_p->message[MSG_PREFIX_LENGTH + MAX_MESSAGE_LENGTH] = '\0';

    /* send back where the message came from */
    message_p->port = uidata_p->port;

    /* send the message which is formatted in the message structure */
    if(strncmp(message_p->message, ":BLN", 4) == 0)
    {
        /* Bulletins do not use an ACK */
        send_message_data(NOACK, number, message_p);
    }
    else
    {
        /* Normal messages use an ACK */
        send_message_data(ACK, number, message_p);
    }
}

/*
 * send an item beacon on request of a station
 *
 * Transmission is not done here, but the item is added to a
 * list of messages which are scheduled for transmission.
 * An item is transmitted two times.
 */
static void send_item(short number, char *message, uidata_t *uidata_p)
{
    message_t *message_p;

    if(internal_command == 1)
    {
        say("Object %d: %s\r\n", number, message);
        return;
    }

    /* create a new message */
    message_p = malloc(sizeof(message_t));

    if(message_p == NULL)
    {
        /* no memory, more luck next time! */
        return;
    }

    /* 
     * Copy message in. Maximum length of a message is = MAX_MESSAGE_LENGTH
     * bytes.
     */
    message_p->message[0] = '\0';
    strncpy(message_p->message, message, MAX_MESSAGE_LENGTH);

    /* make shure there is a null terminator at the end */
    message_p->message[MAX_MESSAGE_LENGTH] = '\0';

    /* send back where the message came from */
    message_p->port = uidata_p->port;

    /* no message id */
    message_p->id = -1;

    /* 
     * message without ack, repeat the message twice to get a more
     * reliable reception.
     */
    message_p->count = 2;

    /* add '\r' to the message */
    strcat(message_p->message, "\r");

    /*
     * Start in 10 secondes, the decay algorithm for non-messages will
     * repeat the object the second time 60 minutes after the first
     * deposit.
     */
    message_p->interval = 10 * number;

    /* start interval-time seconds from now */
    start_timer(&(message_p->timer), message_p->interval * TM_SEC);

    /* add to msg_list_p, new message to the front */
    message_p->next = msg_list_p;
    msg_list_p = message_p; 
}

/*
 * Replace runtime-variables in the answer
 */
static char *query_runtime_replace(char *question, char *answer,
                                   uidata_t *uidata_p)
{
    static char  final_answer[MAX_DATA_LENGTH + 1];
    char        *p, *q, *r, *s;

    p = answer;
    q = final_answer;

    while(*p != '\0')
    {
        if(*p == '%')
        {
            p++;
            switch(*p)
            {
            case 'q':
                if((strlen(final_answer) + strlen(question))
                    < MAX_DATA_LENGTH)
                {
                    /*
                     * Prevent appearence of single quotes so single quotes
                     * can be used to enclose the string to prevent problems
                     * with redirection characters etc.
                     */
                    r = question;
                    while(*r != '\0')
                    {
                        if(*r == '\'')
                        {
                            *r = '.';
                        }
                        r++;
                    }

                    strcat(final_answer, question);
                    p++;
                }
                else
                {
                    *q++ = '%';
                    *q = '\0';
                }
                break;
            case 'x':
                s = tnc_string(uidata_p, REMOTE);
                if((strlen(final_answer) + strlen(s))
                    < MAX_DATA_LENGTH)
                {
                    /*
                     * Prevent appearence of single quotes so single quotes
                     * can be used to enclose the string to prevent problems
                     * with redirection characters etc.
                     */
                    r = s;
                    while(*r != '\0')
                    {
                        if(*r == '\'')
                        {
                            *r = '.';
                        }
                        r++;
                    }

                    strcat(final_answer, s);
                    p++;
                }
                else
                {
                    *q++ = '%';
                    *q = '\0';
                }
                break;
            default:
                *q++ = '%';
                *q = '\0';
                break;
            }
            q = &(final_answer[strlen(final_answer)]);
        }
        else
        {
            *q++ = *p++;
            *q = '\0';
        }
        if(strlen(final_answer) >= MAX_MESSAGE_LENGTH)
            break;
    }

    final_answer[MAX_MESSAGE_LENGTH] = '\0';

    /* return the answer with the replaced variables */
    return final_answer;
}

/*
 * Find the first response to a question. Return NULL when a
 * response can't be found.
 * If the def_answer flag is 'true' search for the default answers.
 */
static char *query_first(char *question, uidata_t *uidata_p, short def_answer)
{
    query_t *query_p;
    char     query[MAX_MESSAGE_LENGTH + 1];
    char    *p;

    /* check if we need to re-read the message file */
    check_queries();

    query_p = digi_query;

    while(query_p != NULL)
    {
        if(def_answer == 0)
        {
            /*
             * Try to find a match for this query
             */
            strcpy(query, query_p->query);

            p = strtok(query, "|");

            while(p != NULL)
            {
                if(*p == '?')
                {
                    p++;
                }

                if(match_string(question, p) == 1)
                {
                    digi_query_cur_p = query_p->next;
                    /* found! */
                    return query_runtime_replace(question, query_p->answer,
                                                 uidata_p);
                }
                
                p = strtok(NULL, "|");
            }
        }
        else
        {
            /*
             * Look for default answer
             */
            if(strcmp(query_p->query, "?") == 0)
            {
                digi_query_cur_p = query_p->next;
                /* found default answer! */
                return query_runtime_replace(question, query_p->answer,
                                                 uidata_p);
            }
        }
        query_p = query_p->next;
    }

    digi_query_cur_p = NULL;

    return NULL;
}

/*
 * Find the next response to a questions, return NULL when no more
 * specific responses can not be found.
 * If the def_answer flag is 'true' search for the default answers.
 */
static char *query_next(char *question, uidata_t *uidata_p, short def_answer)
{
    query_t *query_p;
    char     query[MAX_MESSAGE_LENGTH + 1];
    char    *p;

    query_p = digi_query_cur_p;

    while(query_p != NULL)
    {
        if(def_answer == 0)
        {
            /*
             * Try to fine a match for this query
             */
            strcpy(query, query_p->query);

            p = strtok(query, "|");

            while(p != NULL)
            {
                if(*p == '?')
                {
                    p++;
                }

                if(match_string(question, p) == 1)
                {
                    digi_query_cur_p = query_p->next;

                    /* found! */
                    return query_runtime_replace(question, query_p->answer,
                                                 uidata_p);
                }
                
                p = strtok(NULL, "|");
            }
        }
        else
        {
            /*
             * Look for default answer
             */
            if(strcmp(query_p->query, "?") == 0)
            {
                digi_query_cur_p = query_p->next;
                /* found default answer! */
                return query_runtime_replace(question, query_p->answer,
                                                 uidata_p);
            }
        }

        query_p = query_p->next;
    }

    digi_query_cur_p = NULL;

    return NULL;
}

static void send_autoreply_query(char *question, uidata_t *uidata_p)
{
    char  buf[MAX_DATA_LENGTH + 1];
    FILE *fd;
    char *p,*q;
    short def_answer;
    short number;
    int   ret;

    number = 1;

    def_answer = 0;

    p = query_first(question, uidata_p, def_answer);
    if(p == NULL)
    {
        /* Try default answer */
        def_answer = 1;

        p = query_first(question, uidata_p, def_answer);
    }

    if(p == NULL)
    {
        send_message(ACK, number++, "No messages specified", uidata_p);
    }
    else
    {
        while(p != NULL)
        {
            if((*p == ';') || (*p == ')') || (*p == '^'))
            {
                /* Do not include the '^' */
                if(*p == '^')
                {
                    p++;
                }
                send_item(number++, p, uidata_p);
            }
            else if(*p == '!')
            {
                if(p[1] == '!')
                {
                    /* command starts with !!, needs to be owner! */
                    p++;

                    if( (is_owner(uidata_p->originator) == 0)
                        &&
                        (internal_command == 0)
                        &&
                        (auto_command == 0)
                      )
                    {
                        send_message(ACK, number++, "Aborted, no permission",
                        uidata_p);
                        return;
                    }
                }

                /*
                 * Execute command
                 */
#ifndef __CYGWIN32__
                ret = system(&(p[1]));
        	vsay("system() which executed \"%s\" returned %d\r\n", &(p[1]), ret);
#else
                ret = cmd_system(&(p[1]));
        	vsay("cmd_system() which executed \"%s\" returned %d\r\n", &(p[1]), ret);
#endif /* __CYGWIN32__ */
            }
            else if(*p == '>')
            {
                /*
                 * Read from file
                 */
                fd = fopen(&p[1], "r");

                if(fd != NULL)
                {
                    while(fgets(buf, MAX_DATA_LENGTH, fd) != NULL)
                    {
                        /* truncate at the first CR or LF character, if any */
                        q = strchr(buf, '\r');
                        if(q != NULL) *q = '\0';
                        q = strchr(buf, '\n');
                        if(q != NULL) *q = '\0';

                        if((*buf == ';') || (*buf == ')'))
                        {
                            send_item(number++, buf, uidata_p);
                        }
                        else
                        {
                            /* truncate at maximum allowable length */
                            buf[MAX_MESSAGE_LENGTH] = '\0';

                            send_message(ACK, number++, buf, uidata_p);
                        }
                    }
                    fclose(fd);
                }
                else
                {
                    perror(&p[1]);
                }
            }
            else if(*p == ':')
            {
                send_message_string(number++, p, uidata_p);
            }
            else
            {
                if(*p == '\\') p++;
                send_message(ACK, number++, p, uidata_p);
            }
            p = query_next(question, uidata_p, def_answer);
        }
    }
    return;
}

static void send_autoreply_mheard_port(short port, uidata_t *uidata_p)
{
    char      message[MAX_MESSAGE_LENGTH + 1];
    char      call[CALL_LENGTH + 1];
    mheard_t *mheard_p;
    short     count;
    short     number;
    short     heard_nr;

    if(port > mac_ports())
    {
        sprintf(message, "Digi port %d not active", port);
        send_message(ACK, 1, message, uidata_p);
        return;
    }

    /*
     * count how many send_message calls we did, needed to spread
     * the responses over time
     */
    number = 1;

    /* Count the number of calls */
    heard_nr = 0;

    /* Go through the loop to count how many calls will be send */
    mheard_p = first_mheard(port - 1); /* 0 based port numbers */

    while((mheard_p != NULL) && (heard_nr < digi_heard_show))
    {
        heard_nr++;
        mheard_p = next_mheard(port - 1); /* 0 based port numbers */
    }

    /* THD7E: |Line numbr 1Line numbr 2Line numbr 3Line numb| */
    /*         Heard direct on port 1: none                   */
    /*         Heard direct on port 1: 1 call send            */
    /*         Heard direct on port 1: 8 calls     send       */
    switch(heard_nr) {
    case 0:  sprintf(message, "Heard direct on port %d: none", port);
             break;
    case 1:  sprintf(message, "Heard direct on port %d: 1 call sent",
                                        port);
             break;
    default: sprintf(message, "Heard direct on port %d: %d calls      sent",
                                        port, heard_nr);
             break;
    }

    send_message(ACK, number++, message, uidata_p);

    count = 0;
    heard_nr = 0;

    /* Layout using worst-case length calls */
    /* THD7E: |Line numbr 1Line numbr 2Line numbr 3Line numb| */
    /*         P1 PE1DNN-12  PE1DNN-12  PE1DNN-12  PE1DNN-12  */

    /* start with port number */
    sprintf(message, "P%d ", port);

    /* Go throug once again to dump the calls */
    mheard_p = first_mheard(port - 1); /* 0 based port numbers */

    while((mheard_p != NULL) && (heard_nr < digi_heard_show))
    {
        /*
         * count the heard stations to keep track of the size to show
         */
        heard_nr++;

        /* if there are already calls in the message then add 2 space */
        /* characters to separate the calls from eachother            */
        if(count > 0)
        {
            strcat(message, "  ");
        }

        /* increase count */
        count++;

        /* fill room of the size of a full call with spaces */
        /*           "PE1DNN-12"                            */
        strcpy(call, "         ");
        
        /* patch in the call from the mheard structure */
        strncpy(call, mheard_p->call, strlen(mheard_p->call));
        
        /* add this to the message */
        strcat(message, call);

        /* when there are 4 calls in the message, transmit */
        if(count == 4)
        {
            send_message(ACK, number++, message, uidata_p);

            /* start all over */
            count = 0;

            /* start with port number */
            sprintf(message, "P%d ", port);
        }

        mheard_p = next_mheard(port - 1); /* 0 based port numbers */
    }

    if(count != 0)
    {
        /* send remaining calls */
        send_message(ACK, number, message, uidata_p);
    }
}

static void send_autoreply_mheard_stat(uidata_t *uidata_p)
{
    char       message[MAX_MESSAGE_LENGTH + 1];
    char       stamp[MAX_MESSAGE_LENGTH + 1];
    short      count;
    mheard_t  *mheard_p;
    mheard_t  *mheard_last_p;
    struct tm *t;

    count = 0;

    mheard_p      = first_mheard(-1); /* on all ports */
    mheard_last_p = NULL;

    while(mheard_p != NULL)
    {
        count++;
        mheard_last_p = mheard_p;

        mheard_p = next_mheard(-1);
    }

    if(count != 0)
    {
        tzset();
        t = localtime(&(mheard_last_p->when));

        /* Line numbr 1Line numbr 2Line numbr 3Line numb  <- THD7 screen */
        /* Heard 32767 calls, from 12 Sep 2000, 10:11:23                 */

        strftime(stamp, MAX_MESSAGE_LENGTH - 12, /* size: "Heard 32767 " */
                        "calls, from %b %d %Y, %H:%M:%S", t);

        sprintf(message, "Heard %-5d %s", count, stamp);
    }
    else
    {
        strcpy(message, "Heard list  empty");
    }

    send_message(ACK, 1, message, uidata_p);
}

static void send_autoreply_mheard_call(char *call, uidata_t *uidata_p)
{
    char       message[MAX_MESSAGE_LENGTH + 1];
    char       stamp[MAX_MESSAGE_LENGTH + 1];
    short      port;
    short      count;
    mheard_t  *mheard_p;
    struct tm *t;

    /* Count the number of responses */
    count = 0;

    /* Check out all the ports */
    for(port = 0; port < mac_ports(); port++)
    {
        mheard_p = find_mheard(call, port);
        if(mheard_p != NULL)
        {
            /*
             * Found the call on this port, send a message back
             */

            tzset();
            t = localtime(&(mheard_p->when));

            strftime(stamp, MAX_MESSAGE_LENGTH - CALL_LENGTH - 1 /* port-num */,
                        " rx %b %d %Y - %H:%M:%S on port ", t);

            sprintf(message, "%9s%s%d", mheard_p->call, stamp,
                                        mheard_p->port + 1);

            count++;

            send_message(ACK, count, message, uidata_p);
        }
    }

    if(count == 0)
    {
        /* Counter is still 0, call was not found on any of the ports */

        sprintf(message, "Not found:  \"%s\"", call);
        send_message(ACK, 1, message, uidata_p);
    }
}

static void send_autoreply_dx_port(short port, uidata_t *uidata_p)
{
    char      message[MAX_MESSAGE_LENGTH + 1];
    char      dx_times[MAX_DX_TIMES + 1];
    char     *times_p;
    time_t    now;
    time_t    age;
    time_t    age_hour;
    mheard_t *dx_best_p;
    mheard_t *dx_second_p;
    short     number;

    /*
     * Get current time
     */
    now      = time (NULL);

    /*
     * Initialize age to something
     */
    age      = now; /* We want to see everything, unless overwritten... */
    age_hour = 0L;

    /*
     * Some general checking before searching for the best DX and friends
     */
    if(port > mac_ports())
    {
        sprintf(message, "Digi port %d not active", port);
        send_message(ACK, 1, message, uidata_p);
        return;
    }

    /*
     * Walk through the time table, make a copy before using strtok
     */
    strcpy(dx_times, digi_dx_times);

    /*
     * Count transmissions for separation, start with 1 
     */
    number = 1;

    /*
     * Tokenize through the times we want
     */
    times_p = strtok(dx_times,",\0");

    /* repeat until times_p is NULL or empty string */
    while((times_p != NULL) && (*times_p != '\0'))
    {
        /*
         * Detemine age - how far we should go back into history
         */
        if(strcmp(times_p, "all") != 0)
        {
            /*
             * Calculate 'age' in seconds from the value in hours.
             * There are 3600 seconds in an hour.
             */
            age_hour = (time_t) strtoul(times_p, NULL, 10);
            age      = (time_t) (age_hour * 3600L);
        }

        /*
         * set times_p for next time so we can use strtok for something
         * else during processing
         */
        times_p = strtok(NULL, "\0");

        /* find the best DX in the list (with 0 based port number) */
        find_best_dx(port - 1, &dx_best_p, &dx_second_p, now - age,
                                                            MAX_DX_DISTANCE);

        /* Line numbr 1Line numbr 2Line numbr 3Line numb  <- THD7 screen */
        /* DX-P1 of all 20000.1 km PE1DNN-12   PE1DNN-12"                */
        if(dx_best_p == NULL)
        {
            /* Line numbr 1Line numbr 2Line numbr 3Line numb  <- THD7 screen */
            /* DX-P1 empty DX list!                                          */
            /* DX-P1 no rx in last 10h                                       */
            if(age == now)
            {
                sprintf(message, "DX-P%d empty DX list!", port);
            }
            else
            {
                sprintf(message, "DX-P%d no rx in last %ldh", port, age_hour);
            }
        }
        else
        {
            if(age == now)
            {
                sprintf(message, "DX P%d of all %5ld.%ld %s %-9s", port,
                                  dx_best_p->distance / 10L,
                                  dx_best_p->distance % 10L,
                                  digi_dx_metric,
                                  dx_best_p->call);
            }
            else
            {
                sprintf(message, "DX P%d of %2ldh %5ld.%ld %s %-9s", port,
                                  age_hour,
                                  dx_best_p->distance / 10L,
                                  dx_best_p->distance % 10L,
                                  digi_dx_metric,
                                  dx_best_p->call);
            }

            if(dx_second_p != NULL)
            {
                strcat(message,"   ");
                strcat(message,dx_second_p->call);
            }
        }

        /* send the dx message */
        send_message(ACK, number, message, uidata_p);

        /* increase for next transmission */
        number++;

        /*
         * Get the next
         */
        times_p = strtok(times_p, ",\0");
    }

    return;
}

static void send_autoreply_dx_call(char *call, uidata_t *uidata_p)
{
    char       message[MAX_MESSAGE_LENGTH + 1];
    mheard_t  *m_p;    
    mheard_t  *mheard_p;
    short      port;
    char      *tmp_case;

    tmp_case = call;
    while(*tmp_case != '\0')
    {
        *tmp_case = toupper(*tmp_case);
        tmp_case++;
    }

    mheard_p = NULL;    

    /*
     * Assume a station with the same call also has the same distance. In
     * that case it does not matter from which port the data is read. Just
     * find the most recent and complete entry.
     */
    for(port = 0; port < mac_ports(); port++)
    {
        m_p = find_mheard(call, port);

        if(m_p != NULL)
        {
            if(mheard_p == NULL)
            {
                /* This is always better than nothing */
                mheard_p = m_p;
            }
            else
            {
                if(mheard_p->distance == -1L)
                {
                    /*
                     * The distance is still not known, just take over
                     * this entry, maybe we have more luck this time.
                     */
                    mheard_p = m_p;
                }
            }
        }
    }

    if(mheard_p == NULL)
    {
        /* Line numbr 1Line numbr 2Line numbr 3Line numb  <- THD7 screen */
        /* Not found:"PE1DNN-12"                                         */

        sprintf(message, "Not found:  \"%s\"", call);
    }
    else
    {
        if(mheard_p->distance == -1L)
        {
            /* Line numbr 1Line numbr 2Line numbr 3Line numb  <- THD7 screen */
            /* Unknown pos:"PE1DNN-12"                                       */

            sprintf(message, "Unknown pos:\"%s\"", call);
        }
        else
        {
            /* Line numbr 1Line numbr 2Line numbr 3Line numb  <- THD7 screen */
            /* PE1DNN-12     156.4 km  bearing 000 degrees                   */

            sprintf(message, "%9s   %5ld.%ld %s  bearing %03d degrees",
                          mheard_p->call,
                          mheard_p->distance / 10L,
                          mheard_p->distance % 10L,
                          digi_dx_metric,
                          mheard_p->bearing);
        }
    }

    send_message(ACK, 1, message, uidata_p);
}

#ifdef _TELEMETRY_
static void send_autoreply_tlm_source_analog(short source, uidata_t *uidata_p)
{
    char      message[MAX_MESSAGE_LENGTH + 1];

    /*
     * Some general checking before retrieving a telemetry message
     */
    if(source > telemetry_sources())
    {
        sprintf(message, "Telemetry   source A%d   not active", source);
        send_message(ACK, 1, message, uidata_p);
        return;
    }

    /* Create telemetry response */
    telemetry_response(source, 0, message);

    /* send the telemetry message */
    send_message(ACK, 1, message, uidata_p);

    return;
}

static void send_autoreply_tlm_source_digital(short bitnr, uidata_t *uidata_p)
{
    char      message[MAX_MESSAGE_LENGTH + 1];
    short     max_source;

    /*
     * Some general checking before retrieving a telemetry message
     */
    max_source = telemetry_sources();

    if(max_source < 6)
    {
        sprintf(message, "Telemetry   source B%d   not active", bitnr);
        send_message(ACK, 1, message, uidata_p);
        return;
    }

    /* Create telemetry response */
    telemetry_response(6, bitnr, message);

    /* send the telemetry message */
    send_message(ACK, 1, message, uidata_p);

    return;
}
#endif /* _TELEMETRY_ */

static void send_autoreply_aprsd(uidata_t *uidata_p)
{
    char      message[MAX_MESSAGE_LENGTH + 1];
    mheard_t *mheard_p;
    short     count;

    strcpy(message, "Directs=");

    mheard_p = first_mheard(-1); /* -1 = on all ports */

    count = 0;
    while(mheard_p != NULL)
    {
        /* increase count */
        count++;

        /* catinate the call to the message with leading space */
        strcat(message, " ");
        strcat(message, mheard_p->call);
        
        /* when there are 5 calls in the message, transmit and quit */
        if(count == 5)
        {
            send_message(NOACK, 1, message, uidata_p);
            return;
        }

        mheard_p = next_mheard(-1);
    }

    if(count != 0)
    {
        /* send calls */
        send_message(NOACK, 1, message, uidata_p);
    }
}

static void send_autoreply_ping_aprst(uidata_t *uidata_p)
{
    char  data[MAX_DATA_LENGTH + 1];
    short i;

    /*
     * Worst-case length: 10 calls with full SSID = 10 * 9 = 90 bytes
     *                     1 time ">"                         1 byte
     *                     8 times ","                        8 bytes
     *                     1 times "*"                        1 byte
     *                     1 times ":"                        1 byte
     *                    grand total                       101 bytes
     *
     * This will fit into our data-space without problems
     */

    data[0] = '\0';

    strcat(data, uidata_p->originator);
    strcat(data,">");
    strcat(data, uidata_p->destination);

    for(i = 0; i < uidata_p->ndigi; i++)
    {
        strcat(data,",");
        strcat(data,uidata_p->digipeater[i]);

        if((uidata_p->digi_flags[i] & H_FLAG) != 0)
        {
            /* this one has the H_FLAG set */
            if(i == (uidata_p->ndigi -1))
            {
                /* this is the last digi, add asterix to this call */
                strcat(data,"*");
            }
            else
            {
                /*
                 * Not the last digi, look if the next one also has the
                 * H_FLAG set
                 */
                if((uidata_p->digi_flags[i + 1] & H_FLAG) == 0)
                {
                    /* next digi did not repeat, put asterix on this one */
                    strcat(data,"*");
                }
            }
        }
    }

    /* add a colon at the end */
    strcat(data, ":");

    /*
     * note: send_message will truncate the data to the allowed maximum
     * message length, the rest is lost
     */
    send_message(NOACK, 1, data, uidata_p);
}

static void send_autoreply_aprsm(uidata_t *uidata_p)
{
    message_t *message_p;
    char       message_prefix[MSG_PREFIX_LENGTH + 1];

    /* prepare a message prefix with the originator-call for comparison */
    /* prepare 2 colons with space for a full call ":PE1DNN-12:"        */
    strcpy(message_prefix, ":         :");

    /* patch in the originator-call */
    strncpy(&(message_prefix[1]), uidata_p->originator,
                                strlen(uidata_p->originator));
    
    /* loop though the messages to transmit */
    message_p = msg_list_p;
    while(message_p != NULL)
    {
        /* go through all unacked transmit messages */
        if(message_p->count > 0)
        {
            /* check if this is for the requester */
            if(strncmp(message_p->message, message_prefix, 
                                        MSG_PREFIX_LENGTH) == 0)
            {
                /* transmit_message */
                transmit_message(message_p);
            }
        }
        /* move to next */
        message_p = message_p->next;
    }
}

static void send_autoreply_date(uidata_t *uidata_p)
{
    char       message[MAX_MESSAGE_LENGTH + 1];
    time_t     curtime;
    struct tm *loctime;

    /* Make a timestamp */

    /* Determine current time and convert into local representation. */
    curtime = time (NULL);
    loctime = localtime (&curtime);

    /* Format the time */
    strftime(message, MAX_MESSAGE_LENGTH- 1, "%b %d %Y - %H:%M:%S", loctime);

    /* send date and time */
    send_message(ACK, 1, message, uidata_p);
}

static void send_autoreply_command(char *command, uidata_t *uidata_p)
{
    char      *res;
    char       message[MAX_MESSAGE_LENGTH + 1];
    char       date[MAX_MESSAGE_LENGTH + 1];
    time_t     curtime;
    struct tm *loctime;

    /* send date and time */
    res = do_command(command);
    if(res == NULL)
    {
        send_autoreply_query("!error", uidata_p);
        return;
    }

    send_autoreply_query("!ok", uidata_p);

    /* Make a timestamp */

    /* Determine current time and convert into local representation. */
    curtime = time (NULL);
    loctime = localtime (&curtime);

    /* Format the time */
    strftime(date, MAX_MESSAGE_LENGTH - 1, "%b %d %Y %H:%M:%S", loctime);

    strcpy(message, "Cmd replied ");
    strcat(message, res);

    /* if there is room add timestamp */
    if((strlen(message) + strlen(date)) < (MAX_MESSAGE_LENGTH - 5))
    {
        strcat(message, " on ");
        strcat(message, date);
    }
    send_message(ACK, 2, message, uidata_p);

    return;
}

#ifdef _SATTRACKER_
/*
 * Update satellite database from a tle file.
 *
 * The name of the tle file is defined in the .ini file and should already
 * be present on the system. When a "utle" command is sent to DIGI_NED it will 
 * update its satellite database from this tle file.
 *
 */
static void send_autoreply_utle(uidata_t *uidata_p)
{
    char       message[MAX_MESSAGE_LENGTH + 1];
    short      result;

    if(digi_sat_active == 0)
    {
        vsay("Sattracker not active, missing info in .ini file\r\n");
        send_message(ACK, 1, "Sattracker  not active", uidata_p);
        return;
    }

    if((digi_tle_file == NULL) || (digi_tle_file[0] == '\0'))
    {
        vsay("Sattracker not active, missing TLE file specification in "
             ".ini file\r\n");
        send_message(ACK, 1, "Sattracker  not active", uidata_p);
        return;
    }

    result = AutoUpdate(digi_tle_file);
    switch (result)
    {
        case 0: 
                vsay("Successful update of satellite database from TLE "
                     "file.\r\n");
                strcpy(message, "Upd complete TLE read");
                break;
        case 1:
                vsay("No filename specified to update TLE file from!\r\n");
                strcpy(message, "No filename for TLE file");
                break;
        case 2: 
                vsay("Invalid input file name (%s) specified!\r\n",
                                                digi_tle_file);
                strcpy(message, "Invalid name of TLE file");
                break;
        case 3:
                vsay("Unable to open input TLE file (%s)!\r\n", digi_tle_file);
                strcpy(message, "Can't open  TLE file");
                break;
        default:
                vsay("Update failed, unknown reason!\r\n");
                strcpy(message, "Update fail unknown why");
                break;
    }

    /* send result of update */
    send_message(ACK, 1, message, uidata_p);
}


/*
 * Transmit satellite object.
 *
 * Transmission is not done here, but the object is added to a
 * list of messages which are scheduled for transmission.
 * The object is transmitted only once.
 */
void transmit_satobject(char *object, short port)
{
    message_t *message_p;

    /* create a new message */
    message_p = malloc(sizeof(message_t));

    if(message_p == NULL)
    {
        /* out of memory */
        return;
    }

    /* 
     * Copy the object in. Maximum length of a message is = MAX_MESSAGE_LENGTH
     * bytes.
     */
    message_p->message[0] = '\0';
    strncpy(message_p->message, object, MAX_MESSAGE_LENGTH);

    /* make sure there is a null terminator at the end */
    message_p->message[MAX_MESSAGE_LENGTH] = '\0';

    /* send back where the message came from */
    message_p->port = port;

    /* no message id, since we're broadcasting an object*/
    message_p->id = -1;

    /* 
     * Don't repeat the object.
     */
    message_p->count = 1;

    /* add '\r' to the message */
    strcat(message_p->message, "\r");

    /*
     * Start in 10 seconds.
     */
    message_p->interval = 10;

    /* start interval-time seconds from now */
    start_timer(&(message_p->timer), message_p->interval * TM_SEC);

    /* add to msg_list_p, new object will be placed in front of the list */
    message_p->next = msg_list_p;
    msg_list_p = message_p;
}


/*
 * Handle the "sat" or "trk" command. Inform the user if the satellite is
 * in range or not and generate an appropraite error message if the
 * satellite is not in the database or if it is not supported by the
 * satellite tracking module.
 *
 * Also, if requested satellite is in found in the database, an object will
 * be returned indicating its location. When the satellite is in range, the
 * object will contain doppler corrected uplink and downlink frequencies,
 * elevation angle and azimuth, and in which mode the satellite is in. When
 * the satellite is not in range, the object will indicate its next pass
 * (AOS) in either local time or UTC.
 *
 * When the value of "track" == TRACK then DIGI_NED will start tracking
 * If the satellite is in range, then the object will be updated at
 * an interval specified by sat_in_range_interval in the *.ini file. If the
 * satellite is not in range, then the object will be updated at an interval
 * specified by sat_out_of_range_interval in the *.ini file.
 *
 * When the value of "track" == NOTRACK then DIGI_NED will only send
 * the satellite object once.
 *
 * When the value of "when" == INRANGE then DIGI_NED will only send
 * the satellite object when it is in range (used by 'sat now').
 */
static void send_autoreply_sat(char *sat, uidata_t *uidata_p,
                               track_t track, when_t when)
{
    int        result;
    short      longpass;
    char       satobject[MAX_MESSAGE_LENGTH+1];
    char       satellite[5];
    char       message[MAX_MESSAGE_LENGTH+1];
    short      number;
    char      *sat_p;

    if(digi_sat_active == 0)
    {
        vsay("Sattracker not active, missing info in .ini file\r\n");
        send_message(ACK, 1, "Sattracker  not active", uidata_p);
        return;
    }

    /* counter to spread responses */
    number = 1;

    strcpy(message, "");
    sat_p = strtok(sat, ", \t\n\r");

    while(sat_p != NULL)
    {
        /* Prepare 4 character satellite name */
        strncpy(satellite, sat_p, 4);
        satellite[4] = '\0';

        /* keep adding '_' while not 4 characters long */
        while(strlen(satellite) < 4)
        {
            strcat(satellite, "_");
        }

        /* Save rest of satellite query for the next loop */
        sat_p = strtok(NULL, "\0");

        /* Get satellite object. */
        result = get_sat_object(satellite, digi_use_local, satobject,
                                                    digi_dx_metric, &longpass);
        if(when == ALWAYS)
        {
            switch (result)
            {
                case 0:
                        vsay("Requested satellite not in database.\r\n");
                        sprintf(message, "Invalid sat! ex:sat ao40");
                        break;
                case 1:
                        vsay("No passes can occur for the specified "
                             "satellite for our position.\r\n");
                        sprintf(message, "Invalid sat! ex:sat ao40");
                        break;
                case 2: 
                        vsay("Orbital predictions cannot be made "
                             "for geostationary satellite.\r\n");
                        sprintf(message, "Invalid sat! ex:sat ao40");
                        break;
                case 3:
                        vsay("Displaying %s. This satellite is not "
                             "in range.\r\n", satellite);
                        if(track == TRACK)
                        {
                            add_satobject(satellite, uidata_p, 1, number);
                        }
                        else
                        {
                            add_satobject(satellite, uidata_p, 0, number);
                        }

                        message[0] = '\0';
                        /* Copy satname */
                        strncpy(message, &satobject[1], 5);
                        message[5] = '\0';
                        strcat(message, &satobject[45]);
                        break;
                case 4:
                        vsay("Displaying %s. This satellite is in range.\r\n",
                                satellite);
                        if(track == TRACK)
                        {
                            add_satobject(satellite, uidata_p, 1, number);
                        }
                        else
                        {
                            add_satobject(satellite, uidata_p, 0, number);
                        }
                        sprintf(message, "%s is     in range!", satellite);
                        break;
                default:
                        sprintf(message, "Invalid sat! ex:sat ao40");
                        break;
            }

            send_message(ACK, number++, message, uidata_p);
        }
        else /* when == INRANGE */
        {
            /* Only report if in range */
            if(result == 4)
            {
                vsay("Displaying %s. This satellite is in range.\r\n",
                                satellite);
                if(track == TRACK)
                {
                    add_satobject(satellite, uidata_p, 1, number);
                }
                else
                {
                    add_satobject(satellite, uidata_p, 0, number);
                }
                sprintf(message, "%s is     in range!", satellite);

                send_message(ACK, number++, message, uidata_p);
            }
        }

        /* Get next satellite if available */
        sat_p = strtok(sat_p, ", \t\n\r");
    }
}

/*
 * Handle "sat list" to display all the satellites that are supported
 * (i.e. that appear in DIGI_NED's satellite database)
 */
static void send_autoreply_sat_list(uidata_t *uidata_p)
{
    char  *satellite;
    char   message[MAX_MESSAGE_LENGTH + 1];
    short  number;
    short  count;
    short  sat_nr;

    if(digi_sat_active == 0)
    {
        vsay("Sattracker not active, missing info in .ini file\r\n");
        send_message(ACK, 1, "Sattracker  not active", uidata_p);
        return;
    }

    /*
     * count how many send_message calls we did, needed to spread
     * the responses over time
     */
    number = 1;

    satellite = get_amsat_des_at_index(0);

    if(satellite == NULL)
    {
        strcpy(message, "Found no    satellites");
        send_message(ACK, number++, message, uidata_p);
        return;
    }

    count  = 0;
    sat_nr = 0;

    /*
     * Layout using worst-case length of satellites
     * 
     * THD7E: |Line numbr 1Line numbr 2Line numbr 3Line numb|
     *         SO33 RS12   RS15 AO40   SO41 SO42   MIR_ ISS_
     */

    /* start with empty message number */
    message[0] = '\0';

    /* Go through the list once again to dump the satellites */
    satellite = get_amsat_des_at_index(sat_nr);

    while(satellite != NULL)
    {
        /* if there are already satellites in the message then add space */
        /* characters to separate the satellites from eachother          */
        if(count > 0)
        {
            if((count & 0x01) != 0)
            {
                /* on odd number add one space */
                strcat(message, " ");
            }
            else
            {
                /* on even number add three spaces */
                strcat(message, "   ");
            }
        }

        /* increase count */
        count++;

        /* satellites are all exactly 4 bytes! */
        
        /* add this to the message */
        strcat(message, satellite);

        /* when there are 8 satellites in the message, transmit */
        if(count == 8)
        {
            send_message(ACK, number++, message, uidata_p);

            /* start all over */
            count = 0;

            /* start with empty message */
            message[0] = '\0';
        }

        sat_nr++;
        satellite = get_amsat_des_at_index(sat_nr);
    }

    if(count != 0)
    {
        /* send remaining calls */
        send_message(ACK, number, message, uidata_p);
    }
}

/*
 * Handle "sat now" to all the satellites that are in range
 */
static void send_autoreply_sat_now(uidata_t *uidata_p)
{
    char  *satellite;
    char   message[MAX_MESSAGE_LENGTH + 1];
    short  number;
    char   sats[121]; /* (4 (=satlenght) + 1 (=space)) * 24 (nr of sats) + 1 */
    short  sat_nr;

    if(digi_sat_active == 0)
    {
        vsay("Sattracker not active, missing info in .ini file\r\n");
        send_message(ACK, 1, "Sattracker  not active", uidata_p);
        return;
    }

    /*
     * count how many send_message calls we did, needed to spread
     * the responses over time
     */
    number = 1;

    satellite = get_amsat_des_at_index(0);

    /*
     * Build string with sattelite names
     */
    satellite = get_amsat_des_at_index(0);

    if(satellite == NULL)
    {
        strcpy(message, "Found no    satellites");
        send_message(ACK, number++, message, uidata_p);
        return;
    }

    sat_nr = 0;

    sats[0] = '\0';

    /* Go through the list once again to dump the satellites */
    satellite = get_amsat_des_at_index(sat_nr);

    while(satellite != NULL)
    {
        sat_nr++;

        if(sat_nr != 0)
        {
            strcat(sats, " "); /* space after previous sat */
        }
        strcat(sats, satellite); /* space after previous sat */

        satellite = get_amsat_des_at_index(sat_nr);
    }

    /* request the sat data, send only when in range! */
    send_autoreply_sat(&sats[0], uidata_p, NOTRACK, INRANGE);
}
#endif /* _SATTRACKER_ */

static void send_autoreply_exit(uidata_t *uidata_p)
{
    send_message(SHUTDOWN, 1, "DIGI_NED: System shutdown after acknowlege",
                               uidata_p);
}

static void send_autoreply(char *bp, uidata_t *uidata_p)
{
    char  *p;

    if((internal_command == 0) && (auto_command == 0))
    {
        vsay("New message, send auto-response message back\r\n");
    }

    /* start at the beginning */
    p = bp;

    if((*p != '!') && (*p != '/') && (*p != '\\'))
    {
        /*
         * skip starting character if the query starts with
         * any non-alhanumeric character, except '!', '/' and '\'
         */
        if(isalnum(*p) == 0)
        {
            p++;
        }
    }

    if((strcmp(p, "ping?") == 0) || (strcmp(p, "aprst") == 0))
    {
        send_autoreply_ping_aprst(uidata_p);
        return;
    }
    else if(strcmp(p, "aprsd") == 0)
    {
        send_autoreply_aprsd(uidata_p);
        return;
    }
    else if(strcmp(p, "aprsm") == 0)
    {
        send_autoreply_aprsm(uidata_p);
        return;
    }
    else if((strcmp(p, "aprs?") == 0) || (strcmp(p, "aprsp") == 0))
    {
        send_all(BEACON);
        return;
    }
    else if(strcmp(p, "wx?") == 0)
    {
        send_all(WX);
        return;
    }
    else if(strcmp(p, "date") == 0)
    {
        send_autoreply_date(uidata_p);
        return;
    }
    else if(strncmp(p, "mheard ", 7) == 0)
    {
        if( (strcmp(p, "mheard <port>") == 0)
            ||
            (strcmp(p, "mheard <call>") == 0)
          )
        {
            send_autoreply_query(p, uidata_p);
            return;
        }
        else if(strlen(p) == 8)
        {
            if((p[7] >= '0') && (p[7] < '9'))
            {
                send_autoreply_mheard_port(atoi(&p[7]), uidata_p);
                return;
            }
            else
            {
                send_autoreply_query("mheard <port>", uidata_p);
                return;
            }
        }
        else
        {
            if(stricmp(&p[7],"stat") == 0)
            {
                send_autoreply_mheard_stat(uidata_p);
                return;
            }
            else
            {
                if(strlen(&p[7]) < 10)
                {
                    send_autoreply_mheard_call(&p[7], uidata_p);
                    return;
                }
                else
                {
                    send_autoreply_query("mheard <call>", uidata_p);
                    return;
                }
            }
        }
    }
    else if(strncmp(p, "mh ", 3) == 0)
    {
        if( (strcmp(p, "mh <port>") == 0)
            ||
            (strcmp(p, "mh <call>") == 0)
          )
        {
            send_autoreply_query(p, uidata_p);
            return;
        }
        else if(strlen(p) == 4)
        {
            if((p[3] >= '0') && (p[3] < '9'))
            {
                send_autoreply_mheard_port(atoi(&p[3]), uidata_p);
                return;
            }
            else
            {
                send_autoreply_query("mh <port>", uidata_p);
                return;
            }
        }
        else
        {
            if(stricmp(&p[3],"stat") == 0)
            {
                send_autoreply_mheard_stat(uidata_p);
                return;
            }
            else
            {
                if(strlen(&p[3]) < 10)
                {
                    send_autoreply_mheard_call(&p[3], uidata_p);
                    return;
                }
                else
                {
                    send_autoreply_query("mh <call>", uidata_p);
                    return;
                }
            }
        }
    }
    else if(strncmp(p, "dx ", 3) == 0)
    {
        if( (strcmp(p, "dx <port>") == 0)
            ||
            (strcmp(p, "dx <call>") == 0)
          )
        {
            send_autoreply_query(p, uidata_p);
            return;
        }
        else if(strlen(p) == 4)
        {
            if((p[3] >= '0') && (p[3] < '9'))
            {
                send_autoreply_dx_port(atoi(&p[3]), uidata_p);
                return;
            }
            else
            {
                send_autoreply_query("dx <port>", uidata_p);
                return;
            }
        }
        else
        {
            if(strlen(&p[3]) < 10)
            {
                send_autoreply_dx_call(&p[3], uidata_p);
                return;
            }
            else
            {
                send_autoreply_query("dx <call>", uidata_p);
                return;
            }
        }
    }

#ifdef _TELEMETRY_
    else if(strncmp(p, "tlm ", 4) == 0)
    {
        if(strcmp(p, "tlm <src>") == 0)
        {
            send_autoreply_query(p, uidata_p);
            return;
        }
        else
        {
            if( (p[4] == 'a')
                &&
                (p[5] >= '1') && (p[5] <= '5')
                &&
                (p[6] == '\0')
              )
            {
                send_autoreply_tlm_source_analog(atoi(&p[5]), uidata_p);
                return;
            }
            else if( (p[4] == 'b')
                     &&
                     (p[5] >= '1') && (p[5] <= '8')
                     &&
                     (p[6] == '\0')
                   )
            {
                send_autoreply_tlm_source_digital(atoi(&p[5]), uidata_p);
                return;
            }
            else if((p[4] == 'b') && (p[5] == '\0'))
            {
                send_autoreply_tlm_source_digital(0, uidata_p);
                return;
            }
            else
            {
                send_autoreply_query("tlm <src>", uidata_p);
                return;
            }
        }
    }
#endif /* _TELEMETRY_ */

    else if(strncmp(p, "exit", 4) == 0)
    {
        if( ( (is_owner(uidata_p->originator) == 1)
              &&
              (digi_enable_exit == 1)
              &&
              (auto_command == 0)
            )
            ||
            (internal_command == 1)
          )
        {
            if(strncmp(p, "exit ", 5) == 0)
            {
                exit_info.exitcode = atoi(&p[5]) & 0x00ff;

                vsay("Recieved shutdown request by digi owner's remote "
                    "\"?exit %d\" command\r\n", exit_info.exitcode);
            }
            else
            {
                exit_info.exitcode = 2;

                vsay("Recieved shutdown request by digi owner's remote "
                    "\"?exit\" command\r\n");
            }
            if(internal_command == 0)
            {
                send_autoreply_exit(uidata_p);
            }
            else
            {
                say("Exit via keyboard command, exit code: %d\r\n",
                                                    exit_info.exitcode);

                /* Cleanup before leaving */
#ifdef _LINUX_
                kb_exit();
#endif

#ifdef _SERIAL_
                serial_exit();
#endif /* _SERIAL_ */

                mac_exit();

                fflush(stdout);

                exit(exit_info.exitcode);
            }
            return;
        }
    }
    else if(p[0] == '!')
    {
        if( (is_owner(uidata_p->originator) == 1)
            ||
            (internal_command == 1)
            ||
            (auto_command != 0)
          )
        {
            send_autoreply_command(p, uidata_p);
            return;
        }
    }

#ifdef _SATTRACKER_
    else if(strcmp(p, "utle") == 0)
    {
        if( (is_owner(uidata_p->originator) == 1)
            ||
            (internal_command == 1)
            ||
            (auto_command != 0)
          )
        {
            vsay("Updating Satellite database...\r\n");
            send_autoreply_utle(uidata_p);
        }
        else
        {
            vsay("Ignoring update request for satellite database - "
                 "requestor is not digi owner.\r\n");
            send_message(ACK, 1, "upd request is ignored", uidata_p);
        }
        return;
    }
    else if(strncmp(p, "sat ", 4) == 0)
    {
        if(strcmp(p, "sat <sat>") == 0)
        {
            send_autoreply_query(p, uidata_p);
            return;
        }
        else
        {
            if(strcmp(&p[4], "list") == 0)
            {
                send_autoreply_sat_list(uidata_p);
            }
            else if(strcmp(&p[4], "now") == 0)
            {
                send_autoreply_sat_now(uidata_p);
            }
            else
            {
                send_autoreply_sat(&p[4], uidata_p, NOTRACK, ALWAYS);
            }
            return;
        }
    }
    else if(strncmp(p, "trk ", 4) == 0)
    {
        if(strcmp(p, "trk <sat>") == 0)
        {
            send_autoreply_query(p, uidata_p);
            return;
        }
        else
        {
            send_autoreply_sat(&p[4], uidata_p, TRACK, ALWAYS);
            return;
        }
    }
    else if(strcmp(p, "sats") == 0)
    {
        send_autoreply_sat_list(uidata_p);
        return;
    }
#endif /* _SATTRACKER_ */

    send_autoreply_query(p, uidata_p);
}

static void parse_message(char *bp, uidata_t *uidata_p)
{
    char      *p;
    char      *tmp_case;
    short      id;
    short      new;
    char       stamp[MAX_MESSAGE_LENGTH + 1];
    time_t     curtime;
    struct tm *loctime;

    /* Make a timestamp */

    /* Determine current time and convert into local representation. */
    curtime = time (NULL);
    loctime = localtime (&curtime);

    /* Format the time */
    strftime(stamp, MAX_MESSAGE_LENGTH- 1, "%b %d %Y - %H:%M:%S", loctime);

    /* strip '\r' from message and truncate there  */
    p = strrchr(bp, '\r');
    if(p != NULL)
    {
        *p = '\0';
    }

    vsay("\r\n===========\r\n");
    vsay("Got message: \"%s\" from %s at %s\r\n",
                               bp, uidata_p->originator, stamp);

    p = strrchr(bp, '{');
    if(p == NULL)
    {
        /* No sequence number. Check if it is and 'ack' or 'rej'     */
        /* On rej also do not retransmit, the receiver is overloaded */
        if( (strncmp("ack", bp, 3) == 0)
            ||
            (strncmp("rej", bp, 3) == 0)
          )
        {
            id = atoi(&bp[3]);

            /* remove message */
            remove_message(id);

            /* 
             * check if we are closing down and if this is the ack
             * the close down procedure was waiting for
             */
            if(exit_info.state == CLOSING_DOWN)
            {
                if(id == exit_info.id)
                {
                    /* yes, shutdown acknoledged */
                    vsay("Exiting on %s...\r\n", stamp);

                    /* Cleanup before leaving */
#ifdef _LINUX_
                    kb_exit();
#endif

#ifdef _SERIAL_
                    serial_exit();
#endif /* _SERIAL_ */

                    mac_exit();

                    fflush(stdout);

                    exit(exit_info.exitcode);
                }
            }

            /* for an ACK we are finished now */
            return;
        }
    }
    else
    {
        /* truncate the message and retrieve the sequence number */

        /* truncate message at this point */
        *p = '\0';
        /* advance to just after the '{' */
        p++;

        send_ack(p, uidata_p);
    }

    /*
     * Walk back from the end to the beginning and remove all trailing
     * spaces and tabs
     */
    p = strrchr(bp, '\0');
    while(p != bp)
    {
        /* post decrement */
        p--;

        if((*p == ' ') || (*p == '\t'))
        {
            *p = '\0';
        }
        else
        {
            /* not a space or tab, break out of the loop */
            break;
        }
    }

    /* convert to lowercase */
    tmp_case = bp;
    while(*tmp_case != '\0')
    {
        *tmp_case = tolower(*tmp_case);
        tmp_case++;
    }

    new = is_new_message(bp, uidata_p);

    /* take only messages which are new for this sender */
    if(new == 1)
    {
        /* remember that we already got this message */
        keep_old_message(bp, uidata_p);

        /* display activity (option is requested upon startup) */
        asay("Honored request: \"%s\" from %s on %s\r\n",
                               bp, uidata_p->originator, stamp);

        /* new message, send auto-response message back */
        send_autoreply(bp, uidata_p);
    }
    else
    {
        /* display activity (option is requested upon startup) */
        asay("Ignored request: \"%s\" from %s on %s\r\n",
                               bp, uidata_p->originator, stamp);
    }
}

void do_messages(void)
{
    message_t *message_p;
    message_t *next_message_p;
    message_t *previous_message_p;

    /* first do the transmissions, then cleanup */

    /* loop though the messages to see which ones are due to transmit */
    message_p = msg_list_p;
    while(message_p != NULL)
    {
        /* transmit if the timer for this message is elapsed */
        if(is_elapsed(&(message_p->timer)))
        {
            /* time to transmit the message */
            if(message_p->count > 0)
            {
                /* transmit_message */
                transmit_message(message_p);

                /* lower the count */
                message_p->count--;

                /*
                 * we are finished with transmission of this message,
                 * display visual separator.
                 */
                display_separator();

                if((message_p->message[0] == ';')
                   || 
                   (message_p->message[0] == ')'))
                {
                    /*
                     * For objects and items:
                     * fixed time of 60 seconds before retransmission
                     */
                    message_p->interval = 60 * TM_SEC;
                }
                else
                {
                    /*
                     * For messages:
                     * every time double the time before retransmission
                     */
                    message_p->interval *= 2;
                }

                /* 
                 * setup the timer
                 */
                start_timer(&(message_p->timer), message_p->interval);
            }
        }
        /* move to next */
        message_p = message_p->next;
    }

    /*
     * loop though messages to see which ones can be removed
     *
     * A message can be removed if the count has either reached
     * zero or is set to zero upon reception of an ack. The timer
     * must be expired so that there was enough time to acknowledge
     * the last transmission (this important when an action is
     * triggered by the ack, for example shutdown).
     */
    message_p          = msg_list_p;
    previous_message_p = NULL;
    while(message_p != NULL)
    {
        /* save next */
        next_message_p = message_p->next;
        
        /* 
         * if not for repetition anymore and time to receive an ack
         * is passed then remove the message
         */
        if((message_p->count == 0) && (is_elapsed(&(message_p->timer))))
        {
            /* 
             * if the shutdown messgae is about to be destroyed
             * go back to normal operation (shutdown was not
             * acknowledged)
             */
            if(exit_info.state == CLOSING_DOWN)
            {
                if(message_p->id == exit_info.id)
                {
                    /* shutdown message not acknowledged */
                    vsay("Shutdown aborted, "
                        "switched back to active operation\r\n");
                    exit_info.state = ACTIVE;
                }
            }

            /* destroy this message */
            if(previous_message_p == NULL)
            {
                /* still at the beginning of the list */
                msg_list_p = next_message_p;
            }
            else
            {
                /* not at the beginning of the list */
                previous_message_p->next = next_message_p;
            }

            /* if an ID is used free it */
            if(message_p->id != -1)
            {
                message_id[(short) message_p->id] = 0;
            }

            /* throw away this message */
            free(message_p);

            /* debugging, dump outstanding message ids */
            {
                short i;
                vsay("Outstanding msg-id's: ");
                for(i = 0; i < MAX_NR_OF_IDS; i++)
                {
                    if(message_id[i] != 0)
                    {
                        vsay("%d,", i);
                    }
                }
                vsay("\r\n");
            }
            display_separator();

            /* let message_p point to next message */
            message_p = next_message_p;
        }
        else
        {
            /* keep this message */
            previous_message_p = message_p;
            message_p = next_message_p;
        }
    }
}

short recv_message(uidata_t *uidata_p)
{
    short res;
    char *bp;
    char  data[MSG_PREFIX_LENGTH + MAX_MESSAGE_LENGTH + MSG_SUFFIX_LENGTH + 1];
    char  message_prefix[MSG_PREFIX_LENGTH + 1 + 3];
    char  packet_originator[CALL_LENGTH];
    char  message_originator[CALL_LENGTH];
    enum  { NOT_TO_ME, TO_ME_NORMAL, TO_ME_ALTERNATIVE } prefix_format;

    /* only support for normal AX.25 UI frames, if not, bail out */
    if(((uidata_p->primitive & ~0x10) != 0x03) || (uidata_p->pid != 0xf0))
    {
        /* not a normal AX.25 UI frame, bail out */
        return 0;
    }

    /* Find out if this is a message and to whom it is addressed */

    /* Minimum data-content: ":PE1DNN-12:{1", 13 bytes */
    if(uidata_p->size < 13)
    {
        /* can't be a message */
        return 0;
    }

    /* don't handle too big data packets */
    if(uidata_p->size > 
       (MSG_PREFIX_LENGTH + MAX_MESSAGE_LENGTH + MSG_SUFFIX_LENGTH)
      )
    {
        /* can't be a message */
        return 0;
    }

    /* convert received data to string */
    memcpy(data, uidata_p->data, uidata_p->size);
    data[uidata_p->size] = '\0';

    /* let bp point to the start of the string */
    bp = &data[0];

    /*
     * copy the message_originator, will be overwritten when
     * another call is found as originator of the message.
     */
    strcpy(message_originator, uidata_p->originator);

    /*
     * First check if this is repeated through a tunnel.
     * Third-party messages can be recursive so we have
     * to dig through until we reach the last one before
     * the message. That will be the real message originator.
     */
    while(*bp == '}')
    {
        /* Make the sender of the message the message_originator */

        /* Try to find the end of the call */
        bp = strchr(bp, '>');
        if(bp == NULL)
        {
            /* Original message_originator missing */
            return 0;
        }
        /* Make this a string by patching the '>' character to '\0' */
        *bp = '\0';

        /* check length of the real message_originator */
        if(strlen(&data[1]) >= CALL_LENGTH)
        {
            /* Original message_originator too long */
            return 0;
        }
        
        /*
         * Overwrite the message_originator value with the call
         * recovered from the tunnel data
         */
        strcpy(message_originator, &data[1]); 

        /* advance bp after the '\0' character we put in  */
        bp++;

        /* Move to the end of the 3rd party path */
        bp = strchr(bp, ':');
        if(bp == NULL)
        {
            /* can't be a message */
            return 0;
        }
        /* move to the first char of the data */
        bp++;
    }

    /* prepare a message prefix with the digi-call for comparison */
    /* prepare 2 colons with space for a full call ":PE1DNN-12:"  */
    strcpy(message_prefix, ":         :ack");

    /* patch in the digi-call */
    strncpy(&(message_prefix[1]), digi_digi_call, strlen(digi_digi_call));

    if(strncmp(bp, message_prefix, MSG_PREFIX_LENGTH) == 0)
    {
        /*
         * Match normal format excluding the trailing "ack"
         *
         * PE1DNN>APRS::PA0ABC-12:....
         *             <--------->
         */
        prefix_format = TO_ME_NORMAL;
    }
    else if(strncmp(bp, &message_prefix[1], MSG_PREFIX_LENGTH + 2) == 0)
    {
        /*
         * Match alternative format including the trailing "ack"
         *
         * PE1DNN>APRS:PA0ABC-12:ack....
         *             <----------->
         *
         * This missing colomn format appeared in Pocket APRS. Since the
         * TH-D7 and WinAPRS accept this, DIGI_NED does so too now
         */
        prefix_format = TO_ME_ALTERNATIVE;
    }
    else
    {
        /*
         * No prefix format, not a message or not a message for me
         */
        prefix_format = NOT_TO_ME;
    }

    if(prefix_format != NOT_TO_ME)
    {
        /* This message is addressed to me. */

        /*
         * Check to see if message_originator is on the message blocked
         * list.
         */
        res = is_msg_block(message_originator, uidata_p->port + 1);

        if(res == 1)
        {
            vsay("\r\n===========\r\nMessage Check: ");
            vsay("Originator blocked: ignore message\r\n");
            /* it was a message to addressed to me */
            return 1;
        }

        if(res == -1)
        {
            vsay("\r\n===========\r\nMessage Check: ");
            vsay("I will not react on my own message\r\n");
            /* it was a message to addressed to me */
            return 1;
        }

        if (number_of_hops(uidata_p) > digi_max_msg_hops)
        {
            /* maximum number of hops exceeded */
            vsay("\r\n==========\r\nMessage Check: ");
            vsay("Maximum number of %d hop(s) exceeded.\r\n",
                    digi_max_msg_hops);
            vsay("Message ingnored.\r\n");
            return 1;
        }

        /*
         * move bp passed the message_prefix
         */
        /* 
         * For prefix_format TO_ME_NORMAL the prefix has MSG_PREFIX_LENGTH
         * lenght, the prefix for prefix_format TO_ME_ALTERNATIVE is one
         * byte shorter due to the missing column.
         */
        if(prefix_format == TO_ME_NORMAL)
        {
            /* prefix_format 1 */
            bp = &(bp[MSG_PREFIX_LENGTH]);
        }
        else /* TO_ME_ALTERNATIVE */
        {
            /* prefix_format TO_ME_ALTERNATIVE, only used for acks */
            bp = &(bp[MSG_PREFIX_LENGTH - 1]);
        }

        /*
         * Change the originator of the packet with the real originator
         * of the message (recoverd from the 3rd party header if
         * available). The call will then be stored in the uidata structure
         * and the uidata can now be processed as if the originator came
         * from the AX.25 frame and not from the 3rd party header.
         */
        /*
         * After message processing we have to change it back so remember
         * the orignator of the packet...
         */
        strcpy(packet_originator, uidata_p->originator); 

        /*
         * Now change the orginator to the message_originator
         */
        strcpy(uidata_p->originator, message_originator); 

        parse_message(bp, uidata_p);

        /*
         * Change the originator back to the call of the originator
         * of the packet. This function should not leave a changed
         * packet behind when finished.
         */
        strcpy(uidata_p->originator, packet_originator); 

        /* it was a message to addressed to me */
        return 1;
    }

    /* it was not a message for me */
    return 0;
}

void internal_message(char *command)
{
    uidata_t uidata;
    char *tmp_case;

    /* fill in a fake uidata frame */

    uidata.port = 0;
    uidata.distance = LOCAL;
    uidata.primitive = 0x03; /* UI */
    uidata.pid = 0xFC;
    strcpy(uidata.originator,"KEYBOARD");
    uidata.orig_flags = 0;
    strcpy(uidata.destination,"SCREEN");
    uidata.dest_flags = 0;
    uidata.ndigi = 0;
    strcpy(uidata.data,":KEYBOARD :");
    strcat(uidata.data, command);
    uidata.size = strlen(uidata.data);

    /* internal command */
    internal_command = 1;

    /* convert command to lowercase */
    tmp_case = command;
    while(*tmp_case != '\0')
    {
        *tmp_case = tolower(*tmp_case);
        tmp_case++;
    }

    /* call handler */
    send_autoreply(command, &uidata);

    /* not a internal command */
    internal_command = 0;
}

void auto_message(char *command, uidata_t *uidata_p, short n)
{
    char *tmp_case;

    /*
     * Auto command, carries also the number of times the same message is
     * injected for different ports.
     * 
     * Used to only verbose display in the log the responses for the first
     * port directed back to the caller, the responses for the subsequent
     * ports are the same anyway in most cases.
     */
    auto_command = n;

    /* convert command to lowercase */
    tmp_case = command;
    while(*tmp_case != '\0')
    {
        *tmp_case = tolower(*tmp_case);
        tmp_case++;
    }

    /* call handler */
    send_autoreply(command, uidata_p);

    /* not a auto command */
    auto_command = 0;
}

void init_message(void)
{
    short i;

    for(i = 0; i < MAX_NR_OF_IDS; i++)
    {
        message_id[i] = 0;
    }

    /* for boot-time message */
    boot_time = time(0);

    /* read the message query/answer file */
    read_queries();

    /* not a internal command */
    internal_command = 0;

    /* not a auto command */
    auto_command = 0;

    /* active operation, no shutdown in progress */
    exit_info.state = ACTIVE;

    msg_list_p = NULL;
}
