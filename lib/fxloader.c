/*
* Rudimentary firmware loader for the Cypress FX series
*
* Copyright (c) 2016 Viktor <github@karosium.e4ward.com>
*
* parse_hex_line() courtesy of Paul Stoffregen, paul@ece.orst.edu
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libusb.h"
#include "fxloader.h"

static unsigned int reset_address = 0xE600;

int parse_hex_line(char *theline, char bytes[], int *addr, int *num,  int *code)
{
	int sum, len, cksum;
	char *ptr;

	*num = 0;
	if (theline[0] != ':') return 0;
	if (strlen(theline) < 11) return 0;

	ptr = theline+1;
	if (!sscanf(ptr, "%02x", &len)) return 0;
	ptr += 2;
	if ( strlen(theline) < (11 + (len * 2)) ) return 0;
	if (!sscanf(ptr, "%04x", addr)) return 0;
	ptr += 4;
	//  printf("Line: length=%d Addr=%d\n", len, *addr); 
	if (!sscanf(ptr, "%02x", code)) return 0;
	ptr += 2;
	sum = (len & 255) + ((*addr >> 8) & 255) + (*addr & 255) + (*code & 255);
	while(*num != len) {
		if (!sscanf(ptr, "%02x", (int *)&bytes[*num])) return 0;
		ptr += 2;
		sum += bytes[*num] & 255;
		(*num)++;
		if (*num >= 256) return 0;
	}
	if (!sscanf(ptr, "%02x", &cksum)) return 0;
	if ( ((sum & 255) + (cksum & 255)) & 255 ) return 0; /* checksum error */
	return 1;
}

void CypressSetResetAddress(unsigned int address) {
	reset_address = address;
}
int CypressWriteRam(libusb_device_handle *device,unsigned int addr, unsigned char *buf, unsigned int len) {
	int status;

	status = libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					0xA0,
					addr&0xFFFF, 
					addr>>16,
					buf, 
					len, 
					1000);
	return status;

}

int CypressReset(libusb_device_handle *device,unsigned char suspended) {
	return CypressWriteRam(device,reset_address,&suspended,1);
}

int CypressUploadIhxFirmware(libusb_device_handle *device, char *buf, unsigned int len) {
	char bytes[256];
	char *tempBuf;

	char *line="";
	int i,addr,num,code;

	tempBuf = malloc(len+1);
	tempBuf[len] = 0;
	memcpy(tempBuf,buf,len);

	line = strtok(tempBuf,"\n");	

	i=CypressReset(device,1);

	if (i<0) {
		free(tempBuf);
	 	return i;
	}
	while ((line != NULL) && (code !=1)) {
		parse_hex_line(line,bytes,&addr,&num,&code);

		if (code==0) {
			i=CypressWriteRam(device,addr,(unsigned char*)&bytes,num);
			if (i<0) {
				free(tempBuf);
		 		return i;
			}

		}
		if (code !=1)
			line = strtok(NULL,"\n");		
	}
	free(tempBuf);
	i=CypressReset(device,0);
	if (i<0) {
		 return i;
	}
	return 1;
}