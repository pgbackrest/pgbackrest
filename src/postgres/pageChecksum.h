/***********************************************************************************************************************************
Checksum Implementation for Data Pages
***********************************************************************************************************************************/
#ifndef POSTGRES_PAGECHECKSUM_H
#define POSTGRES_PAGECHECKSUM_H

#include "common/type.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
uint16 pageChecksum(const unsigned char *page, unsigned int blockNo, unsigned int pageSize);
bool pageChecksumTest(
    const unsigned char *page, unsigned int blockNo, unsigned int pageSize, uint32 ignoreWalId, uint32 ignoreWalOffset);
bool pageChecksumBufferTest(
    const unsigned char *pageBuffer, unsigned int pageBufferSize, unsigned int blockNoBegin, unsigned int pageSize,
    uint32 ignoreWalId, uint32 ignoreWalOffset);

#endif
