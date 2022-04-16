/**
 *
 * @mainpage
 * <pre>
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
 *     Class for indexing ROC Banks and their associated Banks of
 *     Module events from an EVIO buffer.
 *
 * </pre>
 */
#include <iostream>
#include <stdint.h>
#include <byteswap.h>
#include "evioBlockParser.hxx"

/**
 * Parse the buffer at provided address
 *
 * Currently assumes the buffer is that which is provided by
 *   evioFileChannel::readAlloc(...)
 */


void
evioBlockParser::Parse(const uint32_t * buf)
{
  evioStreamParser p;
  // Array of parents where index is the node depth
  //  This'll help evioStreamParser keep track of where the node came from
  ClearTriggerBank();
  Parent_t parentMap[5];

  p.parse(buf, *this, (void *) parentMap);

}


void *
evioBlockParser::containerNodeHandler(int bankLength, int constainerType,
				      int contentType, uint16_t tag,
				      uint8_t num, int depth,
				      const uint32_t * bankPointer,
				      int payloadLength,
				      const uint32_t * payload, void *userArg)
{

  EBP_DEBUG(SHOW_NODE_FOUND,
	    "node(%d)  tag = 0x%x  num = 0x%x  type = 0x%x  length = 0x%x\n",
	    depth, tag, num, contentType, bankLength);

  Parent_t *parentMap = (Parent_t *) userArg;

  if(userArg != NULL)
    {
      if(depth > 0)
	EBP_DEBUG(SHOW_NODE_FOUND,
		  " ** parent node(%d)  tag = 0x%x  num = 0x%x   type = 0x%x\n",
		  depth - 1,
		  parentMap[depth - 1].tag,
		  parentMap[depth - 1].num, parentMap[depth - 1].type);

      Parent_t newparent;
      newparent.tag = tag;
      newparent.num = num;
      newparent.type = contentType;

      parentMap[depth] = newparent;
    }

  Parent_t parent;
  if(depth > 0)
    parent = parentMap[depth - 1];

  switch (contentType)
    {
    case EVIO_BANK:
      ////////////////////////////////////////////////////////////
      // BANK OF BANKS
      ////////////////////////////////////////////////////////////
      if(depth > 0)
	{
	  if((parentMap[depth - 1].tag >= PHYSICS_EVENT.min) &&
	     (parentMap[depth - 1].tag <= PHYSICS_EVENT.max))
	    {
	      ////////////////////////////////////////////////////////////
	      // ROC BANK
	      ////////////////////////////////////////////////////////////
	      // parent node is a CODA Physics Event Bank.  This ought to be a ROC Bank
	      EBP_DEBUG(SHOW_BANK_FOUND,
			"   **** Identified ROC:%d Bank ****\n",
			tag);
	      rocMap[tag].length = payloadLength;
	      rocMap[tag].payload = (uint32_t *) bankPointer;
	    }
	  else
	    {
	      ////////////////////////////////////////////////////////////
	      // BANK of BANKS within the ROC Bank
	      ////////////////////////////////////////////////////////////
	      // Its undefined, at the moment, how to handle these.
	      EBP_DEBUG(SHOW_BANK_FOUND,
			"   ???? Identified ROC:%d Bank of Banks tag: 0x%x ????\n",
			parent.tag, tag);
	      rocMap[parent.tag].bankMap[tag].length = payloadLength;
	      rocMap[parent.tag].bankMap[tag].payload = (uint32_t *) payload;
	    }
	}
      else
      	{
      	  // depth = 0, Ought to be CODA Event
      	  if ((tag >= PHYSICS_EVENT.min) &&
      	      (tag <= PHYSICS_EVENT.max))
      	    {
	      ////////////////////////////////////////////////////////////
	      // CODA PHYSICS EVENT BANK
	      ////////////////////////////////////////////////////////////
      	      EBP_DEBUG(SHOW_BANK_FOUND,
			"   **** Identified CODA Physics Event Bank (blockLevel = %d) ****\n",
			num);
	      blockLevel = num;
      	    }
	  else if((tag >= CONTROL_EVENT.min) &&
		  (tag <= CONTROL_EVENT.max))
      	    {
	      ////////////////////////////////////////////////////////////
	      // CODA CONTROL EVENT BANK
	      ////////////////////////////////////////////////////////////
      	      EBP_DEBUG(SHOW_BANK_FOUND, "   **** Identified CODA Control Event Bank ****\n");
      	    }
	  else
	    {
	      EBP_ERROR("Unknown Event Tag 0x%x\n", tag);
	    }


	}

      break;
    case EVIO_SEGMENT:
      ////////////////////////////////////////////////////////////
      // BANK OF SEGMENTS
      ////////////////////////////////////////////////////////////
      if ((parentMap[depth-1].tag >= PHYSICS_EVENT.min) &&
	  (parentMap[depth-1].tag <= PHYSICS_EVENT.max))
	{
	  ////////////////////////////////////////////////////////////
	  // TRIGGER BANK
	  ////////////////////////////////////////////////////////////
	  EBP_DEBUG(SHOW_BANK_FOUND,
		    "   **** Identified Trigger Bank (nrocs = %d) ****\n",
		    num);
	  // parent node is a CODA Physics Event Bank.  This ought to be a Trigger Bank
	  triggerBank.tag.raw = tag;
	  triggerBank.nrocs = num;
	}
      break;

    default:
      EBP_ERROR("Unexpected Container Node type 0x%x\n", contentType);
    }

  return ((void *) parentMap);
}

void *
evioBlockParser::leafNodeHandler(int bankLength, int containerType,
				 int contentType, uint16_t tag, uint8_t num,
				 int depth, const uint32_t * bankPointer,
				 int dataLength, const void *data,
				 void *userArg)
{
  // Routine filled with an example to show how its used
  uint32_t *i;
  uint16_t *s;
  uint64_t *ll;

  EBP_DEBUG(SHOW_NODE_FOUND,
	    " * leaf(%d)  tag = 0x%x  num = 0x%x  type = 0x%x  length = 0x%x  dataLength = 0x%x\n",
	    depth, tag, num, contentType, bankLength, dataLength);

  EBP_DEBUG(SHOW_NODE_FOUND,
	    " *           containerType = 0x%x\n", containerType);

  Parent_t *parentMap = (Parent_t *) userArg;
  if(userArg != NULL)
    {
      if(depth > 0)
	EBP_DEBUG(SHOW_NODE_FOUND,
		  " ** parent node(%d)  tag = 0x%x  num = 0x%x   type = 0x%x\n",
		  depth - 1,
		  parentMap[depth - 1].tag,
		  parentMap[depth - 1].num, parentMap[depth - 1].type);
    }

  Parent_t parent;
  if(depth > 0)
    parent = parentMap[depth - 1];

  /* Trigger Bank segments */
  if((parent.tag >= TRIGGER_BANK.min) && (parent.tag <= TRIGGER_BANK.max))
    {

      switch (contentType)
	{
	case EVIO_UINT32:
	  i = (uint32_t *) data;
	  EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%08x  0x%08x\n", i[0], i[1]);

	  ////////////////////////////////////////////////////////////
	  // ROC DATA SEGMENT (of TRIGGER BANK)
	  ////////////////////////////////////////////////////////////
	  // Parent is the Trigger Bank.  This should be a ROC data segment
	  EBP_DEBUG(SHOW_SEGMENT_FOUND,
		    "   **** Identified ROC %d data segment ****\n",
		    tag);

	  triggerBank.roc[tag].length = dataLength;
	  triggerBank.roc[tag].payload = (uint32_t *) data;
	  break;

	case EVIO_USHORT16:
	  s = (uint16_t *) data;
	  EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%04x  0x%04x\n", s[0], s[1]);

	  ////////////////////////////////////////////////////////////
	  // EVENT TYPE SEGMENT (of TRIGGER BANK)
	  ////////////////////////////////////////////////////////////
	  // Parent is the Trigger Bank.  This should be the event type segment
	  EBP_DEBUG(SHOW_SEGMENT_FOUND, "   **** Identified Event Type segment ****\n");
	  triggerBank.evtype.length = dataLength;
	  triggerBank.evtype.payload = (uint16_t *) data;
	  triggerBank.evtype.ebID = tag;
	  break;

	case EVIO_ULONG64:
	  ll = (uint64_t *) data;
	  EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%016lx  0x%016lx \n", ll[0], ll[1]);

	  EBP_DEBUG(SHOW_SEGMENT_FOUND, "   **** Identified Timestamp segment ****\n");
	  // Parent is the Trigger Bank.  This should be the timestamp segment
	  triggerBank.timestamp.length = dataLength;
	  triggerBank.timestamp.payload = (uint64_t *) data;
	  triggerBank.timestamp.ebID = tag;
	  ll = (uint64_t *) data;
	  EBP_DEBUG(SHOW_NODE_FOUND, "  Data:   0x%016lx  0x%016lx \n", ll[0],
		    ll[1]);
	  break;

	case EVIO_INT32:
	case EVIO_UNKNOWN32:
	case EVIO_FLOAT32:
	case EVIO_CHARSTAR8:
	case EVIO_CHAR8:
	case EVIO_UCHAR8:
	case EVIO_SHORT16:
	case EVIO_DOUBLE64:
	case EVIO_LONG64:
	default:
	  printf("%s: Ignored Content Type (0x%x) (%s)\n",
		 __func__, contentType, DataTypeNames[contentType]);
	}
    }
  else
    {				/* ROC data banks */
      switch (contentType)
	{

	case EVIO_UINT32:
	  rocMap[parent.tag].bankMap[tag].length = dataLength;
	  rocMap[parent.tag].bankMap[tag].payload = (uint32_t *) data;
	  EBP_DEBUG(SHOW_SEGMENT_FOUND,
		    "   **** Identified ROC u32 data bank ****\n");
	  i = (uint32_t *) rocMap[parent.tag].bankMap[tag].payload;
	  EBP_DEBUG(SHOW_NODE_FOUND, "  Data:   0x%08x  0x%08x\n", i[0],
		    i[1]);
	  break;

	case EVIO_USHORT16:
	  EBP_DEBUG(SHOW_SEGMENT_FOUND,
		    "   **** Identified ROC u16 data bank ****\n");
	  s = (uint16_t *) data;
	  EBP_DEBUG(SHOW_NODE_FOUND, "  Data:   0x%04x  0x%04x\n", s[0],
		    s[1]);
	  break;

	case EVIO_ULONG64:
	  EBP_DEBUG(SHOW_SEGMENT_FOUND,
		    "   **** Identified ROC u64 data bank ****\n");
	  ll = (uint64_t *) data;
	  EBP_DEBUG(SHOW_NODE_FOUND, "  Data:   0x%016lx  0x%016lx \n", ll[0],
		    ll[1]);
	  break;

	case EVIO_INT32:
	case EVIO_UNKNOWN32:
	case EVIO_FLOAT32:
	case EVIO_CHARSTAR8:
	case EVIO_CHAR8:
	case EVIO_UCHAR8:
	case EVIO_SHORT16:
	case EVIO_DOUBLE64:
	case EVIO_LONG64:
	default:
	  printf("%s: Ignored Content Type (0x%x) (%s) from parent tag 0x%x\n",
		 __func__, contentType, DataTypeNames[contentType],
		 parent.tag);
	  // printf("\n%-12s\n", (char *)data);
	}

    }

  return ((void *) NULL);
}

// Index the events in the slot bank
// return number of events indexed

uint8_t
evioBlockParser::ParseJLabBank(uint8_t rocID, uint16_t bankID, bool doByteSwap = false)
{
  uint8_t rval = 0;
  int blkCounter = 0; /* count of blocks within the data (one per module) */

  uint32_t *data;
  Slot_t *currentSlot;

  int32_t nwords = GetU32(rocID, bankID, &data);
  /* Index the Bank of Data.
     Looking for Block Headers, Event headers, and Block Trailers  */
  int32_t iword = 0;
  while(iword < nwords)
    {
      jlab_data_word_t jdata;

      jdata.raw = data[iword];
      if(doByteSwap)
	jdata.raw = bswap_32(jdata.raw);

      if(jdata.bf.data_type_defining == 1)
	{
	  switch(jdata.bf.data_type_tag)
	    {
	    case BLOCK_HEADER: /* 0: BLOCK HEADER */
	      {
		block_header_t bheader;
		bheader.raw = jdata.raw;

		blkCounter++; /* Increment local block counter */

		currentSlot = &rocMap[rocID].bankMap[bankID].slotMap[bheader.bf.slot_number];
		currentSlot->slotnumber = bheader.bf.slot_number;
		currentSlot->blockheader[0] = bheader.raw;
		currentSlot->nblockheader = 1;
		currentSlot->evtCounter = 0;

		// Note: The Block header data type and slot number may be the only
		// consistent information across different module formats.
		EBP_DEBUG(SHOW_BLOCK_HEADER,
			  "[%6d  0x%08x] "
			  "BLOCK HEADER: slot %2d, block_number %3d, block_level %3d\n",
			  iword,
			  bheader.raw,
			  bheader.bf.slot_number,
			  bheader.bf.event_block_number,
			  bheader.bf.number_of_events_in_block);
		rval = bheader.bf.number_of_events_in_block;
		break;
	      }

	    case BLOCK_TRAILER: /* 1: BLOCK TRAILER */
	      {
		block_trailer_t btrailer;

		btrailer.raw = jdata.raw;
		currentSlot->blocktrailer = btrailer.raw;

		EBP_DEBUG(SHOW_BLOCK_TRAILER,
			  "[%6d  0x%08x] "
			  "BLOCK TRAILER: slot %2d, nwords %d\n",
			  iword,
			  btrailer.raw,
			  btrailer.bf.slot_number,
			  btrailer.bf.words_in_block);

		if(currentSlot->evtCounter > 0)
		  {
		    // Point to the previous Event
		    Event_t *prevEvent = &currentSlot->eventMap[currentSlot->evtCounter - 1];
		    // Calculate it's data length
		    prevEvent->length = iword - prevEvent->index;
		  }

		// Check the slot number to make sure this block
		// trailer is associated with the previous block
		// header
		if(btrailer.bf.slot_number != currentSlot->slotnumber)
		  {
		    EBP_ERROR("[%6d  0x%08x] "
			      "blockheader slot %d != blocktrailer slot %d\n",
			      iword,
			      btrailer.raw,
			      currentSlot->slotnumber,
			      btrailer.bf.slot_number);
		    rval = -1;
		  }

		// Check the number of words vs. words counted within the block
		if(btrailer.bf.words_in_block != iword + 1)
		  {
		    EBP_ERROR("[%6d  0x%08x] "
			   "trailer #words %d != actual #words %d\n",
			   iword,
			   btrailer.raw,
			   btrailer.bf.words_in_block,
			   iword+1);
		    rval = -1;
		  }
		break;
	      }

	    case EVENT_HEADER: /* 2: EVENT HEADER */
	      {
		event_header_t eheader;
		eheader.raw = jdata.raw;

		// Note: The Event header data type and slot number may be the only
		// consistent information across different module formats.
		EBP_DEBUG(SHOW_EVENT_HEADER,
			  "[%6d  0x%08x] "
			  "EVENT HEADER: trigger number %d\n",
			  iword,
			  eheader.raw,
			  eheader.bf.event_number);

		if(currentSlot->slotnumber == 0)
		  {
		    EBP_ERROR("Event Header Found. Slot Number = 0.\n");
		    return -1;
		  }

		// Point to the current Event in this Slot's eventMap
		Event_t *currentEvent = &currentSlot->eventMap[currentSlot->evtCounter];
		currentEvent->index = iword;
		currentSlot->eventMap[currentSlot->evtCounter].payload = (uint32_t *)&data[iword+1];

		if(currentSlot->evtCounter > 0)
		  {
		    // Point to the previous Event
		    Event_t *prevEvent = &currentSlot->eventMap[currentSlot->evtCounter - 1];
		    // Calculate it's data length
		    prevEvent->length = iword - prevEvent->index;
		  }

		currentSlot->evtCounter++;

		break;
	      }

	    case SCALER_HEADER: /* 12: SCALER HEADER */
	      {
		// Here is some butter to help slide FADC250 data through the parser
		// FIXME: think of a better way please
		block_header_t bh; bh.raw = currentSlot->blockheader[0];
		if(bh.bf.module_ID == JLAB_MODULE_FADC250)
		  {
		    scaler_header_t sheader;
		    sheader.raw = jdata.raw;
		    EBP_DEBUG(SHOW_OTHER,
			  "[%6d  0x%08x] "
			  "SCALER HEADER: number of scaler words %d\n",
			  iword, data[iword],
			  sheader.bf.number_scaler_words);

		    // Skip over these to avoid confusion with data type headers
		    iword += sheader.bf.number_scaler_words;
		  }
		else
		  {
		    EBP_DEBUG(SHOW_OTHER,
			      "[%6d  0x%08x] "
			      "OTHER (%2d)\n",
			      iword,
			      data[iword],
			      jdata.bf.data_type_tag);
		  }
		break;
	      }

	    case DATA_NOT_VALID: /* 14: DATA NOT VALID */
	      {
		EBP_DEBUG(SHOW_OTHER,
			  "[%6d  0x%08x] "
			  "DATA_NOT_VALID\n",
			  iword,
			  data[iword]);

		break;
	      }

	    case FILLER: /* 15: FILLER */
	      {
		EBP_DEBUG(SHOW_OTHER,
			  "[%6d  0x%08x] "
			  "FILLER\n",
			  iword,
			  data[iword]);

		break;
	      }

	    default:
	      /* Ignore all other data types for now */
		EBP_DEBUG(SHOW_OTHER,
			  "[%6d  0x%08x] "
			  "OTHER (%2d)\n",
			  iword,
			  data[iword],
			  jdata.bf.data_type_tag);

	    } /* switch(data_type) */

	} /* if(jdata.bf.data_type_defining == 1) */
      else
	{
	  EBP_DEBUG(SHOW_OTHER,
		    "[%6d  0x%08x] "
		    "continued (%2d)\n",
		    iword,
		    data[iword],
		    jdata.bf.data_type_tag);
	}

      iword++;

    } /* while(iword<nwords) */

  return rval;
}

void
evioBlockParser::ClearTriggerBank()
{
  triggerBank.tag.raw = 0;
  triggerBank.nrocs = 0;
  triggerBank.roc.clear();
}

void
evioBlockParser::ClearBankMap(uint8_t rocID)
{
  rocMap[rocID].bankMap.clear();
}

void
evioBlockParser::ClearSlotMap(uint8_t rocID, uint16_t bankID)
{
  rocMap[rocID].bankMap[bankID].slotMap.clear();
}

void
evioBlockParser::ClearEventMap(uint8_t rocID, uint16_t bankID, uint8_t slotID)
{
  rocMap[rocID].bankMap[bankID].slotMap[slotID].eventMap.clear();
}

void
evioBlockParser::ClearMaps()
{
  ClearTriggerBank();
  rocMap.clear();
}


bool evioBlockParser::Check(uint8_t rocID)
{
  auto
    roc = rocMap.find(rocID);
  if(roc != rocMap.end())
    return true;

  return false;
}

bool evioBlockParser::Check(uint8_t rocID, uint16_t bankID)
{
  if(Check(rocID))
    {
      auto
	bank = rocMap[rocID].bankMap.find(bankID);
      if(bank != rocMap[rocID].bankMap.end())
	return true;
    }

  return false;
}

bool
evioBlockParser::Check(uint8_t rocID, uint16_t bankID, uint8_t slotnumber)
{
  if(Check(rocID, bankID))
    {
      auto slot = rocMap[rocID].bankMap[bankID].slotMap.find(slotnumber);
      if(slot != rocMap[rocID].bankMap[bankID].slotMap.end())
	return true;
    }

  return false;
}

bool
evioBlockParser::Check(uint8_t rocID, uint16_t bankID, uint8_t slotnumber,
		       uint8_t evt)
{
  if(Check(rocID, bankID, slotnumber))
    {
      auto ev =
	rocMap[rocID].bankMap[bankID].slotMap[slotnumber].eventMap.find(evt);
      if(ev !=
	 rocMap[rocID].bankMap[bankID].slotMap[slotnumber].eventMap.end())
	return true;
    }

  return false;
}

/**
 * Get a list of ROCIDs in the parsed buffer
 */

vector<uint8_t>
evioBlockParser::GetRocList()
{
  vector<uint8_t> retRocs;

  for(std::map<uint8_t,Roc_t>::iterator it = rocMap.begin();
      it != rocMap.end(); ++it)
    {
      retRocs.push_back(it->first);
    }

  return retRocs;

}

/**
 * Get a list of Banks for given rocID, in the parsed buffer
 */

vector<uint16_t>
evioBlockParser::GetBankList(uint8_t rocID)
{
  vector<uint16_t> retBanks;

  if(Check(rocID))
    {
      for(std::map<uint16_t,Bank_t>::iterator it = rocMap[rocID].bankMap.begin();
	  it != rocMap[rocID].bankMap.end(); ++it)
	{
	  retBanks.push_back(it->first);
	}
    }

  return retBanks;

}

/**
 * Get a list of Slots for given rocID and bankID, in the parsed buffer
 */

vector<uint8_t>
evioBlockParser::GetSlotList(uint8_t rocID, uint16_t bankID)
{
  vector<uint8_t> retSlots;

  if(Check(rocID, bankID))
    {
      for(std::map<uint8_t,Slot_t>::iterator it = rocMap[rocID].bankMap[bankID].slotMap.begin();
	  it != rocMap[rocID].bankMap[bankID].slotMap.end(); ++it)
	{
	  retSlots.push_back(it->first);
	}
    }

  return retSlots;

}

/**
 * GetU32 - Get a pointer to the U32 data array for rocID, with bank = bankID
 */

int32_t
evioBlockParser::GetU32(uint8_t rocID, uint16_t bankID, uint32_t **payload)
{
  int32_t rval = -1;
  if(Check(rocID, bankID))
    {
      *payload = (uint32_t *)rocMap[rocID].bankMap[bankID].payload;
      rval = rocMap[rocID].bankMap[bankID].length;
    }

  return rval;
}

int32_t
evioBlockParser::GetU32(uint8_t rocID, uint16_t bankID, uint8_t slotID,
			uint8_t eventID, uint32_t **payload)
{
  int32_t rval = -1;
  if(Check(rocID, bankID, slotID))
    {
      *payload = (uint32_t *)rocMap[rocID].bankMap[bankID].slotMap[slotID].eventMap[eventID].payload;
      rval = rocMap[rocID].bankMap[bankID].slotMap[slotID].eventMap[eventID].length;
    }

  return rval;
}

/**
 * GetU16 - Get a pointer to the U16 data array for rocID, with bank = bankID
 */

int32_t
evioBlockParser::GetU16(uint8_t rocID, uint16_t bankID, uint16_t **payload)
{
  int32_t rval = -1;
  if(Check(rocID, bankID))
    {
      *payload = (uint16_t *)rocMap[rocID].bankMap[bankID].payload;
      rval = rocMap[rocID].bankMap[bankID].length;
    }

  return rval;
}

bool
evioBlockParser::CheckTriggerBank()
{
  if(triggerBank.tag.raw == 0)
    return false;

  return true;
}

bool
evioBlockParser::CheckTriggerBank(uint8_t rocID)
{
  auto
    roc = triggerBank.roc.find(rocID);
  if(roc != triggerBank.roc.end())
    return true;

  return false;
}


int32_t
evioBlockParser::GetTriggerBankEvTag(uint16_t *evtag)
{
  int32_t rval = -1;

  if(CheckTriggerBank())
    {
      *evtag = triggerBank.tag.raw;
      rval = 1;
    }

  return rval;
}

int32_t
evioBlockParser::GetTriggerBankTimestamp(uint64_t **payload)
{
  int32_t rval = -1;

  if(CheckTriggerBank())
    {
      *payload = (uint64_t *)triggerBank.timestamp.payload;
      rval = triggerBank.timestamp.length;
    }

  return rval;
}

int32_t
evioBlockParser::GetTriggerBankEvType(uint16_t **payload)
{
  int32_t rval = -1;

  if(CheckTriggerBank())
    {
      *payload = (uint16_t *)triggerBank.evtype.payload;
      rval = triggerBank.evtype.length;
    }

  return rval;
}

int32_t
evioBlockParser::GetTriggerBankRocData(uint8_t rocID, uint32_t **payload)
{
  int32_t rval = -1;

  if(CheckTriggerBank(rocID))
    {
      *payload = (uint32_t *)triggerBank.roc[rocID].payload;
      rval = triggerBank.roc[rocID].length;
    }

  return rval;
}

vector<uint8_t>
evioBlockParser::GetTriggerBankRocSegmentID()
{
  vector<uint8_t> retRocs;

  for(std::map<uint8_t,trigger_segment32_t>::iterator it = triggerBank.roc.begin();
      it != triggerBank.roc.end(); ++it)
    {
      retRocs.push_back(it->first);
    }

  return retRocs;
}
