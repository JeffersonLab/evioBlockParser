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
#include <stdio.h>
#include <string.h>
#include "jvme.h"
#include "simpleLib.h"

/* Global Variables */
static simpleEndian in_endian=SIMPLE_LITTLE_ENDIAN, out_endian=SIMPLE_LITTLE_ENDIAN;    /* Incoming and Output Endian-ness */
static simpleDebug  simpleDebugMask=0;
static simpleModuleConfig sModConfig[SIMPLE_MAX_MODULE_TYPES];
static int block_counter=0; /* count of blocks within the data (one per module) */
static int slot_number[SIMPLE_MAX_NBLOCKS+1]; /* Slot number for the nth block */
static int block_index[SIMPLE_MAX_NBLOCKS+1]; /* Index of start of block within the data */
static int event_counter[SIMPLE_MAX_NBLOCKS+1]; /* Event counter within the block */
static int event_index[SIMPLE_MAX_NBLOCKS+1][SIMPLE_MAX_BLOCKLEVEL+1]; /* index of start of event within block */
static int event_length[SIMPLE_MAX_NBLOCKS+1][SIMPLE_MAX_BLOCKLEVEL+1]; /* Length of event within the block */

int
simpleInit()
{
  int imod=0;

  for(imod=0; imod<SIMPLE_MAX_MODULE_TYPES; imod++)
    {
      sModConfig[imod].bank_number       = -1;
      sModConfig[imod].firstPassRoutine  = NULL;
      sModConfig[imod].secondPassRoutine = NULL;
    }


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
simpleConfigModule(int bank_number, void *firstPassRoutine, void *secondPassRoutine)
{

  return OK;
}

int
simpleUnblock(volatile unsigned int *data, int nwords)
{


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
simpleFirstPass(volatile unsigned int *data, int nwords)
{
  int iword=0; /* Index of current word in *data */
  int data_type=0;
  int current_slot=0, block_level=0, block_number=0;
  int block_words=0;
  int current_block=0, current_event=0;
  int iblock=0;

  /* Re-initialized block counter */
  block_counter = 0; 

  while(iword<nwords)
    {
      if( ((data[iword] & DATA_TYPE_DEFINING_MASK)>>31) == 1)
	{
	  data_type = (data[iword] & DATA_TYPE_MASK)>>27;
	  if(data_type>16)
	    {
	      printf("%s: ERROR: Undefined data type (%d). At Index %d\n",
		     __FUNCTION__,data_type,iword);
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

		    block_counter++; /* Increment block counter */
		    current_block = block_counter - 1;
		    event_counter[current_block] = 0; /* Initialize the event counter */

		    slot_number[current_block] = current_slot;
		    block_index[current_block] = iword;

		
		    if(simpleDebugMask & SIMPLE_SHOW_BLOCK_HEADER)
		      {
			printf("[%3d] BLOCK HEADER: slot %2d, block_number %3d, block_level %3d\n",
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
			printf("[%3d] BLOCK TRAILER: slot %2d, nwords %d\n",
			       iword,
			       current_slot,block_words);
		      }
		
		    current_block = block_counter-1;

		    /* Obtain the previous event length */
		    if(event_counter[current_block] > 0)
		      {
			current_event = event_counter[current_block] - 1;
		  
			event_length[current_block][current_event] = 
			  iword - event_index[current_block][current_event];
		      }

		    /* Check the slot number to make sure this block
		       trailer is associated with the previous block
		       header */
		    if(current_slot != slot_number[current_block])
		      {
			printf("[%3d] ERROR: blockheader slot %d != blocktrailer slot %d\n",
			       iword,
			       slot_number[current_block],current_slot);
		      }

		    /* Check the number of words vs. words counted within the block */
		    if(block_words != (iword - block_index[current_block]+1) )
		      {
			printf("[%3d] ERROR: trailer #words %d != actual #words %d\n",
			       iword,
			       block_words,iword-block_index[current_block]+1);
		      }
		    break;
		  }

		case EVENT_HEADER: /* 2: EVENT HEADER */
		  {
		    if(simpleDebugMask & SIMPLE_SHOW_BLOCK_TRAILER)
		      {
			printf("[%3d] EVENT HEADER: trigger number %d\n"
			       ,iword,
			       (data[iword] & EVENT_HEADER_EVT_NUM_MASK)>>0);

		      }
		    current_block = block_counter-1;

		    /* Obtain the previous event length */
		    if(event_counter[current_block] > 0)
		      {
			current_event = event_counter[current_block] - 1;
		  
			event_length[current_block][current_event] = 
			  iword - event_index[current_block][current_event];
		      }

		    event_counter[current_block]++; /* increment event counter */
		    current_event = event_counter[current_block] - 1;
		    event_index[current_block][current_event] = iword;

		    break;
		  }
	      
		default:
		  /* Ignore all other data types for now */
		  continue;
		} /* switch(data_type) */

	    } /* if(data_type>16) {} else */

	} /* if( ((data[iword] & DATA_TYPE_DEFINING_MASK)>>31) == 1) */

      iword++;

    } /* while(iword<nwords) */

  /* Check for consistent number of events for each block */
  for(iblock=1; iblock<block_counter; iblock++)
    {
      if(event_counter[iblock] != event_counter[0])
	{
	  printf("ERROR: Event Count (block %d): (%3d) != Event Counter (block 0): (%3d)\n",
		 iblock, event_counter[iblock], event_counter[0]);
	}
    }

  return OK;
}

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
simpleSecondPass(volatile unsigned int *data, int nwords)
{

  return OK;

}
