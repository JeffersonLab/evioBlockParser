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

#ifdef VXWORKS
#include <vxWorks.h>
#include <sysLib.h>
#include <logLib.h>
#include <taskLib.h>
#include <vxLib.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jvme.h"
#include "simpleLib.h"


/* Bank type definitions */
#define BT_BANK_ty 0x10
#define BT_UB1_ty  0x07
#define BT_UI2_ty  0x05
#define BT_UI4_ty  0x01


/* Global Variables */
static simpleEndian       in_endian=SIMPLE_LITTLE_ENDIAN, out_endian=SIMPLE_LITTLE_ENDIAN;    /* Incoming and Output Endian-ness */
static simpleDebug        simpleDebugMask=0;
static simpleModuleConfig sModConfig[SIMPLE_MAX_MODULE_TYPES];

/* Trigger event number, start at 1 */
static unsigned int simpleTriggerNumber=1;

/* CODA event Bank Info, updated for each block */
static codaEventBankInfo cBank[SIMPLE_MAX_BANKS];
static unsigned int      cBankMask=0;
static unsigned int      cTriggerBank=0;
static unsigned int      cTriggerBankIndex=0;
static int               nbanks=0;                      /* Number of banks found */

static int               ignoreUndefinedBanks=0;        /* Default is false */

/* User defined Banks for separate modules, configured before run */
static simpleModuleConfig mConf[SIMPLE_MAX_BANKS];
static unsigned int       mConfMask=0;
static unsigned int       mTriggerBankMask=0;
static int                nmconfs=0;

/* Trigger module data (TS, TI) */
static int      tEventCounter=0;
static trigData tData[SIMPLE_MAX_BLOCKLEVEL+1];
static int      tEventSyncFlag; /* Only the last event in the block will be flagged, if the block contains a syncFlag */

/* Payload module data (fADC250, fADC125, f1TDC) */
static int     blkCounter=0; /* count of blocks within the data (one per module) */
static modData mData[SIMPLE_MAX_NBLOCKS+1];
static int     mBankIndex[SIMPLE_MAX_BANKS]; /* Index of module data in mData */
static int     mBankLength[SIMPLE_MAX_BANKS]; /* Length of module data in mData */

/* Other data, to be copied to only one event... or to every event in the block */
static unsigned int      otherBankMask=0;
static unsigned int      otherOnceBankMask=0;

/* Output Coda Events indices and lengths */
static codaEventBankInfo outBank[SIMPLE_MAX_BLOCKLEVEL+1];
static int               outBlocks=0;


/* Structure defaults */
const simpleModuleConfig MODULE_CONFIG_DEFAULTS = 
  {
    -1,   /* type */
    -1,   /* bank_number */
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

int
simpleInit()
{
  int imod=0;

  for(imod=0; imod<SIMPLE_MAX_MODULE_TYPES; imod++)
    {
      sModConfig[imod] = MODULE_CONFIG_DEFAULTS;
    }

  mTriggerBankMask = 0;
  mConfMask        = 0;

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
 * @param bank_number Bank Number that contains the module data
 * @param firstPassRoutine Routine to call for first pass processing
 * @param secondPassRoutine Routine to call for second pass processing
 *
 * @return OK if successful, otherwise ERROR
 */

int
simpleConfigModule(int type, int bank_number, void *firstPassRoutine, void *secondPassRoutine)
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

  mConf[nmconfs].bank_number       = bank_number;
  mConf[nmconfs].type              = type;
  mConf[nmconfs].firstPassRoutine  = firstPassRoutine;
  mConf[nmconfs].secondPassRoutine = secondPassRoutine;

  mConfMask |= (1<<bank_number);

  if((type==MODID_TI) || (type==MODID_TS))
    mTriggerBankMask = (1<<bank_number);
  else
    mConfMask |= (1<<bank_number);

  if((type==MODID_OTHER) || (type==MODID_OTHER_ONCE))
    {
      otherBankMask |= (1<<bank_number);
      if(type==MODID_OTHER_ONCE)
	otherOnceBankMask |= (1<<bank_number);
    }
  
  nmconfs++;

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
    simpleTriggerFirstPass(idata, cBank[cTriggerBankIndex].index, cBank[cTriggerBankIndex].length);

  /* Perform a first pass to organize module event data */
  if(simpleDebugMask & SIMPLE_SHOW_UNBLOCK)
    {
      printf("%s: Start module first pass\n",__FUNCTION__);
    }
  for(ibank=0; ibank<SIMPLE_MAX_BANKS; ibank++)
    {
      if( ((1<<cBank[ibank].ID) & cBankMask) && (ibank != cTriggerBankIndex))
	{
	  if( !((1<<cBank[ibank].ID) & otherBankMask))
	    { /* OTHER banks will be taken care of in the second pass */
	      simpleFirstPass(idata, cBank[ibank].index, cBank[ibank].length, ibank);
	    }
	}
    }

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
  int bank_type=0;
  int ibank=0;

  cBankMask = 0;

  /* First word should be the length of the CODA event */
  nwords = data[iword++];

  /* Next word should be the CODA Event header */
  bank_type = (data[iword++] & 0xFF00)>>8;

  switch(bank_type)
    {
    case BT_BANK_ty:
      {
	/* Determine lengths and indicies of each bank */
	while(iword<nwords)
	  {
	    cBank[ibank].length = data[iword++];
	    cBank[ibank].ID     = (data[iword++] & BANK_NUMBER_MASK)>>16;
	    cBank[ibank].index  = iword; /* Point at first data word */

	    if(ignoreUndefinedBanks)
	      if( ((1<<cBank[ibank].ID) & mConfMask) == 0)
		{
		  if(simpleDebugMask & SIMPLE_SHOW_IGNORED_BANKS)
		    {
		      printf("[%3d  0x%08x] IGNORED BANK %2d: Length = %d, ID = %d\n",
			     iword, data[cBank[ibank].index-2],
			     ibank,
			     cBank[ibank].length,
			     cBank[ibank].ID);
		    }
		  iword += cBank[ibank].length - 1; /* Jump to next bank */
		  continue;
		}

	    /* Add to the mask of banks found */
	    cBankMask |= (1<<(cBank[ibank].ID));

	    /* Check to see if this bank was user defined to be a trigger bank */
	    if((1<<cBank[ibank].ID) & mTriggerBankMask)
	      {
		cTriggerBank = cBank[ibank].ID;
		cTriggerBankIndex = ibank;
	      }

	    if(simpleDebugMask & SIMPLE_SHOW_BANK_FOUND)
	      {
		printf("[%3d  0x%08x] BANK %2d: Length = %d, ID = %d\n",
		       iword, data[cBank[ibank].index-2],
		       ibank,
		       cBank[ibank].length,
		       cBank[ibank].ID);
	      }

	    if(simpleDebugMask & SIMPLE_SHOW_BLOCK_HEADER)
	      {
		printf("[%3d  0x%08x] BANK %2d: Length = %d, ID = %d\n",
		       iword, data[cBank[ibank].index-2],
		       ibank,
		       cBank[ibank].length,
		       cBank[ibank].ID);
	      }

	    iword += cBank[ibank].length - 1; /* Jump to next bank */
	    ibank++;
	  }
	nbanks = ibank;
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

  return OK;
}

/**
 * @ingroup Unblock
 * @brief First pass through data to determine header indicies.
 *
 *    This is the default first pass algorithm, if one is not specified with
 *    simpleConfigModule.  It uses the JLab Data Format Standard
 *
 * @param data Memory address of the data
 * @param nwords The number of words that comprises the data
 *
 * @return OK if successful, otherwise ERROR
 */

int
simpleFirstPass(volatile unsigned int *data, int startIndex, int nwords, int bankIndex)
{
  int rval=OK;
  int iword=0; /* Index of current word in *data */
  int data_type=0;
  int current_slot=0, block_level=0, block_number=0;
  int block_words=0;
  int current_block=0, current_event=0;
  int iblock=0;

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
	      iword<(tData[iev].index+tData[iev].length); 
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
	  if( ((1<<cBank[ibank].ID) & cBankMask) && (ibank != cTriggerBankIndex))
	    {
	      if( ((1<<cBank[ibank].ID) & otherBankMask) == 0)
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
		  if((1<<cBank[ibank].ID) & otherOnceBankMask)
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
