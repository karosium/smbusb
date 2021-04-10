# smbusb

SMBusb is a USB SMBus interface based on the Cypress FX2LP.
This repository contains the firmware and library (libsmbusb) that make up the interface. 
Firmware is uploaded automatically by the library and remains loaded until power-cycle.

For more information see http://www.karosium.com/p/smbusb.html

### Prerequisites

 * pkg-config
 * libusb >= 1.0

On *nix:
 * build environment with autotools
 * sdcc and xxd for building the firmware
 
On Windows:
 * TDM-GCC (http://tdm-gcc.tdragon.net/) 
 
 Note: Windows build uses pre-built firmware

### Build instructions

On *nix:
``` 
./init.sh
./configure (options: --disable-firmware, --disable-tools)
make
make install
```

On Windows:
``` 
build.bat 32
build.bat 64
```
