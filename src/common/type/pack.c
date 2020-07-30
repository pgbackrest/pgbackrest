/***********************************************************************************************************************************
Pack Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/read.h"
#include "common/io/write.h"
#include "common/log.h" // !!! REMOVE
#include "common/type/pack.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PACK_UINT64_SIZE_MAX                                        10

/***********************************************************************************************************************************
Type data
***********************************************************************************************************************************/
typedef struct PackTypeData
{
    PackType type;
    bool valueSingleBit;
    bool valueMultiBit;
    bool size;
    const char *const name;
} PackTypeData;

static const PackTypeData packTypeData[] =
{
    {
        .type = pckTypeUnknown,
        .name = "unknown",
    },
    {
        .type = pckTypeArray,
        .name = "array",
    },
    {
        .type = pckTypeBin,
        .valueSingleBit = true,
        .size = true,
        .name = "binary",
    },
    {
        .type = pckTypeBool,
        .valueSingleBit = true,
        .name = "boolean",
    },
    {
        .type = pckTypeInt32,
        .valueMultiBit = true,
        .name = "int32",
    },
    {
        .type = pckTypeInt64,
        .valueMultiBit = true,
        .name = "int64",
    },
    {
        .type = pckTypeObj,
        .name = "object",
    },
    {
        .type = pckTypePtr,
        .valueMultiBit = true,
        .name = "pointer",
    },
    {
        .type = pckTypeStr,
        .valueSingleBit = true,
        .size = true,
        .name = "string",
    },
    {
        .type = pckTypeUInt32,
        .valueMultiBit = true,
        .name = "uint32",
    },
    {
        .type = pckTypeUInt64,
        .valueMultiBit = true,
        .name = "uint64",
    },
};

/***********************************************************************************************************************************
Object type

24  byte for allocation/list
  X name
0-1 byte primary
0-1 byte checksumPage
0-1 byte checksumPageError
0-3 byte mode
 12 byte sha1
0-X checksumPageErrorList
0-X user
0-X group
0-2 reference
0-5 size
0-5 sizeRepo
  5 timestamp

0 int
1 sign
2 len
3-7 size

struct   name
   120 +   48 = 168

list + alloc + struct + pack
   8      16       21     18 = 63

2 + 5 + 5 + 5

pgdata/base/1000000000/1000000000.123456

ManifestDataPack
1 bytes flags (primary, checksumPage, checksumPageError, modeDefault, userDefault, groupDefault)
20 bytes sha1
name
pack

    const String *name;                                             // File name (must be first member in struct)
    bool primary:1;                                                 // Should this file be copied from the primary?
    bool checksumPage:1;                                            // Does this file have page checksums?
    bool checksumPageError:1;                                       // Is there an error in the page checksum?
    mode_t mode;                                                    // File mode
    char checksumSha1[HASH_TYPE_SHA1_SIZE_HEX + 1];                 // SHA1 checksum
    const VariantList *checksumPageErrorList;                       // List of page checksum errors if there are any
    const String *user;                                             // User name
    const String *group;                                            // Group name
    const String *reference;                                        // Reference to a prior backup

    uint64_t size;                                                  // Original size
    uint64_t sizeRepo;                                              // Size in repo
    time_t timestamp;                                               // Original timestamp

***********************************************************************************************************************************/
typedef struct PackTagStack
{
    PackType type;
    unsigned int idLast;
    unsigned int nullTotal;
} PackTagStack;

struct PackRead
{
    MemContext *memContext;                                         // Mem context
    IoRead *read;                                                   // Read pack from
    Buffer *buffer;                                                 // Buffer to contain read data

    unsigned int tagNextId;                                         // Next tag id
    PackType tagNextType;                                           // Next tag type
    uint64_t tagNextValue;                                          // Next tag value

    List *tagStack;                                                 // Stack of object/array tags
};

OBJECT_DEFINE_FREE(PACK_READ);

struct PackWrite
{
    MemContext *memContext;                                         // Mem context
    IoWrite *write;                                                 // Write pack to
    bool closeOnEnd;                                                // Close write IO on end

    List *tagStack;                                                 // Stack of object/array tags
};

OBJECT_DEFINE_FREE(PACK_WRITE);

/**********************************************************************************************************************************/
static PackRead *
pckReadNewInternal(void)
{
    FUNCTION_TEST_VOID();

    PackRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PackRead")
    {
        this = memNew(sizeof(PackRead));

        *this = (PackRead)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .buffer = bufNew(PACK_UINT64_SIZE_MAX),
            .tagStack = lstNewP(sizeof(PackTagStack)),
        };

        lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj, .idLast = 0});
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

PackRead *
pckReadNew(IoRead *read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, read);
    FUNCTION_TEST_END();

    ASSERT(read != NULL);

    PackRead *this = pckReadNewInternal();
    this->read = read;

    FUNCTION_TEST_RETURN(this);
}

PackRead *
pckReadNewBuf(const Buffer *read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, read);
    FUNCTION_TEST_END();

    ASSERT(read != NULL);

    PackRead *this = pckReadNewInternal();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->read = ioBufferReadNew(read);
        ioReadOpen(this->read);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Unpack an unsigned 64-bit integer from base-128 varint encoding
***********************************************************************************************************************************/
static uint64_t
pckReadUInt64Internal(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    uint64_t result = 0;

    bufLimitSet(this->buffer, 1);

    for (unsigned int bufferIdx = 0; bufferIdx < PACK_UINT64_SIZE_MAX; bufferIdx++)
    {
        bufUsedZero(this->buffer);
        ioRead(this->read, this->buffer);

        result |= (uint64_t)(bufPtr(this->buffer)[0] & 0x7f) << (7 * bufferIdx);

        if (bufPtr(this->buffer)[0] < 0x80)
            break;
    }

    ASSERT(bufPtr(this->buffer)[0] < 0x80);
    bufUsedZero(this->buffer);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
static void
pckReadSizeBuffer(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    size_t size = (size_t)pckReadUInt64Internal(this);
    bufLimitClear(this->buffer);

    if (size > bufSize(this->buffer))
        bufResize(this->buffer, size);

    bufLimitSet(this->buffer, size);
    ioRead(this->read, this->buffer);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static bool
pckReadTagNext(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    bool result = 0;

    PackTagStack *tagStackTop = lstGetLast(this->tagStack);

    bufLimitSet(this->buffer, 1);
    ioRead(this->read, this->buffer);

    unsigned int tag = bufPtr(this->buffer)[0];
    bufUsedZero(this->buffer);

    if (tag == 0)
        this->tagNextId = 0xFFFFFFFF;
    else
    {
        this->tagNextType = tag & 0xF;

        if (packTypeData[this->tagNextType].valueMultiBit)
        {
            if (tag & 0x80)
            {
                this->tagNextId = (tag >> 4) & 0x3;

                if (tag & 0x40)
                    this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 2;

                this->tagNextValue = pckReadUInt64Internal(this);
            }
            else
            {
                this->tagNextId = (tag >> 4) & 0x1;

                if (tag & 0x20)
                    this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 1;

                this->tagNextValue = (tag >> 6) & 0x1;
            }
        }
        else if (packTypeData[this->tagNextType].valueSingleBit)
        {
            this->tagNextId = (tag >> 4) & 0x3;

            if (tag & 0x40)
                this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 2;

            this->tagNextValue = tag >> 7;
        }
        else
        {
            this->tagNextId = (tag >> 4) & 0x7;

            if (tag & 0x80)
                this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 3;

            this->tagNextValue = 0;
        }

        this->tagNextId += tagStackTop->idLast + 1;
        result = true;
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
static uint64_t
pckReadTag(PackRead *this, unsigned int id, PackType type, bool peek)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BOOL, peek);
    FUNCTION_TEST_END();

    PackTagStack *tagStackTop = lstGetLast(this->tagStack);

    if (id == 0)
        id = tagStackTop->idLast + 1;
    else if (id <= tagStackTop->idLast)
        THROW_FMT(FormatError, "field %u was already read", id);

    do
    {
        if (this->tagNextId == 0)
            pckReadTagNext(this);

        if (id < this->tagNextId)
        {
            if (!peek)
                THROW_FMT(FormatError, "field %u does not exist", id);

            break;
        }
        else if (id == this->tagNextId)
        {
            if (!peek)
            {
                if (this->tagNextType != type)
                {
                    THROW_FMT(
                        FormatError, "field %u is type '%s' but expected '%s'", this->tagNextId,
                        packTypeData[this->tagNextType].name, packTypeData[type].name);
                }

                tagStackTop->idLast = this->tagNextId;
                this->tagNextId = 0;
            }

            break;
        }

        // Read data for the field being skipped
        if (packTypeData[type].size && this->tagNextValue != 0)
        {
            pckReadSizeBuffer(this);
            bufUsedZero(this->buffer);
        }

        tagStackTop->idLast = this->tagNextId;
        this->tagNextId = 0;

    }
    while (1);

    FUNCTION_TEST_RETURN(this->tagNextValue);
}

/**********************************************************************************************************************************/
bool
pckReadNext(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(pckReadTagNext(this));
}

/**********************************************************************************************************************************/
unsigned int
pckReadId(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->tagNextId);
}

/**********************************************************************************************************************************/
bool
pckReadNull(PackRead *this, unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckReadTag(this, id, pckTypeUnknown, true);

    FUNCTION_TEST_RETURN((id == 0 ? ((PackTagStack *)lstGetLast(this->tagStack))->idLast + 1 : id) < this->tagNextId);
}

/**********************************************************************************************************************************/
void
pckReadArrayBegin(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckReadTag(this, param.id, pckTypeArray, false);
    lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeArray, .idLast = 0});

    FUNCTION_TEST_RETURN_VOID();
}

void
pckReadArrayEnd(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (lstSize(this->tagStack) == 1 || ((PackTagStack *)lstGetLast(this->tagStack))->type != pckTypeArray)
        THROW(FormatError, "not in array");

    // Make sure we are at the end of the array
    pckReadTag(this, 0xFFFFFFFF - 1, pckTypeUnknown, true);

    // Pop array off the stack
    lstRemoveLast(this->tagStack);

    // Reset tagNextId to keep reading
    this->tagNextId = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
pckReadBool(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(pckReadTag(this, param.id, pckTypeBool, false));
}

/**********************************************************************************************************************************/
int32_t
pckReadInt32(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    uint32_t result = (uint32_t)pckReadTag(this, param.id, pckTypeInt32, false);

    FUNCTION_TEST_RETURN((int32_t)((result >> 1) ^ (~(result & 1) + 1)));
}

/**********************************************************************************************************************************/
int64_t
pckReadInt64(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    uint64_t result = pckReadTag(this, param.id, pckTypeInt64, false);

    FUNCTION_TEST_RETURN((int64_t)((result >> 1) ^ (~(result & 1) + 1)));
}

/**********************************************************************************************************************************/
void
pckReadObjBegin(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckReadTag(this, param.id, pckTypeObj, false);
    lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj, .idLast = 0});

    FUNCTION_TEST_RETURN_VOID();
}

void
pckReadObjEnd(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (lstSize(this->tagStack) == 1 || ((PackTagStack *)lstGetLast(this->tagStack))->type != pckTypeObj)
        THROW(FormatError, "not in object");

    // Make sure we are at the end of the object
    pckReadTag(this, 0xFFFFFFFF - 1, pckTypeUnknown, true);

    // Pop object off the stack
    lstRemoveLast(this->tagStack);

    // Reset tagNextId to keep reading
    this->tagNextId = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void *
pckReadPtr(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((void *)(uintptr_t)pckReadTag(this, param.id, pckTypePtr, false));
}

/**********************************************************************************************************************************/
String *
pckReadStr(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    if (pckReadTag(this, param.id, pckTypeStr, false))
    {
        pckReadSizeBuffer(this);
        result = strNewBuf(this->buffer);
        bufUsedZero(this->buffer);
    }
    else
        result = strNew("");

    FUNCTION_TEST_RETURN(result);
}

String *
pckReadStrNull(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    if (pckReadNull(this, param.id))
    {
        PackTagStack *tagStackTop = lstGetLast(this->tagStack);
        tagStackTop->idLast = param.id == 0 ? tagStackTop->idLast + 1 : param.id;
    }
    else
        result = pckReadStr(this, param);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
uint32_t
pckReadUInt32(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((uint32_t)pckReadTag(this, param.id, pckTypeUInt32, false));
}

/**********************************************************************************************************************************/
uint64_t
pckReadUInt64(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(pckReadTag(this, param.id, pckTypeUInt64, false));
}

/**********************************************************************************************************************************/
void
pckReadEnd(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    while (lstSize(this->tagStack) > 0)
    {
        // Make sure we are at the end of the container
        pckReadTag(this, 0xFFFFFFFF - 1, pckTypeUnknown, true);

        // Remove from stack
        lstRemoveLast(this->tagStack);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
pckReadToLog(const PackRead *this)
{
    return strNewFmt(
        "{depth: %u, idLast: %u}", lstSize(this->tagStack),
        lstSize(this->tagStack) ? ((PackTagStack *)lstGetLast(this->tagStack))->idLast : 0);
}

/**********************************************************************************************************************************/
static PackWrite *
pckWriteNewInternal(void)
{
    FUNCTION_TEST_VOID();

    PackWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PackWrite")
    {
        this = memNew(sizeof(PackWrite));

        *this = (PackWrite)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .tagStack = lstNewP(sizeof(PackTagStack)),
        };

        lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj, .idLast = 0});
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

PackWrite *
pckWriteNew(IoWrite *write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, write);
    FUNCTION_TEST_END();

    ASSERT(write != NULL);

    PackWrite *this = pckWriteNewInternal();
    this->write = write;

    FUNCTION_TEST_RETURN(this);
}

PackWrite *
pckWriteNewBuf(Buffer *write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, write);
    FUNCTION_TEST_END();

    ASSERT(write != NULL);

    PackWrite *this = pckWriteNewInternal();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->write = ioBufferWriteNew(write);
        ioWriteOpen(this->write);
        this->closeOnEnd = true;
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Pack an unsigned 64-bit integer to base-128 varint encoding
***********************************************************************************************************************************/
static void
pckWriteUInt64Internal(PackWrite *this, uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    unsigned char buffer[PACK_UINT64_SIZE_MAX];
    size_t size = 0;

    while (value >= 0x80)
    {
        buffer[size] = (unsigned char)value | 0x80;
        value >>= 7;
        size++;
    }

    buffer[size] = (unsigned char)value;

    ioWrite(this->write, BUF(buffer, size + 1));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
pckWriteTag(PackWrite *this, PackType type, unsigned int id, uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    PackTagStack *tagStackTop = lstGet(this->tagStack, lstSize(this->tagStack) - 1);

    if (id == 0)
        id = tagStackTop->idLast + tagStackTop->nullTotal + 1;
    else
    {
        ASSERT(id > tagStackTop->idLast);
    }

    tagStackTop->nullTotal = 0;

    uint64_t tag = type;
    unsigned int tagId = id - tagStackTop->idLast - 1;

    if (packTypeData[type].valueMultiBit)
    {
        if (value < 2)
        {
            tag |= (value & 0x1) << 6;
            value >>= 1;

            tag |= (tagId & 0x1) << 4;
            tagId >>= 1;

            if (tagId > 0)
                tag |= 0x20;
        }
        else
        {
            tag |= 0x80;

            tag |= (tagId & 0x3) << 4;
            tagId >>= 2;

            if (tagId > 0)
                tag |= 0x40;
        }
    }
    else if (packTypeData[type].valueSingleBit)
    {
        tag |= (value & 0x1) << 7;
        value >>= 1;

        tag |= (tagId & 0x3) << 4;
        tagId >>= 2;

        if (tagId > 0)
            tag |= 0x40;
    }
    else
    {
        ASSERT(value == 0);

        tag |= (tagId & 0x7) << 4;
        tagId >>= 3;

        if (tagId > 0)
            tag |= 0x80;
    }

    uint8_t tagByte = (uint8_t)tag;
    ioWrite(this->write, BUF(&tagByte, 1));

    if (tagId > 0)
        pckWriteUInt64Internal(this, tagId);

    if (value > 0)
        pckWriteUInt64Internal(this, value);

    tagStackTop->idLast = id;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteArrayBegin(PackWrite *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeArray, param.id, 0);
    lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeArray, .idLast = 0});

    FUNCTION_TEST_RETURN(this);
}

PackWrite *
pckWriteArrayEnd(PackWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(lstSize(this->tagStack) != 1);
    ASSERT(((PackTagStack *)lstGetLast(this->tagStack))->type == pckTypeArray);

    pckWriteUInt64Internal(this, 0);
    lstRemoveLast(this->tagStack);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteBool(PackWrite *this, bool value, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeBool, param.id, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteInt32(PackWrite *this, unsigned int id, int32_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(INT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeInt32, id, ((uint32_t)value << 1) ^ (uint32_t)(value >> 31));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteInt64(PackWrite *this, unsigned int id, int64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(INT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeInt64, id, ((uint64_t)value << 1) ^ (uint64_t)(value >> 63));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteObjBegin(PackWrite *this, unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeObj, id, 0);
    lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj, .idLast = 0});

    FUNCTION_TEST_RETURN(this);
}

PackWrite *
pckWriteObjEnd(PackWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(lstSize(this->tagStack) != 1);
    ASSERT(((PackTagStack *)lstGetLast(this->tagStack))->type == pckTypeObj);

    pckWriteUInt64Internal(this, 0);
    lstRemoveLast(this->tagStack);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWritePtr(PackWrite *this, unsigned int id, const void *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM_P(VOID, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypePtr, id, (uintptr_t)value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteStrZN(PackWrite *this, unsigned int id, const char *value, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (value != NULL)
    {
        pckWriteTag(this, pckTypeStr, id, size > 0);

        if (size > 0)
        {
            pckWriteUInt64Internal(this, size);
            ioWrite(this->write, BUF(value, size));
        }
    }
    else
        ((PackTagStack *)lstGetLast(this->tagStack))->nullTotal++;

    FUNCTION_TEST_RETURN(this);
}

PackWrite *
pckWriteStrZ(PackWrite *this, unsigned int id, const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pckWriteStrZN(this, id, value, value == NULL ? 0 : strlen(value)));
}

PackWrite *
pckWriteStr(PackWrite *this, unsigned int id, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pckWriteStrZN(this, id, strZNull(value), strZNull(value) == NULL ? 0 : strSize(value)));
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteUInt32(PackWrite *this, unsigned int id, uint32_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(UINT32, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeUInt32, id, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteUInt64(PackWrite *this, unsigned int id, uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeUInt64, id, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteEnd(PackWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(lstSize(this->tagStack) == 1);

    pckWriteUInt64Internal(this, 0);

    if (this->closeOnEnd)
        ioWriteClose(this->write);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
pckWriteToLog(const PackWrite *this)
{
    return strNewFmt("{depth: %u, idLast: %u}", lstSize(this->tagStack), ((PackTagStack *)lstGetLast(this->tagStack))->idLast);
}
