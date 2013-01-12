// $Id$
//! \defgroup MBNG_FILE_C
//! Config File access functions
//! 
//! NOTE: before accessing the SD Card, the upper level function should
//! synchronize with the SD Card semaphore!
//!   MUTEX_SDCARD_TAKE; // to take the semaphore
//!   MUTEX_SDCARD_GIVE; // to release the semaphore
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2012 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
//! Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <string.h>
#include <uip_task.h>

#include <midi_port.h>
#include <midi_router.h>
#include <seq_bpm.h>
#include <ainser.h>
#include <aout.h>

#include "tasks.h"
#include "file.h"
#include "mbng_file.h"
#include "mbng_file_c.h"
#include "mbng_patch.h"
#include "mbng_event.h"
#include "mbng_dout.h"
#include "mbng_din.h"
#include "mbng_enc.h"
#include "mbng_ain.h"
#include "mbng_ainser.h"
#include "mbng_mf.h"
#include "mbng_cv.h"
#include "mbng_matrix.h"
#include "mbng_lcd.h"

#if !defined(MIOS32_FAMILY_EMULATION)
#include "uip.h"
#include "uip_task.h"
#include "osc_server.h"
#include "osc_client.h"
#endif


/////////////////////////////////////////////////////////////////////////////
//! for optional debugging messages via DEBUG_MSG (defined in mios32_config.h)
/////////////////////////////////////////////////////////////////////////////

// Note: verbose level 1 is default - it prints error messages!
#define DEBUG_VERBOSE_LEVEL 1


/////////////////////////////////////////////////////////////////////////////
//! Local definitions
/////////////////////////////////////////////////////////////////////////////

// in which subdirectory of the SD card are the files located?
// use "/" for root
// use "/<dir>/" for a subdirectory in root
// use "/<dir>/<subdir>/" to reach a subdirectory in <dir>, etc..

#define MBNG_FILES_PATH "/"
//#define MBNG_FILES_PATH "/MySongs/"


/////////////////////////////////////////////////////////////////////////////
//! Local types
/////////////////////////////////////////////////////////////////////////////

// file informations stored in RAM
typedef struct {
  unsigned valid: 1;   // file is accessible
} mbng_file_c_info_t;


/////////////////////////////////////////////////////////////////////////////
//! Local prototypes
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//! Local variables
/////////////////////////////////////////////////////////////////////////////

static mbng_file_c_info_t mbng_file_c_info;


/////////////////////////////////////////////////////////////////////////////
//! Global variables
/////////////////////////////////////////////////////////////////////////////
char mbng_file_c_config_name[MBNG_FILE_C_FILENAME_LEN+1];


/////////////////////////////////////////////////////////////////////////////
//! Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_C_Init(u32 mode)
{
  // invalidate file info
  MBNG_FILE_C_Unload();

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Loads patch file
//! Called from MBNG_FILE_CheckSDCard() when the SD card has been connected
//! \returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_C_Load(char *filename)
{
  s32 error;
  error = MBNG_FILE_C_Read(filename);
#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MBNG_FILE_C] Tried to open patch %s, status: %d\n", filename, error);
#endif

  return error;
}


/////////////////////////////////////////////////////////////////////////////
//! Unloads patch file
//! Called from MBNG_FILE_CheckSDCard() when the SD card has been disconnected
//! \returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_C_Unload(void)
{
  mbng_file_c_info.valid = 0;

  return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
//! \Returns 1 if current patch file valid
//! \Returns 0 if current patch file not valid
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_C_Valid(void)
{
  return mbng_file_c_info.valid;
}


/////////////////////////////////////////////////////////////////////////////
//! help function which removes the quotes of an argument (e.g. .csv file format)
//! can be cascaded with strtok_r
/////////////////////////////////////////////////////////////////////////////
static char *remove_quotes(char *word)
{
  if( word == NULL )
    return NULL;

  if( *word == '"' )
    ++word;

  int len = strlen(word);
  if( len && word[len-1] == '"' )
    word[len-1] = 0;

  return word;
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses a decimal or hex value
//! \returns >= 0 if value is valid
//! \returns -1000000000 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static s32 get_dec(char *word)
{
  if( word == NULL )
    return -1000000000;

  char *next;
  long l = strtol(word, &next, 0);

  if( word == next )
    return -1000000000;

  return l; // value is valid
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses a SR definition (<dec>.D<dec>)
//! \returns >= 0 if value is valid
//! \returns -1 if value is invalid
//! \returns -2 if SR number 0 has been passed (could disable a SR definition)
/////////////////////////////////////////////////////////////////////////////
static s32 get_sr(char *word)
{
  if( word == NULL )
    return -1;

  // check for '.D' separator
  char *word2 = word;
  while( *word2 != '.' )
    if( *word2++ == 0 )
      return -1;
  word2++;
  if( *word2++ != 'D' )
    return -1;

  s32 srNum = get_dec(word);
  if( srNum < 0 )
    return -1;

  if( srNum == 0 )
    return -2; // SR has been disabled...

  s32 pinNum = get_dec(word2);
  if( pinNum < 0 )
    return -1;

  if( pinNum >= 8 )
    return -1;

  return 8*(srNum-1) + pinNum;
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses an IP value
//! \returns > 0 if value is valid
//! \returns 0 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static u32 get_ip(char *brkt)
{
  u8 ip[4];
  char *word;

  int i;
  for(i=0; i<4; ++i) {
    if( (word=remove_quotes(strtok_r(NULL, ".", &brkt))) ) {
      s32 value = get_dec(word);
      if( value >= 0 && value <= 255 )
	ip[i] = value;
      else
	return 0;
    }
  }

  if( i == 4 )
    return (ip[0]<<24)|(ip[1]<<16)|(ip[2]<<8)|(ip[3]<<0);
  else
    return 0; // invalid IP
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses a binary value
//! \returns >= 0 if value is valid
//! \returns -1000000000 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static s32 get_bin(char *word, int numBits, u8 reverse)
{
  if( word == NULL )
    return -1;

  s32 value = 0;
  int bit = 0;
  while( 1 ) {
    if( *word == '1' ) {
      if( reverse )
	value |= 1 << (numBits-bit-1);
      else
	value |= 1 << bit;
    } else if( *word != '0' ) {
      break;
    }
    ++word;
    ++bit;
  }

  if( bit != numBits )
    return -1; // invalid number of bits

  return value;
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses a MIDI IN port parameter
//! \returns >= 0 if value is valid
//! \returns < 0 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static s32 parseMidiInPort(char *value_str)
{
  int in_port = 0xff;
  int port_ix;
  for(port_ix=0; port_ix<MIDI_PORT_InNumGet(); ++port_ix) {
    // terminate port name at first space
    char port_name[10];
    strcpy(port_name, MIDI_PORT_InNameGet(port_ix));
    int i; for(i=0; i<strlen(port_name); ++i) if( port_name[i] == ' ' ) port_name[i] = 0;

    if( strcasecmp(value_str, port_name) == 0 ) {
      in_port = MIDI_PORT_InPortGet(port_ix);
      break;
    }
  }

  if( in_port == 0xff && ((in_port=get_dec(value_str)) < 0 || in_port > 0xff) )
      return -1; // invalid port

  return in_port; // valid port
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses a MIDI OUT port parameter
//! \returns >= 0 if value is valid
//! \returns < 0 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static s32 parseMidiOutPort(char *value_str)
{
  int out_port = 0xff;
  int port_ix;

  for(port_ix=0; port_ix<MIDI_PORT_OutNumGet(); ++port_ix) {
    // terminate port name at first space
    char port_name[10];
    strcpy(port_name, MIDI_PORT_OutNameGet(port_ix));
    int i; for(i=0; i<strlen(port_name); ++i) if( port_name[i] == ' ' ) port_name[i] = 0;
    
    if( strcasecmp(value_str, port_name) == 0 ) {
      out_port = MIDI_PORT_OutPortGet(port_ix);
      break;
    }
  }

  if( out_port == 0xff && ((out_port=get_dec(value_str)) < 0 || out_port > 0xff) )
      return -1; // invalid port

  return out_port; // valid port
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses a simple value definition, and outputs error message if invalid
//! \returns >= 0 if value is valid
//! \returns < 0 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static s32 parseSimpleValue(char *parameter, char **brkt, int min, int max)
{
  int value;
  char *value_str = *brkt;
  if( (value=get_dec(*brkt)) < min || value > max ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR invalid value for parameter '%s %s', range should be within %d..%d!\n", parameter, value_str, min, max);
#endif
    return -1000000000;
  }

  return value;
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses a quoted string
//! \returns >= 0 if string is valid
//! \returns < 0 if string is invalid
/////////////////////////////////////////////////////////////////////////////
static char *getQuotedString(char **brkt)
{
  char *value_str;

  if( *brkt == NULL )
    return NULL;

  value_str = *brkt;
  {
    int quote_started = 0;

    char *search_str = *brkt;
    for(; *search_str != 0 && (*search_str == ' ' || *search_str == '\t'); ++search_str);

    if( *search_str == '\'' || *search_str == '"' ) {
      quote_started = 1;
      ++search_str;
    }

    value_str = search_str;

    if( quote_started ) {
      for(; *search_str != 0 && *search_str != '\'' && *search_str != '\"'; ++search_str);
    } else {
      for(; *search_str != 0 && *search_str != ' ' && *search_str != '\t'; ++search_str);
    }

    if( *search_str != 0 ) {
      *search_str = 0;
      ++search_str;
    }

    *brkt = search_str;
  }

  // end of command line reached?
  if( (*brkt)[0] == 0 )
    *brkt = NULL;

  return value_str[0] ? value_str : NULL;
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses an extended parameter with the syntax
//! 'parameter=value_str'
//! \returns >= 0 if parameter is valid
//! \returns <0 if parameter is invalid or no more parameters found
/////////////////////////////////////////////////////////////////////////////
static s32 parseExtendedParameter(char *cmd, char **parameter, char **value_str, char **brkt)
{
  const char *separators = " \t";

  // parameter name + '='
  if( !(*parameter = strtok_r(NULL, separators, brkt)) ) {
    return -1; // no additional parameter
  }

  // store this information, because it stores the information that the end of the command line has been reached
  u8 brkt_was_NULL = *brkt == NULL;

  // since strstr() doesn't work for unknown reasons (maybe newlib issue)
  u8 eq_found = 0;
  char *search_str = *parameter;

  for(; *search_str != 0 && !eq_found; ++search_str) {
    if( *search_str == '=' ) {
      eq_found = 1;
      *search_str = 0;
      if( search_str[1] != 0 ) {
	*brkt = (char *)&search_str[1];
	(*brkt)[strlen(*brkt)] = brkt_was_NULL ? 0 : ' ';
      }
    }
  }

  // if '=' wasn't found, check if there are spaces between
  if( !eq_found && *brkt != NULL ) {
    // since strstr() doesn't work for unknown reasons (maybe newlib issue)
    char *search_str = *brkt;
    for(; *search_str != 0 && !eq_found; ++search_str) {
      if( *search_str == '=' ) {
	eq_found = 1;
	*brkt = (char *)&search_str[1];
      }
    }
  }

  if( *brkt == NULL || (*brkt)[0] == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: missing value after %s ... %s\n", cmd, *parameter);
#endif
    return -2; // missing value
  } else if( !eq_found ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: missing '=' after %s ... %s\n", cmd, *parameter);
#endif
    return -1;
  }

  // we can't use strtok_r here, since we have to consider quotes...
  *value_str = getQuotedString(brkt);

  // end of command line reached?
  if( brkt_was_NULL || (*brkt)[0] == 0 )
    *brkt = NULL;

  if( *value_str == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: missing value after %s ... %s\n", cmd, *parameter);
#endif
    return -2; // missing value
  }

#if 0
  DEBUG_MSG("%s -> '%s'\n", *parameter, *value_str);
#endif

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses a EVENT command and adds it to the pool
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseEvent(char *cmd, char *brkt)
{
  mbng_event_item_t item;
  char *event = (char *)&cmd[6]; // remove "EVENT_"

  MBNG_EVENT_ItemInit(&item, MBNG_EVENT_ItemIdFromControllerStrGet(event));

#define STREAM_MAX_SIZE 128
  u8 stream[STREAM_MAX_SIZE];
  item.stream = stream;
  item.stream_size = 0;

#define LABEL_MAX_SIZE 41
  char label[LABEL_MAX_SIZE];
  item.label = label;
  label[0] = 0;

  if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_DISABLED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s item not supported!\n", event);
#endif
    return -1;
  }

  // parse the parameters
  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 
    const char *separator_colon = ":";
    const char *separators = " \t;";

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "id") == 0 ) {
      int id;
      if( (id=get_dec(value_str)) < 1 || id > 0xfff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid ID in EVENT_%s ... %s=%s (expect 1..%d)\n", event, parameter, value_str, 0xfff);
#endif
	return -1;
      } else {
	item.id = (item.id & 0xf000) | id;
	if( !(item.hw_id & 0xfff) )
	  item.hw_id = item.id; // default hardware ID if not already define before
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "hw_id") == 0 ) {
      int hw_id;
      if( (hw_id=get_dec(value_str)) < 1 || hw_id > 0xfff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid HW_ID in EVENT_%s ... %s=%s (expect 1..%d)\n", event, parameter, value_str, 0xfff);
#endif
	return -1;
      } else {
	item.hw_id = (item.hw_id & 0xf000) | hw_id; // HW_ID can be modified
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "bank") == 0 ) {
      int bank;
      if( (bank=get_dec(value_str)) < 0 || bank > 255 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid bank in EVENT_%s ... %s=%s (expect 0..255)\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.bank = bank;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "type") == 0 ) {
      mbng_event_type_t event_type = MBNG_EVENT_ItemTypeFromStrGet(value_str);
      if( event_type == MBNG_EVENT_TYPE_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid event type in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.flags.general.type = event_type;

	switch( event_type ) {
	case MBNG_EVENT_TYPE_NOTE_OFF:
	case MBNG_EVENT_TYPE_NOTE_ON:
	case MBNG_EVENT_TYPE_POLY_PRESSURE:
	case MBNG_EVENT_TYPE_CC: {
	  item.stream_size = 2;
	  item.stream[0] = 0x80 | ((event_type-1) << 4);
	  item.stream[1] = 0x30;
	} break;

	case MBNG_EVENT_TYPE_PROGRAM_CHANGE:
	case MBNG_EVENT_TYPE_AFTERTOUCH:
	case MBNG_EVENT_TYPE_PITCHBEND: {
	  item.stream_size = 1;
	  item.stream[0] = 0x80 | ((event_type-1) << 4);
	} break;

	case MBNG_EVENT_TYPE_SYSEX: {
	  item.stream_size = 0; // initial
	} break;

	case MBNG_EVENT_TYPE_NRPN: {
	  item.stream_size = 4;
	  item.stream[0] = 0xb0; // match on CC, will also store channel
	  item.stream[1] = 0x00; // number
	  item.stream[2] = 0x00;
	  item.stream[3] = MBNG_EVENT_NRPN_FORMAT_UNSIGNED; // value format
	} break;

	case MBNG_EVENT_TYPE_META: {
	  item.stream_size = 0;
	} break;

	default:
	  item.stream_size = 0;
	}
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "chn") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 1 || value > 16 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid MIDI channel in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	if( item.flags.general.type == MBNG_EVENT_TYPE_SYSEX || item.flags.general.type == MBNG_EVENT_TYPE_META ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] WARNING: no MIDI channel expected for EVENT_%s due to type: %s\n", event, MBNG_EVENT_ItemTypeStrGet(&item));
#endif
	} else {
	  // no extra check if event_type already defined...
	  stream[0] = (stream[0] & 0xf0) | (value-1);
	}
      }

    } else if( strcasecmp(parameter, "key") == 0 ) {
      int value;

      if( strcasecmp(value_str, "any") == 0 ) {
	value = 128;
      } else if( (value=get_dec(value_str)) < 0 || value > 127 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid key number in EVENT_%s ... %s=%s (expect 0..127 or 'any')\n", event, parameter, value_str);
#endif
	return -1;
      }

      if( item.flags.general.type != MBNG_EVENT_TYPE_NOTE_OFF &&
	  item.flags.general.type != MBNG_EVENT_TYPE_NOTE_ON &&
	  item.flags.general.type != MBNG_EVENT_TYPE_POLY_PRESSURE ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] WARNING: no key number expected for EVENT_%s due to type: %s\n", event, MBNG_EVENT_ItemTypeStrGet(&item));
#endif
      } else {
	// no extra check if event_type already defined...
	stream[1] = value;
      }

    } else if( strcasecmp(parameter, "use_key_number") == 0 || strcasecmp(parameter, "use_cc_number") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid flag in EVENT_%s ... %s=%s (expect 0 or 1)\n", event, parameter, value_str);
#endif
	return -1;
      }

      item.flags.general.use_key_or_cc = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "cc") == 0 ) {
      int value;

      if( strcasecmp(value_str, "any") == 0 ) {
	value = 128;
      } else if( (value=get_dec(value_str)) < 0 || value > 127 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid CC number in EVENT_%s ... %s=%s (expect 0..127 or 'any')\n", event, parameter, value_str);
#endif
	return -1;
      }

      if( item.flags.general.type != MBNG_EVENT_TYPE_CC ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] WARNING: no CC number expected for EVENT_%s due to type: %s\n", event, MBNG_EVENT_ItemTypeStrGet(&item));
#endif
      } else {
	// no extra check if event_type already defined...
	stream[1] = value;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "nrpn") == 0 ) {
      int value;

      if( (value=get_dec(value_str)) < 0 || value >= 16384 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid NRPN number in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	if( item.flags.general.type != MBNG_EVENT_TYPE_NRPN ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] WARNING: no NRPN number expected for EVENT_%s due to type: %s\n", event, MBNG_EVENT_ItemTypeStrGet(&item));
#endif
	} else {
	  // no extra check if event_type already defined...
	  stream[1] = value & 0xff;
	  stream[2] = value >> 8;
	}
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "nrpn_format") == 0 ) {
      mbng_event_nrpn_format_t nrpn_format = MBNG_EVENT_ItemNrpnFormatFromStrGet(value_str);
      if( nrpn_format == MBNG_EVENT_NRPN_FORMAT_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid NRPN format in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	// no extra check if event_type already defined...
	stream[3] = nrpn_format;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "stream") == 0 ) {
      if( item.flags.general.type != MBNG_EVENT_TYPE_SYSEX ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: stream is only expected for SysEx events in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      }

      char *stream_str;
      char *brkt_local = value_str;
      u8 *stream_pos = (u8 *)&stream[item.stream_size];
      // check for STREAM_MAX_SIZE-1, since a meta entry allocates 2 bytes
      while( item.stream_size < (STREAM_MAX_SIZE-1) && (stream_str = strtok_r(NULL, separators, &brkt_local)) ) {
	if( *stream_str == '^' ) {
	  mbng_event_sysex_var_t sysex_var = MBNG_EVENT_ItemSysExVarFromStrGet((char *)&stream_str[1]);
	  if( sysex_var == MBNG_EVENT_SYSEX_VAR_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_C] ERROR: unknown SysEx variable '%s' in EVENT_%s ... %s=%s\n", stream_str, event, parameter, value_str);
#endif
	    return -1;
	  } else {
	    *stream_pos = 0xff; // meta indicator
	    ++stream_pos;
	    ++item.stream_size;
	    *stream_pos = (u8)sysex_var;
	  }
	} else {
	  int value;
	  if( (value=get_dec(stream_str)) < 0 || value > 0xff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid SysEx value '%s' in EVENT_%s ... %s=%s, expecting 0..127 (0x00..0x7f)\n", stream_str, event, parameter, value_str);
#endif
	    return -1;
	  } else {
	    *stream_pos = (u8)value;
	  }
	}

	++stream_pos;
	++item.stream_size;
      }

      if( !item.stream_size ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: stream is empty in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "meta") == 0 ) {
      if( item.flags.general.type != MBNG_EVENT_TYPE_META ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: meta is only expected for Meta events in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      }

      char *values_str = value_str;
      char *brkt_local;

      mbng_event_meta_type_t meta_type;
      if( !(values_str = strtok_r(value_str, separator_colon, &brkt_local)) ||
	  (meta_type = MBNG_EVENT_ItemMetaTypeFromStrGet(values_str)) == MBNG_EVENT_META_TYPE_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid meta type in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      }

      int value = 0;
      if( !(values_str = strtok_r(NULL, separator_colon, &brkt_local)) ||
	  (value=get_dec(values_str)) < 0 || value > 255 ) {
	// ignore - we allow meta events without values
	value = 0;
      }

      int entry = item.stream_size / 2;
      item.stream[2*entry + 0] = meta_type;
      item.stream[2*entry + 1] = value;
      item.stream_size += 2;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "range") == 0 ) {
      if( strncasecmp(value_str, "map", 3) == 0 ) {
	int value;
	if( (value=get_dec((char *)&value_str[3])) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid map number in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	} else if( value == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: map0 doesn't make much sense in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	} else if( value >= 256 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid map in EVENT_%s ... %s=%s (expected 1..255)\n", event, parameter, value_str);
#endif
	} else {
	  item.map = value; // will modify item.min/max in postprocessing step
	}
      } else {
	char *values_str = value_str;
	char *brkt_local;
	int values[2];
	if( !(values_str = strtok_r(value_str, separator_colon, &brkt_local)) ||
	    (values[0]=get_dec(values_str)) <= -1000000000 ||
	    !(values_str = strtok_r(NULL, separator_colon, &brkt_local)) ||
	    (values[1]=get_dec(values_str)) <= -1000000000 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid range format in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	  return -1;
	} else {
	  if( values[0] < -16384 || values[0] > 16383 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid min value in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	  } else if( values[1] < -16384 || values[1] > 16383 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid max value in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	  } else {
	    item.min = values[0];
	    item.max = values[1];
	  }
	}
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "value") == 0 ) {
      // currently not written into .NGC file on save, only (optional) read
      // could become obsolete with 'value snapshot' files
      int value;

      if( (value=get_dec(value_str)) < 0 || value >= 65536 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid value in EVENT_%s ... %s=%s (expect 0..65535)\n", event, parameter, value_str);
#endif
      } else {
	item.value = value;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "secondary_value") == 0 ) {
      // currently not written into .NGC file on save, only (optional) read
      // could become obsolete with 'value snapshot' files
      int value;

      if( (value=get_dec(value_str)) < 0 || value >= 256 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid value in EVENT_%s ... %s=%s (expect 0..255)\n", event, parameter, value_str);
#endif
      } else {
	item.secondary_value = value;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "syxdump_pos") == 0 ) {
      char *values_str = value_str;
      char *brkt_local;

      int receiver;
      if( !(values_str = strtok_r(value_str, separator_colon, &brkt_local)) ||
	  (receiver=get_dec(values_str)) < 1 || receiver > 15 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid receiver number in EVENT_%s ... %s=%s (expect 1..15)\n", event, parameter, value_str);
#endif
	return -1;
      }

      int value = 0;
      if( !(value_str = strtok_r(NULL, separator_colon, &brkt_local)) ||
	  (value=get_dec(value_str)) < 0 || value > 0xfff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid dump position in EVENT_%s ... %s=%s (expect 0..%d)\n", event, parameter, value_str, 0xfff);
#endif
	return -1;
      }

      item.syxdump_pos.receiver = receiver;
      item.syxdump_pos.pos = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "button_mode") == 0 ) {
      mbng_event_button_mode_t button_mode = MBNG_EVENT_ItemButtonModeFromStrGet(value_str);
      if( button_mode == MBNG_EVENT_BUTTON_MODE_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid button mode in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else if( (item.id & 0xf000) != MBNG_EVENT_CONTROLLER_BUTTON ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_BUTTON!\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.flags.DIN.button_mode = button_mode;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "radio_group") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 63 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid value in EVENT_%s ... %s=%s (expecting 0..63)\n", event, parameter, value_str);
#endif
	return -1;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_BUTTON ) {
	item.flags.DIN.radio_group = value;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_LED ) {
	item.flags.DOUT.radio_group = value;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_SENDER ) {
	item.flags.SENDER.radio_group = value;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_RECEIVER ) {
	item.flags.RECEIVER.radio_group = value;
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_BUTTON, LED, SENDER or RECEIVER\n", event, parameter, value_str);
#endif
	return -1;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "ain_mode") == 0 ) {
      mbng_event_ain_mode_t ain_mode = MBNG_EVENT_ItemAinModeFromStrGet(value_str);
      if( ain_mode == MBNG_EVENT_AIN_MODE_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid ain_mode in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_AIN ) {
	item.flags.AIN.ain_mode = ain_mode;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_AINSER ) {
	item.flags.AINSER.ain_mode = ain_mode;
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_AIN or EVENT_AINSER!\n", event, parameter, value_str);
#endif
	return -1;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "enc_mode") == 0 ) {
      mbng_event_enc_mode_t enc_mode = MBNG_EVENT_ItemEncModeFromStrGet(value_str);
      if( enc_mode == MBNG_EVENT_ENC_MODE_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid enc_mode in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else if( (item.id & 0xf000) != MBNG_EVENT_CONTROLLER_ENC ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_ENC!\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.flags.ENC.enc_mode = enc_mode;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "enc_speed_mode") == 0 ) {
      if( (item.id & 0xf000) != MBNG_EVENT_CONTROLLER_ENC ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_ENC!\n", event, parameter, value_str);
#endif
	return -1;
      }

      char *values_str = value_str;
      char *brkt_local;

      mbng_event_enc_speed_mode_t enc_speed_mode;
      if( !(values_str = strtok_r(value_str, separator_colon, &brkt_local)) ||
	  (enc_speed_mode = MBNG_EVENT_ItemEncSpeedModeFromStrGet(values_str)) == MBNG_EVENT_ENC_SPEED_MODE_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid speed mode in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      }

      int value = 0; // we allow that no value is specified
      if( (values_str = strtok_r(NULL, separator_colon, &brkt_local)) ) {
	if( (value=get_dec(values_str)) < 0 || value > 7 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid speed mode parameter in EVENT_%s ... %s=%s (expect 0..7)\n", event, parameter, value_str);
#endif
	  return -1;
	}
      }

      item.flags.ENC.enc_speed_mode = enc_speed_mode;
      item.flags.ENC.enc_speed_mode_par = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "led_matrix_pattern") == 0 ) {
      mbng_event_led_matrix_pattern_t led_matrix_pattern = MBNG_EVENT_ItemLedMatrixPatternFromStrGet(value_str);
      if( led_matrix_pattern == MBNG_EVENT_LED_MATRIX_PATTERN_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid led_matrix_pattern in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.flags.ENC.led_matrix_pattern = led_matrix_pattern;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "offset") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < -16384 || value > 16383 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid offset value in EVENT_%s ... %s=%s (expecting -16384..16383)\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.offset = value;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "ports") == 0 ) {
      int value;
      if( (value=get_bin(value_str, 16, 0)) < 0 || value > 0xffff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid port mask in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.enabled_ports = value;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "fwd_id") == 0 ) {
      char *values_str = value_str;
      char *brkt_local;

      mbng_event_item_id_t fwd_id;
      if( !(values_str = strtok_r(value_str, separator_colon, &brkt_local)) ||
	  (fwd_id=MBNG_EVENT_ItemIdFromControllerStrGet(value_str)) == MBNG_EVENT_CONTROLLER_DISABLED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid fwd_id controller name in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      }

      int id_lower = 0;
      if( !(value_str = strtok_r(NULL, separator_colon, &brkt_local)) ||
	  (id_lower=get_dec(value_str)) < 1 || id_lower > 0xfff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid fwd_id in EVENT_%s ... %s=%s (expect 1..%d)\n", event, parameter, value_str, 0xfff);
#endif
	return -1;
      }

      item.fwd_id = fwd_id | id_lower;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "fwd_to_lcd") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid value in EVENT_%s ... %s=%s (expecting 0 or 1)\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	item.flags.general.fwd_to_lcd = value;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "fwd_gate_to_dout_pin") == 0 ) {
      int value;
      if( (value=get_sr(value_str)) < 0 || value >= 8*MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid DOUT pin in EVENT_%s ... %s=%s (expecting <1..%d>.D<0..7>)\n", event, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_CV ) {
	item.flags.CV.fwd_gate_to_dout_pin = 1 + ((value & 0xfff8) | (7-(value & 7))); // since DOUT data outputs are mirrored
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_CV!\n", event, parameter, value_str);
#endif
	return -1;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "cv_inverted") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid value in EVENT_%s ... %s=%s (expecting 0 or 1)\n", event, parameter, value_str);
#endif
	return -1;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_CV ) {
	item.flags.CV.cv_inverted = value;
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_CV!\n", event, parameter, value_str);
#endif
	return -1;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "cv_gate_inverted") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid value in EVENT_%s ... %s=%s (expecting 0 or 1)\n", event, parameter, value_str);
#endif
	return -1;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_CV ) {
	item.flags.CV.cv_gate_inverted = value;
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_CV!\n", event, parameter, value_str);
#endif
	return -1;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "cv_hz_v") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid value in EVENT_%s ... %s=%s (expecting 0 or 1)\n", event, parameter, value_str);
#endif
	return -1;
      } else if( (item.id & 0xf000) == MBNG_EVENT_CONTROLLER_CV ) {
	item.flags.CV.cv_hz_v = value;
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: EVENT_%s ... %s=%s only expected for EVENT_CV!\n", event, parameter, value_str);
#endif
	return -1;
      }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "lcd_pos") == 0 ) {
      char *values_str = value_str;
      char *brkt_local;
      int values[3];
      if( !(values_str = strtok_r(value_str, separator_colon, &brkt_local)) ||
	  (values[0]=get_dec(values_str)) < 0 ||
	  !(values_str = strtok_r(NULL, separator_colon, &brkt_local)) ||
	  (values[1]=get_dec(values_str)) < 0 ||
	  !(values_str = strtok_r(NULL, separator_colon, &brkt_local)) ||
	  (values[2]=get_dec(values_str)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid LCD position format in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	if( values[0] < 1 || values[0] >= 256 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid LCD number (first item) in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	} else if( values[1] < 1 || values[1] >= 64 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid LCD X position (second item) in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	} else if( values[2] < 1 || values[2] >= 4 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid LCD Y position (third item) in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	} else {
	  item.lcd = values[0] - 1;
	  item.lcd_pos = ((values[1]-1) & 0x3f) | ((values[2]-1) & 0x3) << 6;
	}
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "label") == 0 ) {
      if( strlen(value_str) >= LABEL_MAX_SIZE ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: string to long in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
	return -1;
      } else {
	strcpy(item.label, value_str);
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in EVENT_%s ... %s=%s\n", event, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

#if DEBUG_VERBOSE_LEVEL >= 2
  MBNG_EVENT_ItemPrint(&item);
#endif

  if( MBNG_EVENT_ItemAdd(&item) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: couldn't add EVENT_%s ... id=%d: out of memory!\n", event, item.id & 0xfff);
#endif
    return -2;
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// help function which parses a MAP definition and adds it to the pool
// returns >= 0 if command is valid
// returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseMap(char *cmd, char *brkt)
{
  const char *separators = " \t;";
  int map;

  if( (map=get_dec((char *)&cmd[3])) < 1 || map >= 256 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid map number in %s\n", cmd);
#endif
  }

#define MAP_VALUE_MAX_SIZE 128
  u8 map_values[MAP_VALUE_MAX_SIZE];

  int pos = 0;
  char *value_str;
  while( pos < MAP_VALUE_MAX_SIZE && (value_str = strtok_r(NULL, separators, &brkt)) ) {
    int value;
    if( (value=get_dec(value_str)) < 0 || value > 0xff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid map value '%s' in %s, expecting 0..255 (0x00..0x7f)\n", value_str, cmd);
#endif
      return -1;
    } else {
      map_values[pos++] = (u8)value;
    }
  }

  if( !pos ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: %s doesn't define any value!\n", cmd);
#endif
  } else if( pos >= MAP_VALUE_MAX_SIZE ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: too many values defined for %s (max: %d)!\n", cmd, MAP_VALUE_MAX_SIZE);
#endif
  } else if( MBNG_EVENT_MapAdd(map, map_values, pos) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR: failed to add %s to the pool!\n", cmd);
#endif
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// help function which parses SYSEX_VAR definitions
// returns >= 0 if command is valid
// returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseSysExVar(char *cmd, char *brkt)
{
  // parse the parameters
  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 
    char full_parameter_str[60];
    if( strlen(parameter) < 30 ) // just to ensure...
      sprintf(full_parameter_str, "%s ... %s=", cmd, parameter);
    else
      strcpy(full_parameter_str, cmd);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "dev") == 0 ) {
      int value = parseSimpleValue(full_parameter_str, &value_str, 0, 127);
      if( value >= 0 )
	mbng_patch_cfg.sysex_dev = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "pat") == 0 ) {
      int value = parseSimpleValue(full_parameter_str, &value_str, 0, 127);
      if( value >= 0 )
	mbng_patch_cfg.sysex_pat = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "bnk") == 0 ) {
      int value = parseSimpleValue(full_parameter_str, &value_str, 0, 127);
      if( value >= 0 )
	mbng_patch_cfg.sysex_bnk = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "ins") == 0 ) {
      int value = parseSimpleValue(full_parameter_str, &value_str, 0, 127);
      if( value >= 0 )
	mbng_patch_cfg.sysex_ins = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "chn") == 0 ) {
      int value = parseSimpleValue(full_parameter_str, &value_str, 0, 127);
      if( value >= 0 )
	mbng_patch_cfg.sysex_chn = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s ... %s=%s\n", cmd, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses ENC definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseEnc(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  int sr = 0;
  int pin_a = 0;
  mios32_enc_type_t enc_type = DISABLED;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 
    const char *separator_colon = ":";

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > MIOS32_ENC_NUM_MAX ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid encoder number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, MIOS32_ENC_NUM_MAX-1);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr") == 0 ) {
      if( (sr=get_dec(value_str)) < 0 || sr > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "pins") == 0 ) {
      char *values_str = value_str;
      char *brkt_local;
      int values[2];
      if( !(values_str = strtok_r(value_str, separator_colon, &brkt_local)) ||
	  (values[0]=get_dec(values_str)) < 0 ||
	  !(values_str = strtok_r(NULL, separator_colon, &brkt_local)) ||
	  (values[1]=get_dec(values_str)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid pin format for %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      } else {
	if( values[0] >= 8 ||  (values[0] & 1) ||
	    values[1] >= 8 || !(values[1] & 1) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid pins for %s n=%d ... %s=%s (expecting 0:1, 2:3, 4:5 or 6:7)\n", cmd, num, parameter, value_str);
#endif
	  return -1; // invalid parameter
	} else {
	  pin_a = values[0];
	}
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "type") == 0 ) {
      if( strcasecmp(value_str, "disabled") == 0 ) {
	enc_type = DISABLED;
      } else if( strcasecmp(value_str, "non_detented") == 0 ) {
	enc_type = NON_DETENTED;
      } else if( strcasecmp(value_str, "detented1") == 0 ) {
	enc_type = DETENTED1;
      } else if( strcasecmp(value_str, "detented2") == 0 ) {
	enc_type = DETENTED2;
      } else if( strcasecmp(value_str, "detented3") == 0 ) {
	enc_type = DETENTED3;
      } else if( strcasecmp(value_str, "detented4") == 0 ) {
	enc_type = DETENTED4;
      } else if( strcasecmp(value_str, "detented5") == 0 ) {
	enc_type = DETENTED5;
      } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid type for %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    mios32_enc_config_t enc_config = {
      .cfg.type=enc_type,
      .cfg.speed=NORMAL,
      .cfg.speed_par=0,
      .cfg.sr=sr,
      .cfg.pos=pin_a
    };

    MIOS32_ENC_ConfigSet(num, enc_config); // counting from 1, because SCS menu encoder is assigned to 0
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses DIN_MATRIX definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseDinMatrix(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  int rows = 0;
  mbng_patch_matrix_inverted_t inverted; inverted.ALL = 0;
  int button_emu_id_offset = 0;
  int sr_dout_sel1 = 0;
  int sr_dout_sel2 = 0;
  int sr_din1 = 0;
  int sr_din2 = 0;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > MBNG_PATCH_NUM_MATRIX_DIN ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid DIN matrix number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, MBNG_PATCH_NUM_MATRIX_DIN);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "rows") == 0 ) {
      if( (rows=get_dec(value_str)) < 0 || (rows != 4 && rows != 8 && rows != 16) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid row number for %s n=%d ... %s=%s (only 4, 8 or 16 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "inverted") == 0 || strcasecmp(parameter, "inverted_sel") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid inverted value for %s n=%d ... %s=%s (only 0 or 1 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      inverted.sel = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "inverted_row") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid inverted value for %s n=%d ... %s=%s (only 0 or 1 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      inverted.row = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "button_emu_id_offset") == 0 ) {
      if( (button_emu_id_offset=get_dec(value_str)) < 0 || button_emu_id_offset >= 4095 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid ID offset for %s n=%d ... %s=%s (1..4095 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_sel1") == 0 ) {
      if( (sr_dout_sel1=get_dec(value_str)) < 0 || sr_dout_sel1 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_sel2") == 0 ) {
      if( (sr_dout_sel2=get_dec(value_str)) < 0 || sr_dout_sel2 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_din1") == 0 ) {
      if( (sr_din1=get_dec(value_str)) < 0 || sr_din1 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_din2") == 0 ) {
      if( (sr_din2=get_dec(value_str)) < 0 || sr_din2 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    mbng_patch_matrix_din_entry_t *m = (mbng_patch_matrix_din_entry_t *)&mbng_patch_matrix_din[num-1];
    m->num_rows = rows;
    m->inverted.ALL = inverted.ALL;
    m->button_emu_id_offset = button_emu_id_offset;
    m->sr_dout_sel1 = sr_dout_sel1;
    m->sr_dout_sel2 = sr_dout_sel2;
    m->sr_din1 = sr_din1;
    m->sr_din2 = sr_din2;
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses DOUT_MATRIX definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseDoutMatrix(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  int rows = 0;
  mbng_patch_matrix_inverted_t inverted; inverted.ALL = 0;
  int led_emu_id_offset = 0;
  int sr_dout_sel1 = 0;
  int sr_dout_sel2 = 0;
  int sr_dout_r1 = 0;
  int sr_dout_r2 = 0;
  int sr_dout_g1 = 0;
  int sr_dout_g2 = 0;
  int sr_dout_b1 = 0;
  int sr_dout_b2 = 0;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > MBNG_PATCH_NUM_MATRIX_DOUT ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid DOUT matrix number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, MBNG_PATCH_NUM_MATRIX_DOUT);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "rows") == 0 ) {
      if( (rows=get_dec(value_str)) < 0 || (rows != 4 && rows != 8 && rows != 16) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid row number for %s n=%d ... %s=%s (only 4, 8 or 16 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "inverted") == 0 || strcasecmp(parameter, "inverted_sel") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid inverted value for %s n=%d ... %s=%s (only 0 or 1 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      inverted.sel = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "inverted_row") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid inverted value for %s n=%d ... %s=%s (only 0 or 1 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      inverted.row = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "led_emu_id_offset") == 0 ) {
      if( (led_emu_id_offset=get_dec(value_str)) < 0 || led_emu_id_offset >= 4095 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid offset for %s n=%d ... %s=%s (1..4095 allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_sel1") == 0 ) {
      if( (sr_dout_sel1=get_dec(value_str)) < 0 || sr_dout_sel1 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_sel2") == 0 ) {
      if( (sr_dout_sel2=get_dec(value_str)) < 0 || sr_dout_sel2 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_r1") == 0 ) {
      if( (sr_dout_r1=get_dec(value_str)) < 0 || sr_dout_r1 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_r2") == 0 ) {
      if( (sr_dout_r2=get_dec(value_str)) < 0 || sr_dout_r2 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_g1") == 0 ) {
      if( (sr_dout_g1=get_dec(value_str)) < 0 || sr_dout_g1 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_g2") == 0 ) {
      if( (sr_dout_g2=get_dec(value_str)) < 0 || sr_dout_g2 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_b1") == 0 ) {
      if( (sr_dout_b1=get_dec(value_str)) < 0 || sr_dout_b1 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "sr_dout_b2") == 0 ) {
      if( (sr_dout_b2=get_dec(value_str)) < 0 || sr_dout_b2 > MIOS32_SRIO_NUM_SR ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid SR number for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, MIOS32_SRIO_NUM_SR);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    mbng_patch_matrix_dout_entry_t *m = (mbng_patch_matrix_dout_entry_t *)&mbng_patch_matrix_dout[num-1];
    m->num_rows = rows;
    m->inverted.ALL = inverted.ALL;
    m->led_emu_id_offset = led_emu_id_offset;
    m->sr_dout_sel1 = sr_dout_sel1;
    m->sr_dout_sel2 = sr_dout_sel2;
    m->sr_dout_r1 = sr_dout_r1;
    m->sr_dout_r2 = sr_dout_r2;
    m->sr_dout_g1 = sr_dout_g1;
    m->sr_dout_g2 = sr_dout_g2;
    m->sr_dout_b1 = sr_dout_b1;
    m->sr_dout_b2 = sr_dout_b2;
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses LED_MATRIX_PATTERN definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseLedMatrixPattern(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  int pos = 0;
  u16 pattern = 0x5555; // checkerboard to output anything by default

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > MBNG_PATCH_NUM_MATRIX_DOUT_PATTERNS ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid DOUT pattern number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, MBNG_PATCH_NUM_MATRIX_DOUT_PATTERNS);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "pos") == 0 ) {
      if( value_str[0] != 'M' && ((pos=get_dec(value_str)) < 0 || pos > 15) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid pos value for %s n=%d ... %s=%s (only 0..15 and M allowed)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      // modify pos ix:
      if( value_str[0] == 'M' )
	pos = 8;
      else if( pos >= 8 )
	pos += 1;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "pattern") == 0 ) {
      if( (pattern=get_bin(value_str, 16, 0)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid pattern for %s n=%d ... %s=%s (expecting 16 bits)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    MBNG_MATRIX_PatternSet(num-1, pos, pattern);

    if( pos == 7 ) // pre-set "middle" pattern for the case that it won't be defined
      MBNG_MATRIX_PatternSet(num-1, pos+1, pattern);
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses AIN definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseAin(char *cmd, char *brkt)
{
  // parse the parameters

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "enable_mask") == 0 ) {
      int value;
      if( (value=get_bin(value_str, 6, 0)) < 0 || value > 0x3f ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR: invalid enable mask in AIN ... %s=%s\n", parameter, value_str);
#endif
	return -1;
      } else {
	mbng_patch_ain.enable_mask = value;
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s ... %s=%s\n", cmd, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// help function which parses AINSER definitions
// returns >= 0 if command is valid
// returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseAinSer(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  int enabled = 1;
  int cs = 0;
  int resolution = 7;
  int num_pins = 64;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > MBNG_PATCH_NUM_AINSER_MODULES ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid AINSER module number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, MBNG_PATCH_NUM_MF_MODULES);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "enabled") == 0 ) {
      if( (enabled=get_dec(value_str)) < 0 || enabled > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid enabled value for %s n=%d ... %s=%s (0 or 1)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "cs") == 0 ) {
      if( (cs=get_dec(value_str)) < 0 || cs > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid CS line for %s n=%d ... %s=%s (0 or 1)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "resolution") == 0 ) {
      if( (resolution=get_dec(value_str)) < 1 || resolution > 12 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid resolution for %s n=%d ... %s=%s (4bit .. 12bit)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "num_pins") == 0 ) {
      if( (num_pins=get_dec(value_str)) < 1 || num_pins > AINSER_NUM_PINS ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid number of pins for %s n=%d ... %s=%s (1..%d)\n", cmd, num, parameter, value_str, AINSER_NUM_PINS);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    mbng_patch_ainser_entry_t *ainser = (mbng_patch_ainser_entry_t *)&mbng_patch_ainser[num-1];
    ainser->flags.cs = cs;
    AINSER_EnabledSet(cs, enabled);
    AINSER_NumPinsSet(cs, num_pins);

    //                        0bit 1bit 2bit 3bit 4bit 5bit 6bit 7bit 8bit 9bit 10   11   12
    const u8 deadband[13] = { 255, 255, 255, 255, 255, 127,  63,  31,  15,   7,   3,   1,  0 };
    AINSER_DeadbandSet(cs, deadband[resolution]);
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses MF definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseMf(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  int enabled = 0;
  int midi_in_port = 0x00;
  int midi_out_port = 0x00;
  int config_port = 0x00;
  int chn = 0;
  int ts_first_button_id = 0;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > MBNG_PATCH_NUM_MF_MODULES ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid MF module number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, MBNG_PATCH_NUM_MF_MODULES);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "enabled") == 0 ) {
      if( (enabled=get_dec(value_str)) < 0 || enabled > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid enabled value for %s n=%d ... %s=%s (0 or 1)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "midi_in_port") == 0 ) {
      if( (midi_in_port = parseMidiInPort(value_str)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid midi_in_port for %s n=%d ... %s=%s (0x00..0xff)\n", cmd, num, parameter, value_str);
#endif
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "midi_out_port") == 0 ) {
      if( (midi_out_port = parseMidiOutPort(value_str)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid midi_out_port for %s n=%d ... %s=%s (0x00..0xff)\n", cmd, num, parameter, value_str);
#endif
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "config_port") == 0 ) {
      if( (config_port = parseMidiInPort(value_str)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid config_port for %s n=%d ... %s=%s (0x00..0xff)\n", cmd, num, parameter, value_str);
#endif
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "chn") == 0 ) {
      if( (chn=get_dec(value_str)) < 1 || chn > 16 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid channel for %s n=%d ... %s=%s (1,,16)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }
      --chn; // user counts from 1

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "ts_first_button_id") == 0 ) {
      if( (ts_first_button_id=get_dec(value_str)) < 0 || ts_first_button_id > 4095 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid ts_first_button_id value for %s n=%d ... %s=%s (0..4095)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }
      if( ts_first_button_id )
	ts_first_button_id |= MBNG_EVENT_CONTROLLER_BUTTON;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    mbng_patch_mf_entry_t *mf = (mbng_patch_mf_entry_t *)&mbng_patch_mf[num-1];
    mf->midi_in_port = midi_in_port;
    mf->midi_out_port = midi_out_port;
    mf->config_port = config_port;
    mf->flags.enabled = enabled;
    mf->flags.chn = chn;
    mf->ts_first_button_id = ts_first_button_id;
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses AOUT definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseAout(char *cmd, char *brkt)
{
  // parse the parameters
  aout_if_t if_type = AOUT_IF_NONE;
  int num_channels = 8;
  u32 chn_hz_v = 0x00000000;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "type") == 0 ) {
      int i;
      for(i=0; i<AOUT_NUM_IF; ++i) {
	if( strcasecmp(value_str, AOUT_IfNameGet(i)) == 0 )
	  break;
      }

      if( i >= AOUT_NUM_IF ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid AOUT module type for %s ... %s=%s'\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      if_type = i;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "cs") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid chip select line for %s ... %s=%s (0 or 1)\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      mbng_patch_aout_spi_rc_pin = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "num_channels") == 0 ) {
      if( (num_channels=get_dec(value_str)) < 1 || num_channels > AOUT_NUM_CHANNELS ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid number of channels for %s ... %s=%s (1..%d)\n", cmd, parameter, value_str, AOUT_NUM_CHANNELS);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s ... %s=%s\n", cmd, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  // reconfigure AOUT module
  aout_config_t config;
  config = AOUT_ConfigGet();
  config.if_type = if_type;
  config.num_channels = num_channels;
  config.chn_hz_v = chn_hz_v;
  AOUT_ConfigSet(config);
  AOUT_IF_Init(0);  

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses SCS definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseScs(char *cmd, char *brkt)
{
  // parse the parameters

typedef struct {
  u16 button_emu_id[MBNG_PATCH_SCS_BUTTONS];
  u16 enc_emu_id;
} mbng_patch_scs_t;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    int button = -1;
    if(      strcasecmp(parameter, "soft1_button_emu_id") == 0 ) button = 0;
    else if( strcasecmp(parameter, "soft2_button_emu_id") == 0 ) button = 1;
    else if( strcasecmp(parameter, "soft3_button_emu_id") == 0 ) button = 2;
    else if( strcasecmp(parameter, "soft4_button_emu_id") == 0 ) button = 3;
    else if( strcasecmp(parameter, "shift_button_emu_id") == 0 ) button = 4;

    if( button >= 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 4095 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid button emulation id for %s ... %s=%s (0..4095)\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      mbng_patch_scs.button_emu_id[button] = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "enc_emu_id") == 0 ) {
      int value;
      if( (value=get_dec(value_str)) < 0 || value > 4095 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid enc emulation id for %s ... %s=%s (0..4095)\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      mbng_patch_scs.enc_emu_id = value;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s ... %s=%s\n", cmd, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
//! help function which parses ROUTER definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseRouter(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  int src_port = 0x10;
  int src_chn  = 0;
  int dst_port = 0x10;
  int dst_chn  = 0;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > MIDI_ROUTER_NUM_NODES ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid router node number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, MIDI_ROUTER_NUM_NODES);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "src_port") == 0 ) {
      if( (src_port = parseMidiInPort(value_str)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid source port for %s n=%d ... %s=%s (0x00..0xff)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "src_chn") == 0 ) {
      if( (src_chn=get_dec(value_str)) < 0 || src_chn > 17 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid source channel for %s n=%d ... %s=%s (0..17)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "dst_port") == 0 ) {
      if( (dst_port = parseMidiOutPort(value_str)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid source port for %s n=%d ... %s=%s (0x00..0xff)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "dst_chn") == 0 ) {
      if( (dst_chn=get_dec(value_str)) < 0 || dst_chn > 17 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid source channel for %s n=%d ... %s=%s (0..17)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    midi_router_node_entry_t *n = (midi_router_node_entry_t *)&midi_router_node[num-1];
    n->src_port = src_port;
    n->src_chn = src_chn;
    n->dst_port = dst_port;
    n->dst_chn = dst_chn;
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! \help function which parses ETH definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseEth(char *cmd, char *brkt)
{
#if defined(MIOS32_FAMILY_EMULATION)
  return -1;
#else
  // parse the parameters
  int dhcp = 1;
  u32 local_ip = MY_IP_ADDRESS;
  u32 netmask = MY_NETMASK;
  u32 gateway = MY_GATEWAY;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "dhcp") == 0 ) {
      if( (dhcp=get_dec(value_str)) < 0 || dhcp > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid value for %s ... %s=%s' (expecting 0 or 1)\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "local_ip") == 0 ) {
      if( (local_ip = get_ip(value_str)) == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid IP format for %s ... %s=%s\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "netmask") == 0 ) {
      if( (netmask = get_ip(value_str)) == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid IP format for %s ... %s=%s\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "gateway") == 0 ) {
      if( (gateway = get_ip(value_str)) == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid IP format for %s ... %s=%s\n", cmd, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s ... %s=%s\n", cmd, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  UIP_TASK_IP_AddressSet(local_ip);
  UIP_TASK_NetmaskSet(netmask);
  UIP_TASK_GatewaySet(gateway);
  UIP_TASK_DHCP_EnableSet(dhcp);

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses OSC definitions
//! \returns >= 0 if command is valid
//! \returns <0 if command is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_C_Read - this will blow up the stack usage too much!
s32 parseOsc(char *cmd, char *brkt)
{
  // parse the parameters
  int num = 0;
  u32 remote_ip = OSC_REMOTE_IP;
  int remote_port = OSC_REMOTE_PORT;
  int local_port = OSC_LOCAL_PORT;
  int transfer_mode = OSC_CLIENT_TRANSFER_MODE_MIDI;

  char *parameter;
  char *value_str;
  while( parseExtendedParameter(cmd, &parameter, &value_str, &brkt) >= 0 ) { 

    ////////////////////////////////////////////////////////////////////////////////////////////////
    if( strcasecmp(parameter, "n") == 0 ) {
      if( (num=get_dec(value_str)) < 1 || num > OSC_SERVER_NUM_CONNECTIONS ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid OSC port number for %s ... %s=%s' (1..%d)\n", cmd, parameter, value_str, OSC_SERVER_NUM_CONNECTIONS);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "remote_ip") == 0 ) {
      if( (remote_ip = get_ip(value_str)) == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid IP format for %s n=%d ... %s=%s (0x00..0xff)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "remote_port") == 0 ) {
      if( (remote_port=get_dec(value_str)) < 0 || remote_port > 65535 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid remote port for %s n=%d ... %s=%s (expect 0..65535)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "local_port") == 0 ) {
      if( (local_port=get_dec(value_str)) < 0 || local_port > 65535 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR invalid remote port for %s n=%d ... %s=%s (expect 0..65535)\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else if( strcasecmp(parameter, "transfer_mode") == 0 ) {
      int found_mode = -1;
      int i;
      for(i=0; i<OSC_CLIENT_NUM_TRANSFER_MODES; ++i) {
	char mode[10];
	strcpy(mode, OSC_CLIENT_TransferModeShortNameGet(i));

	// remove spaces and dots
	char *mode_ptr;
	for(mode_ptr=mode; *mode_ptr != 0 && *mode_ptr != ' ' && *mode_ptr != '.'; ++mode_ptr);
	*mode_ptr = 0;

	if( strcasecmp(value_str, mode) == 0 ) {
	  found_mode = i;
	  break;
	}
      }

      if( found_mode < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	DEBUG_MSG("[MBNG_FILE_C] ERROR unknown transfer mode %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
	return -1; // invalid parameter
      }

      transfer_mode = found_mode;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C] WARNING: unsupported parameter in %s n=%d ... %s=%s\n", cmd, num, parameter, value_str);
#endif
      // just continue to keep files compatible
    }
  }

  if( num >= 1 ) {
    OSC_SERVER_RemoteIP_Set(num-1, remote_ip);
    OSC_SERVER_RemotePortSet(num-1, remote_port);
    OSC_SERVER_LocalPortSet(num-1, local_port);
    OSC_CLIENT_TransferModeSet(num-1, transfer_mode);
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! reads the config file content (again)
//! \returns < 0 on errors (error codes are documented in mbng_file.h)
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_C_Read(char *filename)
{
  s32 status = 0;
  mbng_file_c_info_t *info = &mbng_file_c_info;
  file_t file;
  u8 got_first_event_item = 0;

  info->valid = 0; // will be set to valid if file content has been read successfully

  // store current file name in global variable for UI
  memcpy(mbng_file_c_config_name, filename, MBNG_FILE_C_FILENAME_LEN+1);

  char filepath[MAX_PATH];
  sprintf(filepath, "%s%s.NGC", MBNG_FILES_PATH, mbng_file_c_config_name);

#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MBNG_FILE_C] Open config '%s'\n", filepath);
#endif

  if( (status=FILE_ReadOpen(&file, filepath)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 2
    DEBUG_MSG("[MBNG_FILE_C] failed to open file, status: %d\n", status);
#endif
    return status;
  }

  // allocate 1024 bytes from heap
  u32 line_buffer_size = 1024;
  char *line_buffer = pvPortMalloc(line_buffer_size);
  u32 line_buffer_len = 0;
  if( !line_buffer ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] FATAL: out of heap memory!\n");
#endif
    return -1;
  }

  // read config values
  do {
    status=FILE_ReadLine((u8 *)(line_buffer+line_buffer_len), line_buffer_size-line_buffer_len);

    if( status > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 3
      if( line_buffer_len )
	MIOS32_MIDI_SendDebugString("+++");
      MIOS32_MIDI_SendDebugString(line_buffer);
#endif

      // concatenate?
      u32 new_len = strlen(line_buffer);
      // remove spaces
      while( new_len >= 1 && line_buffer[new_len-1] == ' ' ) {
	line_buffer[new_len-1] = 0;
	--new_len;
      }
      if( new_len >= 1 && line_buffer[new_len-1] == '\\' ) {
	line_buffer[new_len-1] = 0;
	line_buffer_len = new_len - 1;
	continue; // read next line
      } else {
	line_buffer_len = 0; // for next round we start at 0 again
      }

      // sscanf consumes too much memory, therefore we parse directly
      const char *separators = " \t;";
      char *brkt;
      char *parameter;

      if( (parameter = remove_quotes(strtok_r(line_buffer, separators, &brkt))) ) {
	
	if( *parameter == 0 || *parameter == '#' ) {
	  // ignore comments and empty lines
	} else if( strcmp(parameter, "RESET_HW") == 0 ) {
	  MBNG_EVENT_PoolClear();
	  MBNG_PATCH_Init(0);
	  MBNG_MATRIX_Init(0);
	  MBNG_ENC_Init(0);
	  MBNG_DOUT_Init(0);
	  MBNG_DIN_Init(0);
	  MBNG_AIN_Init(0);
	  MBNG_AINSER_Init(0);
	  MBNG_MF_Init(0);
	  MBNG_CV_Init(0);
	} else if( strcmp(parameter, "LCD") == 0 ) {
	  char *str = brkt;
	  if( !(str=remove_quotes(str)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_C] ERROR: missing string after LCD message!\n");
#endif
	  } else {
	    // print from a dummy item
	    mbng_event_item_t item;
	    MBNG_EVENT_ItemInit(&item, MBNG_EVENT_CONTROLLER_DISABLED);
	    item.label = str;
	    MBNG_LCD_PrintItemLabel(&item);
	  }
	} else if( strncmp(parameter, "EVENT_", 6) == 0 ) {
	  if( !got_first_event_item ) {
	    got_first_event_item = 1;
	    MBNG_EVENT_PoolClear();
	  }
	  parseEvent(parameter, brkt);
	} else if( strncmp(parameter, "MAP", 3) == 0 ) {
	  if( !got_first_event_item ) {
	    got_first_event_item = 1;
	    MBNG_EVENT_PoolClear();
	  }
	  parseMap(parameter, brkt);
	} else if( strcmp(parameter, "SYSEX_VAR") == 0 ) {
	  parseSysExVar(parameter, brkt);
	} else if( strcmp(parameter, "ENC") == 0 ) {
	  parseEnc(parameter, brkt);
	} else if( strcmp(parameter, "DIN_MATRIX") == 0 ) {
	  parseDinMatrix(parameter, brkt);
	} else if( strcmp(parameter, "DOUT_MATRIX") == 0 ) {
	  parseDoutMatrix(parameter, brkt);
	} else if( strcmp(parameter, "LED_MATRIX_PATTERN") == 0 ) {
	  parseLedMatrixPattern(parameter, brkt);
	} else if( strcmp(parameter, "AIN") == 0 ) {
	  parseAin(parameter, brkt);
	} else if( strcmp(parameter, "AINSER") == 0 ) {
	  parseAinSer(parameter, brkt);
	} else if( strcmp(parameter, "MF") == 0 ) {
	  parseMf(parameter, brkt);
	} else if( strcmp(parameter, "AOUT") == 0 ) {
	  parseAout(parameter, brkt);
	} else if( strcmp(parameter, "SCS") == 0 ) {
	  parseScs(parameter, brkt);
	} else if( strcmp(parameter, "ROUTER") == 0 ) {
	  parseRouter(parameter, brkt);
	} else if( strcmp(parameter, "ETH") == 0 ) {
	  parseEth(parameter, brkt);
	} else if( strcmp(parameter, "OSC") == 0 ) {
	  parseOsc(parameter, brkt);

	} else if( strcmp(parameter, "DebounceCtr") == 0 ) {
	  int value = parseSimpleValue(parameter, &brkt, 0, 255);
	  if( value >= 0 )
	    mbng_patch_cfg.debounce_ctr = value;
	} else if( strcmp(parameter, "GlobalChannel") == 0 ) {
	  int value = parseSimpleValue(parameter, &brkt, 0, 16);
	  if( value >= 0 )
	    mbng_patch_cfg.global_chn = value;
	} else if( strcmp(parameter, "AllNotesOffChannel") == 0 ) {
	  int value = parseSimpleValue(parameter, &brkt, 0, 16);
	  if( value >= 0 )
	    mbng_patch_cfg.all_notes_off_chn = value;
	} else if( strcmp(parameter, "ConvertNoteOffToOn0") == 0 ) {
	  int value = parseSimpleValue(parameter, &brkt, 0, 16);
	  if( value >= 0 )
	    mbng_patch_cfg.convert_note_off_to_on0 = value;
	} else if( strcmp(parameter, "BPM_Preset") == 0 ) {
	  int value = parseSimpleValue(parameter, &brkt, 1, 1000);
	  if( value >= 0 )
	    SEQ_BPM_Set((float)value);
	} else if( strcmp(parameter, "BPM_Mode") == 0 ) {
	  int value = parseSimpleValue(parameter, &brkt, 0, 2);
	  if( value >= 0 )
	    SEQ_BPM_ModeSet(value);
	} else if( strcmp(parameter, "MidiFileClkOutPorts") == 0 ) {
	  s32 enabled_ports = 0;
	  int bit;
	  for(bit=0; bit<16; ++bit) {
	    char *word = remove_quotes(strtok_r(NULL, separators, &brkt));
	    int enable = get_dec(word);
	    if( enable < 0 )
	      break;
	    if( enable >= 1 )
	      enabled_ports |= (1 << bit);
	  }

	  if( bit != 16 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_C] ERROR invalid MIDI port format for parameter '%s'\n", parameter);
#endif
	  } else {
	    MIDI_ROUTER_MIDIClockOutSet(USB0, (enabled_ports & 0x0001) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(USB1, (enabled_ports & 0x0002) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(USB2, (enabled_ports & 0x0004) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(USB3, (enabled_ports & 0x0008) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(UART0, (enabled_ports & 0x0010) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(UART1, (enabled_ports & 0x0020) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(UART2, (enabled_ports & 0x0040) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(UART3, (enabled_ports & 0x0080) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(IIC0, (enabled_ports & 0x0100) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(IIC1, (enabled_ports & 0x0200) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(IIC2, (enabled_ports & 0x0400) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(IIC3, (enabled_ports & 0x0800) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(OSC0, (enabled_ports & 0x1000) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(OSC1, (enabled_ports & 0x2000) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(OSC2, (enabled_ports & 0x4000) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockOutSet(OSC3, (enabled_ports & 0x8000) ? 1 : 0);
	  }	  

	} else if( strcmp(parameter, "MidiFileClkInPorts") == 0 ) {
	  s32 enabled_ports = 0;
	  int bit;
	  for(bit=0; bit<16; ++bit) {
	    char *word = remove_quotes(strtok_r(NULL, separators, &brkt));
	    int enable = get_dec(word);
	    if( enable < 0 )
	      break;
	    if( enable >= 1 )
	      enabled_ports |= (1 << bit);
	  }

	  if( bit != 16 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_C] ERROR invalid MIDI port format for parameter '%s'\n", parameter);
#endif
	  } else {
	    MIDI_ROUTER_MIDIClockInSet(USB0, (enabled_ports & 0x0001) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(USB1, (enabled_ports & 0x0002) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(USB2, (enabled_ports & 0x0004) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(USB3, (enabled_ports & 0x0008) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(UART0, (enabled_ports & 0x0010) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(UART1, (enabled_ports & 0x0020) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(UART2, (enabled_ports & 0x0040) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(UART3, (enabled_ports & 0x0080) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(IIC0, (enabled_ports & 0x0100) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(IIC1, (enabled_ports & 0x0200) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(IIC2, (enabled_ports & 0x0400) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(IIC3, (enabled_ports & 0x0800) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(OSC0, (enabled_ports & 0x1000) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(OSC1, (enabled_ports & 0x2000) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(OSC2, (enabled_ports & 0x4000) ? 1 : 0);
	    MIDI_ROUTER_MIDIClockInSet(OSC3, (enabled_ports & 0x8000) ? 1 : 0);
	  }

	} else {
#if DEBUG_VERBOSE_LEVEL >= 1
	  // changed error to warning, since people are sometimes confused about these messages
	  // on file format changes
	  DEBUG_MSG("[MBNG_FILE_C] WARNING: unknown parameter: %s", line_buffer);
#endif
	}
      } else {
#if DEBUG_VERBOSE_LEVEL >= 2
	// no real error, can for example happen in .csv file
	DEBUG_MSG("[MBNG_FILE_C] ERROR no space or semicolon separator in following line: %s", line_buffer);
#endif
      }
    }

  } while( status >= 1 );

  // release memory from heap
  vPortFree(line_buffer);

  // close file
  status |= FILE_ReadClose(&file);

#if !defined(MIOS32_FAMILY_EMULATION)
  // OSC_SERVER_Init(0) has to be called after all settings have been done!
  OSC_SERVER_Init(0);
#endif

  if( status < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] ERROR while reading file, status: %d\n", status);
#endif
    return MBNG_FILE_C_ERR_READ;
  }

#if DEBUG_VERBOSE_LEVEL >= 1
  if( got_first_event_item ) {
    // post-processing step
    MBNG_EVENT_PoolUpdate();

    DEBUG_MSG("[MBNG_FILE_C] Event Pool Number of Items: %d", MBNG_EVENT_PoolNumItemsGet());
    u32 pool_size = MBNG_EVENT_PoolSizeGet();
    u32 pool_max_size = MBNG_EVENT_PoolMaxSizeGet();
    DEBUG_MSG("[MBNG_FILE_C] Event Pool Allocation: %d of %d bytes (%d%%)",
	      pool_size, pool_max_size, (100*pool_size)/pool_max_size);
  }
#endif

  // file is valid! :)
  info->valid = 1;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! help function to write data into file or send to debug terminal
//! \returns < 0 on errors (error codes are documented in seq_file.h)
/////////////////////////////////////////////////////////////////////////////
static s32 MBNG_FILE_C_Write_Hlp(u8 write_to_file)
{
  s32 status = 0;
  char line_buffer[128];

#define FLUSH_BUFFER if( !write_to_file ) { DEBUG_MSG(line_buffer); } else { status |= FILE_WriteBuffer((u8 *)line_buffer, strlen(line_buffer)); }

  {
    sprintf(line_buffer, "# Reset to default\n");
    FLUSH_BUFFER;
    sprintf(line_buffer, "RESET_HW\n");
    FLUSH_BUFFER;
  }

  {
    sprintf(line_buffer, "\n\n# LCD message after load\n");
    FLUSH_BUFFER;
    sprintf(line_buffer, "LCD \"%%C@(1:1:1)READY.\"\n");
    FLUSH_BUFFER;
  }

  if( MBNG_EVENT_PoolNumItemsGet() > 0 ) {
    sprintf(line_buffer, "\n\n# EVENTs\n");
    FLUSH_BUFFER;

    int num_items = MBNG_EVENT_PoolNumItemsGet();
    int item_ix;
    for(item_ix=0; item_ix<num_items; ++item_ix) {
      mbng_event_item_t item;
      if( MBNG_EVENT_ItemGet(item_ix, &item) < 0 )
	continue;

      sprintf(line_buffer, "EVENT_%-6s id=%3d",
	      MBNG_EVENT_ItemControllerStrGet(item.id),
	      item.id & 0xfff);
      FLUSH_BUFFER;

      if( item.hw_id != item.id ) {
	sprintf(line_buffer, "  hw_id=%3d",
		item.hw_id & 0xfff);
	FLUSH_BUFFER;
      }

      if( item.bank ) {
	sprintf(line_buffer, "  bank=%2d",
		item.bank);
	FLUSH_BUFFER;
      }

      if( item.fwd_id ) {
	sprintf(line_buffer, "  fwd_id=%s:%-3d", MBNG_EVENT_ItemControllerStrGet(item.fwd_id), item.fwd_id & 0xfff);
	FLUSH_BUFFER;
      }

      if( item.flags.general.fwd_to_lcd ) {
	sprintf(line_buffer, "  fwd_to_lcd=1");
	FLUSH_BUFFER;
      }

      sprintf(line_buffer, "  type=%-6s",
	      MBNG_EVENT_ItemTypeStrGet(&item));
      FLUSH_BUFFER;

      switch( item.flags.general.type ) {
      case MBNG_EVENT_TYPE_NOTE_OFF:
      case MBNG_EVENT_TYPE_NOTE_ON:
      case MBNG_EVENT_TYPE_POLY_PRESSURE: {
	if( item.stream_size >= 2 ) {
	  if( item.stream[1] < 128 ) {
	    sprintf(line_buffer, " chn=%2d key=%3d", (item.stream[0] & 0xf)+1, item.stream[1]);
	  } else {
	    sprintf(line_buffer, " chn=%2d key=any", (item.stream[0] & 0xf)+1);
	  }
	  FLUSH_BUFFER;

	  if( item.flags.general.use_key_or_cc ) {
	    sprintf(line_buffer, " use_key_number=1 ");
	    FLUSH_BUFFER;
	  }
	}
      } break;

      case MBNG_EVENT_TYPE_CC: {
	if( item.stream_size >= 2 ) {
	  if( item.stream[1] < 128 ) {
	    sprintf(line_buffer, " chn=%2d cc=%3d ", (item.stream[0] & 0xf)+1, item.stream[1]);
	  } else {
	    sprintf(line_buffer, " chn=%2d cc=any ", (item.stream[0] & 0xf)+1);
	  }
	  FLUSH_BUFFER;

	  if( item.flags.general.use_key_or_cc ) {
	    sprintf(line_buffer, " use_cc_number=1 ");
	    FLUSH_BUFFER;
	  }
	}
      } break;

      case MBNG_EVENT_TYPE_PROGRAM_CHANGE:
      case MBNG_EVENT_TYPE_AFTERTOUCH:
      case MBNG_EVENT_TYPE_PITCHBEND: {
	if( item.stream_size >= 1 ) {
	  sprintf(line_buffer, " chn=%2d", (item.stream[0] & 0xf)+1);
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_TYPE_SYSEX: {
	if( item.stream_size ) {
	  sprintf(line_buffer, " stream=\"");
	  FLUSH_BUFFER;

	  int pos;
	  for(pos=0; pos<item.stream_size; ++pos) {
	    if( item.stream[pos] == 0xff ) { // meta indicator
	      char *var_str = (char *)MBNG_EVENT_ItemSysExVarStrGet(&item, pos+1);
	      if( strcasecmp(var_str, "undef") == 0 ) {
		sprintf(line_buffer, "%s0xff 0x%02x", (pos == 0) ? "" : " ", item.stream[pos+1]);
	      } else {
		sprintf(line_buffer, "%s^%s", (pos == 0) ? "" : " ", var_str);
	      }
	      ++pos;
	    } else {
	      sprintf(line_buffer, "%s0x%02x", (pos == 0) ? "" : " ", item.stream[pos]);
	    }
	    FLUSH_BUFFER;
	  }
	  sprintf(line_buffer, "\"");
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_TYPE_NRPN: {
	if( item.stream_size >= 3 ) {
	  sprintf(line_buffer, " chn=%2d nrpn=%d nrpn_format=%s",
		  (item.stream[0] & 0xf)+1,
		  item.stream[1] | (int)(item.stream[2] << 7),
		  MBNG_EVENT_ItemNrpnFormatStrGet(&item));
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_TYPE_META: {
	int i;
	for(i=0; i<item.stream_size/2; ++i) {
	  sprintf(line_buffer, " meta=%s:%d", MBNG_EVENT_ItemMetaTypeStrGet(&item, i), (int)item.stream[2*i+1]);
	  FLUSH_BUFFER;
	}
      } break;
      }

      if( item.map ) {
	sprintf(line_buffer, "  range=map%-3d ", item.map);
	FLUSH_BUFFER;
      } else {
	sprintf(line_buffer, "  range=%3d:%-3d", item.min, item.max);
	FLUSH_BUFFER;
      }

      {
	char ports_bin[17];
	int bit;
	for(bit=0; bit<16; ++bit) {
	  ports_bin[bit] = (item.enabled_ports & (1 << bit)) ? '1' : '0';
	}
	ports_bin[16] = 0;

	sprintf(line_buffer, "  offset=%3d  ports=%s", item.offset, ports_bin);
	FLUSH_BUFFER;
      }

      if( item.syxdump_pos.receiver ) {
	sprintf(line_buffer, "  syxdump_pos=%d:%d", item.syxdump_pos.receiver, item.syxdump_pos.pos);
	FLUSH_BUFFER;
      }

      // differ between event type
      switch( item.id & 0xf000 ) {
      case MBNG_EVENT_CONTROLLER_SENDER: {
	if( item.flags.SENDER.radio_group ) {
	  sprintf(line_buffer, "  radio_group=%d", item.flags.SENDER.radio_group);
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_CONTROLLER_RECEIVER: {
	if( item.flags.RECEIVER.radio_group ) {
	  sprintf(line_buffer, "  radio_group=%d", item.flags.RECEIVER.radio_group);
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_CONTROLLER_BUTTON: {
	if( item.flags.DIN.button_mode != MBNG_EVENT_BUTTON_MODE_ON_OFF && item.flags.DIN.button_mode != MBNG_EVENT_BUTTON_MODE_UNDEFINED ) {
	  sprintf(line_buffer, "  button_mode=%s", MBNG_EVENT_ItemButtonModeStrGet(&item));
	  FLUSH_BUFFER;
	}

	if( item.flags.DIN.radio_group ) {
	  sprintf(line_buffer, "  radio_group=%d", item.flags.DIN.radio_group);
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_CONTROLLER_LED: {
	if( item.flags.DOUT.radio_group ) {
	  sprintf(line_buffer, "  radio_group=%d", item.flags.DOUT.radio_group);
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_CONTROLLER_BUTTON_MATRIX: {
      } break;

      case MBNG_EVENT_CONTROLLER_LED_MATRIX: {
      } break;

      case MBNG_EVENT_CONTROLLER_ENC: {
	if( item.flags.ENC.enc_mode != MBNG_EVENT_ENC_MODE_ABSOLUTE && item.flags.ENC.enc_mode != MBNG_EVENT_ENC_MODE_UNDEFINED ) {
	  sprintf(line_buffer, "  enc_mode=%s", MBNG_EVENT_ItemEncModeStrGet(&item));
	  FLUSH_BUFFER;
	}

	if( item.flags.ENC.enc_speed_mode != MBNG_EVENT_ENC_SPEED_MODE_AUTO && item.flags.ENC.enc_speed_mode != MBNG_EVENT_ENC_SPEED_MODE_UNDEFINED ) {
	  sprintf(line_buffer, "  enc_speed_mode=%s:%d", MBNG_EVENT_ItemEncSpeedModeStrGet(&item), item.flags.ENC.enc_speed_mode_par);
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_CONTROLLER_AIN: {
	if( item.flags.AIN.ain_mode != MBNG_EVENT_AIN_MODE_DIRECT && item.flags.AIN.ain_mode != MBNG_EVENT_AIN_MODE_UNDEFINED ) {
	  sprintf(line_buffer, "  ain_mode=%s", MBNG_EVENT_ItemAinModeStrGet(&item));
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_CONTROLLER_AINSER: {
	if( item.flags.AINSER.ain_mode != MBNG_EVENT_AIN_MODE_DIRECT && item.flags.AINSER.ain_mode != MBNG_EVENT_AIN_MODE_UNDEFINED ) {
	  sprintf(line_buffer, "  ain_mode=%s", MBNG_EVENT_ItemAinModeStrGet(&item));
	  FLUSH_BUFFER;
	}
      } break;

      case MBNG_EVENT_CONTROLLER_MF: {
      } break;

      case MBNG_EVENT_CONTROLLER_CV: {
	if( item.flags.CV.fwd_gate_to_dout_pin ) {
	  sprintf(line_buffer, "  fwd_gate_to_dout_pin=%d.D%d",
		  ((item.flags.CV.fwd_gate_to_dout_pin-1) / 8) + 1,
		  7 - ((item.flags.CV.fwd_gate_to_dout_pin-1) % 8));
	  FLUSH_BUFFER;
	}

	if( item.flags.CV.cv_inverted ) {
	  sprintf(line_buffer, "  cv_inverted=%d", item.flags.CV.cv_inverted);
	  FLUSH_BUFFER;
	}

	if( item.flags.CV.cv_gate_inverted ) {
	  sprintf(line_buffer, "  cv_gate_inverted=%d", item.flags.CV.cv_gate_inverted);
	  FLUSH_BUFFER;
	}

	if( item.flags.CV.cv_hz_v ) {
	  sprintf(line_buffer, "  cv_hz_v=%d", item.flags.CV.cv_hz_v);
	  FLUSH_BUFFER;
	}
      } break;
      }

      if( item.flags.ENC.led_matrix_pattern != MBNG_EVENT_LED_MATRIX_PATTERN_1 &&
	  item.flags.ENC.led_matrix_pattern != MBNG_EVENT_LED_MATRIX_PATTERN_UNDEFINED ) {
	sprintf(line_buffer, "  led_matrix_pattern=%s", MBNG_EVENT_ItemLedMatrixPatternStrGet(&item));
	FLUSH_BUFFER;
      }

      sprintf(line_buffer, "  lcd_pos=%d:%d:%d", item.lcd+1, (item.lcd_pos%64)+1, (item.lcd_pos/64)+1);
      FLUSH_BUFFER;

      if( item.label && strlen(item.label) ) {
	sprintf(line_buffer, "  label=\"%s\"", item.label);
	FLUSH_BUFFER;
      }

      sprintf(line_buffer, "\n");
      FLUSH_BUFFER;
    }
  }

  if( MBNG_EVENT_PoolNumMapsGet() > 0 ) {
    sprintf(line_buffer, "\n\n# MAPs\n");
    FLUSH_BUFFER;

    int num_maps = MBNG_EVENT_PoolNumMapsGet();
    int map_ix;
    for(map_ix=1; map_ix<=num_maps; ++map_ix) {
      u8 *map_values;
      int map_len = MBNG_EVENT_MapGet(map_ix, &map_values);
      if( map_len > 0 ) {
	sprintf(line_buffer, "MAP%d", map_ix);
	FLUSH_BUFFER;

        int i;
	for(i=0; i<map_len; ++i) {
	  sprintf(line_buffer, " %d", map_values[i]);
	  FLUSH_BUFFER;
	}
	sprintf(line_buffer, "\n");
	FLUSH_BUFFER;
      }
    }
  }

  {
    sprintf(line_buffer, "\n\n# SysEx variables\n");
    FLUSH_BUFFER;

    sprintf(line_buffer, "SYSEX_VAR dev=%d pat=%d bnk=%d ins=%d chn=%d\n",
	    mbng_patch_cfg.sysex_dev,
	    mbng_patch_cfg.sysex_pat,
	    mbng_patch_cfg.sysex_bnk,
	    mbng_patch_cfg.sysex_ins,
	    mbng_patch_cfg.sysex_chn);
    FLUSH_BUFFER;
  }

  {
    sprintf(line_buffer, "\n\n# ENC hardware\n");
    FLUSH_BUFFER;

    int enc;
    for(enc=1; enc<MIOS32_ENC_NUM_MAX; ++enc) {
      mios32_enc_config_t enc_config = MIOS32_ENC_ConfigGet(enc);

      char enc_type[20];
      switch( enc_config.cfg.type ) { // see mios32_enc.h for available types
      case 0xff: strcpy(enc_type, "non_detented"); break;
      case 0xaa: strcpy(enc_type, "detented1"); break;
      case 0x22: strcpy(enc_type, "detented2"); break;
      case 0x88: strcpy(enc_type, "detented3"); break;
      case 0xa5: strcpy(enc_type, "detented4"); break;
      case 0x5a: strcpy(enc_type, "detented5"); break;
      default: enc_type[0] = 0;
      }

      if( enc_type[0] && enc_config.cfg.sr ) {
	sprintf(line_buffer, "ENC n=%3d   sr=%2d  pins=%d:%d   type=%s\n",
		enc,
		enc_config.cfg.sr,
		enc_config.cfg.pos,
		enc_config.cfg.pos+1,
		enc_type);
	FLUSH_BUFFER;
      }
    }
  }

  {
    sprintf(line_buffer, "\n\n# DIN_MATRIX hardware\n");
    FLUSH_BUFFER;

    int matrix;
    mbng_patch_matrix_din_entry_t *m = (mbng_patch_matrix_din_entry_t *)&mbng_patch_matrix_din[0];
    for(matrix=0; matrix<MBNG_PATCH_NUM_MATRIX_DIN; ++matrix, ++m) {

      sprintf(line_buffer, "DIN_MATRIX n=%2d   rows=%d  inverted_sel=%d  inverted_row=%d  sr_dout_sel1=%2d sr_dout_sel2=%2d  sr_din1=%2d sr_din2=%2d",
	      matrix+1,
	      m->num_rows,
	      m->inverted.sel,
	      m->inverted.row,
	      m->sr_dout_sel1,
	      m->sr_dout_sel2,
	      m->sr_din1,
	      m->sr_din2);
      FLUSH_BUFFER;

      if( m->button_emu_id_offset ) {
	sprintf(line_buffer, "  button_emu_id_offset=%d", m->button_emu_id_offset);
	FLUSH_BUFFER;
      }

      sprintf(line_buffer, "\n");
      FLUSH_BUFFER;
    }
  }

  {
    sprintf(line_buffer, "\n\n# DOUT_MATRIX hardware\n");
    FLUSH_BUFFER;

    int matrix;
    mbng_patch_matrix_dout_entry_t *m = (mbng_patch_matrix_dout_entry_t *)&mbng_patch_matrix_dout[0];
    for(matrix=0; matrix<MBNG_PATCH_NUM_MATRIX_DOUT; ++matrix, ++m) {

      sprintf(line_buffer, "DOUT_MATRIX n=%2d   rows=%d  inverted_sel=%d  inverted_row=%d  sr_dout_sel1=%2d sr_dout_sel2=%2d  sr_dout_r1=%2d sr_dout_r2=%2d  sr_dout_g1=%2d sr_dout_g2=%2d  sr_dout_b1=%2d sr_dout_b2=%2d",
	      matrix+1,
	      m->num_rows,
	      m->inverted.sel,
	      m->inverted.row,
	      m->sr_dout_sel1,
	      m->sr_dout_sel2,
	      m->sr_dout_r1,
	      m->sr_dout_r2,
	      m->sr_dout_g1,
	      m->sr_dout_g2,
	      m->sr_dout_b1,
	      m->sr_dout_b2);
      FLUSH_BUFFER;

      if( m->led_emu_id_offset ) {
	sprintf(line_buffer, "  led_emu_id_offset=%d", m->led_emu_id_offset);
	FLUSH_BUFFER;
      }

      sprintf(line_buffer, "\n");
      FLUSH_BUFFER;
    }
  }

  {
    sprintf(line_buffer, "\n\n# LED_MATRIX_PATTERNs\n");
    FLUSH_BUFFER;

    int n, pos;
    for(n=0; n<MBNG_PATCH_NUM_MATRIX_DOUT_PATTERNS; ++n) {
      for(pos=0; pos<MBNG_MATRIX_DOUT_NUM_PATTERN_POS; ++pos) {
	sprintf(line_buffer, "LED_MATRIX_PATTERN n=%2d", n+1);
	FLUSH_BUFFER;

	if( pos == ((MBNG_MATRIX_DOUT_NUM_PATTERN_POS-1)/2) )
	  sprintf(line_buffer, "  pos= M");
	else
	  sprintf(line_buffer, "  pos=%2d", (pos < ((MBNG_MATRIX_DOUT_NUM_PATTERN_POS-1)/2)) ? pos : (pos-1));
	FLUSH_BUFFER;

	{
	  u16 pattern = MBNG_MATRIX_PatternGet(n, pos);
	  char pattern_bin[17];
	  int bit;
	  for(bit=0; bit<16; ++bit) {
	    pattern_bin[bit] = (pattern & (1 << bit)) ? '1' : '0';
	  }
	  pattern_bin[16] = 0;

	  sprintf(line_buffer, "  pattern=%s\n", pattern_bin);
	  FLUSH_BUFFER;
	}
      }

      sprintf(line_buffer, "\n");
      FLUSH_BUFFER;
    }
  }

  {
    sprintf(line_buffer, "\n\n# AIN hardware\n");
    FLUSH_BUFFER;

    char enable_bin[7];
    int bit;
    for(bit=0; bit<6; ++bit) {
      enable_bin[bit] = (mbng_patch_ain.enable_mask & (1 << bit)) ? '1' : '0';
    }
    enable_bin[6] = 0;

    sprintf(line_buffer, "AIN enable_mask=%s\n", enable_bin);
    FLUSH_BUFFER;
  }

  {
    sprintf(line_buffer, "\n\n# AINSER hardware\n");
    FLUSH_BUFFER;

    int module;
    mbng_patch_ainser_entry_t *ainser = (mbng_patch_ainser_entry_t *)&mbng_patch_ainser[0];
    for(module=0; module<MBNG_PATCH_NUM_AINSER_MODULES; ++module, ++ainser) {
      int resolution = 7;
      u8 deadband = AINSER_DeadbandGet(ainser->flags.cs);
      if( deadband <=   0 )      resolution = 12;
      else if( deadband <=   1 ) resolution = 11;
      else if( deadband <=   3 ) resolution = 10;
      else if( deadband <=   7 ) resolution =  9;
      else if( deadband <=  15 ) resolution =  8;
      else if( deadband <=  31 ) resolution =  7;
      else if( deadband <=  63 ) resolution =  6;
      else if( deadband <= 127 ) resolution =  5;
      else                       resolution =  4;

      sprintf(line_buffer, "AINSER n=%3d   enabled=%d  cs=%d  num_pins=%d  resolution=%dbit\n",
	      module+1,
	      AINSER_EnabledGet(ainser->flags.cs),
	      ainser->flags.cs,
	      AINSER_NumPinsGet(ainser->flags.cs),
	      resolution);
      FLUSH_BUFFER;
    }

  }

  {
    sprintf(line_buffer, "\n\n# MF hardware (has to be configured for Motormix protocol!)\n");
    FLUSH_BUFFER;

    int module;
    mbng_patch_mf_entry_t *mf = (mbng_patch_mf_entry_t *)&mbng_patch_mf[0];
    for(module=0; module<MBNG_PATCH_NUM_MF_MODULES; ++module, ++mf) {
      u8 ix;
      char midi_in_port_str[10];
      if( (ix=MIDI_PORT_InIxGet(mf->midi_in_port)) > 0 ) {
	strcpy(midi_in_port_str, MIDI_PORT_InNameGet(ix));
      } else {
	sprintf(midi_in_port_str, "0x%02x", mf->midi_in_port);
      }

      char midi_out_port_str[10];
      if( (ix=MIDI_PORT_OutIxGet(mf->midi_out_port)) > 0 ) {
	strcpy(midi_out_port_str, MIDI_PORT_OutNameGet(ix));
      } else {
	sprintf(midi_out_port_str, "0x%02x", mf->midi_out_port);
      }

      char config_port_str[10];
      if( (ix=MIDI_PORT_InIxGet(mf->config_port)) > 0 ) {
	strcpy(config_port_str, MIDI_PORT_InNameGet(ix));
      } else {
	sprintf(config_port_str, "0x%02x", mf->config_port);
      }

      sprintf(line_buffer, "MF n=%3d   enabled=%d  midi_in_port=%s  midi_out_port=%s  chn=%d  ts_first_button_id=%d  config_port=%s\n",
	      module+1,
	      mf->flags.enabled,
	      midi_in_port_str,
	      midi_out_port_str,
	      mf->flags.chn+1,
	      mf->ts_first_button_id & 0xfff,
	      config_port_str);
      FLUSH_BUFFER;
    }
  }

  {
    sprintf(line_buffer, "\n\n# AOUT hardware\n");
    FLUSH_BUFFER;

    aout_config_t config;
    config = AOUT_ConfigGet();

    sprintf(line_buffer, "AOUT  type=%s  cs=%d  num_channels=%d\n",
	    AOUT_IfNameGet(config.if_type),
	    mbng_patch_aout_spi_rc_pin,
	    config.num_channels);
    FLUSH_BUFFER;
  }

  {
    u16 any_value = 0;

    int i;
    for(i=0; i<MBNG_PATCH_SCS_BUTTONS; ++i)
      any_value |= mbng_patch_scs.button_emu_id[i];
    any_value |= mbng_patch_scs.enc_emu_id;

    if( any_value ) {
      sprintf(line_buffer, "\n\n# SCS Configuration\n");
      FLUSH_BUFFER;

#if MBNG_PATCH_SCS_BUTTONS != 5
# error "Please adapt for new number of buttons"
#endif
      sprintf(line_buffer, "SCS soft1_button_emu_id=%d \\\n", mbng_patch_scs.button_emu_id[0]);
      FLUSH_BUFFER;
      sprintf(line_buffer, "    soft2_button_emu_id=%d \\\n", mbng_patch_scs.button_emu_id[1]);
      FLUSH_BUFFER;
      sprintf(line_buffer, "    soft3_button_emu_id=%d \\\n", mbng_patch_scs.button_emu_id[2]);
      FLUSH_BUFFER;
      sprintf(line_buffer, "    soft4_button_emu_id=%d \\\n", mbng_patch_scs.button_emu_id[3]);
      FLUSH_BUFFER;
      sprintf(line_buffer, "    shift_button_emu_id=%d \\\n", mbng_patch_scs.button_emu_id[4]);
      FLUSH_BUFFER;
      sprintf(line_buffer, "    enc_emu_id=%d\n", mbng_patch_scs.enc_emu_id);
      FLUSH_BUFFER;
    }
  }

  {
    sprintf(line_buffer, "\n\n# ROUTER definitions (Note: chn=0 disables, chn=17 selects all channels)\n");
    FLUSH_BUFFER;

    int node;
    midi_router_node_entry_t *n = (midi_router_node_entry_t *)&midi_router_node[0];
    for(node=0; node<MIDI_ROUTER_NUM_NODES; ++node, ++n) {
      char src_port_str[10];
      u8 src_ix;
      if( (src_ix=MIDI_PORT_InIxGet(n->src_port)) > 0 ) {
	strcpy(src_port_str, MIDI_PORT_InNameGet(src_ix));
      } else {
	sprintf(src_port_str, "0x%02x", n->src_port);
      }

      char dst_port_str[10];
      u8 dst_ix;
      if( (dst_ix=MIDI_PORT_OutIxGet(n->dst_port)) > 0 ) {
	strcpy(dst_port_str, MIDI_PORT_OutNameGet(dst_ix));
      } else {
	sprintf(dst_port_str, "0x%02x", n->dst_port);
      }

      sprintf(line_buffer, "ROUTER n=%2d   src_port=%s  src_chn=%2d   dst_port=%s  dst_chn=%2d\n",
	      node+1,
	      src_port_str,
	      n->src_chn,
	      dst_port_str,
	      n->dst_chn);
      FLUSH_BUFFER;
    }
  }

#if !defined(MIOS32_FAMILY_EMULATION)
  sprintf(line_buffer, "\n\n# Ethernet Setup\n");
  FLUSH_BUFFER;
  {
    sprintf(line_buffer, "ETH dhcp=%d", UIP_TASK_DHCP_EnableGet());
    FLUSH_BUFFER;

    {
      u32 value = UIP_TASK_IP_AddressGet();
      sprintf(line_buffer, "  local_ip=%d.%d.%d.%d",
	      (value >> 24) & 0xff,
	      (value >> 16) & 0xff,
	      (value >>  8) & 0xff,
	      (value >>  0) & 0xff);
      FLUSH_BUFFER;
    }

    {
      u32 value = UIP_TASK_NetmaskGet();
      sprintf(line_buffer, "  netmask=%d.%d.%d.%d",
	      (value >> 24) & 0xff,
	      (value >> 16) & 0xff,
	      (value >>  8) & 0xff,
	      (value >>  0) & 0xff);
      FLUSH_BUFFER;
    }

    {
      u32 value = UIP_TASK_GatewayGet();
      sprintf(line_buffer, "  gateway=%d.%d.%d.%d",
	      (value >> 24) & 0xff,
	      (value >> 16) & 0xff,
	      (value >>  8) & 0xff,
	      (value >>  0) & 0xff);
      FLUSH_BUFFER;
    }

    sprintf(line_buffer, "\n");
    FLUSH_BUFFER;
  }

  {
    sprintf(line_buffer, "\n\n# OSC Configuration\n");
    FLUSH_BUFFER;

    int con;
    for(con=0; con<OSC_SERVER_NUM_CONNECTIONS; ++con) {
      sprintf(line_buffer, "OSC n=%d", con+1);
      FLUSH_BUFFER;

      {
	u32 value = OSC_SERVER_RemoteIP_Get(con);
	sprintf(line_buffer, "  remote_ip=%d.%d.%d.%d",
		(value >> 24) & 0xff,
		(value >> 16) & 0xff,
		(value >>  8) & 0xff,
		(value >>  0) & 0xff);
	FLUSH_BUFFER;
      }

      char mode[10];
      strcpy(mode, OSC_CLIENT_TransferModeShortNameGet(OSC_CLIENT_TransferModeGet(con)));

      // remove spaces and dots
      char *mode_ptr;
      for(mode_ptr=mode; *mode_ptr != 0 && *mode_ptr != ' ' && *mode_ptr != '.'; ++mode_ptr);
      *mode_ptr = 0;


      sprintf(line_buffer, "  remote_port=%d  local_port=%d  transfer_mode=%s\n",
	      OSC_SERVER_RemotePortGet(con),
	      OSC_SERVER_LocalPortGet(con),
	      mode);
      FLUSH_BUFFER;
    }
  }
#endif

  sprintf(line_buffer, "\n\n# Misc. Configuration\n");
  FLUSH_BUFFER;

  sprintf(line_buffer, "DebounceCtr %d\n", mbng_patch_cfg.debounce_ctr);
  FLUSH_BUFFER;
  sprintf(line_buffer, "GlobalChannel %d\n", mbng_patch_cfg.global_chn);
  FLUSH_BUFFER;
  sprintf(line_buffer, "AllNotesOffChannel %d\n", mbng_patch_cfg.all_notes_off_chn);
  FLUSH_BUFFER;
  sprintf(line_buffer, "ConvertNoteOffToOn0 %d\n", mbng_patch_cfg.convert_note_off_to_on0);
  FLUSH_BUFFER;

  return status;
}


/////////////////////////////////////////////////////////////////////////////
//! writes data into config file
//! \returns < 0 on errors (error codes are documented in seq_file.h)
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_C_Write(char *filename)
{
  mbng_file_c_info_t *info = &mbng_file_c_info;

  // store current file name in global variable for UI
  memcpy(mbng_file_c_config_name, filename, MBNG_FILE_C_FILENAME_LEN+1);

  char filepath[MAX_PATH];
  sprintf(filepath, "%s%s.NGC", MBNG_FILES_PATH, mbng_file_c_config_name);

#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MBNG_FILE_C] Open config '%s' for writing\n", filepath);
#endif

  s32 status = 0;
  if( (status=FILE_WriteOpen(filepath, 1)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C] Failed to open/create config file, status: %d\n", status);
#endif
    FILE_WriteClose(); // important to free memory given by malloc
    info->valid = 0;
    return status;
  }

  // write file
  status |= MBNG_FILE_C_Write_Hlp(1);

  // close file
  status |= FILE_WriteClose();


  // check if file is valid
  if( status >= 0 )
    info->valid = 1;

#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MBNG_FILE_C] config file written with status %d\n", status);
#endif

  return (status < 0) ? MBNG_FILE_C_ERR_WRITE : 0;

}

/////////////////////////////////////////////////////////////////////////////
//! sends config data to debug terminal
//! \returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_C_Debug(void)
{
  return MBNG_FILE_C_Write_Hlp(0); // send to debug terminal
}


//! \}
