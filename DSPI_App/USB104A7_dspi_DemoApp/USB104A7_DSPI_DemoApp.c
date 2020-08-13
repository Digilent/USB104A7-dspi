#define WIN32_LEAN_AND_MEAN


#if defined (WIN32)
    
    #include <windows.h>
    
#else

    #include <signal.h>
    #include <termios.h>
	#include <pthread.h>
	#include <unistd.h>

#endif


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

#include "dpcdecl.h"
#include "dmgr.h"
#include "dspi.h"


#ifndef strlwr
char* strlwr(char* str){
	uint8_t *p = (uint8_t*) str;
	while(*p){
		*p = tolower((uint8_t)*p);
		p++;
	}
	return str;
}
#endif


//Command Protocol:
//Op is sent then a parameter or parameters

#define op_write 0xAA
#define op_read 0xBB

#ifndef bool
typedef enum { false, true } bool;
#endif

//Function flags
bool fWrite=false;
bool fRead=false;

bool fExitApplication=false;

//DSPI Initialized Flag
bool fDspiInit=false;

//Global Variables
uint32_t length = 0;
char input[256];

BYTE sendBuf[128];
BYTE recvBuf[128];


DWORD threadID;
enum cmdState{
GETINPUT, // Wait for console thread to enter command
EXECUTE, // Command received, main thread to execute
WAIT // Command received, both threads waiting for DSPI
} cmdState = GETINPUT;

//DSPI Device Variables
HIF hif;
int portNum=0;
int failattempt=0;

//Forward Declarations
void closeDSPI();
int parseArgs(char* input);
void printUsage();
int initDSPI();

#if defined(WIN32)
HANDLE terminalHandle;
DWORD WINAPI TerminalThread();
#else
pthread_t terminalHandle;
void* terminalThread();
#endif

int main(int argc, char* argv[]){
	struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};

	int status;

	printUsage();
	atexit(closeDSPI);

//Initialize the DSPI connection.

	if(status = initDSPI()!=0){
		printf("Is the USB104A7 connected and accessible? Adept runtime 2.20 or later is required.\n");
		return status;
	}else{
		fDspiInit=true;
	}
	

#if defined (WIN32)
	terminalHandle = CreateThread(0, 0, TerminalThread, NULL, 0, &threadID);
#else
	status = pthread_create(&terminalHandle, NULL, &terminalThread, NULL);
#endif

	while(1){
		
		//If the DSPI is not connected
		if(fDspiInit==false){
			//Initialize the DSPI connection.
			if(status = initDSPI()!=0){
			continue;//Retry
			}else{
				fDspiInit=true;
			}
		}

		//cmdState is set in the terminal thread when input is received.
		if(cmdState == EXECUTE){
			//Parse input

			if(parseArgs(input)== -1){
				cmdState= GETINPUT;
			}
			
		}

		//Write wave operation
		if (fWrite){
			fWrite=false;

			sendBuf[0] = op_write;
			sendBuf[1] = (uint8_t)length;
			if(!DspiPut(hif, 0, 1, sendBuf, recvBuf, 2, 0)){
				status = DmgrGetLastError();
				printf("Error %d sending write message.\n",status);
				fDspiInit=fFalse;
				cmdState=GETINPUT;
				continue;
			}
			if(!DspiPut(hif, 0, 1, sendBuf+2, recvBuf, length, 0)){
				status = DmgrGetLastError();
				printf("Error %d sending write message.\n",status);
				fDspiInit=fFalse;
				cmdState=GETINPUT;
				continue;
			}
			
			cmdState=GETINPUT;
		}
		if (fRead){
			fRead=false;

			sendBuf[0]=op_read;
			sendBuf[1]=(uint8_t)length;
			if(!DspiPut(hif, 0, 0, sendBuf, recvBuf, 2, 0)){
				status = DmgrGetLastError();
				printf("Error %d sending read command message.\n",status);
				cmdState=GETINPUT;
				continue;
			}
			DmgrSetTransTimeout(hif, 5000);
			if(!DspiGet(hif, 0, 1, 1, recvBuf, length+1, fFalse)){
				status = DmgrGetLastError();
				printf("Error %d reading message.\n",status);
				cmdState=GETINPUT;
				continue;
			}

			printf("Received:");
			for(int i =1; i<length+1; i++){
				printf("0x%02X ", recvBuf[i]);
			}
			printf("\n");
			cmdState=GETINPUT;
		}

		
		nanosleep(&ts, NULL);
	}

	return 0;
}

/**
* Closes the connection to the DSPI device
*/
void closeDSPI(){
	cmdState=GETINPUT;//Print prompt again.
	DmgrCancelTrans(hif);
	DspiDisable(hif);
	DmgrClose(hif);
	fDspiInit=false;
	cmdState=false;
}

/**
* Parses the input string into commands. Boolean flags are set and executed in the main loop.
*
* @param input the input string to parse
*
* @return 0 if passed, -1 if failed
*
*/
int parseArgs(char* input){
	char* arg;
	arg = strtok(input, " \n");
	if(arg==NULL){
		printf("\nPlease enter a command\n");
		return -1;
	}
	while(arg!=NULL){
		if(strcmp(strlwr(arg), "write")==0){
			length =0;
			arg = strtok(NULL, " \n");
			while(arg != NULL){
				sscanf(arg, "%X", &sendBuf[length+2]);
				length++;
				arg = strtok(NULL, " \n");
			}
			if(length==0){
				printf("Please enter data to write in hex.");
				return -1;
			}
			else{
				fWrite=true;
			}
			
		}
		else if(strcmp(strlwr(arg), "read")==0){
			arg = strtok(NULL, " \n");
			if(arg!=NULL){
				length = atoi(arg);
			}
			if(length>0){
				fRead=true;
			}
			else{
				printf("Please enter the length to read.");
				return -1;
			}
		}
		else {
			printf("Error: Invalid argument %s\n", arg);
			return -1;
		}
		arg = strtok(NULL, " \n");
	}

	return 0;
}

/**
* Prints the usage of the program
*/
void printUsage(){
	printf("USB104A7 DSPI demo\n------------------------------\n");
	printf("Commands\n");
	printf("write [bytes in hex]\t-\twrite bytes to device. IE: write 01 aa 3a 4b 77\n");
	printf("read [length]\t-\treads [length] bytes from device. Read data is a counter\n");

}

/**
* Initializes the connection to the DSPI device.
*
* @return status, 0 if passed
*
*/
int initDSPI(){
	int status;
	int cprtPti;
	DWORD spd;


	//Open device
	if((status = DmgrOpen(&hif, "Usb104A7_DPTI"))==false){
		status = DmgrGetLastError();
		printf("Error %d opening Usb104A7_DSPI\n", status);
		return status;	
	}
	//Get DSPI port count on device
	if((status = DspiGetPortCount(hif, &cprtPti))==false){
		status = DmgrGetLastError();
		printf("Error %d getting DSPI port count\n", status);
		DmgrClose(hif);
		return status;
	}
	if(cprtPti == 0){
		printf("No DSPI ports found\n");
		return status;
	}
	//Enable DSPI bus
	if((status = DspiEnableEx(hif, portNum))==false){
		status = DmgrGetLastError();
		printf("Error %d enabling DSPI bus\n", status);
		DmgrClose(hif);
		return status;
	}
	//Attempt to set speed slower (actual speed is stored in spd)
	DspiSetSpeed(hif, 125000, &spd);
	if(status =DspiSetSpiMode(hif, 0, fFalse)==false){ // Set to SPI Mode 0
		status = DmgrGetLastError();
		printf("Error %d setting SPI mode\n", status);
		DmgrClose(hif);
		return status;
	}
	DmgrSetTransTimeout(hif, 3000);//3 second timeout
	printf("DSPI Device Opened\n");
	return 0;
}
#if defined(WIN32)
DWORD WINAPI TerminalThread(){
#else
void* terminalThread(){
#endif
	
		//Print command prompt
		while(1){
			printf("Enter command:");
			fgets(input, sizeof(input), stdin);
			cmdState=EXECUTE;
			while(cmdState != GETINPUT);
		}
		return 0;
}