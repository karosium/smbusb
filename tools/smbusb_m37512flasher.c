/*
* smbusb_m37512flasher
* Flasher tool for the M37512
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

#define CMD_SET_READ_ADDRESS 0xFF

#define CMD_READ_BLOCK 0xFE

#define CMD_WRITE_BLOCK 0x40
#define CMD_CLEAR_BLOCK 0x20

#define CMD_READ_CLEAR_STATUS_REGISTER 0x70

#define CHUNKLEN 0x10

#define CMD_SBS_CHEMISTRY 0x22

#define BLOCK_B_ADDRESS 0x1000
#define BLOCK_B_SIZE 0x800

#define BLOCK_A_ADDRESS 0x1800
#define BLOCK_A_SIZE 0x800

#define BLOCK_3_ADDRESS 0x4000
#define BLOCK_3_SIZE 0x4000

#define BLOCK_2_ADDRESS 0x8000
#define BLOCK_2_SIZE 0x4000

#define BLOCK_1_ADDRESS 0xC000
#define BLOCK_1_SIZE 0x2000

#define BLOCK_0_ADDRESS 0xE000
#define BLOCK_0_SIZE 0x2000

int readClearStatusRegister() {
	return SMBReadByte(0x16,CMD_READ_CLEAR_STATUS_REGISTER);
}

int eraseFlashBlock(int address) {
	int status;
	unsigned char setAddr[2];

	setAddr[0] = (address >> 8) & 0xFF;	// flipping byte order is fun (thought the designers)
	setAddr[1] = address & 0xFF;

	status = SMBWriteBlock(0x16,CMD_CLEAR_BLOCK,setAddr,2);
	if (status>=0) {
		sleep(1);
		readClearStatusRegister();
	}

	return status;


}

int readFlash(int address, int len, unsigned char* buf) {
	int status,i;
	unsigned char chunk[CHUNKLEN];
	unsigned char setAddr[2];

	if (len % CHUNKLEN !=0) return -99;

	for (i=0;i<len/CHUNKLEN;i++) {
		setAddr[0] = (address+i*CHUNKLEN) & 0xFF;    	
		setAddr[1] = ((address+i*CHUNKLEN) >> 8) & 0xFF;	

		status=SMBWriteBlock(0x16,CMD_SET_READ_ADDRESS,setAddr,2);
		if (status != 2) return -1;

		status=SMBReadBlock(0x16,CMD_READ_BLOCK,chunk);		

		if (status != CHUNKLEN) return status;
		memcpy((buf+(i*CHUNKLEN)),chunk,CHUNKLEN);
	}	
	
	return len;
}
int writeFlash(int address, int len, unsigned char* buf) {
	int status,i;
	unsigned char chunk[CHUNKLEN+2];

	if (len % CHUNKLEN !=0) return -99;

	for (i=0;i<len/CHUNKLEN;i++) {
		memcpy(chunk+2,buf+(i*CHUNKLEN),CHUNKLEN);

		chunk[0] = (address+(i*CHUNKLEN)) & 0xFF;
		chunk[1] = ((address+(i*CHUNKLEN)) >> 8) & 0xFF;	

		readClearStatusRegister();
		status=SMBWriteBlock(0x16,CMD_WRITE_BLOCK,chunk,CHUNKLEN+2);		
		usleep(2000);

		if (status != CHUNKLEN+2) return status;		
	}	
	
	return len;
}


void printHeader() {

	  printf("------------------------------------\n");
	  printf("        smbusb_m37512flasher\n");
 	  printf("------------------------------------\n");
}

void printUsage() {
	  printHeader();
	  printf("options:\n");
	  printf("--dump=<file> ,  -d <file>              =   dump the flash to <file>\n");
	  printf("--write=<file> ,   -w <file>            =   write the <file> to the flash\n");
	  printf("--erase                                 =   just erase the flash block\n");
	  printf("--address=0x<addr> , -a 0x<addr>        =   ram address from which read or write starts\n");
	  printf("--size=0x<size> ,  -s 0x<size>          =   size of data to read or write\n");
	  printf("--preset=<preset> , -p <preset>         =   sets address and size based on a preset, see below.\n");
	  printf("--no-verify                             =   skip verification after flashing (not recommended)\n");
	  printf("\n");
	  printf("Presets:\n");
	  printf("bb                                      =   Data Block B\n");
	  printf("ba                                      =   Data Block A\n");
	  printf("b3                                      =   Block 3\n");
	  printf("b2                                      =   Block 2\n");
	  printf("b1                                      =   Block 1\n");
	  printf("b0                                      =   Block 0\n");
	 

}
void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

int main(int argc, char **argv)
{                       
	char *ramIn = NULL;
	char *ramOut= NULL;

	int c;
	static int noVerify=0;
	static int confirmDelete=0;

	static int opErase=0;
	int opAddress=-1;
	int opSize=-1;

	unsigned char block[0x1FFFF];
	unsigned char block2[0x1FFFF];

	int status;
	int i,j,chk;
	FILE *outFile;
	FILE *inFile;

	if (argc==1) {
		 printUsage();
		 exit(1);
	}

	while (1)
	{
		static struct option long_options[] =
	        {
	          {"confirm-delete", no_argument,       &confirmDelete, 1},
	          {"no-verify", no_argument,       &noVerify, 1},
		  {"erase",  no_argument,&opErase,1},		  

	          {"address",    required_argument, 0, 'a'},
	          {"dump",	required_argument, 0, 'd'},
	          {"size",    required_argument, 0, 's'},
	          {"write",  required_argument, 0, 'w'},

		  {"preset", required_argument,0,'p'},
	          {0, 0, 0, 0}
        };

      int option_index = 0;

      c = getopt_long (argc, argv, "d:a:s:w:p:e",
                       long_options, &option_index);

      if (c == -1)
        break;

      switch (c)
        {
        case 0:
          if (long_options[option_index].flag != 0)
            break;

        case 'a':
          	opAddress=strtol(optarg,NULL,16);	
          break;

        case 'd':
          	ramOut=optarg;
          break;

        case 'w':
          	ramIn=optarg;
          break;
	case 'p':
		if (strcmp(optarg,"bb")==0) {
			opAddress=BLOCK_B_ADDRESS;
			opSize=BLOCK_B_SIZE;
		} else if (strcmp(optarg,"ba")==0) {
			opAddress=BLOCK_A_ADDRESS;
			opSize=BLOCK_A_SIZE;
		} else if (strcmp(optarg,"b3")==0) {
			opAddress=BLOCK_3_ADDRESS;
			opSize=BLOCK_3_SIZE;
		} else if (strcmp(optarg,"b2")==0) {
			opAddress=BLOCK_2_ADDRESS;
			opSize=BLOCK_2_SIZE;
		} else if (strcmp(optarg,"b1")==0) {
			opAddress=BLOCK_1_ADDRESS;
			opSize=BLOCK_1_SIZE;
		} else if (strcmp(optarg,"b0")==0) {
			opAddress=BLOCK_0_ADDRESS;
			opSize=BLOCK_0_SIZE;
		}


	  break;
        case 's':
          	opSize=strtol(optarg,NULL,16);
          break;

        case '?':
		printUsage();
		exit(0);
          break;
        default:
	  abort;
        }
    }

	printHeader();	
	if ((status = SMBOpenDeviceVIDPID(0x04b4,0x8613)) >0) {
		printf("SMBusb Firmware Version: %d.%d.%d\n",status&0xFF,(status >>8)&0xFF,(status >>16)&0xFF);
	} else {
			
		printf("Error Opening SMBusb: libusb error %d\n",status);
		exit(0);
	}

	SMBEnablePEC(0);  // Renesas BootROM does not support PEC :(

	memset(block,0,255);			
	status = SMBReadBlock(0x16,CMD_SBS_CHEMISTRY,block); // read SBS Chemistry.. should return "LION" if running firmware

	if (status == 4) {
		printf("Error communicating with the Boot ROM.\nChip is running firmware\n");		
		printf("Note that there's no universal way to enter the Boot ROM on a programmed chip.\n");
		printf("The command(s) and password(s) vary by make and model of the pack.\n");	
		exit(1);
	}

	printf("------------------------------------\n");

	if (ramOut != NULL) {
		if (opAddress == -1 | opSize==-1) {
			printf("Address or size missing for operation.\n");
			exit(2);
		}
		if (opSize % CHUNKLEN !=0) {
			printf("Size must be multiple of %d\n",CHUNKLEN);
			exit(2);
		}

		printf("Dumping memory 0x%04x-0x%04x ...\n",opAddress,opAddress+opSize-1);
		outFile=fopen(ramOut,"wb");
		if (outFile == NULL) {
			printf("Error opening output file\n");
			exit(3);
		}

		status=readFlash(opAddress,opSize,block);
		
		if (status >0) {			
			printf("Done!\n");
		} else {
			printf("Read Error %d\n",status);
		}

		fwrite(block,opSize,1,outFile);

		fclose(outFile);
		
	}
	
	if (ramIn !=NULL) {		
		if (opSize % CHUNKLEN !=0) {
			printf("Size must be multiple of %d\n",CHUNKLEN);
			exit(2);
		}

		if (!confirmDelete) {
			printf("This will erase and reprogram flash memory on the microcontroller.\nIf you're sure add --confirm-delete and try again.\n");
			exit(0);
		}

		inFile = fopen(ramIn,"rb");

		if (inFile != NULL) { 
			fseek(inFile, 0L, SEEK_END);
			i = ftell(inFile);
			rewind(inFile);	
			if (i != opSize) {
				printf("File size does not match defined size\n");
				exit(4);
			}
		} else {
			printf("Error opening input file\n");
			exit(3);
		}

		fread(block,opSize,1,inFile);

		printf("Erasing flash block starting at 0x%04x ...\n",opAddress);

		if (eraseFlashBlock(opAddress)>0) {
			printf("Done!\n");
		} else {
			printf("ERROR!\n");
			exit(0);
		}

		printf("Writing memory 0x%04x-0x%04x ...\n",opAddress,opAddress+opSize-1);
		status=writeFlash(opAddress,opSize,block);

		if (status>0) {
			fprintf(stderr,"Done!\n");
		} else {
			fprintf(stderr,"Write ERROR %d\n",status);

		}

		if (!noVerify) {
			printf("Verifying 0x%04x-0x%04x ...\n",opAddress,opAddress+opSize-1);
			status = readFlash(opAddress,opSize,block2);
			if (status < 0) {
				printf("Read ERROR %d\n",status);
				exit(0);
			}


			if (i=memcmp(block,block2,opSize) == 0) {
					printf("Verified OK!\n");

			} else {
					printf("Verify FAIL at 0x%04x\n",i);
			}
		}		

		fclose(inFile);		
		
	}
	if (opErase) {
		if (!confirmDelete) {
			printf("This will erase flash memory on the microcontroller.\nIf you're sure add --confirm-delete and try again.\n");
			exit(0);
		}
		printf("Erasing flash block starting at 0x%04x ...\n",opAddress);
		if (eraseFlashBlock(opAddress)>0) {
			printf("Done!\n");
		} else {
			printf("ERROR!\n");
			exit(0);
		}
	}
}

