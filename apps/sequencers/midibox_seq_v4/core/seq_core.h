// $Id$
/*
 * Header file for core routines
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _SEQ_CORE_H
#define _SEQ_CORE_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

#define SEQ_CORE_NUM_STEPS  32
#define SEQ_CORE_NUM_TRACKS 16


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 SEQ_CORE_Init(u32 mode);

extern s32 SEQ_CORE_Reset(void);

extern s32 SEQ_CORE_Start(u32 no_echo);
extern s32 SEQ_CORE_Stop(u32 no_echo);
extern s32 SEQ_CORE_Cont(u32 no_echo);
extern s32 SEQ_CORE_Pause(u32 no_echo);

extern s32 SEQ_CORE_Tick(u32 bpm_tick);

extern s32 SEQ_CORE_Handler(void);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

extern u8 played_step;

extern u8 seq_core_state_run;
extern u8 seq_core_state_pause;


#endif /* _SEQ_CORE_H */
