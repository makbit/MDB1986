/*---------------------------------------------------------------------------*
 * Project:  Ethernet Ping-Pong input/output demonstration firmware.         *
 * Info:     IP Address: 192.168.8.19/255.255.255.0 MAC: see tcpip.h         *
 *           ping 192.168.8.19          arp -a                               *
 * Platform: Milandr 1986VE1T (plasic QFP-144)                               *
 * Note:     tcpip.c uses TIMER1, Max stable F: 96 MHz/Lat:4 (128 MHz/Lat:7) *
 * Version:  0.1 Aug 2020, 0.2 June 2022 (tested ping+, telnet +\-, IRQ)     *
 * Author:   MKM                                                             *
 *---------------------------------------------------------------------------*/
#include <stdint.h>
#include <MDR32F9Qx_port.h>
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_timer.h>
#include <MDR32F9Qx_rst_clk.h>
#include <MDR32F9Qx_eeprom.h>
#include <MDR32F9Qx_eth.h>
#include "tcpip.h"

// PortB
#define MDB_LED_BLUE                   PORT_Pin_2
#define MDB_LED_GREEN                  PORT_Pin_3
#define MDB_LED_RED                    PORT_Pin_4
#define MDB_LED_YELLOW                 PORT_Pin_5
#define MDB_LED_WHITE                  PORT_Pin_6

// PortE
#define MDB_ETH_LED1                   PORT_Pin_9
#define MDB_ETH_LED2                   PORT_Pin_10

// 1ms time counter, extern 
static uint32_t g_msTickCount;
static uint32_t InputFrame[1514/4];

static void LED_Init (void);
static void SYSCLK_Init (void);
static void TIMER2_Init (void);
static void Ethernet_Init (void);

extern void DoNetworkStuff(MDR_ETHERNET_TypeDef*);

//===========================================================================//
//  M A I N                                                                  //
//===========================================================================//
int main()
{
	LED_Init();
	SYSCLK_Init();
	TIMER2_Init();
	Ethernet_Init();

	while(1)
	{
		// Led blinking: Led1 - RX (irq), Led2 - TX (timer).
		if( (g_msTickCount >> 9) & 1 ) // ~0.5 sec
		{
			PORT_ResetBits (MDR_PORTE, MDB_ETH_LED1);
			PORT_ResetBits (MDR_PORTE, MDB_ETH_LED2);
		}
		else
		{
			PORT_SetBits (MDR_PORTE, MDB_ETH_LED2);
		}

		// Send ARP & TCP frames.
		DoNetworkStuff(MDR_ETHERNET1);
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
	RST_CLK_PCLKcmd (RST_CLK_PCLK_PORTE, ENABLE);

	PORT_InitStruct.PORT_FUNC  = PORT_FUNC_PORT;
	PORT_InitStruct.PORT_OE    = PORT_OE_OUT;
	PORT_InitStruct.PORT_MODE  = PORT_MODE_DIGITAL;
	PORT_InitStruct.PORT_SPEED = PORT_SPEED_FAST;

	PORT_InitStruct.PORT_Pin   = MDB_LED_BLUE | MDB_LED_GREEN |
	                             MDB_LED_YELLOW | MDB_LED_RED;
	PORT_Init (MDR_PORTB, &PORT_InitStruct);

	PORT_InitStruct.PORT_Pin   = MDB_ETH_LED1 | MDB_ETH_LED2;
	PORT_Init (MDR_PORTE, &PORT_InitStruct);
}

//===========================================================================//
// 80 MHz, HSEdiv1, PLLmul5                                                  //
//===========================================================================//
void SYSCLK_Init ( void )
{
	// Configure FLASH access time for speed >120MHz
	RST_CLK_PCLKcmd (RST_CLK_PCLK_EEPROM, ENABLE);
	EEPROM_SetLatency (EEPROM_Latency_3);
	//EEPROM_SetLatency (EEPROM_Latency_4);
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

	// HSE on
	RST_CLK_HSEconfig (RST_CLK_HSE_ON);
	while( RST_CLK_HSEstatus() != SUCCESS ) ;

	// CPU_C1_SEL = HSE, 16 x 5 = 80MHz
	RST_CLK_CPU_PLLconfig ( RST_CLK_CPU_PLLsrcHSEdiv1, RST_CLK_CPU_PLLmul5 ); // 80MHz
	RST_CLK_CPU_PLLcmd (ENABLE);
	while( RST_CLK_CPU_PLLstatus() != SUCCESS ) ;

	// CPU_C2_SEL = PLL
	RST_CLK_CPU_PLLuse (ENABLE);
	// CPU_C3_SEL = CPU_C2_SEL
	RST_CLK_CPUclkPrescaler (RST_CLK_CPUclkDIV1);
	// HCLK_SEL = CPU_C3_SEL
	RST_CLK_CPUclkSelection (RST_CLK_CPUclkCPU_C3);
}

//===========================================================================//
// Timer #2 @ 1000us (1ms)                                                   //
//===========================================================================//
void TIMER2_Init (void)
{
	TIMER_CntInitTypeDef timer = { 0 };
	RST_CLK_FreqTypeDef  freq  = { 0 };

	RST_CLK_PCLKcmd (RST_CLK_PCLK_TIMER2, ENABLE);

	TIMER_DeInit (MDR_TIMER2);
	TIMER_CntStructInit (&timer);
	TIMER_BRGInit (MDR_TIMER2, TIMER_HCLKdiv4);
	RST_CLK_GetClocksFreq (&freq);

	timer.TIMER_CounterMode = TIMER_CntMode_ClkFixedDir;
	timer.TIMER_Period      = freq.CPU_CLK_Frequency/1000000/4 * 1000; // 1000us
	TIMER_CntInit (MDR_TIMER2, &timer);
	TIMER_ITConfig (MDR_TIMER2, TIMER_STATUS_CNT_ARR, ENABLE);

	NVIC_ClearPendingIRQ(TIMER2_IRQn);
	NVIC_EnableIRQ(TIMER2_IRQn);
	TIMER_Cmd(MDR_TIMER2, ENABLE);
}

//===========================================================================//
void TIMER2_IRQHandler(void)
{
	TIMER_ClearITPendingBit (MDR_TIMER2, TIMER_STATUS_CNT_ARR);
	g_msTickCount++;
}

//===========================================================================//
// Ethernet                                                                  //
//===========================================================================//
void Ethernet_Init (void)
{
	ETH_InitTypeDef initStruct;

	// Reset ethernet clock settings
	ETH_ClockDeInit();

	RST_CLK_PCLKcmd(RST_CLK_PCLK_DMA, ENABLE);

	// Enable HSE2 oscillator 25 MHz
	RST_CLK_HSE2config(RST_CLK_HSE2_ON);
	while( RST_CLK_HSE2status()!=SUCCESS ) ;

	// Config PHY clock
	ETH_PHY_ClockConfig(ETH_PHY_CLOCK_SOURCE_HSE2, ETH_PHY_HCLKdiv1);
	ETH_BRGInit(ETH_HCLKdiv1);
	ETH_ClockCMD(ETH_CLK1, ENABLE);
	ETH_DeInit(MDR_ETHERNET1);

	ETH_StructInit( &initStruct );
	initStruct.ETH_PHY_Mode                 = ETH_PHY_MODE_AutoNegotiation;
	initStruct.ETH_Transmitter_RST          = SET;
	initStruct.ETH_Receiver_RST             = SET;
	initStruct.ETH_Buffer_Mode              = ETH_BUFFER_MODE_LINEAR;
	initStruct.ETH_Source_Addr_HASH_Filter  = DISABLE;
	// ERRATA 0019
	//initStruct.ETH_Control_Frames_Reception = ENABLE;
	// Set the MAC address (see: tcpip.h).
	initStruct.ETH_MAC_Address[2] = (MAC_0<<8)|MAC_1;
	initStruct.ETH_MAC_Address[1] = (MAC_2<<8)|MAC_3;
	initStruct.ETH_MAC_Address[0] = (MAC_4<<8)|MAC_5;

	ETH_Init(MDR_ETHERNET1, &initStruct);
	ETH_PHYCmd(MDR_ETHERNET1, ENABLE);

	// TCP/IP library init
	TCPLowLevelInit();

	// IRQ Handler init at RX.
	ETH_MACITConfig(MDR_ETHERNET1, ETH_MAC_IT_RF_OK, ENABLE);
	NVIC_ClearPendingIRQ(ETHERNET_IRQn);
	NVIC_EnableIRQ(ETHERNET_IRQn);
	ETH_Start(MDR_ETHERNET1);
}

//===========================================================================//
/*******************************************************************************
* Function Name  : ETHERNET_IRQHandler
* Description    : This function handles ETHERNET global interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void ETHERNET_IRQHandler(void)
{
	uint16_t Status;
	ETH_StatusPacketReceptionTypeDef packet;

	// Flash LED1
	PORT_SetBits (MDR_PORTE, MDB_ETH_LED1);

	// Get packet status and clear IT-flag
	Status = ETH_GetMACITStatusRegister(MDR_ETHERNET1);

	// Proccess valid ETH-packet
	if( (MDR_ETHERNET1->ETH_R_Head != MDR_ETHERNET1->ETH_R_Tail) && (Status & ETH_MAC_IT_RF_OK) )
	{
		// Copy packet data into buffer
		packet.Status = ETH_ReceivedFrame(MDR_ETHERNET1, InputFrame);

		// Unicast packet
		if( packet.Fields.UCA )
			 ProcessEthIAFrame(InputFrame, packet.Fields.Length);
		// Broadcast packet
		else if( packet.Fields.BCA )
			 ProcessEthBroadcastFrame(InputFrame, packet.Fields.Length);
	}
	NVIC_ClearPendingIRQ(ETHERNET_IRQn);
}
//===========================================================================//
