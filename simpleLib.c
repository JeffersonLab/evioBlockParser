/*----------------------------------------------------------------------------*/
/**
 * @mainpage
 * <pre>
 *  Copyright (c) 2014        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Library for
 *      (S)econdary (I)nstance (M)ultiblock (P)rocessing (L)ist (E)xtraction
 *
 * </pre>
 *----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include "simpleLib.h"

typedef void (*VOIDFUNCPTR) ();

/* Global Variables */
simpleDebug        simpleDebugMask=0;

/* data address provided by user */
unsigned long dataAddr = 0;

/* Trigger Bank of Segment */
trigBankInfo trigBank;

/* ROC Banks */
rocBankInfo rocBank[SIMPLE_MAX_ROCS];
int nRocs;

/* Payload module data (fADC250, fADC125, f1TDC) */
/* Bank Data - Banks of 4 byte unsigned integers */
bankDataInfo bankData[SIMPLE_MAX_ROCS][SIMPLE_MAX_BANKS];

int               ignoreUndefinedBanks=0;        /* Default is false */

/* User defined Banks for separate modules, configured before run */
simpleBankConfig uBank[SIMPLE_MAX_BANKS];
int nubanks = 0;

/* Other data, to be copied to only one event... or to every event in the block */
otherBankInfo     otherBank[SIMPLE_MAX_BANKS];
unsigned int      notherBank=0;

/* Structure defaults */
const simpleBankConfig BANK_CONFIG_DEFAULTS =
  {
    -1,   /* length */
    {-1},   /* header */
    0,  /* rocID */
    SIMPLE_LITTLE_ENDIAN, /* endian */
    0,    /* isBlocked */
    0,    /* module_header */
    0xFFFFFFFF, /* header_mask */
    (VOIDFUNCPTR)simpleScanBank /* firstPassRoutine */
  };

int
simpleInit()
{
  int ibank;
  for(ibank = 0; ibank < SIMPLE_MAX_BANKS; ibank++)
    {
      uBank[ibank] = BANK_CONFIG_DEFAULTS;
    }

  nubanks = 0;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the debug level (perhaps mask)
 *
 * @param dbMask Mask of debugging bits
 *
 */
void
simpleConfigSetDebug(int dbMask)
{
  simpleDebugMask = dbMask;
}

/**
 * @ingroup Config
 * @brief Configure Banks to be indexed
 *
 * @param rocID             roc ID
 * @param bankID            Bank ID
 * @param num               NOT USED
 * @param endian            little = 0, big = 1
 * @param isBlocked         no = 0, yes = 1
 * @param firstPassRoutine  Routine to call for first pass processing
 *
 * @return OK if successful, otherwise ERROR
 */

int
simpleConfigBank(int rocID, int bankID, int num,
		 int endian, int isBlocked, void *firstPassRoutine)
{

  if(firstPassRoutine==NULL)
    {
      firstPassRoutine=simpleScanBank;
    }

  uBank[nubanks].rocID             = rocID;
  uBank[nubanks].header.bf.tag     = bankID;
  uBank[nubanks].header.bf.num     = num;
  uBank[nubanks].endian            = endian;
  uBank[nubanks].isBlocked         = isBlocked;
  uBank[nubanks].firstPassRoutine  = firstPassRoutine;

  nubanks++;

  return OK;
}

int
simpleFindConfigBankIndex(int rocID, int tag)
{
  int ibank = 0;

  for(ibank = 0; ibank < nubanks; ibank++)
    {
      if(uBank[ibank].rocID == rocID)
	{
	  if(uBank[ibank].header.bf.tag == tag)
	    {
	      return ibank;
	    }
	}
    }

  return -1;
}

int
simpleConfigIgnoreUndefinedBlocks(int ignore)
{
  if(ignore>=0)
    ignoreUndefinedBanks=1;
  else
    ignoreUndefinedBanks=0;

  printf("%s: INFO: Ignoring undefined banks.\n",__FUNCTION__);

  return OK;
}

int
simpleScan(volatile unsigned int *data, int nwords)  // FIXME: Not using nwords
{
  int iroc = 0, ibank=0;

  memset((char *) &trigBank, 0, sizeof(trigBank));
  memset((char *) rocBank, 0, sizeof(rocBank));
  memset((char *) bankData, 0, sizeof(bankData));

  dataAddr = (unsigned long) data;

  /* Scan over to get Bank indices.. */
  if(simpleDebugMask & SIMPLE_SHOW_UNBLOCK)
    {
      printf("%s: Scan CODA Event for Banks\n",__FUNCTION__);
    }
  simpleScanCodaEvent(data);

  if(simpleDebugMask & SIMPLE_SHOW_UNBLOCK)
    {
      printf("%s: Start Banks for Events\n",__FUNCTION__);
    }

  /* Scan over to get event indices */
  for(iroc=0; iroc<SIMPLE_MAX_ROCS; iroc++)
    {
      /* Check if the rocBank exists */
      if((rocBank[iroc].length > 0))
	{
	  for(ibank=0; ibank<SIMPLE_MAX_BANKS; ibank++)
	    {
	      /* Check if the dataBank for that ROC exists */
	      if((rocBank[iroc].dataBank[ibank].length > 0))
		{
		  /* Scan it */
		  simpleScanBank(data, iroc, ibank);
		}
	    }
	}
    }

  return OK;
}

/**
 * @ingroup Unblock
 * @brief Pass over the CODA event to determine Bank types and indicies
 *
 * @param data Memory address of the data
 *
 * @return OK if successful, otherwise ERROR
 */
int
simpleScanCodaEvent(volatile unsigned int *data)
{
  int iword=0, nwords=0;
  bankHeader_t bh;
  int bank_type=0;

  /* First word should be the length of the CODA event */
  nwords = data[iword++];
  /* Next word should be the CODA Event header */
  bh.raw = data[iword++];

  if(bh.bf.type == EVIO_BANK)
    {
      /* Hopefully this is the start of the trigger bank */
      trigBank.length = data[iword++];
      trigBank.header.raw = data[iword++];
      trigBank.index = iword;

      if(trigBank.header.bf.type == EVIO_SEGMENT)
	{
	  if(simpleDebugMask & SIMPLE_SHOW_BANK_FOUND)
	    {
	      printf("[%6d  0x%08x] TRGB 0x%4x: Length = %d, nrocs = %d\n",
		     trigBank.index - 1, trigBank.header.raw,
		     trigBank.header.bf.type,
		     trigBank.length,
		     trigBank.header.bf.num);

	    }
	}
      else
	{
	  printf("%s: ERROR: 0x%08x Unexpected bank type 0x%x (%d)\n",
		 __func__, trigBank.header.raw,
		 trigBank.header.bf.type,
		 trigBank.header.bf.type);

	  return ERROR;
	}

      /* init number of rocs */
      trigBank.nrocs = 0;
      /* Index each trigger segment */
      while(iword < (trigBank.index + trigBank.length - 1))
	{
	  segmentHeader_t sh;
	  sh.raw = data[iword++];

	  if(simpleDebugMask & SIMPLE_SHOW_SEGMENT_FOUND)
	    {
	      printf("[%6d  0x%08x] SEGM %2d: type = 0x%x, length = %d\n",
		     iword - 1, sh.raw,
		     sh.bf.tag,
		     sh.bf.type,
		     sh.bf.num);
	    }

	  switch(sh.bf.type)
	    {
	    case EVIO_ULONG64:	  /* Event number ( + timestamp) segment */
	      {
		trigBank.segTime.index = iword;
		trigBank.segTime.header.raw = sh.raw;
		break;
	      }
	    case EVIO_USHORT16:	  /* Event type segment */
	      {
		trigBank.segEvType.index = iword;
		trigBank.segEvType.header.raw = sh.raw;
		break;
	      }
	    case EVIO_UINT32:	  /* ROC segment */
	      {
		trigBank.segRoc[sh.bf.tag].index = iword;
		trigBank.segRoc[sh.bf.tag].header.raw = sh.raw;
		trigBank.nrocs++;
		break;
	      }

	    default:
	      printf("%s: ERROR: [0x%08x] Unexpected Segment tag 0x%x (%d) in trigger bank\n",
		     __func__, sh.raw, sh.bf.tag, sh.bf.tag);
	      return ERROR;
	    }

	  /* Move past this segment */
	  iword += sh.bf.num;
	}
    }
  else
    {
      printf("%s: ERROR: 0x%08x Unexpected Bank type 0x%x (%d)\n",
	     __func__, bh.raw, bh.bf.type, bh.bf.type);
      return ERROR;
    }

  /* ROC Banks start here */
  while(iword<nwords)
    {
      bankHeader_t rocBankHeader;
      int rocBankLength = 0, rocID = 0;

      /* Index the ROC bank header */
      rocBankLength = data[iword++] - 1;
      rocBankHeader.raw = data[iword++];

      rocID = rocBankHeader.bf.tag;

      if(rocID > SIMPLE_MAX_ROCS)
	{
	  printf("%s: ERROR: rocID = %d. I cant handle rocIDs > %d\n",
		 __func__, rocID, SIMPLE_MAX_ROCS);
	  return ERROR;
	}

      rocBank[rocID].header.raw = rocBankHeader.raw;
      rocBank[rocID].index = iword;
      rocBank[rocID].length = rocBankLength;

      if(simpleDebugMask & SIMPLE_SHOW_BANK_FOUND)
	{
	  printf("[%6d  0x%08x] ROCB %2d: type = 0x%2x, Length = %d, blocklevel = %d\n",
		 rocBank[rocID].index - 1, rocBank[rocID].header.raw,
		 rocID,
		 rocBank[rocID].header.bf.type,
		 rocBank[rocID].length,
		 rocBank[rocID].header.bf.num);
	}

      switch(rocBank[rocID].header.bf.type)
	{
	case EVIO_BANK: /* Roc Bank is a Bank of Banks */
	  {
	    rocBank[rocID].nbanks = 0;

	    /* Inside the ROC bank.
	       Look for data banks and determine their lengths and indices */
	    while(iword < (rocBank[rocID].index + rocBank[rocID].length - 1))
	      {
		bankHeader_t dataBankHeader;
		int dataBankLength = 0, dataBankID = 0, dataBankIndex = 0;

		dataBankLength = data[iword++] - 1;
		dataBankHeader.raw = data[iword++];
		dataBankIndex  = iword;
		dataBankID = dataBankHeader.bf.tag;

		/* We save the bank header and length in the struct,
		   so .index and .length here refer to the data inside */
		rocBank[rocID].dataBank[dataBankID].length = dataBankLength;
		rocBank[rocID].dataBank[dataBankID].index  = dataBankIndex;
		rocBank[rocID].dataBank[dataBankID].header.raw = dataBankHeader.raw;
		rocBank[rocID].nbanks++;

#ifdef FIGUREITOUT
		if(ignoreUndefinedBanks)
		  {
		    // FIXME: need to check vs configured banks
		    if( rocBank[rocID].dataBank[dataBankID].ID )
		      {
			if(simpleDebugMask & SIMPLE_SHOW_IGNORED_BANKS)
			  {
			    printf("[%6d  0x%08x] IGNORED BANK 0x%2x: Type = 0x%x Num = 0x%x Length = %d\n",
				   dataBankIndex - 1, dataBankHeader.raw,
				   rocBank[rocID].dataBank[dataBankID].header.bf.tag,
				   rocBank[rocID].dataBank[dataBankID].header.bf.type,
				   rocBank[rocID].dataBank[dataBankID].header.bf.num,
				   rocBank[rocID].dataBank[dataBankID].length);
			  }

			/* Jump to next bank */
			iword += rocBank[rocID].dataBank[dataBankID].length;
			continue;
		      }
		  }
#endif

		if(simpleDebugMask & SIMPLE_SHOW_BANK_FOUND)
		  {
		    printf("[%6d  0x%08x] BANK 0x%2x: Type = 0x%x Num = 0x%x Length = %d\n",
			   dataBankIndex - 1, dataBankHeader.raw,
			   rocBank[rocID].dataBank[dataBankID].header.bf.tag,
			   rocBank[rocID].dataBank[dataBankID].header.bf.type,
			   rocBank[rocID].dataBank[dataBankID].header.bf.num,
			   rocBank[rocID].dataBank[dataBankID].length);
		  }

		/* Jump to next bank */
		iword += rocBank[rocID].dataBank[dataBankID].length;
	      }
	    break;
	  }

	case EVIO_UINT32:  /* Roc Bank is a Bank of uint32s, skipped for now */
	  {
	    printf("%s: ERROR: Bank type 0x%x not supported\n",
		   __FUNCTION__,bank_type);
	    return ERROR;
	    /* If there are Lengths and Headers defined by the user, use them to find the
	       indicies */
	    while(iword<nwords)
	      {
		/* FIXME: Will do this later... */
	      }

	    break;
	  }

	default:
	  printf("%s: ERROR: Bank type 0x%x not supported\n",
		 __FUNCTION__,bank_type);
	  return ERROR;
	}

    }
  return OK;
}

/**
 * @ingroup Unblock
 * @brief Scan through a ROC's bank to determine Event header indicies
 *
 *    This is the default method, if one is not specified with
 *    simpleConfigModule.  It uses the JLab Data Format Standard
 *
 * @param data       Memory address of the data
 * @param rocID      Which ROC bank to find the Bank
 * @param bankNumber Which Bank to index.
 *
 * @return OK if successful, otherwise ERROR
 */

int
simpleScanBank(volatile unsigned int *data, int rocID, int bankNumber)
{
  int rval=OK;
  int iword=0; /* Index of current word in *data */
  int current_event=0;
  int nwords = 0;
  int blkCounter=0; /* count of blocks within the data (one per module) */
  unsigned int slotNumber = 0; /* Set in block header, checked in block trailer */
  int userBankIndex;
  int endian = 0;
  jlab_data_word_t jdata;
  block_header_t bheader;
  block_trailer_t btrailer;
  event_header_t eheader;

  blkCounter = 0;

  /* Check if this rocID and bankNumber combo were found in simpleScanCodaEvent */
  if( (rocBank[rocID].header.bf.tag != rocID) ||
      (rocBank[rocID].dataBank[bankNumber].header.bf.tag != bankNumber) )
    {
      if(simpleDebugMask & SIMPLE_SHOW_BANK_NOT_FOUND)
	{
	  printf("%s: rocID = 0x%x, bankNumber = 0x%x NOT found\n",
		 __func__, rocID, bankNumber);
	}
      return -1;
    }

  iword = rocBank[rocID].dataBank[bankNumber].index;
  nwords = iword + rocBank[rocID].dataBank[bankNumber].length;

  bankData[rocID][bankNumber].rocID = rocID;
  bankData[rocID][bankNumber].bankID = bankNumber;

  userBankIndex = simpleFindConfigBankIndex(rocID, bankNumber);
  if(userBankIndex >= 0)
    {
      if(!uBank[userBankIndex].isBlocked)
	return 0;

      endian = uBank[userBankIndex].endian;
    }

  /* Index the Bank of Data.
     Looking for Block Headers, Event headers, and Block Trailers  */
  while(iword<nwords)
    {
      jdata.raw = data[iword];
      if(endian)
	jdata.raw = bswap_32(jdata.raw);

      if(jdata.bf.data_type_defining == 1)
	{
	  switch(jdata.bf.data_type_tag)
	    {
	    case BLOCK_HEADER: /* 0: BLOCK HEADER */
	      {
		bheader.raw = jdata.raw;

		blkCounter++; /* Increment block counter */

		bankData[rocID][bankNumber].evtCounter = 0; /* Initialize the event counter */
		slotNumber = bheader.bf.slot_number;

		bankData[rocID][bankNumber].blkIndex[slotNumber] = iword;
		bankData[rocID][bankNumber].blkLevel   = bheader.bf.number_of_events_in_block;

		if(simpleDebugMask & SIMPLE_SHOW_BLOCK_HEADER)
		  {
		    printf("[%6d  0x%08x] "
			   "BLOCK HEADER: slot %2d, block_number %3d, block_level %3d\n",
			   iword,
			   bheader.raw,
			   bheader.bf.slot_number,
			   bheader.bf.event_block_number,
			   bheader.bf.number_of_events_in_block);
		  }
		break;
	      }

	    case BLOCK_TRAILER: /* 1: BLOCK TRAILER */
	      {
		btrailer.raw = jdata.raw;
		bankData[rocID][bankNumber].blkTrailerIndex[slotNumber] = iword;

		if(simpleDebugMask & SIMPLE_SHOW_BLOCK_TRAILER)
		  {
		    printf("[%6d  0x%08x] "
			   "BLOCK TRAILER: slot %2d, nwords %d\n",
			   iword,
			   btrailer.raw,
			   btrailer.bf.slot_number,
			   btrailer.bf.words_in_block);
		  }

		/* Obtain the previous event length */
		if(bankData[rocID][bankNumber].evtCounter > 0)
		  {
		    current_event = bankData[rocID][bankNumber].evtCounter - 1;

		    bankData[rocID][bankNumber].evtLength[slotNumber][current_event] =
		      iword - bankData[rocID][bankNumber].evtIndex[slotNumber][current_event];
		  }

		/* Check the slot number to make sure this block
		   trailer is associated with the previous block
		   header */
		if(btrailer.bf.slot_number != slotNumber)
		  {
		    printf("[%6d  0x%08x] "
			   "ERROR: blockheader slot %d != blocktrailer slot %d\n",
			   iword,
			   btrailer.raw,
			   slotNumber,
			   btrailer.bf.slot_number);
		    rval = ERROR;
		  }

		/* Check the number of words vs. words counted within the block */
		if(btrailer.bf.words_in_block !=
		   (iword - bankData[rocID][bankNumber].blkIndex[slotNumber]+1) )
		  {
		    printf("[%6d  0x%08x] "
			   "ERROR: trailer #words %d != actual #words %d\n",
			   iword,
			   btrailer.raw,
			   btrailer.bf.words_in_block,
			   iword-bankData[rocID][bankNumber].blkIndex[slotNumber]+1);
		    rval = ERROR;
		  }

		slotNumber = 0; /* Initialize for next block */
		break;
	      }

	    case EVENT_HEADER: /* 2: EVENT HEADER */
	      {
		eheader.raw = jdata.raw;

		if(simpleDebugMask & SIMPLE_SHOW_EVENT_HEADER)
		  {
		    printf("[%6d  0x%08x] "
			   "EVENT HEADER: trigger number %d\n",
			   iword,
			   eheader.raw,
			   eheader.bf.event_number);

		  }

		if(slotNumber == 0)
		  {
		    printf("%s: ERROR.  Event Header Found. Slot Number = 0.\n",
			   __func__);
		    return ERROR;
		  }

		/* Add this slot to the slotMask */
		bankData[rocID][bankNumber].slotMask |= (1 << slotNumber);

		/* Obtain the previous event length */
		if(bankData[rocID][bankNumber].evtCounter > 0)
		  {
		    current_event = bankData[rocID][bankNumber].evtCounter - 1;

		    bankData[rocID][bankNumber].evtLength[slotNumber][current_event] =
		      iword - bankData[rocID][bankNumber].evtIndex[slotNumber][current_event];
		  }

		bankData[rocID][bankNumber].evtCounter++; /* increment event counter */
		current_event = bankData[rocID][bankNumber].evtCounter - 1;
		bankData[rocID][bankNumber].evtIndex[slotNumber][current_event] = iword;

		break;
	      }

	    default:
	      /* Ignore all other data types for now */
	      if(simpleDebugMask & SIMPLE_SHOW_OTHER)
		{
		  printf("(%3d) OTHER: 0x%08x\n",iword,data[iword]);
		}
	    } /* switch(data_type) */

	} /* if(jdata.bf.data_type_defining == 1) */

      iword++;

    } /* while(iword<nwords) */

  return rval;
}

#define CHECKROCID(x,y)				\
  {						\
    if(bankData[x][y].rocID != x)		\
      return -1;				\
  }


/* Data access routines */
int
simpleGetRocBanks(int rocID, int bankID, int *bankList)
{
  CHECKROCID(rocID,bankID);

  return 0;
}

/**
 * @ingroup Data Access
 * @brief Return the slotmask from the specified rocID and bankID
 *
 * @param rocID      Which ROC bank to find the slotmask
 * @param bankID     Which Bank to find the slotmask
 * @param *slotmask  Where to store the slotmask
 *
 * @return 1 if successful, otherwise ERROR
 */

int
simpleGetRocSlotmask(int rocID, int bankID, unsigned int *slotmask)
{
  CHECKROCID(rocID,bankID);

  *slotmask = bankData[rocID][bankID].slotMask;

  return 1;
}

/**
 * @ingroup Data Access
 * @brief Return the block level from the specified rocID and bankID
 *
 * @param rocID        Which ROC bank to find the block level
 * @param bankID       Which Bank to find the block level
 * @param *blockLevel  Where to store the block level
 *
 * @return 1 if successful, otherwise ERROR
 */

int
simpleGetRocBlockLevel(int rocID, int bankID, int *blockLevel)
{
  CHECKROCID(rocID, bankID);

  *blockLevel = bankData[rocID][bankID].blkLevel;

  return 1;
}

/**
 * @ingroup Data Access
 * @brief Return the block header from the specified rocID, bankID, and slot number
 *
 * @param rocID        Which ROC bank to find the block header
 * @param bankID       Which Bank to find the block header
 * @param slot         Which slot to find the block header
 * @param *header      Where to store the block header
 *
 * @return 1 if successful, otherwise ERROR
 */

int
simpleGetSlotBlockHeader(int rocID, int bankID, int slot, unsigned int *header)
{
  int index;
  unsigned int *bufPtr = (unsigned int *)dataAddr;

  CHECKROCID(rocID, bankID);

  if( (bankData[rocID][bankID].slotMask & (1 << slot)) == 0 )
     return -1;

  index = bankData[rocID][bankID].blkIndex[slot];
  *header = bufPtr[index];

  return 1;
}

/**
 * @ingroup Data Access
 * @brief Return the Event header from the specified rocID, bankID, and slot
 *        for the specified event within a block
 *
 * @param rocID        Which ROC bank to find the event header
 * @param bankID       Which Bank to find the event header
 * @param slot         Which slot to find the event header
 * @param evt          Which event within the block to find the event header
 * @param *header  Where to store the event header
 *
 * @return 1 if successful, otherwise ERROR
 */

int
simpleGetSlotEventHeader(int rocID, int bankID, int slot, int evt, unsigned int *header)
{
  int index;
  unsigned int *bufPtr = (unsigned int *)dataAddr;

  CHECKROCID(rocID, bankID);

  if( (bankData[rocID][bankID].slotMask & (1 << slot)) == 0 )
     return -1;

  index = bankData[rocID][bankID].evtIndex[slot][evt];
  *header = bufPtr[index];

  return 1;
}

/**
 * @ingroup Data Access
 * @brief Return the buffer to the part of the data with specified rocID,
 *         bankID, and slot number, and event of the block.
 *
 * @param rocID        Which ROC bank to find the buffer
 * @param bankID       Which Bank to find the buffer
 * @param slot         Which slot to find the buffer
 * @param evt          Which event within the block to find the buffer
 * @param **buffer     Where to store the address of the buffer
 *
 * @return Length of the buffer if successful, otherwise ERROR
 */

int
simpleGetSlotEventData(int rocID, int bankID, int slot, int evt, unsigned int **buffer)
{
  int length = 0;
  unsigned long addr = 0;

  CHECKROCID(rocID, bankID);

  if( (bankData[rocID][bankID].slotMask & (1 << slot)) == 0 )
     return -1;

  addr = (unsigned long)((unsigned int *)dataAddr + bankData[rocID][bankID].evtIndex[slot][evt]);
  *buffer = (unsigned int *) addr;

  length = bankData[rocID][bankID].evtLength[slot][evt];

  return length;
}

/**
 * @ingroup Data Access
 * @brief Return the block trailer from the specified rocID, bankID, and slot number
 *
 * @param rocID        Which ROC bank to find the block trailer
 * @param bankID       Which Bank to find the block trailer
 * @param slot         Which slot to find the block trailer
 * @param *trailer     Where to store the block trailer
 *
 * @return 1 if successful, otherwise ERROR
 */

int
simpleGetSlotBlockTrailer(int rocID, int bankID, int slot, unsigned int *trailer)
{
  int index;
  unsigned int *bufPtr = (unsigned int *)dataAddr;

  CHECKROCID(rocID, bankID);

  if( (bankData[rocID][bankID].slotMask & (1 << slot)) == 0 )
     return -1;

  index = bankData[rocID][bankID].blkTrailerIndex[slot];
  *trailer = bufPtr[index];

  return 1;
}

/**
 * @ingroup Data Access
 * @brief Return the buffer to the Trigger Bank's Run number and Timestamp segment
 *
 * @param **buffer     Where to store the address of the buffer
 *
 * @return Length of the buffer if successful, otherwise ERROR
 */

int
simpleGetTriggerBankTimeSegment(unsigned long long **buffer)
{
  int len = 0;
  unsigned long addr = (unsigned long)((unsigned int *)dataAddr + trigBank.segTime.index);

  *buffer = (unsigned long long int *)addr;
  len = trigBank.segTime.header.bf.num >> 1;

  return len;
}

/**
 * @ingroup Data Access
 * @brief Return the buffer to the Trigger Bank's Event Type segment
 *
 * @param **buffer     Where to store the address of the buffer
 *
 * @return Length of the buffer if successful, otherwise ERROR
 */

int
simpleGetTriggerBankTypeSegment(unsigned short **buffer)
{
  int len = 0;
  unsigned long addr = (unsigned long)((unsigned int *)dataAddr + trigBank.segEvType.index);

  *buffer = (unsigned short *)addr;
  len = trigBank.segEvType.header.bf.num << 1;

  return len;
}

/**
 * @ingroup Data Access
 * @brief Return the buffer to the Trigger Bank's ROC segment
 *
 * @param rocID        Which roc ID to find the buffer
 * @param **buffer     Where to store the address of the buffer
 *
 * @return Length of the buffer if successful, otherwise ERROR
 */

int
simpleGetTriggerBankRocSegment(int rocID, unsigned int **buffer)
{
  int len = 0;
  unsigned long addr = 0;

  if(trigBank.segRoc[rocID].header.bf.tag != rocID)
    {
      return -1;
    }

  addr = (unsigned long)((unsigned int *)dataAddr + trigBank.segRoc[rocID].index);
  *buffer = (unsigned int *) addr;

  len = trigBank.segRoc[rocID].header.bf.num;

  return len;
}
