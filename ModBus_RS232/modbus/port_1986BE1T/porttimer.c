/*
 * FreeModbus Libary: Milandr 1986BE1T Port
 * This library is free software; you can redistribute it and/or modify it.
 * CopyLeft (C) 2022
 *
 * TIMER #1 (32-bit)
 */

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_timer.h>

// Timer counter settings
static TIMER_CntInitTypeDef timerInitStruct = { 0 };

/* ----------------------- static functions ---------------------------------*/
static void prvvTIMERExpiredISR( void );

/* ----------------------- Start implementation ------------------------------*
 * Timer ticks are counted in multiples of 50us.                              *
 * Therefore 20000 ticks are one second.                                      *
 *----------------------------------------------------------------------------*/
BOOL
xMBPortTimersInit (USHORT usTim1Timerout50us)
{
	RST_CLK_FreqTypeDef  rstClkFreqStruct = { 0 };
	ULONG                timeout = 50UL * usTim1Timerout50us;
	
	RST_CLK_PCLKcmd (RST_CLK_PCLK_TIMER1, ENABLE);

	TIMER_DeInit (MDR_TIMER1);
	TIMER_BRGInit (MDR_TIMER1, TIMER_HCLKdiv8);
	TIMER_CntStructInit (&timerInitStruct);
	RST_CLK_GetClocksFreq (&rstClkFreqStruct);

	timerInitStruct.TIMER_CounterMode = TIMER_CntMode_ClkFixedDir;
	timerInitStruct.TIMER_Period = (rstClkFreqStruct.CPU_CLK_Frequency / 8000000UL) * timeout;

	TIMER_CntInit (MDR_TIMER1, &timerInitStruct);
	TIMER_ITConfig (MDR_TIMER1, TIMER_STATUS_CNT_ARR, ENABLE);

	NVIC_EnableIRQ (TIMER1_IRQn);
	TIMER_Cmd (MDR_TIMER1, ENABLE);

	return TRUE;
}

//inline 
void vMBPortTimersEnable ()
{
	// Enable the timer with the timeout passed to xMBPortTimersInit( )
	TIMER_CntInit (MDR_TIMER1, &timerInitStruct);
	TIMER_Cmd (MDR_TIMER1, ENABLE);
}

//inline 
void vMBPortTimersDisable ()
{
	// Disable any pending timers.
	TIMER_Cmd (MDR_TIMER1, DISABLE);
}

/* Create an ISR which is called whenever the timer has expired. This function
 * must then call pxMBPortCBTimerExpired( ) to notify the protocol stack that
 * the timer has expired.
 */
static void prvvTIMERExpiredISR (void)
{
	(void)pxMBPortCBTimerExpired();
}

//===========================================================================//
void TIMER1_IRQHandler (void)
{
	TIMER_ClearITPendingBit (MDR_TIMER1, TIMER_STATUS_CNT_ARR);
	prvvTIMERExpiredISR ();
}
