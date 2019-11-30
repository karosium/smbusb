/*
* Copyright (c) 2016 Viktor <github@karosium.e4ward.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>

#include "libusb.h"
#include "fxloader.h"
#include "libsmbusb.h"

#include "firmware.h"
#include <strings.h>

static libusb_device *dev, **devs;
static libusb_device_handle *device = NULL;

void (*extLogFunc)(unsigned char* buf, unsigned int len) = NULL;

void logerror(const char *format, ...)
{
	if (extLogFunc == NULL) return;
	va_list ap;
	va_start(ap, format);
	char *outpBuf = malloc(10240);
	vsnprintf(outpBuf,10240,format,ap);
}

int InitDevice(){
	int status;
	unsigned int fwver=0;
	/* We need to claim the first interface */
	libusb_set_auto_detach_kernel_driver(device, 1);
	status = libusb_claim_interface(device, 0);
	if (status != LIBUSB_SUCCESS) {
		libusb_close(device);
		logerror("libusb_claim_interface failed: %s\n", libusb_error_name(status));
		return ERR_CLAIM_INTERFACE;
	}

	if (SMBInterfaceID() != 0x4d5355) 
	{
		// try loading firmware
		if ((status = CypressUploadIhxFirmware(device, (char *)&build_smbusb_firmware_ihx, build_smbusb_firmware_ihx_len)) <0) return status;
		sleep(2);	
		return INIT_RETRY;
	}

	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_FIRMWARE_VERSION,
					0, 
					0,
					(void*)&fwver, 
					3, 
					1000);
  	if (status==3) {return fwver;} else {return status;}
}

int SMBOpenDeviceVIDPID(unsigned int vid,unsigned int pid){
	int status;
	if (device != NULL) return ERR_ALREADY_OPEN;


	status = libusb_init(NULL);
	if (status < 0) {
		logerror("libusb_init() failed: %s\n", libusb_error_name(status));
		return status;
	}
	libusb_set_debug(NULL, 0);
	
	openvidpid_retry:
	device = libusb_open_device_with_vid_pid(NULL, (uint16_t)vid, (uint16_t)pid);
		if (device == NULL) {
			logerror("libusb_open() failed\n");
			return ERR_DEVICE_OPEN;		
		}	
	status = InitDevice();
	if (status == INIT_RETRY) goto openvidpid_retry;
	return status;
}

int SMBOpenDeviceBusAddr(unsigned int bus, unsigned int addr){
	int i,status;
	
	if (device != NULL) return ERR_ALREADY_OPEN;

	status = libusb_init(NULL);
	if (status < 0) {
		logerror("libusb_init() failed: %s\n", libusb_error_name(status));
		return status;
	}
	libusb_set_debug(NULL, 0);


	openbusadd_retry:
	if (libusb_get_device_list(NULL, &devs) < 0) {
		logerror("libusb_get_device_list() failed: %s\n", libusb_error_name(status));
		return status;
	}
	for (i=0; (dev=devs[i]) != NULL; i++) {
		if ((libusb_get_bus_number(dev) == bus) && (libusb_get_device_address(dev) == addr)) {
			status = libusb_open(dev, &device);
			libusb_free_device_list(devs, 1);
			if (status < 0) {
				logerror("libusb_open() failed: %s\n", libusb_error_name(status));
				return status;
			}				
			status = InitDevice();
			if (status == INIT_RETRY) goto openbusadd_retry;
			return status;
		}
	}
	return -1;
}

void SMBCloseDevice() {
	if (device == NULL) return;
	libusb_release_interface(device, 0);
	libusb_close(device);
	libusb_exit(NULL);
	device=NULL;
}

unsigned int SMBInterfaceID() {
	unsigned int magic=0;
	int status;
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_INTERFACE_ID,
					0, 
					0,
					(void*)&magic, 
					3, 
					100);
	if ((status <=0) | (magic != 0x4d5355)) {
		return 0;	
	} else {
		return magic;
	}	
}

int SMBReadByte(unsigned int address, unsigned char command) {
	int status, ret=0;
	
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_READ_BYTE,
					address, 
					command,
					(void*)&ret, 
					1, 
					100);
	if (status==1) { return ret;} else {return status;}
}

int SMBSendByte(unsigned int address, unsigned char command) {
	int status, ret=0;
	
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_SEND_BYTE,
					address, 
					command,
					(void*)&ret, 
					1, 
					100);
	return status;
}


int SMBWriteByte(unsigned int address, unsigned char command, unsigned char data) {
	int status, ret=0;
	
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_WRITE_BYTE,
					address, 
					command,
					&data, 
					1, 
					100);
	if (status==1) { return ret;} else {return status;}
}


int SMBReadWord(unsigned int address, unsigned char command) {
	int status, ret=0;
	
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_READ_WORD,
					address, 
					command,
					(void*)&ret, 
					2, 
					100);
	if (status==2) { return ret;} else {return status;}
}

int SMBWriteWord(unsigned int address, unsigned char command, unsigned int data) {
	int status, ret=0;
	
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_WRITE_WORD,
					address, 
					command,
					(void*)&data, 
					2, 
					100);
	if (status==2) { return ret;} else {return status;}
}


int SMBReadBlock(unsigned int address, unsigned char command, unsigned char *data) {
	int status, rcvd=0, total = 0;
	unsigned char *tmp = malloc(64);

	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_READ_BLOCK,
					address, 
					command,
					(void*)tmp, 
					64, 
					100);

	if (status <=0) {
		 free(tmp);
		 return status;
	}
	total = tmp[0];
	rcvd+=status-1;

	memcpy(data,tmp+1,rcvd);
	
	while (rcvd < total) {
		
		status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_READ_BLOCK,
					address, 
					command,
					(void*)tmp, 
					64, 
					100);

		if (status <0) {
			 free(tmp);
			 return status;
		}
		memcpy(data+rcvd,tmp,status);
		rcvd+=status;
	}
		
	free(tmp);
	return total;
}

int SMBWriteBlock(unsigned int address, unsigned char command, unsigned char *data, unsigned char len) {
	int status, i=0, wholeWrites=0, remainder=0;
	unsigned char *tmp = malloc(256);
	
	wholeWrites = (len+1) / 64;
	remainder = (len+1) - wholeWrites*64;

	tmp[0]=len;
	memcpy(tmp+1,data,len);

	while (i<wholeWrites) {
		status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_WRITE_BLOCK,
					address, 
					command,
					(void*)(tmp+(i*64)), 
					64, 
					100);		
		if (status != 64) {
			free(tmp);
			return status;
		}
		i++;
	}

	if (remainder>0) {
		status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_WRITE_BLOCK,
					address, 
					command,
					(void*)(tmp+(wholeWrites*64)), 
					remainder, 
					100);	
		if (status != remainder) {
			free(tmp);
			return status;
		}
	}
	
	free(tmp);
	return len;			
}


unsigned char SMBGetLastReadPECFail() {
	int status;
	unsigned char pec_failed=0;
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_GET_CLEAR_PEC_FAIL,
					1, 
					0,
					(void*)&pec_failed, 
					1, 
					100);

	if (status==1) { return pec_failed;} else return status;	
}

void SMBEnablePEC(unsigned char state) {
		libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_ENABLE_PEC,
					state>0?1:0, 
					0,
					NULL, 
					0, 
					100);
}

int SMBWrite(unsigned char start, unsigned char restart, unsigned char stop, unsigned char *data, unsigned int len) {
	int status,i,wholeWrites,remainder;
	unsigned char rs;	

	wholeWrites = len / 64;
	remainder = len-wholeWrites*64;

	rs=0;

	if (start) rs |= SMB_WRITE_CMD_START_FIRST;
	if (restart) rs |= SMB_WRITE_CMD_RESTART_FIRST;

	i=0;

	while (i<wholeWrites) {
		if ((i==wholeWrites-1) && (remainder==0) && (stop)) rs |= SMB_WRITE_CMD_STOP_AFTER;
		status = libusb_control_transfer(device,
						LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
						SMB_WRITE,
						64, 
						rs,
						(void*)(data+(i*64)), 
						64, 
						100);
		rs &= ~SMB_WRITE_CMD_START_FIRST;
		rs &= ~SMB_WRITE_CMD_RESTART_FIRST;

		if (status < 64) return status;

		i++;
	}
	 
       	if (remainder>0) { 
		if (stop) rs |= SMB_WRITE_CMD_STOP_AFTER;
		status = libusb_control_transfer(device,
						LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
						SMB_WRITE,
						remainder, 
						rs,
						(void*)(data+(wholeWrites*64)), 
						remainder, 
						100);		
	}
	if (status >0) { return len; } else { return status; }
	
}

int SMBRead(unsigned int len, unsigned char* data, unsigned char lastRead) {
	int status,i,wholeReads,remainder;
	unsigned char rs;	
	
	wholeReads = len / 64;
	remainder = len-wholeReads*64;
//	printf("wholeReads:%d, remainder:%d\n",wholeReads,remainder);
	
	rs=0;

	rs |= SMB_READ_CMD_FIRST_READ;
	i=0;
	while (i<wholeReads) {
		if ((lastRead) && (i==wholeReads-1) && (remainder == 0)) rs |= SMB_READ_CMD_LAST_READ;
		status = libusb_control_transfer(device,
						LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
						SMB_READ,
						64, 
						rs,
						(void*)(data+(i*64)), 
						64, 
						100);		
		
		rs &= ~SMB_READ_CMD_FIRST_READ;
//               	printf("wholeRead:%d, status:%d\n",i,status);
		if (status<64) return status;		
		i++;
	}

	if (remainder >0) {
		if (lastRead) {
			rs |= SMB_READ_CMD_LAST_READ;
		}
		status = libusb_control_transfer(device,
						LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
						SMB_READ,
						remainder, 
						rs,
						(void*)(data+(wholeReads*64)), 
						remainder, 
						100);		
	}	
//            	printf("remainder:%d, status:%d\n",i,status);
	return status;
}

unsigned int SMBGetArbPEC() {
	int status;
	short pecs=0;
	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					SMB_GET_MRQ_PECS,
					2, 
					0,
					(void*)&pecs, 
					2, 
					100);

	if (status==2) { return pecs;} else return status;
	
}

int SMBTestAddressACK(unsigned int address) {
	int status;
	unsigned char res;

	status = libusb_control_transfer(device,
				LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
				SMB_TEST_ADDRESS_ACK,
				address, 
				0,
				(void*)&res, 
				1, 
				200);

	if (status ==1) { return res; } else {return status;}

}
int SMBTestCommandACK(unsigned int address, unsigned char command){
	int status;
	unsigned char res;
	status = libusb_control_transfer(device,
				LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
				SMB_TEST_COMMAND_ACK,
				address, 
				command,
				(void*)&res, 
				1, 
				100);

	if (status ==1) { return res; } else {return status;}

}
int SMBTestCommandWrite(unsigned int address, unsigned char command){
	int status;
	unsigned char res;
	status = libusb_control_transfer(device,
				LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
				SMB_TEST_COMMAND_WRITE,
				address, 
				command,
				(void*)&res, 
				1, 
				100);

	if (status ==1) { return res; } else {return status;}
}



void SMBSetDebugLogFunc(void *logFunc) {
	extLogFunc = logFunc;
}

const char* SMBGetErrorString(int errorCode) {
	char* errorMsgBuf = malloc(128);
	switch (errorCode) {
		case LIBUSB_ERROR_INVALID_PARAM:
			return "libusb error: Invalid parameter";
		case LIBUSB_ERROR_ACCESS:
			return "libusb error: Access denied (insufficient permissions?)";
		case LIBUSB_ERROR_NO_DEVICE:
			return "libusb error: No such device (it may have been disconnected)";
		case LIBUSB_ERROR_NOT_FOUND:
			return "libusb error: Entity not found";
		case LIBUSB_ERROR_BUSY:
			return "libusb error: Resource busy";
		case LIBUSB_ERROR_TIMEOUT:
			return "libusb error: Operation timed out";
		case LIBUSB_ERROR_OVERFLOW:
			return "libusb error: Overflow";
		case LIBUSB_ERROR_PIPE:
			return "libusb error: Pipe error";
		case LIBUSB_ERROR_INTERRUPTED:
			return "libusb error: System call interrupted (perhaps due to signal)";
		case LIBUSB_ERROR_NO_MEM:
			return "libusb error: Insufficient memory";
		case LIBUSB_ERROR_NOT_SUPPORTED:
			return "libusb error: Operation not supported or unimplemented on this platform";
		case LIBUSB_ERROR_OTHER:
			return "libusb error: Other error";
		case ERR_DEVICE_OPEN:
			return "Unable to open device. (insufficient permissions? connection issue?)";
		case ERR_ALREADY_OPEN:
			return "Device already in use";
		case ERR_CLAIM_INTERFACE:
			return "Unable to claim interface (insufficient permissions?)";
		default:	
			sprintf(errorMsgBuf,"Unknown libusb error code (%d)",errorCode);		
			return (const char*)errorMsgBuf;
	}
}