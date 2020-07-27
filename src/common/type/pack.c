/***********************************************************************************************************************************
Pack Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
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
struct PackRead
{
    MemContext *memContext;                                         // Mem context
    IoRead *read;                                                   // Read pack from
    Buffer *buffer;                                                 // Buffer to contain read data
    unsigned int idLast;                                            // Last id read

    unsigned int tagNextId;                                         // Next tag id
    PackType tagNextType;                                           // Next tag type
    uint64_t tagNextValue;                                          // Next tag value
};

OBJECT_DEFINE_FREE(PACK_READ);

struct PackWrite
{
    MemContext *memContext;                                         // Mem context
    IoWrite *write;                                                 // Write pack to
    unsigned int idLast;                                            // Last id written
};

OBJECT_DEFINE_FREE(PACK_WRITE);

/**********************************************************************************************************************************/
PackRead *
pckReadNew(IoRead *read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, read);
    FUNCTION_TEST_END();

    ASSERT(read != NULL);

    PackRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PackRead")
    {
        this = memNew(sizeof(PackRead));

        *this = (PackRead)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .read = read,
            .buffer = bufNew(PACK_UINT64_SIZE_MAX),
        };
    }
    MEM_CONTEXT_NEW_END();

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
static uint64_t
pckReadTag(PackRead *this, unsigned int id, PackType type, bool peek)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BOOL, peek);
    FUNCTION_TEST_END();

    if (id <= this->idLast)
        THROW_FMT(FormatError, "field %u was already read", id);

    do
    {
        if (this->tagNextId == 0)
        {
            bufLimitSet(this->buffer, 1);
            ioRead(this->read, this->buffer);

            unsigned int tag = bufPtr(this->buffer)[0];
            bufUsedZero(this->buffer);

            if (tag == 0)
                this->tagNextId = 0xFFFFFFFF;
            else
            {
                this->tagNextType = tag & 0xF;

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

                this->tagNextId += this->idLast + 1;

                LOG_DEBUG_FMT("TAG NEXT ID = %u", this->tagNextId);
            }
        }

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
                    THROW_FMT(FormatError, "field %u is type %u but expected %u", this->tagNextId, this->tagNextType, type);

                this->idLast = this->tagNextId;
                this->tagNextId = 0;
            }

            break;
        }

        this->idLast = this->tagNextId;
        this->tagNextId = 0;

        // Read data for the field being skipped
        // pckReadUInt64Internal(this);
    }
    while (1);

    FUNCTION_TEST_RETURN(this->tagNextValue);
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

    FUNCTION_TEST_RETURN(id < this->tagNextId);
}

/**********************************************************************************************************************************/
int32_t
pckReadInt32(PackRead *this, unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    uint32_t result = (uint32_t)pckReadTag(this, id, pckTypeInt32, false);

    FUNCTION_TEST_RETURN((int32_t)((result >> 1) ^ (~(result & 1) + 1)));
}

/**********************************************************************************************************************************/
int64_t
pckReadInt64(PackRead *this, unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    uint64_t result = pckReadTag(this, id, pckTypeInt64, false);

    FUNCTION_TEST_RETURN((int64_t)((result >> 1) ^ (~(result & 1) + 1)));
}

/**********************************************************************************************************************************/
void *
pckReadPtr(PackRead *this, unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((void *)(uintptr_t)pckReadTag(this, id, pckTypePtr, false));
}

/**********************************************************************************************************************************/
uint64_t
pckReadUInt32(PackRead *this, unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(pckReadTag(this, id, pckTypeUInt32, false));
}

/**********************************************************************************************************************************/
uint64_t
pckReadUInt64(PackRead *this, unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, this);
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(pckReadTag(this, id, pckTypeUInt64, false));
}

/**********************************************************************************************************************************/
String *
pckReadToLog(const PackRead *this)
{
    return strNewFmt("{idLast: %u}", this->idLast);
}

/**********************************************************************************************************************************/
PackWrite *
pckWriteNew(IoWrite *write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, write);
    FUNCTION_TEST_END();

    ASSERT(write != NULL);

    PackWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PackWrite")
    {
        this = memNew(sizeof(PackWrite));

        *this = (PackWrite)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .write = write,
        };
    }
    MEM_CONTEXT_NEW_END();

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
    ASSERT(id > this->idLast);

    unsigned int tagId = id - this->idLast - 1;
    uint64_t tag = type;

    // LOG_DEBUG_FMT("TAG ID: %u", tagId);

    if (value < 2)
    {
        // uint8_t val = (uint8_t)value;
        // LOG_DEBUG_FMT("VALUE < 2: %" PRIu64, value);

        tag |= (value & 0x1) << 6;
        tag |= (tagId & 0x1) << 4;

        if (tagId > 1)
            tag |= 0x20;
    }
    else
    {
        // LOG_DEBUG_FMT("VALUE >= 2: %" PRIu64, value);

        tag |= 0x80;
        tag |= (tagId & 0x3) << 4;

        if (tagId > 3)
            tag |= 0x40;
    }

    uint8_t tagByte = (uint8_t)tag;
    ioWrite(this->write, BUF(&tagByte, 1));

    if (tag & 0x80)
    {
        if (tag & 0x40)
            pckWriteUInt64Internal(this, tagId >> 2);

        pckWriteUInt64Internal(this, value);
    }
    else if (tag & 0x20)
        pckWriteUInt64Internal(this, tagId >> 1);

    this->idLast = id;

    FUNCTION_TEST_RETURN_VOID();
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

    pckWriteUInt64Internal(this, 0);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
pckWriteToLog(const PackWrite *this)
{
    return strNewFmt("{idLast: %u}", this->idLast);
}
