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

#ifndef _DIGITYPE_H_
#define _DIGITYPE_H_

/*
 * This header-file contains all the type definitions used in DIGI_NED
 *
 * Relies on the DIGICONS.H file with constants
 */

/*
 * Simple types
 */

/* type for timer functions */
typedef unsigned long digi_timer_t;

/*
 * Enumeration types
 */

typedef enum { NONE, ADD, REPLACE, NEW, SWAP, 
               HIJACK, ERASE, KEEP, SHIFT } operation_t;
typedef enum { DIGIPEAT, DIGIEND, DIGITO, DIGISSID,
               DIGIFIRST, DIGINEXT } command_t;
typedef enum { NOACK, ACK, SHUTDOWN } ack_t;
typedef enum { NOSUBSTITUTE, SUBSTITUTE } substitute_t;
typedef enum { ACTIVE, CLOSING_DOWN } operational_state_t;
typedef enum { LOCAL, REMOTE } distance_t;
typedef enum { EXCL_ALLBUT, INCL_ALLBUT } porttype_t;
typedef enum { NOT_KEPT, KEPT } preempt_keep_flag_t;
typedef enum { LAT, LON } latlon_t;
typedef enum { NOTRACK, TRACK } track_t;
typedef enum { INRANGE, ALWAYS } when_t;
typedef enum { SEND, BEACON, WX, AUTOMESSAGE } send_type_t;
typedef enum { VAL, MAX, MIN, SUM, AVG, DHM, HMS,
               YMD, YDM, DMY, MDY, MDH, MDHM } wx_var_kind_t;

/*
 * Structure types
 */

/* struct for storing raw assembled RX and TX packets */
typedef struct frame_s {
    short              len;                     /* length of the frame       */
    char               port;                    /* port number               */
    unsigned char      frame[MAX_FRAME_LENGTH]; /* L1 frame without CRC      */
} frame_t;

/* struct for storing RX and TX disassembled packets */
typedef struct uidata_s {
    short              port;                    /* port of this frame        */
    distance_t         distance;                /* LOCAL or REMOTE RX and TX */
    char               primitive;               /* primitive + P/F bit       */
    unsigned short     pid;                     /* PID or 0xffff if no PID   */
    char               originator[CALL_LENGTH]; /* source call of the frame  */
    char               orig_flags;              /* flags surrounding SSID    */
    char               destination[CALL_LENGTH];/* destination call of frame */
    char               dest_flags;              /* flags surrounding SSID    */
    short              ndigi;                   /* number of digipeaters     */
    char               digipeater[MAX_DIGI][CALL_LENGTH]; /* digi calls      */
    char               digi_flags[MAX_DIGI];    /* flags surrounding SSID    */
    short              size;                    /* size of data in frame     */
    char               data[MAX_DATA_LENGTH];   /* data of the frame         */
} uidata_t;

/* struct for storing digipeat, digiend, digito and digissid rules */
typedef struct digipeat_s {
    struct digipeat_s *next;                    /* pointer to next rule      */
    char               digi[CALL_LENGTH];       /* digi/call name to act on  */
    char               output[OUTPORTS_LENGTH]; /* output ports of this rule */
    operation_t        operation;               /* which operation to apply  */
    short              use;                     /* nr. of digi-calls to use  */
    short              ssid;   /* replacement SSID for digito: and digissid: */
    char              *mapping;                 /* digis to add/substitute   */
} digipeat_t;

/* struct for storing preempt rules */
typedef struct preempt_s {
    struct preempt_s  *next;                    /* pointer to next rule      */
    char               digi[CALL_LENGTH];       /* digi name to act on       */
    char               outd[CALL_LENGTH];       /* digi after preempting     */
} preempt_t;

typedef struct preempt_keep_s {
    struct preempt_keep_s  *next;               /* pointer to next rule      */
    char                    digi[CALL_LENGTH];  /* digi name to keep         */
    preempt_keep_flag_t     keep;               /* mark which digis are kept */
} preempt_keep_t;

/* struct for storing paths for messages, dx etc */
typedef struct path_s {
    struct path_s *next;                        /* pointer to next parh      */
    char                  *path;                /* (dest)+digipeat path      */
} path_t;

/* struct for storing messages for transmission */
typedef struct message_s {
    struct message_s  *next;                    /* pointer to next message   */
    digi_timer_t       timer;                   /* timer to start next try   */
    short              interval;                /* time between tries        */
    short              count;                   /* repeat count if not acked */
    short              port;                    /* port to transmit on       */
    char               id;                      /* message id or -1 for none */
    char               message[MSG_PREFIX_LENGTH + MAX_MESSAGE_LENGTH 
                                + MSG_SUFFIX_LENGTH + 1];
                                                /* message + space for '\0'  */
} message_t;

#ifdef _SATTRACKER_
/* struct for storing satellite objects for transmission */
typedef struct satobject_s {
    struct satobject_s  *next;                  /* pointer to next object    */
    digi_timer_t       timer;                   /* timer to transmit next obj*/
    short              interval;                /* time between objects      */
    short              port;                    /* port to transmit on       */
    short              track;                   /* 0 = no need to be tracked */
                                                /* 1 = needs to be tracked   */
    digi_timer_t       nextaos;                 /* time of next aos          */
    char               sat[5];                  /* which satellite           */
    digi_timer_t       track_stop_time;         /* time to stop tracking     */
} satobject_t;
#endif /* _SATTRACKER_ */

/* struct for storing data for the exit command */
typedef struct exit_info_s {
    operational_state_t state;                  /* flag for closing down     */
    short               id;                     /* id of shutdown message    */
    short               exitcode;               /* exitcode when exiting     */
} exit_info_t;

/* struct for storing send (beacon) messages */
typedef struct send_s {
    struct send_s     *next;                    /* pointer to next beacon    */
    digi_timer_t       timer;                   /* time to go for beacon     */
    short              interval;                /* time between beacons      */
    short              absolute;                /* absolute or relative time */
    send_type_t        kind;                    /* kind of beacon            */
    char               output[OUTPORTS_LENGTH]; /* output ports of this rule */
    char              *path;                    /* digipeat path             */
    char              *info;                    /* file containing text or   */
                                                /* wx format definition      */
} send_t;

#ifdef _TELEMETRY_
/* struct for storing telemetry messages */
typedef struct telemetry_s {
    digi_timer_t       timer;                   /* time to go for telemetry  */
    short              interval;                /* time between telemetry    */
    short              absolute;                /* absolute or relative time */
    char               output[OUTPORTS_LENGTH]; /* output ports of this rule */
    char              *path;                    /* digipeat path             */
    char               source[SOURCE_COUNT][SOURCE_LENGTH];
                                                /* data sources for telemetry*/
    short              address[SOURCE_COUNT];   /* sources addresses for mpx */
    short              count;                   /* number of sources         */
    short              sequence;                /* current sequence number   */
} telemetry_t;
#endif /* _TELEMETRY_ */

#ifdef _WX_
typedef struct wx_var_gather_s {
    short              value;                   /* variable value            */
    long               stamp;                   /* timestamp                 */
} wx_var_gather_t;

/* struct for storing weather variables */
typedef struct wx_var_s {
    struct wx_var_s   *next;
    char               variable;                /* variable character        */
    short              value;                   /* variable value            */
    wx_var_kind_t      kind;                    /* kind of variable          */
    short              period;                  /* measurement period        */
    short              cell;                    /* current cell being filled */
    wx_var_gather_t    data[NR_CELLS];          /* gathered data             */
    double             a;                       /* equarion param 'a'        */
    double             b;                       /* equarion param 'b'        */
    double             c;                       /* equarion param 'c'        */
    char               source[SOURCE_LENGTH];   /* data source for wx var.   */
    short              address;                 /* source addres for mpx     */
} wx_var_t;
#endif /* _WX_ */

#ifdef _SERIAL_
/* struct for storing selection an received serial line sentences */
typedef struct digi_serialtext_s {
    struct digi_serialtext_s *next;
    char                   type[7];             /* type of sentence to store */
    char                   sentence[256];       /* last complete sentence    */
} digi_serialtext_t;

/* struct for storing serial line information */
typedef struct digi_serial_s {
    struct digi_serial_s *next;                 /* pointer to next beacon    */
    digi_timer_t          timer;                /* time to go for beacon     */
    short                 interval;             /* time between beacons      */
    short                 absolute;             /* absolute or relative time */
    char               output[OUTPORTS_LENGTH]; /* output ports of this rule */
    short                 comnr;                /* COM port number           */
    short                 baudrate;             /* speed of serial device    */
    char                 *path;                 /* digipeat path             */
    char                  active_in[256];       /* string being read now     */
    digi_serialtext_t    *serialtext_p;         /* storage of read sentences */
    int                   fd;                   
} digi_serial_t;
#endif /* _SERIAL_ */

/* struct for keeping information about handled data */
typedef struct keep_s {
    struct keep_s     *next;                    /* pointer to next keep data */
    digi_timer_t       keep_timer;              /* time before removal       */
    char               originator[CALL_LENGTH]; /* source call of the frame  */
    unsigned short     crc;                     /* 16 bit CRC of data        */
} keep_t;

/* struct for keeping calls in lists for various purposes */
typedef struct call_s {
    struct call_s     *next;                    /* pointer to next call      */
    char               call[CALL_LENGTH];       /* the call which is kept    */
} call_t;

/* struct for keeping position information */
typedef struct position_s {
    long lat;                                   /* lattitude and longitude   */
    long lon;                                   /* in hundereths of minutes  */
} position_t;

/* struct for keeping heard direct calls*/
typedef struct mheard_s {
    struct mheard_s   *next;                    /* pointer to next block call*/
    char               call[CALL_LENGTH];       /* call which was heard      */
    long               when;                    /* when the call was heard   */
    short              port;                    /* port on which it is heard */
    long               distance;                /* distance from the digi    */
    short              bearing;                 /* bearing from our digi     */
} mheard_t;

/* struct for keeping query and answer texts */
typedef struct query_s {
    struct query_s *next;                       /* pointer to next query     */
    char   query[MAX_MESSAGE_LENGTH];           /* the query                 */
    char   answer[MAX_MESSAGE_LENGTH];          /* the answer                */
} query_t;

/* struct for keeping commands to be initially executed after startup */
typedef struct ini_cmd_s {
    struct ini_cmd_s *next;                     /* pointer to next command   */
    char             *command_p;                /* the command to be exected */
} ini_cmd_t;

/* struct for keeping dx_level definitions for a port */
typedef struct dxlevel_s {
    struct dxlevel_s *next;                     /* pointer to next dxlevel   */
    long              distance_min;             /* min distance level        */
    long              distance_max;             /* max distance level        */
    short             age;                      /* nr of hours to look back  */
} dxlevel_t;

/* struct for keeping portname definitions for a port */
typedef struct portname_s {
    struct portname_s *next;                    /* pointer to next dxlevel   */
    short              port;                    /* port number               */
    char              *portname_p;              /* name of the port          */
} portname_t;

#endif /* _DIGITYPE_H_ */
