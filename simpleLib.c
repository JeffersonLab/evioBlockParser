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
#include "simpleLib.h"

#ifndef ERROR
#define ERROR -1
#endif

#ifndef OK
#define OK 0
#endif

typedef void (*VOIDFUNCPTR) ();

#define BT_BANK_ty 0x10
#define BT_SEGMENT_ty 0x20
#define BT_UB1_ty  0x07
#define BT_UI2_ty  0x05
#define BT_UI4_ty  0x01


/* Global Variables */
simpleEndian       in_endian=SIMPLE_LITTLE_ENDIAN, out_endian=SIMPLE_LITTLE_ENDIAN;    /* Incoming and Output Endian-ness */
simpleDebug        simpleDebugMask=0;
simpleModuleConfig sModConfig[SIMPLE_MAX_MODULE_TYPES];

/* Trigger event number, start at 1 */
unsigned int simpleTriggerNumber=1;

/* CODA Event Banks Info, updated for each block */
codaEventBankInfo cBank[SIMPLE_MAX_BANKS];
unsigned int      cTriggerBank=0;
unsigned int      cTriggerBankIndex=0;
int               nbanks=0;                      /* Number of banks found */

/* Trigger Bank of Segment */
trigSegInfo trigBank;

/* ROC Banks */
rocBankInfo rocBank[SIMPLE_MAX_ROCS];
int nRocs;

int               ignoreUndefinedBanks=0;        /* Default is false */

/* User defined Banks for separate modules, configured before run */
simpleModuleConfig mConf[SIMPLE_MAX_BANKS];
int                nmconfs=0;

/* Trigger module data (TS, TI) */
int      tEventCounter=0;
trigData tData[SIMPLE_MAX_BLOCKLEVEL+1];
int      tEventSyncFlag; /* Only the last event in the block will be flagged, if the block contains a syncFlag */

/* Payload module data (fADC250, fADC125, f1TDC) */



int     blkCounter=0; /* count of blocks within the data (one per module) */
modData mData[SIMPLE_MAX_NBLOCKS+1];
int     mBankIndex[SIMPLE_MAX_BANKS]; /* Index of module data in mData */
int     mBankLength[SIMPLE_MAX_BANKS]; /* Length of module data in mData */

/* Other data, to be copied to only one event... or to every event in the block */
otherBankInfo     otherBank[SIMPLE_MAX_BANKS];
unsigned int      notherBank=0;

/* Output Coda Events indices and lengths */
codaEventBankInfo outBank[SIMPLE_MAX_BLOCKLEVEL+1];
int               outBlocks=0;


/* Structure defaults */
const simpleModuleConfig MODULE_CONFIG_DEFAULTS =
  {
    -1,   /* type */
    -1,   /* ID */
    0,    /* module_header */
    0xFFFFFFFF, /* header_mask */
    (VOIDFUNCPTR)simpleFirstPass, /* firstPassRoutine */
    (VOIDFUNCPTR)simpleSecondPass /* secondPassRoutine */
  };

const codaEventBankInfo EVENT_BANK_INFO_DEFAULTS =
  {
    -1,  /* length */
    -1,  /* ID */
    -1   /* index */
  };

const trigData TRIG_DATA_DEFAULTS =
  {
    -1, /* type */
    -1, /* index */
    -1, /* length */
    -1  /* number */
  };

/*
   Bank array utils
*/

/* Bank array utils */
int isA_cBank(int bankID);
int isA_mConf(int bankID);
int isA_mTriggerBank(int bankID);
int isA_otherBank(int bankID);
int isA_otherOnceBank(int bankID);

int
simpleInit()
{
  int imod=0;

  for(imod=0; imod<SIMPLE_MAX_MODULE_TYPES; imod++)
    {
      sModConfig[imod] = MODULE_CONFIG_DEFAULTS;
    }

  memset((char*)mConf,0,sizeof(mConf));

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the expected incoming and output endian-ness
 *
 * @param in_endian Expected incoming endian-ness
 *     - 0: Little endian
 *     - !0: Big endian
 * @param out_endian Set Output endian-ness
 *     - 0: Little endian
 *     - !0: Big endian
 *
 */
void
simpleConfigEndianInOut(int in_end, int out_end)
{
  if(in_end)
    {
      in_endian = SIMPLE_BIG_ENDIAN;
    }
  else
    {
      in_endian = SIMPLE_LITTLE_ENDIAN;
    }

  if(out_end)
    {
      out_endian = SIMPLE_BIG_ENDIAN;
    }
  else
    {
      out_endian = SIMPLE_LITTLE_ENDIAN;
    }
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
 * @brief Configure Module Prescription for Unblocking
 *
 * @param ID Bank ID that contains the module data
 * @param firstPassRoutine Routine to call for first pass processing
 * @param secondPassRoutine Routine to call for second pass processing
 *
 * @return OK if successful, otherwise ERROR
 */

int
simpleConfigModule(int type, int ID, void *firstPassRoutine, void *secondPassRoutine)
{
  if(type>=MAX_MODID)
    {
      printf("%s: ERROR: Invalid type (%d)\n",__FUNCTION__,type);
      return ERROR;
    }

  if(firstPassRoutine==NULL)
    {
      if((type==MODID_TI) || (type==MODID_TS))
	{
	  firstPassRoutine=simpleTriggerFirstPass;
	}
      else
	{
	  firstPassRoutine=simpleFirstPass;
	}
    }

  if(secondPassRoutine==NULL)
    {
      secondPassRoutine=simpleSecondPass;
    }

  mConf[nmconfs].ID                = ID;
  mConf[nmconfs].type              = type;
  mConf[nmconfs].firstPassRoutine  = firstPassRoutine;
  mConf[nmconfs].secondPassRoutine = secondPassRoutine;

  nmconfs++;

  if((type==MODID_OTHER)||(type==MODID_OTHER_ONCE))
    {
      otherBank[notherBank].ID = ID;
      if(type==MODID_OTHER_ONCE)
	otherBank[notherBank].once = 1;

      notherBank++;
    }

  return OK;
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
simpleUnblock(volatile unsigned int *idata, volatile unsigned int *sdata, int nwords)
{
  int ibank=0;

  /* Scan over to get initial indices.. */
  if(simpleDebugMask & SIMPLE_SHOW_UNBLOCK)
    {
      printf("%s: Scan CODA Event\n",__FUNCTION__);
    }
  simpleScanCodaEvent(idata);

  /* Perform a first pass to organize trigger event data */
  if(simpleDebugMask & SIMPLE_SHOW_UNBLOCK)
    {
      printf("%s: Start trigger first pass\n",__FUNCTION__);
    }

  /* Treat the trigger bank, if it was found */
  if(cTriggerBank != 0)
    {
      simpleTriggerFirstPass(idata, cBank[cTriggerBankIndex].index,
			     cBank[cTriggerBankIndex].length);
    }

#ifdef SKIP
  /* Perform a first pass to organize module event data */
  if(simpleDebugMask & SIMPLE_SHOW_UNBLOCK)
    {
      printf("%s: Start module first pass\n",__FUNCTION__);
    }
  for(ibank=0; ibank<SIMPLE_MAX_BANKS; ibank++)
    {
      if((ibank != cTriggerBankIndex) && (cBank[ibank].ID>0)) /* Trigger Banks already handled */
	{
	  if( !isA_otherBank(cBank[ibank].ID) )
	    { /* OTHER banks will be taken care of in the second pass */
	      simpleFirstPass(idata, cBank[ibank].index, cBank[ibank].length, ibank);
	    }
	}
    }
#endif

  /* Perform Second Pass that puts the events in a temporary buffer */
  if(simpleDebugMask & SIMPLE_SHOW_UNBLOCK)
    {
      printf("%s: Start second pass\n",__FUNCTION__);
    }
  simpleSecondPass(sdata,idata,0);

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
  int bank_type=0, bank_length = 0;
  int ibank=0;

  nbanks=0;
  memset((char*)cBank, 0, sizeof(cBank));

  memset((char*)&trigBank, 0, sizeof(trigBank));

  /* First word should be the length of the CODA event */
  nwords = data[iword++];
  /* Next word should be the CODA Event header */
  bank_type = (data[iword++] & 0xFF00)>>8;

  if(bank_type == EVIO_BANK)
    {
      unsigned int triggerBankHeader = 0;
      int trigBankNumRocs = 0, headerIndex = 0;

      /* Hopefully this is the start of the trigger bank */
      trigBank.length = data[iword++];
      headerIndex = iword;
      triggerBankHeader = data[iword++];
      if((triggerBankHeader & 0xFF00)>>8 == EVIO_SEGMENT)
	{
	  trigBank.index = headerIndex;
	  trigBankNumRocs = triggerBankHeader & 0xFF;
	  trigBank.type = (triggerBankHeader & 0xFFFF0000) >> 16;

	if(simpleDebugMask & SIMPLE_SHOW_BANK_FOUND)
	  {
	    printf("[%6d  0x%08x] TRGB 0x%4x: Length = %d, N = %d\n",
		   headerIndex, triggerBankHeader,
		   trigBank.type,
		   trigBank.length,
		   trigBankNumRocs);

	  }
	}
      else
	{
	  printf("%s: ERROR: 0x%08x Unexpected bank type 0x%x (%d)\n",
		 __func__, triggerBankHeader,
		 (triggerBankHeader & 0xFF00)>>8,
		 (triggerBankHeader & 0xFF00)>>8);
	  return ERROR;
	}

      /* Process each trigger segment */
      while(iword < (trigBank.index + trigBank.length))
	{
	  unsigned int segHeader = 0;
	  int segID = 0, segNumber = 0, segLength = 0;

	  headerIndex = iword;
	  segHeader = data[iword++];
	  segID = (segHeader & 0xFF000000) >> 24;
	  segNumber = (segHeader & 0x00FF0000) >> 16;
	  segLength = (segHeader & 0xFFFF);

	  if(simpleDebugMask & SIMPLE_SHOW_SEGMENT_FOUND)
	    {
	      printf("[%6d  0x%08x] SEGM %2d: Length = %d, ID = 0x%x (%d)\n",
		     headerIndex, segHeader,
		     segNumber,
		     segLength,
		     segID, segID);
	    }

	  switch(segNumber)
	    {
	    case 0xA:	  /* Event number ( + timestamp) segment */
	      {
		trigBank.segTime.index = iword;
		trigBank.segTime.length = segHeader & 0xFFFF;
		break;
	      }
	    case 0x5:	  /* Event type segment */
	      {
		trigBank.segEvType.index = iword;
		trigBank.segEvType.length = segHeader & 0xFFFF;
		break;
	      }
	    case 0x1:	  /* ROC segment */
	      {
		trigBank.segRoc[segID].index = iword;
		trigBank.segRoc[segID].length = segHeader & 0xFFFF;
		break;
	      }

	    default:
	      printf("%s: ERROR: [0x%08x] Unexpected Segment 0x%x (%d) in trigger bank\n",
		     __func__, segHeader, segNumber, segNumber);
	      return ERROR;
	    }
	  iword += segLength;
	}
    }
  else
    {
      printf("%s: ERROR: Unexpected Bank type 0x%x (%d)\n",
	     __func__, bank_type, bank_type);
      return ERROR;
    }

  /* ROC Banks start here */
  while(iword<nwords)
    {
      unsigned int rocBankHeader = 0;
      int rocBankLength = 0, rocBankIndex = 0, kType = 0, rocID = 0;


      /* Process the ROC bank header */
      rocBankLength = data[iword++];
      rocBankIndex  = iword;
      rocBankHeader = data[iword++];
      rocID = (rocBankHeader & 0x0FFF0000) >> 16;

      if(rocID > SIMPLE_MAX_ROCS)
	{
	  printf("%s: ERROR: rocID = %d. I cant handle rocIDs > %d\n",
		 __func__, rocID, SIMPLE_MAX_ROCS);
	  return ERROR;
	}

      rocBank[rocID].index = rocBankIndex;
      rocBank[rocID].length = rocBankLength;
      rocBank[rocID].type = (rocBankHeader & 0xFF00) >> 8;
      rocBank[rocID].blockLevel = (rocBankHeader & 0xFF);

      if(simpleDebugMask & SIMPLE_SHOW_BANK_FOUND)
	{
	  printf("[%6d  0x%08x] ROCB %2d: Length = %d, type = 0x%2x blocklevel = %d\n",
		 rocBank[rocID].index, data[rocBank[rocID].index],
		 rocID,
		 rocBank[rocID].length,
		 rocBank[rocID].type,
		 rocBank[rocID].blockLevel);
	}

      switch(rocBank[rocID].type)
	{
	case EVIO_BANK:
	  {
	    /* Determine lengths and indicies of each bank */
	    while(iword < (rocBank[rocID].index + rocBank[rocID].length))
	      {
		int dataBankLength = 0, dataBankID = 0;
		unsigned int dataBankHeader = 0;

		dataBankLength = data[iword++];
		dataBankHeader = data[iword];
		dataBankID = (dataBankHeader & BANK_ID_MASK)>>16;

		rocBank[rocID].dataBank[dataBankID].length = dataBankLength;
		rocBank[rocID].dataBank[dataBankID].index  = iword;
		rocBank[rocID].dataBank[dataBankID].ID     = dataBankID;

		if(ignoreUndefinedBanks)
		  if( !isA_mConf(rocBank[rocID].dataBank[dataBankID].ID) )
		    {
		      if(simpleDebugMask & SIMPLE_SHOW_IGNORED_BANKS)
			{
			  printf("[%6d  0x%08x] IGNORED BANK %2d: Length = %d\n",
				 rocBank[rocID].dataBank[dataBankID].index, dataBankHeader,
				 rocBank[rocID].dataBank[dataBankID].ID,
				 rocBank[rocID].dataBank[dataBankID].length);
			}
		      iword += rocBank[rocID].dataBank[dataBankID].length; /* Jump to next bank */
		      continue;
		    }

		if(simpleDebugMask & SIMPLE_SHOW_BANK_FOUND)
		  {
		    printf("[%6d  0x%08x] BANK %2d: Length = %d\n",
			   rocBank[rocID].dataBank[dataBankID].index, dataBankHeader,
			   rocBank[rocID].dataBank[dataBankID].ID,
			   rocBank[rocID].dataBank[dataBankID].length);
		  }

		iword += rocBank[rocID].dataBank[dataBankID].length; /* Jump to next bank */
	      }
	    break;
	  }

	case BT_UI4_ty:
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
 * @brief Pass through a ROC's bank to determine Event header indicies
 *
 *    This is the default method, if one is not specified with
 *    simpleConfigModule.  It uses the JLab Data Format Standard
 *
 * @param data Memory address of the data
 * @param nwords The number of words that comprises the data
 *
 * @return OK if successful, otherwise ERROR
 */

int
simpleFirstPass(volatile unsigned int *data, int rocID, int bankNumber)
{
  int rval=OK;
  int iword=0; /* Index of current word in *data */
  int data_type=0;
  int current_slot=0, block_level=0, block_number=0;
  int block_words=0;
  int current_block=0, current_event=0;
  int iblock=0;
  int startIndex = 0, nwords = 0; // FIXME: OLD ARGUMENTS

  /* Re-initialized block counter */
  blkCounter = 0;

  iword = startIndex;
  nwords += startIndex;

  while(iword<nwords)
    {
      if( ((data[iword] & DATA_TYPE_DEFINING_MASK)>>31) == 1)
	{
	  data_type = (data[iword] & DATA_TYPE_MASK)>>27;
	  if(data_type>16)
	    {
	      printf("%s: ERROR: Undefined data type (%d). At Index %d\n",
		     __FUNCTION__,data_type,iword);
	      rval = ERROR;
	    }
	  else
	    {
	      switch(data_type)
		{
		case BLOCK_HEADER: /* 0: BLOCK HEADER */
		  {
		    current_slot = (data[iword] & BLOCK_HEADER_SLOT_MASK)>>22;
		    block_number = (data[iword] & BLOCK_HEADER_BLK_NUM_MASK)>>8;
		    block_level = (data[iword] & BLOCK_HEADER_BLK_LVL_MASK)>>0;

		    blkCounter++; /* Increment block counter */
		    current_block = blkCounter - 1;
		    mData[current_block].evtCounter = 0; /* Initialize the event counter */

		    mData[current_block].slotNumber = current_slot;
		    mData[current_block].blkIndex   = iword;


		    if(simpleDebugMask & SIMPLE_SHOW_BLOCK_HEADER)
		      {
			printf("(%3d) BLOCK HEADER: slot %2d, block_number %3d, block_level %3d\n",
			       iword,
			       current_slot,block_number,block_level);
		      }
		    break;
		  }

		case BLOCK_TRAILER: /* 1: BLOCK TRAILER */
		  {
		    current_slot = (data[iword] & BLOCK_TRAILER_SLOT_MASK)>>22;
		    block_words  = (data[iword] & BLOCK_TRAILER_NWORDS)>>0;

		    if(simpleDebugMask & SIMPLE_SHOW_BLOCK_TRAILER)
		      {
			printf("(%3d) BLOCK TRAILER: slot %2d, nwords %d\n",
			       iword,
			       current_slot,block_words);
		      }

		    current_block = blkCounter-1;

		    /* Obtain the previous event length */
		    if(mData[current_block].evtCounter > 0)
		      {
			current_event = mData[current_block].evtCounter - 1;

			mData[current_block].evtLength[current_event] =
			  iword - mData[current_block].evtIndex[current_event];
		      }

		    /* Check the slot number to make sure this block
		       trailer is associated with the previous block
		       header */
		    if(current_slot != mData[current_block].slotNumber)
		      {
			printf("(%3d) ERROR: blockheader slot %d != blocktrailer slot %d\n",
			       iword,
			       mData[current_block].slotNumber,current_slot);
			rval = ERROR;
		      }

		    /* Check the number of words vs. words counted within the block */
		    if(block_words != (iword - mData[current_block].blkIndex+1) )
		      {
			printf("(%3d) ERROR: trailer #words %d != actual #words %d\n",
			       iword,
			       block_words,iword-mData[current_block].blkIndex+1);
			rval = ERROR;
		      }
		    break;
		  }

		case EVENT_HEADER: /* 2: EVENT HEADER */
		  {
		    if(simpleDebugMask & SIMPLE_SHOW_EVENT_HEADER)
		      {
			printf("(%3d) EVENT HEADER: trigger number %d\n"
			       ,iword,
			       (data[iword] & EVENT_HEADER_EVT_NUM_MASK)>>0);

		      }
		    current_block = blkCounter-1;

		    /* Obtain the previous event length */
		    if(mData[current_block].evtCounter > 0)
		      {
			current_event = mData[current_block].evtCounter - 1;

			mData[current_block].evtLength[current_event] =
			  iword - mData[current_block].evtIndex[current_event];
		      }

		    mData[current_block].evtCounter++; /* increment event counter */
		    current_event = mData[current_block].evtCounter - 1;
		    mData[current_block].evtIndex[current_event] = iword;

		    break;
		  }

		default:
		  /* Ignore all other data types for now */
		    if(simpleDebugMask & SIMPLE_SHOW_EVENT_HEADER)
		      {
			printf("(%3d) OTHER: 0x%08x\n",iword,data[iword]);
		      }
		} /* switch(data_type) */

	    } /* if(data_type>16) {} else */

	} /* if( ((data[iword] & DATA_TYPE_DEFINING_MASK)>>31) == 1) */

      iword++;

    } /* while(iword<nwords) */

  /* Check for consistent number of events for each block */
  for(iblock=1; iblock<blkCounter; iblock++)
    {
      if(mData[iblock].evtCounter != mData[0].evtCounter)
	{
	  printf("ERROR: Event Count (block %d): (%3d) != Event Counter (block 0): (%3d)\n",
		 iblock, mData[iblock].evtCounter, mData[0].evtCounter);
	  rval = ERROR;
	}
    }

  return rval;
}

/**
 * @ingroup Unblock
 * @brief First pass through trigger data to determine event types and indicies.
 *
 *    This is the default trigger first pass algorithm, if one is not specified with
 *    simpleConfigModule.  It uses the JLab Data Format Standard
 *
 * @param data Memory address of the data
 * @param nwords The number of words that comprises the data
 *
 * @return OK if successful, otherwise ERROR
 */
int
simpleTriggerFirstPass(volatile unsigned int *data, int start_index, int nwords)
{
  int rval=OK;
  int iword=0; /* Index of current word in *data */
  int data_type=0;
  int current_slot=0, block_level=0, block_number=0;
  int evHasTimestamps=0, nevents=0;
  int block_words=0;
  int ievt=0;

  iword = start_index;
  nwords += start_index;

  while(iword<nwords)
    {

      if( ((data[iword] & DATA_TYPE_DEFINING_MASK)>>31) == 1)
	{
	  data_type = (data[iword] & DATA_TYPE_MASK)>>27;
	  if(data_type>16)
	    {
	      printf("%s: ERROR: Undefined data type (%d). At Index %d\n",
		     __FUNCTION__,data_type,iword);
	      rval = ERROR;
	    }
	  else
	    {
	      switch(data_type)
		{
		case BLOCK_HEADER:
		  {
		    current_slot = (data[iword] & BLOCK_HEADER_SLOT_MASK)>>22;
		    block_level  = (data[iword] & BLOCK_HEADER_BLK_LVL_MASK)>>0;
		    block_number = (data[iword] & BLOCK_HEADER_BLK_NUM_MASK)>>8;

		    tEventCounter = 0; /* Initialize the trig event counter */

		    if(simpleDebugMask & SIMPLE_SHOW_BLOCK_HEADER)
		      {
			printf("[%3d] %08x BLOCK HEADER: slot %2d, block_number %3d, block_level %3d\n",
			       iword,data[iword],
			       current_slot,block_number,block_level);
		      }

		    iword++;
		    /* Next word is the Block Header #2 */
		    if((data[iword] & TRIG_HEADER2_ID_MASK) != TRIG_HEADER2_ID_MASK)
		      {
			printf("[%3d] ERROR: Invalid Trigger Blockheader #2 (0x%08x)\n",
			       iword,
			       data[iword]);
			rval = ERROR;
		      }
		    else
		      {
			evHasTimestamps = (data[iword] & TRIG_HEADER2_HAS_TIMESTAMP_MASK)>>16;
			nevents         = (data[iword] & TRIG_HEADER2_NEVENTS_MASK);

			if(nevents != block_level)
			  {
			    printf("[%3d] %08x ERROR: Trigger Blockheader #2 nevents (%d) != blocklevel (%d)\n",
				   iword, data[iword],
				   nevents, block_level);
			    rval = ERROR;
			  }

			iword++;
			/* Now come the event data */
			for(ievt=0; ievt<nevents; ievt++)
			  {
			    tData[ievt].type   = (data[iword] & TRIG_EVENT_HEADER_TYPE_MASK)>>24;
			    tData[ievt].length = (data[iword] & TRIG_EVENT_HEADER_WORD_COUNT_MASK) + 1;
			    tData[ievt].index  = iword;
			    tData[ievt].number = (data[iword+1] & 0xff);

			    if(simpleDebugMask & SIMPLE_SHOW_EVENT_HEADER)
			      {
				printf("[%3d] %08x TRIG EVENT HEADER: event %3d  type %2d, number = %d\n",
				       iword, data[iword],
				       ievt,
				       tData[ievt].type,
				       tData[ievt].number);
			      }
			    iword += tData[ievt].length;
			    tEventCounter++;
			  }
		      }
		    break;
		  }

		case BLOCK_TRAILER:
		  {
		    current_slot = (data[iword] & BLOCK_TRAILER_SLOT_MASK)>>22;
		    block_words  = (data[iword] & BLOCK_TRAILER_NWORDS)>>0;

		    if(simpleDebugMask & SIMPLE_SHOW_BLOCK_TRAILER)
		      {
			printf("[%3d] %08x TRIG BLOCK TRAILER: slot %2d, nwords %d\n",
			       iword, data[iword],
			       current_slot,block_words);
		      }

		    break;
		  }

		case FILLER:
		default:
		  break;
		} /* switch(data_type) */

	    } /* if(data_type>16) else */
	} /* if( ((data[iword] & DATA_TYPE_DEFINING_MASK)>>31) == 1) */

      iword++;
    } /* while(iword<nwords) */

  return rval;
}

int
simpleTriggerFirstPass_oldTI(volatile unsigned int *data, int start_index, int nwords)
{
  int rval=OK;
  int iword=0; /* Index of current word in *data */
  int current_slot=0, block_level=0, block_number=0;
  int nevents=0;
  int ievt=0;

  iword = start_index;
  nwords += start_index;

  while(iword<nwords)
    {
      /* Old data format Blocklevel=1, event 3, crateID = 0x01
	[  4] Trigger Bank word 0x10420201  Block header #1
	[  5] Trigger Bank word 0x0f012001  Block header #2
	[  6] Trigger Bank word 0x00010002   Event data word 1
	[  7] Trigger Bank word 0x00000003   Event data word 2
	[  8] Trigger Bank word 0x35032cb1   Event data word 3
	[  9] Trigger Bank word 0x965a0002    Dummy word
	[ 10] Trigger Bank word 0x965a0002    Dummy word
	[ 11] Trigger Bank word 0x20000004  Block Trailer
	[ 12] Trigger Bank word 0x0000001b
      */
      if( ((data[iword] & 0xF0000000)>>28) == 1) /* Block Header #1 */
	{
	  current_slot = (data[iword] & 0x007F0000)>>16;
	  block_level  = (data[iword] & 0x000000FF)>>0;
	  block_number = (data[iword] & 0x0000FF00)>>8;

	  iword++;
	  if( (data[iword] & 0xFFFFFF00) != 0x0F012000) /* Block Header #2 */
	    {
	      printf("%s: ERROR: Unexpected data word 0x%08x at index %d\n",
		     __FUNCTION__,data[iword],iword);
	      rval = ERROR;
	    }

	  nevents = block_level;

	  tEventCounter = 0; /* Initialize the trig event counter */

	  iword++;
	  /* Now come the event data */
	  for(ievt=0; ievt<nevents; ievt++)
	    {
	      tData[ievt].type   = (data[iword] & 0xFF000000)>>24;
	      if((tData[ievt].type==0) | (tData[ievt].type>15))
		tData[ievt].type = 15;

	      tData[ievt].length = (data[iword] & 3) + 1;
	      tData[ievt].index  = iword;

	      tData[ievt].number = (data[iword+1] & 0xffffffff)+1;

	      if(simpleDebugMask & SIMPLE_SHOW_EVENT_HEADER)
		{
		  printf("[%3d] %08x TRIG EVENT HEADER: event %3d  type %2d, number = %d\n",
			 iword, data[iword],
			 ievt,
			 tData[ievt].type,
			 tData[ievt].number);
		}
	      iword += tData[ievt].length;
	      tEventCounter++;
	    }

	}
      iword++;

    } /* while(iword<nwords) */

  return rval;
}

/* Macros for opening/closing Events and Banks. */
#define EOPEN(bnum, btype, bSync, evnum) {				\
    StartOfTrigEvent = (OUTp);						\
    *(++(OUTp)) =							\
      ((bSync & 0x1)<<24) | ((bnum) << 16) | ((btype##_ty) << 8) | (0xff & evnum); \
    (OUTp)++;}

#define ECLOSE {							\
    *StartOfTrigEvent = (unsigned int) (((char *) (OUTp)) - ((char *) StartOfTrigEvent)); \
    if ((*StartOfTrigEvent & 1) != 0) {					\
      (OUTp) = ((unsigned int *)((char *) (OUTp))+1);			\
      *StartOfTrigEvent += 1;						\
    };									\
    if ((*StartOfTrigEvent & 2) !=0) {					\
      *StartOfTrigEvent = *StartOfTrigEvent + 2;				\
      (OUTp) = ((unsigned int *)((short *) (OUTp))+1);;			\
    };									\
    *StartOfTrigEvent = ( (*StartOfTrigEvent) >> 2) - 1;};

#define BOPEN(bnum, btype, code) {					\
    unsigned int *StartOfBank;						\
    StartOfBank = (OUTp);						\
    *(++(OUTp)) = (((bnum) << 16) | (btype##_ty) << 8) | (code);	\
    ((OUTp))++;

#define BCLOSE								\
  *StartOfBank = (unsigned int) (((char *) (OUTp)) - ((char *) StartOfBank)); \
  if ((*StartOfBank & 1) != 0) {					\
    (OUTp) = ((unsigned int *)((char *) (OUTp))+1);			\
    *StartOfBank += 1;							\
  };									\
  if ((*StartOfBank & 2) !=0) {						\
    *StartOfBank = *StartOfBank + 2;					\
    (OUTp) = ((unsigned int *)((short *) (OUTp))+1);;			\
  };									\
  *StartOfBank = ( (*StartOfBank) >> 2) - 1;};


/**
 * @ingroup Unblock
 * @brief Second pass through data to determine header indicies.
 *
 *    This is the default second pass algorithm, if one is not specified with
 *    simpleConfigModule.  It uses the JLab Data Format Standard
 *
 * @param data Memory address of the data
 * @param nwords The number of words that comprises the data
 *
 * @return OK if successful, otherwise ERROR
 */

int
simpleSecondPass(volatile unsigned int *odata, volatile unsigned int *idata, int in_nwords)
{
  int iev=0, iblk=0, iword=0, nOUT=0;
  int ibank=0;
  unsigned int *OUTp;
  unsigned int *StartOfTrigEvent;
  int evtype=0, syncFlag=0, evnumber=0;

  OUTp = (unsigned int *)&odata[0];

  outBlocks = 0;

  /* Loop over events */
  for(iev=0; iev<tEventCounter; iev++)
    {

      /* Get the Event Type, syncFlag */
      evtype = tData[iev].type;

      /* The sync event is the last event in the block (marked with the syncFlag) */
      if(iev==(blkCounter-1))
	syncFlag = tEventSyncFlag;
      else
	syncFlag = 0;

      evnumber = simpleTriggerNumber++;

      /* Open the event */
      evtype &= 0xF; /* CODA2 event type limit */
/*       if(evtype==0) evtype = 0xF; /\* Make sure these get through too *\/ */
      EOPEN(evtype,BT_BANK,syncFlag,tData[iev].number);

      /* Insert Trigger Bank first */
      if(simpleDebugMask & SIMPLE_SHOW_SECOND_PASS)
	{
	  printf("%s: %5d  Out Data.. index = %d  length = %d\n",__FUNCTION__,
		 tData[iev].number,
		 tData[iev].index,
		 tData[iev].length);
	}
      outBank[iev].index = nOUT;

      /* Insert the trigger bank */
      if(cTriggerBank != 0)
	{
	  BOPEN(cTriggerBank, BT_UI4, 0);
	  for(iword=tData[iev].index;
	      iword<(tData[iev].index+tData[iev].length-1);
	      iword++)
	    {
	      if(simpleDebugMask & SIMPLE_SHOW_SECOND_PASS)
		{
		  printf("Trig Data  %5d: 0x%08x\n",iword,idata[iword]);
		}
	      *OUTp++ = idata[iword];
	    }
	  BCLOSE;
	}

      /* Insert payload banks */

      /* Loop over (modules) blocks */
      for(ibank=0; ibank<SIMPLE_MAX_BANKS; ibank++)
	{
	  if((ibank != cTriggerBankIndex)  && (cBank[ibank].ID>0))
	    {
	      if( !isA_otherBank(cBank[ibank].ID) )
		{ /* Payload Module Bank */
		  BOPEN(cBank[ibank].ID, BT_UI4, 0);
		  for(iblk=0; iblk<blkCounter; iblk++)
		    {
		      for(iword=mData[iblk].evtIndex[iev];
			  iword<(mData[iblk].evtIndex[iev]+mData[iblk].evtLength[iev]);
			  iword++)
			{
			  if(simpleDebugMask & SIMPLE_SHOW_SECOND_PASS)
			    {
			      printf("Mod  Data  %5d: 0x%08x\n",iword,idata[iword]);
			    }
			  *OUTp++ = idata[iword];
			}
		    } /* Loop over (modules) blocks */
		  BCLOSE;
		}
	      else
		{ /* OTHER data banks */

		  /* Only insert OTHER data in first event of block if specified
		     as MODID_OTHER_ONCE */
		  if( isA_otherOnceBank(cBank[ibank].ID) )
		    if(iev!=0) continue;

		  BOPEN(cBank[ibank].ID, BT_UI4, 0);
		  for(iword=cBank[ibank].index;
		      iword<(cBank[ibank].index+cBank[ibank].length-1); iword++)
		    {
		      if(simpleDebugMask & SIMPLE_SHOW_SECOND_PASS)
			{
			  printf("Other Data  %5d: 0x%08x\n",iword,idata[iword]);
			}
		      *OUTp++ = idata[iword];
		    }
		  BCLOSE;
		}
	    }
	}

      ECLOSE;
      nOUT += outBank[iev].length = (*StartOfTrigEvent)+1;

      outBlocks++;

      if(simpleDebugMask & SIMPLE_SHOW_SECOND_PASS)
	{
	  printf("outBank %d index = %d   length = %d   nOUT = %d\n",
		 iev, outBank[iev].index, outBank[iev].length, nOUT);
	}

    }  /* Loop over events */

  return nOUT;

}

int
simpleFillEvent(volatile unsigned int *odata, volatile unsigned int *idata)
{
  int nwords=0, iev=0, eob=0;
  static int currentEventOfBlock=0;

  if(currentEventOfBlock == outBlocks)
    currentEventOfBlock = 0;

  eob = currentEventOfBlock;


  for(iev=outBank[eob].index; iev < (outBank[eob].index + outBank[eob].length); iev++)
    {
      if(simpleDebugMask & SIMPLE_SHOW_FILL_EVENTS)
	{
	  printf("%s: %d  %d   data = 0x%08x\n",__FUNCTION__,eob,iev,idata[iev]);
	}
      odata[nwords++] = idata[iev];
    }

  if(nwords==0)
    return 0;

  currentEventOfBlock++;

  return nwords;
}

/*
   Bank array utils
*/

int
isA_cBank(int bankID)
{
  int ibank=0;
  for(ibank=0; ibank<nbanks; ibank++)
    if(cBank[ibank].ID == bankID)
      return 1;

  return 0;
}

int
isA_mConf(int bankID)
{
  int ibank=0;
  for(ibank=0; ibank<nmconfs; ibank++)
    if(mConf[ibank].ID == bankID)
      return 1;

  return 0;
}

isA_mTriggerBank(int bankID)
{
  int ibank=0;
  for(ibank=0; ibank<nmconfs; ibank++)
    if(mConf[ibank].ID == bankID)
      if((mConf[ibank].type == MODID_TI) | (mConf[ibank].type == MODID_TS))
	return 1;

  return 0;

}

int
isA_otherBank(int bankID)
{
  int ibank=0;
  for(ibank=0; ibank<notherBank; ibank++)
    if(otherBank[ibank].ID == bankID)
      return 1;

  return 0;

}

int
isA_otherOnceBank(int bankID)
{
  int ibank=0;
  for(ibank=0; ibank<notherBank; ibank++)
    if(otherBank[ibank].ID == bankID)
      if(otherBank[ibank].once == 1)
	return 1;

  return 0;

}
