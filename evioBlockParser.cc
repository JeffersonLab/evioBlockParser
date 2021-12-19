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
#include "evioBlockParser.hxx"

void
evioBlockParser::Parse(const uint32_t * buf)
{
  evioStreamParser p;
  // Array of parents where index is the node depth
  //  This'll help evioStreamParser keep track of where the node came from
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
<<<<<<< variant A
      	}
>>>>>>> variant B


	}
======= end

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

    case EVIO_UINT32:
      i = (uint32_t *) data;
      EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%08x  0x%08x\n", i[0], i[1]);

      if((parent.tag >= TRIGGER_BANK.min) &&
	 (parent.tag <= TRIGGER_BANK.max))
	{
	  ////////////////////////////////////////////////////////////
	  // ROC DATA SEGMENT (of TRIGGER BANK)
	  ////////////////////////////////////////////////////////////
	  // Parent is the Trigger Bank.  This should be a ROC data segment
	  EBP_DEBUG(SHOW_SEGMENT_FOUND,
		    "   **** Identified ROC %d data segment ****\n",
		    tag);

	  triggerBank.roc[tag].length = dataLength;
	  triggerBank.roc[tag].payload = (uint32_t *) data;
	}
      else if((depth == 0) &&
	      (tag >= CONTROL_EVENT.min) &&
	      (tag <= CONTROL_EVENT.max))
	{
	  ////////////////////////////////////////////////////////////
	  // CODA CONTROL EVENT
	  ////////////////////////////////////////////////////////////
	  // Parent is a CODA Control Event
	  EBP_DEBUG(SHOW_BANK_FOUND,
		    "   **** Identified CODA Control Event 0x%x ****\n",
		    tag);
	}
      else
	{
	  ////////////////////////////////////////////////////////////
	  // ROC DATA BANK (potentially module data)
	  ////////////////////////////////////////////////////////////
	  // Potentially a Bank with block data to index
	  EBP_DEBUG(SHOW_BANK_FOUND,
		    "   **** Identified ROC:%d Bank tag: 0x%x ****\n",
		    parent.tag, tag);

	  // Pass it on to the Event Indexer
	}
      break;

    case EVIO_USHORT16:
      s = (uint16_t *) data;
      EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%04x  0x%04x\n", s[0], s[1]);

      if((parent.tag >= TRIGGER_BANK.min) &&
	 (parent.tag <= TRIGGER_BANK.max))
	{
	  ////////////////////////////////////////////////////////////
	  // EVENT TYPE SEGMENT (of TRIGGER BANK)
	  ////////////////////////////////////////////////////////////
	  // Parent is the Trigger Bank.  This should be the event type segment
	  EBP_DEBUG(SHOW_SEGMENT_FOUND, "   **** Identified Event Type segment ****\n");
	  triggerBank.evtype.length = dataLength;
	  triggerBank.evtype.payload = (uint16_t *) data;
	  triggerBank.evtype.ebID = tag;
	}
      else
	{
	  EBP_ERROR("Unknown uint16_t bank.  tag: 0x%x  num: 0x%x\n",
		    tag, num);
	}
      break;

    case EVIO_ULONG64:
      ll = (uint64_t *) data;
      EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%016lx  0x%016lx \n", ll[0], ll[1]);

      if((parent.tag >= TRIGGER_BANK.min) &&
	 (parent.tag <= TRIGGER_BANK.max))
	{
	  ////////////////////////////////////////////////////////////
	  // TIMESTAMP SEGMENT (of TRIGGER BANK)
	  ////////////////////////////////////////////////////////////
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
	  printf("\n%-12s\n", (char *)data);
	}
      else
	{
	  EBP_ERROR("Unknown uint64_t bank.  tag: 0x%x  num: 0x%x\n",
		    tag, num);
	}
      break;

    }

  return ((void *) NULL);
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
