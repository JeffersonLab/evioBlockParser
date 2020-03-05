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
evioBlockParser::Parse(const uint32_t *buf)
{
  evioStreamParser p;
  // Array of parents where index is the node depth
  //  This'll help evioStreamParser keep track of where the node came from
  Parent_t parentMap[5];

  p.parse(buf, *this, (void *)parentMap);

}


void *
evioBlockParser::containerNodeHandler(int bankLength, int constainerType,
				      int contentType, uint16_t tag, uint8_t num,
				      int depth, const uint32_t *bankPointer,
				      int payloadLength, const uint32_t *payload,
				      void *userArg)
{

  EBP_DEBUG(SHOW_NODE_FOUND,
	    "node(%d)  tag = 0x%x  num = 0x%x  type = 0x%x  length = 0x%x\n",
	    depth, tag, num, contentType, bankLength);

  Parent_t *parentMap = (Parent_t*) userArg;

  if(userArg != NULL)
    {
      if(depth > 0)
	EBP_DEBUG(SHOW_NODE_FOUND,
		  " ** parent node(%d)  tag = 0x%x  num = 0x%x   type = 0x%x\n",
		  depth - 1,
		  parentMap[depth-1].tag,
		  parentMap[depth-1].num,
		  parentMap[depth-1].type);

      Parent_t parent;
      parent.tag = tag;
      parent.num = num;
      parent.type = contentType;

      parentMap[depth] = parent;
    }

  switch (contentType)
    {
    case EVIO_BANK:

      if(depth > 0)
	{
	  if ((parentMap[depth-1].tag >= PHYSICS_EVENT.min) &&
	      (parentMap[depth-1].tag <= PHYSICS_EVENT.max))
	    {
	      // parent node is a CODA Physics Event Bank.  This ought to be a ROC Bank
	      rocMap[tag].length = payloadLength;
	      rocMap[tag].payLoad = (uint32_t *)bankPointer;

	    }
	  else
	    {

	    }
	}

      break;
    case EVIO_SEGMENT:
      if ((parentMap[depth-1].tag >= PHYSICS_EVENT.min) &&
	  (parentMap[depth-1].tag <= PHYSICS_EVENT.max))
	{
	  // parent node is a CODA Physics Event Bank.  This ought to be a Trigger Bank
	  rocMap[tag].length = payloadLength;
	  rocMap[tag].payLoad = (uint32_t *)bankPointer;

	}
      // FIXME: Index the trigger bank
      break;

    default:
      ;
    }

  return ((void *)parentMap);
}

void *
evioBlockParser::leafNodeHandler(int bankLength, int containerType, int contentType,
				 uint16_t tag, uint8_t num, int depth,
				 const uint32_t *bankPointer, int dataLength,
				 const void *data, void *userArg)
{
  // Routine filled with an example to show how its used
  uint32_t *i;
  uint16_t *s;
  uint64_t *ll;

  EBP_DEBUG(SHOW_NODE_FOUND,
	    " * leaf(%d)  tag = 0x%x  num = 0x%x  type = 0x%x  length = 0x%x\n",
	    depth, tag, num, contentType, bankLength);

  Parent_t *parentMap = (Parent_t *) userArg;
  if(userArg != NULL)
    {
      if(depth > 0)
	EBP_DEBUG(SHOW_NODE_FOUND,
		  " ** parent node(%d)  tag = 0x%x  num = 0x%x   type = 0x%x\n",
		  depth - 1,
		  parentMap[depth-1].tag,
		  parentMap[depth-1].num,
		  parentMap[depth-1].type);
    }


  switch (contentType)
    {

    case EVIO_UINT32:
      i = (uint32_t *) data;
      EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%08x  0x%08x\n", i[0], i[1]);
      break;

    case EVIO_USHORT16:
      s = (uint16_t *) data;
      EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%04x  0x%04x\n", s[0], s[1]);
      break;

    case EVIO_ULONG64:
      ll = (uint64_t *) data;
      EBP_DEBUG(SHOW_NODE_FOUND,"  Data:   0x%016lx  0x%016lx\n", ll[0], ll[1]);
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

  return ((void *) NULL);
}

bool
evioBlockParser::Check(uint8_t rocID)
{
  auto roc = rocMap.find(rocID);
  if (roc != rocMap.end())
    return true;

  return false;
}

bool
evioBlockParser::Check(uint8_t rocID, uint16_t bankID)
{
  if(Check(rocID))
    {
      auto bank = rocMap[rocID].bankMap.find(bankID);
      if (bank != rocMap[rocID].bankMap.end())
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
      if (slot != rocMap[rocID].bankMap[bankID].slotMap.end())
	return true;
    }

  return false;
}

bool
evioBlockParser::Check(uint8_t rocID, uint16_t bankID, uint8_t slotnumber, uint8_t evt)
{
  if(Check(rocID, bankID, slotnumber))
    {
      auto ev = rocMap[rocID].bankMap[bankID].slotMap[slotnumber].eventMap.find(evt);
      if (ev != rocMap[rocID].bankMap[bankID].slotMap[slotnumber].eventMap.end())
	return true;
    }

  return false;
}
