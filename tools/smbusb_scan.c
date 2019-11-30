/*
* smbusb_scan 
* Scanning tool for SMBus
*
* Copyright (c) 2016 Viktor <github@karosium.e4ward.com>
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
#include <getopt.h>
#include <sys/types.h>

#include "libsmbusb.h"

#if defined(_WIN32) || defined(_WIN64)
char* strsep(char** stringp, const char* delim)
{
  char* start = *stringp;
  char* p;

  p = (start != NULL) ? strpbrk(start, delim) : NULL;

  if (p == NULL)
  {
    *stringp = NULL;
  }
  else
  {
    *p = '\0';
    *stringp = p + 1;
  }

  return start;
}
#endif

#define SCAN_ADDRESS 1
#define SCAN_COMMAND 2
#define SCAN_COMMAND_WRITE 3

void printHeader() {

	  printf("------------------------------------\n");
	  printf("             smbusb_scan\n");
 	  printf("------------------------------------\n");
}
void printUsage() {
	  printHeader();
	  printf("options:\n");
	  printf("--address                , -a                  =   scan for address ACK (START, ADDR, STOP)\n");
	  printf("--command <addr>         , -c <addr>           =   scan for command ACK (START, ADDR, CMD, STOP)\n");
	  printf("--command-write <addr>   , -w <addr>           =   probe command writability for byte, word, block or more\n");
	  printf("--begin=<#>              , -b <#>              =   address or command to start scan at\n");
	  printf("--end=<#>                , -e <#>              =   address or command to end scan at\n");
	  printf("--skip=<#,#,...>         , -s <#,#,...>        =   comma delimited list of HEX addresses or commands to skip during scan\n");
	  printf("\n<addr> is always the read address in HEX\n");
}

void printSkipMap(int start, int end, unsigned char skipMap[]) {
	int i;
	char haveSkip=0;

	printf("Scan range: %02x - %02x\n",start,end);
	printf("Skipping: ");

	for (i=0;i<256;i++) { 
		if (skipMap[i] == 255) { 
			printf("%x ",i); 
			haveSkip=1;
		} 
	}
	if (haveSkip==0) printf("None");
	
	printf("\n------------------------------------\n");
}


int main(int argc, char **argv)
{                       
	unsigned char skipMap[256] = {0};
	int address=0;
	static int scanMode=0;

	char didAck=0;
	int start=0;
	int end=0xFF;

	int status;
	int c,i;
	unsigned char *token;

	if (argc==1) {
		 printUsage();
		 exit(1);
	}

	while (1)
	{
		static struct option long_options[] =
	        {
	          {"address", no_argument,       0, 'a'},
	          {"command", required_argument,       0, 'c'},
	 	  {"command-write", required_argument, 0,'w'},		
	 	  {"begin", required_argument, 0,'b'},
	 	  {"end", required_argument, 0,'e'},

	          {"skip",  required_argument, 0, 's'},

	          {0, 0, 0, 0}
        };

      int option_index = 0;

      c = getopt_long (argc, argv, "ac:w:s:e:b:h",
                       long_options, &option_index);

      if (c == -1)
        break;

      switch (c)
        {
        case 0:
          if (long_options[option_index].flag != 0)
            break;
	case 'a':
		scanMode = SCAN_ADDRESS;		
		break;
	case 'c':
		scanMode = SCAN_COMMAND;
		address = strtol(optarg,NULL,16);
		break;
	case 'w':
		scanMode = SCAN_COMMAND_WRITE;
		address = strtol(optarg,NULL,16);
		break;

        case 's':
	     while ((token = strsep(&optarg, ",")) != NULL)
		skipMap[strtol(token,NULL,16)] = 0xFF;
          break;

	case 'b':
		start=strtol(optarg,NULL,16);
	  break;
	case 'e':
		end=strtol(optarg,NULL,16);
	  break;
	case 'h':
        case '?':
		printUsage();
		exit(0);
          break;
        default:
	  abort;
        }
    }

	printHeader();	

	if ((status = SMBOpenDeviceVIDPID(SMB_DEFAULT_VID,SMB_DEFAULT_PID)) >0) {
		printf("Success. SMBusb Firmware Version: %d.%d.%d\n",status&0xFF,(status >>8)&0xFF,(status >>16)&0xFF);
	} else {
		printf("Error: %s\n",SMBGetErrorString(status));
		exit(0);
	}

	switch (scanMode) {
		case SCAN_ADDRESS:
			printf("Scanning for addresses..\n");
			skipMap[0xA0] = 0xFF; 	// reserved addresses belong to the FX2 config eeprom
			skipMap[0xA1] = 0xFF;   // messing with it like this can hang the device
			printSkipMap(start,end,skipMap);
		
			for(i=start;i<=end;i++) {
				if (skipMap[i] != 0xFF) { 
					status = SMBTestAddressACK(i);		
					if (status< 0) printf("[%x] ERROR: %d\n",i,status);
					if (status == 0xFF)  printf("[%x] ACK\n",i);
				}
			}
				
			break;
		case SCAN_COMMAND:
			printf("Scanning for commands..\n");
			printSkipMap(start,end,skipMap);
			for(i=start;i<=end;i++) {
				if (skipMap[i] != 0xFF) { 
					status = SMBTestCommandACK(address,i);
					if (status< 0) printf("[%x] ERROR: %d\n",i,status);
					if (status ==0xFF)  printf("[%x] ACK\n",i);
				}
			}
				
				
	
			break;
		case SCAN_COMMAND_WRITE:
			printf("Scanning for command writability..\n");
			printSkipMap(start,end,skipMap);
			for(i=start;i<=end;i++) {
				if (skipMap[i] != 0xFF) {
					status = SMBTestCommandWrite(address,i);
					if (status< 0) {
						 printf("[%x] ERROR: %d\n",i,status);
					} else {					
		                        	if (status>0) { 
							didAck=1;
							printf("[%x] ACK",i);
						}
						if (status>1) {
							printf(", Byte writable");
						}
						if (status>2) {
							printf(", Word writable");
						}
						if (status>3) {
							printf(", Block writable");
						}
						if (status>4) {
							printf(", >Block writable");
						}					
					}
					if (didAck) {
						printf("\n");
						didAck=0;
					}
                                	
				}
			}

			break;
		default:
			printUsage();
			exit(1);
		
	}	

}
