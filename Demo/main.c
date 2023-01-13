/*---------------------------------------------------------------------------*
 * External SRAM and SPI demo firmware for MDR1986BE1T (QI).                 *
 *                                                                           *
 * The firmware write data (counter) into External SRAM and reab it back.    *
 * Red/Green led indicates I/O error or succes during memory read/write.     *
 * LED Display shows memory speed (/10) via Max7221 (spi-control).           *
 *                                                                           *
 *                                                                           *
 * Version 1.0 January 2020                                                  *
 *---------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include <MDR32F9Qx_config.h>
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_port.h>
#include <MDR32F9Qx_ssp.h>
#include <MDR32F9Qx_uart.h>
#include <MDR32F9Qx_ebc.h>
#include <MDR32F9Qx_timer.h>
#include <MDR32F9Qx_eeprom.h>

// PortB
#define MDB_LED_BLUE         PORT_Pin_2
#define MDB_LED_GREEN        PORT_Pin_3
#define MDB_LED_RED          PORT_Pin_4
#define MDB_LED_YELLOW       PORT_Pin_5
#define MDB_LED_WHITE        PORT_Pin_6
#define MDB_MAX7221_CS       PORT_Pin_10
#define MDB_FLASH_CS         PORT_Pin_11

// PortC
#define MDB_SRAM_CE          PORT_Pin_2

// PortE
#define MDB_LED_ETHG         PORT_Pin_9
#define MDB_LED_ETHY         PORT_Pin_10

volatile uint16_t *pMemory = (uint16_t*)0x60000000UL;

static void    Port_Init   (void);
static void    SysClk_Init (void);
static void    Timer1_Init (void);
static void    Spi1_Init   (void);
static uint8_t Spi_io      (MDR_SSP_TypeDef* SSPx, uint8_t value);
static void    MAX7221_Init(void);
static void    MAX7221_cmd (uint8_t addr, uint8_t value);
static void    MAX7221_out (uint32_t num, uint8_t icons);
static void    SRAM_Init   (void);
static void    SRAM_Write  (uint32_t count);
static bool    SRAM_Read   (uint32_t count);

//===========================================================================//
void Delay(volatile uint32_t n)
{
	while( n-- > 0 ) ;
}

//===========================================================================//
int main()
{
	uint32_t i = 0;

	Port_Init();
	SysClk_Init();
	Timer1_Init();
	Spi1_Init();
	MAX7221_Init();
	SRAM_Init();

	//---------------------
	while( 1 )
	{
		SRAM_Write(80000);
		if( SRAM_Read(80000) )
		{
			// Success
			PORT_SetBits (MDR_PORTB, MDB_LED_GREEN);
		}
		else
		{
			// Failed
			PORT_SetBits (MDR_PORTB, MDB_LED_RED);
		}
		//--- blinking ---
		if( (i % 10)==0 )
		{
			if( PORT_ReadInputDataBit(MDR_PORTB, MDB_LED_BLUE) )
			{
				PORT_ResetBits (MDR_PORTB, MDB_LED_BLUE);
			}
			else
			{
				PORT_SetBits (MDR_PORTB, MDB_LED_BLUE);
			}
		}
		// Display counter
		MAX7221_out (i++, 0);
	}
}

//===========================================================================//
// Configure PORTB pins: 2,3,4,5,6 for output to switch LED on/off           //
//===========================================================================//
void Port_Init ( void )
{
	PORT_InitTypeDef initStruct;

	PORT_StructInit (&initStruct);

	// Reset
	RST_CLK_PCLKcmd (RST_CLK_PCLK_RST_CLK, ENABLE);

	// Port enable
	RST_CLK_PCLKcmd( RST_CLK_PCLK_PORTA, ENABLE );
	RST_CLK_PCLKcmd( RST_CLK_PCLK_PORTB, ENABLE );
	RST_CLK_PCLKcmd( RST_CLK_PCLK_PORTC, ENABLE );
	RST_CLK_PCLKcmd( RST_CLK_PCLK_PORTD, ENABLE );
	RST_CLK_PCLKcmd( RST_CLK_PCLK_PORTE, ENABLE );
	RST_CLK_PCLKcmd( RST_CLK_PCLK_PORTF, ENABLE );

	initStruct.PORT_FUNC  = PORT_FUNC_PORT;
	initStruct.PORT_OE	  = PORT_OE_OUT;
	initStruct.PORT_MODE  = PORT_MODE_DIGITAL;
	initStruct.PORT_SPEED = PORT_SPEED_FAST;
	initStruct.PORT_Pin   = MDB_LED_WHITE |
	                        MDB_LED_BLUE |
	                        MDB_LED_GREEN |
	                        MDB_LED_YELLOW |
	                        MDB_LED_RED |
	                        MDB_MAX7221_CS;
	PORT_Init (MDR_PORTB, &initStruct);

	initStruct.PORT_Pin   = MDB_LED_ETHY | MDB_LED_ETHG;
	PORT_Init (MDR_PORTE, &initStruct);
}

//===========================================================================//
// 80 MHz, HSEdiv1, PLLmul10                                                 //
//===========================================================================//
void SysClk_Init ( void )
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
	EEPROM_SetLatency (EEPROM_Latency_4);

	// HSE on
	RST_CLK_HSEconfig (RST_CLK_HSE_ON);
	while( RST_CLK_HSEstatus() != SUCCESS ) ;

	// CPU_C1_SEL = HSE, 16 x 5 = 80MHz
	//RST_CLK_CPU_PLLconfig ( RST_CLK_CPU_PLLsrcHSEdiv1, RST_CLK_CPU_PLLmul5 ); // 80MHz
	RST_CLK_CPU_PLLconfig ( RST_CLK_CPU_PLLsrcHSEdiv1, RST_CLK_CPU_PLLmul6 );   // 96MHz
	//RST_CLK_CPU_PLLconfig ( RST_CLK_CPU_PLLsrcHSEdiv1, RST_CLK_CPU_PLLmul8 ); // 128MHz
	//RST_CLK_CPU_PLLconfig ( RST_CLK_CPU_PLLsrcHSEdiv2, RST_CLK_CPU_PLLmul15 );// 120MHz
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
// 2 MHz, SPI #1, Master, PortC[5,6,7,8] @ALTER                              //
//===========================================================================//
void Spi1_Init (void)
{
	SSP_InitTypeDef  ssp;
	PORT_InitTypeDef initStruct;

	RST_CLK_PCLKcmd (RST_CLK_PCLK_SSP1, ENABLE);
	//-----------------------------------------//
	// PortC: 5-TX, 7-CLK, 8-FSS, 6-RXD (in)   //
	//-----------------------------------------//
	PORT_StructInit (&initStruct);
	
	initStruct.PORT_FUNC    = PORT_FUNC_ALTER;
	initStruct.PORT_MODE    = PORT_MODE_DIGITAL;
	initStruct.PORT_SPEED   = PORT_SPEED_FAST;
	initStruct.PORT_Pin     = PORT_Pin_5 | PORT_Pin_7 | PORT_Pin_8;
	initStruct.PORT_OE      = PORT_OE_OUT;
	PORT_Init (MDR_PORTC, &initStruct);
	
	initStruct.PORT_Pin     = PORT_Pin_6;
	initStruct.PORT_OE      = PORT_OE_IN;
	PORT_Init (MDR_PORTC, &initStruct);

	//-----------------------------------------//
	// Reset all SSP settings                  //
	//-----------------------------------------//
	SSP_DeInit (MDR_SSP1);
	//SSP_BRGInit (MDR_SSP1, SSP_HCLKdiv1);
	SSP_BRGInit (MDR_SSP1, SSP_HCLKdiv2);
	SSP_StructInit (&ssp);

	//-----------------------------------------//
	// 80MHz base clock, Speed: 1Mbit/s        //
	//-----------------------------------------//
	// 80M / ( 8 * (1+0x9) )
	ssp.SSP_SCR                 = 0x09;
	ssp.SSP_CPSDVSR             = 8; //2..12
	ssp.SSP_Mode                = SSP_ModeMaster;
	ssp.SSP_WordLength          = SSP_WordLength8b;
	ssp.SSP_SPH                 = SSP_SPH_1Edge;
	ssp.SSP_SPO                 = SSP_SPO_Low;
	ssp.SSP_FRF                 = SSP_FRF_SPI_Motorola;
	ssp.SSP_HardwareFlowControl = SSP_HardwareFlowControl_SSE;
	SSP_Init (MDR_SSP1, &ssp);

	Delay(10); //!bug fix!
	SSP_Cmd (MDR_SSP1, ENABLE);
}

//===========================================================================//
// Read/Write SPI data                                                       //
//===========================================================================//
uint8_t Spi_io (MDR_SSP_TypeDef* ssp, uint8_t out)
{
	uint16_t in = 0;

	// MASTER or SLAVE Mode
	if( (ssp->CR1 & SSP_CR1_MS)==0 )
	{
		while( (ssp->SR & SSP_FLAG_TFE)==0 ) ;
		SSP_SendData(ssp, out);
		while( ssp->SR & SSP_FLAG_BSY ) ;
		in = SSP_ReceiveData(ssp);
	}
	else 
	{
		while( (ssp->SR & SSP_FLAG_RNE)==0 ) ;
		in = SSP_ReceiveData(ssp);
		while( (ssp->SR & SSP_FLAG_TFE)==0 ) ;
		SSP_SendData(ssp, out);
	}
	return (uint8_t)in;
}

//===========================================================================//
// MAX7221 - 7-segment led driver control functions.                         //
//===========================================================================//
void MAX7221_cmd (uint8_t addr, uint8_t value)
{
	PORT_ResetBits (MDR_PORTB, MDB_MAX7221_CS);
	Spi_io (MDR_SSP1, addr);
	Spi_io (MDR_SSP1, value);
	PORT_SetBits (MDR_PORTB, MDB_MAX7221_CS);
}

void MAX7221_out(uint32_t num, uint8_t icons)
{
	MAX7221_cmd( 4, (num / 1) % 10 );    // decimal
	MAX7221_cmd( 3, (num / 10) % 10 );   // tenths
	MAX7221_cmd( 2, (num / 100) % 10 );  // hundredths
	MAX7221_cmd( 1, (num / 1000) % 10 ); // thousandths
	MAX7221_cmd( 5, icons );
}

void MAX7221_Init(void)
{
	MAX7221_cmd(0x0C, 0);   // shutdown
	MAX7221_cmd(0x0C, 1);   // turn-on
	MAX7221_cmd(0x0B, 4);   // scanlimit
	MAX7221_cmd(0x0A, 4);   // intensity
	MAX7221_cmd(0x09, 0xF); // decode-mode
	MAX7221_cmd(0,    0);   // clear
}

//===========================================================================//
// Timer #1 @ 1 ms (1000us)                                                  //
//===========================================================================//
void Timer1_Init (void)
{
	TIMER_CntInitTypeDef timerInitStruct = { 0 };
	RST_CLK_FreqTypeDef  rstClkFreqStruct = { 0 };

	RST_CLK_PCLKcmd(RST_CLK_PCLK_TIMER1, ENABLE);

	TIMER_DeInit(MDR_TIMER1);
	TIMER_CntStructInit(&timerInitStruct);
	TIMER_BRGInit(MDR_TIMER1, TIMER_HCLKdiv1);
	RST_CLK_GetClocksFreq(&rstClkFreqStruct);

	timerInitStruct.TIMER_CounterMode = TIMER_CntMode_ClkFixedDir;
	timerInitStruct.TIMER_Period      = rstClkFreqStruct.CPU_CLK_Frequency/1000000 * 1000;
	TIMER_CntInit(MDR_TIMER1, &timerInitStruct);
	TIMER_ITConfig(MDR_TIMER1, TIMER_STATUS_CNT_ARR, ENABLE);

	NVIC_EnableIRQ(TIMER1_IRQn);
	TIMER_Cmd(MDR_TIMER1, ENABLE);
}

//===========================================================================//
void TIMER1_IRQHandler(void)
{
	static uint16_t time, on;
	TIMER_ClearITPendingBit (MDR_TIMER1, TIMER_STATUS_CNT_ARR);
	if( (++time)==500 )
	{
		time = 0;
		on = !on;
		if( on )
			PORT_SetBits (MDR_PORTE, MDB_LED_ETHY|MDB_LED_ETHG);
		else
			PORT_ResetBits (MDR_PORTE, MDB_LED_ETHY|MDB_LED_ETHG);
	}
	
}

//===========================================================================//
// 256k x 16, SRAM                                                           //
//===========================================================================//
void SRAM_Init (void)
{
	EBC_InitTypeDef          EBC_InitStruct = { 0 };
	EBC_MemRegionInitTypeDef EBC_MemRegionInitStruct = { 0 };
	PORT_InitTypeDef         initStruct = { 0 };

	RST_CLK_PCLKcmd (RST_CLK_PCLK_EBC, ENABLE);

	PORT_StructInit (&initStruct);
	//--------------------------------------------//
	// DATA PA0..PA15 (D0..D15)                   //
	//--------------------------------------------//
	initStruct.PORT_MODE      = PORT_MODE_DIGITAL;
	initStruct.PORT_PD_SHM    = PORT_PD_SHM_ON;
	initStruct.PORT_SPEED     = PORT_SPEED_FAST;
	initStruct.PORT_FUNC      = PORT_FUNC_MAIN;
	initStruct.PORT_Pin       = PORT_Pin_All;
	PORT_Init (MDR_PORTA, &initStruct);	
	//--------------------------------------------//
	// Address PF3-PF15 (A0..A12), A0 - not used. //
	//--------------------------------------------//
	initStruct.PORT_FUNC      = PORT_FUNC_ALTER;
	initStruct.PORT_Pin       = PORT_Pin_4  | PORT_Pin_5  |
	                            PORT_Pin_6  | PORT_Pin_7  |
	                            PORT_Pin_8  | PORT_Pin_9  |
	                            PORT_Pin_10 | PORT_Pin_11 |
	                            PORT_Pin_12 | PORT_Pin_13 |
	                            PORT_Pin_14 | PORT_Pin_15;
	PORT_Init (MDR_PORTF, &initStruct);	
	//--------------------------------------------//
	// Address PD3..PD0 (A13..A16)                //
	//--------------------------------------------//
	initStruct.PORT_FUNC      = PORT_FUNC_OVERRID;
	initStruct.PORT_Pin       = PORT_Pin_0 | PORT_Pin_1 |
	                            PORT_Pin_2 | PORT_Pin_3;
	PORT_Init (MDR_PORTD, &initStruct);	
	//--------------------------------------------//
	// Address PE3, PE4 (A17, A18)                //
	//--------------------------------------------//
	initStruct.PORT_FUNC      = PORT_FUNC_ALTER;
	initStruct.PORT_Pin       = PORT_Pin_3 | PORT_Pin_4;
	PORT_Init (MDR_PORTE, &initStruct);	
	//--------------------------------------------//
	// Control PC0,PC1 (nWE,nOE)                  //
	//--------------------------------------------//
	initStruct.PORT_FUNC      = PORT_FUNC_MAIN;
	initStruct.PORT_Pin       = PORT_Pin_0 | PORT_Pin_1;
	PORT_Init (MDR_PORTC, &initStruct);	
	//--------------------------------------------//
	// Control PC2 (nCE)                          //
	//--------------------------------------------//
	initStruct.PORT_PD        = PORT_PD_DRIVER;
	initStruct.PORT_OE        = PORT_OE_OUT;
	initStruct.PORT_FUNC      = PORT_FUNC_PORT;
	initStruct.PORT_Pin       = MDB_SRAM_CE;
	PORT_Init (MDR_PORTC, &initStruct);	

	//--------------------------------------------//
	// Initialize EBC controler                   //
	//--------------------------------------------//
	EBC_DeInit();
	EBC_StructInit(&EBC_InitStruct);
	EBC_InitStruct.EBC_Mode             = EBC_MODE_RAM;
	EBC_InitStruct.EBC_WaitState        = EBC_WAIT_STATE_3HCLK;
	EBC_InitStruct.EBC_DataAlignment    = EBC_EBC_DATA_ALIGNMENT_16;
	EBC_Init(&EBC_InitStruct);
	
	EBC_MemRegionStructInit(&EBC_MemRegionInitStruct);
	EBC_MemRegionInitStruct.WS_Active   = 2;
	EBC_MemRegionInitStruct.WS_Setup    = EBC_WS_SETUP_CYCLE_1HCLK;
	EBC_MemRegionInitStruct.WS_Hold     = EBC_WS_HOLD_CYCLE_1HCLK;
	EBC_MemRegionInitStruct.Enable_Tune = ENABLE;
	EBC_MemRegionInit (&EBC_MemRegionInitStruct, EBC_MEM_REGION_60000000);
	EBC_MemRegionCMD(EBC_MEM_REGION_60000000, ENABLE);

	// Turn ON RAM (nCE)
	PORT_ResetBits (MDR_PORTC, MDB_SRAM_CE);
}

//===========================================================================//
void SRAM_Write(uint32_t count)
{
	uint32_t i;
	for( i = 0; i < count; i++ )
	{
		pMemory[i] = (uint16_t)(i);
	}
}

bool SRAM_Read(uint32_t count)
{
	uint32_t i;
	for( i = 0; i < count; i++ )
	{
		if( pMemory[i] != ((uint16_t)i) )
		{
			return 0;
		}
	}
	return 1;
}

//===========================================================================//
// ToDo: UART Demo
/**********
		volatile uint16_t uch16;

		while ( UART_GetFlagStatus(MDR_UART2, UART_FLAG_RXFE)==SET ) ;
		uch16 = UART_ReceiveData (MDR_UART2);

		while ( UART_GetFlagStatus(MDR_UART2, UART_FLAG_TXFF)==SET ) ;
		UART_SendData (MDR_UART2, uch16);
*********/


