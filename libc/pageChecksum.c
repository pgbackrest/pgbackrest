/***********************************************************************************************************************************
pageChecksum.c

Checksum implementation for data pages.

Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
Portions Copyright (c) 1994, Regents of the University of California

Copied from src/include/storage/checksum_impl.h in the PostgreSQL project.

The algorithm used to checksum pages is chosen for very fast calculation. Workloads where the database working set fits into OS file
cache but not into shared buffers can read in pages at a very fast pace and the checksum algorithm itself can become the largest
bottleneck.

The checksum algorithm itself is based on the FNV-1a hash (FNV is shorthand for Fowler/Noll/Vo).  The primitive of a plain FNV-1a
hash folds in data 1 byte at a time according to the formula:

    hash = (hash ^ value) * FNV_PRIME

FNV-1a algorithm is described at http://www.isthe.com/chongo/tech/comp/fnv/

PostgreSQL doesn't use FNV-1a hash directly because it has bad mixing of high bits - high order bits in input data only affect high
order bits in output data. To resolve this we xor in the value prior to multiplication shifted right by 17 bits. The number 17 was
chosen because it doesn't have common denominator with set bit positions in FNV_PRIME and empirically provides the fastest mixing
for high order bits of final iterations quickly avalanche into lower positions. For performance reasons we choose to combine 4 bytes
at a time. The actual hash formula used as the basis is:

    hash = (hash ^ value) * FNV_PRIME ^ ((hash ^ value) >> 17)

The main bottleneck in this calculation is the multiplication latency. To hide the latency and to make use of SIMD parallelism
multiple hash values are calculated in parallel. The page is treated as a 32 column two dimensional array of 32 bit values. Each
column is aggregated separately into a partial checksum. Each partial checksum uses a different initial value (offset basis in FNV
terminology). The initial values actually used were chosen randomly, as the values themselves don't matter as much as that they are
different and don't match anything in real data. After initializing partial checksums each value in the column is aggregated
according to the above formula. Finally two more iterations of the formula are performed with value 0 to mix the bits of the last
value added.

The partial checksums are then folded together using xor to form a single 32-bit checksum. The caller can safely reduce the value to
16 bits using modulo 2^16-1. That will cause a very slight bias towards lower values but this is not significant for the performance
of the checksum.

The algorithm choice was based on what instructions are available in SIMD instruction sets. This meant that a fast and good
algorithm needed to use multiplication as the main mixing operator. The simplest multiplication based checksum primitive is the one
used by FNV. The prime used is chosen for good dispersion of values. It has no known simple patterns that result in collisions. Test
of 5-bit differentials of the primitive over 64bit keys reveals no differentials with 3 or more values out of 100000 random keys
colliding. Avalanche test shows that only high order bits of the last word have a bias. Tests of 1-4 uncorrelated bit errors, stray
0 and 0xFF bytes, overwriting page from random position to end with 0 bytes, and overwriting random segments of page with 0x00, 0xFF
and random data all show optimal 2e-16 false positive rate within margin of error.

Vectorization of the algorithm requires 32bit x 32bit -> 32bit integer multiplication instruction. As of 2013 the corresponding
instruction is available on x86 SSE4.1 extensions (pmulld) and ARM NEON (vmul.i32). Vectorization requires a compiler to do the
vectorization for us. For recent GCC versions the flags -msse4.1 -funroll-loops -ftree-vectorize are enough to achieve
vectorization.

The optimal amount of parallelism to use depends on CPU specific instruction latency, SIMD instruction width, throughput and the
amount of registers available to hold intermediate state. Generally, more parallelism is better up to the point that state doesn't
fit in registers and extra load-store instructions are needed to swap values in/out. The number chosen is a fixed part of the
algorithm because changing the parallelism changes the checksum result.

The parallelism number 32 was chosen based on the fact that it is the largest state that fits into architecturally visible x86 SSE
registers while leaving some free registers for intermediate values. For future processors with 256bit vector registers this will
leave some performance on the table. When vectorization is not available it might be beneficial to restructure the computation to
calculate a subset of the columns at a time and perform multiple passes to avoid register spilling. This optimization opportunity
is not used. Current coding also assumes that the compiler has the ability to unroll the inner loop to avoid loop overhead and
minimize register spilling. For less sophisticated compilers it might be beneficial to manually unroll the inner loop.
***********************************************************************************************************************************/
#include "LibC.h"

/***********************************************************************************************************************************
For historical reasons, the 64-bit LSN value is stored as two 32-bit values.
***********************************************************************************************************************************/
typedef struct
{
    uint32 xlogid;      /* high bits */
    uint32 xrecoff;     /* low bits */
} PageXLogRecPtr;

/***********************************************************************************************************************************
Space management information generic to any page.  Only values required for pgBackRest are represented here.

    pd_lsn - identifies xlog record for last change to this page.
    pd_checksum - page checksum, if set.

The LSN is used by the buffer manager to enforce the basic rule of WAL: "thou shalt write xlog before data".  A dirty buffer cannot
be dumped to disk until xlog has been flushed at least as far as the page's LSN.

pd_checksum stores the page checksum, if it has been set for this page; zero is a valid value for a checksum. If a checksum is not
in use then we leave the field unset. This will typically mean the field is zero though non-zero values may also be present if
databases have been pg_upgraded from releases prior to 9.3, when the same byte offset was used to store the current timelineid when
the page was last updated. Note that there is no indication on a page as to whether the checksum is valid or not, a deliberate
design choice which avoids the problem of relying on the page contents to decide whether to verify it. Hence there are no flag bits
relating to checksums.
***********************************************************************************************************************************/
typedef struct PageHeaderData
{
    // LSN is member of *any* block, not only page-organized ones
    PageXLogRecPtr pd_lsn;  /* LSN: next byte after last byte of xlog * record for last change to this page */
    uint16 pd_checksum;     /* checksum */
    uint16 pd_flags;        /* flag bits, see below */
    uint16 pd_lower;        /* offset to start of free space */
    uint16 pd_upper;        /* offset to end of free space */
} PageHeaderData;

typedef PageHeaderData *PageHeader;

/***********************************************************************************************************************************
pageChecksumBlock

Block checksum algorithm.  The data argument must be aligned on a 4-byte boundary.
***********************************************************************************************************************************/
// number of checksums to calculate in parallel
#define N_SUMS 32

// prime multiplier of FNV-1a hash
#define FNV_PRIME 16777619

// Base offsets to initialize each of the parallel FNV hashes into a different initial state.
static const uint32 uiyChecksumBaseOffsets[N_SUMS] =
{
    0x5B1F36E9, 0xB8525960, 0x02AB50AA, 0x1DE66D2A, 0x79FF467A, 0x9BB9F8A3, 0x217E7CD2, 0x83E13D2C,
    0xF8D4474F, 0xE39EB970, 0x42C6AE16, 0x993216FA, 0x7B093B5D, 0x98DAFF3C, 0xF718902A, 0x0B1C9CDB,
    0xE58F764B, 0x187636BC, 0x5D7B3BB1, 0xE73DE7DE, 0x92BEC979, 0xCCA6C0B2, 0x304A0979, 0x85AA43D4,
    0x783125BB, 0x6CA8EAA2, 0xE407EAC6, 0x4B5CFC3E, 0x9FBF8C76, 0x15CA20BE, 0xF2CA9FD3, 0x959BD756
};

// Calculate one round of the checksum.
#define CHECKSUM_COMP(uiChecksum, uiValue) \
do { \
    uint32 uiTemp = (uiChecksum) ^ (uiValue); \
    (uiChecksum) = uiTemp * FNV_PRIME ^ (uiTemp >> 17); \
} while (0)

static uint32
pageChecksumBlock(const char *szData, uint32 uiSize)
{
    uint32 uiySums[N_SUMS];
    uint32 (*puiyDataArray)[N_SUMS] = (uint32 (*)[N_SUMS])szData;
    uint32 uiResult = 0;
    uint32 i, j;

    /* initialize partial checksums to their corresponding offsets */
    memcpy(uiySums, uiyChecksumBaseOffsets, sizeof(uiyChecksumBaseOffsets));

    /* main checksum calculation */
    for (i = 0; i < uiSize / sizeof(uint32) / N_SUMS; i++)
        for (j = 0; j < N_SUMS; j++)
            CHECKSUM_COMP(uiySums[j], puiyDataArray[i][j]);

    /* finally add in two rounds of zeroes for additional mixing */
    for (i = 0; i < 2; i++)
        for (j = 0; j < N_SUMS; j++)
            CHECKSUM_COMP(uiySums[j], 0);

    // xor fold partial checksums together
    for (i = 0; i < N_SUMS; i++)
        uiResult ^= uiySums[i];

    return uiResult;
}

/***********************************************************************************************************************************
pageChecksum

Compute the checksum for a Postgres page.  The page must be aligned on a 4-byte boundary.

The checksum includes the block number (to detect the case where a page is somehow moved to a different location), the page header
(excluding the checksum itself), and the page data.
***********************************************************************************************************************************/
uint16
pageChecksum(const char *szPage, uint32 uiBlockNo, uint32 uiPageSize)
{
    // Save pd_checksum and temporarily set it to zero, so that the checksum calculation isn't affected by the old checksum stored
    // on the page. Restore it after, because actually updating the checksum is NOT part of the API of this function.
    PageHeader pxPageHeader = (PageHeader)szPage;

    uint usOriginalChecksum = pxPageHeader->pd_checksum;
    pxPageHeader->pd_checksum = 0;
    uint uiChecksum = pageChecksumBlock(szPage, uiPageSize);
    pxPageHeader->pd_checksum = usOriginalChecksum;

    // Mix in the block number to detect transposed pages
    uiChecksum ^= uiBlockNo;

    // Reduce to a uint16 with an offset of one. That avoids checksums of zero, which seems like a good idea.
    return (uiChecksum % 65535) + 1;
}

/***********************************************************************************************************************************
pageChecksumTest

Test if checksum is valid for a single page.
***********************************************************************************************************************************/
bool
pageChecksumTest(const char *szPage, uint32 uiBlockNo, uint32 uiPageSize, uint32 uiIgnoreWalId, uint32 uiIgnoreWalOffset)
{
    return
        // This is a new page so don't test checksum
        ((PageHeader)szPage)->pd_upper == 0 ||
        // LSN is after the backup started so checksum is not tested because pages may be torn
        (((PageHeader)szPage)->pd_lsn.xlogid >= uiIgnoreWalId && ((PageHeader)szPage)->pd_lsn.xrecoff >= uiIgnoreWalOffset) ||
        // Checksum is valid
        ((PageHeader)szPage)->pd_checksum == pageChecksum(szPage, uiBlockNo, uiPageSize);
}

/***********************************************************************************************************************************
pageChecksumBufferTest

Test if checksums are valid for all pages in a buffer.
***********************************************************************************************************************************/
bool
pageChecksumBufferTest(
    const char *szPageBuffer, uint32 uiBufferSize, uint32 uiBlockNoStart, uint32 uiPageSize, uint32 uiIgnoreWalId,
    uint32 uiIgnoreWalOffset)
{
    // If the buffer does not represent an even number of pages then error
    if (uiBufferSize % uiPageSize != 0 || uiBufferSize / uiPageSize == 0)
    {
        croak("buffer size %lu, page size %lu are not divisible", uiBufferSize, uiPageSize);
    }

    // Loop through all pages in the buffer
    for (uint32 uiIndex = 0; uiIndex < uiBufferSize / uiPageSize; uiIndex++)
    {
        const char *szPage = szPageBuffer + (uiIndex * uiPageSize);

        // Return false if the checksums do not match
        if (!pageChecksumTest(szPage, uiBlockNoStart + uiIndex, uiPageSize, uiIgnoreWalId, uiIgnoreWalOffset))
            return false;
    }

    // All checksums match
    return true;
}
