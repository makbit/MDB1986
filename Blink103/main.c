//---------------------------------------------------------------------------//
// Project: Blink103 - Tests ST-Link V2 clone hardware.                      //
//          Does led blinking, initializes external oscillator (8MHz x9),    //
//          toggles RESET pin, and issues JTAG IDCODE instruction.           //
// Target:  STM32F103C8T6 or CS32F103 MCU                                    //
// Date:    Nov 2019                                                         //
//---------------------------------------------------------------------------//
#include <stdint.h>
#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
 

//===========================================================================================// 
//   JTAG I/O Pin           | SWD I/O Pin          | CMSIS-DAP Hardware pin mode             //
//------------------------- | -------------------- | ----------------------------------------//
// TCK:    Test Clock       | SWCLK: Clock         | Output Push/Pull                        //
// TMS:    Test Mode Select | SWDIO: Data I/O      | Output Push/Pull, Input                 //
// TDI:    Test Data Input  |                      | Output Push/Pull                        //
// TDO:    Test Data Output |                      | Input                                   //
// nTRST:  Test Reset (opt) |                      | Output Open Drain with pull-up resistor //
// nRESET: Device Reset     | nRESET: Device Reset | Output Open Drain with pull-up resistor //
//===========================================================================================// 
#define PORT_LED             GPIOA
#define PORT_JTDO            GPIOA
#define PORT_JTDI            GPIOA
#define PORT_NRESET          GPIOB
#define PORT_JTRST           GPIOB
#define PORT_JTCK            GPIOB
#define PORT_JTMS            GPIOB

#define PIN_LED              GPIO_Pin_9
#define PIN_JTDO             GPIO_Pin_6
#define PIN_JTDI             GPIO_Pin_7
#define PIN_NRESET           GPIO_Pin_0
#define PIN_JTRST            GPIO_Pin_1
#define PIN_JTCK             GPIO_Pin_13
#define PIN_JTMS             GPIO_Pin_14

//===========================================================================// 
#define IRLEN_CORTEX         0x04
#define IDCODE_CORTEX        0x0E
#define IDCODE_LENGTH        32


//===========================================================================// 
static void PORT_Init (void);
static void SYSCLK_Init (void);

static void     JTAG_SetTCK    (uint8_t on);
static void     JTAG_SetTMS    (uint8_t on);
static void     JTAG_SetTDI    (uint8_t on);
static uint8_t  JTAG_GetTDO    (void);
static void     JTAG_Reset     (void);
static void     JTAG_StrobeTCK (void);
static uint32_t JTAG_IR_Scan   (uint32_t instr, uint8_t nbits);
static uint32_t JTAG_DR_Scan   (uint32_t dat, uint8_t nbits);
static uint32_t JTAG_ReadID    (void);

//===========================================================================// 
static void Delay(unsigned n)
{
	volatile unsigned i;
	for( i = 0; i < n; i++ ) ;
}

//===========================================================================// 
int main(void)
{
	uint32_t id = 0;

	PORT_Init();
	SYSCLK_Init();
 
	while (1)
	{
		Delay(5000);

		id = JTAG_ReadID();
		if( (id & 0x0FFF0FFF)==0x0BA00477 )
		{
			// Cortex-M detected.
			GPIO_SetBits (PORT_LED, PIN_LED);
		}
		else
		{
			GPIO_ResetBits (PORT_LED, PIN_LED);
		}
	}
}


//===========================================================================// 
void PORT_Init (void)
{
	GPIO_InitTypeDef initStructure = { 0 };
 
	// Enable PORT Clock
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);

	// Configure the GPIO pins
	GPIO_StructInit (&initStructure);
	initStructure.GPIO_Pin    = PIN_LED | PIN_JTDI;
	initStructure.GPIO_Mode   = GPIO_Mode_Out_PP;
	initStructure.GPIO_Speed  = GPIO_Speed_10MHz;
	GPIO_Init (GPIOA, &initStructure);

	initStructure.GPIO_Pin    = PIN_JTCK | PIN_JTMS;
	GPIO_Init (GPIOB, &initStructure);

	initStructure.GPIO_Pin    = PIN_JTDO;
	initStructure.GPIO_Mode   = GPIO_Mode_IPU;
	GPIO_Init (GPIOA, &initStructure);
}

//===========================================================================// 
void SYSCLK_Init (void)
{
	RCC_DeInit ();
	RCC_HSEConfig (RCC_HSE_ON);
	if( SUCCESS == RCC_WaitForHSEStartUp() )
	{
		RCC_HCLKConfig  (RCC_SYSCLK_Div1);
		RCC_PCLK2Config (RCC_HCLK_Div1);
		RCC_PCLK1Config (RCC_HCLK_Div1);

		// 8 MHz x 9 == 72 MHz
		RCC_PLLConfig   (RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
		RCC_PLLCmd      (ENABLE);

		while( RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET ) ;

		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		while( RCC_GetSYSCLKSource() != 0x08 ) ;
	}
	
}

//===========================================================================//
void JTAG_SetTCK (uint8_t on)
{
	if( on ) GPIO_SetBits (PORT_JTCK, PIN_JTCK);
	else GPIO_ResetBits (PORT_JTCK, PIN_JTCK);
}

void JTAG_SetTMS (uint8_t on)
{
	if( on ) GPIO_SetBits (PORT_JTMS, PIN_JTMS);
	else GPIO_ResetBits (PORT_JTMS, PIN_JTMS);
}

void JTAG_SetTDI (uint8_t on)
{
	if( on ) GPIO_SetBits (PORT_JTDI, PIN_JTDI);
	else GPIO_ResetBits (PORT_JTDI, PIN_JTDI);
}

uint8_t JTAG_GetTDO (void)
{
	return GPIO_ReadInputDataBit(PORT_JTDO, PIN_JTDO);
}

//===========================================================================//
void JTAG_StrobeTCK (void)
{
	JTAG_SetTCK(1);
	Delay(5);
	JTAG_SetTCK(0);
	Delay(5);
}

//-----------------------------------------------------------------------------
void JTAG_Reset (void)
{
	JTAG_SetTMS(1);
	JTAG_StrobeTCK ();                       // move to Test Logic Reset state
	JTAG_StrobeTCK ();
	JTAG_StrobeTCK ();
	JTAG_StrobeTCK ();
	JTAG_StrobeTCK ();
	Delay(40);
	JTAG_SetTMS(0);
	JTAG_StrobeTCK ();                       // move to Run_Test/Idle state
}

//-----------------------------------------------------------------------------
uint32_t JTAG_IR_Scan (uint32_t instr, uint8_t nbits)
{
	uint8_t  i;                             // JTAG IR bit counter
	uint32_t retval = 0;                    // JTAG instruction read
	uint32_t bits   = 1L << (nbits-1);      // Precalculated operand
   
	JTAG_SetTMS(1);
	Delay(5);
	JTAG_StrobeTCK ();                      // move to SelectDR
	JTAG_StrobeTCK ();                      // move to SelectIR
	Delay(5);
	JTAG_SetTMS(0);
	JTAG_StrobeTCK ();                      // move to Capture_IR
	JTAG_StrobeTCK ();                      // move to Shift_IR state
	Delay(20);

	for( i = 0; i < nbits; i++ )
	{
		JTAG_SetTDI (instr & 0x01);         // shift IR, LSB-first
		instr = instr >> 1;
		retval = retval >> 1;
		if( JTAG_GetTDO() )
		{
			retval |= bits;
		}
		if( i == (nbits - 1) )
		{
			JTAG_SetTMS(1);                 // move to Exit1_IR state
			Delay(5);
		}
		JTAG_StrobeTCK();
	}
	Delay(20);
	JTAG_SetTMS(1);
	JTAG_StrobeTCK ();                      // move to Update_IR
	JTAG_SetTMS(0);
	Delay(5);
	JTAG_StrobeTCK ();                      // move to RTI state
	return retval;
}

//-----------------------------------------------------------------------------
uint32_t JTAG_DR_Scan (uint32_t dat, uint8_t nbits)
{
	uint8_t  i;                             // JTAG DR bit counter
	uint32_t retval = 0;                    // JTAG return value
	uint32_t bits   = 1L << (nbits-1);      // Precalculated operand

	JTAG_SetTMS(1);
	Delay(5);
	JTAG_StrobeTCK ();                      // move to SelectDR
	Delay(5);
	JTAG_SetTMS(0);
	JTAG_StrobeTCK ();                      // move to Capture_DR
	JTAG_StrobeTCK ();                      // move to Shift_DR state

	for( i = 0; i < nbits; i++ )
	{
		JTAG_SetTDI( dat & 0x01 );          // shift DR, LSB-first
		dat = dat >> 1;
		retval = retval >> 1;
		if( JTAG_GetTDO() )
		{
			retval |= bits;
		}
		if( i == (nbits - 1) )
		{
			JTAG_SetTMS(1);                 // move to Exit1_DR state
			Delay(5);
		}
		JTAG_StrobeTCK();
	}
	Delay(20);
	JTAG_SetTMS(1);
	JTAG_StrobeTCK ();                      // move to Update_DR
	JTAG_SetTMS(0);
	Delay(5);
	JTAG_StrobeTCK ();                      // move to RTI state
	return retval;
}

//-----------------------------------------------------------------------------
uint32_t JTAG_ReadID (void)
{
	uint32_t id;
	uint32_t ir;

	JTAG_Reset ();                                   // Reset the JTAG state machine on DUT
	ir = JTAG_IR_Scan (IDCODE_CORTEX, IRLEN_CORTEX); // Load IDCODE into IR and HALT the DUT
	id = JTAG_DR_Scan (0L, IDCODE_LENGTH);           // Read the IDCODE

	if( (id & 0xFF) == 0x93 )
	{
		// Spartan detected
	}
	return id;
}

