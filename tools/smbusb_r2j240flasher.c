/*
* smbusb_r2j240flasher
* Flasher tool for the R2J240 (and bootrom-compatible) chips
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

#define CMD_SBS_CHEMISTRY 0x22

#define CMD_READ_RAM 0xFC

#define CMD_WRITE_RAM 0xFD

#define CMD_ERASE_BLOCK 0x20 

#define CMD_READ_CLEAR_STATUS_REG 0x50

#define CMD_EXECUTE_FLASH 0xD001

#define FIRMWARE_ADDRESS 0x4000
#define FIRMWARE_SIZE 0x8000

#define DATAFLASH1_ADDRESS 0x3000
#define DATAFLASH1_SIZE 0x400

#define DATAFLASH2_ADDRESS 0x3400
#define DATAFLASH2_SIZE 0x400

#define DATAFLASH3_ADDRESS 0xC000
#define DATAFLASH3_SIZE 0x2000


int eraseFlashBlock(unsigned int address) {
	int status;
	unsigned char block[3];

	block[0]=address&0xFF;
	block[1]=(address>>8)&0xFF;
	block[2]=(address>>16)&0xFF;
	status = SMBWriteBlock(0x16,CMD_ERASE_BLOCK,block,3);
	if (status<0) return status;
	sleep(1);
	status = SMBReadWord(0x16,CMD_READ_CLEAR_STATUS_REG);
	return status;
}


int readRam(int address, unsigned int size, unsigned char *buf) {
	int status,i;
	unsigned char block[0xFF];

	block[0]=0x16;
	block[1]=CMD_READ_RAM;
	block[2]=address&0xFF;
	block[3]=(address>>8)&0xFF;
	block[4]=(address>>16)&0xFF;
	block[5]=size&0xFF;
	block[6]=(size>>8)&0xFF;
	status= SMBWrite(1,0,0,block,7);
	if (status <0) return status;

	block[0]=0x17;
	status= SMBWrite(0,1,0,block,1);
	
	return SMBRead(size,buf,1);
}

int writeRam(int address, unsigned int size, unsigned char *buf) {
	int status,i;
	unsigned char block[0xFF];

	block[0]=0x16;
	block[1]=CMD_WRITE_RAM;
	block[2]=address&0xFF;
	block[3]=(address>>8)&0xFF;
	block[4]=(address>>16)&0xFF;
	block[5]=size&0xFF;
	block[6]=(size>>8)&0xFF;
	status= SMBWrite(1,0,0,block,7);

	if (status <0) return status;

	status= SMBWrite(0,0,1,buf,size);
	
	return status;
	
}

void printHeader() {

	  printf("------------------------------------\n");
	  printf("        smbusb_r2j240flasher\n");
 	  printf("------------------------------------\n");
}
void printUsage() {
	  printHeader();
	  printf("options:\n");
	  printf("--dump=<file> ,  -d <file>              =   dump the ram (or flash, which is mapped in ram) to <file>\n");
	  printf("--write=<file> ,   -w <file>            =   write the <file> to the ram (or flash)\n");
	  printf("--address=0x<addr> , -a 0x<addr>        =   ram address from which read or write starts\n");
	  printf("--size=0x<size> ,  -s 0x<size>          =   size of data to read or write\n");
	  printf("--preset=<preset> , -p <preset>         =   sets address and size based on a preset, see below.\n");
	  printf("--execute                               =   exit the Boot ROM and execute firmware\n");
	  printf("--no-verify                             =   skip verification after flashing (not recommended)\n");
	  printf("--fix-lgc-static-checksum               =   adds fixed checksum to end of data (LGC algo.)\n");
	  printf("                                            (use when flashing modified static data)\n");
	  printf("\n");
	  printf("Presets:\n");
	  printf("df1                                     =   DataFlash1 (usually dynamic data to persist between resets)\n");
	  printf("df2                                     =   DataFlash2 (usually dynamic data to persist between resets)\n");
	  printf("df3                                     =   DataFlash3 (usually static data)\n");
	  printf("fw                                      =   The firmware\n");
	  printf("full1                                   =   0-FFFF first half of a full dump\n");
	  printf("full2                                   =   10000-11FFF second half of full dump\n");
	  printf("                                            Note: this is probably the boot rom and is unwritable\n");
	  printf("                                                  full1 will be enough for most puproses\n");
	 

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

	static int lgcChecksumFix=0;
	static int opExecute=0;
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
	          {"execute",  no_argument, &opExecute,1},
		  {"erase",  no_argument,&opErase,1},
		  {"fix-lgc-static-checksum", no_argument, &lgcChecksumFix,1},

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
		if (strcmp(optarg,"df1")==0) {
			opAddress=DATAFLASH1_ADDRESS;
			opSize=DATAFLASH1_SIZE;
		} else if (strcmp(optarg,"df2")==0) {
			opAddress=DATAFLASH2_ADDRESS;
			opSize=DATAFLASH2_SIZE;
		} else if (strcmp(optarg,"df3")==0) {
			opAddress=DATAFLASH3_ADDRESS;
			opSize=DATAFLASH3_SIZE;
		} else if (strcmp(optarg,"full1")==0) {
			opAddress=0;
			opSize=0x10000;
		} else if (strcmp(optarg,"full2")==0) {
			opAddress=0x10000;
			opSize=0x2000;
		} else if (strcmp(optarg,"fw")==0) {
			opAddress=FIRMWARE_ADDRESS;
			opSize=FIRMWARE_SIZE;
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

	SMBEnablePEC(0); // Renesas BootROM does not support PEC :(

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
		printf("Dumping memory 0x%04x-0x%04x ...\n",opAddress,opAddress+opSize-1);
		outFile=fopen(ramOut,"wb");
		if (outFile == NULL) {
			printf("Error opening output file\n");
			exit(3);
		}

		status=readRam(opAddress,opSize,block);
		
		if (status >0) {			
			printf("Done!\n");
		} else {
			printf("Error %d\n",status);
		}

		fwrite(block,opSize,1,outFile);

		fclose(outFile);
		
	}
	
	if (ramIn !=NULL) {
		if (!confirmDelete) {
			printf("This may erase and reprogram flash memory on the microcontroller.\nIf you're sure add --confirm-delete and try again.\n");
			exit(0);
		}
		printf("Erasing flash block starting at 0x%04x ...\n",opAddress);
		if (eraseFlashBlock(opAddress)>0) {
			printf("Done!\n");
		} else {
			printf("ERROR!\n");
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

		if (lgcChecksumFix) {
			chk=0;
			for(j=0;j<(opSize/4)-1;j++) {
				chk -= *((uint32_t *)block+j);
			}
		
			*((uint32_t *)block+j) = chk;
			printf("Fixing LGC static checksum..\nDone!\n");
		}

		printf("Writing memory 0x%04x-0x%04x ...\n",opAddress,opAddress+opSize-1);

		writeRam(opAddress,opSize,block);	

		fprintf(stderr,"Done!\n");

		if (!noVerify) {
			printf("Verifying 0x%04x-0x%04x ...\n",opAddress,opAddress+opSize-1);
			readRam(opAddress,opSize,block2);
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
			printf("This may erase and reprogram flash memory on the microcontroller.\nIf you're sure add --confirm-delete and try again.\n");
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
	if (opExecute) {
		printf("Exiting Boot ROM and starting firmware\n");
		SMBWriteWord(0x16,0,CMD_EXECUTE_FLASH);
	}

}
