
#ifndef __DMABANKTOOLS__
#define __DMABANKTOOLS__

long *dStartOfEvent[1],devent_depth__=0;

#define BLOCKOPEN(bnum, btype, blocklev) {				\
    the_event->type = blocklev;						\
    dStartOfEvent[devent_depth__++] = (dma_dabufp);			\
    *(++(dma_dabufp)) = LSWAP((syncFlag<<24) | ((bnum) << 16) | ((btype##_ty) << 8) | (0xff & (the_event->nevent))); \
    ((dma_dabufp))++;}

#define BLOCKCLOSE {devent_depth__--;					\
    long eventlen;							\
    eventlen = (long) (((char *) (dma_dabufp)) - ((char *) dStartOfEvent[devent_depth__])); \
    *dStartOfEvent[devent_depth__] = LSWAP(eventlen);			\
    if ((eventlen & 1) != 0) {						\
      (dma_dabufp) = ((long *)((char *) (dma_dabufp))+1);		\
      eventlen += 1;							\
      *dStartOfEvent[devent_depth__] = LSWAP(eventlen);			\
    };									\
    if ((eventlen & 2) !=0) {						\
      eventlen = eventlen + 2;						\
      *dStartOfEvent[devent_depth__] = LSWAP(eventlen);			\
      (dma_dabufp) = ((long *)((short *) (dma_dabufp))+1);;		\
    };									\
    eventlen = ( (eventlen)>>2) - 1;					\
    *dStartOfEvent[devent_depth__] = LSWAP(eventlen);};				
  
#define EVENTOPEN(bnum, btype) {					\
    the_event->type = 1;						\
    dStartOfEvent[devent_depth__++] = (dma_dabufp);			\
    *(++(dma_dabufp)) = LSWAP((syncFlag<<24) | ((bnum) << 16) | ((btype##_ty) << 8) | (0xff & (the_event->nevent))); \
    ((dma_dabufp))++;}

#define EVENTCLOSE {devent_depth__--;					\
    long eventlen;							\
    eventlen = (long) (((char *) (dma_dabufp)) - ((char *) dStartOfEvent[devent_depth__])); \
    *dStartOfEvent[devent_depth__] = LSWAP(eventlen);			\
    if ((eventlen & 1) != 0) {						\
      (dma_dabufp) = ((long *)((char *) (dma_dabufp))+1);		\
      eventlen += 1;							\
      *dStartOfEvent[devent_depth__] = LSWAP(eventlen);			\
    };									\
    if ((eventlen & 2) !=0) {						\
      eventlen = eventlen + 2;						\
      *dStartOfEvent[devent_depth__] = LSWAP(eventlen);			\
      (dma_dabufp) = ((long *)((short *) (dma_dabufp))+1);;		\
    };									\
    eventlen = ( (eventlen)>>2) - 1;					\
    *dStartOfEvent[devent_depth__] = LSWAP(eventlen);};				
  
#define BANKOPEN(bnum, btype, code) {					\
    long *StartOfBank;							\
    StartOfBank = (dma_dabufp);						\
    *(++(dma_dabufp)) = LSWAP((((bnum) << 16) | (btype##_ty) << 8) | (code)); \
    ((dma_dabufp))++;


#define BANKCLOSE							\
  long banklen;								\
  banklen = (long) (((char *) (dma_dabufp)) - ((char *) StartOfBank));	\
  *StartOfBank = LSWAP(banklen);					\
  if ((banklen & 1) != 0) {						\
    (dma_dabufp) = ((long *)((char *) (dma_dabufp))+1);			\
    banklen += 1;							\
    *StartOfBank = LSWAP(banklen);					\
  };									\
  if ((banklen & 2) !=0) {						\
    banklen = banklen + 2;						\
    *StartOfBank = LSWAP(banklen);					\
    (dma_dabufp) = ((long *)((short *) (dma_dabufp))+1);;		\
  };									\
  *StartOfBank = LSWAP(( (banklen) >> 2) - 1);				\
  };

#endif
