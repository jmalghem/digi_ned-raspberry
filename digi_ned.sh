#!/bin/bash
#
# Sample start file to start DIGI_NED in background
#
# Put this file in the same directory as "digi_ned". This script can be
# called from any place.
#

#
# Check if the script is called by root, if not then stop
#
if [ `id --user` -ne 0 ]
then
    echo $0: DIGI_NED can only run with root privilege!
    exit 1
fi

#
# Find out the DIGI_NED directory, for this the "digi_ned.sh" must be in
# the same directory as the "digi_ned" program, "digi_ned.ini" file.
#
DIRECTORY=`dirname $0`
#
# Change to this directory.
#
cd $DIRECTORY

#
# Start DIGI_NED in background, pass parameters along
#
nohup ./digi_ned $* &

#
# Show results
#
echo DIGI_NED started with process-id $!, use \"kill $!\" to stop DIGI_NED

