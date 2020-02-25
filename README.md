
# SIMPLE

This library started out as an library that "unblocked" or
"disentanlged" events in a secondary readout list.  It's now turned
into a library that indexes a CODA 3 event to make it easier to get to
events sequentially.

This library should work well in C++, but will be re-written in C++ to
take advantage of the EVIO C++ API (which could replace much of the
bank processing).

## Example usage:

* Initialize library and configure what ROCs to expect, and their banks:

```C
  simpleInit();

  int rocID = 1, bankID = 0x3, endian = 0;
  simpleConfigBank(rocID, bankID, endian);
```

 * if the data is in bigendian, just set endian = 1.
   This does not modify the data.  It just informs simple that the
   block and event headers will need to be byte-swapped before decoding
   at them.

* Get an event from EVIO:

```C
  evReadAlloc(handle, &buf, &bufLen);
```

* Scan over the buffer with simple:

```C
  simpleScan(buf, bufLen);
```

* Grab the Trigger Bank data.

```C
  int len;
  unsigned long long *buf_ll;
  len = simpleGetTriggerBankTimeSegment(&buf_ll); // Event number + Timestamp

  unsigned short *buf_s;
  len = simpleGetTriggerBankTimeSegment(&buf_s);  // Event Type

  unsigned int *buf_i;
  rocID = 1;
  len = simpleGetTriggerBankTimeSegment(rocID, &buf_i); // Misc ROC data
```

 * From this info, you should be able to get the block level.  If not:

* Grab the Block level:

```C
  rocID = 1; bankID = 3;
  unsigned int blockLevel;
  simpleGetRocBlockLevel(rocID, bankID, &blockLevel)
```

* Loop through events in the block:

```C
  int iev;
  for(iev = 0; iev < blockLevel; iev++)
    {
       int slot = 3;
       unsigned int *simpleBuf;
       int simpleLen;

       rocID = 1; bankID = 3; slot = 3;  // FADC Slot 3
       simpleLen = simpleGetSlotEventData(rocID, bankID, slot, iev, &simpleBuf);

       // ... do something with simpleBuf.
       // simpleBuf will point to the event header of the iev^th event in the block
       // simpleLen will be the length of the buffer to analyze
       // e.g.
       int idata;
       for(idata = 0; idata < simpleLen; idata++)
         {
	    fadcDataDecode(simpleBuf[idata]);
	 }

    }
```


* Other useful routines

```C
int simpleGetRocSlotmask(int rocID, int bankID, unsigned int *slotmask);
int simpleGetSlotBlockHeader(int rocID, int bank, int slot, unsigned int *header);
int simpleGetSlotEventHeader(int rocID, int bank, int slot, int evt, unsigned int *header);
int simpleGetSlotBlockTrailer(int rocID, int bank, int slot, unsigned int *trailer);
```
