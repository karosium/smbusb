/*
* smbusb_bootstrap
* Alternative VID/PID bootstrap tool for SMBusb
*
* Copyright (c) 2019 Viktor <github@karosium.e4ward.com>
*
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <getopt.h>

#include "libsmbusb.h"

void printHeader() {

	  printf("------------------------------------\n");
	  printf("          smbusb_bootstrap\n");
 	  printf("------------------------------------\n");
}

void printUsage() {
	  printHeader();
	  printf("Upload the SMBusb firmware to a Cypress device with a different VID:PID than the devboard\n");
	  printf("\n");
	  printf("usage: ./smbusb_bootstrap -d VID:PID\n");
	  printf("eg: ./smbusb_bootstrap -d 04B4:1234\n");

}

int main(int argc, char*argv[])
{
	int status;
	int i, c;
	char* tmp;
	int vid = 0;
	int pid = 0; 

	unsigned char *tempStr = malloc(256);
	unsigned int tempWord;

	static struct option long_options[] =
        {
          {"device",  required_argument, 0, 'd'},
	          {0, 0, 0, 0}
       	};

	while (1)
	{

	      int option_index = 0;

	      c = getopt_long (argc, argv, "d:",
                       long_options, &option_index);

	      if (c == -1)
	        break;

	      switch (c)
	        {
	        case 0:
	          if (long_options[option_index].flag != 0)
	            break;

        	case 'd':
			tmp = strtok(optarg,":");
			vid = strtoul(tmp,NULL,16);
			tmp = strtok(NULL,":");
			if (tmp != NULL)
				pid=strtoul(tmp,NULL,16);
		break;	

		}
	}

	if ((vid==0) | (pid==0)) {
		printUsage();
		exit(0);
	}
	printHeader();

	printf("Initializing device: %04X:%04X\n",vid,pid);

	SMBOpenDeviceVIDPID(vid,pid);
	SMBCloseDevice();

	if ((status = SMBOpenDeviceVIDPID(SMB_DEFAULT_VID,SMB_DEFAULT_PID)) >0) {
		printf("Success. SMBusb Firmware Version: %d.%d.%d\n",status&0xFF,(status >>8)&0xFF,(status >>16)&0xFF);
	} else {
		printf("Error: %s\n",SMBGetErrorString(status));
		exit(0);
	}
}
