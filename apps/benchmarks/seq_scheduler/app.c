// $Id$
/*
 * Benchmark for MIDI Out Scheduler
 * See README.txt for details
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

#include <FreeRTOS.h>
#include <portmacro.h>

#include <seq_midi_out.h>
#include "benchmark.h"
#include "app.h"


/////////////////////////////////////////////////////////////////////////////
// Global Variables
/////////////////////////////////////////////////////////////////////////////

volatile u8 print_msg;


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

// timer used to measure delays (TIM1..TIM8)
#define BENCHMARK_TIMER  TIM4
#define BENCHMARK_TIMER_RCC RCC_APB1Periph_TIM4


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u16 benchmark_cycles;
static u8 benchmark_overrun;


/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  s32 i;

  // initialize all LEDs
  MIOS32_BOARD_LED_Init(0xffffffff);

  // initialize benchmark
  BENCHMARK_Init(0);

  // init benchmark result
  benchmark_cycles = 0;
  benchmark_overrun = 0;

  // print first message
  print_msg = PRINT_MSG_INIT;
}


/////////////////////////////////////////////////////////////////////////////
// This task is running endless in background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{
  // clear LCD screen
  MIOS32_LCD_Clear();

  // endless loop: print status information on LCD
  while( 1 ) {
    // new message requested?
    // TODO: add FreeRTOS specific queue handling!
    u8 new_msg = PRINT_MSG_NONE;
    portENTER_CRITICAL(); // port specific FreeRTOS function to disable tasks (nested)
    if( print_msg ) {
      new_msg = print_msg;
      print_msg = PRINT_MSG_NONE; // clear request
    }
    portEXIT_CRITICAL(); // port specific FreeRTOS function to enable tasks (nested)

    switch( new_msg ) {
      case PRINT_MSG_INIT:
        MIOS32_LCD_CursorSet(0, 0);
        MIOS32_LCD_PrintString("see README.txt   ");
        MIOS32_LCD_CursorSet(0, 1);
        MIOS32_LCD_PrintString("for details     ");
	break;

      case PRINT_MSG_SEQ_STATUS:
      {
        MIOS32_LCD_CursorSet(0, 0);
	MIOS32_LCD_PrintFormattedString("A%3d  M%3d  D%3d",
					seq_midi_out_allocated,
					seq_midi_out_max_allocated,
					seq_midi_out_dropouts);

        MIOS32_LCD_CursorSet(0, 1);
	MIOS32_LCD_PrintFormattedString("Time: %5d.%d mS %c", 
					benchmark_cycles/10, benchmark_cycles%10, 
					benchmark_overrun ? 'O' : ' ');

	// request status screen again (will stop once a new screen is requested by another task)
	print_msg = PRINT_MSG_SEQ_STATUS;
      }
      break;
    }
  }
}


/////////////////////////////////////////////////////////////////////////////
//  This hook is called when a complete MIDI event has been received
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedEvent(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
  if( midi_package.type == NoteOn && midi_package.velocity > 0 ) {
    // enable timer clock
    if( BENCHMARK_TIMER_RCC == RCC_APB2Periph_TIM1 || BENCHMARK_TIMER_RCC == RCC_APB2Periph_TIM8 )
      RCC_APB2PeriphClockCmd(BENCHMARK_TIMER_RCC, ENABLE);
    else
      RCC_APB1PeriphClockCmd(BENCHMARK_TIMER_RCC, ENABLE);

    // time base configuration
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period = 0xffff; // max period
    TIM_TimeBaseStructure.TIM_Prescaler = 7200-1; // 100 uS accuracy @ 72 MHz
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(BENCHMARK_TIMER, &TIM_TimeBaseStructure);

    // enable interrupt request
    TIM_ITConfig(BENCHMARK_TIMER, TIM_IT_Update, ENABLE);

    // start counter
    TIM_Cmd(BENCHMARK_TIMER, ENABLE);

    // reset benchmark
    BENCHMARK_Reset();

    portENTER_CRITICAL(); // port specific FreeRTOS function to disable tasks (nested)

    // turn on LED (e.g. for measurements with a scope)
    MIOS32_BOARD_LED_Set(0xffffffff, 1);

    // reset counter
    BENCHMARK_TIMER->CNT = 1; // set to 1 instead of 0 to avoid new IRQ request
    TIM_ClearITPendingBit(BENCHMARK_TIMER, TIM_IT_Update);

    // start benchmark
    BENCHMARK_Start();

    // capture counter value
    benchmark_cycles = BENCHMARK_TIMER->CNT;

    // set overrun flag if required
    if( (benchmark_overrun = TIM_GetITStatus(BENCHMARK_TIMER, TIM_IT_Update)) != RESET )
      benchmark_cycles = 0xffff;

    // turn off LED
    MIOS32_BOARD_LED_Set(0xffffffff, 0);

    portEXIT_CRITICAL(); // port specific FreeRTOS function to enable tasks (nested)

    // print status screen
    print_msg = PRINT_MSG_SEQ_STATUS;
  }
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a SysEx byte has been received
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedSysEx(mios32_midi_port_t port, u8 sysex_byte)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a byte has been received via COM interface
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedCOM(mios32_com_port_t port, u8 byte)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called before the shift register chain is scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServicePrepare(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called after the shift register chain has been scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServiceFinish(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a button has been toggled
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void APP_DIN_NotifyToggle(u32 pin, u32 pin_value)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a pot has been moved
/////////////////////////////////////////////////////////////////////////////
void APP_AIN_NotifyChange(u32 pin, u32 pin_value)
{
}
