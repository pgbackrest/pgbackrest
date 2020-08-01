/***********************************************************************************************************************************
Pack Handler

  7 more value indicator
  6 value low-order bit
  5 more id indicator
  4 id low order bit
0-3 type

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
    const String *const name;
} PackTypeData;

static const PackTypeData packTypeData[] =
{
    {
        .type = pckTypeUnknown,
        .name = STRDEF("unknown"),
    },
    {
        .type = pckTypeArray,
        .name = STRDEF("array"),
    },
    {
        .type = pckTypeBin,
        .valueSingleBit = true,
        .size = true,
        .name = STRDEF("bin"),
    },
    {
        .type = pckTypeBool,
        .valueSingleBit = true,
        .name = STRDEF("bool"),
    },
    {
        .type = pckTypeInt32,
        .valueMultiBit = true,
        .name = STRDEF("int32"),
    },
    {
        .type = pckTypeInt64,
        .valueMultiBit = true,
        .name = STRDEF("int64"),
    },
    {
        .type = pckTypeObj,
        .name = STRDEF("obj"),
    },
    {
        .type = pckTypePtr,
        .valueMultiBit = true,
        .name = STRDEF("ptr"),
    },
    {
        .type = pckTypeStr,
        .valueSingleBit = true,
        .size = true,
        .name = STRDEF("str"),
    },
    {
        .type = pckTypeTime,
        .valueMultiBit = true,
        .name = STRDEF("time"),
    },
    {
        .type = pckTypeUInt32,
        .valueMultiBit = true,
        .name = STRDEF("uint32"),
    },
    {
        .type = pckTypeUInt64,
        .valueMultiBit = true,
        .name = STRDEF("uint64"),
    },
};

/***********************************************************************************************************************************
Object type
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
    PackTagStack *tagStackTop;                                      // Top tag on the stack
};

OBJECT_DEFINE_FREE(PACK_READ);

struct PackWrite
{
    MemContext *memContext;                                         // Mem context
    IoWrite *write;                                                 // Write pack to
    bool closeOnEnd;                                                // Close write IO on end

    List *tagStack;                                                 // Stack of object/array tags
    PackTagStack *tagStackTop;                                      // Top tag on the stack
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

        this->tagStackTop = lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj});
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

        this->tagNextId += this->tagStackTop->idLast + 1;
        result = true;
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
static uint64_t
pckReadTag(PackRead *this, unsigned int *id, PackType type, bool peek)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM_P(UINT, id);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BOOL, peek);
    FUNCTION_TEST_END();

    if (*id == 0)
        *id = this->tagStackTop->idLast + 1;
    else if (*id <= this->tagStackTop->idLast)
        THROW_FMT(FormatError, "field %u was already read", *id);

    do
    {
        if (this->tagNextId == 0)
            pckReadTagNext(this);

        if (*id < this->tagNextId)
        {
            if (!peek)
                THROW_FMT(FormatError, "field %u does not exist", *id);

            break;
        }
        else if (*id == this->tagNextId)
        {
            if (!peek)
            {
                if (this->tagNextType != type)
                {
                    THROW_FMT(
                        FormatError, "field %u is type '%s' but expected '%s'", this->tagNextId,
                        strZ(packTypeData[this->tagNextType].name), strZ(packTypeData[type].name));
                }

                this->tagStackTop->idLast = this->tagNextId;
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

        this->tagStackTop->idLast = this->tagNextId;
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
__attribute__((always_inline)) static inline bool
pckReadNullInternal(PackRead *this, unsigned int *id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM_P(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);

    pckReadTag(this, id, pckTypeUnknown, true);

    FUNCTION_TEST_RETURN(*id < this->tagNextId);
}

bool
pckReadNull(PackRead *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(pckReadNullInternal(this, &param.id));
}

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline bool
pckReadDefaultNull(PackRead *this, bool defaultNull, unsigned int *id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(BOOL, defaultNull);
        FUNCTION_TEST_PARAM_P(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);

    if (defaultNull && pckReadNullInternal(this, id))
    {
        this->tagStackTop->idLast = *id;
        FUNCTION_TEST_RETURN(true);
    }

    FUNCTION_TEST_RETURN(false);
}

/**********************************************************************************************************************************/
PackType
pckReadType(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->tagNextType);
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

    pckReadTag(this, &param.id, pckTypeArray, false);
    this->tagStackTop = lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeArray});

    FUNCTION_TEST_RETURN_VOID();
}

void
pckReadArrayEnd(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (lstSize(this->tagStack) == 1 || this->tagStackTop->type != pckTypeArray)
        THROW(FormatError, "not in array");

    // Make sure we are at the end of the array
    unsigned int id = 0xFFFFFFFF - 1;
    pckReadTag(this, &id, pckTypeUnknown, true);

    // Pop array off the stack
    lstRemoveLast(this->tagStack);
    this->tagStackTop = lstGetLast(this->tagStack);

    // Reset tagNextId to keep reading
    this->tagNextId = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
pckReadBool(PackRead *this, PckReadBoolParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(BOOL, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(param.defaultValue);

    FUNCTION_TEST_RETURN(pckReadTag(this, &param.id, pckTypeBool, false));
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

    uint32_t result = (uint32_t)pckReadTag(this, &param.id, pckTypeInt32, false);

    FUNCTION_TEST_RETURN((int32_t)((result >> 1) ^ (~(result & 1) + 1)));
}

/**********************************************************************************************************************************/
int64_t
pckReadInt64(PackRead *this, PckReadInt64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(INT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(param.defaultValue);

    uint64_t result = pckReadTag(this, &param.id, pckTypeInt64, false);

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

    pckReadTag(this, &param.id, pckTypeObj, false);
    this->tagStackTop = lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj});

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
    unsigned id = 0xFFFFFFFF - 1;
    pckReadTag(this, &id, pckTypeUnknown, true);

    // Pop object off the stack
    lstRemoveLast(this->tagStack);
    this->tagStackTop = lstGetLast(this->tagStack);

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

    FUNCTION_TEST_RETURN((void *)(uintptr_t)pckReadTag(this, &param.id, pckTypePtr, false));
}

/**********************************************************************************************************************************/
String *
pckReadStr(PackRead *this, PckReadStrParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(STRING, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(strDup(param.defaultValue));

    String *result = NULL;

    if (pckReadTag(this, &param.id, pckTypeStr, false))
    {
        pckReadSizeBuffer(this);
        result = strNewBuf(this->buffer);
        bufUsedZero(this->buffer);
    }
    else
        result = strNew("");

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
time_t
pckReadTime(PackRead *this, PckReadTimeParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(TIME, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(param.defaultValue);

    uint64_t result = pckReadTag(this, &param.id, pckTypeTime, false);

    FUNCTION_TEST_RETURN((time_t)((result >> 1) ^ (~(result & 1) + 1)));
}

/**********************************************************************************************************************************/
uint32_t
pckReadUInt32(PackRead *this, PckReadUInt32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(UINT32, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(param.defaultValue);

    FUNCTION_TEST_RETURN((uint32_t)pckReadTag(this, &param.id, pckTypeUInt32, false));
}

/**********************************************************************************************************************************/
uint64_t
pckReadUInt64(PackRead *this, PckReadUInt64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(UINT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(param.defaultValue);

    FUNCTION_TEST_RETURN(pckReadTag(this, &param.id, pckTypeUInt64, false));
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
        unsigned int id = 0xFFFFFFFF - 1;
        pckReadTag(this, &id, pckTypeUnknown, true);

        // Remove from stack
        lstRemoveLast(this->tagStack);
    }

    this->tagStackTop = NULL;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
pckReadToLog(const PackRead *this)
{
    return strNewFmt(
        "{depth: %u, idLast: %u, tagNextId: %u, tagNextType: %u, tagNextValue %" PRIu64 "}", lstSize(this->tagStack),
        this->tagStackTop != NULL ? this->tagStackTop->idLast : 0, this->tagNextId, this->tagNextType, this->tagNextValue);
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

        this->tagStackTop = lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj});
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

    if (id == 0)
        id = this->tagStackTop->idLast + this->tagStackTop->nullTotal + 1;
    else
    {
        ASSERT(id > this->tagStackTop->idLast);
    }

    this->tagStackTop->nullTotal = 0;

    uint64_t tag = type;
    unsigned int tagId = id - this->tagStackTop->idLast - 1;

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

    this->tagStackTop->idLast = id;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteNull(PackWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    this->tagStackTop->nullTotal++;

    FUNCTION_TEST_RETURN(this);
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
    this->tagStackTop = lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeArray});

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
    this->tagStackTop = lstGetLast(this->tagStack);

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
pckWriteInt32(PackWrite *this, int32_t value, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(INT, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeInt32, param.id, ((uint32_t)value << 1) ^ (uint32_t)(value >> 31));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteInt64(PackWrite *this, int64_t value, PckWriteInt64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(INT64, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(INT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (param.defaultNull && value == param.defaultValue)
        this->tagStackTop->nullTotal++;
    else
        pckWriteTag(this, pckTypeInt64, param.id, ((uint64_t)value << 1) ^ (uint64_t)(value >> 63));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteObjBegin(PackWrite *this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypeObj, param.id, 0);
    this->tagStackTop = lstAdd(this->tagStack, &(PackTagStack){.type = pckTypeObj});

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
    this->tagStackTop = lstGetLast(this->tagStack);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWritePtr(PackWrite *this, const void *value, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM_P(VOID, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    pckWriteTag(this, pckTypePtr, param.id, (uintptr_t)value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteStr(PackWrite *this, const String *value, PckWriteStrParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(STRING, param.defaultValue);
    FUNCTION_TEST_END();

    if (param.defaultNull && strEq(value, param.defaultValue))
        this->tagStackTop->nullTotal++;
    else
    {
        ASSERT(value != NULL);

        pckWriteTag(this, pckTypeStr, param.id, strSize(value) > 0);

        if (strSize(value) > 0)
        {
            pckWriteUInt64Internal(this, strSize(value));
            ioWrite(this->write, BUF(strZ(value), strSize(value)));
        }
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteTime(PackWrite *this, time_t value, PckWriteTimeParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(TIME, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(TIME, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (param.defaultNull && value == param.defaultValue)
        this->tagStackTop->nullTotal++;
    else
        pckWriteTag(this, pckTypeTime, param.id, ((uint64_t)value << 1) ^ (uint64_t)((int64_t)value >> 63));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteUInt32(PackWrite *this, uint32_t value, PckWriteUInt32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT32, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(UINT32, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (param.defaultNull && value == param.defaultValue)
        this->tagStackTop->nullTotal++;
    else
        pckWriteTag(this, pckTypeUInt32, param.id, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteUInt64(PackWrite *this, uint64_t value, PckWriteUInt64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(UINT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (param.defaultNull && value == param.defaultValue)
        this->tagStackTop->nullTotal++;
    else
        pckWriteTag(this, pckTypeUInt64, param.id, value);

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
    this->tagStackTop = NULL;

    if (this->closeOnEnd)
        ioWriteClose(this->write);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
pckWriteToLog(const PackWrite *this)
{
    return strNewFmt(
        "{depth: %u, idLast: %u}", this->tagStackTop == NULL ? 0 : lstSize(this->tagStack),
        this->tagStackTop == NULL ? 0 : this->tagStackTop->idLast);
}

/**********************************************************************************************************************************/
const String *
pckTypeToStr(PackType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < sizeof(packTypeData) / sizeof(PackTypeData));

    FUNCTION_TEST_RETURN(packTypeData[type].name);
}
