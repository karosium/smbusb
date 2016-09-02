@echo off
gcc -Wall -shared smbusb.c fxloader.c -I../libusb -L../libusb -lusb-1.0 -olibsmbusb.dll
if %ERRORLEVEL% GTR 0 (
	echo Error building library
	exit 1
)