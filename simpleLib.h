#ifndef __SIMPLELIBH__
#define __SIMPLELIBH__
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
 *     Header for
 *      (S)econdary (I)nstance (M)ultiblock (P)rocessing (L)ist (E)xtraction
 *
 * </pre>
 *----------------------------------------------------------------------------*/
#define SIMPLE_MAX_MODULE_TYPES  4
#define SIMPLE_MAX_BLOCKLEVEL  255
#define SIMPLE_MAX_ROCS        255
#define SIMPLE_MAX_BANKS       255
#define SIMPLE_MAX_SLOTS        32

#define BANK_ID_MASK   0xFFFF0000


#ifndef ERROR
#define ERROR -1
#endif

#ifndef OK
#define OK 0
#endif


/* Standard JLab Module Data Format */
#define DATA_TYPE_DEFINING_MASK   0x80000000
#define DATA_TYPE_MASK            0x78000000

#define BLOCK_HEADER_SLOT_MASK    0x07C00000
#define BLOCK_HEADER_MODID_MASK   0x003C0000
#define BLOCK_HEADER_BLK_NUM_MASK 0x0003FF00
#define BLOCK_HEADER_BLK_LVL_MASK 0x000000FF

#define TRIG_HEADER2_ID_MASK            0xFF100000
#define TRIG_HEADER2_HAS_TIMESTAMP_MASK (1<<16)
#define TRIG_HEADER2_NEVENTS_MASK 0x000000FF

#define BLOCK_TRAILER_SLOT_MASK   0x07C00000
#define BLOCK_TRAILER_NWORDS      0x003FFFFF

#define EVENT_HEADER_SLOT_MASK    0x07C00000
#define EVENT_HEADER_EVT_NUM_MASK 0x003FFFFF

#define TRIG_EVENT_HEADER_TYPE_MASK       0xFF000000
#define TRIG_EVENT_HEADER_WORD_COUNT_MASK 0x0000FFFF

#define FILLER_SLOT_MASK          0x07C00000

typedef struct
{
  unsigned int undef:27;
  unsigned int data_type_tag:4;
  unsigned int data_type_defining:1;
} jlab_data_word;

typedef union
{
  unsigned int raw;
  jlab_data_word bf;
} jlab_data_word_t;

/* 0: BLOCK HEADER */
typedef struct
{
  unsigned int number_of_events_in_block:8;
  unsigned int event_block_number:10;
  unsigned int module_ID:4;
  unsigned int slot_number:5;
  unsigned int data_type_tag:4;
  unsigned int data_type_defining:1;
} block_header;

typedef union
{
  unsigned int raw;
  block_header bf;
} block_header_t;

/* 1: BLOCK TRAILER */
typedef struct
{
  unsigned int words_in_block:22;
  unsigned int slot_number:5;
  unsigned int data_type_tag:4;
  unsigned int data_type_defining:1;
} block_trailer;

typedef union
{
  unsigned int raw;
  block_trailer bf;
} block_trailer_t;

/* 2: EVENT HEADER */
typedef struct
{
  unsigned int event_number:22;
  unsigned int slot_number:5;
  unsigned int data_type_tag:4;
  unsigned int data_type_defining:1;
} event_header;

typedef union
{
  unsigned int raw;
  event_header bf;
} event_header_t;


/* Bank type definitions - These are in evioDictEntry.hxx */
enum DataType {
  EVIO_UNKNOWN32    =  (0x0),
  EVIO_UINT32       =  (0x1),
  EVIO_FLOAT32      =  (0x2),
  EVIO_CHARSTAR8    =  (0x3),
  EVIO_SHORT16      =  (0x4),
  EVIO_USHORT16     =  (0x5),
  EVIO_CHAR8        =  (0x6),
  EVIO_UCHAR8       =  (0x7),
  EVIO_DOUBLE64     =  (0x8),
  EVIO_LONG64       =  (0x9),
  EVIO_ULONG64      =  (0xa),
  EVIO_INT32        =  (0xb),
  EVIO_TAGSEGMENT   =  (0xc),
  EVIO_ALSOSEGMENT  =  (0xd),
  EVIO_ALSOBANK     =  (0xe),
  EVIO_COMPOSITE    =  (0xf),
  EVIO_BANK         =  (0x10),
  EVIO_SEGMENT      =  (0x20)
};

typedef enum jlabDataTypes
  {
    BLOCK_HEADER   = 0,
    BLOCK_TRAILER  = 1,
    EVENT_HEADER   = 2,
    TRIGGER_TIME   = 3,
    DATA_NOT_VALID = 14,
    FILLER         = 15
  } DataTypes;

typedef enum jlabModuleTypes
  {
    MODID_TI = 0,
    MODID_FA250 = 1,
    MODID_FA125 = 2,
    MODID_F1TDC_V2 = 3,
    MODID_F1TDC_V3 = 4,
    MODID_TS = 5,
    MODID_TD = 6,
    MODID_SSP = 7,
    MODID_JLAB_DISC = 8,
    MODID_OTHER = 9,
    MODID_OTHER_ONCE = 10,
    MAX_MODID
  } ModuleID;

typedef enum simpleEndianType
  {
    SIMPLE_LITTLE_ENDIAN = 0,
    SIMPLE_BIG_ENDIAN    = 1
  } simpleEndian;


typedef enum simpleDebugType
  {
    SIMPLE_SHOW_BLOCK_HEADER     = (1<<0),
    SIMPLE_SHOW_BLOCK_TRAILER    = (1<<1),
    SIMPLE_SHOW_EVENT_HEADER     = (1<<2),
    SIMPLE_SHOW_EVENT_TIMESTAMP  = (1<<3),
    SIMPLE_SHOW_OTHER            = (1<<4),
    SIMPLE_SHOW_BANK_FOUND       = (1<<5),
    SIMPLE_SHOW_FILL_EVENTS      = (1<<6),
    SIMPLE_SHOW_SECOND_PASS      = (1<<7),
    SIMPLE_SHOW_UNBLOCK          = (1<<8),
    SIMPLE_SHOW_IGNORED_BANKS    = (1<<9),
    SIMPLE_SHOW_SEGMENT_FOUND    = (1<<10),
    SIMPLE_SHOW_BANK_NOT_FOUND   = (1<<11)
  } simpleDebug;

typedef struct
{
  unsigned int num:8;
  unsigned int type:6;
  unsigned int padding:2;
  unsigned int tag:16;
} bankHeader;

typedef union
{
  unsigned int raw;
  bankHeader bf;
} bankHeader_t;

typedef struct
{
  unsigned int num:16;
  unsigned int type:6;
  unsigned int padding:2;
  unsigned int tag:8;
} segmentHeader;

typedef union
{
  unsigned int raw;
  segmentHeader bf;
} segmentHeader_t;

typedef struct CodaBankInfoStruct
{
  int length;
  bankHeader_t header;
  int index;
} codaBankInfo;

typedef struct CodaSegmentInfoStruct
{
  int length;
  segmentHeader_t header;
  int index;
} codaSegmentInfo;

typedef struct BankConfigStruct
{
  int length;
  bankHeader_t header;
  int rocID;
  int    endian;
  int    isBlocked;
  unsigned int module_header;
  unsigned int header_mask;
  void  *firstPassRoutine;
} simpleBankConfig;

typedef struct RocBankStruct
{
  int length;
  bankHeader_t header;
  int index;
  int rocID;
  int nbanks;
  codaBankInfo dataBank[SIMPLE_MAX_BANKS];
} rocBankInfo;

typedef struct TriggerBankStruct
{
  int length;
  bankHeader_t header;
  int index;
  int nrocs;
  codaSegmentInfo segTime;
  codaSegmentInfo segEvType;
  codaSegmentInfo segRoc[SIMPLE_MAX_ROCS];
} trigBankInfo;

typedef struct BankDataStruct
{
  int rocID;
  int bankID;
  int blkIndex[SIMPLE_MAX_SLOTS];
  int blkTrailerIndex[SIMPLE_MAX_SLOTS];
  int blkLevel;
  int evtCounter;
  unsigned int slotMask;
  int evtIndex[SIMPLE_MAX_SLOTS][SIMPLE_MAX_BLOCKLEVEL+1];
  int evtLength[SIMPLE_MAX_SLOTS][SIMPLE_MAX_BLOCKLEVEL+1];
} bankDataInfo;

typedef struct OtherBankStruct
{
  int ID;
  int once;
} otherBankInfo;

/* prototypes */
#ifdef __cplusplus
extern "C" {
#endif

int  simpleInit();

void simpleConfigSetDebug(int dbMask);

int  simpleConfigBank(int rocID, int tag, int num,
		 int endian, int isBlocked, void *firstPassRoutine);

int  simpleConfigIgnoreUndefinedBlocks(int ignore);

int  simpleScan(volatile unsigned int *data, int nwords);
int  simpleScanCodaEvent(volatile unsigned int *data);
int  simpleScanBank(volatile unsigned int *data, int rocID, int bankNumber);

int simpleGetRocBanks(int rocID, int bankID, int *bankList);
int simpleGetRocSlotmask(int rocID, int bankID, unsigned int *slotmask);
int simpleGetRocBlockLevel(int rocID, int bankID, int *blockLevel);

int simpleGetSlotBlockHeader(int rocID, int bank, int slot, unsigned int *header);
int simpleGetSlotEventHeader(int rocID, int bank, int slot, int evt, unsigned int *header);
int simpleGetSlotEventData(int rocID, int bank, int slot, int evt, unsigned int **buffer);
int simpleGetSlotBlockTrailer(int rocID, int bank, int slot, unsigned int *trailer);

int simpleGetTriggerBankTimeSegment(unsigned long long **buffer);
int simpleGetTriggerBankTypeSegment(unsigned short **buffer);
int simpleGetTriggerBankRocSegment(int rocID, unsigned int **buffer);

#ifdef __cplusplus
}
#endif

#endif /* __SIMPLELIBH__ */
