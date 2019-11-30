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
gcc -m%1 -Wall -shared smbusb.c fxloader.c -I../libusb_win%1 -L../libusb_win%1 -lusb-1.0 -olibsmbusb.dll
if %ERRORLEVEL% GTR 0 (
	echo Error building library
	exit 1
)