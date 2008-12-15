// $Id$
/*
 * Header file for tasks which have to be serviced by FreeRTOS/MacOS
 *
 * For MIOS32, the appr. tasks.c file is located in ../mios32/tasks.c
 * For MacOS, the code is implemented in ui.m
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _TASKS_H
#define _TASKS_H


/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

// called from tasks.c
extern s32 TASKS_Init(u32 mode);

extern void SEQ_TASK_Period1mS(void);
extern void SEQ_TASK_MIDI(void);
extern void SEQ_TASK_Pattern(void);

// located in tasks.c
extern void SEQ_TASK_PatternResume(void);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _TASKS_H */
