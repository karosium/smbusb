@echo off

where /q gcc.exe
if ERRORLEVEL 1 (
	echo gcc not found in path. Please install TDM-GCC: http://tdm-gcc.tdragon.net/download
)

if NOT "%1"=="32" ( 
	if NOT "%1"=="64" (
		echo usage: 
		echo        build.bat 32      - build 32bit release
		echo        build.bat 64      - build 64bit release
		pause
		exit 0
	)
)

if not exist libusb_win%1 (
	echo Please create a folder called libusb_win%1 and copy the following files 
	echo from the latest Windows libusb binary package into it.
	echo *
	echo * libusb-1.0.dll from the MinGW%1 folder
	echo * libusb.h from the include folder
	echo *
	echo Look for the 7Zip archives at https://sourceforge.net/projects/libusb/files/libusb-1.0/
	pause
	exit 0
) else (
	if not exist libusb_win%1\libusb.h (
		echo Missing libusb\libusb.h
		exit 0
	)
	if not exist libusb_win%1\libusb-1.0.dll (
		echo Missing libusb\libusb-1.0.dll
		exit 0
	)	

	echo Building %1bit release.

	echo Prerequisites found. 
	
	echo Building libsmbusb
	
	copy /y firmware\firmware.h lib\ >nul

	cd lib
	build.bat %1

	echo Building tools
	cd ..\tools
	build.bat %1

	cd..

	mkdir RELEASE_WIN%1
	copy /y libusb_win%1\libusb-1.0.dll RELEASE_WIN%1\ >nul
	copy /y lib\libsmbusb.dll RELEASE_WIN%1\ >nul
	copy /y tools\*.exe RELEASE_WIN%1\ >nul
	
	echo Done.
)