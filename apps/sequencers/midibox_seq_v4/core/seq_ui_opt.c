// $Id$
/*
 * Options page
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
#include "tasks.h"

#include "seq_lcd.h"
#include "seq_ui.h"

#include "seq_file.h"
#include "seq_file_c.h"
#include "seq_file_gc.h"

#include "seq_core.h"
#include "seq_cc.h"
#include "seq_midi_port.h"
#include "seq_midi_sysex.h"


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define NUM_OF_ITEMS       8
#define ITEM_STEPS_MEASURE 0
#define ITEM_STEPS_PATTERN 1
#define ITEM_SYNC_CHANGE   2
#define ITEM_RATOPC        3
#define ITEM_SYNC_MUTE     4
#define ITEM_SYNC_UNMUTE   5
#define ITEM_PASTE_CLR_ALL 6
#define ITEM_INIT_CC       7


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u8 store_file_required;


/////////////////////////////////////////////////////////////////////////////
// Local LED handler function
/////////////////////////////////////////////////////////////////////////////
static s32 LED_Handler(u16 *gp_leds)
{
  if( ui_cursor_flash ) // if flashing flag active: no LED flag set
    return 0;

  switch( ui_selected_item ) {
    case ITEM_STEPS_MEASURE:  *gp_leds = 0x0003; break;
    case ITEM_STEPS_PATTERN:  *gp_leds = 0x000c; break;
    case ITEM_SYNC_CHANGE:    *gp_leds = 0x0030; break;
    case ITEM_RATOPC:         *gp_leds = 0x00c0; break;
    case ITEM_SYNC_MUTE:      *gp_leds = 0x0100; break;
    case ITEM_SYNC_UNMUTE:    *gp_leds = 0x0200; break;
    case ITEM_PASTE_CLR_ALL:  *gp_leds = 0x3000; break;
    case ITEM_INIT_CC:        *gp_leds = 0xc000; break;
  }

  return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
// Local encoder callback function
// Should return:
//   1 if value has been changed
//   0 if value hasn't been changed
//  -1 if invalid or unsupported encoder
/////////////////////////////////////////////////////////////////////////////
static s32 Encoder_Handler(seq_ui_encoder_t encoder, s32 incrementer)
{
  switch( encoder ) {
    case SEQ_UI_ENCODER_GP1:
    case SEQ_UI_ENCODER_GP2:
      ui_selected_item = ITEM_STEPS_MEASURE;
      break;

    case SEQ_UI_ENCODER_GP3:
    case SEQ_UI_ENCODER_GP4:
      ui_selected_item = ITEM_STEPS_PATTERN;
      break;

    case SEQ_UI_ENCODER_GP5:
    case SEQ_UI_ENCODER_GP6:
      ui_selected_item = ITEM_SYNC_CHANGE;
      break;

    case SEQ_UI_ENCODER_GP7:
    case SEQ_UI_ENCODER_GP8:
      ui_selected_item = ITEM_RATOPC;
      break;

    case SEQ_UI_ENCODER_GP9:
      ui_selected_item = ITEM_SYNC_MUTE;
      break;

    case SEQ_UI_ENCODER_GP10:
      ui_selected_item = ITEM_SYNC_UNMUTE;
      break;

    case SEQ_UI_ENCODER_GP11:
    case SEQ_UI_ENCODER_GP12:
      return -1; // not mapped

    case SEQ_UI_ENCODER_GP13:
    case SEQ_UI_ENCODER_GP14:
      ui_selected_item = ITEM_PASTE_CLR_ALL;
      break;

    case SEQ_UI_ENCODER_GP15:
    case SEQ_UI_ENCODER_GP16:
      ui_selected_item = ITEM_INIT_CC;
      break;
  }

  // for GP encoders and Datawheel
  switch( ui_selected_item ) {
    case ITEM_STEPS_MEASURE:
      if( encoder == SEQ_UI_ENCODER_GP1 ) {
	// increment in +/- 16 steps
	u8 value = seq_core_steps_per_measure >> 4;
	if( SEQ_UI_Var8_Inc(&value, 0, 15, incrementer) >= 0 ) {
	  seq_core_steps_per_measure = (value << 4) + 15;
	  store_file_required = 1;
	  return 1;
	}
      } else {
	if( SEQ_UI_Var8_Inc(&seq_core_steps_per_measure, 0, 255, incrementer) >= 0 ) {
	  store_file_required = 1;
	  return 1;
	}
      }
      return 0;

    case ITEM_STEPS_PATTERN:
      if( encoder == SEQ_UI_ENCODER_GP3 ) {
	// increment in +/- 16 steps
	u8 value = seq_core_steps_per_pattern >> 4;
	if( SEQ_UI_Var8_Inc(&value, 0, 15, incrementer) >= 0 ) {
	  seq_core_steps_per_pattern = (value << 4) + 15;
	  store_file_required = 1;
	  return 1;
	}
      } else {
	if( SEQ_UI_Var8_Inc(&seq_core_steps_per_pattern, 0, 255, incrementer) >= 0 ) {
	  store_file_required = 1;
	  return 1;
	}
      }
      return 0;

    case ITEM_SYNC_CHANGE:
      if( incrementer )
	seq_core_options.SYNCHED_PATTERN_CHANGE = incrementer > 0 ? 1 : 0;
      else
	seq_core_options.SYNCHED_PATTERN_CHANGE ^= 1;
      store_file_required = 1;
      return 1;

    case ITEM_PASTE_CLR_ALL:
      if( incrementer )
	seq_core_options.PASTE_CLR_ALL = incrementer > 0 ? 1 : 0;
      else
	seq_core_options.PASTE_CLR_ALL ^= 1;
      store_file_required = 1;
      return 1;

    case ITEM_INIT_CC: {
      u8 value = seq_core_options.INIT_CC;
      if( SEQ_UI_Var8_Inc(&value, 0, 127, incrementer) >= 0 ) {
	seq_core_options.INIT_CC = value;
	store_file_required = 1;
	return 1;
      }
      return 0;
    } break;

    case ITEM_RATOPC:
      if( incrementer )
	seq_core_options.RATOPC = incrementer > 0 ? 1 : 0;
      else
	seq_core_options.RATOPC ^= 1;
      store_file_required = 1;
      return 1;

    case ITEM_SYNC_MUTE: {
      if( incrementer )
	seq_core_options.SYNCHED_MUTE = incrementer > 0 ? 1 : 0;
      else
	seq_core_options.SYNCHED_MUTE ^= 1;
      store_file_required = 1;
      return 1;
    } break;

    case ITEM_SYNC_UNMUTE: {
      if( incrementer )
	seq_core_options.SYNCHED_UNMUTE = incrementer > 0 ? 1 : 0;
      else
	seq_core_options.SYNCHED_UNMUTE ^= 1;
      store_file_required = 1;
      return 1;
    } break;
  }

  return -1; // invalid or unsupported encoder
}


/////////////////////////////////////////////////////////////////////////////
// Local button callback function
// Should return:
//   1 if value has been changed
//   0 if value hasn't been changed
//  -1 if invalid or unsupported button
/////////////////////////////////////////////////////////////////////////////
static s32 Button_Handler(seq_ui_button_t button, s32 depressed)
{
  if( depressed ) return 0; // ignore when button depressed

#if 0
  // leads to: comparison is always true due to limited range of data type
  if( button >= SEQ_UI_BUTTON_GP1 && button <= SEQ_UI_BUTTON_GP16 ) {
#else
  if( button <= SEQ_UI_BUTTON_GP16 ) {
#endif
    // re-use encoder handler - only select UI item, don't increment, flags will be toggled
    return Encoder_Handler((int)button, 0);
  }

  // remaining buttons:
  switch( button ) {
    case SEQ_UI_BUTTON_Select:
    case SEQ_UI_BUTTON_Right:
      if( depressed ) return -1;
      if( ++ui_selected_item >= NUM_OF_ITEMS )
	ui_selected_item = 0;
      return 1; // value always changed

    case SEQ_UI_BUTTON_Left:
      if( depressed ) return -1;
      if( ui_selected_item == 0 )
	ui_selected_item = NUM_OF_ITEMS-1;
      else
	--ui_selected_item;
      return 1; // value always changed

    case SEQ_UI_BUTTON_Up:
      if( depressed ) return -1;
      return Encoder_Handler(SEQ_UI_ENCODER_Datawheel, 1);

    case SEQ_UI_BUTTON_Down:
      if( depressed ) return -1;
      return Encoder_Handler(SEQ_UI_ENCODER_Datawheel, -1);
  }

  return -1; // invalid or unsupported button
}


/////////////////////////////////////////////////////////////////////////////
// Local Display Handler function
// IN: <high_prio>: if set, a high-priority LCD update is requested
/////////////////////////////////////////////////////////////////////////////
static s32 LCD_Handler(u8 high_prio)
{
  if( high_prio )
    return 0; // there are no high-priority updates

  // layout:
  // 00000000001111111111222222222233333333330000000000111111111122222222223333333333
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
  // <--------------------------------------><-------------------------------------->
  //  Measure   Pattern  SyncChange   RATOPC  SyncMute            Paste/Clr  Init CC 
  //  16 Steps  16 Steps     off       off   Mute Unmute            Steps       64   

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 0);
  SEQ_LCD_PrintString(" Measure   Pattern  SyncChange   RATOPC  SyncMute            Paste/Clr  Init CC ");
  SEQ_LCD_PrintSpaces(18);

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 1);

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_STEPS_MEASURE && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(10);
  } else {
    SEQ_LCD_PrintFormattedString("%3d Steps ", (int)seq_core_steps_per_measure + 1);
  }

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_STEPS_PATTERN && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(10);
  } else {
    SEQ_LCD_PrintFormattedString("%3d Steps ", (int)seq_core_steps_per_pattern + 1);
  }
  SEQ_LCD_PrintSpaces(4);

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_SYNC_CHANGE && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(3);
  } else {
    SEQ_LCD_PrintString(seq_core_options.SYNCHED_PATTERN_CHANGE ? "on " : "off");
  }
  SEQ_LCD_PrintSpaces(7);

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_RATOPC && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(3);
  } else {
    SEQ_LCD_PrintString(seq_core_options.RATOPC ? " on" : "off");
  }
  SEQ_LCD_PrintSpaces(3);


  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_SYNC_MUTE && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(4);
  } else {
    SEQ_LCD_PrintString(seq_core_options.SYNCHED_MUTE ? "Mute" : " off");
  }
  SEQ_LCD_PrintSpaces(1);

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_SYNC_UNMUTE && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(6);
  } else {
    SEQ_LCD_PrintString(seq_core_options.SYNCHED_UNMUTE ? "Unmute" : "  off ");
  }
  SEQ_LCD_PrintSpaces(12);  

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_PASTE_CLR_ALL && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(5);
  } else {
    SEQ_LCD_PrintString(seq_core_options.PASTE_CLR_ALL ? "Track" : "Steps");
  }
  SEQ_LCD_PrintSpaces(6);

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_INIT_CC && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(3);
  } else {
    SEQ_LCD_PrintFormattedString("%3d", seq_core_options.INIT_CC);
  }
  SEQ_LCD_PrintSpaces(3);


  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Local exit function
/////////////////////////////////////////////////////////////////////////////
static s32 EXIT_Handler(void)
{
  s32 status = 0;

  if( store_file_required ) {
    // write config files
    MUTEX_SDCARD_TAKE;
    if( (status=SEQ_FILE_C_Write(seq_file_session_name)) < 0 )
      SEQ_UI_SDCardErrMsg(2000, status);
    MUTEX_SDCARD_GIVE;

    MUTEX_SDCARD_TAKE;
    if( (status=SEQ_FILE_GC_Write()) < 0 )
      SEQ_UI_SDCardErrMsg(2000, status);
    MUTEX_SDCARD_GIVE;

    store_file_required = 0;
  }

  return status;
}


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_UI_OPT_Init(u32 mode)
{
  // install callback routines
  SEQ_UI_InstallButtonCallback(Button_Handler);
  SEQ_UI_InstallEncoderCallback(Encoder_Handler);
  SEQ_UI_InstallLEDCallback(LED_Handler);
  SEQ_UI_InstallLCDCallback(LCD_Handler);
  SEQ_UI_InstallExitCallback(EXIT_Handler);

  store_file_required = 0;

  return 0; // no error
}
