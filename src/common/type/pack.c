/***********************************************************************************************************************************
Pack Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/type/pack.h"
#include "common/type/object.h"

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

24 + 1 + 3 + 12 + 2 + 5 + 5 + 5

pgdata/base/1000000000/1000000000.123456

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
struct Pack
{
    unsigned char *buffer;                                          // Packed buffer
    size_t size;                                                    // Size of data in buffer
    size_t extra;                                                   // Allocated size of buffer
    MemContext *memContext;                                         // Mem context
};

OBJECT_DEFINE_MOVE(PACK);
OBJECT_DEFINE_FREE(PACK);

/**********************************************************************************************************************************/
Pack *
pckNew(void)
{
    FUNCTION_TEST_VOID();

    Pack *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Pack")
    {
        this = memNew(sizeof(Pack));

        *this = (Pack)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .buffer = memNew(PACK_EXTRA_MIN),
            .extra = PACK_EXTRA_MIN,
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

static uint32_t
pckToUInt32(const uint8_t *buffer)
{
    uint32_t result = buffer[0] & 0x7f;

    for (unsigned int bufferIdx = 1; bufferIdx <= 5; bufferIdx++)
    {
        if (buffer[bufferIdx - 1] < 0x80)
            break;

        result |= (uint32_t)(buffer[bufferIdx] & 0x7f) << (7 * bufferIdx);
    }

    return result;
    //
    // uint32_t result = data[0] & 0x7f;
    //
    // if (data[0] & 0x80)
    // {
    //     result |= ((uint32_t) (data[1] & 0x7f) << 7);
    //
    //     if (data[1] & 0x80)
    //     {
    //         result |= ((uint32_t) (data[2] & 0x7f) << 14);
    //
    //         if (data[2] & 0x80)
    //         {
    //             result |= ((uint32_t) (data[3] & 0x7f) << 21);
    //
    //             if (data[3] & 0x80)
    //                 result |= ((uint32_t) (data[4]) << 28);
    //         }
    //     }
    // }

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

    for (unsigned int bufferIdx = 1; bufferIdx <= 9; bufferIdx++)
    {
        if (buffer[bufferIdx - 1] < 0x80)
            break;

        result |= (uint64_t)(buffer[bufferIdx] & 0x7f) << (7 * bufferIdx);
    }

    return result;
}

/**********************************************************************************************************************************/
String *
pckToLog(const Pack *this)
{
    return strNewFmt("{size: %zu, extra: %zu}", this->size, this->extra);
}
