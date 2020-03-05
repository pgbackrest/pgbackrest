/***********************************************************************************************************************************
Checksum Implementation for Data Pages
***********************************************************************************************************************************/
#ifndef POSTGRES_PAGECHECKSUM_H
#define POSTGRES_PAGECHECKSUM_H

#include <stdint.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
uint16_t pgPageChecksum(const unsigned char *page, unsigned int blockNo);
uint64_t pgPageLsn(const unsigned char *page);
bool pgPageChecksumTest(
    const unsigned char *page, unsigned int blockNo, unsigned int pageSize, uint32_t ignoreWalId, uint32_t ignoreWalOffset);

#endif
