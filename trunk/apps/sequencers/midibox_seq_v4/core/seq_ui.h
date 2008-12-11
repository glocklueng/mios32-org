// $Id$
/*
 * Header file for user interface routines
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _SEQ_UI_H
#define _SEQ_UI_H

/////////////////////////////////////////////////////////////////////////////
// Menu Page definitions
/////////////////////////////////////////////////////////////////////////////

// must be kept in sync with ui_init_callback list in seq_ui.c!
#define SEQ_UI_PAGES 6

typedef enum {
  SEQ_UI_PAGE_NONE,
  SEQ_UI_PAGE_EDIT,
  SEQ_UI_PAGE_TRKEVNT,
  SEQ_UI_PAGE_TRKDIR,
  SEQ_UI_PAGE_TRKDIV,
  SEQ_UI_PAGE_TRKLEN
} seq_ui_page_t;


// Prototypes for all UI pages are burried here
extern s32 SEQ_UI_TODO_Init(u32 mode);
extern s32 SEQ_UI_EDIT_Init(u32 mode);
extern s32 SEQ_UI_TRKEVNT_Init(u32 mode);
extern s32 SEQ_UI_TRKDIR_Init(u32 mode);
extern s32 SEQ_UI_TRKDIV_Init(u32 mode);
extern s32 SEQ_UI_TRKLEN_Init(u32 mode);


/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// cursor flash with following waveform:
// 0..399 mS: cursor on
// 400..499 mS: cursor off
#define SEQ_UI_CURSOR_FLASH_CTR_MAX     500  // mS
#define SEQ_UI_CURSOR_FLASH_CTR_LED_OFF 400  // mS


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

typedef union {
  struct {
    unsigned ALL:8;
  };
  struct {
    unsigned MENU_PRESSED:1;
    unsigned MENU_PAGE_DISPLAYED:1;
  };
} seq_ui_button_state_t;


typedef enum {
  SEQ_UI_BUTTON_GP1,
  SEQ_UI_BUTTON_GP2,
  SEQ_UI_BUTTON_GP3,
  SEQ_UI_BUTTON_GP4,
  SEQ_UI_BUTTON_GP5,
  SEQ_UI_BUTTON_GP6,
  SEQ_UI_BUTTON_GP7,
  SEQ_UI_BUTTON_GP8,
  SEQ_UI_BUTTON_GP9,
  SEQ_UI_BUTTON_GP10,
  SEQ_UI_BUTTON_GP11,
  SEQ_UI_BUTTON_GP12,
  SEQ_UI_BUTTON_GP13,
  SEQ_UI_BUTTON_GP14,
  SEQ_UI_BUTTON_GP15,
  SEQ_UI_BUTTON_GP16,
  SEQ_UI_BUTTON_Exit,
  SEQ_UI_BUTTON_Select,
  SEQ_UI_BUTTON_Left,
  SEQ_UI_BUTTON_Right,
  SEQ_UI_BUTTON_Up,
  SEQ_UI_BUTTON_Down
} seq_ui_button_t;


typedef enum {
  SEQ_UI_ENCODER_GP1,
  SEQ_UI_ENCODER_GP2,
  SEQ_UI_ENCODER_GP3,
  SEQ_UI_ENCODER_GP4,
  SEQ_UI_ENCODER_GP5,
  SEQ_UI_ENCODER_GP6,
  SEQ_UI_ENCODER_GP7,
  SEQ_UI_ENCODER_GP8,
  SEQ_UI_ENCODER_GP9,
  SEQ_UI_ENCODER_GP10,
  SEQ_UI_ENCODER_GP11,
  SEQ_UI_ENCODER_GP12,
  SEQ_UI_ENCODER_GP13,
  SEQ_UI_ENCODER_GP14,
  SEQ_UI_ENCODER_GP15,
  SEQ_UI_ENCODER_GP16,
  SEQ_UI_ENCODER_Datawheel
} seq_ui_encoder_t;



/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 SEQ_UI_Init(u32 mode);

extern s32 SEQ_UI_Button_Handler(u32 pin, u32 pin_value);
extern s32 SEQ_UI_Encoder_Handler(u32 encoder, s32 incrementer);
extern s32 SEQ_UI_LED_Handler(void);
extern s32 SEQ_UI_LED_Handler_Periodic();
extern s32 SEQ_UI_MENU_Handler_Periodic();

extern s32 SEQ_UI_PageSet(seq_ui_page_t page);

extern s32 SEQ_UI_InstallInitCallback(void *callback);
extern s32 SEQ_UI_InstallButtonCallback(void *callback);
extern s32 SEQ_UI_InstallEncoderCallback(void *callback);
extern s32 SEQ_UI_InstallLEDCallback(void *callback);
extern s32 SEQ_UI_InstallLCDCallback(void *callback);

extern u8  SEQ_UI_VisibleTrackGet(void);
extern s32 SEQ_UI_GxTyInc(s32 incrementer);
extern s32 SEQ_UI_CCInc(u8 cc, u16 min, u16 max, s32 incrementer);
extern s32 SEQ_UI_CCSetFlags(u8 cc, u16 flag_mask, u16 value);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

extern u8 seq_ui_display_update_req;
extern u8 seq_ui_display_init_req;

extern seq_ui_button_state_t seq_ui_button_state;

extern u8 ui_selected_group;
extern u8 ui_selected_tracks;
extern u8 ui_selected_par_layer;
extern u8 ui_selected_trg_layer;
extern u8 ui_selected_step_view;
extern u8 ui_selected_step;
extern u8 ui_selected_item;

extern volatile u8 ui_cursor_flash;

#endif /* _SEQ_UI_H */
