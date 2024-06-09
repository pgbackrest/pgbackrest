/***********************************************************************************************************************************
Pack Type

Each pack field begins with a one byte tag. The four high order bits of the tag contain the field type (PackTypeMap). To allow more
types than four bits will allow, the type bits can be set to 0xF and the rest of the type (- 0xF) will be stored by a base-128
encoded integer immediately following the tag. The four lower order bits vary by type.

When the "more ID delta" indicator is set then the tag (and type, if any) will be followed by a base-128 encoded integer with the
higher order ID delta bits. The ID delta represents the delta from the ID of the previous field. When the "more value indicator"
then the tag (and the type and ID delta, if any) will be followed by a base-128 encoded integer with the high order value bits, i.e.
the bits that were not stored directly in the tag.

For integer types the value is the integer being stored but for string and binary types the value is 1 if the size is greater than 0
and 0 if the size is 0. When the size is greater than 0 the tag is immediately followed by (or after the delta ID if "more ID delta"
is set) the base-128 encoded size and then by the string/binary bytes. For string and binary types the value bit indicates if there
is data, not the length of the data, which is why the length is stored immediately following the tag when the value bit is set. This
prevents storing an additional byte when the string/binary length is zero.

The following are definitions for the pack tag field and examples of how it is interpreted.

Integer types (packTypeMapData[type].valueMultiBit) when an unsigned value is <= 1 or a signed value is >= -1 and <= 0:
  3 - more value indicator bit set to 0
  2 - value low order bit
  1 - more ID delta indicator bit
  0 - ID delta low order bit

  Example: b704
    b = unsigned int 64 type
    7 = tag byte low bits: 0 1 1 1 meaning:
        "value low order bit" - the value of the u64 field is 1
        "more ID delta indicator bit" - there exists a gap (i.e. NULLs are not stored so there is a gap between the stored IDs)
        "ID delta low order bit" - gaps are interpreted as the currently stored ID minus previously stored ID minus 1, therefore if
            the previously store ID is 1 and the ID of this u64 field is 11 then a gap of 10 exists. 10 is represented internally as
            9 since there is always at least a gap of 1 which never needs to be recorded (it is a given). 9 in bit format is
            1 0 0 1 - the low-order bit is 1 so the "ID delta low order bit" is set.
    04 = since the low order bit of the internal ID delta was already set in bit 0 of the tag byte, then the remain bits are shifted
        right by one and represented in this second byte as 4. To get the ID delta for 04, shift the 4 back to the left one and then
        add back the "ID delta low order bit" to give a binary representation of 1 0 0 1 = 9. Add back the 1 which is never
        recorded and the ID gap is 10.

Integer types (packTypeMapData[type].valueMultiBit) when an unsigned value is > 1 or a signed value is < -1 or > 0:
  3 - more value indicator bit set to 1
  2 - more ID delta indicator bit
0-1 - ID delta low order bits

  Example: 5e021f
    5 = signed int 64 type
    e = tag byte low bits: 1 1 1 0 meaning:
        "more value indicator bit set to 1" - the actual value is < -1 or > 0
        "more ID delta indicator bit" - there exists a gap (i.e. NULLs are not stored so there is a gap between the stored IDs)
        "ID delta low order bits" - here the bit 1 is set to 1 and bit 0 is not so the ID delta has the second low order bit set but
        not the first
    02 = since bit 0 and bit 1 of the tag byte are accounted for then the 02 is the result of shifting the ID delta right by 2.
        Shifting the 2 back to the left by 2 and adding back the second low order bit as 1 and the first low order bit as 0 then
        the bit representation would be 1 0 1 0 which is ten (10) so the gap between the IDs is 11.
    1f = signed, zigzag representation of -16 (the actual value)

String, binary types, and boolean (packTypeMapData[type].valueSingleBit):
  3 - value bit
  2 - more ID delta indicator bit
0-1 - ID delta low order bits
  Note: binary type is interpreted the same way as string type

  Example: 8c090673616d706c65
    8 = string type
    c = tag byte low bits: 1 1 0 0 meaning:
        "value bit" - there is data
        "more ID delta indicator bit" - there exists a gap (i.e. NULLs are not stored so there is a gap between the stored IDs)
    09 = since neither "ID delta low order bits" is set in the tag, they are both 0, so shifting 9 left by 2, the 2 low order bits
        are now 0 so the result is 0x24 = 36 in decimal. Add back the 1 which is never recorded and the ID gap is 37.
    06 = the length of the string is 6 bytes
    73616d706c65 = the 6 bytes of the string value ("sample")

  Example: 30
    3 = boolean type
    0 = "value bit" 0 means the value is false
    Note that if the boolean had been pack written with .defaultWrite = false there would have been a gap instead of the 30.

Array and object types:
  3 - more ID delta indicator bit
0-2 - ID delta low order bits
  Note: arrays and objects are merely containers for the other pack types.

  Example: 1801 (container begin)
    1 = array type
    8 = "more ID delta indicator bit" - there exists a gap (i.e. NULLs are not stored so there is a gap between the stored IDs)
    01 = since there are three "ID delta low order bits", the 01 will be shifted left by 3 with zeros, resulting in 8. Add back
        the 1 which is never recorded and the ID gap is 9.
    ...
    00 = container end - the array/object container end will occur when a 0 byte (00) is encountered that is not part of a pack
        field within the array/object

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
#include "common/type/pack.h"

/***********************************************************************************************************************************
Map PackType types to the types that will be written into the pack. This hides the details of the type IDs from the user and allows
the IDs used in the pack to differ from the IDs the user sees.
***********************************************************************************************************************************/
typedef enum
{
    pckTypeMapUnknown = 0,                                          // Used internally when the type is not known
    pckTypeMapArray = 1,                                            // Maps to pckTypeArray
    pckTypeMapBool = 2,                                             // Maps to pckTypeBool
    pckTypeMapI32 = 3,                                              // Maps to pckTypeI32
    pckTypeMapI64 = 4,                                              // Maps to pckTypeI64
    pckTypeMapObj = 5,                                              // Maps to pckTypeObj

    // The empty position 6 can be used for a new type that will be encoded entirely in the tag

    pckTypeMapStr = 7,                                              // Maps to pckTypeStr
    pckTypeMapU32 = 8,                                              // Maps to pckTypeU32
    pckTypeMapU64 = 9,                                              // Maps to pckTypeU64
    pckTypeMapStrId = 10,                                           // Maps to pckTypeStrId

    // The empty positions before 15 can be used for new types that will be encoded entirely in the tag

    pckTypeMapTime = 15,                                            // Maps to pckTypeTime
    pckTypeMapBin = 16,                                             // Maps to pckTypeBin
    pckTypeMapPack = 17,                                            // Maps to pckTypePack
    pckTypeMapMode = 18,                                            // Maps to pckTypeMode
} PackTypeMap;

typedef struct PackTypeMapData
{
    PackType type;                                                  // Data type
    bool valueSingleBit;                                            // Can the value be stored in a single bit (e.g. bool)
    bool valueMultiBit;                                             // Can the value require multiple bits (e.g. integer)
    bool size;                                                      // Does the type require a size (e.g. string)
} PackTypeMapData;

static const PackTypeMapData packTypeMapData[] =
{
    // Unknown type map data should not be used
    {0},

    // Formats that can be encoded entirely in the tag
    {
        .type = pckTypeArray,
    },
    {
        .type = pckTypeBool,
        .valueSingleBit = true,
    },
    {
        .type = pckTypeI32,
        .valueMultiBit = true,
    },
    {
        .type = pckTypeI64,
        .valueMultiBit = true,
    },
    {
        .type = pckTypeObj,
    },
    // Placeholders for unused type that can be encoded entirely in the tag
    {0},
    {
        .type = pckTypeStr,
        .valueSingleBit = true,
        .size = true,
    },
    {
        .type = pckTypeU32,
        .valueMultiBit = true,
    },
    {
        .type = pckTypeU64,
        .valueMultiBit = true,
    },
    {
        .type = pckTypeStrId,
        .valueMultiBit = true,
    },

    // Placeholders for unused types that can be encoded entirely in the tag
    {0},
    {0},
    {0},
    {0},

    // Formats that require an extra byte to encode
    {
        .type = pckTypeTime,
        .valueMultiBit = true,
    },
    {
        .type = pckTypeBin,
        .valueSingleBit = true,
        .size = true,
    },
    {
        .type = pckTypePack,
        .size = true,
    },
    {
        .type = pckTypeMode,
        .valueMultiBit = true,
    },
};

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct PackTagStackItem
{
    PackTypeMap typeMap;                                            // Tag type map
    unsigned int idLast;                                            // Last id in the container
    unsigned int nullTotal;                                         // Total nulls since last tag written
} PackTagStackItem;

typedef struct PackTagStack
{
    List *stack;                                                    // Stack of object/array tags
    PackTagStackItem bottom;                                        // Static bottom of the stack
    PackTagStackItem *top;                                          // Top tag on the stack
    unsigned int depth;                                             // Stack depth
} PackTagStack;

struct PackRead
{
    IoRead *read;                                                   // Read pack from
    Buffer *buffer;                                                 // Buffer containing read data
    const uint8_t *bufferPtr;                                       // Pointer to buffer
    size_t bufferPos;                                               // Position in the buffer
    size_t bufferUsed;                                              // Amount of data in the buffer

    unsigned int tagNextId;                                         // Next tag id
    PackTypeMap tagNextTypeMap;                                     // Next tag type map
    uint64_t tagNextValue;                                          // Next tag value
    size_t tagNextSize;                                             // Next tag size

    PackTagStack tagStack;                                          // Stack of object/array tags
};

struct PackWrite
{
    IoWrite *write;                                                 // Write pack to
    Buffer *buffer;                                                 // Buffer to contain write data

    PackTagStack tagStack;                                          // Stack of object/array tags
};

/***********************************************************************************************************************************
Push a container onto the tag stack
***********************************************************************************************************************************/
static void
pckTagStackPush(MemContext *const memContext, PackTagStack *const tagStack, const PackTypeMap typeMap)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, memContext);
        FUNCTION_TEST_PARAM_P(VOID, tagStack);
        FUNCTION_TEST_PARAM(ENUM, typeMap);
    FUNCTION_TEST_END();

    ASSERT(tagStack != NULL);
    ASSERT(typeMap == pckTypeMapArray || typeMap == pckTypeMapObj);

    if (tagStack->stack == NULL)
    {
        MEM_CONTEXT_BEGIN(memContext)
        {
            tagStack->stack = lstNewP(sizeof(PackTagStackItem));
        }
        MEM_CONTEXT_END();
    }

    tagStack->top = lstAdd(tagStack->stack, &(PackTagStackItem){.typeMap = typeMap});
    tagStack->depth++;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Check that a container can be popped off the stack
***********************************************************************************************************************************/
static void
pckTagStackPopCheck(PackTagStack *const tagStack, const PackTypeMap typeMap)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, tagStack);
        FUNCTION_TEST_PARAM(ENUM, typeMap);
    FUNCTION_TEST_END();

    ASSERT(tagStack != NULL);
    ASSERT(typeMap == pckTypeMapArray || typeMap == pckTypeMapObj);

    if (tagStack->depth == 0 || tagStack->top->typeMap != typeMap)
        THROW_FMT(FormatError, "not in %s", strZ(strIdToStr(packTypeMapData[typeMap].type)));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Pop a container off the tag stack
***********************************************************************************************************************************/
static void
pckTagStackPop(PackTagStack *const tagStack)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, tagStack);
    FUNCTION_TEST_END();

    ASSERT(tagStack != NULL);
    ASSERT(tagStack->depth != 0);
    ASSERT(tagStack->stack != NULL);

    lstRemoveLast(tagStack->stack);
    tagStack->depth--;

    if (tagStack->depth == 0)
        tagStack->top = &tagStack->bottom;
    else
        tagStack->top = lstGetLast(tagStack->stack);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to create common data
static PackRead *
pckReadNewInternal(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(PackRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (PackRead)
        {
            .tagStack = {.bottom = {.typeMap = pckTypeMapObj}},
        };

        this->tagStack.top = &this->tagStack.bottom;
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(PACK_READ, this);
}

FN_EXTERN PackRead *
pckReadNewIo(IoRead *const read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, read);
    FUNCTION_TEST_END();

    ASSERT(read != NULL);

    PackRead *const this = pckReadNewInternal();

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->read = read;
        this->buffer = bufNew(ioBufferSize());
        this->bufferPtr = bufPtr(this->buffer);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(PACK_READ, this);
}

FN_EXTERN PackRead *
pckReadNew(const Pack *const pack)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK, pack);
    FUNCTION_TEST_END();

    if (pack == NULL)
        FUNCTION_TEST_RETURN(PACK_READ, NULL);

    FUNCTION_TEST_RETURN(PACK_READ, pckReadNewC(bufPtrConst((const Buffer *)pack), bufUsed((const Buffer *)pack)));
}

FN_EXTERN PackRead *
pckReadNewC(const unsigned char *const buffer, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    PackRead *const this = pckReadNewInternal();
    this->bufferPtr = buffer;
    this->bufferUsed = size;

    FUNCTION_TEST_RETURN(PACK_READ, this);
}

/***********************************************************************************************************************************
Read bytes from the buffer

IMPORTANT NOTE: To avoid having dynamically created return buffers the current buffer position (this->bufferPos) is stored in the
object. Therefore this function should not be used as a parameter in other function calls since the value of this->bufferPos will
change.
***********************************************************************************************************************************/
static size_t
pckReadBuffer(PackRead *const this, const size_t size)
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

        FUNCTION_TEST_RETURN(SIZE, remaining < size ? remaining : size);
    }

    FUNCTION_TEST_RETURN(SIZE, size);
}

/***********************************************************************************************************************************
Consume buffer left in tagNextSize
***********************************************************************************************************************************/
static void
pckReadConsumeBuffer(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    while (this->tagNextSize != 0)
    {
        const size_t size = pckReadBuffer(this, this->tagNextSize);
        this->bufferPos += size;
        this->tagNextSize -= size;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Unpack an unsigned 64-bit integer from base-128 varint encoding
***********************************************************************************************************************************/
static uint64_t
pckReadU64Internal(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->read != NULL)
    {
        // Internal buffer should be empty
        ASSERT(this->bufferUsed == this->bufferPos);
        FUNCTION_TEST_RETURN(UINT64, ioReadVarIntU64(this->read));
    }

    FUNCTION_TEST_RETURN(UINT64, cvtUInt64FromVarInt128(this->bufferPtr, &this->bufferPos, this->bufferUsed));
}

/***********************************************************************************************************************************
Read next field tag
***********************************************************************************************************************************/
static bool
pckReadTagNext(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool result = false;

    // Read the tag byte
    pckReadBuffer(this, 1);
    const unsigned int tag = this->bufferPtr[this->bufferPos];
    this->bufferPos++;

    // If the current container is complete (e.g. object)
    if (tag == 0)
    {
        this->tagNextId = UINT_MAX;
    }
    // Else a regular tag
    else
    {
        // Read field type (e.g. int64, string)
        this->tagNextTypeMap = tag >> 4;

        if (this->tagNextTypeMap == 0xF)
            this->tagNextTypeMap = (unsigned int)pckReadU64Internal(this) + 0xF;

        CHECK(
            FormatError, this->tagNextTypeMap < LENGTH_OF(packTypeMapData) && packTypeMapData[this->tagNextTypeMap].type != 0,
            "invalid tag type");

        // If the value can contain multiple bits (e.g. integer)
        if (packTypeMapData[this->tagNextTypeMap].valueMultiBit)
        {
            // If the value is stored following the tag (value > 1 bit)
            if (tag & 0x8)
            {
                // Read low order bits of the field ID delta
                this->tagNextId = tag & 0x3;

                // Read high order bits of the field ID delta when specified
                if (tag & 0x4)
                    this->tagNextId |= (unsigned int)pckReadU64Internal(this) << 2;

                // Read value
                this->tagNextValue = pckReadU64Internal(this);
            }
            // Else the value is stored in the tag (value == 1 bit)
            else
            {
                // Read low order bit of the field ID delta
                this->tagNextId = tag & 0x1;

                // Read high order bits of the field ID delta when specified
                if (tag & 0x2)
                    this->tagNextId |= (unsigned int)pckReadU64Internal(this) << 1;

                // Read value
                this->tagNextValue = (tag >> 2) & 0x3;
            }
        }
        // Else the value is a single bit (e.g. boolean)
        else if (packTypeMapData[this->tagNextTypeMap].valueSingleBit)
        {
            // Read low order bits of the field ID delta
            this->tagNextId = tag & 0x3;

            // Read high order bits of the field ID delta when specified
            if (tag & 0x4)
                this->tagNextId |= (unsigned int)pckReadU64Internal(this) << 2;

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
                this->tagNextId |= (unsigned int)pckReadU64Internal(this) << 3;

            // Value length is variable so is stored after the tag
            this->tagNextValue = 0;
        }

        // Increment the next tag id
        this->tagNextId += this->tagStack.top->idLast + 1;

        // Get tag size if it exists
        if (packTypeMapData[this->tagNextTypeMap].size &&
            (this->tagNextValue > 0 || !packTypeMapData[this->tagNextTypeMap].valueSingleBit))
        {
            this->tagNextSize = (size_t)pckReadU64Internal(this);
        }
        else
            this->tagNextSize = 0;

        // Tag was found
        result = true;
    }

    FUNCTION_TEST_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read field tag

Some tags and data may be skipped based on the value of the id parameter.
***********************************************************************************************************************************/
static uint64_t
pckReadTag(PackRead *const this, unsigned int *const id, const PackTypeMap typeMap, const bool peek)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM_P(UINT, id);
        FUNCTION_TEST_PARAM(ENUM, typeMap);
        FUNCTION_TEST_PARAM(BOOL, peek);                            // Look at the next tag without advancing the field id
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);
    ASSERT((peek && typeMap == pckTypeMapUnknown) || (!peek && typeMap != pckTypeMapUnknown));

    // Increment the id by one if no id was specified
    if (*id == 0)
    {
        *id = this->tagStack.top->idLast + 1;
    }
    // Else check that the id has been incremented
    else if (*id <= this->tagStack.top->idLast)
        THROW_FMT(FormatError, "field %u was already read", *id);

    // Search for the requested id
    do
    {
        // Get the next tag if it has not been read yet
        if (this->tagNextId == 0)
            pckReadTagNext(this);

        // Return if the id does not exist
        if (*id < this->tagNextId)
        {
            break;
        }
        // Else the id exists
        else if (*id == this->tagNextId)
        {
            // When not peeking the next tag (just to see what it is) then error if the type map is not as specified
            if (!peek)
            {
                if (this->tagNextTypeMap != typeMap)
                {
                    THROW_FMT(
                        FormatError, "field %u is type '%s' but expected '%s'", this->tagNextId,
                        strZ(strIdToStr(packTypeMapData[this->tagNextTypeMap].type)),
                        strZ(strIdToStr(packTypeMapData[typeMap].type)));
                }

                this->tagStack.top->idLast = this->tagNextId;
                this->tagNextId = 0;
            }

            break;
        }

        // Read data for the field being skipped if this is not the field requested
        pckReadConsumeBuffer(this);

        // Increment the last id to the id just read
        this->tagStack.top->idLast = this->tagNextId;

        // Read tag on the next iteration
        this->tagNextId = 0;
    }
    while (1);

    FUNCTION_TEST_RETURN(UINT64, this->tagNextValue);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
pckReadNext(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->tagNextId == 0);

    FUNCTION_TEST_RETURN(BOOL, pckReadTagNext(this));
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
pckReadId(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(UINT, this->tagNextId);
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
pckReadSize(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(packTypeMapData[this->tagNextTypeMap].size);

    FUNCTION_TEST_RETURN(SIZE, this->tagNextSize);
}

/**********************************************************************************************************************************/
FN_EXTERN void
pckReadConsume(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    unsigned int id = 0;

    pckReadTag(this, &id, pckTypeMapUnknown, true);
    pckReadTag(this, &id, this->tagNextTypeMap, false);
    pckReadConsumeBuffer(this);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN const unsigned char *
pckReadBufPtr(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(packTypeMapData[this->tagNextTypeMap].size);
    ASSERT(this->buffer == NULL);

    FUNCTION_TEST_RETURN_CONST_P(UCHARDATA, this->bufferPtr + this->bufferPos);
}

/**********************************************************************************************************************************/
// Internal version of pckReadNull() that does not require a PackIdParam struct. Some functions already have an id variable so
// assigning that to a PackIdParam struct and then copying it back is wasteful.
static inline bool
pckReadNullInternal(PackRead *const this, unsigned int *const id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM_P(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);

    // Read tag at specified id
    pckReadTag(this, id, pckTypeMapUnknown, true);

    // If the field is NULL then set idLast (to avoid rechecking the same id on the next call) and return true
    if (*id < this->tagNextId)
    {
        this->tagStack.top->idLast = *id;
        FUNCTION_TEST_RETURN(BOOL, true);
    }

    // The field is not NULL
    FUNCTION_TEST_RETURN(BOOL, false);
}

FN_EXTERN bool
pckReadNull(PackRead *const this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, pckReadNullInternal(this, &param.id));
}

/**********************************************************************************************************************************/
FN_EXTERN PackType
pckReadType(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(STRING_ID, packTypeMapData[this->tagNextTypeMap].type);
}

/**********************************************************************************************************************************/
FN_EXTERN void
pckReadArrayBegin(PackRead *const this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Read array begin
    pckReadTag(this, &param.id, pckTypeMapArray, false);

    // Add array to the tag stack so IDs can be tracked separately from the parent container
    pckTagStackPush(objMemContext(this), &this->tagStack, pckTypeMapArray);

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
pckReadArrayEnd(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Check tag stack pop
    pckTagStackPopCheck(&this->tagStack, pckTypeMapArray);

    // Make sure we are at the end of the array
    unsigned int id = UINT_MAX - 1;
    pckReadTag(this, &id, pckTypeMapUnknown, true);

    // Pop array off the stack
    pckTagStackPop(&this->tagStack);

    // Reset tagNextId to keep reading
    this->tagNextId = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
pckReadBin(PackRead *const this, PckReadBinParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(BUFFER, NULL);

    Buffer *result;

    // If buffer size > 0
    if (pckReadTag(this, &param.id, pckTypeMapBin, false))
    {
        // Get the buffer size
        result = bufNew(this->tagNextSize);

        // Read the buffer out in chunks
        while (bufUsed(result) < bufSize(result))
        {
            size_t size = pckReadBuffer(this, bufRemains(result));
            bufCatC(result, this->bufferPtr, this->bufferPos, size);
            this->bufferPos += size;
        }
    }
    // Else return a zero-sized buffer
    else
        result = bufNew(0);

    FUNCTION_TEST_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
pckReadBool(PackRead *const this, PckReadBoolParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(BOOL, param.defaultValue);

    FUNCTION_TEST_RETURN(BOOL, pckReadTag(this, &param.id, pckTypeMapBool, false));
}

/**********************************************************************************************************************************/
FN_EXTERN int32_t
pckReadI32(PackRead *const this, PckReadI32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(INT, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(INT, param.defaultValue);

    FUNCTION_TEST_RETURN(INT, cvtInt32FromZigZag((uint32_t)pckReadTag(this, &param.id, pckTypeMapI32, false)));
}

/**********************************************************************************************************************************/
FN_EXTERN int64_t
pckReadI64(PackRead *const this, PckReadI64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(INT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(INT64, param.defaultValue);

    FUNCTION_TEST_RETURN(INT64, cvtInt64FromZigZag(pckReadTag(this, &param.id, pckTypeMapI64, false)));
}

/**********************************************************************************************************************************/
FN_EXTERN mode_t
pckReadMode(PackRead *const this, PckReadModeParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(MODE, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(MODE, param.defaultValue);

    FUNCTION_TEST_RETURN(MODE, (mode_t)pckReadTag(this, &param.id, pckTypeMapMode, false));
}

/**********************************************************************************************************************************/
FN_EXTERN void
pckReadObjBegin(PackRead *const this, PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Read object begin
    pckReadTag(this, &param.id, pckTypeMapObj, false);

    // Add object to the tag stack so IDs can be tracked separately from the parent container
    pckTagStackPush(objMemContext(this), &this->tagStack, pckTypeMapObj);

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
pckReadObjEnd(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Check tag stack pop
    pckTagStackPopCheck(&this->tagStack, pckTypeMapObj);

    // Make sure we are at the end of the object
    unsigned id = UINT_MAX - 1;
    pckReadTag(this, &id, pckTypeMapUnknown, true);

    // Pop object off the stack
    pckTagStackPop(&this->tagStack);

    // Reset tagNextId to keep reading
    this->tagNextId = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN PackRead *
pckReadPackRead(PackRead *const this, PckReadPackParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    Pack *const pack = pckReadPack(this, param);
    PackRead *const result = pckReadNew(pack);

    if (result != NULL)
        pckMove(pack, objMemContext(result));

    FUNCTION_TEST_RETURN(PACK_READ, result);
}

FN_EXTERN PackRead *
pckReadPackReadConst(PackRead *const this, PckReadPackParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(PACK_READ, NULL);

    // Read the tag
    pckReadTag(this, &param.id, pckTypeMapPack, false);

    PackRead *const result = pckReadNewC(pckReadBufPtr(this), pckReadSize(this));

    pckReadConsumeBuffer(this);

    FUNCTION_TEST_RETURN(PACK_READ, result);
}

FN_EXTERN Pack *
pckReadPack(PackRead *const this, PckReadPackParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(PACK, NULL);

    // Read the tag
    pckReadTag(this, &param.id, pckTypeMapPack, false);

    // Get the pack size
    Buffer *const result = OBJ_NAME(bufNew(this->tagNextSize), Pack::Buffer);

    // Read the pack out in chunks
    while (bufUsed(result) < bufSize(result))
    {
        size_t size = pckReadBuffer(this, bufRemains(result));
        bufCatC(result, this->bufferPtr, this->bufferPos, size);
        this->bufferPos += size;
    }

    FUNCTION_TEST_RETURN(PACK, (Pack *)result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
pckReadStr(PackRead *const this, PckReadStrParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(STRING, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(STRING, strDup(param.defaultValue));

    String *result;

    // If string size > 0
    if (pckReadTag(this, &param.id, pckTypeMapStr, false))
    {
        // Read the string size
        const size_t sizeExpected = (size_t)this->tagNextSize;

        // Read the string out in chunks
        result = strNew();

        while (strSize(result) != sizeExpected)
        {
            const size_t sizeRead = pckReadBuffer(this, sizeExpected - strSize(result));
            strCatZN(result, (char *)this->bufferPtr + this->bufferPos, sizeRead);
            this->bufferPos += sizeRead;
        }
    }
    // Else return an empty string
    else
        result = strNew();

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN StringId
pckReadStrId(PackRead *const this, PckReadStrIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(UINT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(STRING_ID, param.defaultValue);

    FUNCTION_TEST_RETURN(STRING_ID, pckReadTag(this, &param.id, pckTypeMapStrId, false));
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
pckReadStrLst(PackRead *const this, PckReadStrLstParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(STRING_LIST, NULL);

    pckReadArrayBeginP(this, .id = param.id);

    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        while (!pckReadNullP(this))
            strLstAdd(result, pckReadStrP(this));
    }
    MEM_CONTEXT_TEMP_END();

    pckReadArrayEndP(this);

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN time_t
pckReadTime(PackRead *const this, PckReadTimeParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(TIME, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(TIME, param.defaultValue);

    FUNCTION_TEST_RETURN(TIME, (time_t)cvtInt64FromZigZag(pckReadTag(this, &param.id, pckTypeMapTime, false)));
}

/**********************************************************************************************************************************/
FN_EXTERN uint32_t
pckReadU32(PackRead *const this, PckReadU32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(UINT32, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(UINT32, param.defaultValue);

    FUNCTION_TEST_RETURN(UINT32, (uint32_t)pckReadTag(this, &param.id, pckTypeMapU32, false));
}

/**********************************************************************************************************************************/
FN_EXTERN uint64_t
pckReadU64(PackRead *const this, PckReadU64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(UINT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (pckReadNullInternal(this, &param.id))
        FUNCTION_TEST_RETURN(UINT64, param.defaultValue);

    FUNCTION_TEST_RETURN(UINT64, pckReadTag(this, &param.id, pckTypeMapU64, false));
}

/**********************************************************************************************************************************/
FN_EXTERN void
pckReadEnd(PackRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    CHECK(FormatError, this->tagStack.depth == 0, "invalid pack end read in container");

    // Make sure we are at the end of the pack
    unsigned int id = UINT_MAX - 1;
    pckReadTag(this, &id, pckTypeMapUnknown, true);

    this->tagStack.top = NULL;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
pckReadToLog(const PackRead *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{depth: %u, idLast: %u, tagNextId: %u, tagNextType: %u, tagNextValue %" PRIu64 "}", this->tagStack.depth,
        this->tagStack.top->idLast, this->tagNextId, this->tagNextTypeMap, this->tagNextValue);
}

/**********************************************************************************************************************************/
// Helper to create common data
static PackWrite *
pckWriteNewInternal(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(PackWrite, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (PackWrite)
        {
            .tagStack = {.bottom = {.typeMap = pckTypeMapObj}},
        };

        this->tagStack.top = &this->tagStack.bottom;
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

FN_EXTERN PackWrite *
pckWriteNew(const PckWriteNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, param.size);
    FUNCTION_TEST_END();

    PackWrite *this = pckWriteNewInternal();

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->buffer = bufNew(param.size == 0 ? PACK_EXTRA_MIN : param.size);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

FN_EXTERN PackWrite *
pckWriteNewIo(IoWrite *const write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, write);
    FUNCTION_TEST_END();

    ASSERT(write != NULL);

    PackWrite *const this = pckWriteNewInternal();

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->write = write;
        this->buffer = bufNew(ioBufferSize());
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/***********************************************************************************************************************************
Write to io or buffer
***********************************************************************************************************************************/
static void
pckWriteBuffer(PackWrite *const this, const Buffer *const buffer)
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
            if (!bufEmpty(this->buffer))
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
pckWriteU64Internal(PackWrite *const this, const uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    unsigned char buffer[CVT_VARINT128_BUFFER_SIZE];
    size_t size = 0;

    cvtUInt64ToVarInt128(value, buffer, &size, sizeof(buffer));

    // Write encoded bytes to the buffer
    pckWriteBuffer(this, BUF(buffer, size));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Write field tag
***********************************************************************************************************************************/
static void
pckWriteTag(PackWrite *const this, const PackTypeMap typeMap, unsigned int id, uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(ENUM, typeMap);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // If id is not specified then add one to previous tag (and include all NULLs)
    if (id == 0)
    {
        id = this->tagStack.top->idLast + this->tagStack.top->nullTotal + 1;
    }
    // Else the id must be greater than the last one
    else
        CHECK(AssertError, id > this->tagStack.top->idLast, "field id must be greater than last id");

    // Clear NULLs now that field id has been calculated
    this->tagStack.top->nullTotal = 0;

    // Calculate field ID delta
    unsigned int tagId = id - this->tagStack.top->idLast - 1;

    // Write field type map (e.g. int64, string)
    uint64_t tag = typeMap >= 0xF ? 0xF0 : typeMap << 4;

    // If the value can contain multiple bits (e.g. integer)
    if (packTypeMapData[typeMap].valueMultiBit)
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
    else if (packTypeMapData[typeMap].valueSingleBit)
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

    // Write remaining type map
    if (typeMap >= 0xF)
        pckWriteU64Internal(this, typeMap - 0xF);

    // Write low order bits of the field ID delta
    if (tagId > 0)
        pckWriteU64Internal(this, tagId);

    // Write low order bits of the value
    if (value > 0)
        pckWriteU64Internal(this, value);

    // Set last field id
    this->tagStack.top->idLast = id;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Write a default as NULL (missing)
***********************************************************************************************************************************/
static inline bool
pckWriteDefaultNull(PackWrite *const this, const bool defaultWrite, const bool defaultEqual)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BOOL, defaultWrite);
        FUNCTION_TEST_PARAM(BOOL, defaultEqual);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Write a NULL if not forcing the default to be written and the value passed equals the default
    if (!defaultWrite && defaultEqual)
    {
        this->tagStack.top->nullTotal++;
        FUNCTION_TEST_RETURN(BOOL, true);
    }

    // Let the caller know that it should write the value
    FUNCTION_TEST_RETURN(BOOL, false);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteNull(PackWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    this->tagStack.top->nullTotal++;

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteArrayBegin(PackWrite *const this, const PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Write the array tag
    pckWriteTag(this, pckTypeMapArray, param.id, 0);

    // Add array to the tag stack so IDs can be tracked separately from the parent container
    pckTagStackPush(objMemContext(this), &this->tagStack, pckTypeMapArray);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

FN_EXTERN PackWrite *
pckWriteArrayEnd(PackWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Check tag stack pop
    pckTagStackPopCheck(&this->tagStack, pckTypeMapArray);

    // Write end of array tag
    pckWriteU64Internal(this, 0);

    // Pop array off the stack to revert to ID tracking for the prior container
    pckTagStackPop(&this->tagStack);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteBin(PackWrite *const this, const Buffer *const value, const PckWriteBinParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BUFFER, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, false, value == NULL))
    {
        ASSERT(value != NULL);

        // Write buffer size if > 0
        pckWriteTag(this, pckTypeMapBin, param.id, !bufEmpty(value));

        // Write buffer data if size > 0
        if (!bufEmpty(value))
        {
            pckWriteU64Internal(this, bufUsed(value));
            pckWriteBuffer(this, value);
        }
    }

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteBool(PackWrite *const this, const bool value, const PckWriteBoolParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(BOOL, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapBool, param.id, value);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteI32(PackWrite *const this, const int32_t value, const PckWriteI32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(INT, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(INT, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapI32, param.id, cvtInt32ToZigZag(value));

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteI64(PackWrite *const this, const int64_t value, const PckWriteI64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(INT64, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(INT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapI64, param.id, cvtInt64ToZigZag(value));

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteMode(PackWrite *const this, const mode_t value, const PckWriteModeParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT32, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(MODE, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapMode, param.id, value);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteObjBegin(PackWrite *const this, const PackIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Write the object tag
    pckWriteTag(this, pckTypeMapObj, param.id, 0);

    // Add object to the tag stack so IDs can be tracked separately from the parent container
    pckTagStackPush(objMemContext(this), &this->tagStack, pckTypeMapObj);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

FN_EXTERN PackWrite *
pckWriteObjEnd(PackWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Check tag stack pop
    pckTagStackPopCheck(&this->tagStack, pckTypeMapObj);

    // Write end of object tag
    pckWriteU64Internal(this, 0);

    // Pop object off the stack to revert to ID tracking for the prior container
    pckTagStackPop(&this->tagStack);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWritePack(PackWrite *const this, const Pack *const value, const PckWritePackParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(BUFFER, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, false, value == NULL))
    {
        ASSERT(value != NULL);

        pckWriteTag(this, pckTypeMapPack, param.id, 0);
        pckWriteU64Internal(this, bufUsed((Buffer *)value));
        pckWriteBuffer(this, (Buffer *)value);
    }

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteStr(PackWrite *this, const String *const value, const PckWriteStrParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(STRING, param.defaultValue);
    FUNCTION_TEST_END();

    if (!pckWriteDefaultNull(this, param.defaultWrite, strEq(value, param.defaultValue)))
    {
        ASSERT(value != NULL);

        // Write string size if > 0
        pckWriteTag(this, pckTypeMapStr, param.id, strSize(value) > 0);

        // Write string data if size > 0
        if (strSize(value) > 0)
        {
            pckWriteU64Internal(this, strSize(value));
            pckWriteBuffer(this, BUF(strZ(value), strSize(value)));
        }
    }

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteStrId(PackWrite *const this, const uint64_t value, const PckWriteStrIdParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(UINT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapStrId, param.id, value);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteStrLst(PackWrite *const this, const StringList *const value, const PckWriteStrLstParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(STRING_LIST, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, false, value == NULL))
    {
        ASSERT(value != NULL);

        pckWriteArrayBeginP(this, .id = param.id);

        for (unsigned int valueIdx = 0; valueIdx < strLstSize(value); valueIdx++)
            pckWriteStrP(this, strLstGet(value, valueIdx));

        pckWriteArrayEndP(this);
    }

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteTime(PackWrite *const this, const time_t value, const PckWriteTimeParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(TIME, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(TIME, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapTime, param.id, cvtInt64ToZigZag(value));

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteU32(PackWrite *const this, const uint32_t value, const PckWriteU32Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT32, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(UINT32, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapU32, param.id, value);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteU64(PackWrite *const this, const uint64_t value, const PckWriteU64Param param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM(UINT, param.id);
        FUNCTION_TEST_PARAM(BOOL, param.defaultWrite);
        FUNCTION_TEST_PARAM(UINT64, param.defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (!pckWriteDefaultNull(this, param.defaultWrite, value == param.defaultValue))
        pckWriteTag(this, pckTypeMapU64, param.id, value);

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN PackWrite *
pckWriteEnd(PackWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->tagStack.depth == 0);

    pckWriteU64Internal(this, 0);
    this->tagStack.top = NULL;

    // If writing to io flush the internal buffer
    if (this->write != NULL)
    {
        if (!bufEmpty(this->buffer))
            ioWrite(this->write, this->buffer);
    }
    // Else resize the external buffer to trim off extra space added during processing
    else
        bufResize(this->buffer, bufUsed(this->buffer));

    FUNCTION_TEST_RETURN(PACK_WRITE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Pack *
pckWriteResult(PackWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
    FUNCTION_TEST_END();

    Pack *result = NULL;

    if (this != NULL)
    {
        ASSERT(this->tagStack.top == NULL);

        result = (Pack *)OBJ_NAME(this->buffer, Pack::Buffer);
    }

    FUNCTION_TEST_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
pckWriteToLog(const PackWrite *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{depth: %u, idLast: %u}", this->tagStack.depth, this->tagStack.top == NULL ? 0 : this->tagStack.top->idLast);
}
