/*
* smbusb_comm
* SMBus communicator
*
* Copyright (c) 2016 Viktor <github@karosium.e4ward.com>
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

void printHeader() {

	  printf("------------------------------------\n");
	  printf("            smbusb_comm\n");
 	  printf("------------------------------------\n");
}
void printUsage() {
	  printHeader();
	  printf("usage:\n");
	  printf("--address=<0xaddr>    , -a <0xaddr>    =   Sets SMBus address for operation\n");
	  printf("--command=<0xcommand> , -c <0xcommand> =   Sets SMBus command for operation\n");
	  printf("--write=<0xdata>      , -w <0xdata>    =   Write operation\n");
	  printf("--read=<# of bytes>   , -r <#>         =   Read operation (# of bytes determines read mode)\n");
	  printf("--null-write          , -n             =   Start->addr->cmd->Stop\n");
	  printf("--verbose             , -v             =   Print status messages\n");
	  printf("--no-pec                               =   Disable SMBus Packet Error Checking\n");
	  printf("examples:\n");
	  printf("smbusb_comm -a 0x16 -c 0 -r 2 -s       =   Word Read Command 0 (Manufacturer Access in SBS)\n");
	  printf("smbusb_comm -a 0x16 -c F -w 41ef010102 =   Block Write Command 0xF (0x is always optional)\n");

}

char* hex_decode(const char *in, int len,char *out)
{
        unsigned int i, t, hn, ln;

        for (t = 0,i = 0; i < len; i+=2,++t) {

                hn = in[i] > '9' ? in[i] - 'a' + 10 : in[i] - '0';
                ln = in[i+1] > '9' ? in[i+1] - 'a' + 10 : in[i+1] - '0';

                out[t] = (hn << 4 ) | ln;
        }

        return out;
}

int main(int argc, char **argv)
{                       
	int c;
	static int noPec=0;
	char verbose = 0;
	char block[1024];	
	char block2[1024];	

	char buf[256];
	char blockCheck;

	int op=0;
	int opAddress,opCommand = -1;
	int opReadLen=0;
	int status;
	int i,j;

	if (argc==1) {
		 printUsage();
		 exit(1);
	}

	
	while (1)
	{
		static struct option long_options[] =
	        {
	 	  {"no-pec", no_argument,       &noPec, 1},		

	          {"address",  required_argument, 0, 'a'},
	          {"command",  required_argument, 0, 'c'},
	          {"write",    required_argument, 0, 'w'},
	          {"read",    required_argument, 0, 'r'},
	          {"null-write",  no_argument, 0, 'n'},
	          {"verbose",  no_argument, 0, 'v'},

	          {0, 0, 0, 0}
        };

      int option_index = 0;

      c = getopt_long (argc, argv, "a:c:w:r:v", //s
                       long_options, &option_index);

      if (c == -1)
        break;

      switch (c)
        {
        case 0:
          if (long_options[option_index].flag != 0)
            break;

        case 'a':
          	opAddress = strtol(optarg,NULL,16);
          break;

        case 'c':
		opCommand = strtol(optarg,NULL,16);
          break;

        case 'w':
		op=1;
		strcpy(block,optarg);
          break;

        case 'r':
		op=2;
          	opReadLen=strtol(optarg,NULL,10);
          break;
	case 'n':
		op=3;
	  break;
	case 'v':
		verbose=1;
	  break;
        case '?':
		printUsage();
		exit(0);
          break;
        default:
	  abort;
        }
    }

	if (verbose) printHeader();	

	if ((status = SMBOpenDeviceVIDPID(0x04b4,0x8613)) >0) {
		if (verbose) printf("SMBusb Firmware Version: %d.%d.%d\n",status&0xFF,(status >>8)&0xFF,(status >>16)&0xFF);
	} else {
		printf("Error Opening SMBusb: libusb error %d\n",status);
		exit(-1);
	}

	if (noPec) {
		SMBEnablePEC(0);
		if (verbose) printf("PEC is DISABLED\n");
	} else {
		SMBEnablePEC(1);
		if (verbose) printf("PEC is ENABLED\n");

	}

	if (verbose) printf("-----------------------------\n");

	if (opAddress == -1 | opCommand == -1) {
		printf("Missing address or command\n");
		exit(-5);
	}
	if (opReadLen == 0 && strcmp("",block)==0) {
		printf("Missing read length or write data\n");
		exit(-5);
	}
	if (op==1) {
		if (strlen(block)>0) {
			if (strlen(block) % 2 != 0) {
				printf("Write data must be an even number of characters (hex bytes)\n");
				exit(-5);			
			}
			if (block[0]=='0' && (block[1]=='x' || block[1]=='X')){
				strncpy(block2,block+2,254);
				strcpy(block,block2);
			}
			for(i=0;i<strlen(block);i++) {
				blockCheck=tolower(block[i]);
				if (!((blockCheck >=48 && blockCheck <=57) || (blockCheck >=97 && blockCheck<=102))){
					printf("Write data contains invalid characters. Only hex is allowed\n");
					exit(-5);
				}
				block[i]=blockCheck;
			}
			hex_decode(block,strlen(block),buf);
			if (verbose) printf("Writing %d bytes to addr 0x%02x cmd 0x%02x\n",strlen(block)/2,opAddress,opCommand);
	
			if (strlen(block)/2==1) {
				SMBWriteByte(opAddress,opCommand,buf[0]);				
			} else if(strlen(block)/2==2) {
				j=*((int*)buf) & 0xFFFF;
				j= ((j>>8) | (j<<8)) &0xFFFF;

				status = SMBWriteWord(opAddress,opCommand,j);
				if (status >=0) {
					if (verbose) printf("OK\n");
					exit(0);
				} else {
					printf("Error %d\n",status);
					exit(status);
				}
			} else {
				SMBWriteBlock(opAddress,opCommand,buf,strlen(block)/2);
			}
			
		}		
		
	} else if (op==3) {
		SMBSendByte(opAddress,opCommand);
	} else if (op==2) {
		if (opReadLen ==1) {
			status = SMBReadByte(opAddress,opCommand);
				if (status >=0) {
					if (verbose) printf("OK\n");
					printf("%02x\n",status);
					exit(0);
				} else {
					printf("Error %d\n",status);
					exit(status);
				}
 
		} else if(opReadLen ==2) {
			status = SMBReadWord(opAddress,opCommand);
				if (status >=0) {
					if (verbose) printf("OK\n");
			
					printf("%04x\n",status);
					exit(0);
				} else {
					printf("Error %d\n",status);
					exit(status);
				}
		} else  {

			status = SMBReadBlock(opAddress,opCommand,buf);
				if (status >=0) {
					if (verbose) printf("OK. Read %d bytes\n",status);
					for (i=0;i<status;i++) {
						printf("%02x", buf[i]);
					}
					printf("\n");
					exit(0);
				} else {
					printf("Error %d\n",status);
					exit(status);
				}
			
		}
	}
	


}
