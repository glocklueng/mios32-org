// $Id$
/*
 * Header file for MIOS32 System Initialisation
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MIOS32_SYS_H
#define _MIOS32_SYS_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// The DBGMCU_CR register allows to suspend peripherals when CPU is in halt
// state to simplify debugging (e.g. no timer interrupt is triggered each
// time the program is stepped)
// See STM32 reference manual for the meaning of these flags.
// By default, we suspend all peripherals which are provided by DBGMCU_CR
#ifndef MIOS32_SYS_STM32_DBGMCU_CR
#define MIOS32_SYS_STM32_DBGMCU_CR 0x001fff00
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

// *not* compatible to NTP timestamp format, the fraction has mS accuracy
typedef struct {
  u32 seconds;
  u32 fraction_ms;
} mios32_sys_time_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_SYS_Init(u32 mode);

extern s32 MIOS32_SYS_Reset(void);

extern u32 MIOS32_SYS_ChipIDGet(void);
extern u32 MIOS32_SYS_FlashSizeGet(void);
extern u32 MIOS32_SYS_RAMSizeGet(void);
extern s32 MIOS32_SYS_SerialNumberGet(char *str);

extern s32 MIOS32_SYS_TimeSet(mios32_sys_time_t t);
extern mios32_sys_time_t MIOS32_SYS_TimeGet(void);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MIOS32_SYS_H */