// $Id$
/*
 * MIDIbox CV V2 mapping functions
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
#include <aout.h>
#include <seq_bpm.h>
#include <app.h>
#include <MbCvEnvironment.h>


#include "mbcv_map.h"
#include "mbcv_patch.h"


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// TODO
static u8 seq_core_din_sync_pulse_ctr;


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
extern "C" s32 MBCV_MAP_Init(u32 mode)
{
  int i;

  // initialize J5 pins
#if 0
  // they will be enabled after the .CV2 file has been read
  // as long as this hasn't been done, activate pull-downs
  for(i=0; i<12; ++i)
    MIOS32_BOARD_J5_PinInit(i, MIOS32_BOARD_PIN_MODE_INPUT_PD);
#else
  // LPC17 is robust enough against shorts (measurements show 20 mA max per pin)
  // we can enable J5 pins by default to simplify usage
  for(i=0; i<12; ++i)
    MIOS32_BOARD_J5_PinInit(i, MIOS32_BOARD_PIN_MODE_OUTPUT_PP);
  for(i=0; i<4; ++i) // J5C replacement for LPC17
    MIOS32_BOARD_J28_PinInit(i, MIOS32_BOARD_PIN_MODE_OUTPUT_PP);
#endif

  // initialize AOUT driver
  AOUT_Init(0);

  // change default config to AOUT by default
  MBCV_MAP_IfSet(AOUT_IF_MAX525);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Get/Set/Name AOUT interface
/////////////////////////////////////////////////////////////////////////////
extern "C" s32 MBCV_MAP_IfSet(aout_if_t if_type)
{
  if( if_type >= AOUT_NUM_IF )
    return -1; // invalid interface

  aout_config_t config;
  config = AOUT_ConfigGet();
  config.if_type = if_type;
  config.if_option = (config.if_type == AOUT_IF_74HC595) ? 0xffffffff : 0x00000000; // AOUT_LC: select 8/8 bit configuration
  config.num_channels = 8;
  //config.chn_inverted = 0; // configurable
  AOUT_ConfigSet(config);
  return AOUT_IF_Init(0);
}

extern "C" aout_if_t MBCV_MAP_IfGet(void)
{
  aout_config_t config;
  config = AOUT_ConfigGet();
  return config.if_type;
}


// will return 8 characters
extern "C" const char* MBCV_MAP_IfNameGet(aout_if_t if_type)
{
  return AOUT_IfNameGet(if_type);
}


/////////////////////////////////////////////////////////////////////////////
// Get/Set/Name Calibration Mode
/////////////////////////////////////////////////////////////////////////////

extern "C" s32 MBCV_MAP_CaliModeSet(u8 cv, aout_cali_mode_t mode)
{
  if( cv >= CV_SE_NUM )
    return -1; // invalid cv channel selected

  if( mode >= MBCV_MAP_NUM_CALI_MODES )
    return -2; // invalid mode selected

  return AOUT_CaliModeSet(cv, mode);

}

extern "C" aout_cali_mode_t MBCV_MAP_CaliModeGet(void)
{
  return AOUT_CaliModeGet();
}

extern "C" const char* MBCV_MAP_CaliNameGet(void)
{
  return AOUT_CaliNameGet(MBCV_MAP_CaliModeGet());
}


/////////////////////////////////////////////////////////////////////////////
// Updates all CV channels and gates
/////////////////////////////////////////////////////////////////////////////
extern "C" s32 MBCV_MAP_Update(void)
{
  static u8 last_gates = 0xff; // to force an update
  static u8 last_start_stop = 0xff; // to force an update

  // retrieve the AOUT values of all channels
  MbCvEnvironment* env = APP_GetEnv();

  u16 *out = env->cvOut.first();
  for(int cv=0; cv<CV_SE_NUM; ++cv, ++out)
    AOUT_PinSet(cv, *out);

  AOUT_DigitalPinsSet(env->cvGates);

  // Start/Stop at J5C.A9
  u8 start_stop = SEQ_BPM_IsRunning();
  if( start_stop != last_start_stop ) {
    last_start_stop = start_stop;
#if defined(MIOS32_FAMILY_STM32F10x)
    MIOS32_BOARD_J5_PinSet(9, start_stop);
#elif defined(MIOS32_FAMILY_LPC17xx)
    MIOS32_BOARD_J28_PinSet(1, start_stop);
#else
# warning "please adapt for this MIOS32_FAMILY"
#endif
  }

  // DIN Sync Pulse at J5C.A8
  if( seq_core_din_sync_pulse_ctr > 1 ) {
#if defined(MIOS32_FAMILY_STM32F10x)
    MIOS32_BOARD_J5_PinSet(8, 1);
#elif defined(MIOS32_FAMILY_LPC17xx)
    MIOS32_BOARD_J28_PinSet(0, 1);
#else
# warning "please adapt for this MIOS32_FAMILY"
#endif
    --seq_core_din_sync_pulse_ctr;
  } else if( seq_core_din_sync_pulse_ctr == 1 ) {
#if defined(MIOS32_FAMILY_STM32F10x)
    MIOS32_BOARD_J5_PinSet(8, 0);
#elif defined(MIOS32_FAMILY_LPC17xx)
    MIOS32_BOARD_J28_PinSet(0, 0);
#else
# warning "please adapt for this MIOS32_FAMILY"
#endif

    seq_core_din_sync_pulse_ctr = 0;
  }

  // update AOUTs
  AOUT_Update();

  // update J5 Outputs (forwarding AOUT digital pins for modules which don't support gates)
  u8 gates = AOUT_DigitalPinsGet();
  if( gates != last_gates ) {
    int i;

    last_gates = gates;
    for(i=0; i<8; ++i) {
      MIOS32_BOARD_J5_PinSet(i, gates & 1);
      gates >>= 1;
    }

    // for compatibility with MBSEQ V4 where J5B.A6 and J5B.A7 allocated by MIDI OUT3
#if defined(MIOS32_FAMILY_STM32F10x)
    // -> Gate 7 and 8 also routed to J5C.A10 and J5C.A11
    MIOS32_BOARD_J5_PinSet(10, (last_gates & 0x40) ? 1 : 0);
    MIOS32_BOARD_J5_PinSet(11, (last_gates & 0x80) ? 1 : 0);
#elif defined(MIOS32_FAMILY_LPC17xx)
    // -> Gate 7 and 8 also routed to J28.WS and J28.MCLK
    MIOS32_BOARD_J28_PinSet(2, (last_gates & 0x40) ? 1 : 0);
    MIOS32_BOARD_J28_PinSet(3, (last_gates & 0x80) ? 1 : 0);
#else
# warning "please adapt for this MIOS32_FAMILY"
#endif
  }

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Called to reset all channels/notes (e.g. after session change)
/////////////////////////////////////////////////////////////////////////////
extern "C" s32 MBCV_MAP_ResetAllChannels(void)
{
  MbCvEnvironment* env = APP_GetEnv();
  int cv;

  for(cv=0; cv<CV_SE_NUM; ++cv)
    env->mbCv[cv].noteAllOff(false);

  // reset AOUT voltages
  for(cv=0; cv<CV_SE_NUM; ++cv) {
    AOUT_PinPitchSet(cv, 0x0000);
    AOUT_PinSet(cv, 0x0000);
  }

  return 0; // no error
}
