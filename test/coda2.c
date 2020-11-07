#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <byteswap.h>
#include "evio.h"
#include "simpleLib.h"

int
main(int argc, char **argv)
{
  int verbose = 1;
  int handle;
  uint32_t *buf, bufLen, nevents = 0, status = 0;
  char filename[128] = "/daqfs/scratch/moffit/hallc/lappd_692.dat";

  simpleConfigSetDebug(0xffff & ~SIMPLE_SHOW_OTHER);
  /* Open file  */
  if((status = evOpen((char *)filename,
		      (char *) "r", &handle)) < 0)
    {
      printf("Unable to open file %s status = %d\n", filename, status);
      exit(-1);
    }
  else
    {
      printf("Opened %s for reading (status = 0x%x, handle = %d)\n\n",
	     filename, status, handle);
    }

  int version = 0;
  /* Get evio version # of file */
  status = evIoctl(handle, (char*)"v", &version);
  if (status == S_SUCCESS)
    {
      printf("Evio file version = %d\n\n", version);
    }
  else
    {
      printf("status = 0x%x\n", status);
    }

  simpleInit();
  simpleConfigBank(3, 0x3, 1,
		 0, 1, NULL);
  simpleConfigBank(3, 0x1, 0,
  		 0, 1, NULL);
  simpleConfigSetCodaVersion(2);

  while((status = evReadAlloc(handle, &buf, &bufLen)) == 0)
    {				/* read the event and allocate the correct size buffer */
      uint32_t nWords = 0, bt = 0, dt = 0, blk = 0;
      int pe = 0;
      nWords = buf[0] + 1;
      bt = ((buf[1] & 0xffff0000) >> 16);	/* Bank Tag */
      dt = ((buf[1] & 0xff00) >> 8);	/* Data Type */
      blk = buf[4];	/* readout Event number */

      nevents++;

      if(nevents > 4)
	break;

      if(verbose)
	printf("    BLOCK #%d,  Bank tag = 0x%04x, Data type = 0x%04x,  Total len = %d words\n",
	       nevents, bt, dt, nWords);

      /* Check on what type of event block this is */
      /* CODA Reserved bank type */
      switch (bt) {
      case 17:
	if(verbose) printf("    ** Prestart Event **\n");
	break;
      case 18:
	if(verbose) printf("    ** Go Event **\n");
	break;
      case 19:
	if(verbose) printf("    ** Pause Event **\n");
	break;
      case 20:
	if(verbose) printf("    ** End Event **\n");
	break;
      default:
	if(bt < 17)
	  {
	    if(verbose)
	      printf("    ** Physics Event Block (ev number: %d) **\n",blk);
	    pe=1;
	  }
	else
	  {
	    if(verbose) printf("    ** Undefined CODA Event Type **\n");
	  }
      }

      if(pe)
	{
	  simpleScan(buf, nWords);

	  unsigned int *try;
	  simpleGetSlotBlockHeader(3,3,12, &try[0]);
	  printf("Slot 12 BLock Header = 0x%08x\n", try[0]);

	  int len, iword;
	  unsigned int *bufi;
	  len = simpleGetSlotEventData(3, 0x3, 12, 0, &bufi);
	  if(len > 0)
	    {
	      for(iword = 0; iword < 5; iword++)
	      {
		printf("[%6d  0x%x]\n",
		       iword, bufi[iword]);
	      }
	    }
	  else
	    {
	      printf("len = %d\n", len);
	    }

	}
    }

  if ( status == EOF )
    {
      printf("Found end-of-file; total %d events. \n", nevents);
    }
  else if(status != 0)
    {
      printf("Error reading file (status = 0x%x, quit)\n",status);
    }


  return 0;
}
