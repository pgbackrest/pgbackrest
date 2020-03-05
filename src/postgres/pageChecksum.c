/***********************************************************************************************************************************
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"
#include "postgres/interface.h"
#include "postgres/interface/static.auto.h"

/***********************************************************************************************************************************
Return the lsn for a page
***********************************************************************************************************************************/
uint64_t
pgPageLsn(const unsigned char *page)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(UCHARDATA, page);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN((uint64_t)((PageHeader)page)->pd_lsn.xlogid << 32 | ((PageHeader)page)->pd_lsn.xrecoff);
}

/***********************************************************************************************************************************
pageChecksumTest - test if checksum is valid for a single page
***********************************************************************************************************************************/
bool
pgPageChecksumTest(
    unsigned char *page, unsigned int blockNo, unsigned int pageSize, uint32_t ignoreWalId, uint32_t ignoreWalOffset)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(UCHARDATA, page);
        FUNCTION_TEST_PARAM(UINT, blockNo);
        FUNCTION_TEST_PARAM(UINT, pageSize);
        FUNCTION_TEST_PARAM(UINT32, ignoreWalId);
        FUNCTION_TEST_PARAM(UINT32, ignoreWalOffset);
    FUNCTION_TEST_END();

    ASSERT(page != NULL);

    FUNCTION_TEST_RETURN(
        // This is a new page so don't test checksum
        ((PageHeader)page)->pd_upper == 0 ||
        // LSN is after the backup started so checksum is not tested because pages may be torn
        (((PageHeader)page)->pd_lsn.xlogid >= ignoreWalId && ((PageHeader)page)->pd_lsn.xrecoff >= ignoreWalOffset) ||
        // Checksum is valid if a full page
        (pageSize == PG_PAGE_SIZE_DEFAULT && ((PageHeader)page)->pd_checksum == pgPageChecksum(page, blockNo)));
}
