/*************************************************************************
 *
 *  vme_list.c - Library of routines for readout and buffering of 
 *                events using a JLAB Trigger Interface V3 (TI) with 
 *                a Linux VME controller.
 *
 */

#define BLOCKLEVEL 10
#define BUFFERLEVEL 10
#define BLOCKLIMIT  5

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10*BLOCKLEVEL  /* Should be at least BLOCKLEVEL*BUFFERLEVEL */
#define MAX_EVENT_LENGTH   4*BLOCKLEVEL*500        /* Size in Bytes */

/* Define Interrupt source and address */
#define TI_MASTER
#define TI_READOUT TI_READOUT_EXT_POLL  /* Poll for available data, external triggers */
#define TI_ADDR    (2<<19)          /* GEO slot 21 */

/* Decision on whether or not to readout the TI for each block 
   - Comment out to disable readout 
*/
#define TI_DATA_READOUT

#define FIBER_LATENCY_OFFSET 0x4A  /* measured longest fiber length */

#include "dmaBankTools.h"
#include "tiprimary_list.c" /* source required for CODA */
#include "fadcLib.h"

/* Default block level */

int usePulser=1;

/* Redefine tsCrate according to TI_MASTER or TI_SLAVE */
#ifdef TI_SLAVE
int tsCrate=0;
#else
#ifdef TI_MASTER
int tsCrate=1;
#endif
#endif

/* FADC Library Variables */
extern int fadcA32Base;
int FA_SLOT;

/* function prototype */
void rocTrigger(int arg);

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  unsigned short iflag;

  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1); 

  /*****************
   *   TI SETUP
   *****************/
  int overall_offset=0x80;

#ifndef TI_DATA_READOUT
  /* Disable data readout */
  tiDisableDataReadout();
  /* Disable A32... where that data would have been stored on the TI */
  tiDisableA32();
#endif

  /* Set crate ID */
  tiSetCrateID(0x01); /* ROC 1 */

  if(usePulser)
    tiSetTriggerSource(TI_TRIGGER_PULSER);
  else
    tiSetTriggerSource(TI_TRIGGER_TSINPUTS);


  /* Set needed TS input bits */
  tiEnableTSInput( TI_TSINPUT_1 );

  /* Load the trigger table that associates 
     pins 21/22 | 23/24 | 25/26 : trigger1
     pins 29/30 | 31/32 | 33/34 : trigger2
  */
  tiLoadTriggerTable(0);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

/*   /\* Set the sync delay width to 0x40*32 = 2.048us *\/ */
  tiSetSyncDelayWidth(0x54, 0x40, 1);

  /* Set the busy source to non-default value (no Switch Slot B busy) */
  tiSetBusySource(TI_BUSY_LOOPBACK,1);

/*   tiSetFiberDelay(10,0xcf); */

#ifdef TI_MASTER
  /* Set number of events per block */
  tiSetBlockLevel(BLOCKLEVEL);
#endif

  tiSetBlockBufferLevel(BUFFERLEVEL);

  tiSetBlockLimit(BLOCKLIMIT);

  tiStatus(0);


  /* Program/Init FADC Modules Here */
  iflag  = 0;  /* No SDC */
  iflag |= FA_INIT_SOFT_SYNCRESET;
  iflag |= FA_INIT_SOFT_TRIG;
  iflag |= FA_INIT_INT_CLKSRC;

  fadcA32Base = 0x08800000;

  faInit((2<<19),(1<<19),4,iflag);

  FA_SLOT = faSlot(0);

  /*      Setup FADC Programming */
  faSetBlockLevel(FA_SLOT,BLOCKLEVEL);

  /*  for Block Reads */
  faEnableBusError(FA_SLOT);
  /*  for Single Cycle Reads */
  /*       faDisableBusError(FA_SLOT); */
  
  /*  Set All channel thresholds to 0 */
  faSetThreshold(FA_SLOT,0,0xffff);
  
    
  /*  Setup option 1 processing - RAW Window Data     <-- */
  /*        option 2            - RAW Pulse Data */
  /*        option 3            - Integral Pulse Data */
  /*  Setup 200 nsec latency (PL  = 50)  */
  /*  Setup  80 nsec Window  (PTW = 20) */
  /*  Setup Pulse widths of 36ns (NSB(3)+NSA(6) = 9)  */
  /*  Setup up to 1 pulse processed */
  /*  Setup for both ADC banks(0 - all channels 0-15) */
  faSetProcMode(FA_SLOT,1,50,20,3,6,1,0);
  
  faClear(FA_SLOT);

  faGStatus(0);



  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  unsigned short iflag;
  int stat;
  int islot;

  tiStatus(0);

  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{
  int islot;
  /* Enable modules, if needed, here */
  /*  Enable FADC */
  faEnable(0,0,0);

  /*  Send Sync Reset to FADC */
  faSync(0);

  /* Show the current block level */
  printf("%s: Current Block Level = %d\n",
	 __FUNCTION__,tiGetCurrentBlockLevel());

  if(usePulser) {
    tiSetRandomTrigger(1,0x6);
    /* Software trigger - large range */
/*     tiSoftTrig(1,200,0x7fff,1); */
  }    
  /* Use this info to change block level is all modules */

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  int islot;

  tiStatus(0);

  /* FADC Disable */
  faDisable(0,0);

  /* FADC Event status - Is all data read out */
  faGStatus(0);


  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());
  
}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int evnum)
{
  int ii, islot;
  int stat, dCnt, len=0, idata, nwords=0;
  unsigned int datascan;

  tiSetOutputPort(1,0,0,0);

  /* Send a software trigger to fADC250 */
  for(ii=0; ii<BLOCKLEVEL; ii++)
    faTrig(faSlot(0));

  BLOCKOPEN(evnum,BT_BANK,BLOCKLEVEL);

#ifdef TI_DATA_READOUT
  BANKOPEN(4,BT_UI4,0);

  vmeDmaConfig(2,5,1); 
  dCnt = tiReadBlock(dma_dabufp,80+(3*BLOCKLEVEL),1);
  if(dCnt<=0)
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
      dma_dabufp += dCnt;
    }

  BANKCLOSE;
#endif

  /* Bank for fADC250 data */
  BANKOPEN(3,BT_UI4,0);

  /* Check for valid data here */
  for(ii=0;ii<1000;ii++) 
    {
      datascan = faBready(faSlot(0));
      if (datascan>0) 
	{
	  break;
	}
    }

  if(datascan>0) 
    {
      nwords = faReadBlock(faSlot(0),dma_dabufp,200*BLOCKLEVEL,1);
    
      if(nwords < 0) 
	{
	  printf("ERROR: in transfer (event = %d), nwords = 0x%x\n", tiGetIntCount(),nwords);
	  *dma_dabufp++ = LSWAP(0xda000bad);
	} 
      else 
	{
	  dma_dabufp += nwords;
	}
    } 
  else 
    {
      printf("ERROR: Data not ready in event %d\n",tiGetIntCount());
      *dma_dabufp++ = LSWAP(0xda000bad);
    }
  BANKCLOSE;

  BANKOPEN(5,BT_UI4,0);
  *dma_dabufp++ = LSWAP(tiGetIntCount());
  *dma_dabufp++ = LSWAP(0xdead);
  *dma_dabufp++ = LSWAP(0xcebaf111);
  BANKCLOSE;

  BLOCKCLOSE;

  tiSetOutputPort(0,0,0,0);

}

void
rocCleanup()
{
  int islot=0;

  printf("%s: Reset all FADCs\n",__FUNCTION__);
  
}
