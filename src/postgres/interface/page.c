/***********************************************************************************************************************************
PostgreSQL Page Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "postgres/interface/static.vendor.h"

/* number of checksums to calculate in parallel */
#define N_SUMS 32
/* prime multiplier of FNV-1a hash */
#define FNV_PRIME 16777619

/*
 * Base offsets to initialize each of the parallel FNV hashes into a
 * different initial state.
 */
static const uint32 checksumBaseOffsets[N_SUMS] = {
    0x5B1F36E9, 0xB8525960, 0x02AB50AA, 0x1DE66D2A,
    0x79FF467A, 0x9BB9F8A3, 0x217E7CD2, 0x83E13D2C,
    0xF8D4474F, 0xE39EB970, 0x42C6AE16, 0x993216FA,
    0x7B093B5D, 0x98DAFF3C, 0xF718902A, 0x0B1C9CDB,
    0xE58F764B, 0x187636BC, 0x5D7B3BB1, 0xE73DE7DE,
    0x92BEC979, 0xCCA6C0B2, 0x304A0979, 0x85AA43D4,
    0x783125BB, 0x6CA8EAA2, 0xE407EAC6, 0x4B5CFC3E,
    0x9FBF8C76, 0x15CA20BE, 0xF2CA9FD3, 0x959BD756
};

/*
 * Calculate one round of the checksum.
 */
#define CHECKSUM_COMP(checksum, value) \
do { \
    uint32 __tmp = (checksum) ^ (value); \
    (checksum) = __tmp * FNV_PRIME ^ (__tmp >> 17); \
} while (0)

/***********************************************************************************************************************************
Include the page checksum code
***********************************************************************************************************************************/
#include "postgres/interface/pageChecksum.vendor.c.inc"

ADD_PAGE_SIZE(PG_PAGE_SIZE_1);
ADD_PAGE_SIZE(PG_PAGE_SIZE_2);
ADD_PAGE_SIZE(PG_PAGE_SIZE_4);
ADD_PAGE_SIZE(PG_PAGE_SIZE_8);
ADD_PAGE_SIZE(PG_PAGE_SIZE_16);
ADD_PAGE_SIZE(PG_PAGE_SIZE_32);

/**********************************************************************************************************************************/
FN_EXTERN uint16_t
pgPageChecksum(const unsigned char *page, uint32_t blockNo, unsigned int pageSize)
{
    switch (pageSize)
    {
        #define ADD_CASE(SZ) case SZ: return pg_checksum_page ## SZ((const char *)page, blockNo);

        ADD_CASE(PG_PAGE_SIZE_8);     // The default page size should be checked first
        ADD_CASE(PG_PAGE_SIZE_1);
        ADD_CASE(PG_PAGE_SIZE_2);
        ADD_CASE(PG_PAGE_SIZE_4);
        ADD_CASE(PG_PAGE_SIZE_16);
        ADD_CASE(PG_PAGE_SIZE_32);

        #undef ADD_CASE
    }

    THROW(AssertError, "Invalid page size");
    return 0;
}
