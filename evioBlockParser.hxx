#pragma once
/**
 *  Copyright (c) 2020        Southeastern Universities Research Association,
 *                            Thomas Jefferson National Accelerator Facility
 *
 *    This software was developed under a United States Government license
 *    described in the NOTICE file included as part of this distribution.
 *
 *    Authors: Bryan Moffit
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3
 *             Phone: (757) 269-5660             12000 Jefferson Ave.
 *             Fax:   (757) 269-5800             Newport News, VA 23606
 *
 *----------------------------------------------------------------------------
 *
 * Description:
 *     Header for Class for indexing ROC Banks and their associated
 *     Banks of Module events
 *
 */

#include <map>
#include "evioUtil.hxx"

using namespace std;
using namespace evio;

#define EBP_ERROR(format, ...) {					\
    char buf[128];							\
    sprintf((char *)buf,format, ## __VA_ARGS__);			\
    cerr << __func__ << ": ERROR: " << buf;				\
  }

#define EBP_SHOW(format, ...) {						\
    char buf[128];							\
    sprintf((char *)buf,format, ## __VA_ARGS__);			\
    cout << buf;							\
  }

#define EBP_DEBUG(type, format, ...) {					\
    if(type & debugMask)						\
      {									\
	char buf[128];							\
	sprintf((char *)buf,format, ## __VA_ARGS__);			\
	cout << __func__ << ": " << buf;				\
      }									\
  }

class evioBlockParser:public evioStreamParserHandler
{
  //
  // structs & unions for JLab data format decoding
  //
  enum jlabDataTypes
    {
      BLOCK_HEADER   = 0,
      BLOCK_TRAILER  = 1,
      EVENT_HEADER   = 2,
      TRIGGER_TIME   = 3,
      SCALER_HEADER  = 12,
      DATA_NOT_VALID = 14,
      FILLER         = 15
    };

  enum jlabModuleIDs
    {
     JLAB_MODULE_FADC250 = 1
    };

  struct jlab_data_word
  {
    uint32_t undef:27;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  };

  union jlab_data_word_t
  {
    uint32_t raw;
    jlab_data_word bf;
  };

  // 0: BLOCK HEADER
  struct block_header
  {
    uint32_t number_of_events_in_block:8;
    uint32_t event_block_number:10;
    uint32_t module_ID:4;
    uint32_t slot_number:5;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  };

  union block_header_t
  {
    uint32_t raw;
    block_header bf;
  };

  // 1: BLOCK TRAILER
  struct block_trailer
  {
    uint32_t words_in_block:22;
    uint32_t slot_number:5;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  };

  union block_trailer_t
  {
    uint32_t raw;
    block_trailer bf;
  };

  // 2: EVENT HEADER
  struct event_header
  {
    uint32_t event_number:22;
    uint32_t slot_number:5;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  };

  union event_header_t
  {
    uint32_t raw;
    event_header bf;
  };

  // 12: SCALER
  struct scaler_header
  {
    uint32_t number_scaler_words:6;
    uint32_t undef:21;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  };

  union scaler_header_t
  {
    uint32_t raw;
    scaler_header bf;
  };




  // Range struct used to decode CODA Bank Tag
  struct range
  {
    uint16_t min;
    uint16_t max;
  };

  // CODA Reserved Bank Tags
  range CODA_BANK     = {0xFF00, 0xFFFF};
  range CONTROL_EVENT = {0xFFC0, 0xFFEF};
  range PHYSICS_EVENT = {0xFF50, 0xFF8F};
  range TRIGGER_BANK  = {0xFF10, 0xFF4F};

  /* struct & union to decode trigger bank tag */
  struct trigger_bank_tag
  {
    uint16_t timestamp:1;      // bit 0: Contains event timestamps
    uint16_t runInfo:1;        // bit 1: Contains run number and run type in 64bit segment
    uint16_t noSpecificData:1; // bit 2: No Event Specific Data (From TS/TI)
    uint16_t blank:1;
    uint16_t built:1;          // bit 4: Built trigger (from PEB, etc)
    uint16_t raw:1;            // bit 5: Raw trigger (from TS/TI)
    uint16_t ff0:10;
  };

  union trigger_bank_tag_t
  {
    uint16_t raw;
    trigger_bank_tag bf;
  };

  struct trigger_segment16_t
  {
    uint8_t ebID;
    uint32_t length;
    uint16_t *payload;
  };

  struct trigger_segment32_t
  {
    uint32_t length;
    uint32_t *payload;
  };

  struct trigger_segment64_t
  {
    uint8_t ebID;
    uint32_t length;
    uint64_t *payload;
  };

  struct TriggerBank_t
  {
    trigger_bank_tag_t tag;
    trigger_segment64_t timestamp;
    trigger_segment16_t evtype;
    uint8_t nrocs;
    map <uint8_t, trigger_segment32_t> roc;
  };


  //
  // structs for indexing banks and events
  //
  struct Event_t
  {
    uint32_t index;			// points to event header
    uint32_t length;
    uint32_t *payload;
  };

  struct Slot_t
  {
    uint8_t slotnumber;
    uint8_t  nblockheader;
    uint32_t blockheader[2];
    uint32_t blocktrailer;
    uint8_t evtCounter;
    map < uint8_t, Event_t > eventMap;
  };

  struct Bank_t
  {
    uint32_t length;
    uint32_t header;
    uint32_t index;			// points to first data word
    uint32_t slotmask;
    uint32_t *payload;
    map < uint8_t, Slot_t > slotMap;
  };

  struct Roc_t
  {
    uint32_t length;
    uint32_t header;
    uint32_t index;			// Index after header
    uint32_t *payload;
    map < uint16_t, Bank_t > bankMap;
  };

  //
  // struct to pass to container and leaf nodes
  //
  struct Parent_t
  {
    uint16_t tag;
    uint16_t num;
    uint8_t  type;
  };

  //
  // enum for endian flag
  //
  enum endian_type
    {
      EBP_LITTLE_ENDIAN = 0,
      EBP_BIG_ENDIAN    = 1
    };

public:
  evioBlockParser() {};
  ~evioBlockParser() {};

  void Parse(const uint32_t *buf);
  uint8_t ParseJLabBank(uint8_t rocID, uint16_t bankID, bool doByteSwap);
  void SetDebugMask(uint32_t mask) {debugMask = mask;};

  //
  // enum for debug flags
  //
  enum debug_flags
    {
      SHOW_BLOCK_HEADER     = (1<<0),
      SHOW_BLOCK_TRAILER    = (1<<1),
      SHOW_EVENT_HEADER     = (1<<2),
      SHOW_EVENT_TIMESTAMP  = (1<<3),
      SHOW_OTHER            = (1<<4),
      SHOW_NODE_FOUND       = (1<<5),
      SHOW_FILL_EVENTS      = (1<<6),
      SHOW_SECOND_PASS      = (1<<7),
      SHOW_UNBLOCK          = (1<<8),
      SHOW_IGNORED_BANKS    = (1<<9),
      SHOW_BANK_FOUND       = (1<<10),
      SHOW_SEGMENT_FOUND    = (1<<11),
      SHOW_BANK_NOT_FOUND   = (1<<12)
    };

  //
  // Routines for checking if the evio buffer was found to contain the sought after
  //  rocIDs, bankIDs, slotnumbers, and events
  //
  bool Check(uint8_t rocID);
  bool Check(uint8_t rocID, uint16_t bankID);
  bool Check(uint8_t rocID, uint16_t bankID, uint8_t slotnumber);
  bool Check(uint8_t rocID, uint16_t bankID, uint8_t slotnumber, uint8_t evt);

  //
  // Data access routines
  //
  //

  vector<uint8_t> GetRocList();
  vector<uint16_t> GetBankList(uint8_t rocID);
  vector<uint8_t>  GetSlotList(uint8_t rocID, uint16_t bankID);
  int32_t GetU32(uint8_t rocID, uint16_t bankID, uint32_t **payload);
  int32_t GetU32(uint8_t rocID, uint16_t bankID, uint8_t slotID,
		 uint8_t eventID, uint32_t **payload);
  int32_t GetU16(uint8_t rocID, uint16_t bankID, uint16_t **payload);

  bool CheckTriggerBank();
  bool CheckTriggerBank(uint8_t rocID);
  int32_t GetTriggerBankEvTag(uint16_t *evtag);
  int32_t GetTriggerBankTimestamp(uint64_t **payload);
  int32_t GetTriggerBankEvType(uint16_t **payload);
  int32_t GetTriggerBankRocData(uint8_t rocID, uint32_t **payload);
  vector<uint8_t> GetTriggerBankRocSegmentID();
  void ClearMaps();


private:
  // Main storage container
  map < uint8_t, Roc_t > rocMap;
  uint8_t blockLevel;
  TriggerBank_t triggerBank;

  // Handler for processing Banks of segments/banks
  void *containerNodeHandler(int bankLength, int constainerType,
			     int contentType, uint16_t tag, uint8_t num,
			     int depth, const uint32_t * bankPointer,
			     int payloadLength, const uint32_t * payload,
			     void *userArg);

  // Handler for processing Banks/segments of uint32s, uint16s, uint8s, et al.
  void *leafNodeHandler(int bankLength, int containerType, int contentType,
			uint16_t tag, uint8_t num, int depth,
			const uint32_t * bankPointer, int dataLength,
			const void *data, void *userArg);

  void ClearTriggerBank();
  void ClearBankMap(uint8_t rocID);
  void ClearSlotMap(uint8_t rocID, uint16_t bankID);
  void ClearEventMap(uint8_t rocID, uint16_t bankID, uint8_t slotID);
  uint32_t debugMask;

};
