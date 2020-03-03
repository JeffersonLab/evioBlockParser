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

  p.parse(buf, *this, (void *)NULL);

}


void *
evioBlockParser::containerNodeHandler(int bankLength, int constainerType,
				      int contentType, uint16_t tag, uint8_t num,
				      int depth, const uint32_t *bankPointer,
				      int payloadLength, const uint32_t *payload,
				      void *userArg)
{
  // Routine filled with an example to show how its used

  printf("node   depth %2d   type,tag,num,length:  0x%x 0x%x 0x%x 0x%x\n",
	 depth, contentType, tag, num, bankLength);
  return NULL;
}

void *
evioBlockParser::leafNodeHandler(int bankLength, int containerType, int contentType,
				 uint16_t tag, uint8_t num, int depth,
				 const uint32_t *bankPointer, int dataLength,
				 const void *data, void *userArg)
{
  // Routine filled with an example to show how its used
  int *i;
  short *s;
  const char *c;
  float *f;
  double *d;
  int64_t *ll;

  printf("   tag = 0x%x  num = 0x%x\n",
	 node_tag, node_num);
  printf
    ("   leaf   depth %2d   type,tag,num,length:  0x%x 0x%x 0x%x 0x%x     data:   ",
     depth, contentType, tag, num, bankLength);

  switch (contentType)
    {

    case 0x0:
    case 0x1:
    case 0xb:
      i = (int *) data;
      cout << hex << i[0] << " " << i[1] << dec << endl;
      break;


    case 0x2:
      f = (float *) data;
      cout << f[0] << " " << f[1] << endl;
      break;

    case 0x3:
      c = (const char *) data;
      cout << c << endl;
      break;

    case 0x6:
    case 0x7:
      c = (const char *) data;
      cout << c[0] << " " << c[1] << endl;
      break;

    case 0x4:
    case 0x5:
      s = (short *) data;
      cout << hex << s[0] << " " << s[1] << dec << endl;
      break;

    case 0x8:
      d = (double *) data;
      cout << d[0] << " " << d[1] << endl;
      break;

    case 0x9:
    case 0xa:
      ll = (int64_t *) data;
      cout << hex << ll[0] << " " << ll[1] << dec << endl;
      break;

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
