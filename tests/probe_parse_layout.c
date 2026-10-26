#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t LogEst;
typedef int16_t ynVar;
typedef u32 yDbMask;

typedef struct sqlite3 sqlite3;
typedef struct Vdbe Vdbe;
typedef struct ExprList ExprList;
typedef struct IndexedExpr IndexedExpr;
typedef struct TableLock TableLock;
typedef struct AutoincInfo AutoincInfo;
typedef struct Table Table;
typedef struct TriggerPrg TriggerPrg;
typedef struct ParseCleanup ParseCleanup;
typedef struct Returning Returning;
typedef struct VList VList;
typedef struct Index Index;
typedef struct Trigger Trigger;
typedef struct With With;
typedef struct RenameToken RenameToken;

typedef struct Token Token;
struct Token {
  const char *z;
  unsigned int n;
};

typedef struct Parse Parse;
struct Parse {
  sqlite3 *db;
  char *zErrMsg;
  Vdbe *pVdbe;
  int rc;
  u8 colNamesSet;
  u8 checkSchema;
  u8 nested;
  u8 nTempReg;
  u8 isMultiWrite;
  u8 mayAbort;
  u8 hasCompound;
  u8 okConstFactor;
  u8 disableLookaside;
  u8 prepFlags;
  u8 withinRJSubrtn;
  int nRangeReg;
  int iRangeReg;
  int nErr;
  int nTab;
  int nMem;
  int szOpAlloc;
  int iSelfTab;
  int nLabel;
  int nLabelAlloc;
  int *aLabel;
  ExprList *pConstExpr;
  IndexedExpr *pIdxEpr;
  IndexedExpr *pIdxPartExpr;
  Token constraintName;
  yDbMask writeMask;
  yDbMask cookieMask;
  int regRowid;
  int regRoot;
  int nMaxArg;
  int nSelect;
  int nTableLock;
  TableLock *aTableLock;
  AutoincInfo *pAinc;
  Parse *pToplevel;
  Table *pTriggerTab;
  TriggerPrg *pTriggerPrg;
  ParseCleanup *pCleanup;
  union {
    int addrCrTab;
    Returning *pReturning;
  } u1;
  u32 oldmask;
  u32 newmask;
  LogEst nQueryLoop;
  u8 eTriggerOp;
  u8 bReturning;
  u8 eOrconf;
  u8 disableTriggers;
  int aTempReg[8];
  Parse *pOuterParse;
  Token sNameToken;
  Token sLastToken;
  ynVar nVar;
  u8 iPkSortOrder;
  u8 explain;
  u8 eParseMode;
  int nVtabLock;
  int nHeight;
  int addrExplain;
  VList *pVList;
  Vdbe *pReprepare;
  const char *zTail;
  Table *pNewTable;
  Index *pNewIndex;
  Trigger *pNewTrigger;
  const char *zAuthContext;
  Token sArg;
  Table **apVtabLock;
  With *pWith;
  RenameToken *pRename;
};

int main(void) {
  printf("sizeof(Parse) = %lu\n", (unsigned long)sizeof(Parse));
  printf("offsetof(Parse, zErrMsg) = %lu\n", (unsigned long)offsetof(Parse, zErrMsg));
  printf("offsetof(Parse, aTempReg) = %lu\n", (unsigned long)offsetof(Parse, aTempReg));
  printf("offsetof(Parse, sLastToken) = %lu\n", (unsigned long)offsetof(Parse, sLastToken));
  printf("sizeof(Parse) - offsetof(Parse, sLastToken) = %lu\n", (unsigned long)(sizeof(Parse) - offsetof(Parse, sLastToken)));
  return 0;
}
