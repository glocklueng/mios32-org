// $Id$
/*
 * MBSID Sound Engine Routines
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
#include <string.h>

#include <sid.h>

#include "sid_se.h"
#include "sid_se_l.h"


/////////////////////////////////////////////////////////////////////////////
// for optional debugging messages via DEBUG_MSG (defined in mios32_config.h)
// should be at least 1 for sending error messages
/////////////////////////////////////////////////////////////////////////////
#define DEBUG_VERBOSE_LEVEL 1


/////////////////////////////////////////////////////////////////////////////
// Global Variables
/////////////////////////////////////////////////////////////////////////////

sid_se_voice_t sid_se_voice[SID_SE_NUM_VOICES];
sid_se_midi_voice_t sid_se_midi_voice[SID_SE_NUM_MIDI_VOICES];
sid_se_lfo_t sid_se_lfo[SID_SE_NUM_LFO];
sid_se_env_t sid_se_env[SID_SE_NUM_ENV];
sid_se_wt_t sid_se_wt[SID_SE_NUM_WT];
sid_se_seq_t sid_se_seq[SID_SE_NUM_SEQ];


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 SID_SE_Init(u32 mode)
{
  int i, sid;
  s32 status = 0;

  for(i=0; i<SID_SE_NUM_VOICES; ++i)
    SID_SE_VoiceInit((sid_se_voice_t *)&sid_se_voice[i]);

  for(i=0; i<SID_SE_NUM_MIDI_VOICES; ++i)
    SID_SE_MIDIVoiceInit((sid_se_midi_voice_t *)&sid_se_midi_voice[i]);

  for(i=0; i<SID_SE_NUM_LFO; ++i)
    SID_SE_LFOInit((sid_se_lfo_t *)&sid_se_lfo[i]);

  for(i=0; i<SID_SE_NUM_ENV; ++i)
    SID_SE_ENVInit((sid_se_env_t *)&sid_se_env[i]);

  for(i=0; i<SID_SE_NUM_WT; ++i)
    SID_SE_WTInit((sid_se_wt_t *)&sid_se_wt[i]);

  for(i=0; i<SID_SE_NUM_SEQ; ++i)
    SID_SE_SEQInit((sid_se_seq_t *)&sid_se_seq[i]);


  // initialize engine subfunctions
  status |= SID_SE_L_Init(mode);

  return status;
}


/////////////////////////////////////////////////////////////////////////////
// Initialises a voice
/////////////////////////////////////////////////////////////////////////////
s32 SID_SE_VoiceInit(sid_se_voice_t *voice)
{
  // clear complete structure
  memset(voice, 0, sizeof(sid_se_voice_t));

  voice->pitchbender = 0x8000;

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Initialises a MIDI Voice
/////////////////////////////////////////////////////////////////////////////
s32 SID_SE_MIDIVoiceInit(sid_se_midi_voice_t *midi_voice)
{
  // clear complete structure
  memset(midi_voice, 0, sizeof(sid_se_midi_voice_t));

  NOTESTACK_Init(&midi_voice->notestack,
		 NOTESTACK_MODE_PUSH_TOP,
		 &midi_voice->notestack_items[0],
		 SID_SE_NOTESTACK_SIZE);

  midi_voice->split_upper = 0x7f;
  midi_voice->pitchbender = 0x8000;

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Initialises a LFO
/////////////////////////////////////////////////////////////////////////////
s32 SID_SE_LFOInit(sid_se_lfo_t *lfo)
{
  // just clear complete structure
  memset(lfo, 0, sizeof(sid_se_voice_t));
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Initialises an Envelope
/////////////////////////////////////////////////////////////////////////////
s32 SID_SE_ENVInit(sid_se_env_t *env)
{
  // just clear complete structure
  memset(env, 0, sizeof(sid_se_env_t));
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Initialises a Wavetable
/////////////////////////////////////////////////////////////////////////////
s32 SID_SE_WTInit(sid_se_wt_t *wt)
{
  // just clear complete structure
  memset(wt, 0, sizeof(sid_se_wt_t));
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Initialises a Sequencer
/////////////////////////////////////////////////////////////////////////////
s32 SID_SE_SEQInit(sid_se_seq_t *seq)
{
  // just clear complete structure
  memset(seq, 0, sizeof(sid_se_seq_t));
  return 0; // no error
}
