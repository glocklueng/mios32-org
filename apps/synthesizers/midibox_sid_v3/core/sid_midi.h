// $Id$
/*
 * Header file for MBSID MIDI Parser
 *
 * ==========================================================================
 *
 *  Copyright (C) 2009 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _SID_MIDI_H
#define _SID_MIDI_H

#include "sid_se.h"

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 SID_MIDI_Init(u32 mode);

extern s32 SID_MIDI_Receive(mios32_midi_port_t port, mios32_midi_package_t midi_package);

extern s32 SID_MIDI_PushWT(sid_se_voice_t *v, u8 note);
extern s32 SID_MIDI_PopWT(sid_se_voice_t *v, u8 note);
extern s32 SID_MIDI_ArpNoteOn(sid_se_voice_t *v, u8 note, u8 velocity);
extern s32 SID_MIDI_ArpNoteOff(sid_se_voice_t *v, u8 note);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

#endif /* _SID_MIDI_H */
