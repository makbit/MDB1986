/*---------------------------------------------------------------------------*
 * Project:  MIL_STD_1553B simple input/output demonstration firmware.       *
 * Info:     C-style initialization of the BusController and                 *
 *           transfer of 4 words ("12 34 56 78") to RT with TDA = 1.         *
 * Platform: Milandr 1986VE1T (plasic QFP-144)                               *
 * File:     M1553io/main.c                                                  *
 * Version:  0.1 Feb 2020                                                    *
 * Author:   MKM                                                             *
 *---------------------------------------------------------------------------*/
#include <stdint.h>
#include <MDR32F9Qx_port.h>
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_timer.h>
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_eeprom.h>
#include <MDR32F9Qx_mil_std_1553.h>
#include "ms1553.h"

// PortB
#define MDB_LED_BLUE                   PORT_Pin_2
#define MDB_LED_GREEN                  PORT_Pin_3
#define MDB_LED_RED                    PORT_Pin_4
#define MDB_LED_YELLOW                 PORT_Pin_5
#define MDB_LED_WHITE                  PORT_Pin_6
#define MDB_MAX7221_CS                 PORT_Pin_10
#define MDB_FLASH_CS                   PORT_Pin_11

// 1ms time counter
//extern 
uint32_t g_msTickCount;

extern void LED_Init (void);
extern void SYSCLK_Init (void);
extern void TIMER1_Init (void);
extern void MS1553_Init_BC (void);
extern void MS1553_Init_RT (void);

extern int  MS1553_Wait (uint32_t flag);
extern int  MS1553_Cmd  (uint8_t tda, uint8_t cmd);
extern int  MS1553_Send (uint8_t tda, uint8_t sub, uint8_t nWords, void *pData);
//extern int  MS1553_Recv (uint8_t tda, uint8_t sub, uint8_t nWords, void *pData);

//===========================================================================//
void DelayMS(uint32_t ms)
{
	uint32_t tick = g_msTickCount;
	while( tick + ms > g_msTickCount ) ;
}

//===========================================================================//
int main()
{
	LED_Init();
	SYSCLK_Init();
	TIMER1_Init();
	MS1553_Init_BC();

	// MS1553_Init_RT(adr);

	while(1)
	{
		PORT_ResetBits (MDR_PORTB, MDB_LED_GREEN);
		PORT_ResetBits (MDR_PORTB, MDB_LED_RED);

		PORT_SetBits (MDR_PORTB, MDB_FLASH_CS);
		MS1553_Send (1, 16, 4, "12345678");
		MS1553_Wait (MIL_STD_1553_FLAG_IDLE);
		PORT_ResetBits (MDR_PORTB, MDB_FLASH_CS);

		if( MIL_STD_1553_GetFlagStatus(MDR_MIL_STD_15531, MIL_STD_1553_FLAG_ERR)==SET )
		{
			volatile uint32_t e;
			PORT_SetBits (MDR_PORTB, MDB_LED_RED);

			e = MIL_STD_1553_GetErrorStatus(MDR_MIL_STD_15531);
			if( e ) PORT_SetBits (MDR_PORTB, MDB_LED_BLUE);
			PORT_ResetBits (MDR_PORTB, MDB_LED_BLUE);
		}
		else
		{
			PORT_SetBits (MDR_PORTB, MDB_LED_GREEN);
		}
		DelayMS(100);

		MS1553_Cmd (1, MS1553_CMD_RESET);
		//MS1553_Cmd (1, MS1553_CMD_STATUS_WORD);
		MS1553_Wait (MIL_STD_1553_FLAG_IDLE);
	}
}

//===========================================================================//
// Configure PORTB pins for LED on/off                                       //
//===========================================================================//
void LED_Init ( void )
{
	PORT_InitTypeDef PORT_InitStruct;

	PORT_StructInit (&PORT_InitStruct);
	RST_CLK_PCLKcmd (RST_CLK_PCLK_PORTB, ENABLE);

	PORT_InitStruct.PORT_FUNC  = PORT_FUNC_PORT;
	PORT_InitStruct.PORT_OE    = PORT_OE_OUT;
	PORT_InitStruct.PORT_MODE  = PORT_MODE_DIGITAL;
	PORT_InitStruct.PORT_SPEED = PORT_SPEED_FAST;
	PORT_InitStruct.PORT_Pin   = MDB_LED_BLUE | MDB_LED_GREEN |
	                             MDB_LED_YELLOW | MDB_LED_RED |
	                             MDB_MAX7221_CS | MDB_FLASH_CS;
	PORT_Init (MDR_PORTB, &PORT_InitStruct);
}

//===========================================================================//
// 80 MHz, HSEdiv1, PLLmul5                                                  //
//===========================================================================//
void SYSCLK_Init ( void )
{
	// Configure FLASH access time for speed >120MHz
	RST_CLK_PCLKcmd (RST_CLK_PCLK_EEPROM, ENABLE);
	//--------------------------------//
	// MCU Frequency | Flash Latency  //
	//---------------|----------------//
	//   <  25 MHz   |    0           //
	//   <  50 MHz   |    1           //
	//   <  75 MHz   |    2           //
	//   < 100 MHz   |    3           //
	//   < 125 MHz   |    4 (default) //
	//   < 150 MHz   |    5           //
	//   < 175 MHz   |    6           //
	//--------------------------------//
	EEPROM_SetLatency (EEPROM_Latency_3);

	// HSE on
	RST_CLK_HSEconfig (RST_CLK_HSE_ON);
	while( RST_CLK_HSEstatus() != SUCCESS ) ;

	// CPU_C1_SEL = HSE, 16 x 5 = 80MHz
	RST_CLK_CPU_PLLconfig ( RST_CLK_CPU_PLLsrcHSEdiv1, RST_CLK_CPU_PLLmul5 ); // 80MHz
	RST_CLK_CPU_PLLcmd (ENABLE);
	while( RST_CLK_CPU_PLLstatus() != SUCCESS ) ;

	// CPU_C3_SEL = CPU_C2_SEL
	RST_CLK_CPUclkPrescaler (RST_CLK_CPUclkDIV1);
	// CPU_C2_SEL = PLL
	RST_CLK_CPU_PLLuse (ENABLE);
	// HCLK_SEL = CPU_C3_SEL
	RST_CLK_CPUclkSelection (RST_CLK_CPUclkCPU_C3);
}

//===========================================================================//
// Timer #1 @ 1000us (1ms)                                                   //
//===========================================================================//
void TIMER1_Init (void)
{
	TIMER_CntInitTypeDef timer = { 0 };
	RST_CLK_FreqTypeDef  freq  = { 0 };

	RST_CLK_PCLKcmd (RST_CLK_PCLK_TIMER1, ENABLE);

	TIMER_DeInit (MDR_TIMER1);
	TIMER_CntStructInit (&timer);
	TIMER_BRGInit (MDR_TIMER1, TIMER_HCLKdiv1);
	RST_CLK_GetClocksFreq (&freq);

	timer.TIMER_CounterMode = TIMER_CntMode_ClkFixedDir;
	timer.TIMER_Period      = freq.CPU_CLK_Frequency/1000000 * 1000; // 1000us
	TIMER_CntInit (MDR_TIMER1, &timer);
	TIMER_ITConfig (MDR_TIMER1, TIMER_STATUS_CNT_ARR, ENABLE);

	NVIC_EnableIRQ(TIMER1_IRQn);
	TIMER_Cmd(MDR_TIMER1, ENABLE);
}

//===========================================================================//
void TIMER1_IRQHandler(void)
{
	TIMER_ClearITPendingBit (MDR_TIMER1, TIMER_STATUS_CNT_ARR);
	g_msTickCount++;
}

//===========================================================================//
// MS1553_Init() - initializes MIL_STD_1553 Bus Controller                   //
//===========================================================================//
void MS1553_Init_BC (void)
{
	PORT_InitTypeDef         PORT_InitStruct      = { 0 };
	MIL_STD_1553_InitTypeDef MS1553_InitStructure = { 0 };

	RST_CLK_PCLKcmd (RST_CLK_PCLK_MIL_STD_15531, ENABLE);
	RST_CLK_PCLKcmd (RST_CLK_PCLK_PORTC, ENABLE);
	RST_CLK_PCLKcmd (RST_CLK_PCLK_PORTD, ENABLE);	
	
	PORT_StructInit (&PORT_InitStruct);

	// PRMA+/-: PortC #13,14(,15)
	PORT_InitStruct.PORT_Pin   = PORT_Pin_13 | PORT_Pin_14;// | PORT_Pin_15;
	PORT_InitStruct.PORT_FUNC  = PORT_FUNC_MAIN;
	PORT_InitStruct.PORT_SPEED = PORT_SPEED_FAST;
	PORT_InitStruct.PORT_MODE  = PORT_MODE_DIGITAL;
	PORT_Init (MDR_PORTC, &PORT_InitStruct);

	// PRMD+/-: PortD #1,2,(5)
	PORT_InitStruct.PORT_Pin   = PORT_Pin_1 | PORT_Pin_2;// | PORT_Pin_5;
	PORT_InitStruct.PORT_FUNC  = PORT_FUNC_MAIN;
	PORT_InitStruct.PORT_SPEED = PORT_SPEED_FAST;
	PORT_InitStruct.PORT_MODE  = PORT_MODE_DIGITAL;
	PORT_Init (MDR_PORTD, &PORT_InitStruct);

	// Reset all MIL_STD_15531 settings
	MIL_STD_1553_DeInit (MDR_MIL_STD_15531);
	MIL_STD_1553_BRGInit (MIL_STD_1553_HCLKdiv1);
	MIL_STD_1553xStructInit (&MS1553_InitStructure);

	// Initialize MIL_STD_1553_InitStructure
	MS1553_InitStructure.MIL_STD_1553_Mode = MIL_STD_1553_ModeBusController;
	MS1553_InitStructure.MIL_STD_1553_RERR = DISABLE; // auto reset errors 
	MS1553_InitStructure.MIL_STD_1553_DIV  = 80;      // 80Mhz/80 = 1Mhz
	MS1553_InitStructure.MIL_STD_1553_RTA  = 0;       // not used for BC
	MS1553_InitStructure.MIL_STD_1553_TRA  = ENABLE;  // use main channel A
	MS1553_InitStructure.MIL_STD_1553_TRB  = DISABLE; // channel B reserved

	// Configure MIL_STD_15531 parameters
	MIL_STD_1553_Init (MDR_MIL_STD_15531, &MS1553_InitStructure);
	MIL_STD_1553_Cmd (MDR_MIL_STD_15531, ENABLE);
}

//===========================================================================//
// MS1553_Wait() - waits for MIL_STD_1553 event/status                       //
//===========================================================================//
int MS1553_Wait (uint32_t flag)
{
	uint32_t tick = g_msTickCount + 2;
	assert_param( IS_MIL_STD_1553_FLAG(flag) );
	while( MIL_STD_1553_GetFlagStatus(MDR_MIL_STD_15531, flag) != SET )
	{
		if( tick==g_msTickCount ) return ERROR;
	}
	return SUCCESS;
}

//===========================================================================//
// MS1553_Cmd() - issues command to TD                                       //
//===========================================================================//
int MS1553_Cmd  (uint8_t tda, uint8_t cmd)
{
	MIL_STD_1553_CommandWordTypeDef command;

	assert_param( tda < 32);
	assert_param( cmd <= MS1553_CMD_OVERRIDE_STX );

	command.CommandWord                  = 0;
	command.Fields.ReadWriteBit          = MIL_STD_1553_TD_TO_BC;
	command.Fields.Data                  = cmd;
	command.Fields.Subaddress            = 0;
	command.Fields.TerminalDeviceAddress = tda;
	if( cmd==MS1553_CMD_SYNC_WITH_DATA || cmd==MS1553_CMD_SHUTDOWN_SEL || cmd==MS1553_CMD_OVERRIDE_SEL )
	{
		command.Fields.ReadWriteBit      = MIL_STD_1553_BC_TO_TD;
	}

	MDR_MIL_STD_15531->CommandWord1 = command.CommandWord;
	MIL_STD_1553_StartTransmision (MDR_MIL_STD_15531);
	return SUCCESS;
}

//===========================================================================//
// MS1553_Send() - sends pData to the 1553 bus (format #1)                   //
//===========================================================================//
int MS1553_Send (uint8_t tda, uint8_t sub, uint8_t nWords, void *pData)
{
	int i;
	uint32_t aData[32];
	MIL_STD_1553_CommandWordTypeDef command;

	assert_param( tda < 32 );
	assert_param( sub < 32 );
	assert_param( nWords <= 32 );

	command.CommandWord                  = 0;
	command.Fields.ReadWriteBit          = MIL_STD_1553_BC_TO_TD;
	command.Fields.Data                  = nWords & 0x1F;
	command.Fields.Subaddress            = sub & 0x1F;
	command.Fields.TerminalDeviceAddress = tda & 0x1F;

	MDR_MIL_STD_15531->CommandWord1 = command.CommandWord;

	for( i = 0; i < nWords; i++ )
	{
		aData[i] = ((uint16_t *)pData)[i];
	}
	
	MIL_STD_1553_WiteDataToSendBuffer (MDR_MIL_STD_15531, sub, nWords, aData);
	MIL_STD_1553_StartTransmision (MDR_MIL_STD_15531);
	return SUCCESS;
}
