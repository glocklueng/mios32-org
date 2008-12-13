// $Id$
/*
 * LCD utility functions
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

#include "seq_lcd.h"

/////////////////////////////////////////////////////////////////////////////
// Global variables
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

const u8 charset_vbars[64] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e, 0x1e,
  0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e, 0x1e, 0x1e,
  0x00, 0x00, 0x00, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
  0x00, 0x00, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
  0x00, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
  0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e
};

const u8 charset_hbars[64] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // empty bar
  0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, // "|  "
  0x00, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, // "|| "
  0x00, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x00, // "|||"
  0x00, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, // " o "
  0x00, 0x10, 0x14, 0x15, 0x15, 0x14, 0x10, 0x00, // " > "
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // not used
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // not used
};


/////////////////////////////////////////////////////////////////////////////
// Clear both LCDs
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_Clear(void)
{
  int i;

  // clear both LCDs
  for(i=0; i<2; ++i) {
    MIOS32_LCD_DeviceSet(i);
    MIOS32_LCD_Clear();
  }

  // select first LCD again
  MIOS32_LCD_DeviceSet(0);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// initialise character set (if not already active)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_InitSpecialChars(seq_lcd_charset_t charset)
{
  static seq_lcd_charset_t current_charset = SEQ_LCD_CHARSET_None;

  if( charset != current_charset ) {
    current_charset = charset;

    int dev;
    for(dev=0; dev<2; ++dev) {
      MIOS32_LCD_DeviceSet(dev);
      switch( charset ) {
        case SEQ_LCD_CHARSET_VBars:
	  MIOS32_LCD_SpecialCharsInit((u8 *)charset_vbars);
	  break;
        case SEQ_LCD_CHARSET_HBars:
	  MIOS32_LCD_SpecialCharsInit((u8 *)charset_hbars);
	  break;
        default:
	  return -1; // charset doesn't exist
      }
    }
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints <num> spaces
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintSpaces(u8 num)
{
  do {
    MIOS32_LCD_PrintChar(' ');
  } while( --num );

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints a vertical bar for a 3bit value
// (1 character)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintVBar(u8 value)
{
  return MIOS32_LCD_PrintChar(value);
}


/////////////////////////////////////////////////////////////////////////////
// prints a horizontal bar for a 4bit value
// (5 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintHBar(u8 value)
{
  // special chars which should be print depending on meter value (16 entries, only 14 used)
  const u8 hbar_table[16][5] = {
    { 4, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0 },
    { 2, 0, 0, 0, 0 },
    { 3, 0, 0, 0, 0 },
    { 3, 1, 0, 0, 0 },
    { 3, 2, 0, 0, 0 },
    { 3, 3, 0, 0, 0 },
    { 3, 3, 1, 0, 0 },
    { 3, 3, 2, 0, 0 },
    { 3, 3, 3, 0, 0 },
    { 3, 3, 3, 1, 0 },
    { 3, 3, 3, 2, 0 },
    { 3, 3, 3, 3, 1 },
    { 3, 3, 3, 3, 2 },
    { 3, 3, 3, 3, 3 },
    { 3, 3, 3, 3, 3 }
  };

  int i;
  for(i=0; i<5; ++i)
    MIOS32_LCD_PrintChar(hbar_table[value][i]);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints a note string (3 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintNote(u8 note)
{
  const char note_tab[12][3] = { "c-", "c#", "d-", "d#", "e-", "f-", "f#", "g-", "g#", "a-", "a#", "b-" };

  // print "---" if note number is 0
  if( note == 0 )
    MIOS32_LCD_PrintString("---");
  else {
    u8 octave = 0;

    // determine octave, note contains semitone number thereafter
    while( note >= 12 ) {
      ++octave;
      note -= 12;
    }

    // print semitone (capital letter if octave >= 2)
    MIOS32_LCD_PrintChar(octave >= 2 ? (note_tab[note][0] + 'A'-'a') : note_tab[note][0]);
    MIOS32_LCD_PrintChar(note_tab[note][1]);

    // print octave
    switch( octave ) {
      case 0:  MIOS32_LCD_PrintChar('2'); break; // -2
      case 1:  MIOS32_LCD_PrintChar('1'); break; // -1
      default: MIOS32_LCD_PrintChar('0' + (octave-2)); // 0..7
    }
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints the gatelength (4 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintGatelength(u8 len)
{
  const char len_tab[25][5] = {
    "  0%", // 0
    "  4%", // 1
    "  8%", // 2
    " 13%", // 3
    " 17%", // 4
    " 21%", // 5
    " 25%", // 6
    " 29%", // 7
    " 33%", // 8
    " 38%", // 9
    " 42%", // 10
    " 46%", // 11
    " 50%", // 12
    " 54%", // 13
    " 58%", // 14
    " 52%", // 15
    " 67%", // 16
    " 71%", // 17
    " 75%", // 18
    " 79%", // 19
    " 83%", // 20
    " 88%", // 21
    " 92%", // 22
    " 96%", // 23
    "100%" // 24
  };

  if( len < 25 ) { // gatelength
    MIOS32_LCD_PrintString((char *)len_tab[len]);
  } else if( len < 33 ) { // gilde
    MIOS32_LCD_PrintString("Gld.");
  } else { // multi trigger
    --len; // for easier calculation
    MIOS32_LCD_PrintFormattedString("%1dx%2d", (len>>5)+1, (len&0x1f)+1);
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints selected group/track (4 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintGxTy(u8 group, u8 selected_tracks)
{
  const char selected_tracks_tab[16] = { '-', '1', '2', 'M', '3', 'M', 'M', 'M', '4', 'M', 'M', 'M', 'M', 'M', 'M', 'A' };

  MIOS32_LCD_PrintChar('G');
  MIOS32_LCD_PrintChar('1' + group);
  MIOS32_LCD_PrintChar('T');
  MIOS32_LCD_PrintChar(selected_tracks_tab[selected_tracks & 0xf]);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// TMP: prints selected trigger layer (8 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintTrgLayer(u8 layer)
{
  const char selected_trg_layer[3][7] = { "Gate  ", "Acc.  ", "Roll  " };

  MIOS32_LCD_PrintChar('A' + layer);
  MIOS32_LCD_PrintChar(':');
  MIOS32_LCD_PrintString((char *)selected_trg_layer[layer]);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints MIDI port (4 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintMIDIPort(mios32_midi_port_t port)
{
  switch( port >> 4 ) {
    case 0:  MIOS32_LCD_PrintString("Def."); break;
    case 1:  MIOS32_LCD_PrintFormattedString("USB%x", port%16); break;
    case 2:  MIOS32_LCD_PrintFormattedString("UAR%x", port%16); break;
    case 3:  MIOS32_LCD_PrintFormattedString("IIC%x", port%16); break;
    default: MIOS32_LCD_PrintFormattedString("0x%02X", port); break;
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints step view (6 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintStepView(u8 step_view)
{

  MIOS32_LCD_PrintFormattedString("S%2d-%2d", (step_view*16)+1, (step_view+1)*16);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// prints selected step (6 characters)
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_LCD_PrintSelectedStep(u8 step_sel, u8 step_max)
{
  MIOS32_LCD_PrintFormattedString("%3d/%2d", step_sel+1, step_max+1);

  return 0; // no error
}


