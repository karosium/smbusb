/*
* smbusb_bq8030flasher
* Flasher tool for the BQ8030 series chips
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

#define CMD_SET_PROGRAM_BLOCK_ADDRESS 0x0
#define CMD_READ_PROGRAM_BLOCK 0x2
#define CMD_WRITE_PROGRAM_BLOCK 0x5
#define CMD_ERASE_PROGRAM_FLASH 0x7

#define CMD_EXECUTE_FLASH 0x08

#define CMD_SET_EEPROM_ADDRESS 0x9
#define CMD_READ_EEPROM_BLOCK 0xC
#define CMD_WRITE_EEPROM_BLOCK 0x10
#define CMD_ERASE_EEPROM_FLASH 0x12

#define CMD_BOOTROM_VERSION 0x0D

#define CMD_SBS_CHEMISTRY 0x22

#define DATA_ERASE_CONFIRM 0x83DE

#define EEPROM_BASE_ADDR 0x4000

#define PROGRAM_BLOCKSZ 0x60
#define EEPROM_BLOCKSZ 0x20

#define PROGRAM_BLOCK_COUNT 768
#define EEPROM_BLOCK_COUNT 64
#define EEPROM_RESERVED_BYTES 64

void eraseProgramFlash() {
	SMBWriteWord(0x16,CMD_ERASE_PROGRAM_FLASH,DATA_ERASE_CONFIRM);
	sleep(1);
}

void eraseEepromFlash() {
	SMBWriteWord(0x16,CMD_ERASE_EEPROM_FLASH,DATA_ERASE_CONFIRM);
	sleep(1);
}

int readProgramBlock(int blockNr, unsigned char* buf) {
	int status,i;
	unsigned char setAddr[3];

	setAddr[0] = blockNr & 0xFF;
	setAddr[1] = (blockNr >> 8) & 0xFF;
	setAddr[2] = (blockNr >> 16) & 0xFF;

	status=SMBWriteBlock(0x16,CMD_SET_PROGRAM_BLOCK_ADDRESS,setAddr,3);
	if (status != 3) return -1;
	
	status=SMBReadBlock(0x16,CMD_READ_PROGRAM_BLOCK,buf);
	
	return status;
}

int writeProgramBlock(int blockNr, unsigned char* buf) {
	int status;
	unsigned char block[0x62];

	block[0]=blockNr&0xFF;
	block[1]=(blockNr >>8) &0xFF;
	memcpy(block+2,buf,0x60);

        status = SMBWriteBlock(0x16,CMD_WRITE_PROGRAM_BLOCK,block,0x62);
	usleep(200000);

	return (status > 0 ? status-2 : status);
}

int writeEepromBlock(unsigned char blockNr, unsigned char* buf) {
	int status;
	unsigned char block[33];
	
	block[0]=blockNr;
	memcpy(block+1,buf,32);

        status = SMBWriteBlock(0x16,CMD_WRITE_EEPROM_BLOCK,block,33);
	usleep(2000);	
	return (status > 0 ? status-1 : status);
}


int readEepromBlock(unsigned char blockNr, unsigned char* buf) {
	int status,i;

	status=SMBWriteWord(0x16,CMD_SET_EEPROM_ADDRESS,EEPROM_BASE_ADDR+(blockNr*32)); 
	if (status <0) return -1;
	
	status=SMBReadBlock(0x16,CMD_READ_EEPROM_BLOCK,buf);
	
	return status;

}

void printHeader() {

	  printf("------------------------------------\n");
	  printf("        smbusb_bq8030flasher\n");
 	  printf("------------------------------------\n");
}
void printUsage() {
	  printHeader();
	  printf("options:\n");
	  printf("--save-program=<file> ,  -p <file>          =   save the chip's program flash to <file>\n");
	  printf("--save-eeprom=<file> ,   -e <file>          =   save the chip's eeprom(data) flash to <file>\n");
	  printf("--flash-program=<file> , -f <file>          =   flash the <file> to the chip's program flash\n");
	  printf("--flash-eeprom=<file> ,  -w <file>          =   flash the <file> to the chip's eeprom(data) flash\n");

	  printf("--execute                                   =   exit the Boot ROM and execute program flash\n");
	  printf("--no-verify                                 =   skip verification after flashing (not recommended)\n");
	  printf("--no-pec                                    =   disable SMBus Packet Error Checking (not recommended)\n");
}

int main(int argc, char **argv)
{                       
	char *programIn = NULL;
	char *programOut= NULL;
	char *eepromIn= NULL;
	char *eepromOut= NULL;
	int c;
	static int noVerify=0;
	static int noPec=0;
	static int confirmDelete=0;
	static int execute=0;
	unsigned char block[256];
	unsigned char block2[256];

	int status;
	int i,j;

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
	 	  {"no-pec", no_argument,       &noPec, 1},		
	          {"execute",    no_argument, &execute,1},

	          {"save-program",  required_argument, 0, 'p'},
	          {"save-eeprom",  required_argument, 0, 'e'},
	          {"flash-program",    required_argument, 0, 'f'},
	          {"flash-eeprom",    required_argument, 0, 'w'},

	          {0, 0, 0, 0}
        };

      int option_index = 0;

      c = getopt_long (argc, argv, "p:e:w:f:",
                       long_options, &option_index);

      if (c == -1)
        break;

      switch (c)
        {
        case 0:
          if (long_options[option_index].flag != 0)
            break;

        case 'p':
          	programOut=optarg;
          break;

        case 'e':
          	eepromOut=optarg;
          break;

        case 'f':
          	programIn=optarg;	
          break;

        case 'w':
          	eepromIn=optarg;
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

	if ((programOut != NULL | eepromOut != NULL) & (programIn != NULL | eepromIn != NULL)) {
		printf("Write and read during the same run is not supported!\n");
		exit(1);
	}

	if ((status = SMBOpenDeviceVIDPID(SMB_DEFAULT_VID,SMB_DEFAULT_PID)) >0) {
		printf("SMBusb Firmware Version: %d.%d.%d\n",status&0xFF,(status >>8)&0xFF,(status >>16)&0xFF);
	} else {
		printf("Error: %s\n",SMBGetErrorString(status));
		exit(0);
	}

	if (noPec) {
		SMBEnablePEC(0);
		printf("PEC is DISABLED\n");
	} else {
		SMBEnablePEC(1);
		printf("PEC is ENABLED\n");

	}

	memset(block,0,256);			// read SBS Chemistry.. should return "LION" if running firmware
	status = SMBReadBlock(0x16,CMD_SBS_CHEMISTRY,block);

	if (status == 4) {
		printf("Error communicating with the Boot ROM.\nChip is running firmware\n");		
		printf("Note that there's no universal way to enter the Boot ROM on a programmed chip.\n");
		printf("The command(s) and password(s) vary by make and model of the pack.\n");	
		exit(1);
	}

	status = SMBReadWord(0x16,CMD_BOOTROM_VERSION);

	if (status <= 0) {
		printf("Error communicating with the Boot ROM.\nChip is not in the correct mode, has hung or there's an interface issue\n");
		exit(1);
	} else {
		printf("TI Boot ROM version %d.%d\n",(status>>8)&0xFF,status&0xFF);
	}

	printf("------------------------------------\n");


	if (programOut != NULL) {
		printf("Reading program flash\n");
		outFile=fopen(programOut,"wb");
		if (outFile == NULL) {
			printf("Error opening input file\n");
			exit(3);
		}


		for (i=0;i<PROGRAM_BLOCK_COUNT;i++) {
			status=readProgramBlock(i,block);
			if (status != PROGRAM_BLOCKSZ) {
				printf("Error: %s\n",SMBGetErrorString(status));
				exit(2);
			}
			fwrite(block,PROGRAM_BLOCKSZ,1,outFile);
			fprintf(stderr,".");
		}
			fprintf(stderr,"\nDone!\n");
		fclose(outFile);
		
	}

	if (eepromOut != NULL) {
		printf("Reading eeprom(data) flash\n");
		outFile=fopen(eepromOut,"wb");
		if (outFile == NULL) {
			printf("Error opening input file\n");
			exit(3);
		}

		for (i=0;i<EEPROM_BLOCK_COUNT;i++) {
			status=readEepromBlock(i,block);
			if (status != EEPROM_BLOCKSZ) {
				printf("Error: %s\n",SMBGetErrorString(status));
				exit(2);
			}
			fwrite(block,EEPROM_BLOCKSZ,1,outFile);
			fprintf(stderr,".");
		}
			fprintf(stderr,"\nDone!\n");
		fclose(outFile);

	}
	
	if (programIn !=NULL) {
		if (!confirmDelete) {
			printf("This will erase and reprogram the program flash on the microcontroller.\nIf you're sure add --confirm-delete and try again.\n");
			exit(0);
		}
		printf("Erasing program flash\n");
		eraseProgramFlash();
		printf("Done\n");
		printf("Flashing program flash\n");

		inFile = fopen(programIn,"rb");

		if (inFile != NULL) { 
			fseek(inFile, 0L, SEEK_END);
			i = ftell(inFile);
			rewind(inFile);	
			if (i != PROGRAM_BLOCKSZ * PROGRAM_BLOCK_COUNT) {
				printf("File size does not match flash size\n");
				exit(4);
			}
		} else {
			printf("Error opening input file");
			exit(3);
		}
	
		for (i=0;i<PROGRAM_BLOCK_COUNT;i++) {
			fread(block,PROGRAM_BLOCKSZ,1,inFile);
			status=writeProgramBlock(i,block);
			if (status != PROGRAM_BLOCKSZ) {
				printf("Error: %s\n",SMBGetErrorString(status));
				exit(2);
			}

			fprintf(stderr,".");
		}
		fprintf(stderr,"\nDone!\n");

		if (!noVerify) {
			printf("Verifying\n");
			rewind(inFile);
			for (i=0;i<PROGRAM_BLOCK_COUNT;i++) {
				fread(block,PROGRAM_BLOCKSZ,1,inFile);
				status=readProgramBlock(i,block2);				
				if (status != PROGRAM_BLOCKSZ) {
					printf("Error: %s\n",SMBGetErrorString(status));
					exit(2);
				}
				if (memcmp(block,block2,PROGRAM_BLOCKSZ) == 0) {
					fprintf(stderr,".");
				} else {
					printf("Block verify fail. Block #%d\n",i);
					exit(0);
				}
			}
		}
		fprintf(stderr,"\nVerified OK!\n");
		

		fclose(inFile);		
		
	}

	if (eepromIn !=NULL) {
		if (!confirmDelete) {
			printf("This will erase and reprogram the eeprom(data) flash on the microcontroller.\nIf you're sure add --confirm-delete and try again.\n");
			exit(0);
		}

		inFile = fopen(eepromIn,"rb");

		if (inFile != NULL) { 
			fseek(inFile, 0L, SEEK_END);
			i = ftell(inFile);
			rewind(inFile);	
			if (i != EEPROM_BLOCKSZ * EEPROM_BLOCK_COUNT) {
				printf("File size does not match flash size\n");
				exit(4);
			}
		} else {
			printf("Error opening input file");
			exit(3);
		}

		printf("Erasing eeprom(data) flash\n");
		eraseEepromFlash();
		printf("Done\n");
		printf("Flashing eeprom(data) flash\n");
	
		for (i=0;i<EEPROM_BLOCK_COUNT-(EEPROM_RESERVED_BYTES/EEPROM_BLOCKSZ);i++) {
			fread(block,EEPROM_BLOCKSZ,1,inFile);
			status=writeEepromBlock(i,block);
			if (status != EEPROM_BLOCKSZ) {
				printf("Error: %s\n",SMBGetErrorString(status));
				exit(2);
			}

			fprintf(stderr,".");
		}
		fprintf(stderr,"\nDone!\n");

		if (!noVerify) {
			printf("Verifying\n");
			rewind(inFile);
			for (i=0;i<EEPROM_BLOCK_COUNT-(EEPROM_RESERVED_BYTES/EEPROM_BLOCKSZ);i++) {
				fread(block,EEPROM_BLOCKSZ,1,inFile);
				status=readEepromBlock(i,block2);				
				if (status != EEPROM_BLOCKSZ) {
					printf("Error: %s\n",SMBGetErrorString(status));
					exit(2);
				}
				if (memcmp(block,block2,EEPROM_BLOCKSZ) == 0) {
					fprintf(stderr,".");
				} else {
					printf("Block verify fail. Block #%d\n",i);
					exit(0);
				}
			}
		}
		fprintf(stderr,"\nVerified OK!\n");	

		fclose(inFile);		
		
	}

	if (execute) {
		printf("Exiting Boot ROM and starting program flash! Good luck!\n");
		SMBSendByte(0x16,CMD_EXECUTE_FLASH);
	}
	

}
