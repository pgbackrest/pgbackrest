/***********************************************************************************************************************************
PostgreSQL Page Checksum

Adapted from PostgreSQL src/include/storage/checksum_impl.h.
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "postgres/interface/static.vendor.h"

/***********************************************************************************************************************************
Page checksum calculation
***********************************************************************************************************************************/
// Number of checksums to calculate in parallel
#define PARALLEL_SUM                                                32

// Prime multiplier of FNV-1a hash
#define FNV_PRIME                                                   16777619

// Calculate one round of the checksum
#define CHECKSUM_ROUND(checksum, value)                                                                                            \
    do                                                                                                                             \
    {                                                                                                                              \
        const uint32_t tmp = (checksum) ^ (value);                                                                                 \
        checksum = tmp * FNV_PRIME ^ (tmp >> 17);                                                                                  \
    } while (0)

// Main calculation loop
#define CHECKSUM_CASE(pageSize)                                                                                                    \
    case pageSize:                                                                                                                 \
        for (uint32_t i = 0; i < (uint32) (pageSize / (sizeof(uint32) * PARALLEL_SUM)); i++)                                       \
            for (uint32_t j = 0; j < PARALLEL_SUM; j++)                                                                            \
                CHECKSUM_ROUND(sums[j], ((PgPageChecksum##pageSize *)page)->data[i][j]);                                           \
                                                                                                                                   \
        break;

/***********************************************************************************************************************************
Define unions that will make the code valid under strict aliasing for each page size
***********************************************************************************************************************************/
#define CHECKSUM_UNION(pageSize)                                                                                                   \
    typedef union                                                                                                                  \
    {                                                                                                                              \
        PageHeaderData phdr;                                                                                                       \
        uint32_t data[pageSize / (sizeof(uint32_t) * PARALLEL_SUM)][PARALLEL_SUM];                                                 \
    } PgPageChecksum##pageSize;

CHECKSUM_UNION(pgPageSize1);
CHECKSUM_UNION(pgPageSize2);
CHECKSUM_UNION(pgPageSize4);
CHECKSUM_UNION(pgPageSize8);
CHECKSUM_UNION(pgPageSize16);
CHECKSUM_UNION(pgPageSize32);

/**********************************************************************************************************************************/
FN_EXTERN uint16_t
pgPageChecksum(unsigned char *const page, const uint32_t blockNo, const PgPageSize pageSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(UCHARDATA, page);
        FUNCTION_TEST_PARAM(UINT, blockNo);
        FUNCTION_TEST_PARAM(ENUM, pageSize);
    FUNCTION_TEST_END();

    // Save pd_checksum and temporarily set it to zero, so that the checksum calculation isn't affected by the old checksum stored
    // on the page. Restore it after, because actually updating the checksum is NOT part of the API of this function.
    const uint16_t checksumPrior = ((PageHeaderData *)page)->pd_checksum;
    ((PageHeaderData *)page)->pd_checksum = 0;

    // Initialize partial checksums to their corresponding offsets
    uint32_t sums[PARALLEL_SUM] =
    {
        0x5b1f36e9, 0xb8525960, 0x02ab50aa, 0x1de66d2a, 0x79ff467a, 0x9bb9f8a3, 0x217e7cd2, 0x83e13d2c,
        0xf8d4474f, 0xe39eb970, 0x42c6ae16, 0x993216fa, 0x7b093b5d, 0x98daff3c, 0xf718902a, 0x0b1c9cdb,
        0xe58f764b, 0x187636bc, 0x5d7b3bb1, 0xe73de7de, 0x92bec979, 0xcca6c0b2, 0x304a0979, 0x85aa43d4,
        0x783125bb, 0x6ca8eaa2, 0xe407eac6, 0x4b5cfc3e, 0x9fbf8c76, 0x15ca20be, 0xf2ca9fd3, 0x959bd756,
    };

    // Main checksum calculation
    switch (pageSize)
    {
        CHECKSUM_CASE(pgPageSize8);                                 // Default page size should be checked first
        CHECKSUM_CASE(pgPageSize1);
        CHECKSUM_CASE(pgPageSize2);
        CHECKSUM_CASE(pgPageSize4);
        CHECKSUM_CASE(pgPageSize16);
        CHECKSUM_CASE(pgPageSize32);

        default:
            pgPageSizeCheck(pageSize);
    }

    // Add in two rounds of zeroes for additional mixing
    for (uint32_t i = 0; i < 2; i++)
        for (uint32_t j = 0; j < PARALLEL_SUM; j++)
            CHECKSUM_ROUND(sums[j], 0);

    // Xor fold partial checksums together
    uint32 result = 0;

    for (uint32_t i = 0; i < PARALLEL_SUM; i++)
        result ^= sums[i];

    // Restore prior checksum
    ((PageHeaderData *)page)->pd_checksum = checksumPrior;

    // Mix in the block number to detect transposed pages
    result ^= blockNo;

    // Reduce to a uint16 (to fit in the pd_checksum field) with an offset of one. That avoids checksums of zero, which seems like a
    // good idea.
    FUNCTION_TEST_RETURN(UINT16, (uint16_t)((result % 65535) + 1));
}

/**********************************************************************************************************************************/
FN_EXTERN bool
pgPageSizeValid(const PgPageSize pageSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, pageSize);
    FUNCTION_TEST_END();

    switch (pageSize)
    {
        case pgPageSize1:
        case pgPageSize2:
        case pgPageSize4:
        case pgPageSize8:
        case pgPageSize16:
        case pgPageSize32:
            FUNCTION_TEST_RETURN(BOOL, true);
    }

    FUNCTION_TEST_RETURN(BOOL, false);
}

/**********************************************************************************************************************************/
FN_EXTERN void
pgPageSizeCheck(const PgPageSize pageSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, pageSize);
    FUNCTION_TEST_END();

    if (!pgPageSizeValid(pageSize))
    {
        THROW_FMT(
            FormatError, "page size is %u but only %i, %i, %i, %i, %i, and %i are supported", pageSize, pgPageSize1, pgPageSize2,
            pgPageSize4, pgPageSize8, pgPageSize16, pgPageSize32);
    }

    FUNCTION_TEST_RETURN_VOID();
}
