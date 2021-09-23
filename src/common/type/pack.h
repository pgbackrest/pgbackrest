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

The standard default is the C default for that type (e.g. bool = false, int = 0) but can be changed with the .defaultValue
parameter. For example, pckWriteBoolP(write, false, .defaultWrite = true) will write a 0 (i.e. false) with a field ID into the pack,
but pckWriteBoolP(write, false) will not write to the pack, it will simply skip the ID.  Note that
pckWriteStrP(packWrite, NULL, .defaultWrite = true) is not valid since there is no way to explicitly write a NULL.

NULLs are not stored in a pack and are therefore not typed. A NULL is essentially just a gap in the field IDs. Fields that are
frequently NULL are best stored at the end of an object. When using read functions the default will always be returned
when the field is NULL (i.e. missing). There are times when NULL must be explicitly passed, for example:
pckWriteStrP(resultPack, result.pageChecksumResult != NULL ? jsonFromKv(result.pageChecksumResult) : NULL);
In this case, NULL is declared since jsonFromKv() does not accept a NULL parameter and, following the rules for NULLs the field ID
is skipped when result.pageChecksumResult == NULL. Upon reading, we can declare a NULL_STR when a NULL (field ID gap) is
encountered, e.g. jsonToVar(pckReadStrP(jobResult, .defaultValue = NULL_STR)).

A pack is an object by default. Objects can store fields, objects, or arrays. Objects and arrays will be referred to collectively as
containers. Fields contain data to be stored, e.g. integers, strings, etc.

Here is a simple example of a pack:

PackWrite *write = pckWriteNewP();
pckWriteU64P(write, 77);
pckWriteBoolP(write, false, .defaultWrite = true);
pckWriteI32P(write, -1, .defaultValue = -1);
pckWriteStringP(write, STRDEF("sample"));
pckWriteEndP();

A string representation of this pack is `1:uint64:77,2:bool:false,4:str:sample`. The boolean was stored even though it was the
default because a write was explicitly requested. The int32 field was not stored because the value matched the explicitly set
default. Note that there is a gap in the ID stream, which represents the NULL/default value.

This pack can be read with:

PackRead *read = pckReadNew(pack);
pckReadU64P(read);
pckReadBoolP(read);
pckReadI32P(read, .defaultValue = -1);
pckReadStringP(read);
pckReadEndP();

Note that defaults are not stored in the pack so any defaults that were applied when writing (by setting .defaultValue) must be
applied again when reading by setting .defaultValue if the default value is not a standard C default.

If we don't care about the NULL/default, another way to read is:

PackRead *read = pckReadNew(pack);
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

#include <sys/stat.h>
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
typedef struct Pack Pack;
typedef struct PackRead PackRead;
typedef struct PackWrite PackWrite;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"
#include "common/type/stringId.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Pack data type
***********************************************************************************************************************************/
typedef enum
{
    pckTypeArray = STRID5("array", 0x190ca410),
    pckTypeBin = STRID5("bin", 0x39220),
    pckTypeBool = STRID5("bool", 0x63de20),
    pckTypeI32 = STRID6("i32", 0x1e7c91),
    pckTypeI64 = STRID6("i64", 0x208891),
    pckTypeObj = STRID5("obj", 0x284f0),
    pckTypeMode = STRID5("mode", 0x291ed0),
    pckTypePack = STRID5("pack", 0x58c300),
    pckTypePtr = STRID5("ptr", 0x4a900),
    pckTypeStr = STRID5("str", 0x4a930),
    pckTypeStrId = STRID5("strid", 0x44ca930),
    pckTypeTime = STRID5("time", 0x2b5340),
    pckTypeU32 = STRID6("u32", 0x1e7d51),
    pckTypeU64 = STRID6("u64", 0x208951),
} PackType;

/***********************************************************************************************************************************
Pack Functions
***********************************************************************************************************************************/
// Cast Buffer to Pack
__attribute__((always_inline)) static inline const Pack *
pckFromBuf(const Buffer *const buffer)
{
    return (const Pack *)buffer;
}

// Move to a new parent mem context
__attribute__((always_inline)) static inline Pack *
pckMove(Pack *const this, MemContext *const parentNew)
{
    return (Pack *)bufMove((Buffer *)this, parentNew);
}

// Cast Pack to Buffer
__attribute__((always_inline)) static inline const Buffer *
pckToBuf(const Pack *const pack)
{
    return (const Buffer *)pack;
}

/***********************************************************************************************************************************
Read Constructors
***********************************************************************************************************************************/
// Note that the pack is not moved into the PackRead mem context and must be moved explicitly if the PackRead object is moved.
PackRead *pckReadNew(const Pack *pack);

PackRead *pckReadNewIo(IoRead *read);

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
typedef struct PckReadI32Param
{
    VAR_PARAM_HEADER;
    unsigned int id;
    int32_t defaultValue;
} PckReadI32Param;

#define pckReadI32P(this, ...)                                                                                                     \
    pckReadI32(this, (PckReadI32Param){VAR_PARAM_INIT, __VA_ARGS__})

int32_t pckReadI32(PackRead *this, PckReadI32Param param);

// Read 64-bit signed integer
typedef struct PckReadI64Param
{
    VAR_PARAM_HEADER;
    unsigned int id;
    int64_t defaultValue;
} PckReadI64Param;

#define pckReadI64P(this, ...)                                                                                                     \
    pckReadI64(this, (PckReadI64Param){VAR_PARAM_INIT, __VA_ARGS__})

int64_t pckReadI64(PackRead *this, PckReadI64Param param);

// Read mode
typedef struct PckReadModeParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
    mode_t defaultValue;
} PckReadModeParam;

#define pckReadModeP(this, ...)                                                                                                     \
    pckReadMode(this, (PckReadModeParam){VAR_PARAM_INIT, __VA_ARGS__})

mode_t pckReadMode(PackRead *this, PckReadModeParam param);

// Move to a new parent mem context
__attribute__((always_inline)) static inline PackRead *
pckReadMove(PackRead *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Read object begin/end
#define pckReadObjBeginP(this, ...)                                                                                                \
    pckReadObjBegin(this, (PackIdParam){VAR_PARAM_INIT, __VA_ARGS__})

void pckReadObjBegin(PackRead *this, PackIdParam param);

#define pckReadObjEndP(this)                                                                                                       \
    pckReadObjEnd(this)

void pckReadObjEnd(PackRead *this);

// Read pack
typedef struct PckReadPackParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PckReadPackParam;

#define pckReadPackReadP(this, ...)                                                                                                    \
    pckReadPackRead(this, (PckReadPackParam){VAR_PARAM_INIT, __VA_ARGS__})

PackRead *pckReadPackRead(PackRead *this, PckReadPackParam param);

// Read pack buffer
#define pckReadPackP(this, ...)                                                                                                 \
    pckReadPack(this, (PckReadPackParam){VAR_PARAM_INIT, __VA_ARGS__})

Pack *pckReadPack(PackRead *this, PckReadPackParam param);

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

// Read string id
typedef struct PckReadStrIdParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
    StringId defaultValue;
} PckReadStrIdParam;

#define pckReadStrIdP(this, ...)                                                                                                   \
    pckReadStrId(this, (PckReadStrIdParam){VAR_PARAM_INIT, __VA_ARGS__})

uint64_t pckReadStrId(PackRead *this, PckReadStrIdParam param);

// Read string list
typedef struct PckReadStrLstParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PckReadStrLstParam;

#define pckReadStrLstP(this, ...)                                                                                                  \
    pckReadStrLst(this, (PckReadStrLstParam){VAR_PARAM_INIT, __VA_ARGS__})

StringList *pckReadStrLst(PackRead *const this, PckReadStrLstParam param);

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
typedef struct PckReadU32Param
{
    VAR_PARAM_HEADER;
    unsigned int id;
    uint32_t defaultValue;
} PckReadU32Param;

#define pckReadU32P(this, ...)                                                                                                     \
    pckReadU32(this, (PckReadU32Param){VAR_PARAM_INIT, __VA_ARGS__})

uint32_t pckReadU32(PackRead *this, PckReadU32Param param);

// Read 64-bit unsigned integer
typedef struct PckReadU64Param
{
    VAR_PARAM_HEADER;
    unsigned int id;
    uint64_t defaultValue;
} PckReadU64Param;

#define pckReadU64P(this, ...)                                                                                                     \
    pckReadU64(this, (PckReadU64Param){VAR_PARAM_INIT, __VA_ARGS__})

uint64_t pckReadU64(PackRead *this, PckReadU64Param param);

// Read end
#define pckReadEndP(this)                                                                                                          \
    pckReadEnd(this)

void pckReadEnd(PackRead *this);

/***********************************************************************************************************************************
Read Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
pckReadFree(PackRead *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Write Constructors
***********************************************************************************************************************************/
typedef struct PckWriteNewParam
{
    VAR_PARAM_HEADER;
    size_t size;
} PckWriteNewParam;

#define pckWriteNewP(...)                                                                                                          \
    pckWriteNew((PckWriteNewParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteNew(PckWriteNewParam param);

PackWrite *pckWriteNewIo(IoWrite *write);

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
typedef struct PckWriteI32Param
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    int32_t defaultValue;
} PckWriteI32Param;

#define pckWriteI32P(this, value, ...)                                                                                             \
    pckWriteI32(this, value, (PckWriteI32Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteI32(PackWrite *this, int32_t value, PckWriteI32Param param);

// Write 64-bit signed integer
typedef struct PckWriteI64Param
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    int64_t defaultValue;
} PckWriteI64Param;

#define pckWriteI64P(this, value, ...)                                                                                             \
    pckWriteI64(this, value, (PckWriteI64Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteI64(PackWrite *this, int64_t value, PckWriteI64Param param);

// Write mode
typedef struct PckWriteModeParam
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    mode_t defaultValue;
} PckWriteModeParam;

#define pckWriteModeP(this, value, ...)                                                                                             \
    pckWriteMode(this, value, (PckWriteModeParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteMode(PackWrite *this, mode_t value, PckWriteModeParam param);

// Move to a new parent mem context
__attribute__((always_inline)) static inline PackWrite *
pckWriteMove(PackWrite *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

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

// Write pack
typedef struct PckWritePackParam
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
} PckWritePackParam;

#define pckWritePackP(this, value, ...)                                                                                            \
    pckWritePack(this, value, (PckWritePackParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWritePack(PackWrite *this, const Pack *value, PckWritePackParam param);

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

// Write string id
typedef struct PckWriteStrIdParam
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    StringId defaultValue;
} PckWriteStrIdParam;

#define pckWriteStrIdP(this, value, ...)                                                                                           \
    pckWriteStrId(this, value, (PckWriteStrIdParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteStrId(PackWrite *this, uint64_t value, PckWriteStrIdParam param);

// Write string list
typedef struct PckWriteStrLstParam
{
    VAR_PARAM_HEADER;
    unsigned int id;
} PckWriteStrLstParam;

#define pckWriteStrLstP(this, value, ...)                                                                                          \
    pckWriteStrLst(this, value, (PckWriteStrLstParam){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteStrLst(PackWrite *const this, const StringList *const value, const PckWriteStrLstParam param);

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
typedef struct PckWriteU32Param
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    uint32_t defaultValue;
} PckWriteU32Param;

#define pckWriteU32P(this, value, ...)                                                                                             \
    pckWriteU32(this, value, (PckWriteU32Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteU32(PackWrite *this, uint32_t value, PckWriteU32Param param);

// Write 64-bit unsigned integer
typedef struct PckWriteU64Param
{
    VAR_PARAM_HEADER;
    bool defaultWrite;
    unsigned int id;
    uint64_t defaultValue;
} PckWriteU64Param;

#define pckWriteU64P(this, value, ...)                                                                                             \
    pckWriteU64(this, value, (PckWriteU64Param){VAR_PARAM_INIT, __VA_ARGS__})

PackWrite *pckWriteU64(PackWrite *this, uint64_t value, PckWriteU64Param param);

// Write end
#define pckWriteEndP(this)                                                                                                         \
    pckWriteEnd(this)

PackWrite *pckWriteEnd(PackWrite *this);

/***********************************************************************************************************************************
Write Getters/Setters
***********************************************************************************************************************************/
// Get Pack the PackWrite was writing to (returns NULL if pckWriteNew() was not used to construct the object). This function is only
// valid after pckWriteEndP() has been called.
Pack *pckWriteResult(PackWrite *this);

/***********************************************************************************************************************************
Write Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
pckWriteFree(PackWrite *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_PACK_TYPE                                                                                                     \
    Pack *
#define FUNCTION_LOG_PACK_FORMAT(value, buffer, bufferSize)                                                                        \
    objToLog(value, "Pack", buffer, bufferSize)

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
