// $Id$
/*
 * Header file for MSD Driver
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MSD_H
#define _MSD_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MSD_Init(u32 mode);
extern s32 MSD_Periodic_mS(void);

extern s32 MSD_CheckAvailable(void);

extern s32 MSD_LUN_AvailableSet(u8 lun, u8 available);
extern s32 MSD_LUN_AvailableGet(u8 lun);

extern s32 MSD_RdLEDGet(u16 lag_ms);
extern s32 MSD_WrLEDGet(u16 lag_ms);

/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MSD_H */
