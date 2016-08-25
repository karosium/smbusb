# libsmbusb

libsmbusb is a library for communicating with the SMBusb firmware. It automatically tries to upload the
firmware to the selected Cypress FX2LP device if it's not already running it.

### Functions:

#### Open/Close/Test

```c
int SMBOpenDeviceVIDPID(unsigned int vid, unsigned int pid);

int SMBOpenDeviceBusAddr(unsigned int bus, unsigned int addr);
```
    return value >0 on success and contains the firmware version contained in the 3 lower bytes
    least signicant byte is most significant version number eg. 0x030001 = 1.0.3
    if <0 then error code, see libsmbusb.h

```c
void SMBCloseDevice();
	
unsigned int SMBInterfaceId();
```
    returns 0x4d5355 if firmware is responding properly
    
```c
void SMBSetDebugLogFunc(void *logFunc);
```
    A pointer to a function with parameters (char * buf, int len) can be passed here to catch debug messages

#### Communication

##### Standard SMBus

```c
int SMBSendByte(unsigned int address, unsigned char command);
```
    Sends the command byte immediately followed by STOP. Useful to invoke some parameterless commands.

```c
int SMBReadByte(unsigned int address, unsigned char command);
```
    The standard SMBus Read Byte protocol. Reads an unsigned byte from "command"
```c
int SMBWriteByte(unsigned int address, unsigned char command, unsigned char data);
```
    The standard SMBus Write Byte protocol. Writes a unsigned byte to "command":
```c
int SMBReadWord(unsigned int address, unsigned char command);
```
    The standard SMBus Read Word protocol. Reads a 16bit word from "command"
```c
int SMBWriteWord(unsigned int address, unsigned char command, unsigned int data);
```
    The standard SMBus Write Word protocol. Writes a 16bit word to "command"
```c
int SMBReadBlock(unsigned int address, unsigned char command, unsigned char *data);
```
    The standard SMBus Read Block protocol. Reads a maximum of 255 bytes from "command"
```c
int SMBWriteBlock(unsigned int address, unsigned char command, unsigned char *data, unsigned char len);
```
    The standard SMBus Write Block protocol. Writes a maximum of 255 bytes to "command"


* Return values all for functions above will be >=0 on success. 
* Usually the number of bytes read for reads and 0 for writes.
* Values <0 are libusb error codes.
* Address parameters are always the READ address of the device.
    
    
```c
extern void SMBEnablePEC(unsigned char state);
```
    0 disables, 1 enables SMBus Packet Error Checking. This is done in-firmware.
    When PEC is enabled reads will hard fail on PEC errors. Use SMBGetLastReadPECFail() 
    to check whether hard-fails (retval < 0)  were due to a PEC mismatch.
    
    Note that PEC is enabled by default and should be disabled manually if not needed.

```c
unsigned char SMBGetLastReadPECFail();
```
    Returns 0xFF if PEC is enabled and the last read failed because of PEC, 0 otherwise
    Calling this function also clears the PEC error flag so call it after every read when interested
    in PEC failure and it's location.

##### Arbitrary SMBus(/I2C)
```c
int SMBWrite(unsigned char start, unsigned char restart, unsigned char stop, 
             unsigned char *data, unsigned int len);
```
    Write some bytes.
    start and restart will generate the condition before sending "data" if ==1,
    stop will send STOP after "data" is sent if ==1
```c
int SMBRead(unsigned int len, unsigned char* data, unsigned char lastRead);
```
    Read some bytes.
    Any number of bytes can be read. if lastRead is ==1 then STOP will be sent afterwards.
    
    SMBWrite and SMBRead allow implementation of unstandard SMBus devices or I2C.
```c
unsigned int SMBGetArbPEC();
```
    If PEC has been enabled with SMBEnablePEC(1) then PEC will be calculated on the fly when using
    SMBWrite and SMBRead commands. 
    The firmware will read the extra PEC byte when SMBRead is called with lastRead==1
    GetArbPec returns a 16bit word with the MSB being the PEC received from the device 
    and the LSB being the calculated PEC.
    
    Note that not all devices will support PEC.
    
    Note that PEC is enabled by default and should be disabled manually if not needed.
    
