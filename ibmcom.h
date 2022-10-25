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

#ifndef _IBMCOM_H_
#define _IBMCOM_H_
/*****************************************************************************
 *                                 ibmcom.h                                  *
 *****************************************************************************
 * DESCRIPTION: ANSI C function prototypes and other definitions for the     *
 *              routines in ibmcom.c                                         *
 *                                                                           *
 * REVISIONS:   18 OCT 89 - RAC - Original code.                             *
 *****************************************************************************/

enum par_code   { COM_NONE, COM_EVEN, COM_ODD, COM_ZERO, COM_ONE };

#int             com_carrier(void);
void            com_deinstall(void);
#void            com_flush_rx(void);
#void            com_flush_tx(void);
int             com_install(int portnum);
#void interrupt  com_interrupt_driver();
#void            com_lower_dtr(void);
void            com_raise_dtr(void);
int             com_rx(void);
#int             com_rx_empty(void);
void            com_set_parity(enum par_code parity, int stop_bits);
void            com_set_speed(unsigned speed);
#void            com_tx(char c);
#int             com_tx_empty(void);
#int             com_tx_ready(void);
#void            com_tx_string(char *s);

#endif /* _IBMCOM_H_ */

#endif /* _SERIAL_ */
