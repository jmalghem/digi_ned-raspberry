#
#  Copyright (C) 2000-2009  Henk de Groot - PE1DNN
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#  Please note that the GPL allows you to use the driver, NOT the radio.
#  In order to use the radio, you need a license from the communications
#  authority of your country.
#
DIGI_NED = 0.4.2

#
# This Makefile is only used on Linux and Unix lookalike systems. The DOS
# version of DIGI_NED uses "digi_ned.prj" which is a TurboC++ 3.0 project
# file. So the options below are only for Linux and Unix lookalike systems.
#
# Select the type of interface you want to use
#
# OLD_AX25: AX.25 Utils for 2.0.xx kernels
# NEW_AX25: AX.25 Utils for 2.2.xx and higher kernels
# KISS: Using a /dev/tty port to an (M)KISS capable TNC
# AGWPE: Using a network connection to another PC running AGWPE
#
# Uncomment one (but only one) option
#
# Specify the type of interface you want to use by uncommeting one of the
# following lines:
#
#INTERFACE = OLD_AX25
INTERFACE = NEW_AX25
#INTERFACE = KISS
#INTERFACE = AGWPE

DEFS += -D$(INTERFACE)

# if satellite tracker needs to be excluded then comment the next line
#DEFS += -D_SATTRACKER_

# if telemetry stuff needs to be excluded then comment the next line
#DEFS += -D_TELEMETRY_

# if weather stuff needs to be excluded then comment the next line
#DEFS += -D_WX_

# if port-output stuff needs to be excluded then comment the next line
#DEFS += -D_OUTPORT_

# if !ptt command stuff needs to be excluded then comment the next line
#DEFS += -D_PTT_

# if the serial-input stuff needs to be excluded then comment the next line
DEFS += -D_SERIAL_

ifeq ($(INTERFACE), OLD_AX25)
LIBS    = -lax25
else
ifeq ($(INTERFACE), NEW_AX25)
LIBS    = -lax25
endif
endif

RM = rm 
ECHO = /bin/echo
# Use this when debugging with GDB
#CC = gcc -O -ggdb -Wall
# Use this to generate a finite version
#CC = gcc -O6 -g -Wall -Wstrict-prototypes -Wunused
CC = gcc -O6 -g -Wall -Wstrict-prototypes

INCLUDES= -I./ -I/usr/include -I/usr/local/include
LIBS    += -lm -L/usr/local/lib -L/usr/lib/
DEFS    += -D_LINUX_
CFLAGS  = -c $(INCLUDES) $(DEFS)
LDFLAGS = -Wl,-s

PROGRAMS = call.o digipeat.o genlib.o keep.o main.o \
	message.o preempt.o mheard.o distance.o dx.o \
	output.o read_ini.o send.o timer.o telemetr.o \
	command.o mac_if.o linux.o cygwin.o predict.o \
	sat.o kiss.o agwpe.o serial.o wx.o
PROGS = call.c digipeat.c genlib.c keep.c main.c \
	message.c preempt.c mheard.c distance.c dx.c \
	output.c read_ini.c send.c timer.c telemetr.c \
	command.c mac_if.c linux.c cygwin.c predict.c \
	sat.c kiss.c agwpe.c serial.c wx.c

all: digi_ned

digi_ned: version.h $(PROGRAMS)
	$(CC) $(LDFLAGS) -o $@ $(PROGRAMS) $(LIBS)

tar: clean
	cd ..;tar czf digi_ned-$(DIGI_NED).tgz digi_ned

clean:
	-gccmakedep
	-$(RM) -f *.o core digi_ned *.bak *.orig *.log *.tnc *.dec *.exe backup.zip

depend:
	gccmakedep $(DEFS) -- $(INCLUDES) -- $(PROGS)

version.h: Makefile
	@echo "Creating version.h and version.bat in DOS format"
	@$(ECHO) -n -e "#define VERSION \"$(DIGI_NED)\"\r\n" > version.h
	@$(ECHO) "set archive=dnexe"`echo $(DIGI_NED) | tr -d "."`".zip\r\n" \
		> version.bat

# DO NOT DELETE
