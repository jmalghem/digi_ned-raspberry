@echo off
rem 
rem This file is part of DIGI_NED
rem 

rem Jump over the restart part
goto first_time

rem Restart digi
:restart

echo Restarting DIGI_NED

rem Unload AX25_MAC MAC layer driver
ax25_mac.exe -u

rem First time start or restarting
:first_time

rem For VGA you may want to enable the next line...
rem Do not forget to switch back to 25 lines below
rem mode con lines=50

rem For EGA you may want to enable the next line...
rem Do not forget to switch back to 25 lines below
rem mode con lines=43

rem Load AX25_MAC MAC layer driver
ax25_mac.exe -PCOM1 -B1200 -L -F -C17 -BU50

rem
rem Digi_ned exit codes:
rem
rem -1 error during startup
rem  0 okay, normal exit
rem  1 exit due to debug-trap (only if compiled with debugging)
rem  2 exit by operator ?exit command
rem
digi_ned.exe -v %1

rem
rem Catch exitcodes
rem
rem Note that 'if errorlevel 123' which means 'errorlevel greater than 123',
rem If we want to catch one specific errorlevel than we also need to check
rem the next higher level avoid catching that one too. Also we need to
rem check on high numbers first. In the example I have explicit tests
rem for exitcodes 123 and 10
rem

rem
rem Startup error
rem
if errorlevel 255 goto exit_255

rem
rem Two example exit codes "123" and "10"
rem
if errorlevel 124 goto default
if errorlevel 123 goto exit_123

if errorlevel 11 goto default
if errorlevel 10 goto exit_10

rem
rem Exited by operator
rem
if errorlevel 3 goto default
if errorlevel 2 goto exit_2

rem
rem Debug trap
rem
if errorlevel 1 goto exit_1

rem
rem Normal exit trap
rem
if errorlevel 0 goto exit_0

rem
rem Default handling for not caught exitcodes
rem
:default
echo Unhandled exitcode, restart will be done
goto restart

rem
rem Handling for exit code 255 (same as -1)
rem
:exit_255
echo Error at startup
goto leave

rem
rem Handling for exit code 123 (example for ?exit 123)
rem
:exit_123
echo Exit by remote ?exit 123 command
goto leave

rem
rem Handling for exit code 10 (example for ?exit 10)
rem
:exit_10
echo Exit by remote ?exit 10 command
goto leave

rem
rem Handling for exit code 2 (?exit command, restart the digi)
rem
:exit_2
echo Restart by remote ?exit command
goto restart

rem
rem Handling for exit code 1 (Debug code found an error)
rem
:exit_1
echo Debug code detected a bug in the code
goto leave

rem
rem Handling for exit code 0 (normal exit by ALT-X)
rem
:exit_0
echo Normal exit
goto leave

:leave
rem Switch back to 25 lines if you used 50 (VGA) or 43 (EGA) lines
rem You may want to have this at other places as well, but be aware
rem not to mess-up the exit-code of DIGI_NED, otherwise the 
rem "errorlevel" commands do not work anymore...
rem mode con lines=25

rem Unload AX25_MAC MAC layer driver
ax25_mac.exe -u
