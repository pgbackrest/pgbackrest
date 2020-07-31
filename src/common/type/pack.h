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
Read Constructors
***********************************************************************************************************************************/
PackRead *pckReadNew(IoRead *read);
PackRead *pckReadNewBuf(const Buffer *read);

/***********************************************************************************************************************************
Write Constructors
***********************************************************************************************************************************/
PackWrite *pckWriteNew(IoWrite *write);
PackWrite *pckWriteNewBuf(Buffer *write);

/***********************************************************************************************************************************
Read Functions
***********************************************************************************************************************************/
bool pckReadNext(PackRead *this);
unsigned int pckReadId(PackRead *this);
bool pckReadNull(PackRead *this, unsigned int id);

// Read array begin/end
typedef struct PackIdParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PackIdParam;

#define pckReadArrayBeginP(this, ...)                                                                                              \
    pckReadArrayBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void pckReadArrayBegin(PackRead *this, PackIdParam param);
void pckReadArrayEnd(PackRead *this);

// Read boolean
#define pckReadBoolP(this, ...)                                                                                                    \
    pckReadBool(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

bool pckReadBool(PackRead *this, PackIdParam param);

// Read 32-bit signed integer
#define pckReadInt32P(this, ...)                                                                                                   \
    pckReadInt32(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

int32_t pckReadInt32(PackRead *this, PackIdParam param);

// Read 64-bit signed integer
#define pckReadInt64P(this, ...)                                                                                                   \
    pckReadInt64(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

int64_t pckReadInt64(PackRead *this, PackIdParam param);

// Read object begin/end
#define pckReadObjBeginP(this, ...)                                                                                                \
    pckReadObjBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void pckReadObjBegin(PackRead *this, PackIdParam param);
void pckReadObjEnd(PackRead *this);

// Read pointer. See pckWritePtrP() for cautions.
#define pckReadPtrP(this, ...)                                                                                                     \
    pckReadPtr(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void *pckReadPtr(PackRead *this, PackIdParam param);

// Read string. pckReadStrNull() returns NULL if the string is NULL.
#define pckReadStrP(this, ...)                                                                                                     \
    pckReadStr(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

String *pckReadStr(PackRead *this, PackIdParam param);

#define pckReadStrNullP(this, ...)                                                                                                 \
    pckReadStrNull(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

String *pckReadStrNull(PackRead *this, PackIdParam param);

// Read 32-bit unsigned integer
#define pckReadUInt32P(this, ...)                                                                                                  \
    pckReadUInt32(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

uint32_t pckReadUInt32(PackRead *this, PackIdParam param);

// Read 64-bit unsigned integer
#define pckReadUInt64P(this, ...)                                                                                                  \
    pckReadUInt64(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

uint64_t pckReadUInt64(PackRead *this, PackIdParam param);

// Read end
void pckReadEnd(PackRead *this);

/***********************************************************************************************************************************
Write Functions
***********************************************************************************************************************************/
// Write array begin/end
#define pckWriteArrayBeginP(this, ...)                                                                                             \
    pckWriteArrayBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteArrayBegin(PackWrite *this, PackIdParam param);
PackWrite *pckWriteArrayEnd(PackWrite *this);

// Write boolean
#define pckWriteBoolP(this, value, ...)                                                                                            \
    pckWriteBool(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteBool(PackWrite *this, bool value, PackIdParam param);

// Write 32-bit signed integer
#define pckWriteInt32P(this, value, ...)                                                                                           \
    pckWriteInt32(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteInt32(PackWrite *this, int32_t value, PackIdParam param);

// Write 64-bit signed integer
#define pckWriteInt64P(this, value, ...)                                                                                           \
    pckWriteInt64(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteInt64(PackWrite *this, int64_t value, PackIdParam param);

// Write object begin/end
#define pckWriteObjBeginP(this, ...)                                                                                               \
    pckWriteObjBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteObjBegin(PackWrite *this, PackIdParam param);
PackWrite *pckWriteObjEnd(PackWrite *this);

// Read pointer. Use with extreme caution. Pointers cannot be sent to another host -- they must only be used locally.
#define pckWritePtrP(this, value, ...)                                                                                             \
    pckWritePtr(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWritePtr(PackWrite *this, const void *value, PackIdParam param);

// Write string
#define pckWriteStrP(this, value, ...)                                                                                             \
    pckWriteStr(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteStr(PackWrite *this, const String *value, PackIdParam param);

#define pckWriteStrZP(this, value, ...)                                                                                            \
    pckWriteStrZ(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteStrZ(PackWrite *this, const char *value, PackIdParam param);

#define pckWriteStrZNP(this, value, ...)                                                                                           \
    pckWriteStrZN(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteStrZN(PackWrite *this, const char *value, size_t size, PackIdParam param);

// Write 32-bit unsigned integer
#define pckWriteUInt32P(this, value, ...)                                                                                          \
    pckWriteUInt32(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteUInt32(PackWrite *this, uint32_t value, PackIdParam param);

// Write 64-bit unsigned integer
#define pckWriteUInt64P(this, value, ...)                                                                                          \
    pckWriteUInt64(this, value, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteUInt64(PackWrite *this, uint64_t value, PackIdParam param);

// Write end
PackWrite *pckWriteEnd(PackWrite *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Read Destructor
***********************************************************************************************************************************/
void pckReadFree(PackRead *this);

/***********************************************************************************************************************************
Write Destructor
***********************************************************************************************************************************/
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
