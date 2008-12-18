// $Id$
/*
 * MIDI file player
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

#include <seq_bpm.h>
#include <seq_midi_out.h>
#include <mid_parser.h>

#include "seq_midply.h"
#include "seq_ui.h"


/////////////////////////////////////////////////////////////////////////////
// for optional debugging via COM interface
/////////////////////////////////////////////////////////////////////////////
#define DEBUG_VERBOSE_LEVEL 0


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

// how much time has to be bridged between prefetch cycles (time in mS)
#define PREFETCH_TIME_MS 50 // mS


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////

static s32 SEQ_MIDPLY_PlayOffEvents(void);
static s32 SEQ_MIDPLY_SongPos(u16 new_song_pos);
static s32 SEQ_MIDPLY_Tick(u32 bpm_tick);

static s32 SEQ_MIDPLY_getc(void);
static s32 SEQ_MIDPLY_eof(void);
static s32 SEQ_MIDPLY_seek(u32 pos);

static s32 SEQ_MIDPLY_PlayEvent(u8 track, mios32_midi_package_t midi_package, u32 tick);
static s32 SEQ_MIDPLY_PlayMeta(u8 track, u8 meta, u32 len, u8 *buffer, u32 tick);


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u32 midifile_pos;
static u32 midifile_len;

static u32 next_prefetch;

static u8 ffwd_silent_mode;


#include "mb_midifile_demo.inc"


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_MIDPLY_Init(u32 mode)
{
  // init MIDI parser module
  MID_PARSER_Init(0);

  // install callback functions
  MID_PARSER_InstallFileCallbacks(&SEQ_MIDPLY_getc, &SEQ_MIDPLY_eof, &SEQ_MIDPLY_seek);
  MID_PARSER_InstallEventCallbacks(&SEQ_MIDPLY_PlayEvent, &SEQ_MIDPLY_PlayMeta);

  // reset sequencer
  SEQ_MIDPLY_Reset();

  // init BPM generator
  SEQ_BPM_Init(0);

  // initial values
  next_prefetch = 0;
  ffwd_silent_mode = 0;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// this sequencer handler is called periodically to check for new requests
// from BPM generator
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_MIDPLY_Handler(void)
{
  // handle requests

  u8 num_loops = 0;
  u8 again = 0;
  do {
    ++num_loops;

    // note: don't remove any request check - clocks won't be propagated
    // so long any Stop/Cont/Start/SongPos event hasn't been flagged to the sequencer
    if( SEQ_BPM_ChkReqStop() ) {
      SEQ_MIDPLY_PlayOffEvents();
    }

    if( SEQ_BPM_ChkReqCont() ) {
      // release pause mode
      ui_seq_pause = 0;
    }

    if( SEQ_BPM_ChkReqStart() ) {
      SEQ_MIDPLY_Reset();

      // initial prefetch (100 BPM ticks)
      MID_PARSER_FetchEvents(0, 100);
      next_prefetch = 0;
    }

    u16 new_song_pos;
    if( SEQ_BPM_ChkReqSongPos(&new_song_pos) ) {
      SEQ_MIDPLY_SongPos(new_song_pos);
    }

    u32 bpm_tick;
    if( SEQ_BPM_ChkReqClk(&bpm_tick) > 0 ) {
      again = 1; // check all requests again after execution of this part

      SEQ_MIDPLY_Tick(bpm_tick);
    }
  } while( again && num_loops < 10 );

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This function plays all "off" events
// Should be called on sequencer reset/restart/pause to avoid hanging notes
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_PlayOffEvents(void)
{
  // play "off events"
  SEQ_MIDI_OUT_FlushQueue();

  // send Note Off to all channels
  // TODO: howto handle different ports?
  // TODO: should we also send Note Off events? Or should we trace Note On events and send Off if required?
  int chn;
  mios32_midi_package_t midi_package;
  midi_package.type = CC;
  midi_package.event = CC;
  midi_package.evnt2 = 0;
  for(chn=0; chn<16; ++chn) {
    midi_package.chn = chn;
    midi_package.evnt1 = 123; // All Notes Off
    MIOS32_MIDI_SendPackage(DEFAULT, midi_package);
    midi_package.evnt1 = 121; // Controller Reset
    MIOS32_MIDI_SendPackage(DEFAULT, midi_package);
  }

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Resets song position of sequencer
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_MIDPLY_Reset(void)
{
  ui_seq_pause = 0;

  next_prefetch = 0;

  // read midifile
  midifile_pos = 0;
  midifile_len = MID_FILE_LEN;
  MID_PARSER_Read();

  // set initial BPM (according to MIDI file spec)
  SEQ_BPM_PPQN_Set(384); // not specified
  SEQ_BPM_Set(120.0);

  // since timebase has been changed, ensure that Off-Events are played 
  // (otherwise they will be played much later...)
  SEQ_MIDPLY_PlayOffEvents();

  // reset BPM tick
  SEQ_BPM_TickSet(0);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Sets new song position (new_song_pos resolution: 16th notes)
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_SongPos(u16 new_song_pos)
{
  u16 new_tick = new_song_pos * (SEQ_BPM_PPQN_Get() / 4);

#if DEBUG_VERBOSE_LEVEL >= 1
  printf("[SEQ_MIDPLY] Setting new song position %u (-> %u ticks)\n\r", new_song_pos, new_tick);
#endif

  // play off events
  SEQ_MIDPLY_PlayOffEvents();

  // restart song
  MID_PARSER_RestartSong();

  if( new_song_pos > 1 ) {
    // (silently) fast forward to requested position
    ffwd_silent_mode = 1;
    MID_PARSER_FetchEvents(0, new_tick-1);
    ffwd_silent_mode = 0;

    // prefetch some events
    MID_PARSER_FetchEvents(new_tick, 100);
  }

  // when do we expect the next prefetch:
  next_prefetch = new_tick;

  // set new tick value
  SEQ_BPM_TickSet(new_tick);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// performs a single ppqn tick
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_Tick(u32 bpm_tick)
{
  if( bpm_tick >= next_prefetch ) {
#if DEBUG_VERBOSE_LEVEL >= 2
    printf("[SEQ_MIDPLY] Prefetch started at tick %u (prefetching %u..%u)\n\r", SEQ_BPM_TickGet(), tick_offset, tick_offset+num_ticks-1);
#endif

    // get number of prefetch ticks depending on current BPM
    u32 prefetch_ticks = SEQ_BPM_TicksFor_mS(PREFETCH_TIME_MS);
    next_prefetch += prefetch_ticks;

    if( MID_PARSER_FetchEvents(next_prefetch, prefetch_ticks) == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY] End of song reached after %u ticks - restart!\n\r", bpm_ticks);
#endif
      SEQ_MIDPLY_SongPos(0);
    }


#if DEBUG_VERBOSE_LEVEL >= 2
    printf("[SEQ_MIDPLY] Prefetch finished at tick %u\n\r", SEQ_BPM_TickGet());
#endif
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// reads a byte from the .mid file
// returns -1 if end of file reached
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_getc(void)
{
  if( midifile_pos >= midifile_len )
    return -1; // error: end of file reached, buffer not complete!

  return mid_file[midifile_pos++];
}

/////////////////////////////////////////////////////////////////////////////
// returns 1 if end of file reached
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_eof(void)
{
  if( midifile_pos >= midifile_len )
    return 1; // end of file reached

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// sets file pointer to a specific position
// returns -1 if end of file reached
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_seek(u32 pos)
{
  midifile_pos = pos;
  if( midifile_pos >= midifile_len )
    return -1; // end of file reached

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// called when a MIDI event should be played at a given tick
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_PlayEvent(u8 track, mios32_midi_package_t midi_package, u32 tick)
{
  // ignore all events in silent mode (for SEQ_MIDPLY_SongPos function)
  // we could implement a more intelligent parser, which stores the sent CC/program change, etc...
  // and sends the last received values before restarting the song...
  if( ffwd_silent_mode )
    return 0;

  seq_midi_out_event_type_t event_type = SEQ_MIDI_OUT_OnEvent;
  if( midi_package.event == NoteOff || (midi_package.event == NoteOn && midi_package.velocity == 0) )
    event_type = SEQ_MIDI_OUT_OffEvent;

  return SEQ_MIDI_OUT_Send(DEFAULT, midi_package, event_type, tick);
}


/////////////////////////////////////////////////////////////////////////////
// called when a Meta event should be played/processed at a given tick
/////////////////////////////////////////////////////////////////////////////
static s32 SEQ_MIDPLY_PlayMeta(u8 track, u8 meta, u32 len, u8 *buffer, u32 tick)
{
  switch( meta ) {
    case 0x00: // Sequence Number
      if( len == 2 ) {
	u32 seq_number = (*buffer++ << 8) | *buffer;
#if DEBUG_VERBOSE_LEVEL >= 1
	printf("[SEQ_MIDPLY:%d:%u] Meta - Sequence Number %u\n\r", track, tick, seq_number);
#endif
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	printf("[SEQ_MIDPLY:%d:%u] Meta - Sequence Number with %d bytes -- ERROR: expecting 2 bytes!\n\r", track, tick, len);
#endif
      }
      break;

    case 0x01: // Text Event
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - Text: %s\n\r", track, tick, buffer);
#endif
      break;

    case 0x02: // Copyright Notice
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - Copyright: %s\n\r", track, tick, buffer);
#endif
      break;

    case 0x03: // Sequence/Track Name
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - Track Name: %s\n\r", track, tick, buffer);
#endif
      break;

    case 0x04: // Instrument Name
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - Instr. Name: %s\n\r", track, tick, buffer);
#endif
      break;

    case 0x05: // Lyric
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - Lyric: %s\n\r", track, tick, buffer);
#endif
      break;

    case 0x06: // Marker
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - Marker: %s\n\r", track, tick, buffer);
#endif
      break;

    case 0x07: // Cue Point
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - Cue Point: %s\n\r", track, tick, buffer);
#endif
      break;

    case 0x20: // Channel Prefix
      if( len == 1 ) {
	u32 prefix = *buffer;
#if DEBUG_VERBOSE_LEVEL >= 1
	printf("[SEQ_MIDPLY:%d:%u] Meta - Channel Prefix %u\n\r", track, tick, prefix);
#endif
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	printf("[SEQ_MIDPLY:%d:%u] Meta - Channel Prefix with %d bytes -- ERROR: expecting 1 byte!\n\r", track, tick, len);
#endif
      }
      break;

    case 0x2f: // End of Track
#if DEBUG_VERBOSE_LEVEL >= 1
      printf("[SEQ_MIDPLY:%d:%u] Meta - End of Track\n\r", track, tick, meta);
#endif
      break;

    case 0x51: // Set Tempo
      if( len == 3 ) {
	u32 tempo_us = (*buffer++ << 16) | (*buffer++ << 8) | *buffer;
	float bpm = 60.0 * (1E6 / (float)tempo_us);
	SEQ_BPM_PPQN_Set(MIDI_PARSER_PPQN_Get());

	// set tempo immediately on first tick
	if( tick == 0 ) {
	  SEQ_BPM_Set(bpm);
	} else {
	  // put tempo change request into the queue
	  mios32_midi_package_t tempo_package; // or Softis?
	  tempo_package.ALL = (u32)bpm;
	  SEQ_MIDI_OUT_Send(DEFAULT, tempo_package, SEQ_MIDI_OUT_TempoEvent, tick);
	}

#if DEBUG_VERBOSE_LEVEL >= 1
	printf("[SEQ_MIDPLY:%d:%u] Meta - Tempo to %u uS -> %d.%d BPM\n\r", track, tick, tempo_us, bpm/10, bpm%10);
#endif
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	printf("[SEQ_MIDPLY:%d:%u] Meta - Tempo with %u bytes -- ERROR: expecting 3 bytes!\n\r", track, tick, len);
#endif
      }
      break;

    // other known events which are not handled here:
    // 0x54: SMPTE offset
    // 0x58: Time Signature
    // 0x59: Key Signature
    // 0x7f: Sequencer Specific Meta Event

#if DEBUG_VERBOSE_LEVEL >= 1
    default:
      printf("[SEQ_MIDPLY:%d:%u] Meta Event 0x%02x with length %u not processed\n\r", track, tick, meta, len);
#endif
  }

  return 0;
}

