/*****************************************************************
 * 
 * tiprimary_list.c - "Primary" Readout list routines for tiprimary
 *
 * Usage:
 *
 *    #include "tiprimary_list.c"
 *
 *  then define the following routines:
 * 
 *    void rocDownload(); 
 *    void rocPrestart(); 
 *    void rocGo(); 
 *    void rocEnd(); 
 *    void rocTrigger();
 *
 * SVN: $Rev$
 *
 */


#define ROL_NAME__ "TIPRIMARY"
#ifndef MAX_EVENT_LENGTH
/* Check if an older definition is used */
#ifdef MAX_SIZE_EVENTS
#define MAX_EVENT_LENGTH MAX_SIZE_EVENTS
#else
#define MAX_EVENT_LENGTH 10240
#endif /* MAX_SIZE_EVENTS */
#endif /* MAX_EVENT_LENGTH */
#ifndef MAX_EVENT_POOL
/* Check if an older definition is used */
#ifdef MAX_NUM_EVENTS
#define MAX_EVENT_POOL MAX_NUM_EVENTS
#else
#define MAX_EVENT_POOL   400
#endif /* MAX_NUM_EVENTS */
#endif /* MAX_EVENT_POOL */
/* POLLING_MODE */
#define POLLING___
#define POLLING_MODE
/* INIT_NAME should be defined by roc_### (maybe at compilation time - check Makefile-rol) */
#ifndef INIT_NAME
#warn "INIT_NAME undefined. Setting to tiprimary_list__init"
#define INIT_NAME tiprimary_list__init
#endif
#include <stdio.h>
#include <rol.h>
#include "jvme.h"
#include <TIPRIMARY_source.h>
#include "tiLib.h"
extern int bigendian_out;

extern int tiFiberLatencyOffset; /* offset for fiber latency */
extern int tiDoAck;
extern int tiNeedAck;
extern int tsCrate;
/*! Buffer node pointer */
extern DMANODE *the_event;
/*! Data pointer */
extern unsigned int *dma_dabufp;

int blocklev;
int chuck=0;

#define ISR_INTLOCK INTLOCK
#define ISR_INTUNLOCK INTUNLOCK

pthread_mutex_t ack_mutex=PTHREAD_MUTEX_INITIALIZER;
#define ACKLOCK {				\
    if(pthread_mutex_lock(&ack_mutex)<0)	\
      perror("pthread_mutex_lock");		\
}
#define ACKUNLOCK {				\
    if(pthread_mutex_unlock(&ack_mutex)<0)	\
      perror("pthread_mutex_unlock");		\
}
pthread_cond_t ack_cv = PTHREAD_COND_INITIALIZER;
#define ACKWAIT {					\
    if(pthread_cond_wait(&ack_cv, &ack_mutex)<0)	\
      perror("pthread_cond_wait");			\
}
#define ACKSIGNAL {					\
    if(pthread_cond_signal(&ack_cv)<0)			\
      perror("pthread_cond_signal");			\
  }
int ack_runend=0;

/* ROC Function prototypes defined by the user */
void rocDownload();
void rocPrestart();
void rocGo();
void rocEnd();
void rocTrigger(int evnum);
void rocCleanup();
int  getOutQueueCount();
int  getInQueueCount();
void doAck();

/* Asynchronous (to tiprimary rol) trigger routine, connects to rocTrigger */
void asyncTrigger();

/* Input and Output Partitions for VME Readout */
DMA_MEM_ID vmeIN, vmeOUT;


static void __download()
{
  int status;

  daLogMsg("INFO","Readout list compiled %s", DAYTIME);
#ifdef POLLING___
  rol->poll = 1;
#endif
  *(rol->async_roc) = 0; /* Normal ROC */

  bigendian_out=1;

  pthread_mutex_init(&ack_mutex, NULL);
  pthread_cond_init(&ack_cv,NULL);
 
  /* Open the default VME windows */
  vmeOpenDefaultWindows();

  /* Initialize memory partition library */
  dmaPartInit();

  /* Setup Buffer memory to store events */
  dmaPFreeAll();
  vmeIN  = dmaPCreate("vmeIN",MAX_EVENT_LENGTH,MAX_EVENT_POOL,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);

  /* Reinitialize the Buffer memory */
  dmaPReInitAll();
  dmaPStatsAll();

  /* Initialize VME Interrupt interface - use defaults */
  tiFiberLatencyOffset = FIBER_LATENCY_OFFSET;

  tiInit(TI_ADDR,TI_READOUT,0);

  /* Execute User defined download */
  rocDownload();
 
  daLogMsg("INFO","Download Executed");

  tiDisableVXSSignals();

  /* If the TI Master, send a Clock and Trig Link Reset */
#ifdef TI_MASTER
  tiClockReset();
  taskDelay(2);
  tiTrigLinkReset();
#endif

} /*end download */     

static void __prestart()
{
  ACKLOCK;
  ack_runend=0;
  ACKUNLOCK;
  CTRIGINIT;
  *(rol->nevents) = 0;
  unsigned long jj, adc_id;
  daLogMsg("INFO","Entering Prestart");

  chuck=0;


  TIPRIMARY_INIT;
  CTRIGRSS(TIPRIMARY,1,usrtrig,usrtrig_done);
  CRTTYPE(1,TIPRIMARY,1);


  tiEnableVXSSignals();

  vmeCheckMutexHealth(2);

  /* Execute User defined prestart */
  rocPrestart();

  /* If the TI Master, send a Sync Reset 
     - required by FADC250 after it is enabled */
#ifdef TI_MASTER
  printf("%s: Sending sync as TI master\n",__FUNCTION__);
  sleep(1);
  tiSyncReset(1);
  taskDelay(2);
#endif

  /* Connect User Trigger Routine */
  tiIntConnect(TI_INT_VEC,asyncTrigger,0);

  daLogMsg("INFO","Prestart Executed");

  if (__the_event__) WRITE_EVENT_;
  *(rol->nevents) = 0;
  rol->recNb = 0;
} /*end prestart */     

static void __end()
{
  int ii, ievt, rem_count;
  int len, type, lock_key;
  DMANODE *outEvent;
  int oldnumber;
  int iwait=0;
  int blocksLeft=0;
  unsigned int blockstatus=0;
  int bready=0;

  /* Stop the madness */
  if(tsCrate)
    {
      tiDisableTriggerSource(1);
    }

  printf("%s: Starting purge of TI blocks\n",__FUNCTION__);
  while(iwait<10000)
    {
      iwait++;
      if (getOutQueueCount()>0) 
	{
	  /*       chuck=1; */
	  printf("Purging an event in vmeOUT (iwait = %d, count = %d)\n",iwait,
		 getOutQueueCount());
	  /* This wont work without a secondary readout list (will crash EB or hang the ROC) */
	  __poll();
	}
      bready=tiBReady();
      if(tsCrate)
	blockstatus=tiBlockStatus(0,0);
      else
	blockstatus=0;

      if(bready==0)
	{
	  if(blockstatus==0)
	    {
	      printf("tiBlockStatus = 0x%x   tiBReady() = %d\n",
		     blockstatus,bready);
	      break;
	    }
	}
    }

  if(tsCrate) 
    tiBlockStatus(0,1);

  printf("%s: DONE with purge of TI blocks\n",__FUNCTION__);
  INTLOCK;
  INTUNLOCK;

  iwait=0;
  printf("starting secondary poll loops\n");
  while(iwait<10000)
    {
      iwait++;
      if(iwait>10000)
	{
	  printf("Halt on iwait>10000\n");
	  break;
	}
      if(getOutQueueCount()>0)
	{
	  __poll();
	}
      else
	break;
    }
  printf("secondary __poll loops %d\n",iwait++);

  tiStatus(0);
  dmaPStatsAll();
  tiIntDisable();
  tiIntDisconnect();

  /* Execute User defined end */
  rocEnd();

  CDODISABLE(TIPRIMARY,1,0);
 
  dmaPStatsAll();
      
  daLogMsg("INFO","End Executed");

  if (__the_event__) WRITE_EVENT_;
} /* end end block */

static void __pause()
{
  CDODISABLE(TIPRIMARY,1,0);
  daLogMsg("INFO","Pause Executed");
  
  if (__the_event__) WRITE_EVENT_;
} /*end pause */

static void __go()
{
  daLogMsg("INFO","Entering Go");
  ACKLOCK;
  ack_runend=0;
  ACKUNLOCK;
  chuck=0;

  CDOENABLE(TIPRIMARY,1,1);
  rocGo();
  
  tiIntEnable(0); 
  
  if (__the_event__) WRITE_EVENT_;
}

void usrtrig(unsigned long EVTYPE,unsigned long EVSOURCE)
{
  long EVENT_LENGTH;
  int ii, len, data, lock_key;
  int syncFlag=0, lateFail=0;
  unsigned int event_number=0;
  unsigned int currentWord=0;
  unsigned int shmEvent=0, shmData[MAX_EVENT_LENGTH>>2];
  DMANODE *outEvent;
  static int prev_len=0;

  if(chuck==1)
    printf("%s: chuck\n",__FUNCTION__);

  outEvent = dmaPGetItem(vmeOUT);
  if(chuck==1)
    printf("%s: after dmaPGetItem\n",__FUNCTION__);
  if(outEvent != NULL) 
    {
      len = outEvent->length;
      blocklev = outEvent->type;
      event_number = outEvent->nevent;

      UEOPEN(0xfc01 | 0,BT_BANK,event_number);

      if(rol->dabufp != NULL) 
	{
/* #define PRINTOUT */
#ifdef PRINTOUT
	  printf("Received %d triggers... \n",
		 event_number);
#endif
	  for(ii=0;ii<len;ii++) 
	    {
	      currentWord = LSWAP(outEvent->data[ii]);
	      *rol->dabufp++ = currentWord;
#ifdef PRINTOUT
	      if((ii%5)==0) printf("\n\t");
	      printf("  0x%08x ",(unsigned int)LSWAP(outEvent->data[ii]));
#endif
	    }
#ifdef PRINTOUT
	  printf("\n\n");
#endif
	  prev_len=len;
	}
      else 
	{
	  printf("tiprimary_list: ERROR rol->dabufp is NULL -- Event lost\n");
	}

      UECLOSE;

      for(ii=1; ii<blocklev; ii++)
	{
	  UEOPEN(0xfe00 | ii,BT_BANK,event_number);
	  UECLOSE;
	}

      /* Add some "User" Events of type 0xFE00 + event of block 
	 to help trigger the secondary readout list for every trigger */
      if(chuck==1)
	printf("%s: before ACKLOCK\n",__FUNCTION__);
      ACKLOCK;

      if(chuck==1)
	printf("%s: GOT ACKLOCK\n",__FUNCTION__);
      dmaPFreeItem(outEvent);

      if(tiNeedAck>0)
	{
	  tiNeedAck=0;
	  if(chuck==1)
	    printf("%s: before SEND SIGNAL\n",__FUNCTION__);
	  ACKSIGNAL;
	}
      ACKUNLOCK;
    }
  else
    {
      logMsg("Error: no Event in vmeOUT queue\n",0,0,0,0,0,0);
    }
  
} /*end trigger */

/*
 * doAck() 
 *   Routine to send a signal to the asyncTrigger thread, indicating that a
 *    buffer has been freed.
 *
 */

void doAck()
{
  ACKLOCK;
  if(tiNeedAck>0)
    {
      tiNeedAck=0;
      ACKSIGNAL;
    }
  ACKUNLOCK;

}

void asyncTrigger()
{
  int intCount=0;
  int length,size;
  unsigned int tirval;
  int clocksource=-1;

  intCount = tiGetIntCount();

  /* grap a buffer from the queue */
  GETEVENT(vmeIN,intCount);
  if(the_event->length!=0) 
    {
      printf("Interrupt Count = %d\t",intCount);
      printf("the_event->length = %d\n",the_event->length);
    }

  /* Execute user defined Trigger Routine */
  rocTrigger(intCount);

  /* Put this event's buffer into the OUT queue. */
  ACKLOCK;
  PUTEVENT(vmeOUT);
  /* Check if the event length is larger than expected */
  length = (((int)(dma_dabufp) - (int)(&the_event->length))) - 4;
  size = the_event->part->size - sizeof(DMANODE);

  if(length>size) 
    {
      printf("rocLib: ERROR: Event length > Buffer size (%d > %d).  Event %d\n",
	     length,size,the_event->nevent);
    }
  if(dmaPEmpty(vmeIN))
    {

      printf("WARN: vmeIN out of event buffers (intCount = %d).\n",intCount);

      if(ack_runend==0 || tiBReady()>0)
	{
	  /* Set the NeedAck for Ack after a buffer is freed */
	  tiNeedAck = 1;
	  
	  /* Wait for the signal indicating that a buffer has been freed */
	  ACKWAIT;
	}
/*       else */
/* 	{ */
/* 	  printf(" WHO CARES... ack_runend==1  tiBready = %d\n",tiBReady()); */
/* 	} */

    }

  ACKUNLOCK;


}

void usrtrig_done()
{
} /*end done */

void __done()
{
  poolEmpty = 0; /* global Done, Buffers have been freed */
} /*end done */

static void __status()
{
} /* end status */

int
getOutQueueCount()
{
  if(vmeOUT) 
    return(vmeOUT->list.c);
  else
    return(0);
}

int
getInQueueCount()
{
  if(vmeIN) 
    return(vmeIN->list.c);
  else
    return(0);
}

/* This routine is automatically executed just before the shared libary
   is unloaded.

   Clean up memory that was allocated 
*/
__attribute__((destructor)) void end (void)
{
  static int ended=0;

  if(ended==0)
    {
      printf("ROC Cleanup\n");
      
      rocCleanup();
      
      dmaPFreeAll();
      vmeCloseDefaultWindows();

      ended=1;
    }

}
