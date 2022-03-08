/*---------------------------------------------------------------------------*
 * ModBus RTU demo firmware for MDR1986BE1T (QI).                            *
 *                                                                           *
 * The firmware creates ModBus RTU Slave device with address 0x0A and        *
 * can read/write Coils commands at address 0. [1..8]                        *
 * Write Coils turns on/off single Led (0-blue, 1-green, 2-red, 3-yellow).   *
 * Read Coils returns Led status and Buttons state (1-pressed, 0-released).  *
 *                                                                           *
 * Note #1: Add paths to 'modbus\*' sources in cfg dialog.                   *
 * Note #2: Address: 0-based, Number: 1-based.                               *
 * Note #3: Check: mbrtu.c Line 215.                                         *
 * Note #4: HSE_Value in MDR32F9Qx_config.h set to 16MHz                     *
 * More info: https://github.com/cwalter-at/freemodbus/issues/16             *
 *                                                                           *
 * Version 1.0 Feb 2022                                                      *
 *---------------------------------------------------------------------------*/

#include <stdint.h>
#include <MDR32F9Qx_port.h>
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_uart.h>
#include "modbus/include/mb.h"
#include "modbus/include/mbutils.h"

#define BTN_USER1            PORT_Pin_0
#define BTN_USER2            PORT_Pin_1
#define LED_BLUE             PORT_Pin_2
#define LED_GREEN            PORT_Pin_3
#define LED_RED              PORT_Pin_4
#define LED_YELLOW           PORT_Pin_5
#define LED_WHITE            PORT_Pin_6

// Address: 0-based, Number: 1-based.
#define REG_COILS_START      1
#define REG_COILS_SIZE       8

#define REG_HOLDING_START    1
#define REG_HOLDING_NREGS    32

static UCHAR ucRegCoilsBuf[REG_COILS_SIZE / 8];
static USHORT usRegHoldingBuf[REG_HOLDING_NREGS] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static void CLK_Init (void);
static void LED_Init (void);
static void BTN_Init (void);

//===========================================================================//
// main()                                                                    //
//===========================================================================//
int main()
{
	volatile eMBErrorCode eStatus;

	CLK_Init ();
	LED_Init ();
	BTN_Init ();

//	eStatus = eMBInit (MB_RTU, 0x0A, 0, 9600, MB_PAR_NONE);
	eStatus = eMBInit (MB_RTU, 0x0A, 0, 115200, MB_PAR_NONE);
	eStatus = eMBEnable ();

	while (1)
	{
		eStatus = eMBPoll ();

		// Turn leds on/off
		if (ucRegCoilsBuf[0] & 0x01) PORT_SetBits (MDR_PORTB, LED_BLUE);
		else                         PORT_ResetBits (MDR_PORTB, LED_BLUE);

		if (ucRegCoilsBuf[0] & 0x02) PORT_SetBits (MDR_PORTB, LED_GREEN);
		else                         PORT_ResetBits (MDR_PORTB, LED_GREEN);

		if (ucRegCoilsBuf[0] & 0x04) PORT_SetBits (MDR_PORTB, LED_RED);
		else                         PORT_ResetBits (MDR_PORTB, LED_RED);

		if (ucRegCoilsBuf[0] & 0x08) PORT_SetBits (MDR_PORTB, LED_YELLOW);
		else                         PORT_ResetBits (MDR_PORTB, LED_YELLOW);

		// Return button state
		if (PORT_ReadInputDataBit(MDR_PORTB, BTN_USER1)) ucRegCoilsBuf[0] |= 0x10;
		else                                             ucRegCoilsBuf[0] &= ~0x10;

		if (PORT_ReadInputDataBit(MDR_PORTB, BTN_USER2)) ucRegCoilsBuf[0] |= 0x20;
		else                                             ucRegCoilsBuf[0] &= ~0x20;
	}
}

//===========================================================================//
// 80 MHz (16*5), HSEdiv1, PLLmul5, Default FLASH latency (4)                //
//===========================================================================//
void CLK_Init (void)
{
	RST_CLK_HSEconfig (RST_CLK_HSE_ON);
	while (RST_CLK_HSEstatus() != SUCCESS) ;

	RST_CLK_CPU_PLLconfig (RST_CLK_CPU_PLLsrcHSEdiv1, RST_CLK_CPU_PLLmul5);
	RST_CLK_CPU_PLLcmd (ENABLE);
	while (RST_CLK_CPU_PLLstatus() != SUCCESS) ;

	RST_CLK_CPUclkPrescaler (RST_CLK_CPUclkDIV1);
	RST_CLK_CPU_PLLuse (ENABLE);
	RST_CLK_CPUclkSelection (RST_CLK_CPUclkCPU_C3);
}

//===========================================================================//
// Configure PORT_B pins: 2,3,4,5,6 for output to switch LED on/off          //
//===========================================================================//
void LED_Init (void)
{
	PORT_InitTypeDef initStruct;

	PORT_StructInit (&initStruct);

	RST_CLK_PCLKcmd (RST_CLK_PCLK_PORTB, ENABLE);

	initStruct.PORT_FUNC  = PORT_FUNC_PORT;
	initStruct.PORT_OE    = PORT_OE_OUT;
	initStruct.PORT_MODE  = PORT_MODE_DIGITAL;
	initStruct.PORT_SPEED = PORT_SPEED_FAST;
	initStruct.PORT_Pin   = LED_WHITE | LED_BLUE | LED_GREEN | LED_YELLOW | LED_RED;
	PORT_Init (MDR_PORTB, &initStruct);
}

//===========================================================================//
// Buttons use PB0, PB1                                                      //
//===========================================================================//
void BTN_Init (void)
{
	PORT_InitTypeDef initStruct;

	PORT_StructInit (&initStruct);

	RST_CLK_PCLKcmd (RST_CLK_PCLK_PORTB, ENABLE);

	initStruct.PORT_FUNC   = PORT_FUNC_PORT;
	initStruct.PORT_OE     = PORT_OE_IN;
	initStruct.PORT_MODE   = PORT_MODE_DIGITAL;
	initStruct.PORT_SPEED  = PORT_SPEED_FAST;
	initStruct.PORT_PD_SHM = PORT_PD_SHM_ON;
	initStruct.PORT_Pin    = BTN_USER1 | BTN_USER2;
	PORT_Init (MDR_PORTB, &initStruct);
}

//===========================================================================//
// Coils - 1 bit, read/write                                                 //
//===========================================================================//
eMBErrorCode
eMBRegCoilsCB (UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils, eMBRegisterMode eMode)
{
	eMBErrorCode    eStatus = MB_ENOERR;
	SHORT           iNCoils = (SHORT)usNCoils;
	USHORT          usBitOffset;

	if ( (usAddress >= REG_COILS_START) && (usAddress + usNCoils <= REG_COILS_START + REG_COILS_SIZE) )
	{
		usBitOffset = (USHORT)(usAddress - REG_COILS_START);
		switch (eMode)
		{
			case MB_REG_READ:
				while (iNCoils > 0)
				{
					*pucRegBuffer++ = xMBUtilGetBits (ucRegCoilsBuf, usBitOffset, (iNCoils > 8 ? 8 : iNCoils));
					iNCoils -= 8;
					usBitOffset += 8;
				}
				break;
			case MB_REG_WRITE:
				while (iNCoils > 0)
				{
					xMBUtilSetBits (ucRegCoilsBuf, usBitOffset, (iNCoils > 8 ? 8 : iNCoils), *pucRegBuffer++);
					iNCoils -= 8;
				}
				break;
		}
	}
	else
	{
		eStatus = MB_ENOREG;
	}
	return eStatus;
}

//===========================================================================//
// Holding register - 16-bit, read/write                                     //
//===========================================================================//
eMBErrorCode
eMBRegHoldingCB (UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode)
{
	eMBErrorCode    eStatus = MB_ENOERR;
	int             iRegIndex;

	if ((usAddress >= REG_HOLDING_START) && (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS))
	{
		iRegIndex = (int)(usAddress - REG_HOLDING_START);
		switch (eMode)
		{
			case MB_REG_READ:
				while (usNRegs > 0)
				{
					*pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[iRegIndex] >> 8);
					*pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[iRegIndex] & 0xFF);
					iRegIndex++;
					usNRegs--;
				}
				break;
			case MB_REG_WRITE:
				while (usNRegs > 0)
				{
					usRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
					usRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
					iRegIndex++;
					usNRegs--;
				}
				break;
		}
	}
	else
	{
		eStatus = MB_ENOREG;
	}

	return eStatus;
}

//===========================================================================//
// Discrete - 1 bit, read only                                               //
//===========================================================================//
eMBErrorCode
eMBRegDiscreteCB (UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete)
{
	return MB_ENOREG;
}

//===========================================================================//
// Input register - 16-bit, read only                                        //
//===========================================================================//
eMBErrorCode
eMBRegInputCB (UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs)
{
	return MB_ENOREG;
}
