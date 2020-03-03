#ifndef __EVIOBLOCKPARSER_HXX
#define __EVIOBLOCKPARSER_HXX
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


class evioBlockParser:public evioStreamParserHandler
{
  //
  // Typedefs for JLab data format decoding
  //
  typedef struct
  {
    uint32_t undef:27;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  } jlab_data_word;

  typedef union
  {
    uint32_t raw;
    jlab_data_word bf;
  } jlab_data_word_t;

  // 0: BLOCK HEADER
  typedef struct
  {
    uint32_t number_of_events_in_block:8;
    uint32_t event_block_number:10;
    uint32_t module_ID:4;
    uint32_t slot_number:5;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  } block_header;

  typedef union
  {
    uint32_t raw;
    block_header bf;
  } block_header_t;

  // 1: BLOCK TRAILER
  typedef struct
  {
    uint32_t words_in_block:22;
    uint32_t slot_number:5;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  } block_trailer;

  typedef union
  {
    uint32_t raw;
    block_trailer bf;
  } block_trailer_t;

  // 2: EVENT HEADER
  typedef struct
  {
    uint32_t event_number:22;
    uint32_t slot_number:5;
    uint32_t data_type_tag:4;
    uint32_t data_type_defining:1;
  } event_header;

  typedef union
  {
    uint32_t raw;
    event_header bf;
  } event_header_t;

  //
  // Typedefs for indexing banks and events
  //
  typedef struct EventInfo
  {
    uint32_t index;			// points to event header
    uint32_t length;
  } Event_t;

  typedef struct SlotInfo
  {
    uint8_t slotnumber;
    uint8_t  nblockheader;
    uint32_t blockheader[2];
    uint32_t blocktrailer;
    map < uint8_t, Event_t > eventMap;
  } Slot_t;

  typedef struct DataBankInfo
  {
    uint32_t length;
    uint32_t header;
    uint32_t index;			// points to first data word
    uint32_t slotmask;
    map < uint8_t, Slot_t > slotMap;
  } Bank_t;

  typedef struct RocBankInfo
  {
    uint32_t length;
    uint32_t header;
    uint32_t index;			// Index after header
    map < uint16_t, Bank_t > bankMap;
  } Roc_t;

public:
  evioBlockParser();
  ~evioBlockParser();

  void Parse(const uint32_t *buf);

private:
  // Main storage container
  map < uint8_t, Roc_t > rocMap;

  // Prescriptions for parsing the EVIO buffer
  uint16_t node_tag;
  uint32_t node_num;
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

  //
  // Routines for checking if the evio buffer was found to contain the sought after
  //  rocIDs, bankIDs, slotnumbers, and events
  //
  bool Check(uint8_t rocID);
  bool Check(uint8_t rocID, uint16_t bankID);
  bool Check(uint8_t rocID, uint16_t bankID, uint8_t slotnumber);
  bool Check(uint8_t rocID, uint16_t bankID, uint8_t slotnumber, uint8_t evt);

};

#endif //__EVIOBLOCKPARSER_HXX
