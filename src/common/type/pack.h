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
    pckTypeTime,
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
typedef struct PackIdParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PackIdParam;

// Read next field
bool pckReadNext(PackRead *this);

// Current field id
unsigned int pckReadId(PackRead *this);

// Current field type
PackType pckReadType(PackRead *this);

// Is the field NULL?
#define pckReadNullP(this, ...)                                                                                                    \
    pckReadNull(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

bool pckReadNull(PackRead *this, PackIdParam param);

// Read array begin/end
#define pckReadArrayBeginP(this, ...)                                                                                              \
    pckReadArrayBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void pckReadArrayBegin(PackRead *this, PackIdParam param);
void pckReadArrayEnd(PackRead *this);

// Read binary
typedef struct PckReadBinParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
} PckReadBinParam;

#define pckReadBinP(this, ...)                                                                                                     \
    pckReadBin(this, (PckReadBinParam){VAR_PARAM_INIT, __VA_ARGS__})

Buffer *pckReadBin(PackRead *this, PckReadBinParam param);

// Read boolean
typedef struct PckReadBoolParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    uint32_t defaultValue;
} PckReadBoolParam;

#define pckReadBoolP(this, ...)                                                                                                    \
    pckReadBool(this, (PckReadBoolParam){VAR_PARAM_INIT, __VA_ARGS__})

bool pckReadBool(PackRead *this, PckReadBoolParam param);

// Read 32-bit signed integer
typedef struct PckReadInt32Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    int32_t defaultValue;
} PckReadInt32Param;

#define pckReadInt32P(this, ...)                                                                                                   \
    pckReadInt32(this, (PckReadInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

int32_t pckReadInt32(PackRead *this, PckReadInt32Param param);

// Read 64-bit signed integer
typedef struct PckReadInt64Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    int64_t defaultValue;
} PckReadInt64Param;

#define pckReadInt64P(this, ...)                                                                                                   \
    pckReadInt64(this, (PckReadInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

int64_t pckReadInt64(PackRead *this, PckReadInt64Param param);

// Read object begin/end
#define pckReadObjBeginP(this, ...)                                                                                                \
    pckReadObjBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void pckReadObjBegin(PackRead *this, PackIdParam param);
void pckReadObjEnd(PackRead *this);

// Read pointer. See pckWritePtrP() for cautions.
typedef struct PckReadPtrParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
} PckReadPtrParam;

#define pckReadPtrP(this, ...)                                                                                                     \
    pckReadPtr(this, (PckReadPtrParam){VAR_PARAM_INIT, __VA_ARGS__})

void *pckReadPtr(PackRead *this, PckReadPtrParam param);

// Read string
typedef struct PckReadStrParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    const String *defaultValue;
} PckReadStrParam;

#define pckReadStrP(this, ...)                                                                                                     \
    pckReadStr(this, (PckReadStrParam){VAR_PARAM_INIT, __VA_ARGS__})

String *pckReadStr(PackRead *this, PckReadStrParam param);

// Read time
typedef struct PckReadTimeParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    time_t defaultValue;
} PckReadTimeParam;

#define pckReadTimeP(this, ...)                                                                                                    \
    pckReadTime(this, (PckReadTimeParam){VAR_PARAM_INIT, __VA_ARGS__})

time_t pckReadTime(PackRead *this, PckReadTimeParam param);

// Read 32-bit unsigned integer
typedef struct PckReadUInt32Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    uint32_t defaultValue;
} PckReadUInt32Param;

#define pckReadUInt32P(this, ...)                                                                                                  \
    pckReadUInt32(this, (PckReadUInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

uint32_t pckReadUInt32(PackRead *this, PckReadUInt32Param param);

// Read 64-bit unsigned integer
typedef struct PckReadUInt64Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    uint64_t defaultValue;
} PckReadUInt64Param;

#define pckReadUInt64P(this, ...)                                                                                                  \
    pckReadUInt64(this, (PckReadUInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

uint64_t pckReadUInt64(PackRead *this, PckReadUInt64Param param);

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

// Write binary
typedef struct PckWriteBinParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
} PckWriteBinParam;

#define pckWriteBinP(this, value, ...)                                                                                             \
    pckWriteBin(this, value, (PckWriteBinParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteBin(PackWrite *this, const Buffer *value, PckWriteBinParam param);

// Write boolean
typedef struct PckWriteBoolParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    uint32_t defaultValue;
} PckWriteBoolParam;

#define pckWriteBoolP(this, value, ...)                                                                                            \
    pckWriteBool(this, value, (PckWriteBoolParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteBool(PackWrite *this, bool value, PckWriteBoolParam param);

// Write 32-bit signed integer
typedef struct PckWriteInt32Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    int32_t defaultValue;
} PckWriteInt32Param;

#define pckWriteInt32P(this, value, ...)                                                                                           \
    pckWriteInt32(this, value, (PckWriteInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteInt32(PackWrite *this, int32_t value, PckWriteInt32Param param);

// Write 64-bit signed integer
typedef struct PckWriteInt64Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    int64_t defaultValue;
} PckWriteInt64Param;

#define pckWriteInt64P(this, value, ...)                                                                                           \
    pckWriteInt64(this, value, (PckWriteInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteInt64(PackWrite *this, int64_t value, PckWriteInt64Param param);

// Write null
#define pckWriteNullP(this)                                                                                                        \
    pckWriteNull(this)

PackWrite *pckWriteNull(PackWrite *this);

// Write object begin/end
#define pckWriteObjBeginP(this, ...)                                                                                               \
    pckWriteObjBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteObjBegin(PackWrite *this, PackIdParam param);
PackWrite *pckWriteObjEnd(PackWrite *this);

// Read pointer. Use with extreme caution. Pointers cannot be sent to another host -- they must only be used locally.
typedef struct PckWritePtrParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
} PckWritePtrParam;

#define pckWritePtrP(this, value, ...)                                                                                             \
    pckWritePtr(this, value, (PckWritePtrParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWritePtr(PackWrite *this, const void *value, PckWritePtrParam param);

// Write string
typedef struct PckWriteStrParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    const String *defaultValue;
} PckWriteStrParam;

#define pckWriteStrP(this, value, ...)                                                                                             \
    pckWriteStr(this, value, (PckWriteStrParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteStr(PackWrite *this, const String *value, PckWriteStrParam param);

// Write time
typedef struct PckWriteTimeParam
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    time_t defaultValue;
} PckWriteTimeParam;

#define pckWriteTimeP(this, value, ...)                                                                                           \
    pckWriteTime(this, value, (PckWriteTimeParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteTime(PackWrite *this, time_t value, PckWriteTimeParam param);

// Write 32-bit unsigned integer
typedef struct PckWriteUInt32Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    uint32_t defaultValue;
} PckWriteUInt32Param;

#define pckWriteUInt32P(this, value, ...)                                                                                          \
    pckWriteUInt32(this, value, (PckWriteUInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteUInt32(PackWrite *this, uint32_t value, PckWriteUInt32Param param);

// Write 64-bit unsigned integer
typedef struct PckWriteUInt64Param
{
    VAR_PARAM_HEADER;
    bool defaultNull;
    unsigned int id;
    uint64_t defaultValue;
} PckWriteUInt64Param;

#define pckWriteUInt64P(this, value, ...)                                                                                          \
    pckWriteUInt64(this, value, (PckWriteUInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteUInt64(PackWrite *this, uint64_t value, PckWriteUInt64Param param);

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
Helper Functions
***********************************************************************************************************************************/
const String *pckTypeToStr(PackType type);

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
