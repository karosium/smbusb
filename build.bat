@echo off

where /q gcc.exe
if ERRORLEVEL 1 (
	echo gcc not found in path. Please install TDM-GCC: http://tdm-gcc.tdragon.net/download
)

if not exist libusb (
	echo Please create a folder called libusb and copy the following files 
	echo from the latest Windows libusb binary package into it.
	echo Look for the 7Zip archives at https://sourceforge.net/projects/libusb/files/libusb-1.0/
	echo *
	echo * libusb-1.0.dll
	echo * libusb.h
	echo *
	pause
	exit 0
) else (
	if not exist libusb\libusb.h (
		echo Missing libusb\libusb.h
		exit 0
	)
	if not exist libusb\libusb-1.0.dll (
		echo Missing libusb\libusb-1.0.dll
		exit 0
	)	

	echo Prerequisites found. 
	
	echo Building libsmbusb
	
	copy /y firmware\firmware.h lib\ >nul

	cd lib
	build.bat

	echo Building tools
	cd ..\tools
	build.bat

	cd..

	mkdir RELEASE_WIN
	copy /y libusb\libusb-1.0.dll RELEASE_WIN\ >nul
	copy /y lib\libsmbusb.dll RELEASE_WIN\ >nul
	copy /y tools\*.exe RELEASE_WIN\ >nul
	
	echo Done.
)