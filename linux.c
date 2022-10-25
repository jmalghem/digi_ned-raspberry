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
 *  2022-10 (F4GVY) : Remove calls to io functions not supported by ARM.
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "linux.h"
#include "digicons.h"

/* global data */
short directvideo;

/* local data */
static int    kb_initialized = 0;
static int    kb_fd   = -1;
static int    kb_char = -1;
static int    kb_esc  = 0;
static struct termios t_old;

/*
 * Restore the tty to the old state we had before initialization
 */
void kb_exit()
{
    if(kb_initialized == 1)
    {
        (void) tcsetattr(kb_fd, TCSANOW, &t_old);
        close(kb_fd);
        kb_initialized = 0;
    }
}

/*
 * Initialize the tty to poll for keys
 *
 * If is fails just silently ignore. The keys will not work but the DIGI,
 * which is most important, will still run. Failures will occur if
 * stdin is not associated with a tty.
 */
void kb_init() 
{
    struct termios t;

    if(kb_initialized == 1)
    {
        /* Already done, ignore */
        return;
    }

    kb_fd = open("/dev/tty", O_RDWR|O_NOCTTY|O_NONBLOCK);

    /* Open our TTY */
    if (kb_fd == -1)
    {
        return;
    }

    /* Check if stdin is connected to a tty, stdin has file descriptor 0 */
    if(isatty(kb_fd) == 0)
    {
        close(kb_fd);
        return;
    }

    /* Get the attributes of the TTY for modification */
    if (tcgetattr(kb_fd, &t) != 0)
    {
        /* Can't get settings, kb not initialized */
        close(kb_fd);
        return;
    }

    /* Get the attributes of the TTY to be able to restore them later */
    if (tcgetattr(kb_fd, &t_old) != 0)
    {
        /* Can't get settings, kb not initialized */
        close(kb_fd);
        return;
    }

    /* Set the terminal settings to raw access */
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 2;
    t.c_iflag     = 0;
    t.c_lflag     = ISIG;
    t.c_cflag    |= HUPCL|CLOCAL|CREAD|CRTSCTS;

    /* flush the old data */
    if (tcflush(kb_fd, TCIFLUSH) == -1)
    {
        /* Can't flush old data, kb not initialized */
        close(kb_fd);
        return;
    }

    /* Set the new attributes for the TTY */
    if (tcsetattr(kb_fd, TCSANOW, &t) == -1)
    {
        /* Can't change settings, kb not initialized */
        close(kb_fd);
        return;
    }

    /* Intialize used variables */
    kb_char = -1;
    kb_esc  = 0;

    /* Change flag, we are done! */
    kb_initialized = 1;

    return;
}

/*
 * Check if the keyboard has been hit
 */
int kbhit()
{
    int    i;
    char   key;

    if(kb_initialized != 1)
    {
        /*
         * We don't read keys, initialization failed somehow or
         * initialization was not done because the keyboard support
         * was not wanted.
         */
        return 0;
    }

    i = read(kb_fd, &key, 1);

    if(i == -1)
    {
        if(kb_char != -1)
        {
            /* still key pending (after ESC) */
            return 1;
        }
        return 0;
    }
    else
    {
        /* Use ESC as "ALT" replacement */
        if(key == 0x001b)
        {
            if(kb_esc == 0)
            {
                /* First escape, remember this! No key yet */
                kb_esc = 1;
                return 0;
            }
            else
            {
                /* Second escape, we want this key! */
                kb_esc  = 0;
                kb_char = key;
                return 1;
            }
        }
        else 
        {
            /*
             * Key was not ESC. Was previous key ESC? if if no then just
             * pass, if yes, translate to DOS scancode
             */
            if(kb_esc == 0)
            {
#ifdef __CYGWIN32__
                /* ALT key conversions */
                switch((int) key) {
                case -31: /* ALT A */
                          kb_esc = 1;
                          kb_char = 0x1e;
                          return kb_esc;
                          break;
                case -30: /* ALT B */
                          kb_esc = 1;
                          kb_char = 0x30;
                          return kb_esc;
                          break;
                case -24: /* ALT H */
                          kb_esc = 1;
                          kb_char = 0x23;
                          return kb_esc;
                          break;
                case -20: /* ALT L */
                          kb_esc = 1;
                          kb_char = 0x26;
                          return kb_esc;
                          break;
                case -12: /* ALT T */
                          kb_esc = 1;
                          kb_char = 0x14;
                          return kb_esc;
                          break;
                case -10: /* ALT V */
                          kb_esc = 1;
                          kb_char = 0x2f;
                          return kb_esc;
                          break;
                case -8:  /* ALT X */
                          kb_esc = 1;
                          kb_char = 0x2d;
                          return kb_esc;
                          break;
                }
#endif
                /* not following ESC, normal key */

                /* Map 0x7f to backspace */
                if(key == 0x7f)
                        key = '\b';

                kb_char = key;
                return 1;
            }
            else
            {
                /* following ESC, ALT key, convert */
                switch(key) {
                case 'a':
                case 'A':   kb_char = 0x1e;
                            break;
                case 'b':
                case 'B':   kb_char = 0x30;
                            break;
                case 'h':
                case 'H':   kb_char = 0x23;
                            break;
                case 'l':
                case 'L':   kb_char = 0x26;
                            break;
                case 't':
                case 'T':   kb_char = 0x14;
                            break;
                case 'v':
                case 'V':   kb_char = 0x2f;
                            break;
                case 'x':
                case 'X':   kb_char = 0x2d;
                            break;
                default:    /* remove ESC flag */
                            kb_esc = 0;
                }

                /* return that we have a complete sequence now */
                return kb_esc;
            }
        }
    }
}

/*
 * Return a character after kbhit returned '1'
 */
char getch()
{
    char c;

    if(kb_esc == 1)
    {
        kb_esc = 0;
        return '\0';
    }
    else
    {
        c = (char) kb_char; 
        kb_char = -1;

        return c;
    }
}

#ifdef _SERIAL_
/*
 * Restore the tty to the old state we had before initialization
 */
void serial_close(int fd)
{
    struct termios t;

    if(fd != -1)
    {
        /* Get the attributes of the serial line for modification */
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

        /* Set the new attributes for the serial line */
        (void) tcsetattr(fd, TCSANOW, &t);

        close(fd);
    }
}

/*
 * Open the serial line to poll for data
 *
 * Return -1 if it fails.
 */
int serial_open(short comnr, short baudrate) 
{
    struct termios t;
    char   dev[256];
    int    fd;

    sprintf(dev, "/dev/ttyAMA%d", comnr);

    fd = open(dev, O_RDWR|O_NOCTTY|O_NONBLOCK);

    /* Open our serial line */
    if (fd == -1)
    {
        printf("/* Unable to open, ignore */");
        return -1;
    }

    /* Check if fd is connected to a tty, stdin has file descriptor 0 */
    if(isatty(fd) == 0)
    {
        printf("/* Not a serial line, ignore */");
        close(fd);
        return -1;
    }

    /* Get the attributes of the serial line for modification */
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
    case 1200: cfsetispeed(&t, B1200);
               cfsetospeed(&t, B1200);
               break;
    case 2400: cfsetispeed(&t, B2400);
               cfsetospeed(&t, B2400);
               break;
    case 4800: cfsetispeed(&t, B4800);
               cfsetospeed(&t, B4800);
               break;
    case 9600: cfsetispeed(&t, B9600);
               cfsetospeed(&t, B9600);
               break;
    default:   /* Illegal baudrate on serial, ignore */
               break;
    }

    /* flush the old data */
    if (tcflush(fd, TCIFLUSH) == -1)
    {
        /* Can't flush old data, serial line not initialized */
        serial_close(fd);
        return -1;
    }

    /* Set the new attributes for the serial line */
    if (tcsetattr(fd, TCSANOW, &t) == -1)
    {
        /* Can't change settings, serial line not initialized */
        serial_close(fd);
        return -1;
    }

    return fd;
}

/*
 * Read a character from the serial line
 */
int serial_in(int fd, char *buf_p)
{
    int i;

    i = read(fd, buf_p, 1);

    return i;
}
#endif /* _SERIAL_ */
