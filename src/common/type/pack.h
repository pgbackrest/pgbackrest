/***********************************************************************************************************************************
Pack Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_PACK_H
#define COMMON_TYPE_PACK_H

/***********************************************************************************************************************************
Minimum number of extra bytes to allocate for packs that are growing or are likely to grow
***********************************************************************************************************************************/
#ifndef PACK_EXTRA_MIN
    #define PACK_EXTRA_MIN                                          64
#endif

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PACK_READ_TYPE                                              PackRead
#define PACK_READ_PREFIX                                            pckRead

typedef struct PackRead PackRead;

#define PACK_WRITE_TYPE                                             PackWrite
#define PACK_WRITE_PREFIX                                           pckWrite

typedef struct PackWrite PackWrite;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Pack type
***********************************************************************************************************************************/
typedef enum
{
    pckTypeUnknown = 0,
    pckTypeArray,
    pckTypeBin,
    pckTypeBool,
    pckTypeInt32,
    pckTypeInt64,
    pckTypeObj,
    pckTypePtr,
    pckTypeStr,
    pckTypeUInt32,
    pckTypeUInt64,
} PackType;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
PackRead *pckReadNew(IoRead *read);
PackRead *pckReadNewBuf(const Buffer *read);

PackWrite *pckWriteNew(IoWrite *write);
PackWrite *pckWriteNewBuf(Buffer *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pckReadNext(PackRead *this);
unsigned int pckReadId(PackRead *this);
bool pckReadNull(PackRead *this, unsigned int id);

void pckReadArrayBegin(PackRead *this, unsigned int id);
void pckReadArrayEnd(PackRead *this);
bool pckReadBool(PackRead *this, unsigned int id);
int32_t pckReadInt32(PackRead *this, unsigned int id);
int64_t pckReadInt64(PackRead *this, unsigned int id);
void pckReadObjBegin(PackRead *this, unsigned int id);
void pckReadObjEnd(PackRead *this);
void *pckReadPtr(PackRead *this, unsigned int id);
String *pckReadStr(PackRead *this, unsigned int id);
String *pckReadStrNull(PackRead *this, unsigned int id);
uint32_t pckReadUInt32(PackRead *this, unsigned int id);
uint64_t pckReadUInt64(PackRead *this, unsigned int id);

void pckReadEnd(PackRead *this);

PackWrite *pckWriteArrayBegin(PackWrite *this, unsigned int id);
PackWrite *pckWriteArrayEnd(PackWrite *this);
PackWrite *pckWriteBool(PackWrite *this, unsigned int id, bool value);
PackWrite *pckWriteInt32(PackWrite *this, unsigned int id, int32_t value);
PackWrite *pckWriteInt64(PackWrite *this, unsigned int id, int64_t value);
PackWrite *pckWriteObjBegin(PackWrite *this, unsigned int id);
PackWrite *pckWriteObjEnd(PackWrite *this);
PackWrite *pckWritePtr(PackWrite *this, unsigned int id, const void *value);
PackWrite *pckWriteStr(PackWrite *this, unsigned int id, const String *value);
PackWrite *pckWriteStrZ(PackWrite *this, unsigned int id, const char *value);
PackWrite *pckWriteStrZN(PackWrite *this, unsigned int id, const char *value, size_t size);
PackWrite *pckWriteUInt32(PackWrite *this, unsigned int id, uint32_t value);
PackWrite *pckWriteUInt64(PackWrite *this, unsigned int id, uint64_t value);

PackWrite *pckWriteEnd(PackWrite *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void pckReadFree(PackRead *this);
void pckWriteFree(PackWrite *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *pckReadToLog(const PackRead *this);

#define FUNCTION_LOG_PACK_READ_TYPE                                                                                                \
    PackRead *
#define FUNCTION_LOG_PACK_READ_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, pckReadToLog, buffer, bufferSize)

String *pckWriteToLog(const PackWrite *this);

#define FUNCTION_LOG_PACK_WRITE_TYPE                                                                                               \
    PackWrite *
#define FUNCTION_LOG_PACK_WRITE_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, pckWriteToLog, buffer, bufferSize)

#endif
