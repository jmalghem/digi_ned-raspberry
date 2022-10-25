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

#ifndef _DIGICONS_H_
#define _DIGICONS_H_

/*
 * This header-file contains all the constant definitions used in DIGI_NED
 */

/* AX.25 definition constants */
#define MAX_DIGI             8     /* Maximum number of DIGI's in a frame  */ 
#define MAX_PORTS            8     /* Maximum number of PORTS in AX25_MAC  */ 
#define CALL_LENGTH         10     /* Max. length of a call: "PE1DNN-12\0" */
#define MAX_DATA_LENGTH    256     /* Maximum data lenght in a frame       */
#define MAX_FRAME_LENGTH   400     /* Maximum length of a raw frame        */

/* specific values for flags */
#define END_FLAG  (0x01)
#define C_FLAG    (0x80)
#define H_FLAG    (0x80)
#define R1_FLAG   (0x20)
#define R2_FLAG   (0x40)
#define RR_FLAG   (R1_FLAG | R2_FLAG)
#define FLAG_MASK (0xE0)
#define SSID_MASK (0x1E)

/* constants from the specification */
#define MAX_MESSAGE_LENGTH 128     /* Max length of a message              */ 
#define MSG_PREFIX_LENGTH   11     /* Length of ":PE1DNN-12:" without '\0' */ 
#define MSG_SUFFIX_LENGTH    6     /* Length of "{12345\r" without '\0'    */ 
#define MAX_LINE_LENGTH    256     /* Maximum line lenght in .ini file etc */

/* DIGI_NED constatants */
#define MAX_PORTS            8     /* Maximum number of ports supported    */ 
#define OUTPORTS_LENGTH     16     /* Fits "0,1,2,3,4,5,6,7\0"             */
#define SOURCE_COUNT         6     /* Max 6 data sources in telemetry      */
#define SOURCE_LENGTH        7     /* Fits "lpt0_8\0", to be extended later*/
#define MAX_NR_OF_IDS      100     /* Number of message ID's               */ 
#define APRS_BC_SIZE         6     /* Size of an ?APRS? broadcast          */ 
#define APRS_BC_STRING "?aprs?"    /* ?APRS? broadcast string              */ 
#define APRS_WX_SIZE         4     /* Size of an ?WX? broadcast            */ 
#define APRS_WX_STRING "?wx?"      /* ?WX? broadcast string                */ 
#define LINUX_PORTS    "./digi_lnx.ini" /* file with ports for LINUX       */ 
#define MAX_DX_TIMES        80     /* Maximum length of dx_times definition*/
#define MAX_DX_DISTANCE  32767L    /* Maximum distance default value       */
#define MAX_DX_DIST_STR "32767"    /* Max distance default value string    */
#define NR_CELLS           100     /* Nr of cells with data in WX vars     */

/* LIBC constants */
#define MAX_ITOA_LENGTH     18     /* itoa returns maximal 17 characters   */

/* constants using 1 second timer-ticks */
#define TM_SEC               1
#define TM_MIN              60
#define TM_HOUR           3600

#endif /* _DIGICONS_H_ */
