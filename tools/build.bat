@echo off
if NOT "%1"=="32" ( 
	if NOT "%1"=="64" (
		echo usage: 
		echo        build.bat 32      - build 32bit release
		echo        build.bat 64      - build 64bit release
		pause
		exit 0
	)
)
gcc -m%1 -L../lib -I../lib smbusb_scan.c -o smbusb_scan.exe -lsmbusb
if %ERRORLEVEL% GTR 0 goto tool_build_err
gcc -m%1 -L../lib -I../lib smbusb_sbsreport.c -o smbusb_sbsreport.exe -lsmbusb
if %ERRORLEVEL% GTR 0 goto tool_build_err
gcc -m%1 -L../lib -I../lib smbusb_bq8030flasher.c -o smbusb_bq8030flasher.exe -lsmbusb
if %ERRORLEVEL% GTR 0 goto tool_build_err
gcc -m%1 -L../lib -I../lib smbusb_r2j240flasher.c -o smbusb_r2j240flasher.exe -lsmbusb
if %ERRORLEVEL% GTR 0 goto tool_build_err
gcc -m%1 -L../lib -I../lib smbusb_m37512flasher.c -o smbusb_m37512flasher.exe -lsmbusb
if %ERRORLEVEL% GTR 0 goto tool_build_err
gcc -m%1 -L../lib -I../lib smbusb_bootstrap.c -o smbusb_bootstrap.exe -lsmbusb
if %ERRORLEVEL% GTR 0 goto tool_build_err
gcc -m%1 -L../lib -I../lib smbusb_comm.c -o smbusb_comm.exe -lsmbusb

goto tool_build_ok
:tool_build_err
echo Error building tools
exit 1

:tool_build_ok