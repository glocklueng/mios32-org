// $Id$
/*
 * Header file for MIDIbox CV MIDI functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MBCV_MIDI_H
#define _MBCV_MIDI_H

#include "mbcv_patch.h"

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MBCV_MIDI_Init(u32 mode);

extern s32 MBCV_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t package);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

extern u8 mbcv_midi_dout_gate_sr[MIOS32_SRIO_NUM_SR];

extern u32 mbcv_midi_gates; // prepared for up to 32 channels
extern u8  mbcv_midi_gateclr_ctr[MBCV_PATCH_NUM_CV];

extern u8  mbcv_midi_note[MBCV_PATCH_NUM_CV];
extern u8  mbcv_midi_velocity[MBCV_PATCH_NUM_CV];
extern u8  mbcv_midi_cc[MBCV_PATCH_NUM_CV];
extern u16 mbcv_midi_nrpn[MBCV_PATCH_NUM_CV];
extern u8  mbcv_midi_aftertouch[MBCV_PATCH_NUM_CV];
extern s16 mbcv_midi_pitch[MBCV_PATCH_NUM_CV];

#endif /* _MBCV_MIDI_H */