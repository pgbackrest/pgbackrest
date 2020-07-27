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
PackWrite *pckWriteNew(IoWrite *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pckReadNull(PackRead *this, unsigned int id);

bool pckReadBool(PackRead *this, unsigned int id);
int32_t pckReadInt32(PackRead *this, unsigned int id);
int64_t pckReadInt64(PackRead *this, unsigned int id);
void *pckReadPtr(PackRead *this, unsigned int id);
uint64_t pckReadUInt32(PackRead *this, unsigned int id);
uint64_t pckReadUInt64(PackRead *this, unsigned int id);

PackWrite *pckWriteBool(PackWrite *this, unsigned int id, bool value);
PackWrite *pckWriteInt32(PackWrite *this, unsigned int id, int32_t value);
PackWrite *pckWriteInt64(PackWrite *this, unsigned int id, int64_t value);
PackWrite *pckWritePtr(PackWrite *this, unsigned int id, const void *value);
PackWrite *pckWriteUInt32(PackWrite *this, unsigned int id, uint32_t value);
PackWrite *pckWriteUInt64(PackWrite *this, unsigned int id, uint64_t value);

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
