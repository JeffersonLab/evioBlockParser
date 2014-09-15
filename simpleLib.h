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
#define SIMPLE_MAX_SLOT_NUMBER  21
#define SIMPLE_MAX_NBLOCKS      SIMPLE_MAX_SLOT_NUMBER
#define SIMPLE_MAX_BLOCKLEVEL  255
#define SIMPLE_MAX_BANKS        16

#define BANK_NUMBER_MASK   0xFFFF0000


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
    MODID_JLAB_DISC = 8
  } ModuleID;

typedef enum simpleEndianType
  {
    SIMPLE_LITTLE_ENDIAN,
    SIMPLE_BIG_ENDIAN
  } simpleEndian;


typedef enum simpleDebugType
  {
    SIMPLE_SHOW_BLOCK_HEADER     = (1<<0),
    SIMPLE_SHOW_BLOCK_TRAILER    = (1<<1),
    SIMPLE_SHOW_EVENT_HEADER     = (1<<2),
    SIMPLE_SHOW_EVENT_TIMESTAMP  = (1<<3)
  } simpleDebug;

typedef struct ModuleProcStruct
{
  int    bank_number;
  unsigned int module_header;
  unsigned int header_mask;
  void  *firstPassRoutine;
  void  *secondPassRoutine;
} simpleModuleConfig;

typedef struct CodaEventBankInfoStruct
{
  int length;
  int ID;
  int index;
} codaEventBankInfo;

typedef struct TriggerDataStruct
{
  int type;
  int index;
  int length;
  int number;
} trigData;

typedef struct ModuleDataStruct
{
  int slotNumber;
  int blkIndex;
  int evtCounter;
  int evtIndex[SIMPLE_MAX_BLOCKLEVEL+1];
  int evtLength[SIMPLE_MAX_BLOCKLEVEL+1];
} modData;



int  simpleInit();
void simpleConfigEndianInOut(int in_end, int out_end);
void simpleConfigSetDebug(int dbMask);
int  simpleConfigModule(int bank_number, void *firstPassRoutine, void *secondPassRoutine);
int  simpleUnblock(volatile unsigned int *data, int nwords);
int  simpleScanCodaEvent(volatile unsigned int *data);
int  simpleFirstPass(volatile unsigned int *data, int nwords);
int  simpleTriggerFirstPass(volatile unsigned int *data, int nwords);
int  simpleSecondPass(volatile unsigned int *odata, volatile unsigned int *idata, int in_nwords);

#endif /* __SIMPLELIBH__ */
