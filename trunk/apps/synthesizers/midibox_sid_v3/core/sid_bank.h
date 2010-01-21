// $Id$
/*
 * Header file for MBSID Patch Structure
 *
 * ==========================================================================
 *
 *  Copyright (C) 2009 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _SID_BANK_H
#define _SID_BANK_H

#include "sid_patch.h"


/////////////////////////////////////////////////////////////////////////////
// Global Defines
/////////////////////////////////////////////////////////////////////////////

#define SID_BANK_NUM 1  // currently only a single bank is available in ROM


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 SID_BANK_Init(u32 mode);

extern s32 SID_BANK_PatchWrite(sid_patch_ref_t *pr);
extern s32 SID_BANK_PatchRead(sid_patch_ref_t *pr);

extern s32 SID_BANK_PatchNameGet(u8 bank, u8 patch, char *buffer);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _SID_BANK_H */