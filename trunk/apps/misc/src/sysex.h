#ifndef _SYSEX_H
#define _SYSEX_H

/////////////////////////////////////////////////////////////////////////////
// Exported variables
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// command states
#define SYSEX_STATE_GET  0
#define SYSEX_STATE_END   1

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////

typedef union {
  struct {
    unsigned ALL:8;
  };

  struct {
    unsigned CTR:3;           //Counter for the header bytes
    unsigned :1;
    unsigned :1;
    unsigned :1;
    unsigned :1;         
    unsigned MY_SYSEX:1;      //We are currently receiving our SYSEX data
  };

} sysex_state_t;

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern void SYSEX_Init(void);
extern void SYSEX_Parser(u8 midi_in);
extern void SYSEX_SendAck(u8 ack_code, u8 ack_arg);
extern void SYSEX_SendData(u8* array8, u8 length);
extern void SYSEX_SendCommand(u8 type, bool doubleClick, bool tripleClick, bool longClick, u8 control, u8* arguments, u8 length);

#endif /* _SYSEX_H */


