// $Id$
/*
 * Preset handling
 *
 * ==========================================================================
 *
 *  Copyright (C) 2009 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

#include <eeprom.h>
#include "presets.h"
#include "midimon.h"
#include "uip_task.h"
#include "osc_server.h"
#include "keyboard.h"


/////////////////////////////////////////////////////////////////////////////
// for optional debugging messages via MIOS32_MIDI_SendDebug*
/////////////////////////////////////////////////////////////////////////////

#define DEBUG_VERBOSE_LEVEL 1

#ifndef DEBUG_MSG
# define DEBUG_MSG MIOS32_MIDI_SendDebugMessage
#endif


/////////////////////////////////////////////////////////////////////////////
// Reads the EEPROM content during boot
// If EEPROM content isn't valid (magic number mismatch), clear EEPROM
// with default data
/////////////////////////////////////////////////////////////////////////////
s32 PRESETS_Init(u32 mode)
{
  s32 status = 0;

  // init EEPROM emulation
  if( (status=EEPROM_Init(0)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[PRESETS] EEPROM initialisation failed with status %d!\n", status);
#endif
    return status;
  }

  // check for magic number in EEPROM - if not available, initialize the structure
  if( PRESETS_Read32(PRESETS_ADDR_MAGIC01) != EEPROM_MAGIC_NUMBER ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[PRESETS] magic number not found - initialize EEPROM!\n");
#endif

    // clear EEPROM
    int addr;
    for(addr=0; addr<PRESETS_EEPROM_SIZE; ++addr)
      status |= PRESETS_Write16(addr, 0x00);

    status |= PRESETS_StoreAll();

    // finally write magic numbers
    if( status >= 0 )
      status |= PRESETS_Write32(PRESETS_ADDR_MAGIC01, EEPROM_MAGIC_NUMBER);
  }

  if( status >= 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[PRESETS] reading configuration data from EEPROM!\n");
#endif

    u8 midimon_setup = PRESETS_Read16(PRESETS_ADDR_MIDIMON);
    status |= MIDIMON_InitFromPresets((midimon_setup>>0) & 1, (midimon_setup>>1) & 1, (midimon_setup>>2) & 1);

    status |= UIP_TASK_InitFromPresets(PRESETS_Read16(PRESETS_ADDR_UIP_USE_DHCP),
				       PRESETS_Read32(PRESETS_ADDR_UIP_IP01),
				       PRESETS_Read32(PRESETS_ADDR_UIP_NETMASK01),
				       PRESETS_Read32(PRESETS_ADDR_UIP_GATEWAY01));

    u8 con = 0;
    for(con=0; con<PRESETS_NUM_OSC_RECORDS; ++con) {
      int offset = con*PRESETS_OFFSET_BETWEEN_OSC_RECORDS;
      status |= OSC_SERVER_InitFromPresets(con,
					   PRESETS_Read32(PRESETS_ADDR_OSC0_REMOTE01 + offset),
					   PRESETS_Read16(PRESETS_ADDR_OSC0_REMOTE_PORT + offset),
					   PRESETS_Read16(PRESETS_ADDR_OSC0_LOCAL_PORT + offset));
    }

    int kb;
    keyboard_config_t *kc = (keyboard_config_t *)&keyboard_config[0];
    for(kb=0; kb<KEYBOARD_NUM; ++kb, ++kc) {
      int offset = kb*PRESETS_OFFSET_BETWEEN_KB_RECORDS;
      kc->midi_ports = PRESETS_Read16(PRESETS_ADDR_KB1_MIDI_PORTS + offset);
      kc->midi_chn = PRESETS_Read16(PRESETS_ADDR_KB1_MIDI_CHN + offset);

      u16 note_offsets = PRESETS_Read16(PRESETS_ADDR_KB1_NOTE_OFFSET + offset);
      kc->note_offset = (note_offsets >> 0) & 0xff;
      kc->din_key_offset = (note_offsets >> 8) & 0xff;

      kc->num_rows = PRESETS_Read16(PRESETS_ADDR_KB1_ROWS + offset);
      kc->dout_sr1 = PRESETS_Read16(PRESETS_ADDR_KB1_DOUT_SR1 + offset);
      kc->dout_sr2 = PRESETS_Read16(PRESETS_ADDR_KB1_DOUT_SR2 + offset);
      kc->din_sr1 = PRESETS_Read16(PRESETS_ADDR_KB1_DIN_SR1 + offset);
      kc->din_sr2 = PRESETS_Read16(PRESETS_ADDR_KB1_DIN_SR2 + offset);

      u16 misc = PRESETS_Read16(PRESETS_ADDR_KB1_MISC + offset);
      kc->din_inverted   = (misc & (1 << 0)) ? 1 : 0;
      kc->break_inverted = (misc & (1 << 1)) ? 1 : 0;
      kc->scan_velocity  = (misc & (1 << 2)) ? 1 : 0;
      kc->scan_optimized = (misc & (1 << 3)) ? 1 : 0;

      kc->delay_fastest = PRESETS_Read16(PRESETS_ADDR_KB1_DELAY_FASTEST + offset);
      kc->delay_slowest = PRESETS_Read16(PRESETS_ADDR_KB1_DELAY_SLOWEST + offset);

      u16 ain_assign = PRESETS_Read16(PRESETS_ADDR_KB1_AIN_ASSIGN + offset);
      kc->ain_pitchwheel = (ain_assign >> 0) & 0xff;
      kc->ain_modwheel   = (ain_assign >> 8) & 0xff;

      u16 ctrl_assign = PRESETS_Read16(PRESETS_ADDR_KB1_CTRL_ASSIGN + offset);
      kc->ctrl_pitchwheel = (ctrl_assign >> 0) & 0xff;
      kc->ctrl_modwheel   = (ctrl_assign >> 8) & 0xff;
    }
    KEYBOARD_Init(1); // without overwriting default configuration

  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Help functions to read a value from EEPROM (big endian coding!)
/////////////////////////////////////////////////////////////////////////////
u16 PRESETS_Read16(u8 addr)
{
  return EEPROM_Read(addr);
}

u32 PRESETS_Read32(u8 addr)
{
  return ((u32)EEPROM_Read(addr) << 16) | EEPROM_Read(addr+1);
}

/////////////////////////////////////////////////////////////////////////////
// Help function to write a value into EEPROM (big endian coding!)
/////////////////////////////////////////////////////////////////////////////
s32 PRESETS_Write16(u8 addr, u16 value)
{
  return EEPROM_Write(addr, value);
}

s32 PRESETS_Write32(u8 addr, u32 value)
{
  return EEPROM_Write(addr, (value >> 16) & 0xffff) | EEPROM_Write(addr+1, (value >> 0) & 0xffff);
}


/////////////////////////////////////////////////////////////////////////////
// Stores all presets
// (the EEPROM emulation ensures that a value won't be written if it is already
// equal)
/////////////////////////////////////////////////////////////////////////////
s32 PRESETS_StoreAll(void)
{
  s32 status = 0;

  // write MIDImon data
  status |= PRESETS_Write16(PRESETS_ADDR_MIDIMON,
			    (MIDIMON_ActiveGet() ? 1 : 0) |
			    (MIDIMON_FilterActiveGet() ? 2 : 0) |
			    (MIDIMON_TempoActiveGet() ? 4 : 0));

  // write uIP data
  status |= PRESETS_Write16(PRESETS_ADDR_UIP_USE_DHCP, UIP_TASK_DHCP_EnableGet());
  status |= PRESETS_Write32(PRESETS_ADDR_UIP_IP01, UIP_TASK_IP_AddressGet());
  status |= PRESETS_Write32(PRESETS_ADDR_UIP_NETMASK01, UIP_TASK_NetmaskGet());
  status |= PRESETS_Write32(PRESETS_ADDR_UIP_GATEWAY01, UIP_TASK_GatewayGet());

  // write OSC data
  u8 con = 0;
  for(con=0; con<PRESETS_NUM_OSC_RECORDS; ++con) {
    int offset = con*PRESETS_OFFSET_BETWEEN_OSC_RECORDS;
    status |= PRESETS_Write32(PRESETS_ADDR_OSC0_REMOTE01 + offset, OSC_SERVER_RemoteIP_Get(con));
    status |= PRESETS_Write16(PRESETS_ADDR_OSC0_REMOTE_PORT + offset, OSC_SERVER_RemotePortGet(con));
    status |= PRESETS_Write16(PRESETS_ADDR_OSC0_LOCAL_PORT + offset, OSC_SERVER_LocalPortGet(con));
  }

    int kb;
    keyboard_config_t *kc = (keyboard_config_t *)&keyboard_config[0];
    for(kb=0; kb<KEYBOARD_NUM; ++kb, ++kc) {
      int offset = kb*PRESETS_OFFSET_BETWEEN_KB_RECORDS;
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_MIDI_PORTS + offset, kc->midi_ports);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_MIDI_CHN + offset, kc->midi_chn);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_NOTE_OFFSET + offset, kc->note_offset | ((u16)kc->din_key_offset << 8));
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_ROWS + offset, kc->num_rows);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_DOUT_SR1 + offset, kc->dout_sr1);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_DOUT_SR2 + offset, kc->dout_sr2);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_DIN_SR1 + offset, kc->din_sr1);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_DIN_SR2 + offset, kc->din_sr2);

      u16 misc =
	(kc->din_inverted   << 0) |
	(kc->break_inverted << 1) |
	(kc->scan_velocity  << 2) |
	(kc->scan_optimized << 3);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_MISC + offset, misc);

      status |= PRESETS_Write16(PRESETS_ADDR_KB1_DELAY_FASTEST + offset, kc->delay_fastest);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_DELAY_SLOWEST + offset, kc->delay_slowest);

      u16 ain_assign =
	(kc->ain_pitchwheel << 0) |
	(kc->ain_modwheel   << 8);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_AIN_ASSIGN + offset, ain_assign);

      u16 ctrl_assign =
	(kc->ctrl_pitchwheel << 0) |
	(kc->ctrl_modwheel   << 8);
      status |= PRESETS_Write16(PRESETS_ADDR_KB1_CTRL_ASSIGN + offset, ctrl_assign);

    }

  return 0; // no error
}

