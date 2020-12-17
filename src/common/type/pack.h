/***********************************************************************************************************************************
Pack Type

The pack type encodes binary data compactly while still maintaining structure and strict typing. The idea is based on Thrift,
ProtocolBuffers, and Avro, compared here: https://medium.com/better-programming/use-binary-encoding-instead-of-json-dec745ec09b6.
The pack type has been further optimized to balance between purely in-memory structures and those intended to be passed via a
protocol or saved in a file.

Integers are stored with base-128 varint encoding which is equivalent to network byte order, i.e., the endianness of the sending and
receiving host don't matter.

The overall idea is similar to JSON but IDs are used instead of names, typing is more granular, and the representation is far more
compact. A pack can readily be converted to JSON but the reverse is not as precise due to loose typing in JSON. A pack is a stream
format, i.e. it is intended to be read in order from beginning to end.

Fields in a pack are identified by IDs. A field ID is stored as a delta from the previous ID, which is very efficient, but means
that reading from the middle is generally not practical. The size of the gap between field IDs is important -- a gap of 1 never
incurs extra cost, but depending on the field type larger gaps may require additional bytes to store the field ID delta.

NULLs are not stored in a pack and are therefore not typed. A NULL is essentially just a gap in the field IDs. Fields that are
frequently NULL are best stored at the end of an object. When using .defaultWrite = false in write functions a NULL will be written
(by making a gap in the IDs) if the value matches the default. When using read functions the default will always be returned
when the field is NULL (i.e. missing). The standard default is the C default for that type (e.g. bool = false, int = 0) but can be
changed with the .defaultValue parameter. For example, pckWriteBoolP(write, false, .defaultWrite = true) will write a 0 with an ID
into the pack, but pckWriteBoolP(write, false) will not write to the pack, it will simply skip the ID. Note that
pckWriteStrP(packWrite, NULL, .defaultWrite = true) is not valid since there is no way to explcitly write a NULL.

A pack is an object by default. Objects can store fields, objects, or arrays. Objects and arrays will be referred to collectively as
containers. Fields contain data to be stored, e.g. integers, strings, etc.

Here is a simple example of a pack:

PackWrite *write = pckWriteNew(buffer);
pckWriteU64P(write, 77);
pckWriteBoolP(write, false, .defaultWrite = true);
pckWriteI32P(write, -1, .defaultValue = -1);
pckWriteStringP(write, STRDEF("sample"));
pckWriteEndP();

A string representation of this pack is `1:uint64:77,2:bool:false,4:str:sample`. The boolean was stored even though it was the
default because a write was explcitly requested. The int32 field was not stored because the value matched the expicitly set default.
Note that there is a gap in the ID stream, which represents the NULL/default value.

This pack can be read with:

PackRead *read = pckReadNew(buffer);
pckReadU64P(read);
pckReadBoolP(read);
pckReadI32P(read, .defaultValue = -1);
pckReadStringP(read);
pckReadEndP();

Note that defaults are not stored in the pack so any defaults that were applied when writing (by setting .defaulWrite and
optionally .defaultValue) must be applied again when reading (by optionally setting .defaultValue).

If we don't care about the NULL/default, another way to read is:

PackRead *read = pckReadNew(buffer);
pckReadU64P(read);
pckReadBoolP(read);
pckReadStringP(read, .id = 4);
pckReadEndP();

By default each read/write advances the field ID by one. If an ID is specified it must be unique and increasing, because it will
advance the field ID beyond the value specified. An error will occur if an ID is attempted to be read/written but the field ID has
advanced beyond it.

An array can be read with:

pckReadArrayBeginP(read);

while (pckReadNext(read))
{
    // Read array element
}

pckReadArrayEndP(read);

Note that any container (i.e. array or object) resets the field ID to one so there is no need for the caller to maintain a
cumulative field ID. At the end of a container the numbering will continue from wherever the outer container left off.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_PACK_H
#define COMMON_TYPE_PACK_H

#include <time.h>

/***********************************************************************************************************************************
Minimum number of extra bytes to allocate for packs that are growing or are likely to grow
***********************************************************************************************************************************/
#ifndef PACK_EXTRA_MIN
    #define PACK_EXTRA_MIN                                          128
#endif

/***********************************************************************************************************************************
Object types
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
Pack data type
***********************************************************************************************************************************/
typedef enum
{
    pckTypeUnknown = 0,
    pckTypeArray,
    pckTypeBin,
    pckTypeBool,
    pckTypeI32,
    pckTypeI64,
    pckTypeObj,
    pckTypePtr,
    pckTypeStr,
    pckTypeTime,
    pckTypeU32,
    pckTypeU64,
} PackType;

/***********************************************************************************************************************************
Read Constructors
***********************************************************************************************************************************/
PackRead *pckReadNew(IoRead *read);
PackRead *pckReadNewBuf(const Buffer *buffer);

/***********************************************************************************************************************************
Read Functions
***********************************************************************************************************************************/
typedef struct PackIdParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PackIdParam;

// Read next field. This is useful when the type of the next field is unknown, i.e. a completely dynamic data structure, or for
// debugging. If you just need to know if the field exists or not, then use pckReadNullP().
bool pckReadNext(PackRead *this);

// Current field id. Set after a call to pckReadNext().
unsigned int pckReadId(PackRead *this);

// Current field type. Set after a call to pckReadNext().
PackType pckReadType(PackRead *this);

// Is the field NULL? If the field is NULL the id will be advanced so the field does not need to be read explictly. If the field is
// not NULL then the id is not advanced since a subsequent read is expected.
#define pckReadNullP(this, ...)                                                                                                    \
    pckReadNull(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

bool pckReadNull(PackRead *this, PackIdParam param);

// Read array begin/end
#define pckReadArrayBeginP(this, ...)                                                                                              \
    pckReadArrayBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void pckReadArrayBegin(PackRead *this, PackIdParam param);

#define pckReadArrayEndP(this)                                                                                                     \
    pckReadArrayEnd(this)

void pckReadArrayEnd(PackRead *this);

// Read binary
typedef struct PckReadBinParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PckReadBinParam;

#define pckReadBinP(this, ...)                                                                                                     \
    pckReadBin(this, (PckReadBinParam){VAR_PARAM_INIT, __VA_ARGS__})

Buffer *pckReadBin(PackRead *this, PckReadBinParam param);

// Read boolean
typedef struct PckReadBoolParam
{
    VAR_PARAM_HEADER;
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
    unsigned int id;
    int32_t defaultValue;
} PckReadInt32Param;

#define pckReadI32P(this, ...)                                                                                                     \
    pckReadI32(this, (PckReadInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

int32_t pckReadI32(PackRead *this, PckReadInt32Param param);

// Read 64-bit signed integer
typedef struct PckReadInt64Param
{
    VAR_PARAM_HEADER;
    unsigned int id;
    int64_t defaultValue;
} PckReadInt64Param;

#define pckReadI64P(this, ...)                                                                                                     \
    pckReadI64(this, (PckReadInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

int64_t pckReadI64(PackRead *this, PckReadInt64Param param);

// Read object begin/end
#define pckReadObjBeginP(this, ...)                                                                                                \
    pckReadObjBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void pckReadObjBegin(PackRead *this, PackIdParam param);

#define pckReadObjEndP(this)                                                                                                       \
    pckReadObjEnd(this)

void pckReadObjEnd(PackRead *this);

// Read pointer. Use with extreme caution. Pointers cannot be sent to another host -- they must only be used locally.
typedef struct PckReadPtrParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PckReadPtrParam;

#define pckReadPtrP(this, ...)                                                                                                     \
    pckReadPtr(this, (PckReadPtrParam){VAR_PARAM_INIT, __VA_ARGS__})

void *pckReadPtr(PackRead *this, PckReadPtrParam param);

// Read string
typedef struct PckReadStrParam
{
    VAR_PARAM_HEADER;
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
    unsigned int id;
    uint32_t defaultValue;
} PckReadUInt32Param;

#define pckReadU32P(this, ...)                                                                                                     \
    pckReadU32(this, (PckReadUInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

uint32_t pckReadU32(PackRead *this, PckReadUInt32Param param);

// Read 64-bit unsigned integer
typedef struct PckReadUInt64Param
{
    VAR_PARAM_HEADER;
    unsigned int id;
    uint64_t defaultValue;
} PckReadUInt64Param;

#define pckReadU64P(this, ...)                                                                                                     \
    pckReadU64(this, (PckReadUInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

uint64_t pckReadU64(PackRead *this, PckReadUInt64Param param);

// Read end
#define pckReadEndP(this)                                                                                                          \
    pckReadEnd(this)

void pckReadEnd(PackRead *this);

/***********************************************************************************************************************************
Read Destructor
***********************************************************************************************************************************/
void pckReadFree(PackRead *this);

/***********************************************************************************************************************************
Write Constructors
***********************************************************************************************************************************/
PackWrite *pckWriteNew(IoWrite *write);
PackWrite *pckWriteNewBuf(Buffer *buffer);

/***********************************************************************************************************************************
Write Functions
***********************************************************************************************************************************/
// Write array begin/end
#define pckWriteArrayBeginP(this, ...)                                                                                             \
    pckWriteArrayBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteArrayBegin(PackWrite *this, PackIdParam param);

#define pckWriteArrayEndP(this)                                                                                                    \
    pckWriteArrayEnd(this)

PackWrite *pckWriteArrayEnd(PackWrite *this);

// Write binary
typedef struct PckWriteBinParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PckWriteBinParam;

#define pckWriteBinP(this, value, ...)                                                                                             \
    pckWriteBin(this, value, (PckWriteBinParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteBin(PackWrite *this, const Buffer *value, PckWriteBinParam param);

// Write boolean
typedef struct PckWriteBoolParam
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
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
    bool defaultWrite;
    unsigned int id;
    int32_t defaultValue;
} PckWriteInt32Param;

#define pckWriteI32P(this, value, ...)                                                                                             \
    pckWriteI32(this, value, (PckWriteInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteI32(PackWrite *this, int32_t value, PckWriteInt32Param param);

// Write 64-bit signed integer
typedef struct PckWriteInt64Param
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    int64_t defaultValue;
} PckWriteInt64Param;

#define pckWriteI64P(this, value, ...)                                                                                             \
    pckWriteI64(this, value, (PckWriteInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteI64(PackWrite *this, int64_t value, PckWriteInt64Param param);

// Write null
#define pckWriteNullP(this)                                                                                                        \
    pckWriteNull(this)

PackWrite *pckWriteNull(PackWrite *this);

// Write object begin/end
#define pckWriteObjBeginP(this, ...)                                                                                               \
    pckWriteObjBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteObjBegin(PackWrite *this, PackIdParam param);

#define pckWriteObjEndP(this)                                                                                                      \
    pckWriteObjEnd(this)

PackWrite *pckWriteObjEnd(PackWrite *this);

// Write pointer. Use with extreme caution. Pointers cannot be sent to another host -- they must only be used locally.
typedef struct PckWritePtrParam
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
} PckWritePtrParam;

#define pckWritePtrP(this, value, ...)                                                                                             \
    pckWritePtr(this, value, (PckWritePtrParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWritePtr(PackWrite *this, const void *value, PckWritePtrParam param);

// Write string
typedef struct PckWriteStrParam
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
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
    bool defaultWrite;
    unsigned int id;
    time_t defaultValue;
} PckWriteTimeParam;

#define pckWriteTimeP(this, value, ...)                                                                                            \
    pckWriteTime(this, value, (PckWriteTimeParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteTime(PackWrite *this, time_t value, PckWriteTimeParam param);

// Write 32-bit unsigned integer
typedef struct PckWriteUInt32Param
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    uint32_t defaultValue;
} PckWriteUInt32Param;

#define pckWriteU32P(this, value, ...)                                                                                             \
    pckWriteU32(this, value, (PckWriteUInt32Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteU32(PackWrite *this, uint32_t value, PckWriteUInt32Param param);

// Write 64-bit unsigned integer
typedef struct PckWriteUInt64Param
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    uint64_t defaultValue;
} PckWriteUInt64Param;

#define pckWriteU64P(this, value, ...)                                                                                             \
    pckWriteU64(this, value, (PckWriteUInt64Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteU64(PackWrite *this, uint64_t value, PckWriteUInt64Param param);

// Write end
#define pckWriteEndP(this)                                                                                                         \
    pckWriteEnd(this)

PackWrite *pckWriteEnd(PackWrite *this);

/***********************************************************************************************************************************
Write Getters/Setters
***********************************************************************************************************************************/
// Get buffer the pack is writing to (returns NULL if pckWriteNewBuf() was not used to construct the object)
const Buffer *pckWriteBuf(const PackWrite *this);

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
