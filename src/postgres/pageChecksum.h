/***********************************************************************************************************************************
Checksum Implementation for Data Pages
***********************************************************************************************************************************/
#ifndef POSTGRES_PAGECHECKSUM_H
#define POSTGRES_PAGECHECKSUM_H

#include <stdint.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
uint16_t pageChecksum(const unsigned char *page, unsigned int blockNo, unsigned int pageSize);
bool pageChecksumTest(
    const unsigned char *page, unsigned int blockNo, unsigned int pageSize, uint32_t ignoreWalId, uint32_t ignoreWalOffset);
bool pageChecksumBufferTest(
    const unsigned char *pageBuffer, unsigned int pageBufferSize, unsigned int blockNoBegin, unsigned int pageSize,
    uint32_t ignoreWalId, uint32_t ignoreWalOffset);

#endif
