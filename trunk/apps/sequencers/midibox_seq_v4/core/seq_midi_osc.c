// $Id$
/*
 * MIDI OSC Routines
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
#include <string.h>
#include "tasks.h"

#include "seq_midi_osc.h"


/////////////////////////////////////////////////////////////////////////////
// for optional debugging messages via DEBUG_MSG (defined in mios32_config.h)
/////////////////////////////////////////////////////////////////////////////
#define DEBUG_VERBOSE_LEVEL 0


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global variables
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u16 osc_transfer_mode[SEQ_MIDI_OSC_NUM_PORTS];


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_MIDI_OSC_Init(u32 mode)
{
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Transfer Mode Set/Get functions
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_MIDI_OSC_TransferModeSet(u8 osc_port, u8 mode)
{
  if( osc_port >= SEQ_MIDI_OSC_NUM_PORTS )
    return -1; // invalid connection

  osc_transfer_mode[osc_port] = mode;
  return 0;
}

u8 SEQ_MIDI_OSC_TransferModeGet(u8 osc_port)
{
  return osc_transfer_mode[osc_port];
}

/////////////////////////////////////////////////////////////////////////////
// sends a MIDI packet as OSC datagram
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_MIDI_OSC_SendPackage(u8 osc_port, mios32_midi_package_t package)
{
  // create the OSC packet
  u8 packet[128];
  u8 *end_ptr = packet;

  if( osc_port >= SEQ_MIDI_OSC_NUM_PORTS )
    return -1; // invalid port

  if( osc_transfer_mode[osc_port] != SEQ_MIDI_OSC_TRANSFER_MODE_MIDI &&
      package.type >= NoteOff && package.type <= PitchBend ) {
    char event_path[30];
    switch( package.type ) {
    case NoteOff:
      package.velocity = 0;
      // fall through
    case NoteOn:
      sprintf(event_path, "/%d/note", package.chn+1);
      end_ptr = MIOS32_OSC_PutString(end_ptr, event_path);
      if( osc_transfer_mode[osc_port] == SEQ_MIDI_OSC_TRANSFER_MODE_FLOAT ) {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",if");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.note);
	end_ptr = MIOS32_OSC_PutFloat(end_ptr, (float)package.velocity/127.0);
      } else {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",ii");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.note);
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.velocity);
      }
      break;

    case PolyPressure:
      sprintf(event_path, "/%d/polypressure", package.chn+1);
      end_ptr = MIOS32_OSC_PutString(end_ptr, event_path);
      if( osc_transfer_mode[osc_port] == SEQ_MIDI_OSC_TRANSFER_MODE_FLOAT ) {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",if");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.note);
	end_ptr = MIOS32_OSC_PutFloat(end_ptr, (float)package.velocity/127.0);
      } else {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",ii");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.note);
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.velocity);
      }
      break;

    case CC:
      sprintf(event_path, "/%d/cc/%d", package.chn+1, package.cc_number);
      end_ptr = MIOS32_OSC_PutString(end_ptr, event_path);
      if( osc_transfer_mode[osc_port] == SEQ_MIDI_OSC_TRANSFER_MODE_FLOAT ) {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",f");
	end_ptr = MIOS32_OSC_PutFloat(end_ptr, (float)package.value/127.0);
      } else {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",i");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.value);
      }
      break;

    case ProgramChange:
      sprintf(event_path, "/%d/programchange", package.chn+1);
      end_ptr = MIOS32_OSC_PutString(end_ptr, event_path);
      if( osc_transfer_mode[osc_port] == SEQ_MIDI_OSC_TRANSFER_MODE_FLOAT ) {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",f");
	end_ptr = MIOS32_OSC_PutFloat(end_ptr, (float)package.program_change/127.0);
      } else {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",i");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.program_change);
      }
      break;

    case Aftertouch:
      sprintf(event_path, "/%d/aftertouch", package.chn+1);
      end_ptr = MIOS32_OSC_PutString(end_ptr, event_path);
      if( osc_transfer_mode[osc_port] == SEQ_MIDI_OSC_TRANSFER_MODE_FLOAT ) {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",f");
	end_ptr = MIOS32_OSC_PutFloat(end_ptr, (float)package.note/127.0);
      } else {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",i");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, package.note);
      }
      break;

    case PitchBend: {
      sprintf(event_path, "/%d/pitchbend", package.chn+1);
      end_ptr = MIOS32_OSC_PutString(end_ptr, event_path);
      int value = ((package.evnt1 & 0x7f) | (int)((package.evnt2 & 0x7f) << 7)) - 8192;
      if( value >= 0 && value <= 127 )
	value = 0;
      if( osc_transfer_mode[osc_port] == SEQ_MIDI_OSC_TRANSFER_MODE_FLOAT ) {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",f");
	end_ptr = MIOS32_OSC_PutFloat(end_ptr, (float)value/8191.0);
      } else {
	end_ptr = MIOS32_OSC_PutString(end_ptr, ",i");
	end_ptr = MIOS32_OSC_PutInt(end_ptr, value);
      }
    } break;

    default:
      sprintf(event_path, "/%d/invalid", package.chn);
      end_ptr = MIOS32_OSC_PutString(end_ptr, event_path);
      break;
    }
  } else {
    char midi_path[8];
    strcpy(midi_path, "/midiX");
    midi_path[5] = '1' + osc_port;
    end_ptr = MIOS32_OSC_PutString(end_ptr, midi_path);
    end_ptr = MIOS32_OSC_PutString(end_ptr, ",m");
    end_ptr = MIOS32_OSC_PutMIDI(end_ptr, package);
  }

  // send packet and exit
  return OSC_SERVER_SendPacket(osc_port, packet, (u32)(end_ptr-packet));
}
