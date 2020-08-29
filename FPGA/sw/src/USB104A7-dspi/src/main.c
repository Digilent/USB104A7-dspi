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
#include "xil_testmem.h"
#include "xintc.h"

#define BUFFER_SIZE 4
XIntc INTERRUPTC;
XSpi DSPI;
u8 WriteBuffer[4];
u8 ReadBuffer[4];

#define N_REGISTERS 64
#define BTNREG 0
#define LEDREG 1
volatile u8 RegisterSet[N_REGISTERS];
u8 cmd=0;
u8 reg=0;

volatile u8 transferDone=0;
volatile u8 slaveSelected=0;

int init();

void DSPI_Interrupt_Handler(void *CallBackRef, u32 StatusEvent, u32 ByteCount){
	if(StatusEvent == XST_SPI_SLAVE_MODE){//Slave select low
		//Load registers with current system state when SS goes low.
		RegisterSet[BTNREG] = Xil_In32(XPAR_AXI_GPIO_BTNS_LEDS_BASEADDR);
		RegisterSet[LEDREG] = Xil_In32(XPAR_AXI_GPIO_BTNS_LEDS_BASEADDR+8);

	}
	if(StatusEvent == XST_SPI_TRANSFER_DONE){
		/*
		 * Tell main loop that a command has been received.
		 */
		transferDone=1;
	}
}

int main()
{
	int Status;
	if((Status=init())!=XST_SUCCESS){
		xil_printf("Error %d during initialization. Exiting.\r\n", Status);
	}

	/*
	 * Prepare the data buffers for transmission and to receive data
	 * when the SPI device is selected by a master.
	 */
	XSpi_Transfer(&DSPI, WriteBuffer, ReadBuffer, 2);

	while(1){

		if(transferDone==1){
			transferDone=0;
			cmd = ReadBuffer[0];
			reg = ReadBuffer[1];
			xil_printf("Recv %02X %X\r\n", cmd, reg);
			switch(cmd){
				case 0xAA://Write op
					Status = XSpi_Transfer(&DSPI, WriteBuffer, ReadBuffer, 1);//Read data
					if(Status!=XST_SUCCESS){
						xil_printf("spi: %d\r\n", Status);
					}
					xil_printf("Write op received\r\n");
					while(transferDone==0);
					transferDone=0;
					RegisterSet[reg] = ReadBuffer[0];
					xil_printf("Register %d set to: 0x%02X\r\n", reg, RegisterSet[reg]);
					if(reg == LEDREG){
						Xil_Out32(XPAR_AXI_GPIO_BTNS_LEDS_BASEADDR+8, RegisterSet[LEDREG]);
					}
					break;
				case 0xBB://Read op
					WriteBuffer[0] = RegisterSet[reg];
					Status= XSpi_Transfer(&DSPI, WriteBuffer, ReadBuffer, 1);
					if(Status!=XST_SUCCESS){
						xil_printf("spi: %d\r\n", Status);
					}
					xil_printf("Read op received\r\n");
					while(transferDone==0);
					transferDone=0;
					break;
				default:
					xil_printf("Invalid command received: 0x%02X", ReadBuffer[0]);
					break;
			}

			XSpi_Transfer(&DSPI, WriteBuffer, ReadBuffer, 2);

		}

	}

    cleanup_platform();
    return 0;
}


int init(){
	int Status;
	XSpi_Config *config;

	/*
	 * Initialize the base platform by enabling caches & uart controller.
	 */
	init_platform();


	/*
	 * Initialize the SPI driver so that it is  ready to use.
	 */
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
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	Status = XIntc_Initialize(&INTERRUPTC, XPAR_AXI_INTC_0_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Error initializing INTC\r\n");
		return XST_FAILURE;
	}

	/*
	 * Connect a device driver handler that will be called when an interrupt
	 * for the device occurs, the device driver handler performs the
	 * specific interrupt processing for the device.
	 */
	Status = XIntc_Connect(&INTERRUPTC, XPAR_INTC_0_SPI_0_VEC_ID,
				(XInterruptHandler) XSpi_InterruptHandler,
				(void *)&DSPI);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the SPI can cause interrupts through the interrupt controller.
	 */
	Status = XIntc_Start(&INTERRUPTC, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
	   return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the SPI device.
	 */
	XIntc_Enable(&INTERRUPTC, XPAR_INTC_0_SPI_0_VEC_ID);

	/* Enable interrupts from the hardware */
	Xil_ExceptionInit();
	// Register the interrupt controller handler with the exception table.
	// This is in fact the ISR dispatch routine, which calls our ISRs
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
				(Xil_ExceptionHandler)XIntc_InterruptHandler,
				&INTERRUPTC);
	Xil_ExceptionEnable();


	/*
	 * Setup the handler for the SPI that will be called from the interrupt
	 * context when an SPI status occurs, specify a pointer to the SPI
	 * driver instance as the callback reference so the handler is able to
	 * access the instance data.
	 */
	XSpi_SetStatusHandler(&DSPI, &DSPI,
				(XSpi_StatusHandler) DSPI_Interrupt_Handler);

	/*
	 * Start the SPI driver so that the device is enabled.
	 */
	XSpi_Start(&DSPI);

	/*
	 * Set Buttons as inputs
	 */
	Xil_Out32(XPAR_AXI_GPIO_BTNS_LEDS_BASEADDR+4, 0xFFFF);

	/*
	 * Set LEDs low and as outputs
	 */
	Xil_Out32(XPAR_AXI_GPIO_BTNS_LEDS_BASEADDR+8, 0);
	Xil_Out32(XPAR_AXI_GPIO_BTNS_LEDS_BASEADDR+12, 0);

	xil_printf("USB104A7-dspi Demo Initialized.\r\n");

	return XST_SUCCESS;
}
