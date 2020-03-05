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

	  // stream parse the buffer
	  cout << endl << endl << "Stream parsing event:" << endl << endl;
	  evioBlockParser p;
	  p.SetDebugMask(0xffff &~ evioBlockParser::SHOW_NODE_FOUND);

	  p.Parse(buf);

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
