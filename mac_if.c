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

#ifndef _LINUX_
/* Implementation for NON Unix type systems */

#include <dos.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include <string.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "read_ini.h"
#include "mac_if.h"
#include "timer.h"
#include "output.h"

#define MAC_CHECK  1
#define MAC_INPUT  2
#define MAC_OUTPUT 3
#define MAC_PORTS  0x0fb
#define MAC_DEBUG  0x0fd

static unsigned char hdr_mac[]  = "??AX25_MAC";

unsigned short mac_vector = 0;

/* AX25_MAC call to check if RX data is available */
static short mac_avl(void)
{
    union REGS inregs;
    union REGS outregs;

    if(mac_vector == 0)
        return 0;

    inregs.h.ah = MAC_CHECK;
    int86(mac_vector, &inregs, &outregs);
    if(outregs.x.ax == 1)
        return 1;
    return 0;
}

/* AX25_MAC call to retrieve RX data */
static short mac_inp(frame_t far *frame_p)
{
    union REGS inregs;
    union REGS outregs;
    struct SREGS sregs;

    if(mac_vector == 0)
        return -1;

    inregs.h.ah = MAC_INPUT;
    sregs.es    = FP_SEG(frame_p);
    inregs.x.di = FP_OFF(frame_p);

    int86x(mac_vector, &inregs, &outregs, &sregs);

    return outregs.h.al;
}

/* AX25_MAC call to put TX data in the MAC layer */
static short mac_out(frame_t far *frame_p)
{
    union REGS inregs;
    union REGS outregs;
    struct SREGS sregs;

    if(mac_vector == 0)
        return 0;

    inregs.h.ah = MAC_OUTPUT;
    sregs.es    = FP_SEG(frame_p);
    inregs.x.di = FP_OFF(frame_p);

    int86x(mac_vector, &inregs, &outregs, &sregs);

    return outregs.h.al;
}

/* AX25_MAC call to retrieve the number of active ports */
short mac_ports(void)
{
    union REGS inregs;
    union REGS outregs;

    if(mac_vector == 0)
        return 0;

    inregs.h.ah = MAC_PORTS;
    int86(mac_vector, &inregs, &outregs);

    return (short) outregs.h.al;
}

/*
 * Initialize access to MAC layer
 *
 * returns: 0 if MAC layer cannot be found
 *          1 if MAC layer is found in memory
 */
short mac_init(void)
{
    unsigned char vector, *p1;
    unsigned char far * p2;
    unsigned char far * handler;
    frame_t             frame;

    for (vector = 0xFF; vector >= 0x40; vector--)
    {
        handler = (unsigned char far *) getvect (vector);

        p1 = hdr_mac;

        for (p2 = handler; *p1 == '?' || *p1 == *p2; p2++)
        {
            if (!*++p1)
            {
                mac_vector = ((short) vector) & 0x00ff;

                while(mac_avl())
                {
                    /* flush pending input */
                    (void) mac_inp(&frame);
                }

                return 1;
            }
        }
    }

    return 0;
}

/*
 * Unload access to MAC layer
 *
 * returns: void
 */
void mac_exit(void)
{
    return;
}

/*
 * Flush pending data in the MAC layer
 *
 * returns: void
 */
void mac_flush(void)
{
    frame_t frame;

    while(mac_avl())
    {
        /* flush pending input */
        (void) mac_inp(&frame);
    }
    return;
}
#else /* _LINUX_ */
/* Implementation for Unix type systems */

/* Both old and new AX.25 tools use AX.25 via the kernel */
#ifdef OLD_AX25
#define KERNEL_AX25
#endif /* OLD_AX25 */
#ifdef NEW_AX25
#define KERNEL_AX25
#endif /* NEW_AX25 */

#ifdef KERNEL_AX25
/* Implementation for Unix type systems using the AX.25 kernel support */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "read_ini.h"
#include "mac_if.h"
#include "timer.h"
#include "output.h"

#include <sys/socket.h>
#include <linux/ax25.h>
#ifndef _NETROSE_ROSE_H /* Avoid problems with SUSE 8.0 */
#include <linux/rose.h>
#endif
#include <linux/if_ether.h>

#ifdef NEW_AX25
#include <netax25/axconfig.h>
#else
#include <ax25/axutils.h>
#include <ax25/axconfig.h>
#endif /* NEW_AX25 */

typedef struct ports_s {
    int    s_out;
    char  *dev_p;
    char  *port_p;
} ports_t;
static ports_t lnx_port[MAX_PORTS];
static int     lnx_port_nr = 0;
static int     lnx_s_in;

/* AX25_MAC call to check if RX data is available */
static short mac_avl(void)
{
    struct timeval tv;
    fd_set fds;
    int retval;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&fds);
    FD_SET(lnx_s_in, &fds);

    /* wait only a short time (not 0, that gives a very high load!) */
    tv.tv_sec = 0;
    tv.tv_usec = 10;

    retval = select(lnx_s_in + 1, &fds, NULL, NULL, &tv);

    if (retval != 0)
    {
        /* data available */
        return 1;
    }

    /* no data */
    return 0;
}

/* AX25_MAC call to retrieve RX data */
static short mac_inp(frame_t *frame_p)
{
    struct sockaddr  from;
    unsigned int     from_len;
    char             data[MAX_FRAME_LENGTH + 1];
    int              data_length;
    int              i;
    ports_t         *prt_p;

    from_len = sizeof(from);
    data_length = recvfrom(lnx_s_in, data, MAX_FRAME_LENGTH + 1, 0,
                                                            &from, &from_len);

    if(data_length < 0) {
        if (errno == EWOULDBLOCK)
            return -1;
        say("Error returned from \"recvfrom\" in function mac_inp()\r\n");
        exit(1);
    }

    if(data_length < 2)
        return -1; /* short packet */

    if(data[0] != 0)
        return -1; /* not a data packet */

    data_length--;

    memcpy(frame_p->frame, &(data[1]), data_length);
    frame_p->len  = data_length;

    /* find out from which port this came */
    for(i = 0; i < lnx_port_nr; i++)
    {
        prt_p      = &(lnx_port[i]);

        if(strcmp(prt_p->dev_p, from.sa_data) == 0)
        {
            /* found it! */
            frame_p->port = i;
            return 0;
        }
    }

    /* data on a port we don't use for DIGI_NED */
    return -1;
}

/* AX25_MAC call to put TX data in the MAC layer */
static short mac_out(frame_t *frame_p)
{
    struct sockaddr  to;
    char             data[MAX_FRAME_LENGTH + 1];
    int              res;
    ports_t         *prt_p;

    prt_p = &(lnx_port[(short) frame_p->port]);

    bzero(&to,sizeof(struct sockaddr));
    to.sa_family = AF_INET;
    strcpy(to.sa_data, prt_p->dev_p);

    data[0] = 0; /* this is data */
    memcpy(&data[1], frame_p->frame, frame_p->len);

    res = sendto(prt_p->s_out, data, frame_p->len + 1, 0, &to, sizeof(to));
    if (res >= 0)
    {
            return 0;
    }
    if (errno == EMSGSIZE) {
            vsay("Sendto: packet (size %d) too long\r\n", frame_p->len + 1);
            return -1;
    }
    if (errno == EWOULDBLOCK) {
            vsay("Sendto: busy\r\n");
            return -1;
    }
    perror("sendto");
    return -1;
}

/* AX25_MAC call to retrieve the number of active ports */
short mac_ports(void)
{
    return lnx_port_nr;
}

/*
 * Initialize access to MAC layer
 *
 * returns: 0 if MAC layer cannot be found
 *          1 if MAC layer is found in memory
 */
short mac_init(void)
{
    struct sockaddr  sa;
    ports_t         *prt_p;
    short            i;

    if(geteuid() != 0)
    {
        display_help_root();
        exit(-1);
    }

    /* general startup, done once */
    if (ax25_config_load_ports() == 0) {
        say("ERROR: problem with axports file\r\n");
        return 0;
    }

    /* convert axport to device and open transmitter sockets */
    for(i = 0; i < lnx_port_nr; i++)
    {
        prt_p = &(lnx_port[i]);

        if ((prt_p->dev_p = ax25_config_get_dev(prt_p->port_p)) == NULL)
        {
            say("ERROR: invalid port name - %s\r\n", prt_p->port_p);
            return 0;
        }

        /* do transmitter side */
        prt_p->s_out = socket(PF_INET, SOCK_PACKET, htons(ETH_P_AX25));
        if (prt_p->s_out < 0)
        {
            perror("tx socket");
            return 0;
        }

        bzero(&sa,sizeof(struct sockaddr));
        strcpy(sa.sa_data, prt_p->dev_p);
        sa.sa_family = AF_INET;
        if (bind(prt_p->s_out, &sa, sizeof(struct sockaddr)) < 0)
        {
            perror("bind");
            close(prt_p->s_out);
            return 0;
        }
    }

    /* open receive socket, one receiver will do for al ports  */
    /*
     * Use ETH_P_AX25 because with PF_AX25 we also will read the data
     * that we transmit ourselfs, that's useless!
     */
    lnx_s_in = socket(PF_INET, SOCK_PACKET, htons(ETH_P_AX25));
    if (lnx_s_in < 0)
    {
        perror("rx socket");
        return 0;
    }

    return 1;
}

/*
 * Unload access to MAC layer
 *
 * returns: void
 */
void mac_exit(void)
{
    return;
}

int add_digi_port(char *port_p)
{
    ports_t         *prt_p;

    if(lnx_port_nr < MAX_PORTS)
    {
        prt_p = &(lnx_port[lnx_port_nr]);

        prt_p->port_p = port_p;

        lnx_port_nr++;

        /* succesfully added */
        return 1;
    }
    else
    {
        /* Can't add more ports */
        return 0;
    }
}

#else /* OLD_AX25 || NEW_AX25 */
#ifdef KISS
/* Implementation for Unix type systems using KISS via a tty device */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "read_ini.h"
#include "mac_if.h"
#include "timer.h"
#include "output.h"
#include "kiss.h"

static int     digi_port_nr = 0;

/* AX25_MAC call to check if RX data is available */
static short mac_avl(void)
{
    short res;

    res = kiss_avl();

    return res;
}

/* AX25_MAC call to retrieve RX data */
static short mac_inp(frame_t *frame_p)
{
    short res;

    res = kiss_inp(frame_p);

    return res;
}

/* AX25_MAC call to TX data */
static short mac_out(frame_t *frame_p)
{
    short res;

    res = kiss_out(frame_p);

    return res;
}

/* AX25_MAC call to retrieve the number of active ports */
short mac_ports(void)
{
    return digi_port_nr;
}

/*
 * Initialize access to MAC layer
 *
 * returns: 0 if MAC layer cannot be found
 *          1 if MAC layer is found in memory
 */
short mac_init(void)
{
    kiss_init();

    return 1;
}

/*
 * Unload access to MAC layer
 *
 * returns: void
 */
void mac_exit(void)
{
    kiss_exit();

    return;
}

int add_digi_port(char *port_p)
{
    digi_port_nr = kiss_param(port_p);

    /* succesfully added */
    if((digi_port_nr >= 1) && (digi_port_nr <= 8))
        return 1;
    else
        return 0;
}
#else /* KISS */
#ifdef AGWPE
/* Implementation for Unix type systems using AGWPE via a TCP/IP network */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "digicons.h"
#include "digitype.h"
#include "genlib.h"
#include "read_ini.h"
#include "mac_if.h"
#include "timer.h"
#include "output.h"
#include "agwpe.h"

/* AX25_MAC call to check if RX data is available */
static short mac_avl(void)
{
    short res;

    res = agwpe_avl();
    
    if(res == 1)
    {
        return 1;
    }
    else
    {
        if(res == 3)
        {
            agwpe_reinit();
        }

        /* returns 0 on any other response value than 1 */
        return 0;
    }
}

/* AX25_MAC call to retrieve RX data */
static short mac_inp(frame_t *frame_p)
{
    short res;

    res = agwpe_inp(frame_p);

    return res;
}

/* AX25_MAC call to TX data */
static short mac_out(frame_t *frame_p)
{
    short res;

    res = agwpe_out(frame_p);

    return res;
}

/* AX25_MAC call to retrieve the number of active ports */
short mac_ports(void)
{
    int res;

    res = agwpe_ports();

    return res;
}

/*
 * Initialize access to MAC layer
 *
 * returns: 0 if MAC layer cannot be found
 *          1 if MAC layer is found in memory
 */
short mac_init(void)
{
    int res;

    res = agwpe_init();

    return res;
}

/*
 * Unload access to MAC layer
 *
 * returns: void
 */
void mac_exit(void)
{
    agwpe_exit();

    return;
}

int add_digi_port(char *port_p)
{
    int res;

    res = agwpe_param(port_p);

    return res;
}
#endif /* AGWPE */
#endif /* KISS */
#endif /* KERNEL_AX25 */
#endif /* _LINUX_ */

/*
 * convert a raw AX.25 frame to a disassembled frame structure
 *
 * returns: 0 if frame could not be decoded
 *          1 if frame could be decoded and has a supported PID
 *         -1 if frame could be decoded but has an unsupported PID
 *         -2 if frame could be decoded but has an illegal call
 */
static short frame2uidata(frame_t *frame_p, uidata_t *uidata_p)
{
    unsigned char  i,j;
    unsigned short k;
    unsigned short ndigi;
    unsigned short ssid;
    unsigned short len;
    unsigned char *bp;
    unsigned short pid;
    unsigned short call_ok;
    unsigned short hops_total;
    unsigned short hops_to_go;
    char          *p;
    
    /* assume all calls are okay */
    call_ok = 1;
    
    uidata_p->port = frame_p->port;
    len = frame_p->len;
    bp  = &(frame_p->frame[0]);

    if (!bp || !len) 
    {
        vsay("Short packet (no data)\r\n");
        return 0;
    }

    if (len < 15)
    {
        vsay("Short packet (< 15 bytes)\r\n");
        return 0;
    }

    if (bp[1] & 1) /* Compressed FlexNet Header */
    {
        vsay("Compressed FlexNet header in packet, not supported\r\n");
        return 0;
    }

    /* cleanup data size before beginning to decode */
    uidata_p->size = 0;

    /* Destination of frame */
    j = 0;
    for(i = 0; i < 6; i++)
    {
        if ((bp[i] &0xfe) != 0x40)
            uidata_p->destination[j++] = bp[i] >> 1;
    }
    ssid = (bp[6] & SSID_MASK) >> 1;
    if(ssid != 0)
    {
        uidata_p->destination[j++] = '-';
        if((ssid / 10) != 0)
        {
            uidata_p->destination[j++] = '1';
        }
        ssid = (ssid % 10);
        uidata_p->destination[j++] = ssid + '0';
    }
    uidata_p->dest_flags = bp[6] & FLAG_MASK;
    uidata_p->destination[j] = '\0';
    if(is_call(uidata_p->destination) == 0)
    {
        vsay("Invalid Destination call in received packet\r\n");
        call_ok = 0;
    }

    bp += 7;
    len -= 7;

    /* Source of frame */
    j = 0;
    for(i = 0; i < 6; i++) 
    {
        if ((bp[i] &0xfe) != 0x40)
            uidata_p->originator[j++] = bp[i] >> 1;
    }
    ssid = (bp[6] & SSID_MASK) >> 1;
    if(ssid != 0)
    {
            uidata_p->originator[j++] = '-';
            if((ssid / 10) != 0)
            {
                uidata_p->originator[j++] = '1';
            }
            ssid = (ssid % 10);
            uidata_p->originator[j++] = ssid + '0';
    }
    uidata_p->orig_flags = bp[6] & FLAG_MASK;
    uidata_p->originator[j] = '\0';
    if(is_call(uidata_p->originator) == 0)
    {
        vsay("Invalid Originator call in received packet\r\n");
        call_ok = 0;
    }

    bp += 7;
    len -= 7;

    /* Digipeaters */
    ndigi=0;
    while ((!(bp[-1] & END_FLAG)) && (len >= 7))
    {
        /* Digi of frame */
        if(ndigi != 8)
        {
            j = 0;
            for(i = 0; i < 6; i++)
            {
                if ((bp[i] &0xfe) != 0x40) 
                    uidata_p->digipeater[ndigi][j++] = bp[i] >> 1;
            }
            if(j == 0)
            {
                vsay("Short Digipeater call found (0 bytes length)\r\n");
                call_ok = 0;
            }
            uidata_p->digi_flags[ndigi] = (bp[6] & FLAG_MASK);
            ssid = (bp[6] & SSID_MASK) >> 1;
            if(ssid != 0)
            {
                    uidata_p->digipeater[ndigi][j++] = '-';
                    if((ssid / 10) != 0)
                    {
                        uidata_p->digipeater[ndigi][j++] = '1';
                    }
                    ssid = (ssid % 10);
                    uidata_p->digipeater[ndigi][j++] = ssid + '0';
            }
            uidata_p->digipeater[ndigi][j] = '\0';
            if(is_call(uidata_p->digipeater[ndigi]) == 0)
            {
                vsay("Invalid Digipeater call in received packet\r\n");
                call_ok = 0;
            }
            ndigi++;
        }
        bp += 7;
        len -= 7;

    }
    uidata_p->ndigi = ndigi;

    /* if at the end, short packet */
    if(!len) 
    {
        vsay("Short packet (no primitive found)\r\n");
        return 0;
    }

    /* We are now at the primitive bit */
    uidata_p->primitive = *bp;
    bp++;
    len--;
    /* Flag with 0xffff that we don't have a PID */
    uidata_p->pid = 0xffff; 

    /* if there were call decoding problems return now */
    if(call_ok == 0)
    {
        return -2;
    }

    /*
     * Determine if a packet is received LOCAL (i.e. direct) or
     * REMOTE (i.e. via a digipeater)
     */
    if(uidata_p->ndigi == 0)
    {
        /* no digipeaters at all, must be LOCAL then */
        uidata_p->distance = LOCAL;
    }
    else
    {
        /* there are digi's, look if it was repeated at least once */
        if((uidata_p->digi_flags[0] & H_FLAG) != 0)
        {
            /* this packet was digipeated and thus not received localy */
            uidata_p->distance = REMOTE;
        }
        else
        {
            /*
             * Here are the exceptions! With these exceptions no H_FLAG
             * is set on the first digipeater although the packet was
             * already digipeated one or more times.
             *
             * These are two types of digipeating use this:
             *
             * 1) Digipeating on destination-SSID. After the first hop
             * the digipeater-call of the first digipeater is added
             * without a H_FLAG. Subsequent digipeaters do not change
             * this until the SSID reaches 0. When the SSID reaches 0
             * the H_FLAG on the existing first digipeater call is set.
             *
             * Example flow, starting with destination SSID=3
             * PE1DNN>APRS-3:>Digipeating...               Direct packet
             * PE1DNN>APRS-2,PE1DNN-2:>Digipeating...      First hop
             * PE1DNN>APRS-1,PE1DNN-2:>Digipeating...      Second hop
             * PE1DNN>APRS,PE1DNN-2*:>Digipeating...       Third hop
             *
             * 2) Intelligent digipeater calls counting down or even
             * finished counting down... The H_FLAG may be set when
             * the call is completely "used", but this is not always
             * the case.
             *
             * Example flow, starting with destination WIDE3-3
             * PE1DNN>APRS,WIDE3-3:>Digipeating...         Direct packet
             * PE1DNN>APRS,WIDE3-2:>Digipeating...         First hop
             * PE1DNN>APRS,WIDE3-1:>Digipeating...         Second hop
             * PE1DNN>APRS,WIDE3*:>Digipeating...          Third hop
             * (if the packet in the third hop passed a Kantronics TNC
             *  with NOID set, then the TNC will not mark WIDE3 with a
             *  H_FLAG,i.e PE1DNN>APRS,WIDE3:>Digipeating...)
             *
             * Exception under the new paradigm:
             * PE1DNN>APRS,WIDE2-1:>Digipeating...         Still 1st hop
             */

            /*
             * Check if the packet is digipeating on SSID. This is the
             * case when the destination SSID is not '0'. We only need
             * to find the '-' on the destination call. With SSID = '0'
             * the '-' is not present.
             */
            p = strchr(uidata_p->destination, '-');
            if(p != NULL)
            {
                /*
                 * Destination SSID is not '0', assume digipeating on
                 * destination SSID. I.e the packet was already repeated
                 * once because there is an unused digipeater call in the
                 * digipeater list. With digipeating on destination-SSID
                 * the starting packet should not have any digipeaters at
                 * all.
                 */
                uidata_p->distance = REMOTE;
            }
            else if(strcmp(uidata_p->digipeater[0], digi_digi_call) == 0)
            {
                    /*
                     * Direct to me, must be LOCAL;
                     */
                    uidata_p->distance = LOCAL;
            }
            else if(strcmp(uidata_p->digipeater[0], "WIDE2-1") == 0)
            {
                    /*
                     * WIDE2-1, must be LOCAL;
                     */
                    uidata_p->distance = LOCAL;
            }
            else
            {
                /*
                 * retrieve hops_total and hops_to_go from the first
                 * digipeater call
                 */

                /* Initialize to no hops to go anymore */
                hops_to_go = 0;

                p = strchr(uidata_p->digipeater[0], '-');
                if(p == NULL)
                {
                    /*
                     * "hops_to_go" was already initialized to zero
                     *
                     * put 'p' at the last digit of the call
                     * We checked already above that the call is at least 1
                     * character in size.
                     */
                    p = uidata_p->digipeater[0];
                    p = &(p[strlen(p) - 1]);
                }
                else
                {
                    /*
                     * Retrieve hops_to_go from the SSID
                     *
                     * p[0] points to the '-'
                     * p[1] points to the first SSID character
                     * p[2] points to '\0' or the second SSID character
                     *      if there is a second character the first
                     *      SSID character is always '1'.
                     *      (SSID can be 1..15; when 0 it is no shown)
                     */
                    if((p[1] >= '0') && (p[1] <= '9'))
                    {
                        /* Keep this value */
                        hops_to_go = (short)(p[1] - '0');

                        /*
                         * Since the value was not '\0' it is save to
                         * check the next character. This is only needed
                         * if the first character is '1'
                         */
                        if(p[1] == '1')
                        {
                            if((p[2] >= '0') && (p[2] <= '9'))
                            {
                                /*
                                 * The first SSID character meant 10, the
                                 * number of hops_to_go is 10 plus the
                                 * second SSID character
                                 */
                                hops_to_go = 10 + (short)(p[2] - '0');
                            }
                        }
                    }

                    /*
                     * put 'p' at the last digit of the call
                     * We checked already  above that the call is at least 1
                     * character in size.
                     */
                    p--;
                }

                /*
                 * Retrieve hops_total, if any. Initialize to 15 before
                 * start so intelligent calls without a total hopcount
                 * are regarded as not being the first unless the SSID=15.
                 *
                 * If the digipeater does not have a SSID then initialize to
                 * zero.
                 */
                if (hops_to_go == 0)
                {
                    hops_total = 0;
                }
                else
                {
                    hops_total = 15;
                }

                if((*p >= '0') && (*p <= '9'))
                {
                    /* Keep this value */
                    hops_total = (short)(*p - '0');

                    /*
                     * Check last but one character if the last digit
                     * was smaller than 6. Check if the pointer doesn't
                     * move before the start of the call.
                     */
                    p--;
                    if( (uidata_p->digipeater[0] <= p)
                        &&
                        (hops_total < 6)
                      )
                    {
                        if(*p == '1')
                        {
                            /*
                             * Take this '1' into account, the total hop
                             * count is 10 more.
                             */
                            hops_total = hops_total + 10;
                        }
                    }
                }

                /*
                 * We collected hops_total and hops_to_go.
                 *
                 * Check if it is an "Intelligent" digi-call which is in
                 * progress counting down or is finished.
                 */
                if(hops_total > hops_to_go)
                {
                    /*
                     * The hops_total is higher than hops_to_go, this is
                     * assumed to be an "Intelligent" call busy counting
                     * down.
                     */
                    uidata_p->distance = REMOTE;
                }
                else
                {
                    /*
                     * The hops_to_go count has a higher or equal number 
                     * than hops_total. This is the case when it is an
                     * "Intelligent" call that did not start count-down
                     * yet or when the SSID is higher than the last digit(s)
                     * of the call (no "Intelligent" digipeating used).
                     *
                     * In these cases the packet must be direct.
                     */
                    uidata_p->distance = LOCAL;
                }
            }
        }
    }

    /* No data left, ready */
    if (!len)
    {
        /* this is not an UI frame, in kenwood_mode it should be ignored */
        if(digi_kenwood_mode == 0)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }

    /* keep the PID */
    pid = ((short) *bp) & 0x00ff; 
    uidata_p->pid = pid;
    bp++;
    len--;

    k = 0;
    while (len)
    {
        i = *bp++;

        if(k < 256) uidata_p->data[k++] = i;

        len--;
    }
    uidata_p->size = k;

    if(call_ok == 0)
    {
        return -2;
    }

    if(digi_kenwood_mode == 0)
    {
        /* check PID */
        if( (pid != 0xf0) /* normal AX.25 */
            &&
            (pid != 0xcc) /* IP datagram */
            &&
            (pid != 0xcd) /* ARP */
            &&
            (pid != 0xcf) /* NETROM */
            &&
            ((digi_opentrac_enable == 0) || (pid != 0x77)) /* Opentrac */
          )
        {
            /*
             * only support for AX.25, IP, ARP, NETROM and Opentrac
             * packets, bail out on others
             */
            return -1;
        }
    }
    else
    {
        /* only support for AX.25 UI frames */
        if((uidata_p->primitive & ~0x10) != 0x03)
        {
            /* not a normal AX.25 UI frame, bail out */
            return -1;
        }
        else
        {
            /*
             * AX.25 UI frame, check for PID=F0 and
             * PID=77 if Opentrac is enabled
             */

            /* check PID */
            if( (pid != 0xf0) /* normal AX.25 */
                &&
                ((digi_opentrac_enable == 0) || (pid != 0x77)) /* Opentrac */
              )
            {
                /*
                 * only support for AX.25, IP, ARP, NETROM and Opentrac
                 * packets, bail out
                 */
                return -1;
            }
        }
    }

    return 1;
}

/*
 * convert a disassembled frame structure to a raw AX.25 frame 
 */
static void uidata2frame(uidata_t *uidata_p, frame_t *frame_p)
{
    unsigned char  i,j;
    unsigned short k;
    unsigned short ssid;
    unsigned short len;
    unsigned char *bp;

    frame_p->port = uidata_p->port;

    bp = &(frame_p->frame[0]);

    len = 0; /* begin of frame */

    /* Destination of frame */
    j = 0;
    for(i = 0; i < 6; i++)
    {
        /* if not end of string or at SSID mark */
        if((uidata_p->destination[j] != '\0')
           &&
           (uidata_p->destination[j] != '-'))
        {
            bp[i] = (uidata_p->destination[j++] << 1);
        }
        else
        {
            bp[i] = 0x40;
        }
    }
    if(uidata_p->destination[j] == '-')
    {
        /* "j" points to the SSID mark */
        j++;
        
        ssid = uidata_p->destination[j++] - '0';
        if(uidata_p->destination[j] != '\0')
        {
            ssid = (10 * ssid) + (uidata_p->destination[j++] - '0');
        }
    }
    else
    {
        ssid = 0;
    }
    bp[6] = (ssid << 1) | uidata_p->dest_flags;
    bp += 7;
    len += 7;

    /* Source of frame */
    j = 0;
    for(i = 0; i < 6; i++)
    {
        /* if not end of string or at SSID mark */
        if((uidata_p->originator[j] != '\0')
           &&
           (uidata_p->originator[j] != '-'))
        {
            bp[i] = (uidata_p->originator[j++] << 1);
        }
        else
        {
            bp[i] = 0x40;
        }
    }
    if(uidata_p->originator[j] == '-')
    {
        /* "j" points to the SSID mark */
        j++;

        ssid = uidata_p->originator[j++] - '0';
        if(uidata_p->originator[j] != '\0')
        {
            ssid = (10 * ssid) + (uidata_p->originator[j++] - '0');
        }
    }
    else
    {
        ssid = 0;
    }
    bp[6] = (ssid << 1) | uidata_p->orig_flags;
    bp += 7;
    len += 7;

    /* Digipeaters */
    for(k = 0; k < uidata_p->ndigi; k++)
    {
        /* check if the distribution distance should be LOCAL */
        if( uidata_p->distance == LOCAL )
        {
            /*
             * to keep the frame local avoid that it is digipeated after
             * we transmitted it, there shall not be any unused digipeater
             * in the frame. break out of the loop if an unused digipeater
             * is encountered
             *
             * an unused digipeater doesn't have its H_BIT set in the SSID
             */
            if((uidata_p->digi_flags[k] & H_FLAG) == 0)
            {
                /* don't put "unused" digipeaters in the frame */
                break; /* break out of the for loop */
            }
        }

        j = 0;
        for(i = 0; i < 6; i++)
        {
            /* if not end of string or at SSID mark */
            if((uidata_p->digipeater[k][j] != '\0')
               &&
               (uidata_p->digipeater[k][j] != '-'))
            {
                bp[i] = (uidata_p->digipeater[k][j++] << 1);
            }
            else
            {
                bp[i] = 0x40;
            }
        }
        if(uidata_p->digipeater[k][j] == '-')
        {
            /* "j" points to the SSID mark */
            j++;

            ssid = uidata_p->digipeater[k][j++] - '0';
            if(uidata_p->digipeater[k][j] != '\0')
            {
                ssid = (10 * ssid) + (uidata_p->digipeater[k][j++] - '0');
            }
        }
        else
        {
            ssid = 0;
        }
        bp[6] = (ssid << 1) | uidata_p->digi_flags[k];
        bp += 7;
        len += 7;
    }

    /* patch bit 1 on the last address */
    bp[-1] = bp[-1] | END_FLAG;

    /* We are now at the primitive bit */
    *bp = uidata_p->primitive;
    bp++;
    len++;

    /* if PID == 0xffff we don't have a PID */
    if(uidata_p->pid == 0xffff)
    {
        frame_p->len = len;
        return;
    }

    /* set the PID */
    *bp = (char) uidata_p->pid;
    bp++;
    len++;

    /* when size == 0 don't copy */
    if(uidata_p->size == 0)
    {
        frame_p->len = len;
        return;
    }

    memcpy(bp, &(uidata_p->data[0]), uidata_p->size);
    len += uidata_p->size;

    frame_p->len = len;

    return;
}

/*
 * Check the uidata packet for overrun of a Kenwood tranceiver
 *
 * Tested for the TH-D7E with version 2.0 prom. Hopefully do
 * the other Kenwood tranceivers have the same limit (if any)
 *
 * Returns:
 *          0 - do not transmit this packet
 *          1 - okay to transmit this packet
 */
short uidata_kenwood_mode(uidata_t *uidata_p)
{
    short budget;
    short i;

    if((digi_kenwood_mode != 1) && (digi_kenwood_mode != 2))
    {
        return 1; /* no special handling at all */
    }

    if((digi_opentrac_enable == 1) && (uidata_p->pid == 0x77))
    {
        return 1; /* no truncation of Opentrack packets */
    }

    /* 
     * The packet is assumed to be internally formatted like this:
     *
     *  SOURCE>ID,PATH,PATH*,PATH:Hey look at me, this is my long ID<CR>
     *  <--------------------- Max 195 characters --------------------->
     */

    /* start budget is 195 bytes, this is including the <CR> */
    budget = 195;
    
    /* start to count down */
    budget = budget - strlen(uidata_p->originator);

    budget--; /* for the '>' sign */

    budget = budget - strlen(uidata_p->destination);

    if(uidata_p->ndigi > 0)
    {
        for(i = 0; i < uidata_p->ndigi; i++)
        {
            budget--; /* for the ',' separator */
            budget = budget - strlen(uidata_p->digipeater[i]);
        }

        /*
         * if the first digi indicates 'digipeated' then there must be
         * a '*' somewhere...
         */
        if((uidata_p->digi_flags[0] & H_FLAG) != 0)
        {
            budget--; /* for the '*' marking somewhere on one of the digis */
        }
    }

    budget--; /* for the ':' header terminator */

    /* see if we have any room left for the payload (we should) */
    if(budget <= 0)
    {
        /* no data to transmit, header took all the space */
        return 0;
    }

    /* look if the packet is too big */
    if(uidata_p->size > budget)
    {
        if(digi_kenwood_mode == 2)
        {
            /* do not transmit too long packets*/
            vsay("Kenwood mode: packet too long, not transmitted\r\n");
            return 0;
        }
        else
        {
            /* truncate the packet to maximum allowable size */
            uidata_p->size = budget;
            vsay("Kenwood mode: packet too long, truncated to %d bytes\r\n",
                    budget);
        }
    }

    /* okay to transmit */
    return 1;
}

/*
 * get disassembled AX.25 from MAC layer
 *
 * returns: 0 if no data is received
 *          1 data received and put in the uidata_p structure
 *          2 data received with unknown PID, data put in the uidata_p struct
 *          3 corrupt data frame received, data in uidata_p not complete
 *         -1 on error (conversion check for debugging)
 */
short get_uidata(uidata_t *uidata_p)
{
    frame_t frame;
#ifdef debug
    frame_t check_frame;
#endif
    short   result;

    while (mac_avl ())
    {
        if(mac_inp(&frame) == 0)
        {
#ifdef debug
            dump_raw(&frame);
#endif

            result = frame2uidata(&frame, uidata_p);

            /* Write to TNC log (if needed) */
            tnc_log(uidata_p, REMOTE); 

            if(result == 1)
            {
                /* frame decoded correctly */
#ifdef debug
                uidata2frame(uidata_p,&check_frame);
                if(frame.port != check_frame.port)
                {
                    vsay("Check: port error, org %d, check %d\r\n",
                            (short) frame.port + 1, (short) check_frame.port + 1);
                    dump_raw(&frame);
                    dump_raw(&check_frame);
                    dump_uidata_from(uidata_p);
                    if(frame2uidata(&check_frame, uidata_p) != 0)
                    {
                        dump_uidata_from(uidata_p);
                    }
                    return -1;
                }
                if(frame.len != check_frame.len)
                {
                    vsay("Check: len error, org %d, check %d\r\n",
                             frame.len, check_frame.len);
                    dump_raw(&frame);
                    dump_raw(&check_frame);
                    dump_uidata_from(uidata_p);
                    if(frame2uidata(&check_frame, uidata_p) != 0)
                    {
                        dump_uidata_from(uidata_p);
                    }
                    return -1;
                }
                if(memcmp(&(frame.frame[0]), &(check_frame.frame[0]),
                        frame.len) != 0)
                {
                    vsay("Check: data error\r\n");
                    dump_raw(&frame);
                    dump_raw(&check_frame);
                    dump_uidata_from(uidata_p);
                    if(frame2uidata(&check_frame, uidata_p) != 0)
                    {
                        dump_uidata_from(uidata_p);
                    }
                    return -1;
                }
#endif
                return 1;
            }
            else if(result == -1)
            {
                /* frame decoded but unknown PID */
                dump_uidata_from(uidata_p);

                if(digi_kenwood_mode == 0)
                {
                    /*
                     * only support for AX.25, IP, ARP, NETROM and
                     * Opentrac packets
                     */
                    if(digi_opentrac_enable == 1)
                    {
                        vsay("Not an AX.25, IP, ARP, NETROM or Opentrac packet"
                            " (PID=%X), not handled\r\n", uidata_p->pid);
                    }
                    else
                    {
                        vsay("Not an AX.25, IP, ARP or NETROM packet"
                            " (PID=%X), not handled\r\n", uidata_p->pid);
                    }
                }
                else
                {
                    if(digi_opentrac_enable == 1)
                    {
                        vsay("Kenwood mode: only normal AX.25 UI and "
                            "Opentrac packets\r\n");
                    }
                    else
                    {
                        vsay("Kenwood mode: only normal AX.25 UI packets\r\n");
                    }
                }

                return 2;
            }
            else if(result == -2)
            {
                /* frame decoded but errors in call */
                dump_uidata_from(uidata_p);

                /* error in frame decoding */
                return 3;
            }
            else
            {
                /* error in frame decoding */
                return 3;
            }
        }
    }
    return 0;
}

/*
 * send disassembled AX.25 frame to MAC layer
 *
 * returns: nothing (best effort service)
 */
void put_uidata(uidata_t *uidata_p)
{
    frame_t frame;

    if(((digi_ptt_data >> uidata_p->port) & 0x01) == 0x00)
    {
        vsay("PTT: transmission for port %d is disabled; "
             "frame transmission ignored\r\n", uidata_p->port + 1);
        return;
    }

    /* Write to TNC log (if needed) */
    tnc_log(uidata_p, uidata_p->distance); 

    /* convert to an AX.25 frame */
    uidata2frame(uidata_p, &frame);

#ifdef debug
    dump_raw(&frame);
#endif

    /* transmit this frame */
    (void) mac_out(&frame);

    return;
}

