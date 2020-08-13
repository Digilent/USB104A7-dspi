/******************************************************************************/
/*                                                                            */
/* main.c -- Demonstration of a simple use of Adept DSPI on USB104A7          */
/*                                                                            */
/******************************************************************************/
/* Author: Thomas Kappenman                                                   */
/* Copyright 2020, Digilent Inc.                                              */
/******************************************************************************/
/* File Description:                                                          */
/*                                                                            */
/* This file contains a basic demo in order to use Digilent Adept DSPI on     */
/* the USB104A7                                                               */
/*                                                                            */
/* Adept DSPI commands are sent from the USB104A7_DSPI_DemoApp application.   */
/* When a write operation is received, then print out the bytes that were     */
/* received. When a read operation is received, then send [length] bytes      */
/* back over DSPI as a counter.                                               */
/*                                                                            */
/******************************************************************************/
/* Revision History:                                                          */
/*                                                                            */
/*    08/13/2020(TommyK):   Created                                           */
/*                                                                            */
/******************************************************************************/
/* Baud Rate :                                                                */
/*                                                                            */
/* 115200 or what was specified in UARTlite core                              */
/*                                                                            */
/******************************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xspi.h"
#include "xparameters.h"

#define BUFFER_SIZE 4
XSpi DSPI;
u8 WriteBuffer[128];
u8 ReadBuffer[128];

int init();

int main()
{
	int Count=0;
	int i;
	int length = 0;
	int Status;
	if((Status=init())!=XST_SUCCESS){
		xil_printf("Error %d during initialization. Exiting.\r\n", Status);
	}

	while(1){

		/*
		 * Prepare the data buffers for transmission and to send/receive data
		 * when the SPI device is selected by a master.
		 */
		Status = XSpi_Transfer(&DSPI, WriteBuffer, ReadBuffer, 2);
		if(Status!=XST_SUCCESS){
			xil_printf("Error %d during SPI Transfer. Closing Application.\r\n");
			return -1;
		}
		//Operation is byte 0, length is byte 1
		length = ReadBuffer[1];

		switch(ReadBuffer[0]){
		case 0xAA://Write op, print out bytes that were received
			xil_printf("Write op received\r\n");
			Status = XSpi_Transfer(&DSPI, WriteBuffer, ReadBuffer, length);//Read length
			if(Status!=XST_SUCCESS){
				xil_printf("Error %d during SPI Transfer. Closing Application.\r\n");
				return -1;
			}
			xil_printf("Received:");
			for(i=0;i<length; i++){
				xil_printf("0x%02X ", ReadBuffer[i]);
			}
			xil_printf("\r\n");
			break;
		case 0xBB://Read op, send a byte count back
			xil_printf("Read op received\r\n");
			Count=0;
			//First byte is a dummy byte, so go to length+1
			for(i=0; i<length+1; i++){
				WriteBuffer[i]=Count;
				Count++;
			}
			Status= XSpi_Transfer(&DSPI, WriteBuffer, ReadBuffer, length+1);
			if(Status!=XST_SUCCESS){
				xil_printf("Error %d during SPI Transfer. Closing Application.\r\n");
				return -1;
			}
			break;
		default:
			xil_printf("Invalid command received: 0x%02X", ReadBuffer[0]);
			break;
		}

	}

    cleanup_platform();
    return 0;
}


int init(){
	int Status;
	XSpi_Config *config;

	init_platform();

	config = XSpi_LookupConfig(XPAR_AXI_QUAD_SPI_0_DEVICE_ID);
	if (config == NULL) {
		return XST_FAILURE;
	}
	Status = XSpi_CfgInitialize(&DSPI, config,
				config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * The SPI device is a slave by default and the clock phase and polarity
	 * have to be set according to its master. SPI Mode 0
	 */
	Status = XSpi_SetOptions(&DSPI, 0);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Start the SPI driver so that the device is enabled.
	 */
	XSpi_Start(&DSPI);

	/*
	 * Disable Global interrupt to use polled mode operation.
	 */
	XSpi_IntrGlobalDisable(&DSPI);

	xil_printf("USB104A7-dspi Demo Initialized.\r\n");

	return XST_SUCCESS;
}
