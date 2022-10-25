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
 *  Based on code from 18 OCT 89 - RAC - Original code.
 *  Modified by PE1DNN
 */
#ifdef _SERIAL_
/*****************************************************************************
 *                                 ibmcom.c                                  *
 *****************************************************************************
 * DESCRIPTION: This file contains a set of routines for doing low-level     *
 *              serial communications on the IBM PC.  It was translated      *
 *              directly from Wayne Conrad's IBMCOM.PAS version 3.1, with    *
 *              the goal of near-perfect functional correspondence between   *
 *              the Pascal and C versions.                                   *
 *                                                                           *
 * REVISIONS:   18 OCT 89 - RAC - Original translation from IBMCOM.PAS, with *
 *                                liberal plagiarism of comments from the    *
 *                                Pascal.                                    *
 *****************************************************************************/

#include        <stdio.h>
#include        <unistd.h>
#include        <fcntl.h>
#include        <termios.h>
#include        "ibmcom.h"

int uart0_filestream = -1;

int com_install(int portnum) {

    if (com_installed)                          /* Drivers already installed */
        return 3;
    if ((portnum < 0) || (portnum > 0))  /* Port number out of bounds */
        return 1;

    uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart0_filestream == -1)
	{
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
	}
	
    struct termios options;
    tcgetattr(uart0_filestream, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;		//<Set baud rate
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(uart0_filestream, TCIFLUSH);
    tcsetattr(uart0_filestream, TCSANOW, &options);
    return 0;                                   /* Successful installation */
    }                                           /* End com_install() */

void com_deinstall(void) {

    if (uart0_filestream != -1) 
    {
	close(uart0_filestream);
    }
}                                           /* End com_deinstall() */
void com_set_speed(unsigned speed) {

}                                           /* End com_set_speed() */

void com_set_parity(enum par_code parity, int stop_bits) {
    }                                           /* End com_set_parity() */
void com_raise_dtr(void) {
}

int     com_rx(void) {
    int bytes_read;
    int bytes_read = 0;
    if (uart0_filestream != -1)
    {
       byte_length = read(uart0_filestream, byte_read, 1); 
    }
    if (byte_length == 0) {
	return -1;
    } else {
	return byte_read;
   }
}                                           /* End com_tx() */

#endif /* _SERIAL_ */
