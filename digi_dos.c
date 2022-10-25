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

#include <string.h>
#include "digicons.h"
#include "digitype.h"
#include "output.h"
#include "digi_dos.h"
#include "dos.h"
#include "ibmcom.h"

#ifdef _TELEMETRY_
/*
 * Parameter as the verified format "lpt1", "lpt2" or "lpt3", or
 * for the 8 bit multiplexed input "lpt1_8", "lpt2_8" or "lpt3_8".
 * The function does not check this but assumes it is okay!.
 *
 * The second parameter is used in case the lpt port is multiplexed.
 * The port number (which is 0 based and runs from 0 to 5) is put on
 * bits 1, 2 and 3 of the control port. These bits correspond with the
 * "Auto Linefeed", "Initialize Printer" and "Select Printer" outputs
 * of the LPT port.
 *
 * Function returns the LPT status or -1 if the status could not be
 * read.
 */
short get_lpt_status(char *lpt, short port)
{
    short far* lpt_bios;
    short      status;
    short      lptio;
    int        arg;

    /* switch on lpt number */
    /*
     * Port numbers are taken from the BIOS area
     */
    switch(lpt[3]) {
    case '1':
            lpt_bios = MK_FP(0x040, 0x08);
            break;
    case '2':
            lpt_bios = MK_FP(0x040, 0x0A);
            break;
    case '3':
            lpt_bios = MK_FP(0x040, 0x0C);
            break;
    default:
            /* Wrong lpt name */
            return -1;
    }
    lptio = *lpt_bios;

    /*
     * if the BIOS did not detect an LPT on this port then the command
     * fails.
     */
    if(lptio == 0)
    {
        /* Not installed */
        return -1;
    }

    /* shift port number and keep the 3 port-number bits */
    port = (port << 1) & 0x0e;

    /*
     * Invert the "Auto Linefeed" and "Printer Select" bits, these
     * are inverted back by hardware.
     */
    port = port ^ 0x0a;

    /*
     * Make sure the port is not bi-directional (needed for some
     * systems, for example the HP OmniBook 600C
     */
    /*
     * write control port with the port number and resetting
     * directonal bits and strobe.
     */
    outportb(lptio + 2, port);

    /*
     * delay for 4 ms to give the hardware time to put the nibble
     * on the LPT port
     */
    delay(4);

    /*
     * Read status register, one above the base port
     */
    if(lpt[4] == '\0')
    {
        arg = inportb(lptio + 1);

        /* move read character to a short */
        status = (short) arg;

        /* one bit was reversed by the port hardware, flip it back */
        status = status ^ 0x080;

        /* make shure the top byte is 0 and the 3 lower bits are also 0 */
        status = status & 0x00f8;
    }
    else
    {
        /* set strobe bit and port number */
        outportb(lptio + 2, port + 0x01);

        /*
         * delay for 4 ms to give the hardware time to put the nibble
         * on the LPT port
         */
        delay(4);

        /* read lower nibble */
        arg = (inportb(lptio + 1)) &  0x00f0;

        /* make this the lower nibble */
        status = arg >> 4;

        /* reset strobe bit, keep port number */
        outportb(lptio + 2, port);

        /*
         * delay for 4 ms to give the hardware time to put the nibble
         * on the LPT port
         */
        delay(4);

        /* read higher nibble */
        arg = (inportb(lptio + 1)) &  0x00f0;

        /* add high nibble */
        status = status | arg;

        /*
         * one bit was reversed by the port hardware (in low and high
         * nibble), flip it back
         */
        status = status ^ 0x088;
    }

    /* return the status */
    return (status & 0x00ff);
}
#endif /* _TELEMETRY_ */

#ifdef _OUTPORT_
/*
 * Parameter as the verified format "lpt1", "lpt2" or "lpt3", or for the
 * 8 bit multiplexed input "lpt1_8", "lpt2_8" or "lpt3_8". The function
 * does not check this but assumes it is okay!.
 *
 * The data is a string with "0", "1" and pulse and don't care flags, low-bit
 * first. Also the data is already checked before calling this function.
 *
 * Function returns the LPT data or -1 if the data could not be changed.
 */
short out_lpt_data(char *lpt, char *change_data)
{
    char  data[9];
    short far* lpt_bios;
    short lptio;
    short i;
    int   num;
    int   res;
    int   loops;

    /* switch on lpt number */
    /*
     * Port numbers are taken from the BIOS area
     */
    switch(lpt[3]) {
    case '1':
            lpt_bios = MK_FP(0x040, 0x08);
            break;
    case '2':
            lpt_bios = MK_FP(0x040, 0x0A);
            break;
    case '3':
            lpt_bios = MK_FP(0x040, 0x0C);
            break;
    default:
            /* Wrong lpt name */
            return -1;
    }
    lptio = *lpt_bios;

    /*
     * if the BIOS did not detect an LPT on this port then the command
     * fails.
     */
    if(lptio == 0)
    {
        /* Not installed */
        return -1;
    }

    /*
     * Make sure the port is not bi-directional (needed for some
     * systems, for example the HP OmniBook 600C
     */
    /* write control port, resetting all bits */
    outportb(lptio + 2, 0);

    /*
     * delay for 4 ms to give the hardware time to stabilize the data
     * on the LPT port
     */
    delay(4);

    /*
     * Read data register, this is on the base port
     */
    num = inportb(lptio);

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

    if(strchr(change_data, 'p') == NULL)
    {
        loops = 1;
    }
    else
    {
        loops = 2;
    }

    while(loops > 0)
    {
        /*
         * Take over the not don't care values from change_data into data
         */
        for(i = 0; i < 8; i++)
        {
            if(change_data[i] == '\0')
            {
                /* no more changes, we can stop this loop */
                break;
            }

            if((change_data[i] == '0') || (change_data[i] == '1'))
            {
                /* this bit should be changed */
                data[i] = change_data[i];
            }

            if(change_data[i] == 'p')
            {
                /*
                 * This bit should be flipped (the second time it is flipped
                 * back
                 */
                if(data[i] == '1')
                {
                    data[i] = '0';
                }
                else
                {
                    data[i] = '1';
                }
            }
        }

        /* Convert data to number, argument is D0..D7 */
        num = 0;
        for(i = 7; i >= 0; i--)
        {
            num = num << 1;

            if(data[i] == '1')
            {
                num = num + 1;
            }
        }

        /* Set the data on the LPT port */
        outportb(lptio, num);

        /* engage in next loop if needed */
        loops--;
        if(loops > 0)
        {
            /* toggle waits 1 second */
            sleep(1);
        }
    }

    /*
     * delay for 4 ms to give the hardware time to stabilize the data
     * on the LPT port
     */
    delay(4);

    /* Read the data back from the LPT port */
    res = inportb(lptio);

    /* Check if the data was succesfully changed */
    if(res == num)
    {
        /* return port value for success */
        return ((short) res) & 0x00ff;
    }
    else
    {
        return -1;
    }
}
#endif /* _OUTPORT_ */

#ifdef _SERIAL_
/*
 * Restore the tty to the old state we had before initialization
 */
#pragma argsused
void serial_close(int fd)
{
    com_deinstall();
}

/*
 * Open the serial line to poll for data
 *
 * Return -1 if it fails.
 */
int serial_open(short comnr, short baudrate) 
{
    int status;

    /* Install COM port */
    status = com_install(comnr);

    if(status != 0)
    {
        say("com_install() error: %d\r\n", status);
        say("Note: In dos only 1 serial port can be specified\r\n");
        return -1;
    }

    com_set_speed(baudrate);
    com_set_parity(COM_NONE, 1);
    com_raise_dtr();
    
    return comnr;
}

/*
 * Read a character from the serial line
 */
#pragma argsused
int serial_in(int fd, char *buf_p)
{
    int i;

    i = com_rx();

    if(i != -1)
    {
        /* Character read */
        *buf_p = (char) i;
        return 1;
    }
    else
    {
        /* No data */
        return -1;
    }
}
#endif /* _SERIAL_ */
