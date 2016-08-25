#include <string.h>
#include <stdio.h>

#include <fx2ints.h>
#include <fx2regs.h>
#include <fx2macros.h>
#include <autovector.h>
#include <setupdat.h>
#include <i2c.h>
#include <serial.h>
#include <delay.h>
#include <eputils.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_REVISION 0

#define SYNCDELAY SYNCDELAY4;

#define I2C_MAX_RETRIES 5
#define I2C_TIMEOUT 10


#define SMB_INTERFACE_ID 0x99
#define SMB_FIRMWARE_VERSION 0x98

// Standard SMB protocol commands

#define SMB_ENABLE_PEC 0x5



#define SMB_READ_BYTE 0x10
#define SMB_WRITE_BYTE 0x11
#define SMB_SEND_BYTE 0x12	// Start->Addr->Cmd->Stop. Invoke command without read or write

#define SMB_READ_WORD 0x20
#define SMB_WRITE_WORD 0x21

#define SMB_READ_BLOCK 0x30
#define SMB_WRITE_BLOCK 0x32

#define BLOCK_SEQ_TIMEOUT 5

// Arbitrary SMB operations

#define SMB_WRITE 0x50			//smb_addr = length, smb_cmd = write_cmd
#define SMB_WRITE_CMD_START_FIRST 0x1
#define SMB_WRITE_CMD_RESTART_FIRST 0x2
#define SMB_WRITE_CMD_STOP_AFTER 0x4

#define SMB_READ 0x51			// smb_addr = length, smb_cmd = read_cmd
#define SMB_READ_CMD_FIRST_READ 0x1
#define SMB_READ_CMD_LAST_READ 0x2	// last read block, handles LASTRD, STOP

#define SMB_GET_CLEAR_PEC_FAIL	0x54
#define SMB_GET_MRQ_PECS	0x55

#define SMB_RESET_INTERFACE 0x61 	// sets stop, clears mrq_pecs, empties blockread temp buffer
                                 
// SMB Hacking and Discovery

#define SMB_TEST_ADDRESS_ACK 0x90
#define SMB_TEST_COMMAND_ACK 0x91
#define SMB_TEST_COMMAND_WRITE 0x92

BYTE pec_crc(BYTE crc, BYTE data) {
    BYTE i;

    data = crc ^ data;
  
    for ( i = 0; i < 8; i++ ) 
    {
        if (( data & 0x80 ) != 0 )
        {
            data <<= 1;
            data ^= 0x07;
        }
        else
        {
            data <<= 1;
        }
    }
    return (BYTE)data;
}


volatile __bit dosud;
__bit on;

volatile WORD count = 0;
volatile BOOL pec_enabled = TRUE;
volatile BOOL pec_failed = FALSE;

volatile __xdata BYTE temp[192];
volatile __xdata WORD tempptr=0,templen=0;
volatile __xdata BYTE mrq_pec=0,rcv_pec=0;

void main() {

 REVCTL = 0;

 dosud=FALSE;
 on=FALSE;
 
 REVCTL = 0x03; // DYN_OUT=1, ENH_PKT=1
 
 RENUMERATE_UNCOND();

 SETCPUFREQ(CLK_48M);
 sio0_init(57600); 
 
 USE_USB_INTS();
 
 ENABLE_SUDAV();
 ENABLE_USBRESET();
 ENABLE_HISPEED();

 TMOD = 0x01; 
 
 EA=1;

 ENABLE_TIMER0();
 TR0=1;  


 while(TRUE) {

 if (dosud) {
   handle_setupdata();
   dosud=FALSE;
 } 

 }
 

}

BOOL handle_get_descriptor() {
  return FALSE;
}


BOOL i2c_start() {
	WORD tries = 0;

	retry:
		if (tries>=I2C_MAX_RETRIES) return FALSE;
	        I2CS |= bmSTART;
	        if ( I2CS & bmBERR ) {            
		            delay(10);
			    tries++;
		            goto retry;
		}

		return TRUE;
}

void i2c_restart() {
	I2CS |= bmSTART; 
}

void i2c_stop() {
	
            I2CS |= bmSTOP;
	    count=0;
            while  (I2CS&bmSTOP) {
		if (count>I2C_TIMEOUT) {
			return;
		}
	    }
}

/*
@returns isAck
*/
BOOL i2c_byteout(BYTE outb) {

	I2DAT = outb;
	
	count=0;
	while ( !(I2CS & bmDONE) ) {
		if (count>I2C_TIMEOUT) {
			i2c_stop();
			return FALSE;
		}
	}
        if (I2CS & bmBERR) return FALSE;	
        
        return (I2CS&bmACK);
}

BYTE i2c_bytein(BOOL is_first, BOOL is_single, BOOL is_before_last, BOOL is_last) {

	BYTE b;

        if (is_single) {
		I2CS |= bmLASTRD;
	}
	
	if (is_first) {
		BYTE discard = I2DAT;
		count=0;
		while ( !(I2CS & bmDONE) ){
			if (count>I2C_TIMEOUT) {
				i2c_stop();
				return FALSE;
			}

		}
	}

	if (is_last) {
		I2CS |= bmSTOP;
	}	

        if (is_before_last) {
		I2CS |= bmLASTRD;
	}

	b = I2DAT;

	if (!is_last)  {
		count=0;
		while ( !(I2CS & bmDONE) ){
			if (count>I2C_TIMEOUT) {
				i2c_stop();
				return FALSE;
			}

		}

	} else {
		count=0;
		while ( !(I2CS & bmSTOP) ){
			if (count>I2C_TIMEOUT) {
				i2c_stop();
				return FALSE;
			}

		}

	}		

	return b;
}

        
BOOL handle_vendorcommand(BYTE cmd) {

 WORD smb_addr = SETUP_VALUE();
 WORD smb_cmd = SETUP_INDEX();
 WORD smb_len = SETUP_LENGTH();
 BYTE b,i=0,j=0,k=0,blocklen=0,rs=0,pec=0, rpec=0;
 BOOL ack=FALSE;
 
 switch (cmd) {
    case SMB_ENABLE_PEC:
	while (EP0CS&bmEPBUSY); // wait until ready
	        EP0BCH=0;
	        EP0BCL=0;		
		pec_enabled = (smb_addr>0);
		if (!pec_enabled) {
			mrq_pec = 0; rcv_pec=0;
		}
		return TRUE;
	break;
    case SMB_INTERFACE_ID:
	while (EP0CS&bmEPBUSY); // wait until ready
		*(EP0BUF) = 0x55; *(EP0BUF+1) = 0x53; 	*(EP0BUF+2) = 0x4D;
	        EP0BCH=0;
	        EP0BCL=3;		
		return TRUE;
	break;
    case SMB_FIRMWARE_VERSION:
	while (EP0CS&bmEPBUSY); // wait until ready
		*(EP0BUF) = VERSION_MAJOR;
		*(EP0BUF+1) = VERSION_MINOR;
		*(EP0BUF+2) = VERSION_REVISION;
		EP0BCH=0;
	        EP0BCL=3;		
		return TRUE;
	break;
    case SMB_SEND_BYTE:	
	while (EP0CS&bmEPBUSY); // wait until ready

	if (!i2c_start()) return FALSE;
	if (!i2c_byteout(smb_addr)) {
		i2c_stop();
		return FALSE;
	}
	if (!i2c_byteout(smb_cmd)) {
		i2c_stop();
		return FALSE;
	}
	i2c_stop();

	EP0BCH=0;
	EP0BCL=0;		

	return TRUE;

	break;
    case SMB_READ_BYTE:

	while (EP0CS&bmEPBUSY); // wait until ready

	if (!i2c_start()) goto rbfail;
	if (!i2c_byteout(smb_addr)) goto rbfail;
	if (!i2c_byteout(smb_cmd)) goto rbfail;
	i2c_restart();
	if (!i2c_byteout(smb_addr+1)) goto rbfail;

        if (pec_enabled) {
		b = i2c_bytein(TRUE,FALSE,TRUE,FALSE); 
		pec = pec_crc(pec,smb_addr);
		pec = pec_crc(pec,smb_cmd);
		pec = pec_crc(pec,smb_addr+1);		
		pec = pec_crc(pec,b);
		rpec = i2c_bytein(FALSE,FALSE,FALSE,TRUE); 
		if (rpec != pec) {
			 EP0BCH=0;
			 EP0BCL=0;		
			 pec_failed=TRUE;
  			 goto rbfail;
		}		
	} else {
		b = i2c_bytein(TRUE,TRUE,FALSE,TRUE); 
	}

	*EP0BUF = b;
	 EP0BCH=0;
	 EP0BCL=1;		
	
	goto rbsuccess;

	rbfail:
	i2c_stop();
	return FALSE;
	
	rbsuccess:
	return TRUE;

	break;

    case SMB_WRITE_BYTE:
	EP0BCL=0; // read from the host
	while (EP0CS&bmEPBUSY); // wait until ready

	if (!i2c_start()) goto wbfail;
	if (!i2c_byteout(smb_addr)) goto wbfail;
	if (!i2c_byteout(smb_cmd)) goto wbfail;
	if (!i2c_byteout(*EP0BUF)) goto wbfail;
	if (pec_enabled) {
		pec = pec_crc(pec, smb_addr);
		pec = pec_crc(pec, smb_cmd);
		pec = pec_crc(pec, *EP0BUF);
		if (!i2c_byteout(pec)) goto wbfail;		
	}
	i2c_stop();
	
	goto wbsuccess;

	wbfail:
	i2c_stop();
	return FALSE;
	
	wbsuccess:
	return TRUE;

	break;

    case SMB_READ_WORD:
	while (EP0CS&bmEPBUSY); // wait until ready

	if (!i2c_start()) goto rwfail;
	if (!i2c_byteout(smb_addr)) goto rwfail;
	if (!i2c_byteout(smb_cmd)) goto rwfail;
	i2c_restart();
	if (!i2c_byteout(smb_addr+1)) goto rwfail;
	if (pec_enabled) {
		b = i2c_bytein(TRUE,FALSE,FALSE,FALSE);
		*EP0BUF = b;
		pec = pec_crc(pec,smb_addr);
		pec = pec_crc(pec,smb_cmd);
		pec = pec_crc(pec,smb_addr+1);		
		pec = pec_crc(pec,b);
		b = i2c_bytein(FALSE,FALSE,TRUE,FALSE);
		*(EP0BUF+1) = b;
		pec = pec_crc(pec,b);
		rpec = i2c_bytein(FALSE,FALSE,FALSE,TRUE);
		if (rpec != pec) {
			 EP0BCH=0;
			 EP0BCL=0;
			 pec_failed=TRUE;		
 			 goto rwfail;
		}
	} else { 
		b = i2c_bytein(TRUE,FALSE,TRUE,FALSE);
		*EP0BUF = b;
		b = i2c_bytein(FALSE,FALSE,FALSE,TRUE);
		*(EP0BUF+1) = b;	
	}

        EP0BCH=0;
        EP0BCL=2;		
	
	goto rwsuccess;

	rwfail:
	i2c_stop();
	return FALSE;
	
	rwsuccess:
	return TRUE;

	break;

    case SMB_WRITE_WORD:
	EP0BCL=0; // read from the host
	while (EP0CS&bmEPBUSY); // wait until ready

	if (!i2c_start()) goto wwfail;
	if (!i2c_byteout(smb_addr)) goto wwfail;
	if (!i2c_byteout(smb_cmd)) goto wwfail;
	if (!i2c_byteout(*EP0BUF)) goto wwfail;
	if (!i2c_byteout(*(EP0BUF+1))) goto wwfail;
	if (pec_enabled) {
		pec = pec_crc(pec,smb_addr);
		pec = pec_crc(pec,smb_cmd);
		pec = pec_crc(pec,*EP0BUF);		
		pec = pec_crc(pec,*(EP0BUF+1));		
		if (!i2c_byteout(pec)) goto wwfail;				
	}
	i2c_stop();
	
	goto wwsuccess;

	wwfail:
	i2c_stop();
	return FALSE;
	
	wwsuccess:
	return TRUE;

	break;

    case SMB_READ_BLOCK:
	while (EP0CS&bmEPBUSY); // wait until ready

	if (templen>0 && count>BLOCK_SEQ_TIMEOUT) {
		templen=0; tempptr=0;
	}

	if ((templen-tempptr)>0) {
		// when there is outstanding data, this command returns it in sequence, ignoring parameters

		blocklen = templen-tempptr >64 ? 64:templen-tempptr;
		i=tempptr;
		j=0;
		while (j < blocklen) { 
			*(EP0BUF+j) = temp[i];			
			i++; j++;
		}
		tempptr+=j;

	        EP0BCH=0;
	        EP0BCL=blocklen;

		if (templen-tempptr == 0) {
			templen=0; tempptr=0;
		}
		return TRUE;
	} 

	if (!i2c_start()) goto rlfail;
	if (!i2c_byteout(smb_addr)) goto rlfail;
	if (!i2c_byteout(smb_cmd)) goto rlfail;
	i2c_restart();
	if (!i2c_byteout(smb_addr+1)) goto rlfail;
	if (pec_enabled) {
		pec = pec_crc(pec,smb_addr);
		pec = pec_crc(pec,smb_cmd);
		pec = pec_crc(pec,smb_addr+1);
	}
	
	// HW i2c on the FX2 needs prior knowledge of the last byte we want to read at least 2 bytes
	// in advance. We don't have that with SMBus Block Reads
	// 
	// worst case scenario: the first read (block size) returns 1.
	// we would've already needed to set LASTRD at this point
	// workaround: always read the PEC byte

	i=0; 
	blocklen=255;
	while (i<blocklen) {
		b = i2c_bytein(i==0,FALSE,i==blocklen-2,i==blocklen-1);		
		if (i==0) { 	
			blocklen = b+2; //read the PEC byte always (+1 the blocksz, +1 the pec)
		} 
		if (pec_enabled && i<blocklen-1) pec = pec_crc(pec,b); // last byte is PEC, don't need to include that in crc
		if (pec_enabled && i==blocklen-1) rpec = b; // but we can save it
		if (i<blocklen-1) { // never need the PEC in the buffer
			if (i<64) {
        			*(EP0BUF+i)=b; 
			} else {
				temp[i-64] = b;
			}
		}
		
		i++;
	}

	if ((blocklen-1)>64) {			// if blocklen sans the PEC byte including the blocksz byte >64
		templen = (blocklen-1)-64;	// then the rest is stores in temp
		tempptr = 0;
		count = 0;
	}

	if (pec_enabled) {
		if (rpec != pec) {
			 EP0BCH=0;
			 EP0BCL=0;	
			 pec_failed=TRUE;	
 			 goto rlfail;
		}		
	}
	
	EP0BCH=0;
	EP0BCL=(blocklen-1)>64 ? 64 : blocklen-1;	
	
	goto rlsuccess;

	rlfail:
	i2c_stop();
	return FALSE;
	
	rlsuccess:
	return TRUE;

	break;

    case SMB_WRITE_BLOCK:
	EP0BCL=0; // read from the host
	while (EP0CS&bmEPBUSY); // wait until ready

	if (templen>0 && count>BLOCK_SEQ_TIMEOUT) {
		templen=0; tempptr=0;
	}
	
	if (templen==0 && *EP0BUF >64) {		// when no SEQ write in progress and the write is too big to be completed in one req
		templen=*EP0BUF;			// templen = blocksz 
		tempptr=0;				
		// put the 63 bytes into the temp
		while (tempptr<63) {                    // store the first 63 bytes in this req (max reqsz - blocksz byte)
			temp[tempptr] = *(EP0BUF+1+tempptr);
			tempptr++;
		}
		count=0;          			// clear timer counter
		return TRUE;
		
	}
	if (templen>0) {				// if in-progress SEQ write
		if (tempptr < templen) {		// still data to be read
			blocklen = templen-tempptr > 64 ? 64 : templen-tempptr; // will it be completed in this req?
			i=0; j=tempptr;
			while (i<blocklen) {
				temp[j] = *(EP0BUF+i);
				i++; j++;
			}		
			tempptr+=i;			
		}
		if (tempptr == templen) {	// we have all the data
				if (!i2c_start()) goto wlfail;
				if (!i2c_byteout(smb_addr)) goto wlfail;
				if (!i2c_byteout(smb_cmd)) goto wlfail;	
				if (!i2c_byteout(templen)) goto wlfail;
				if (pec_enabled) {
					pec = pec_crc(pec,smb_addr);
					pec = pec_crc(pec,smb_cmd);
					pec = pec_crc(pec,templen);
				}
				i=0;
				while (i<templen) {
					if (!i2c_byteout(temp[i])) goto wlfail;
					if (pec_enabled) {
	       					pec = pec_crc(pec,temp[i]);
					}
					i++;
				}
				if (pec_enabled) {
					if (!i2c_byteout(pec)) goto wlfail;					
				}

				i2c_stop();

	                        tempptr=0; templen=0;	// we're done!
				
				goto wlsuccess;
		} else {
			return TRUE; // just got some data but not ready to send yet
		}
	}
	

	if (!i2c_start()) goto wlfail;
	if (!i2c_byteout(smb_addr)) goto wlfail;
	if (!i2c_byteout(smb_cmd)) goto wlfail;	
	if (!i2c_byteout(*EP0BUF)) goto wlfail;
	if (pec_enabled) {
		pec = pec_crc(pec,smb_addr);
		pec = pec_crc(pec,smb_cmd);
		pec = pec_crc(pec,*EP0BUF);
	}

	i=0;
	while (i<*EP0BUF) {
		if (!i2c_byteout(*(EP0BUF+1+i))) goto wlfail;
		if (pec_enabled) {
			pec = pec_crc(pec,*(EP0BUF+1+i));
		}

		i++;
	}
	if (pec_enabled) {
		if (!i2c_byteout(pec)) goto wlfail;					
	}

	i2c_stop();
	
	goto wlsuccess;

	wlfail:
	i2c_stop();
	return FALSE;
	
	wlsuccess:
	return TRUE;

	break;
    

    case SMB_WRITE:
		EP0BCL=0; // read from the host
		while (EP0CS&bmEPBUSY); // wait for read to finish
		if (smb_cmd & SMB_WRITE_CMD_START_FIRST){
			if (pec_enabled) {
				mrq_pec = 0; 
			}
			if (!i2c_start()) goto wafail;
		}
		if (smb_cmd & SMB_WRITE_CMD_RESTART_FIRST){
			i2c_restart();			
		}	
		i=0;
		while (i<smb_addr) {
			if (!i2c_byteout(*(EP0BUF+i))) goto wafail;
			if (pec_enabled) {
				mrq_pec = pec_crc(mrq_pec,*(EP0BUF+i));
			}
			i++;
		}
		if (smb_cmd & SMB_WRITE_CMD_STOP_AFTER) {
			if (pec_enabled) {
				 if (!i2c_byteout(mrq_pec)) goto wafail;			
			}
			i2c_stop();
		}

		goto wasuccess;
	    
	    wafail:
		i2c_stop();
		return FALSE;
	    wasuccess:
		return TRUE;
	    break;
    case SMB_READ:	    
	    while (EP0CS&bmEPBUSY); // wait until ready	
		rs=smb_cmd; //read state	
		i=0;					
		j=smb_addr;
		if (pec_enabled && (rs & SMB_READ_CMD_LAST_READ)) {
			j++; // inject read of the pec byte here
		}
		while (i<j) {
			b = i2c_bytein(rs & SMB_READ_CMD_FIRST_READ,
						 ((rs & SMB_READ_CMD_FIRST_READ) && (j==1)),
						 ((rs & SMB_READ_CMD_LAST_READ) && (i==j-2)),
						 ((rs & SMB_READ_CMD_LAST_READ) && (i==j-1)));			
			if (pec_enabled) {
				if ((rs & SMB_READ_CMD_LAST_READ) && (i==j-1)) {
					rcv_pec = b;					
				} else {
					*(EP0BUF+i) = b;
					mrq_pec = pec_crc(mrq_pec,b);					

				}
			} else {
				*(EP0BUF+i) = b;	
			}
		
			i++;
                        rs &= ~SMB_READ_CMD_FIRST_READ;
			
		}

	    EP0BCH=0;
	    EP0BCL=smb_addr;

	    return TRUE;		

            break;

     case SMB_GET_CLEAR_PEC_FAIL:
	    while (EP0CS&bmEPBUSY); // wait until ready
		if (pec_failed) {
			*EP0BUF=0xFF;
			pec_failed=FALSE;
		} else {
			*EP0BUF=0;
		}
	       EP0BCH=0;
 	       EP0BCL=1;

	    return TRUE;	
	    break;
     case 0x66:
	    while (EP0CS&bmEPBUSY); // wait until ready
	    i=0;
	    while (i<64) {
		*(EP0BUF+i) = temp[i];
		i++;
	    }
	       EP0BCH=0;
 	       EP0BCL=64;

	    return TRUE;
	
	break;
     case 0x67:
	    while (EP0CS&bmEPBUSY); // wait until ready
	    smb_addr = count;
  	    *EP0BUF = smb_addr &0xFF;
  	    *(EP0BUF+1) = smb_addr >>8;

	     EP0BCH=0;
 	     EP0BCL=2;

	    return TRUE;
	
	break;
     case SMB_GET_MRQ_PECS:
	    while (EP0CS&bmEPBUSY); // wait until ready
  	    *EP0BUF = mrq_pec;
  	    *(EP0BUF+1) = rcv_pec;

	     EP0BCH=0;
 	     EP0BCL=2;

	    return TRUE;
		
	break;
     case SMB_RESET_INTERFACE:
	    while (EP0CS&bmEPBUSY); // wait until ready
	    mrq_pec=0; rcv_pec=0; templen=0; tempptr=0; 
	    i2c_stop();
	    return TRUE;
	break;
     case SMB_TEST_ADDRESS_ACK:
	    while (EP0CS&bmEPBUSY); // wait until ready
	        if (!i2c_start()) return FALSE;
		ack = i2c_byteout(smb_addr);
		i2c_stop();
		if (ack) {
		 *EP0BUF=0xFF;
		} else {
		 *EP0BUF=0;
		}
    	       EP0BCH=0;
 	       EP0BCL=1;
	    return TRUE;   				
	    break;

     case SMB_TEST_COMMAND_ACK:
		while (EP0CS&bmEPBUSY); // wait until ready
		
	        if (!i2c_start()) return FALSE;
		if (!i2c_byteout(smb_addr)) goto tcaFail;
		ack = (i2c_byteout(smb_cmd));
		tcaFail:
		i2c_stop();
		if (ack) {
		 *EP0BUF=0xFF;
		} else {
		 *EP0BUF=0;
		}
    	       EP0BCH=0;
 	       EP0BCL=1;
	    return TRUE;   				
	    break;
     case SMB_TEST_COMMAND_WRITE:
		while (EP0CS&bmEPBUSY); // wait until ready
		*EP0BUF = 0;
	        if (!i2c_start()) return FALSE;
		if (!i2c_byteout(smb_addr)) goto tcwOver;
		if (i2c_byteout(smb_cmd)) { (*EP0BUF)++; // Command ACK = command exists
		} else {goto tcwOver;}
		if (i2c_byteout(3)) { (*EP0BUF)++; // Data Byte #1 ACK = byte writable
		} else {goto tcwOver;}
		if (i2c_byteout(0)) { (*EP0BUF)++; // Data Byte #2 ACK = word writable
		} else {goto tcwOver;}
		if (!i2c_byteout(0)) {goto tcwOver;}
		if (i2c_byteout(0)) { (*EP0BUF)++; // Data Byte #4 ACK = block writable
		} else {goto tcwOver;}
		if (i2c_byteout(0)) { (*EP0BUF)++; // Data Byte #5 ACK = >block writable
		} else {goto tcwOver;}

		tcwOver:
		i2c_stop();
		
		EP0BCH=0;
		EP0BCL=1;
		
		return TRUE;   				

	        
		break;

     default:
		return FALSE; // unknown command
 }
            
}
  

void timer0_isr() __interrupt TF0_ISR {
 count++;
}


// set *alt_ifc to the current alt interface for ifc
BOOL handle_get_interface(BYTE ifc, BYTE* alt_ifc) {
 *alt_ifc=0;
 return TRUE;
}
// return TRUE if you set the interface requested
// NOTE this function should reconfigure and reset the endpoints
// according to the interface descriptors you provided.
BOOL handle_set_interface(BYTE ifc,BYTE alt_ifc) {  
 //return ifc==0&&alt_ifc==0; 
// printf ( "Host wants to set interface: %d\n", alt_ifc );
 
 return TRUE;
}
// handle getting and setting the configuration
// 0 is the default.  If you support more than one config
// keep track of the config number and return the correct number
// config numbers are set int the dscr file.
volatile BYTE config=1;
BYTE handle_get_configuration() { 
 return config; 
}
// return TRUE if you handle this request
// NOTE changing config requires the device to reset all the endpoints
BOOL handle_set_configuration(BYTE cfg) { 
// printf ( "host wants config: %d\n" , cfg );
 config=cfg; 
 return TRUE;
}


void sudav_isr() __interrupt SUDAV_ISR {
 dosud=TRUE;
 CLEAR_SUDAV();
}

void usbreset_isr() __interrupt USBRESET_ISR {
 handle_hispeed(FALSE);
 CLEAR_USBRESET();
}
void hispeed_isr() __interrupt HISPEED_ISR {
 handle_hispeed(TRUE);
 CLEAR_HISPEED();
}

