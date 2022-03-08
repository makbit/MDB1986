/*
 * FreeModbus Libary: Milandr 1986BE1T (QI) Port
 * This library is free software; you can redistribute it and/or modify it.
 * CopyLeft (C) 2022
 *
 * UART #2 MCU Config: Port D, Pins: 13, 14; FIFO Off, 8 data, 1(2) stop-bit.
 * Note: SendByte() can be blocking.
 */

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include <MDR32F9Qx_port.h>
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_uart.h>

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR( void );
static void prvvUARTRxISR( void );

//------------------------ Start implementation -----------------------------//
// If xRXEnable enable serial receive interrupts.                            //
// If xTxENable enable transmitter empty interrupts.                         //
//---------------------------------------------------------------------------//
void
vMBPortSerialEnable (BOOL xRxEnable, BOOL xTxEnable)
{
	UART_ITConfig (MDR_UART2, UART_IT_RX, xRxEnable ? ENABLE : DISABLE);
	UART_ITConfig (MDR_UART2, UART_IT_TX, xTxEnable ? ENABLE : DISABLE);
}

//===========================================================================//
//                                                                           //
//===========================================================================//
BOOL
xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity )
{
	UART_InitTypeDef UART_InitStructure;
	PORT_InitTypeDef PORT_InitStruct;

	RST_CLK_PCLKcmd (RST_CLK_PCLK_PORTD, ENABLE);

	//--------------------------------
	PORT_StructInit (&PORT_InitStruct);

	PORT_InitStruct.PORT_FUNC   = PORT_FUNC_MAIN;
	PORT_InitStruct.PORT_SPEED  = PORT_SPEED_FAST;
	PORT_InitStruct.PORT_MODE   = PORT_MODE_DIGITAL;
	
	PORT_InitStruct.PORT_OE     = PORT_OE_OUT;
	PORT_InitStruct.PORT_Pin    = PORT_Pin_13;
	PORT_Init (MDR_PORTD, &PORT_InitStruct);

	PORT_InitStruct.PORT_OE     = PORT_OE_IN;
	PORT_InitStruct.PORT_Pin    = PORT_Pin_14;
	PORT_Init (MDR_PORTD, &PORT_InitStruct);

	//--------------------------------
	UART_StructInit (&UART_InitStructure);

	UART_InitStructure.UART_BaudRate   = ulBaudRate;
	UART_InitStructure.UART_WordLength = UART_WordLength8b;
	UART_InitStructure.UART_FIFOMode   = UART_FIFO_OFF;
	UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_RXE |
	                                              UART_HardwareFlowControl_TXE;

	switch (eParity)
	{
		case MB_PAR_EVEN:
			UART_InitStructure.UART_Parity   = UART_Parity_Even;
			UART_InitStructure.UART_StopBits = UART_StopBits1;
			break;
		case MB_PAR_ODD:
			UART_InitStructure.UART_Parity   = UART_Parity_Odd;
			UART_InitStructure.UART_StopBits = UART_StopBits1;
			break;
		default:
			UART_InitStructure.UART_Parity   = UART_Parity_No;
			UART_InitStructure.UART_StopBits = UART_StopBits2;
			break;
	}

	RST_CLK_PCLKcmd (RST_CLK_PCLK_UART2, ENABLE);
	UART_BRGInit (MDR_UART2, UART_HCLKdiv8);
	UART_Init (MDR_UART2, &UART_InitStructure);
	NVIC_EnableIRQ (UART2_IRQn);
	UART_Cmd (MDR_UART2, ENABLE);

	return TRUE;
}

//===========================================================================//
//                                                                           //
//===========================================================================//
/* Put a byte in the UARTs transmit buffer. This function is called
 * by the protocol stack if pxMBFrameCBTransmitterEmpty( ) has been
 * called. */
BOOL
xMBPortSerialPutByte( CHAR ucByte )
{
	UART_SendData (MDR_UART2, ucByte);
	//while (MDR_UART2->FR & UART_FR_BUSY) ;
	return TRUE;
}

/* Return the byte in the UARTs receive buffer. This function is called
 * by the protocol stack after pxMBFrameCBByteReceived( ) has been called.
 */
BOOL
xMBPortSerialGetByte( CHAR * pucByte )
{
	*pucByte = (CHAR) UART_ReceiveData (MDR_UART2);
	return TRUE;
}

/* Create an interrupt handler for the transmit buffer empty interrupt
 * (or an equivalent) for your target processor. This function should then
 * call pxMBFrameCBTransmitterEmpty( ) which tells the protocol stack that
 * a new character can be sent. The protocol stack will then call 
 * xMBPortSerialPutByte( ) to send the character.
 */
static void prvvUARTTxReadyISR( void )
{
	pxMBFrameCBTransmitterEmpty ();
}

/* Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
static void prvvUARTRxISR( void )
{
	pxMBFrameCBByteReceived ();
}

//===========================================================================//
//                                                                           //
//===========================================================================//
void UART2_IRQHandler (void)
{
	if ( (UART_GetITStatus(MDR_UART2, UART_IT_RX))==SET )
	{
		UART_ClearITPendingBit (MDR_UART2, UART_IT_RX);
		prvvUARTRxISR ();
	}
	
	if ( (UART_GetITStatus(MDR_UART2, UART_IT_TX))==SET )
	{
		UART_ClearITPendingBit (MDR_UART2, UART_IT_TX);
		prvvUARTTxReadyISR ();
	}
}
