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
#ifdef KISS
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include "digicons.h"
#include "digitype.h"
#include "output.h"
#include "kiss.h"

/*
 * This file contains all code to read and write from a KISS port
 */

/*
 * Kiss type, we can only have one (multiport) kiss device
 */

/* struct for storing kiss information */
typedef struct digi_kiss_s {
    char                 *dev;                  /* tty device                */
    short                 baudrate;             /* speed of kiss device      */
    int                   maxports;             /* total nr of ports         */
    char                  frame[400];           /* frame beiing received     */
    short                 index;                /* length received so far    */
    short                 length;               /* length of complete frame  */
    char                  esc;                  /* remember escape           */
    char                  error;                /* remember frame error      */
    int                   fd;                   /* filedescriptor            */
} digi_kiss_t;

/*
 * variable containing the 'kiss' command data
 */
static digi_kiss_t kiss_info = { NULL, 9600, 0, "\0", 0, 0, 0, 0, 0 };

/*
 * Restore the tty to the old state we had before initialization
 */
static void kiss_close(int fd)
{
    struct termios t;

    if(fd != -1)
    {
        /* Get the attributes of the kiss line for modification */
        if (tcgetattr(fd, &t) != 0)
        {
            /* Can't get settings, close and quit without restoring */
            close(fd);
            return;
        }

        /* Set the terminal back in canonical mode with signal support*/
        t.c_lflag     = ISIG | ICANON;

        /* flush the old data */
        (void) tcflush(fd, TCIFLUSH);

        /* Set the new attributes for the kiss line */
        (void) tcsetattr(fd, TCSANOW, &t);

        close(fd);
    }
}

/*
 * Open the kiss line to poll for data
 *
 * Return -1 if it fails.
 */
static int kiss_open(char *dev, short baudrate) 
{
    struct termios t;
    int    fd;

    fd = open(dev, O_RDWR|O_NOCTTY|O_NONBLOCK);

    /* Open our kiss line */
    if (fd == -1)
    {
        /* Unable to open, ignore */
        return -1;
    }

    /* Check if fd is connected to a tty, stdin has file descriptor 0 */
    if(isatty(fd) == 0)
    {
        /* Not a kiss line, ignore */
        close(fd);
        return -1;
    }

    /* Get the attributes of the kiss line for modification */
    if (tcgetattr(fd, &t) != 0)
    {
        /* Can't get settings, not initialized */
        close(fd);
        return -1;
    }

    /* Set the terminal settings to raw access */
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 2;
    t.c_iflag     = 0;
    t.c_lflag     = 0;
    t.c_cflag     = CS8 | HUPCL|CLOCAL|CREAD;

    /* Change the baudrate settings */
    switch(baudrate) {
    case 1200:  cfsetispeed(&t, B1200);
                cfsetospeed(&t, B1200);
                break;
    case 2400:  cfsetispeed(&t, B2400);
                cfsetospeed(&t, B2400);
                break;
    case 4800:  cfsetispeed(&t, B4800);
                cfsetospeed(&t, B4800);
                break;
    case 9600:  cfsetispeed(&t, B9600);
                cfsetospeed(&t, B9600);
                break;
    case 19200: cfsetispeed(&t, B19200);
                cfsetospeed(&t, B19200);
                break;
    default:    /* Illegal baudrate on kiss, ignore */
                break;
    }

    /* flush the old data */
    if (tcflush(fd, TCIFLUSH) == -1)
    {
        /* Can't flush old data, kiss line not initialized */
        kiss_close(fd);
        return -1;
    }

    /* Set the new attributes for the kiss line */
    if (tcsetattr(fd, TCSANOW, &t) == -1)
    {
        /* Can't change settings, kiss line not initialized */
        kiss_close(fd);
        return -1;
    }

    return fd;
}

/*
 * Read a character from the kiss line
 */
static int kiss_in(int fd, char *buf_p)
{
    int i;

    i = read(fd, buf_p, 1);

    return i;
}

/*
 * Restore the kiss lines to a safe setting
 */
void kiss_exit(void)
{
    kiss_close(kiss_info.fd);
}

/*
 * Initialize the kiss line to poll for data
 *
 * If is fails just silently ignore.
 */
void kiss_init() 
{
    kiss_info.fd = kiss_open(kiss_info.dev, kiss_info.baudrate); 

    kiss_info.length = 0;
    kiss_info.esc = 0;
    kiss_info.error = 0;

    return;
}

/*
 * Checks the availability of a KISS frame. For this it reads data from the
 * kiss line until there is no data anymore or until a complete data frame
 * has been read.
 *
 * Returns 1 when a complete data-frame has been read, returns 0 otherwise
 */
short kiss_avl()
{
    int              i;
    char             data;

    if(kiss_info.fd == -1)
    {
        /*
         * Not initialized, do nothing
         */
        return 0;
    }

    /* Loop until all data from the kiss line is read */
    while(1)
    {
        i = kiss_in(kiss_info.fd, &data);

        if(i == -1)
        {
            /* No data read (anymore) */
            return 0;
        }

        /* Frame END (and also begin of next frame) */
        if(data == FEND)
        {
            /* frame complete */
            kiss_info.length = kiss_info.index;
            kiss_info.index = 0;
            kiss_info.esc = 0;

            if(kiss_info.error == 0)
            {
                /* complete frame without errors */
                if( (kiss_info.length > 0)
                    &&
                    ((kiss_info.frame[0] & 0x0f) == 0)
                  )
                {
                    /* dataframe */
                    return 1;
                }
            }
            else
            {
                kiss_info.error = 0;
            }
            continue;
        }

        if(kiss_info.esc == 1)
        {
            kiss_info.esc = 0;
            if(data == TFESC)
            {
                kiss_info.frame[kiss_info.index] = FESC;

                /* increment the index */
                if(kiss_info.index < 400)
                {
                    kiss_info.index++;
                }
                else
                {
                    /* overflow, frame too large */
                    kiss_info.error = 1;
                }
                continue;
            }
            else if(data == TFEND)
            {
                kiss_info.frame[kiss_info.index] = FEND;

                /* increment the index */
                if(kiss_info.index < 400)
                {
                    kiss_info.index++;
                }
                else
                {
                    /* overflow, frame too large */
                    kiss_info.error = 1;
                }
                continue;
            }
        }

        if(data == FESC)
        {
            kiss_info.esc = 1;
            continue;
        }

        kiss_info.frame[kiss_info.index] = data;

        /* increment the index */
        if(kiss_info.index < 400)
        {
            kiss_info.index++;
        }
        else
        {
            /* overflow, frame too large */
            kiss_info.error = 1;
        }
    }
}

short kiss_inp(frame_t *frame_p)
{
    short port;

    /* complete frame without errors */
    if( (kiss_info.length == 0)
        ||
        ((kiss_info.frame[0] & 0x0f) != 0)
      )
    {
        /* no dataframe */
        return -1;
    }

    port = kiss_info.frame[0];
    port = (port >> 4) & 0x0f;

    if(port >= kiss_info.maxports)
    {
        /* port out of range*/
        return -1;
    }

    frame_p->port = port;

    frame_p->len = kiss_info.length - 1;
    memcpy(frame_p->frame, &(kiss_info.frame[1]), kiss_info.length - 1);

    return 0;
}

short kiss_out(frame_t *frame_p)
{
    int   i;
    int   ret;
    char  c;
    char  fend = FEND;
    char  fesc = FESC;
    char  tfend = TFESC;
    char  tfesc = TFESC;
    char  datatype;

    /* start frame */
    ret = write(kiss_info.fd, &fend, 1);
    if(ret == -1) { return -1; }

    /* write dataframe */
    datatype = frame_p->port;
    datatype = datatype << 4;

    ret = write(kiss_info.fd, &datatype, 1);
    if(ret == -1) { return -1; }

    /* write data */
    i = frame_p->len;

    for(i = 0; i < frame_p->len; i++)
    {
        c = frame_p->frame[i];
        if(c == FEND)
        {
            /* send escape followed by transposed end */
            ret = write(kiss_info.fd, &fesc, 1);
            if(ret == -1) { return -1; }
            ret = write(kiss_info.fd, &tfend, 1);
            if(ret == -1) { return -1; }
        }
        else if(c == FESC)
        {
            /* send escape followed by transposed escape */
            ret = write(kiss_info.fd, &fesc, 1);
            if(ret == -1) { return -1; }
            ret = write(kiss_info.fd, &tfesc, 1);
            if(ret == -1) { return -1; }
        }
        else
        {
            /* send as is */
            ret = write(kiss_info.fd, &c, 1);
            if(ret == -1) { return -1; }
        }
    }

    /* end frame */
    ret = write(kiss_info.fd, &fend, 1);
    if(ret == -1) { return -1; }
 
    return 0;
}

short kiss_param(char *param)
{
    char *p;

    p = strtok(param, ":");
    if(p == NULL)
    {
        say("kiss_param error, expected device not found.\r\n");    
        return 0;
    }
    kiss_info.dev = strdup(p);

    p = strtok(NULL, ":");
    if(p == NULL)
    {
        say("kiss_param error, expected baudrate not found.\r\n");    
        return 0;
    }
    kiss_info.baudrate = atoi(p);

    p = strtok(NULL, ":");
    if(p == NULL)
    {
        say("kiss_param error, expected number of ports not found.\r\n");    
        return 0;
    }

    kiss_info.maxports = atoi(p);

    return kiss_info.maxports;
}
#endif /* KISS */
