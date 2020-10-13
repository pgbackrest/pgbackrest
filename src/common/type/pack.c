/***********************************************************************************************************************************
Pack Type

Each pack field begins with a one byte tag. The four high order bits of the tag contain the field type (PackType). The four lower
order bits vary by type.

Integer types (packTypeData[type].valueMultiBit) when an unsigned value is <= 1 or a signed value is >= -1 and <= 0:
  3 - more value indicator bit set to 0
  2 - value low order bit
  1 - more ID delta indicator bit
  0 - ID delta low order bit

Integer types (packTypeData[type].valueMultiBit) when an unsigned value is > 1 or a signed value is < -1 or > 0:
  3 - more value indicator bit set to 1
  2 - more ID delta indicator bit
0-1 - ID delta low order bits

String, binary types, and boolean (packTypeData[type].valueSingleBit):
  3 - value bit
  2 - more ID delta indicator bit
0-1 - ID delta low order bits

Note that for string and binary types the value indicates if there is data, not the length of the data, which is stored immediately
following the tag when the value bit is set. This prevents storing an additional byte when the the string/binary length is zero.

Array and object types:
  3 - more ID delta indicator bit
0-2 - ID delta low order bits

When the "more ID delta" indicator is set then the tag will be followed by a base-128 encoded integer with the higher order ID delta
bits. The ID delta represents the delta from the ID of the previous field. When the "more value indicator" then the tag (and the ID
delta, if any) will be followed by a base-128 encoded integer with the high order value bits, i.e. the bits that were not stored
directly in the tag.

For integer types the value is the integer being stored. For string and binary types the value is 1 if the size is greater than 0
and 0 if the size is 0. When the size is greater than 0 the tag is immediately followed the base-128 encoded size and then by the
string/binary bytes.
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/convert.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PACK_UINT64_SIZE_MAX                                        10

/***********************************************************************************************************************************
Type data
***********************************************************************************************************************************/
typedef struct PackTypeData
{
    PackType type;                                                  // Data type
    bool valueSingleBit;                                            // Can the value be stored in a single bit (e.g. bool)
    bool valueMultiBit;                                             // Can the value require multiple bits (e.g. integer)
    bool size;                                                      // Does the type require a size (e.g. string)
    const String *const name;                                       // Type name used in error messages
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
        .type = pckTypeI32,
        .valueMultiBit = true,
        .name = STRDEF("i32"),
    },
    {
        .type = pckTypeI64,
        .valueMultiBit = true,
        .name = STRDEF("i64"),
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
        .type = pckTypeU32,
        .valueMultiBit = true,
        .name = STRDEF("u32"),
    },
    {
        .type = pckTypeU64,
        .valueMultiBit = true,
        .name = STRDEF("u64"),
    },
};

/***********************************************************************************************************************************
Object types
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
    const uint8_t *bufferPtr;                                       // Pointer to buffer
    size_t bufferPos;                                               // Position in the buffer
    size_t bufferUsed;                                              // Amount of data in the buffer

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
    Buffer *buffer;                                                 // Buffer to contain write data

    List *tagStack;                                                 // Stack of object/array tags
    PackTagStack *tagStackTop;                                      // Top tag on the stack
};

OBJECT_DEFINE_FREE(PACK_WRITE);

/**********************************************************************************************************************************/
// Helper to create common data
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
    this->buffer = bufNew(ioBufferSize());
    this->bufferPtr = bufPtr(this->buffer);

    FUNCTION_TEST_RETURN(this);
}

PackRead *
pckReadNewBuf(const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    PackRead *this = pckReadNewInternal();
    this->bufferPtr = bufPtrConst(buffer);
    this->bufferUsed = bufUsed(buffer);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Read bytes from the buffer

IMPORTANT NOTE: To avoid having dyamically created return buffers the current buffer position (this->bufferPos) is stored in the
object. Therefore this function should not be used as a parameter in other function calls since the value of this->bufferPos will
change.
***********************************************************************************************************************************/
static size_t
pckReadBuffer(PackRead *this, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    size_t remaining = this->bufferUsed - this->bufferPos;

    if (remaining < size)
    {
        if (this->read != NULL)
        {
            // Nothing can be remaining since each read fetches exactly the number of bytes required
            ASSERT(remaining == 0);
            bufUsedZero(this->buffer);

            // Limit the buffer for the next read so we don't read past the end of the pack
            bufLimitSet(this->buffer, size < bufSizeAlloc(this->buffer) ? size : bufSizeAlloc(this->buffer));

            // Read bytes
            ioReadSmall(this->read, this->buffer);
            this->bufferPos = 0;
            this->bufferUsed = bufUsed(this->buffer);
            remaining = this->bufferUsed;
        }

        if (remaining < 1)
            THROW(FormatError, "unexpected EOF");

        FUNCTION_TEST_RETURN(remaining < size ? remaining : size);
    }

    FUNCTION_TEST_RETURN(size);
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
    uint8_t byte;

    for (unsigned int bufferIdx = 0; bufferIdx < PACK_UINT64_SIZE_MAX; bufferIdx++)
    {
        pckReadBuffer(this, 1);
        byte = this->bufferPtr[this->bufferPos];

        result |= (uint64_t)(byte & 0x7f) << (7 * bufferIdx);

        if (byte < 0x80)
            break;

        this->bufferPos++;
    }

    if (byte >= 0x80)
        THROW(FormatError, "unterminated base-128 integer");

    this->bufferPos++;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Read next field tag
***********************************************************************************************************************************/
static bool
pckReadTagNext(PackRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    bool result = false;

    // Read the tag byte
    pckReadBuffer(this, 1);
    unsigned int tag = this->bufferPtr[this->bufferPos];
    this->bufferPos++;

    // If the current container is complete (e.g. object)
    if (tag == 0)
    {
        this->tagNextId = 0xFFFFFFFF;
    }
    // Else a regular tag
    else
    {
        // Read field type (e.g. int64, string)
        this->tagNextType = tag >> 4;

        // If the value can contain multiple bits (e.g. integer)
        if (packTypeData[this->tagNextType].valueMultiBit)
        {
            // If the value is stored following the tag (value > 1 bit)
            if (tag & 0x8)
            {
                // Read low order bits of the field ID delta
                this->tagNextId = tag & 0x3;

                // Read high order bits of the field ID delta when specified
                if (tag & 0x4)
                    this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 2;

                // Read value
                this->tagNextValue = pckReadUInt64Internal(this);
            }
            // Else the value is stored in the tag (value == 1 bit)
            else
            {
                // Read low order bit of the field ID delta
                this->tagNextId = tag & 0x1;

                // Read high order bits of the field ID delta when specified
                if (tag & 0x2)
                    this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 1;

                // Read value
                this->tagNextValue = (tag >> 2) & 0x3;
            }
        }
        // Else the value is a single bit (e.g. boolean)
        else if (packTypeData[this->tagNextType].valueSingleBit)
        {
            // Read low order bits of the field ID delta
            this->tagNextId = tag & 0x3;

            // Read high order bits of the field ID delta when specified
            if (tag & 0x4)
                this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 2;

            // Read value
            this->tagNextValue = (tag >> 3) & 0x1;
        }
        // Else the value is multiple tags (e.g. container)
        else
        {
            // Read low order bits of the field ID delta
            this->tagNextId = tag & 0x7;

            // Read high order bits of the field ID delta when specified
            if (tag & 0x8)
                this->tagNextId |= (unsigned int)pckReadUInt64Internal(this) << 3;

            // Value length is variable so is stored after the tag
            this->tagNextValue = 0;
        }

        // Increment the next tag id
        this->tagNextId += this->tagStackTop->idLast + 1;

        // Tag was found
        result = true;
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Read field tag

Some tags and data may be skipped based on the value of the id parameter.
***********************************************************************************************************************************/
static uint64_t
pckReadTag(PackRead *this, unsigned int *id, PackType type, bool peek)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM_P(UINT, id);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BOOL, peek);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);
    ASSERT((peek && type == pckTypeUnknown) || (!peek && type != pckTypeUnknown));

    // Increment the id by one if no id was specified
    if (*id == 0)
    {
        *id = this->tagStackTop->idLast + 1;
    }
    // Else check that the id has been incremented
    else if (*id <= this->tagStackTop->idLast)
        THROW_FMT(FormatError, "field %u was already read", *id);

    // Search for the requested id
    do
    {
        // Get the next tag if it has been read yet
        if (this->tagNextId == 0)
            pckReadTagNext(this);

        // Error when the id does not exist
        if (*id < this->tagNextId)
        {
            if (!peek)
                THROW_FMT(FormatError, "field %u does not exist", *id);

            break;
        }
        // Else the id exists
        else if (*id == this->tagNextId)
        {
            // When not peeking the next tag (just to see what it is) then error if the type is not as specified
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

        // Read data for the field being skipped if this is not the field requested
        if (packTypeData[type].size && this->tagNextValue != 0)
        {
            size_t sizeExpected = (size_t)pckReadUInt64Internal(this);

            while (sizeExpected != 0)
            {
                size_t sizeRead = pckReadBuffer(this, sizeExpected);
                sizeExpected -= sizeRead;
                this->bufferPos += sizeRead;
            }
        }

        // Increment the last id to the id just read
        this->tagStackTop->idLast = this->tagNextId;

        // Read tag on the next iteration
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
// Internal version of pckReadNull() that does not require a PackIdParam struct. Some functions already have an id variable so
// assigning that to a PackIdParam struct and then copying it back is wasteful.
static inline bool
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
Helper function to determine whether a default should be returned when the field is NULL (missing)
***********************************************************************************************************************************/
static inline bool
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
Buffer *
pckReadBin(PackRead *this, PckReadBinParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(NULL);

    Buffer *result = NULL;

    if (pckReadTag(this, &param.id, pckTypeBin, false))
    {
        result = bufNew((size_t)pckReadUInt64Internal(this));

        while (bufUsed(result) < bufSize(result))
        {
            size_t size = pckReadBuffer(this, bufRemains(result));
            bufCatC(result, this->bufferPtr, this->bufferPos, size);
            this->bufferPos += size;
        }
    }
    else
        result = bufNew(0);

    FUNCTION_TEST_RETURN(result);
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
pckReadI32(PackRead *this, PckReadInt32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(INT, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(param.defaultValue);

    FUNCTION_TEST_RETURN(cvtInt32FromZigZag((uint32_t)pckReadTag(this, &param.id, pckTypeI32, false)));
}

/**********************************************************************************************************************************/
int64_t
pckReadI64(PackRead *this, PckReadInt64Param param)
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

    FUNCTION_TEST_RETURN(cvtInt64FromZigZag(pckReadTag(this, &param.id, pckTypeI64, false)));
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
pckReadPtr(PackRead *this, PckReadPtrParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadDefaultNull(this, param.defaultNull, &param.id))
        FUNCTION_TEST_RETURN(NULL);

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
        size_t sizeExpected = (size_t)pckReadUInt64Internal(this);
        result = strNew("");

        while (strSize(result) != sizeExpected)
        {
            size_t sizeRead = pckReadBuffer(this, sizeExpected - strSize(result));
            strCatZN(result, (char *)this->bufferPtr + this->bufferPos, sizeRead);
            this->bufferPos += sizeRead;
        }
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

    FUNCTION_TEST_RETURN((time_t)cvtInt64FromZigZag(pckReadTag(this, &param.id, pckTypeTime, false)));
}

/**********************************************************************************************************************************/
uint32_t
pckReadU32(PackRead *this, PckReadUInt32Param param)
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

    FUNCTION_TEST_RETURN((uint32_t)pckReadTag(this, &param.id, pckTypeU32, false));
}

/**********************************************************************************************************************************/
uint64_t
pckReadU64(PackRead *this, PckReadUInt64Param param)
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

    FUNCTION_TEST_RETURN(pckReadTag(this, &param.id, pckTypeU64, false));
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
// Helper to create common data
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
    this->buffer = bufNew(ioBufferSize());

    FUNCTION_TEST_RETURN(this);
}

PackWrite *
pckWriteNewBuf(Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    PackWrite *this = pckWriteNewInternal();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->buffer = buffer;
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Write to io or buffer
***********************************************************************************************************************************/
static void pckWriteBuffer(PackWrite *this, const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // If writing directly to a buffer
    if (this->write == NULL)
    {
        // Add space in the buffer to write and add extra space so future writes won't always need to resize the buffer
        if (bufRemains(this->buffer) < bufUsed(buffer))
            bufResize(this->buffer, (bufSizeAlloc(this->buffer) + bufUsed(buffer)) + PACK_EXTRA_MIN);

        // Write to the buffer
        bufCat(this->buffer, buffer);
    }
    // Else writing to io
    else
    {
        // If there's enough space to write to the internal buffer then do that
        if (bufRemains(this->buffer) >= bufUsed(buffer))
            bufCat(this->buffer, buffer);
        else
        {
            // Flush the internal buffer if it has data
            if (bufUsed(this->buffer) > 0)
            {
                ioWrite(this->write, this->buffer);
                bufUsedZero(this->buffer);
            }

            // If there's enough space to write to the internal buffer then do that
            if (bufRemains(this->buffer) >= bufUsed(buffer))
            {
                bufCat(this->buffer, buffer);
            }
            // Else write directly to io
            else
                ioWrite(this->write, buffer);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
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

    pckWriteBuffer(this, BUF(buffer, size + 1));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Write field tag
***********************************************************************************************************************************/
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

    // If id is not specified then add one to previous tag (and include all NULLs)
    if (id == 0)
    {
        id = this->tagStackTop->idLast + this->tagStackTop->nullTotal + 1;
    }
    // Else the id must be greater than the last one
    else
        CHECK(id > this->tagStackTop->idLast);

    // Clear NULLs now that field id has been calculated
    this->tagStackTop->nullTotal = 0;

    // Calculate field ID delta
    unsigned int tagId = id - this->tagStackTop->idLast - 1;

    // Write field type (e.g. int64, string)
    uint64_t tag = type << 4;

    // If the value can contain multiple bits (e.g. integer)
    if (packTypeData[type].valueMultiBit)
    {
        // If the value is stored in the tag (value == 1 bit)
        if (value < 2)
        {
            // Write low order bit of the value
            tag |= (value & 0x1) << 2;
            value >>= 1;

            // Write low order bit of the field ID delta
            tag |= tagId & 0x1;
            tagId >>= 1;

            // Set bit to indicate that high order bits of the field ID delta are be written after the tag
            if (tagId > 0)
                tag |= 0x2;
        }
        // Else the value is stored following the tag (value > 1 bit)
        else
        {
            // Set bit to indicate that the value is written after the tag
            tag |= 0x8;

            // Write low order bits of the field ID delta
            tag |= tagId & 0x3;
            tagId >>= 2;

            // Set bit to indicate that high order bits of the field ID delta are be written after the tag
            if (tagId > 0)
                tag |= 0x4;
        }
    }
    // Else the value is a single bit (e.g. boolean)
    else if (packTypeData[type].valueSingleBit)
    {
        // Write value
        tag |= (value & 0x1) << 3;
        value >>= 1;

        // Write low order bits of the field ID delta
        tag |= tagId & 0x3;
        tagId >>= 2;

        // Set bit to indicate that high order bits of the field ID delta are be written after the tag
        if (tagId > 0)
            tag |= 0x4;
    }
    else
    {
        // No value expected
        ASSERT(value == 0);

        // Write low order bits of the field ID delta
        tag |= tagId & 0x7;
        tagId >>= 3;

        // Set bit to indicate that high order bits of the field ID delta must be written after the tag
        if (tagId > 0)
            tag |= 0x8;
    }

    // Write tag
    uint8_t tagByte = (uint8_t)tag;
    pckWriteBuffer(this, BUF(&tagByte, 1));

    // Write low order bits of the field ID delta
    if (tagId > 0)
        pckWriteUInt64Internal(this, tagId);

    // Write low order bits of the value
    if (value > 0)
        pckWriteUInt64Internal(this, value);

    // Set last field id
    this->tagStackTop->idLast = id;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Write a default as NULL (missing)
***********************************************************************************************************************************/
static inline bool
pckWriteDefaultNull(PackWrite *this, bool defaultNull, bool defaultEqual)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BOOL, defaultNull);
        FUNCTION_TEST_PARAM(BOOL, defaultEqual);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (defaultNull && defaultEqual)
    {
        this->tagStackTop->nullTotal++;
        FUNCTION_TEST_RETURN(true);
    }

    FUNCTION_TEST_RETURN(false);
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
pckWriteBin(PackWrite *this, const Buffer *value, PckWriteBinParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BUFFER, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
    FUNCTION_TEST_END();

    if (!pckWriteDefaultNull(this, param.defaultNull, value == NULL))
    {
        ASSERT(value != NULL);

        pckWriteTag(this, pckTypeBin, param.id, bufUsed(value) > 0);

        if (bufUsed(value) > 0)
        {
            pckWriteUInt64Internal(this, bufUsed(value));
            pckWriteBuffer(this, value);
        }
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteBool(PackWrite *this, bool value, PckWriteBoolParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(BOOL, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultNull, value == param.defaultValue))
        pckWriteTag(this, pckTypeBool, param.id, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteI32(PackWrite *this, int32_t value, PckWriteInt32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(INT, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(INT, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultNull, value == param.defaultValue))
        pckWriteTag(this, pckTypeI32, param.id, cvtInt32ToZigZag(value));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteI64(PackWrite *this, int64_t value, PckWriteInt64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(INT64, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(INT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultNull, value == param.defaultValue))
        pckWriteTag(this, pckTypeI64, param.id, cvtInt64ToZigZag(value));

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
pckWritePtr(PackWrite *this, const void *value, PckWritePtrParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM_P(VOID, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultNull, value == NULL))
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

    if (!pckWriteDefaultNull(this, param.defaultNull, strEq(value, param.defaultValue)))
    {
        ASSERT(value != NULL);

        pckWriteTag(this, pckTypeStr, param.id, strSize(value) > 0);

        if (strSize(value) > 0)
        {
            pckWriteUInt64Internal(this, strSize(value));
            pckWriteBuffer(this, BUF(strZ(value), strSize(value)));
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

    if (!pckWriteDefaultNull(this, param.defaultNull, value == param.defaultValue))
        pckWriteTag(this, pckTypeTime, param.id, cvtInt64ToZigZag(value));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteU32(PackWrite *this, uint32_t value, PckWriteUInt32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT32, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(UINT32, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultNull, value == param.defaultValue))
        pckWriteTag(this, pckTypeU32, param.id, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteU64(PackWrite *this, uint64_t value, PckWriteUInt64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultNull);
        FUNCTION_TEST_PARAM(UINT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultNull, value == param.defaultValue))
        pckWriteTag(this, pckTypeU64, param.id, value);

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

    // If writing to io flush the internal buffer
    if (this->write != NULL)
    {
        if (bufUsed(this->buffer) > 0)
            ioWrite(this->write, this->buffer);
    }
    // Else resize the external buffer to trim off extra space added during processing
    else
        bufResize(this->buffer, bufUsed(this->buffer));

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
