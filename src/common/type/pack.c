/***********************************************************************************************************************************
Pack Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/write.h"
#include "common/type/pack.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PACK_UINT64_SIZE_MAX                                        10

/***********************************************************************************************************************************
Object type

3 bits for type (0 byte is terminator)
1 bit for default
4 bits for id (will need to add an extra byte at some point)

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
struct PackWrite
{
    MemContext *memContext;                                         // Mem context
    IoWrite *write;                                                 // Write pack to
    unsigned int idLast;                                            // Last id written
};

OBJECT_DEFINE_FREE(PACK_WRITE);

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
Pack/unpack an unsigned 32-bit integer to/from base-128 varint encoding
***********************************************************************************************************************************/
static size_t
pckFromUInt32(uint32_t value, uint8_t *out)
{
    unsigned int result = 0;

    if (value >= 0x80)
    {
        out[result++] = (uint8_t)(value | 0x80);
        value >>= 7;

        if (value >= 0x80)
        {
            out[result++] = (uint8_t)(value | 0x80);
            value >>= 7;

            if (value >= 0x80)
            {
                out[result++] = (uint8_t)(value | 0x80);
                value >>= 7;

                if (value >= 0x80)
                {
                    out[result++] = (uint8_t)(value | 0x80);
                    value >>= 7;
                }
            }
        }
    }

    out[result++] = (uint8_t)value;

    return result;
}

/***********************************************************************************************************************************
Pack/unpack an unsigned 64-bit integer to/from base-128 varint encoding
***********************************************************************************************************************************/
static size_t
pckFromUInt64(uint64_t value, uint8_t *out)
{
    uint32_t hi = (uint32_t) (value >> 32);
    uint32_t lo = (uint32_t) value;

    if (hi == 0)
        return pckFromUInt32((uint32_t) lo, out);

    out[0] = (uint8_t)(lo | 0x80);
    out[1] = (uint8_t)((lo >> 7) | 0x80);
    out[2] = (uint8_t)((lo >> 14) | 0x80);
    out[3] = (uint8_t)((lo >> 21) | 0x80);

    if (hi < 8)
    {
        out[4] = (uint8_t)((hi << 4) | (lo >> 28));
        return 5;
    }
    else
    {
        out[4] = (uint8_t)(((hi & 7) << 4) | (lo >> 28) | 0x80);
        hi >>= 3;
    }

    unsigned int result = 5;

    while (hi >= 128)
    {
        out[result++] = (uint8_t)(hi | 0x80);
        hi >>= 7;
    }

    out[result++] = (uint8_t)hi;

    return result;
}

static uint64_t
pckToUInt64(const uint8_t *buffer)
{
    uint64_t result = buffer[0] & 0x7f;

    for (unsigned int bufferIdx = 1; bufferIdx < PACK_UINT64_SIZE_MAX; bufferIdx++)
    {
        if (buffer[bufferIdx - 1] < 0x80)
            break;

        result |= (uint64_t)(buffer[bufferIdx] & 0x7f) << (7 * bufferIdx);
    }

    return result;
}

/**********************************************************************************************************************************/
static void
pckWriteUInt64Internal(PackWrite *this, uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    unsigned char buffer[PACK_UINT64_SIZE_MAX];
    size_t size = pckFromUInt64(value, buffer);
    ioWrite(this->write, BUF(buffer, size));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
// static void
// pckWriteTag(PackWrite *this, PackType type, unsigned int id)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(PACK_WRITE, this);
//         FUNCTION_TEST_PARAM(ENUM, type);
//         FUNCTION_TEST_PARAM(UINT, id);
//     FUNCTION_TEST_END();
//
//     ASSERT(this != NULL);
//     ASSERT(id > this->idLast);
//
//     pckWriteUInt64Internal(this, (id - this->idLast) << 4 | type);
//
//     this->idLast = id;
//
//     FUNCTION_TEST_RETURN_VOID();
// }

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

    unsigned char buffer[PACK_UINT64_SIZE_MAX];
    size_t size = pckFromUInt64(value, buffer);

    pckWriteUInt64Internal(this, (uint64_t)(id - this->idLast - 1) << 5 | (size - 1) << 1 | pckTypeUInt64);
    ioWrite(this->write, BUF(buffer, size));

    this->idLast = id;

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
