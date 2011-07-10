// $Id$
/*
 * Local SCS Configuration
 *
 * ==========================================================================
 *
 *  Copyright (C) 2011 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <string.h>

#include <scs.h>
#include "scs_config.h"
#include "cc_labels.h"


/////////////////////////////////////////////////////////////////////////////
// Local defines
/////////////////////////////////////////////////////////////////////////////
#define NUM_KNOBS 10

/////////////////////////////////////////////////////////////////////////////
// Local parameter variables
/////////////////////////////////////////////////////////////////////////////

static u8 selectedKnob;
static u8 knobCC[NUM_KNOBS];
static u8 knobChn[NUM_KNOBS];
static u8 knobValue[NUM_KNOBS];


/////////////////////////////////////////////////////////////////////////////
// String Conversion Functions
/////////////////////////////////////////////////////////////////////////////
static void stringDec(u32 ix, u16 value, char *label)    { sprintf(label, "%3d ", value); }
static void stringDecP1(u32 ix, u16 value, char *label)  { sprintf(label, "%3d ", value+1); }
static void stringDec000(u32 ix, u16 value, char *label) { sprintf(label, "%03d ", value); }

static void stringCCFull(u32 ix, u16 value, char *line1, char *line2)
{
  // print CC parameter on full screen (2x20)
  sprintf(line1, "CC Assignment Knob#%d", selectedKnob);
  sprintf(line2, "%s    (CC#%03d)", CC_LABELS_Get(value), value);
}


/////////////////////////////////////////////////////////////////////////////
// Parameter Selection Functions
/////////////////////////////////////////////////////////////////////////////
static u16  selectNOP(u32 ix, u16 value)    { return value; }


/////////////////////////////////////////////////////////////////////////////
// Parameter Access Functions
/////////////////////////////////////////////////////////////////////////////
static u16  knobGet(u32 ix)              { return selectedKnob; }
static void knobSet(u32 ix, u16 value)   { selectedKnob = value; }

static u16  knobValueGet(u32 ix)               { return knobValue[ix]; }
static void knobValueSet(u32 ix, u16 value)
{
  if( knobValue[ix] != value ) {
    knobValue[ix] = value;
    MIOS32_MIDI_SendCC(USB0,  knobChn[ix], knobCC[ix], knobValue[ix]);
    MIOS32_MIDI_SendCC(UART0, knobChn[ix], knobCC[ix], knobValue[ix]);
  }
}

static u16  selKnobCCGet(u32 ix)               { return knobCC[selectedKnob]; }
static void selKnobCCSet(u32 ix, u16 value)    { knobCC[selectedKnob] = value; }

static u16  selKnobChnGet(u32 ix)              { return knobChn[selectedKnob]; }
static void selKnobChnSet(u32 ix, u16 value)   { knobChn[selectedKnob] = value; }

static u16  selKnobValueGet(u32 ix)            { return knobValue[selectedKnob]; }
static void selKnobValueSet(u32 ix, u16 value) { knobValueSet(selectedKnob, value); } // re-use, will send CC


/////////////////////////////////////////////////////////////////////////////
// Menu Structure
/////////////////////////////////////////////////////////////////////////////

const scs_menu_item_t pageCfg[] = {
  SCS_ITEM("Knb ", 0, NUM_KNOBS-1, knobGet,         knobSet,         selectNOP, stringDecP1,  NULL),
  SCS_ITEM("Chn ", 0, 0x0f,        selKnobChnGet,   selKnobChnSet,   selectNOP, stringDecP1,  NULL),
  SCS_ITEM("CC# ", 0, 0x7f,        selKnobCCGet,    selKnobCCSet,    selectNOP, stringDec000, stringCCFull),
  SCS_ITEM("Val ", 0, 0x7f,        selKnobValueGet, selKnobValueSet, selectNOP, stringDec,    NULL),
};

const scs_menu_item_t pageKnb[] = {
  SCS_ITEM("K 1 ",  0, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 2 ",  1, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 3 ",  2, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 4 ",  3, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 5 ",  4, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 6 ",  5, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 7 ",  6, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 8 ",  7, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K 9 ",  8, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
  SCS_ITEM("K10 ",  9, 0x7f,      knobValueGet,    knobValueSet,    selectNOP, stringDec000, NULL),
};

const scs_menu_page_t rootMode0[] = {
  SCS_PAGE("Knb ", pageKnb),
  SCS_PAGE("Cfg ", pageCfg),
};


/////////////////////////////////////////////////////////////////////////////
// This function returns the two lines of the main page (2x20 chars)
/////////////////////////////////////////////////////////////////////////////
static s32 getStringMainPage(char *line1, char *line2)
{
  strcpy(line1, "Main Page");
  strcpy(line2, "Press soft button");

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This function is called when the rotary encoder is moved in the main page
/////////////////////////////////////////////////////////////////////////////
static s32 encMovedInMainPage(s32 incrementer)
{
  MIOS32_MIDI_SendDebugMessage("Encoder moved in main page: %d\n", incrementer);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This function is called when a soft button is pressed in the main page
// if a value < 0 is returned, the menu won't be entered
/////////////////////////////////////////////////////////////////////////////
static s32 buttonPressedInMainPage(u8 softButton)
{
  // here the root table could be changed depending on soft button

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This function returns the upper line on the page selection (20 chars)
/////////////////////////////////////////////////////////////////////////////
static s32 getStringPageSelection(char *line1)
{
  strcpy(line1, "Select Page:");

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Initialisation of SCS Config
// mode selects the used SCS config (currently only one available selected with 0)
// return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 SCS_CONFIG_Init(u32 mode)
{
  if( mode > 0 )
    return -1;

  switch( mode ) {
  case 0: {
    // default knob assignments
    int i;
    for(i=0; i<NUM_KNOBS; ++i)
      knobCC[i] = 16 + i;

    // install table
    SCS_INSTALL_ROOT(rootMode0);
    SCS_InstallMainPageStringHook(getStringMainPage);
    SCS_InstallPageSelectStringHook(getStringPageSelection);
    SCS_InstallEncMainPageHook(encMovedInMainPage);
    SCS_InstallButtonMainPageHook(buttonPressedInMainPage);
    break;
  }
  default: return -1; // mode not supported
  }

  return 0; // no error
}