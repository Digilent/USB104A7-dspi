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
/**
* Converts a string to lower case
*
* @return pointer to input str in lower case
*
*/
char* strlwr(char* str){
	uint8_t *p = (uint8_t*) str;
	while(*p){
		*p = tolower((uint8_t)*p);
		p++;
	}
	return str;
}
#endif

#define op_write 0xAA
#define op_read 0xBB

#define BTNREG 0
#define LEDREG 1

#ifndef bool
typedef enum { false, true } bool;
#endif

//Function flags
bool fWrite=false;
bool fRead=false;
bool fRunApplication=false;

//DSPI Initialized Flag
bool fDspiInit=false;

//Global Variables
uint8_t reg = 0;
uint8_t data = 0;
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
int parseParam(char* arg);
void printUsage();
int initDSPI();

#if defined(WIN32)
HANDLE terminalHandle;
DWORD WINAPI TerminalThread();
#else
pthread_t terminalHandle;
void* terminalThread();

void
SignalHandler(int sig) {
    if ( SIGINT == sig ) {
        fRunApplication = 0;
    }

    if ( SIGHUP == sig ) {
        fRunApplication = 0;
    }

    if ( SIGTERM == sig ) {
        fRunApplication = 0;
    }

    DebugMsg(INFO, "INFO: received request to terminate!!!\n");
}

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
	struct sigaction    sa;
    /* Setup a signal handler to catch Ctrl-C so that we can attempt a
    ** graceful shutdown.
    */
    sa.sa_handler = &SignalHandler;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    if ( -1 == sigaction(SIGINT, &sa, NULL) ) {
        DebugMsg(CRITICAL, "ERROR: failed to register signal handler for SIGINT\n");
        return 1;
    }

    if ( -1 == sigaction(SIGHUP, &sa, NULL) ) {
        DebugMsg(CRITICAL, "ERROR: failed to register signal handler for SIGHUP\n");
        return 1;
    }

    if ( -1 == sigaction(SIGTERM, &sa, NULL) ) {
        DebugMsg(CRITICAL, "ERROR: failed to register signal handler for SIGHUP\n");
        return 1;
    }

	status = pthread_create(&terminalHandle, NULL, &terminalThread, NULL);
#endif

	while(fRunApplication){
		
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

		//Write LEDs operation
		if (fWrite){
			fWrite = false;

			sendBuf[0] = op_write;
			sendBuf[1] = reg;
			sendBuf[2] = data;
			if(!DspiPut(hif, 0, 1, sendBuf, recvBuf, 2, 0)){
				status = DmgrGetLastError();
				printf("Error %d sending write message.\n",status);
				fDspiInit=fFalse;
				cmdState=GETINPUT;
				continue;
			}
			//A small delay is added to allow the USB104A7 to queue up another SPI transfer.
			//This is a limitation of the software driver used in this demo.
			nanosleep(&ts, NULL);
			if(!DspiPut(hif, 0, 1, sendBuf+2, recvBuf, 1, 0)){
				status = DmgrGetLastError();
				printf("Error %d sending write message.\n",status);
				fDspiInit=fFalse;
				cmdState=GETINPUT;
				continue;
			}
			
			cmdState=GETINPUT;
		}
		if (fRead){
			fRead = false;

			sendBuf[0] = op_read;
			sendBuf[1] = reg;
			if(!DspiPut(hif, 0, 0, sendBuf, recvBuf, 2, 0)){
				status = DmgrGetLastError();
				printf("Error %d sending read command message.\n",status);
				cmdState=GETINPUT;
				continue;
			}
			//A small delay is added to allow the USB104A7 to queue up another SPI transfer.
			//This is a limitation of the software driver used in this demo.
			nanosleep(&ts, NULL);
			if(!DspiGet(hif, 0, 1, 1, recvBuf, 1, fFalse)){
				status = DmgrGetLastError();
				printf("Error %d reading message.\n",status);
				cmdState=GETINPUT;
				continue;
			}

			printf("Register %d = 0x%02X", reg, recvBuf[0]);
			printf("\n");
			cmdState=GETINPUT;
		}

		
		nanosleep(&ts, NULL);
	}
	closeDSPI();
	exit(0);
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
			arg = strtok(NULL, " \n");
			reg = parseParam(arg);
			if(reg == -1){
				printf("Unrecognize register %s. Please enter register number/signifier: IE: 1 0x1 led btn", arg);
				return -1;
			}
			arg = strtok(NULL, " \n");
			data = parseParam(arg);
			if(data == -1){
				printf("Unrecognized data: %s. Please enter a decimal or hex value. IE: 0xA or 10", arg);
				return -1;
			}
			fWrite=true;
		}
		else if(strcmp(strlwr(arg), "read")==0){
			arg = strtok(NULL, " \n");
			reg = parseParam(arg);
			if(reg == -1){
				printf("Unrecognize register %s. Please enter data to write in hex.", arg);
				return -1;
			}

			fRead=true;
		}
		else if(strcmp(strlwr(arg), "help")==0 || arg[0]=='?'){
			printUsage();
			return -1;
		}
		else {
			printf("Error: Invalid argument %s\n", arg);
			printUsage();
			return -1;
		}
		arg = strtok(NULL, " \n");
	}

	return 0;
}

/**
* Parses the input string into a value.
*
* @param input the input string to parse
*
* @return integer value of a parameter
*
*/
int parseParam(char* arg){
	uint8_t val;
	if(arg==NULL){
		return -1;
	}
	//0xNN
	if(arg[0]=='0' && (arg[1]=='x'||arg[1]=='X')){
		sscanf(arg, "0x%X", &val);
	}
	//led(s)
	else if (strncmp(strlwr(arg), "led", 3)==0){
		val = LEDREG;
	}
	else if(strncmp(strlwr(arg),"btn", 3)==0){
		val = BTNREG;
	}
	//N
	else{
		val = strtol(arg, NULL, 10);
		if (val==0 && arg[0]!='0'){//strtol returns 0 if no number is found. Check if the arg is actually 0
			val=-1;//Arg was not a number
		}
	}
	return val;
}

/**
* Prints the usage of the program
*/
void printUsage(){
	printf("USB104A7 DSPI demo\n------------------------------\n");
	printf("This demo implements 64 makeshift registers on the USB104A7 that this application can read and write to.\n");
	printf("Registers:\n");
	printf("0 \"btn\" - Buttons\n1 \"led\" - LEDs\n2 - 63 General Purpose\n");
	printf("Commands\n");
	printf("write [register] [byte]\t-\twrite byte to \"register\" on board. IE: \"write led 5\" will turn on LD2 and LD0\n");
	printf("read [register]\t-\treads a \"register\" from the device. IE: \"read btn\" will read the value of the buttons\n");
	printf("help ?\t-\tPrints this usage menu\n");

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

/**
* User input thread. Polls fgets for user input, then waits for cmdState to return to GETINPUT.
*/
#if defined(WIN32)
DWORD WINAPI TerminalThread(){
#else
void* terminalThread(){
#endif
	
		//Print command prompt
		while(fRunApplication){
			printf("Enter command:");
			fgets(input, sizeof(input), stdin);
			cmdState=EXECUTE;
			while(cmdState != GETINPUT);
		}
		return 0;
}