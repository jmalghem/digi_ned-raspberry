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
#ifdef AGWPE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>

#include <stdio.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>

#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "digicons.h"
#include "digitype.h"
#include "timer.h"
#include "output.h"
#include "agwpe.h"

/* Only uncomment the next line for debugging the AGWPE interface */
/* #define AGWPE_DEBUG */

/*
 * This file contains all code to read and write from a AGWPE port
 */

/*
 * AGWPE type, we can only have one (multiport) agwpe connection
 */

/* struct for storing agwpe information */
typedef struct digi_agwpe_s {
    char          *hostname;                    /* hostname or TCP/IP addr.  */
    short          ipport;                      /* TCP/IP port number        */
    short          port;                        /* Radio port number         */
    int            maxports;                    /* total nr of ports         */
    char           header[AGWPE_HEADER_SZ];     /* AGWPE header for reception*/
    long           to_receive;                  /* Nr of bytes to receive    */
    char           frame[400];                  /* frame beiing received     */
    short          index;                       /* length received so far    */
    short          length;                      /* length of complete frame  */
    int            fd;                          /* filedescriptor            */
    digi_timer_t   conn_timer;                  /* connection check timer    */
    digi_timer_t   rx_timer;                    /* reception check timer     */
} digi_agwpe_t;

typedef enum { AGWPE_INIT, AGWPE_REINIT } agwpe_open_t;

typedef enum { AGWPE_DOWN, AGWPE_UP } agwpe_state_t;

#define AGWPE_AVL_IDLE  0
#define AGWPE_AVL_DATA  1
#define AGWPE_AVL_PORT  2
#define AGWPE_AVL_ERROR 3
#define AGWPE_AVL_REG   4

#define AGWPE_CONN_TIMER (30 * TM_SEC)
#define AGWPE_RX_TIMER (60 * TM_SEC)

/*
 * variable containing the 'agwpe' port information
 */
static digi_agwpe_t agwpe_info = {
    NULL, /* *hostname  */
    8000, /* ipport     */
    0,    /* port       */
    0,    /* maxports   */
    "\0", /* header[]   */
    0,    /* te_recieve */
    "\0", /* frame[]    */
    0,    /* index      */
    0,    /* lenght     */
    0,    /* fd         */
    0,    /* conn_timer */ 
    0     /* rx_timer   */
};

/*
 * variable containing the state of the connection to AGWPE
 */
static agwpe_state_t agwpe_state = AGWPE_DOWN;

/* local function prototypes */
static short agwpe_local_avl(void);

/*
 * Open the AGWPE network connection and start reception of raw packets
 *
 * When open_type == AGWPE_INIT then give verbose messages, otherwise
 * open silent.
 *
 * "agwpe_state" will be AGWPE_UP on succes, AGWPE_DOWN if the
 * opening procedure fails.
 */
static void agwpe_open(agwpe_open_t open_type) 
{
    struct sockaddr_in sockaddr;
    struct hostent *hostinfo;
    short  ports;
    short  i;
    int    optval = 1;
    char   header[AGWPE_HEADER_SZ];
    char  *p;
#ifdef AGWLOGON
    char   logon[AGWPE_LOGON_SZ];
#endif
    short  retval;

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: agwpe_open called\r\n");
#endif

    /* If AGWPE is already up, we are finished... */
    if(agwpe_state == AGWPE_UP)
    {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: AGWPE interface is already up, nothing to do...\r\n");
#endif
        return;
    }

    /* Cleanup port administration */
    agwpe_info.port       = 0;
    agwpe_info.to_receive = 0;
    agwpe_info.index      = 0;
    agwpe_info.length     = 0;

    /* go find information about the desired host machine */
    if ((hostinfo = gethostbyname(agwpe_info.hostname)) == 0) {
        if(open_type == AGWPE_INIT)
        {
            perror("gethostbyname");
            say("Host lookup for '%s' failed, is the hostname correct?\r\n"
                 ,agwpe_info.hostname);
        }
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: gethostbyname successful\r\n");
#endif

    /* fill in the socket structure with host information */
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = ((struct in_addr *)(hostinfo->h_addr))->s_addr;
    sockaddr.sin_port = htons(agwpe_info.ipport);

    /* grab an Internet domain socket */
    if ((agwpe_info.fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        if(open_type == AGWPE_INIT)
        {
            perror("socket");
            say("Opening of network socket failed.\r\n");
        }
        return;
    }

    /* Turn on the socket keepalive option */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: opening socket successful, fd = %d\r\n", agwpe_info.fd);
#endif

    optval = 1;

    if(setsockopt (agwpe_info.fd, SOL_SOCKET, SO_KEEPALIVE,
                   &optval, sizeof(optval)) == -1)
    {
        if(open_type == AGWPE_INIT)
        {
            perror("setsockopt");
            say("Changing socket option to enable KEEPALIVE failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

    /* Disable the Nagle algorithm (speeds things up) */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: setsockopt for keepalive successful\r\n");
#endif

    optval = 1;

    if(setsockopt (agwpe_info.fd, IPPROTO_TCP, TCP_NODELAY,
                   &optval, sizeof(optval)) == -1)
    {
        if(open_type == AGWPE_INIT)
        {
            perror("setsockopt");
            say("Changing socket option to enable TCP_NODELAY failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: setsockopt for disableing Nagle algorithm successful\r\n");
#endif

    /* connect to PORT on HOST */
    if (connect(agwpe_info.fd,(struct sockaddr *) 
                &sockaddr, sizeof(sockaddr)) == -1) 
    {
        if(open_type == AGWPE_INIT)
        {
            perror("connect");
            say("Connection to host '%s', port %d failed.\r\n",
                        agwpe_info.hostname, agwpe_info.ipport);
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: connect to port %d on host '%s' successful\r\n", agwpe_info.ipport, agwpe_info.hostname);
#endif

    /* make the socket non-blocking */
    if (fcntl(agwpe_info.fd, F_SETFL, O_NONBLOCK) == -1) {
        if(open_type == AGWPE_INIT)
        {
            perror("fcntl");
            say("Switching network socket to non blocking failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: switched connection to non-blocking\r\n");
#endif

#ifdef AGWLOGON
    /* Connected to to AGWPE, logon */
    memset(&logon, 0, AGWPE_LOGON_SZ);

    logon[AGWPE_KIND] = 'P';  /* frame type 'P'           */
    logon[AGWPE_DATALEN] = (char) ((255 + 255) % 0x00ff);
    logon[AGWPE_DATALEN + 1] = (char) (((255 + 255) >>8 ) % 0x00ff);
    strcpy(&(logon[AGWPE_LOGON_NAME]), "name");
    strcpy(&(logon[AGWPE_LOGON_PWD]), "password");
        
    if(write(agwpe_info.fd, logon, AGWPE_LOGON_SZ) != AGWPE_LOGON_SZ)
    {
        if(open_type == AGWPE_INIT)
        {
            perror("write");
            say("Writing to AGWPE connection failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }
#endif

    /* Connected to to AGWPE, get port information */
    memset(&header, 0, AGWPE_HEADER_SZ);

    header[AGWPE_KIND] = 'G';  /* frame type 'G', port info */

    if(write(agwpe_info.fd, header, AGWPE_HEADER_SZ) != AGWPE_HEADER_SZ)
    {
        if(open_type == AGWPE_INIT)
        {
            perror("write");
            say("Writing to AGWPE connection failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: start connection-check timer and reception timer\r\n");
#endif

    /*
     * Start connection check timer en reception timeout timer
     */
    start_timer(&agwpe_info.conn_timer, AGWPE_CONN_TIMER);
    start_timer(&agwpe_info.rx_timer, AGWPE_RX_TIMER);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: writing port info request successful\r\n");
    say("AGWPE-DEBUG: agwpe_open: waiting for response for port info\r\n");
#endif

    /*
     * Await response from G command
     */
    while((retval = agwpe_local_avl()) == AGWPE_AVL_IDLE);
    if(retval != AGWPE_AVL_PORT)
    {
        /* Not received what we exected, quit... */
        if(open_type == AGWPE_INIT)
        {
            say("Channel info reception failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: response for port info received\r\n");
#endif

    p = strtok(agwpe_info.frame, ";");
    if(p == NULL)
    {
        if(open_type == AGWPE_INIT)
        {
            say("Nr of ports not found in AGWPE channel info\r\n");    
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }
    ports = atoi(p);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: received number of ports\r\n");
#endif

    if(ports > MAX_PORTS)
    {
        if(open_type == AGWPE_INIT)
        {
            say("AGWPE has %d ports, DIGI_NED will use the first 8 of them\r\n"
                   , ports);
        }
        ports = 8;
    }

    if(open_type == AGWPE_INIT)
    {
        /* first time, set the number of ports */
        agwpe_info.maxports = ports;

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: INIT, setting number of ports\r\n");
#endif
    }
    else if(agwpe_info.maxports != ports)
    {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: REOPEN, checking; number of ports different\r\n");
#endif
        /*
         * If the number of ports changed then quit!
         *
         * The digi_ned.ini rule file has to re-read with the new number of
         * ports.
         */

        say("The number of AGWPE ports changed after reinitialisation!\r\n");
        say("Previous number of ports: %d\r\n", agwpe_info.maxports);
        say("New number of ports     : %d\r\n", ports);
        say("Please restart DIGI_NED for the changes to take effect.\r\n");

        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);

        /* send a SIGTERM signal to ourself */
        kill(getpid(), SIGTERM);
        
        return;
    }
#ifdef AGWPE_DEBUG
    else
    {
        say("AGWPE-DEBUG: agwpe_open: REOPEN, checking; number of ports ok\r\n");
    }
#endif

    if(open_type == AGWPE_INIT)
    {
#ifdef AGWPE_DEBUG
        say("AGWPE-DEBUG: agwpe_open: INIT, show port information\r\n");
#endif
        for(i = 0; i< agwpe_info.maxports; i++)
        {
            p = strtok(NULL, ";");
            if(p != NULL)
            {
                vsay("%s\r\n", p);
            }
            else
            {
                vsay("Port%d <no information>\r\n", i + 1);
            }
        }
    }
#ifdef AGWPE_DEBUG
    else
    {
        say("AGWPE-DEBUG: agwpe_open: REINIT, do not show port information\r\n");
    }
#endif

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: unregister call te be sure\r\n");
#endif

    /* Connected to to AGWPE, to be sure unregister call */
    memset(&header, 0, AGWPE_HEADER_SZ);

    header[AGWPE_KIND] = 'x';  /* frame type 'x', unregister call */

    memcpy(&header[AGWPE_FROMCALL], "DIGI_NED", 8);

    /* Best effort, don't await result */
    if(write(agwpe_info.fd, header, AGWPE_HEADER_SZ) != AGWPE_HEADER_SZ)
    { ; }

    /* Allow de-registration to arrive */
    sleep(1);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: register call\r\n");
#endif

    /* Connected to to AGWPE, register call */
    memset(&header, 0, AGWPE_HEADER_SZ);

    header[AGWPE_KIND] = 'X';  /* frame type 'X', register call */

    memcpy(&header[AGWPE_FROMCALL], "DIGI_NED", 8);

    if(write(agwpe_info.fd, header, AGWPE_HEADER_SZ) != AGWPE_HEADER_SZ)
    {
        if(open_type == AGWPE_INIT)
        {
            perror("write");
            say("Writing call registration to AGWPE connection failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: start connection-check timer and reception timer\r\n");
#endif

    /*
     * Start connection check timer en reception timeout timer
     */
    start_timer(&agwpe_info.conn_timer, AGWPE_CONN_TIMER);
    start_timer(&agwpe_info.rx_timer, AGWPE_RX_TIMER);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: writing register call successful\r\n");
    say("AGWPE-DEBUG: agwpe_open: waiting for response register call\r\n");
#endif

    /*
     * Await response from X command
     */
    while((retval = agwpe_local_avl()) == AGWPE_AVL_IDLE);
    if(retval != AGWPE_AVL_REG)
    {
        /* Not received what we exected, quit... */
        if(open_type == AGWPE_INIT)
        {
            say("Can't register call with AGWPE, continuing without.\r\n");
        }
    }

#ifdef AGWPE_DEBUG
    else
    {
        say("AGWPE-DEBUG: agwpe_open: response for register call received\r\n");
    }
#endif

    /* start raw data reception */
    memset(&header, 0, AGWPE_HEADER_SZ);

    header[AGWPE_KIND] = 'k';  /* frame type 'k': request raw data */

    if(write(agwpe_info.fd, header, AGWPE_HEADER_SZ) != AGWPE_HEADER_SZ)
    {
        if(open_type == AGWPE_INIT)
        {
            perror("write");
            say("Writing to AGWPE connection failed.\r\n");
        }
        shutdown(agwpe_info.fd, 2);
        close(agwpe_info.fd);
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: wrote request for RAW data reception\r\n");
#endif

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_open: start connection-check timer and reception timer\r\n");
#endif

    /*
     * Start connection check timer en reception timeout timer
     */
    start_timer(&agwpe_info.conn_timer, AGWPE_CONN_TIMER);
    start_timer(&agwpe_info.rx_timer, AGWPE_RX_TIMER);

    /* We reached the end, AGWPE is up now */
    agwpe_state = AGWPE_UP;

    return;
}

/*
 * Gently close an agwpe connection
 */
static void agwpe_close(void)
{
    char   header[AGWPE_HEADER_SZ];

    /* if AGWPE was up, unregister and close the connection */
    if(agwpe_state == AGWPE_DOWN)
    {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_close: agwpe connection was already down\r\n");
#endif
        return;
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_close: terminating agwpe connection\r\n");
#endif

    /* Connected to to AGWPE, unregister call */
    memset(&header, 0, AGWPE_HEADER_SZ);

    header[AGWPE_KIND] = 'x';  /* frame type 'x', unregister call */

    memcpy(&header[AGWPE_FROMCALL], "DIGI_NED", 8);

    /* Best effort, don't await result */
    if(write(agwpe_info.fd, header, AGWPE_HEADER_SZ) != AGWPE_HEADER_SZ)
    { ; }

    /* Allow de-registration to arrive */
    sleep(1);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_close: unregister request done\r\n");
#endif

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_close: shutdown socket\r\n");
#endif

    /* Shutdown the old socket */
    shutdown(agwpe_info.fd, 2);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_close: close old agwpe socket\r\n");
#endif
    /* Close the old socket */
    close(agwpe_info.fd);

    /* AGWPE is down now */
    agwpe_state = AGWPE_DOWN;
}

/*
 * Restore the agwpe interface to a safe setting
 */
void agwpe_exit(void)
{
    /* Well, this is simple... */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_exit: leaving agwpe interface\r\n");
#endif

    /* if AGWPE was up, close the interface */
    agwpe_close();
}

/*
 * Initialize the AGWPE network connection
 */
short agwpe_init(void) 
{
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_init: initialize agwpe connection\r\n");
#endif

    agwpe_open(AGWPE_INIT); 

    if(agwpe_state == AGWPE_DOWN)
    {
        /* failed ot open connection to AGWPE */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_init: initialization of agwpe connection failed\r\n");
#endif

        return 0;
    }
    else
    {
        /* opening connection to AGWPE succesfull */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_init: initialization of agwpe connection successful\r\n");
#endif

        return 1;
    }
}

/*
 * Reinitialize the AGWPE network connection
 *
 * Keeps on trying until the connection is up again
 */
void agwpe_reinit(void) 
{
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_reinit: re-initialize agwpe connection\r\n");
#endif

    /* if AGWPE was up, close the connection and give connection lost message */
    if(agwpe_state == AGWPE_UP)
    {
#ifdef AGWPE_DEBUG
        say("AGWPE-DEBUG: agwpe_reinit: close old agwpe connection\r\n");
#endif
        vsay("Connection lost or not responding, re-establishing "
             "AGWPE network connection\r\n");

        /* Close the old connection */
        agwpe_close();
    }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_reinit: initialize agwpe connection again\r\n");
#endif

    agwpe_open(AGWPE_REINIT); 

    if(agwpe_state == AGWPE_UP)
    {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_reinit: initialization of agwpe connection successfull\r\n");
#endif
        vsay("Re-initialization of AGWPE network connection succes\r\n");

        display_separator();
    }
}

/*
 * Checks the availability of a AGWPE frame. For this it reads data from
 * AGWPE until there is no data anymore or until a complete data frame
 * has been read.
 *
 * Returns AGWPE_AVL_RAW   when a complete 'K' data-frame has been read
 * Returns AGWPE_AVL_PORT  when a complete 'G' data-frame has been read
 * Returns AGWPE_AVL_REG   when a complete 'X' data-frame with successfull
 *                         reg. has been read
 * Returns AGWPE_AVL_ERROR when the connection is lost or when the interface
 *                         seem to be not responding or out of sync.
 * Rerurns AGWPE_AVL_IDLE  otherwise (no or incomplete data reception)
 */
short agwpe_avl(void)
{
    short result;

    /* if AGWPE is down return connection loss once every 40 calls */
    if(agwpe_state == AGWPE_DOWN)
    {
        static short counter = 0;

        /* Slow down looping while the AGWPE is down */
        /* 25 ms delay */
        usleep(25000);

        /*
         * Only return loss once every 40 calls, this will keep digi_ned
         * more responsive to keyboard input since only ones per second an
         * attempt is made to reconnect
         */
        counter++;
        if(counter >= 40)
        {
            /* reset counter */
            counter = 0;

            /* result "connection lost" */
            result = AGWPE_AVL_ERROR;
        }
        else
        {
            /* just make result, "no data received" */
            result = AGWPE_AVL_IDLE;
        }
    }
    else /* agwpe_status == AGWPE_UP */
    {
        result = agwpe_local_avl();
    }

    return result;
}

/*
 * internal "agwpe_avl" procedure that doesn't look if the interface is up,
 * used by the agwpe_open procedure (that's the reason why it is not up, we
 * are busy trying to get it up...)
 */
static short agwpe_local_avl(void)
{
    long  received;
    long  to_receive;
    char  buffer[AGWPE_RX_BUFFER];
    char  header[AGWPE_HEADER_SZ];
    char *p;

    if(agwpe_info.to_receive == 0)
    {
        if(read(agwpe_info.fd, agwpe_info.header, AGWPE_HEADER_SZ)
                                               == AGWPE_HEADER_SZ)
        {
            agwpe_info.port = (int) agwpe_info.header[AGWPE_PORT];

            /* Retrieve length from the AGWPE header */
            p = &(agwpe_info.header[AGWPE_DATALEN]);

            /* Read little endian formatted long-int value */
            to_receive = (((short) p[3]) & 0x00ff);
            to_receive = to_receive << 8;
            to_receive = to_receive + (((short) p[2]) & 0x00ff);
            to_receive = to_receive << 8;
            to_receive = to_receive + (((short) p[1]) & 0x00ff);
            to_receive = to_receive << 8;
            to_receive = to_receive + (((short) p[0]) & 0x00ff);

            agwpe_info.to_receive = to_receive;

            if( (agwpe_info.header[AGWPE_KIND] != 'K')
                &&
                (agwpe_info.header[AGWPE_KIND] != 'G')
                &&
                (agwpe_info.header[AGWPE_KIND] != 'X')
              )
            {
                /*
                 * Unexpected frame type, assume out of sync
                 * Report connection loss so the connection is re-initialized
                 */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Unknown frametype %c received\r\n", agwpe_info.header[AGWPE_KIND]);
    say("AGWPE-DEBUG: agwpe_local_avl: Assume the connection is out of sync (reinitialize)\r\n");
#endif

                /* Drop frame, set number of bytes to receive to 0 */
                agwpe_info.to_receive = 0;

                return AGWPE_AVL_ERROR;
            }

            if((to_receive > 2048) || (to_receive < 0))
            {
                /*
                 * This looks insane, I don't trust it!
                 * Report connection loss so the connection is re-initialized
                 */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: about to receive %d bytes!\r\n", to_receive);
    say("AGWPE-DEBUG: agwpe_local_avl: this can't be right, assume out of sync (reinitialize)\r\n");
#endif

                /* Drop frame, set number of bytes to receive to 0 */
                agwpe_info.to_receive = 0;

                return AGWPE_AVL_ERROR;
            }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: correct header received, restart timers\r\n");
#endif
            /*
             * Start connection check timer en reception timeout timer
             */
            start_timer(&agwpe_info.conn_timer, AGWPE_CONN_TIMER);
            start_timer(&agwpe_info.rx_timer, AGWPE_RX_TIMER);
        }
        else
        {
            switch (errno) {

            case EAGAIN:
                /* Normal situation when there is no data */
                if(is_elapsed(&agwpe_info.rx_timer))
                {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: reception timer ran out\r\n");
#endif

                    /*
                     * We expected to receive something by now,
                     * AGWPE doesn't seem to answer anymor.
                     *
                     * Report connection loss, this will rebuild the
                     * connection
                     */
                    return AGWPE_AVL_ERROR;
                }
                else if(is_elapsed(&agwpe_info.conn_timer))
                {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: connection check timer ran out\r\n");
    say("AGWPE-DEBUG: agwpe_local_avl: send Port Info request so AGWPE to check reception of data\r\n");
#endif

                    /*
                     * Send 'G'request to AGWPE so AGWPE has to respond and
                     * DIGI_NED knows the connection is still okay in both
                     * directions.
                     */
                    memset(&header, 0, AGWPE_HEADER_SZ);

                    /* frame type 'G': request raw data */
                    header[AGWPE_KIND] = 'G';

                    if(write(agwpe_info.fd, header, AGWPE_HEADER_SZ)
                        != AGWPE_HEADER_SZ)
                    {
#ifdef AGWPE_DEBUG
   perror("write");
   say("AGWPE-DEBUG: agwpe_local_avl: writing 'G' command to AGWPE connection failed, reinitialize.\r\n");
#endif

                        /*
                         * Report connection loss, this will rebuild the
                         * connection
                         */
                        return AGWPE_AVL_ERROR;
                    }

#ifdef AGWPE_DEBUG
   say("AGWPE-DEBUG: agwpe_local_avl: writing 'G' command done, wait and restart connection-check timer...\r\n");
#endif

                    /*
                     * Start connection check timer
                     */
                    start_timer(&agwpe_info.conn_timer, AGWPE_CONN_TIMER);

                    /* No header received, wait for next */
                    return AGWPE_AVL_IDLE;
                }
                else
                {
                    /* We are finished for now */
                    return AGWPE_AVL_IDLE;
                }

            case ECONNRESET:
            case ECONNABORTED:
            default:
                /* Connection lost to peer */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: connection reset or aborted\r\n");
    say("AGWPE-DEBUG: agwpe_local_avl: reinitialization is needed\r\n");
#endif

                /* Report connection loss */
                return AGWPE_AVL_ERROR;
                break;
            }

            /* No header received, wait for next */
            return AGWPE_AVL_IDLE;
        }
    }

    while(agwpe_info.to_receive != 0)
    {
        /* How many bytes of data do we expect */
        to_receive = agwpe_info.to_receive;

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: need to get %d bytes to get the packet\r\n", to_receive);
#endif

        /* I there is more data than our receive buffer allows the downsize */
        if(to_receive > AGWPE_RX_BUFFER)
        {
            to_receive = AGWPE_RX_BUFFER;
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: bytecount to big for read buffer, using multiple reads\r\n");
    say("AGWPE-DEBUG: agwpe_local_avl: receiving %d byte chunk\r\n", to_receive);
#endif
        }

        received = read(agwpe_info.fd, buffer, to_receive);

        if(received > 0)
        {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: received %d bytes\r\n", received);
#endif
            agwpe_info.to_receive = agwpe_info.to_receive - received;

            if(agwpe_info.index < 400)
            {
                if(received > (long)(400 - agwpe_info.index))
                {
                    memcpy(&(agwpe_info.frame[agwpe_info.index]),
                                buffer, 400 - agwpe_info.index);
                    agwpe_info.index = 400;
                }
                else
                {
                    memcpy(&(agwpe_info.frame[agwpe_info.index]),
                                buffer, received);
                    agwpe_info.index = agwpe_info.index + (short) received;
                }
            }

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: data was received ok, restart timers\r\n");
#endif
            /*
             * Start connection check timer en reception timeout timer
             */
            start_timer(&agwpe_info.conn_timer, AGWPE_CONN_TIMER);
            start_timer(&agwpe_info.rx_timer, AGWPE_RX_TIMER);
        }
        else
        {
            /* No more data from read */
            switch (errno) {
            case EAGAIN:
                /* Normal situation when there is no data */
                if(is_elapsed(&agwpe_info.rx_timer))
                {
                    /*
                     * We are in the middle of a frame reception and the data
                     * just stopped. There cannot be 60 seconds between the
                     * header and the rest. Terminate the link and reconnect.
                     */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: reception timer ran out, rebuild connection\r\n");
#endif

                    /* Drop frame, set number of bytes to receive to 0 */
                    agwpe_info.to_receive = 0;

                    /*
                     * Report connection loss, this will rebuild the
                     * connection
                     */
                    return AGWPE_AVL_ERROR;
                }
                else
                {
                    /* We are finished for now */
                    return AGWPE_AVL_IDLE;
                }
                break;

            case ECONNRESET:
            case ECONNABORTED:
            default:
                /* Connection lost to peer */

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: connection reset or aborted\r\n");
    say("AGWPE-DEBUG: agwpe_local_avl: reinitialization is needed\r\n");
#endif
                /* Drop frame, set number of bytes to receive to 0 */
                agwpe_info.to_receive = 0;

                /* Report connection loss */
                return AGWPE_AVL_ERROR;
                break;
            }

            /* We are finished for now */
            return AGWPE_AVL_IDLE;
        }
    }

    agwpe_info.length = agwpe_info.index;
    agwpe_info.index = 0;

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: AGWPE packet complete!\r\n");
#endif

    if(agwpe_info.header[AGWPE_KIND] == 'K')
    {
        /* frame type 'K': raw data */

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Got AGWPE raw data packet\r\n");
#endif
        /* complete frame without errors */
        if( (agwpe_info.length != 0)
            &&
            ((agwpe_info.frame[0] & 0x0f) == 0)
          )
        {
            /* dataframe */
            return AGWPE_AVL_DATA;
        }
    }
    else if(agwpe_info.header[AGWPE_KIND] == 'G')
    {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Got AGWPE channel info packet (connection check)\r\n");
#endif
        /* frame type 'G': channel info */
        return AGWPE_AVL_PORT;
    }
    else if(agwpe_info.header[AGWPE_KIND] == 'X')
    {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Got AGWPE call registration packet\r\n");
#endif
        /* frame type 'X': call registration */
        if( agwpe_info.length != 1 )
        {
            /* Registration packet length should be 1 */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Registration packet lenght != 1 (%d)\r\n", agwpe_info.length);
#endif
            return AGWPE_AVL_ERROR;
        }
        else if (agwpe_info.frame[0] == 1)
        {
            /* Registration success */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Registration success\r\n");
#endif
            return AGWPE_AVL_REG;
        }
        else
        {
            /* Registration failed */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Registration failed\r\n");
#endif
            return AGWPE_AVL_ERROR;
        }
    }
    else
    {
        vsay("Received unexpected AGWPE datatype %c\r\n",
                agwpe_info.header[AGWPE_KIND]);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_local_avl: Unknown frametype %c received\r\n", agwpe_info.header[AGWPE_KIND]);
    say("AGWPE-DEBUG: agwpe_local_avl: Assume the connection is out of sync (reinitialize)\r\n");
#endif

        /* Unexpected, reinitialize connection */
        return AGWPE_AVL_ERROR;
    }

    return AGWPE_AVL_IDLE;
}

/*
 * Retrieve the received AGWPE frame. Agwpe_local_avl collected the frame,
 * here we only need to return the data that is waiting
 *
 * Returns 0 on succes, -1 on failure
 */
short agwpe_inp(frame_t *frame_p)
{
    /* complete frame without errors */
    if(agwpe_info.port >= agwpe_info.maxports)
    {
        /* port out of range*/
        return -1;
    }

    frame_p->port = agwpe_info.port;

    frame_p->len = agwpe_info.length - 1;
    memcpy(frame_p->frame, &(agwpe_info.frame[1]), agwpe_info.length - 1);

    return 0;
}

/*
 * Send a frame as raw frame to AGWPE. It is assumed a network connection
 * is available and up. This is a best-effort delivery. There is no
 * check for network errors, recovery of network failures is done on the
 * receiver side.
 *
 * Returns 0 on succes, -1 on failure
 */
short agwpe_out(frame_t *frame_p)
{
    /* send a data to AGWPE via the existing network socket */
    char  header[AGWPE_HEADER_SZ];
    long  to_transmit;
    char *packet;
    long  packet_size;
 
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_out: sending data\r\n");
#endif

    if(agwpe_state == AGWPE_DOWN)
    {
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_out: AGWPE is down, data transmission skipped\r\n");
#endif
        return -1;
    }

    /*
     * Build the complete packet to send
     *
     * Size is header + type-byte + AX.25 frame size
     */
    packet_size = AGWPE_HEADER_SZ + 1 + frame_p->len;

    /* allocate space for the whole packet */
    packet = malloc(packet_size);
    if(packet == NULL)
    {
        /* Out of memory, transmission skipped */
        return -1;
    }

    /* build up the header */
    memset(&header, 0, AGWPE_HEADER_SZ);

    header[AGWPE_PORT] = (char) frame_p->port;

    header[AGWPE_KIND] = 'K';

    to_transmit = frame_p->len + 1; /* one more for the 0x00 type byte */

    /* Store length into the AGWPE header */

    /* Store as little endian long-int */
    header[AGWPE_DATALEN + 0] = (char) (to_transmit & 0x00ff);
    to_transmit = to_transmit >> 8;
    header[AGWPE_DATALEN + 1] = (char) (to_transmit & 0x00ff);
    to_transmit = to_transmit >> 8;
    header[AGWPE_DATALEN + 2] = (char) (to_transmit & 0x00ff);
    to_transmit = to_transmit >> 8;
    header[AGWPE_DATALEN + 3] = (char) (to_transmit & 0x00ff);

    /* Copy header to the packet */
    memcpy(packet, header, AGWPE_HEADER_SZ);

    /* Put type byte in the packet */
    packet[AGWPE_HEADER_SZ] = (char) 0x00;

    /* Copy AX.25 frame to the packet */
    memcpy(&(packet[AGWPE_HEADER_SZ + 1]), &(frame_p->frame[0]), frame_p->len);

#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_out: send %ld byte AGWPE packet to AGWPE\r\n",
            packet_size);
#endif

    /* Send the packet */
    if(write(agwpe_info.fd, packet, packet_size) != packet_size)
    {
        /* write failed */
#ifdef AGWPE_DEBUG
    say("AGWPE-DEBUG: agwpe_out: sending AGPPE packet to AGWPE failed\r\n");
#endif
        /* free allocated memory */
        free(packet);

        return -1;
    }
     
    /* free allocated memory */
    free(packet);

    return 0;
}

/*
 * Return the number of AGWPE ports, received from the AGWPE port info
 */
short agwpe_ports(void)
{
    return agwpe_info.maxports;
}

short agwpe_param(char *param)
{
    char *p;

    p = strtok(param, ":");
    if(p == NULL)
    {
        say("-p parameter error, expected hostname or TCP/IP address not found.\r\n");    
        return 0;
    }
    agwpe_info.hostname = strdup(p);

    p = strtok(NULL, ":");
    if(p == NULL)
    {
        say("-p parameter error, expected TCP/IP portnumber not found.\r\n");    
        return 0;
    }
    agwpe_info.ipport = atoi(p);

    return 1;
}
#endif /* AGWPE */
