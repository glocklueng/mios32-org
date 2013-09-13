// $Id: seq_ui_opt.c 1261 2011-07-17 18:21:45Z tk $
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
#include "seq_hwcfg.h"

/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#if 0
#define NUM_OF_ITEMS       9
#define ITEM_STEPS_MEASURE 0
#define ITEM_STEPS_PATTERN 1
#define ITEM_SYNC_CHANGE   2
#define ITEM_RATOPC        3
#define ITEM_REMOTE_MODE   4
#define ITEM_REMOTE_ID     5
#define ITEM_REMOTE_PORT   6
#define ITEM_REMOTE_REQUEST 7
#define ITEM_PASTE_CLR_ALL 8
#else
#define NUM_OF_ITEMS       7
#define ITEM_STEPS_MEASURE 0
#define ITEM_STEPS_PATTERN 1
#define ITEM_SYNC_CHANGE   2
#define ITEM_RATOPC        3
#define ITEM_SYNC_MUTE     4
#define ITEM_SYNC_UNMUTE   5
#define ITEM_PASTE_CLR_ALL 6
#endif

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
#if 0
    case ITEM_REMOTE_MODE:    *gp_leds = 0x0100; break;
    case ITEM_REMOTE_ID:      *gp_leds = 0x0200; break;
    case ITEM_REMOTE_PORT:    *gp_leds = 0x0400; break;
    case ITEM_REMOTE_REQUEST: *gp_leds = 0x1800; break;
#else
    case ITEM_SYNC_MUTE:      *gp_leds = 0x0100; break;
    case ITEM_SYNC_UNMUTE:    *gp_leds = 0x0200; break;
#endif
    case ITEM_PASTE_CLR_ALL:  *gp_leds = 0xc000; break;
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
	if( SEQ_UI_Var8_Inc(&ui_selected_item, 0, NUM_OF_ITEMS, incrementer) >= 1 )
	return 1;
      else
	return 0;
		//ui_selected_item = ITEM_MODE;
      break;
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

#if 0
    case SEQ_UI_ENCODER_GP9:
      ui_selected_item = ITEM_REMOTE_MODE;
      break;      
    case SEQ_UI_ENCODER_GP10:
      ui_selected_item = ITEM_REMOTE_ID;
      break;
    case SEQ_UI_ENCODER_GP11:
      ui_selected_item = ITEM_REMOTE_PORT;
      break;      
    case SEQ_UI_ENCODER_GP12:
    case SEQ_UI_ENCODER_GP13:
      ui_selected_item = ITEM_REMOTE_REQUEST;
      break;
#else
    case SEQ_UI_ENCODER_GP9:
      ui_selected_item = ITEM_SYNC_MUTE;
      break;

    case SEQ_UI_ENCODER_GP10:
      ui_selected_item = ITEM_SYNC_UNMUTE;
      break;

    case SEQ_UI_ENCODER_GP11:
    case SEQ_UI_ENCODER_GP12:
    case SEQ_UI_ENCODER_GP13:
    case SEQ_UI_ENCODER_GP14:
      return -1; // not mapped

#endif
      
    case SEQ_UI_ENCODER_GP15:
    case SEQ_UI_ENCODER_GP16:
      ui_selected_item = ITEM_PASTE_CLR_ALL;
      break;
  }

  // for GP encoders and Datawheel
  switch( ui_selected_item ) {
    case ITEM_STEPS_MEASURE:
      if( encoder == SEQ_UI_ENCODER_GP1 ) {
	// increment in +/- 16 steps
	//DEBUG_MSG("encoder 1 gewijzigd maar ook steps measure aangepast, shit"); this is ok, never happens
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
      if( encoder == SEQ_UI_ENCODER_GP3 || ( encoder == SEQ_UI_ENCODER_Datawheel && (seq_hwcfg_mb909.enabled)    )) {
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

    case ITEM_RATOPC:
      if( incrementer )
	seq_core_options.RATOPC = incrementer > 0 ? 1 : 0;
      else
	seq_core_options.RATOPC ^= 1;
      store_file_required = 1;
      return 1;

#if 0
    case ITEM_REMOTE_MODE: {
      u8 value = (u8)seq_ui_remote_mode;
      if( SEQ_UI_Var8_Inc(&value, 0, 2, incrementer) >= 0 ) {
	seq_ui_remote_mode = value;

	switch( seq_ui_remote_mode ) {
	  case SEQ_UI_REMOTE_MODE_AUTO:
  	  case SEQ_UI_REMOTE_MODE_SERVER:
	    if( seq_ui_remote_active_mode == SEQ_UI_REMOTE_MODE_CLIENT )
	      SEQ_MIDI_SYSEX_REMOTE_SendMode(SEQ_UI_REMOTE_MODE_AUTO);
	    seq_ui_remote_active_mode = SEQ_UI_REMOTE_MODE_AUTO;
	    break;
	  case SEQ_UI_REMOTE_MODE_CLIENT:
	    SEQ_MIDI_SYSEX_REMOTE_SendMode(SEQ_UI_REMOTE_MODE_SERVER); // request server
	    break;
	}

	store_file_required = 1;
	return 1;
      }
      return 0;
    } break;

    case ITEM_REMOTE_ID:
      if( SEQ_UI_Var8_Inc(&seq_ui_remote_id, 0, 127, incrementer) >= 0 ) {
	store_file_required = 1;
	return 1;
      }
      return 0;

    case ITEM_REMOTE_PORT: {
      u8 port_ix = SEQ_MIDI_PORT_OutIxGet(seq_ui_remote_port);
      if( SEQ_UI_Var8_Inc(&port_ix, 0, SEQ_MIDI_PORT_OutNumGet()-1, incrementer) >= 0 ) {
	seq_ui_remote_port = SEQ_MIDI_PORT_OutPortGet(port_ix);
	store_file_required = 1;
	return 1; // value changed
      }
      return 0; // no change
    } break;

    case ITEM_REMOTE_REQUEST: {
      switch( seq_ui_remote_mode ) {
        case SEQ_UI_REMOTE_MODE_SERVER:
	  SEQ_UI_Msg(SEQ_UI_MSG_USER, 1000, "Not in", "Server Mode!");
	  break;
        case SEQ_UI_REMOTE_MODE_AUTO:
        case SEQ_UI_REMOTE_MODE_CLIENT:
	  SEQ_MIDI_SYSEX_REMOTE_SendMode(SEQ_UI_REMOTE_MODE_SERVER);
	  SEQ_UI_Msg(SEQ_UI_MSG_USER, 1000, "Remote Server", "requested.");
	  break;
      }
      return 1;
    } break;
#else
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
#endif
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
#if 0
  // 00000000001111111111222222222233333333330000000000111111111122222222223333333333
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
  // <--------------------------------------><-------------------------------------->
  //  Measure   Pattern  SyncChange   RATOPC Remote ID Port Request         Paste/Clr
  //  16 Steps  16 Steps     off       off    Auto  00 Def. Connect:yes       Steps  
#else
  // 00000000001111111111222222222233333333330000000000111111111122222222223333333333
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
  // <--------------------------------------><-------------------------------------->
  //  Measure   Pattern  SyncChange   RATOPC  SyncMute                      Paste/Clr
  //  16 Steps  16 Steps     off       off   Mute Unmute                      Steps  
#endif

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 0);
  SEQ_LCD_PrintString(" Measure   Pattern  ");
  SEQ_LCD_CursorSet(0, 2);
  SEQ_LCD_PrintString("SyncChange   RATOPC ");
  SEQ_LCD_CursorSet(0, 4);
  SEQ_LCD_PrintString("SyncMute            ");
  SEQ_LCD_CursorSet(0, 6);
  SEQ_LCD_PrintString("           Paste/Clr");
  //SEQ_LCD_PrintSpaces(18);

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
  
  SEQ_LCD_CursorSet(0, 3);
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

  SEQ_LCD_CursorSet(0, 5);
#if 0
  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_REMOTE_MODE && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(6);
  } else {
    const char mode_str[3][7] = {
      " Auto ",
      "Server",
      "Client",
    };
    SEQ_LCD_PrintString((char *)mode_str[seq_ui_remote_mode]);
  }
  SEQ_LCD_PrintSpaces(1);

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_REMOTE_ID && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(2);
  } else {
    SEQ_LCD_PrintFormattedString("%02x", seq_ui_remote_id);
  }
  SEQ_LCD_PrintSpaces(1);

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_REMOTE_PORT && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(4);
  } else {
    SEQ_LCD_PrintString(SEQ_MIDI_PORT_OutNameGet(SEQ_MIDI_PORT_OutIxGet(seq_ui_remote_port)));
  }

  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_REMOTE_REQUEST && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(12);
  } else {
    SEQ_LCD_PrintFormattedString(" Connect:%s", (seq_ui_remote_active_mode != SEQ_UI_REMOTE_MODE_AUTO) ? "yes" : "no ");
  }
  SEQ_LCD_PrintSpaces(7);
#else
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
  SEQ_LCD_PrintSpaces(22);  
#endif
  SEQ_LCD_CursorSet(0, 7);
  ///////////////////////////////////////////////////////////////////////////
  if( ui_selected_item == ITEM_PASTE_CLR_ALL && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(5);
  } else {
    SEQ_LCD_PrintString(seq_core_options.PASTE_CLR_ALL ? "Track" : "Steps");
  }
  SEQ_LCD_PrintSpaces(2);


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
