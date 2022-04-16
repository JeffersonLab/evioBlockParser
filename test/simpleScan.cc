#include <iostream>
#include <stdint.h>
#include <byteswap.h>
#include "evioFileChannel.hxx"
#include "evioUtil.hxx"
#include "evioBlockParser.hxx"

int
main(int argc, char **argv)
{
  int nevents = 0, maxev = 0;
  uint32_t *buf, bufLen;
  evioFileChannel *chan;

  if(argc > 1)
    {
      chan = new evioFileChannel(argv[1], "r");
    }
  else
    {
      chan = new evioFileChannel("/home/moffit/tmp/vtpCompton_854.dat.0", "r");
    }

  // open file
  chan->open();

  if(argc == 3)
    {
      maxev = atoi(argv[2]);
      printf("Read at most %d events\n", maxev);
    }
  else
    {
      maxev = 0;
    }

  try
    {
      while(chan->readAlloc(&buf, &bufLen))
	{				/* read the event and allocate the correct size buffer */
	  nevents++;

	  printf("\n  *** event %d  len 0x%x  type 0x%x ***\n",
		 nevents, buf[0],
		 (buf[1] & 0xffff0000) >> 16);

	  // evioBlockParser on the buffer
	  cout << endl << endl << "evioBlockParser event:" << endl << endl;
	  evioBlockParser p;
	  p.SetDebugMask(0xffff &~ evioBlockParser::SHOW_NODE_FOUND);

	  p.ClearMaps();
	  p.Parse(buf);

	  uint16_t evtag; int32_t evtag_len;
	  evtag_len = p.GetTriggerBankEvTag(&evtag);
	  if(evtag_len == -1)
	    continue;

	  uint64_t *evtimestamp; int32_t evtimestamp_len;
	  evtimestamp_len = p.GetTriggerBankTimestamp(&evtimestamp);

	  printf("evtag data (%d) = 0x%04x\n", evtag_len, evtag);

	  printf("evtimestamp data (%d) = \n", evtimestamp_len);
	  for(int32_t idt = 0; idt < evtimestamp_len; idt++)
	    {
	      printf(" 0x%016lx \n", evtimestamp[idt]);
	    }
	  printf("\n");

	  vector<uint8_t> roclist(32);
	  vector<uint16_t> banklist(32);

	  roclist = p.GetRocList();

	  cout << "roclist size = " << roclist.size() << endl;

	  for(uint32_t iroc=0; iroc < roclist.size(); iroc++)
	    {
	      printf("%4d: rocID = 0x%x\n", iroc, roclist[iroc]);

	      banklist = p.GetBankList(roclist[iroc]);
	      cout << "\tbanklist size = " << banklist.size() << endl;
	      for(uint32_t ibank = 0; ibank < banklist.size(); ibank++)
	      	{

		  uint32_t *data;
		  int len = 0;
		  len = p.GetU32(roclist[iroc], banklist[ibank], &data);
	      	  printf("\t%4d: bank 0x%02x  size = %8d\n", ibank, banklist[ibank], len);

		  bool doByteSwap = (banklist[ibank] == 0x56) ? 1 : 0;

		  len = p.ParseJLabBank(roclist[iroc], banklist[ibank], doByteSwap);
		  printf(" roc: %x  bank: %x  blocklevel: %d\n",
			 roclist[iroc], banklist[ibank], len);
		  vector<uint8_t> slotlist = p.GetSlotList(roclist[iroc], banklist[ibank]);
		  for(uint32_t islot = 0; islot < slotlist.size(); islot++)
		    {
		      len = p.GetU32(roclist[iroc], banklist[ibank], slotlist[islot], 0, &data);
		      printf("slot %d  len = %d\n", slotlist[islot], len);
		      for(int i=0; i < 3; i++)
			printf("%2d: 0x%08x \n",
			       i, data[i]);
		    }
	      	}

	    }

	  if((nevents >= maxev) && (maxev != 0))
	    break;

	}

      // close file
      chan->close();

    }
  catch (evioException e)
    {
      cerr << endl << e.toString() << endl << endl;
      std::exit(EXIT_FAILURE);

    }
  catch (...)
    {
      cerr << endl << "?Unknown error" << endl << endl;
      std::exit(EXIT_FAILURE);
    }


  return 0;
}
