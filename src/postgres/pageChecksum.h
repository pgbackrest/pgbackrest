#ifndef PAGE_CHECKSUM_H
#define PAGE_CHECKSUM_H

#include "common/type.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
uint16 pageChecksum(const char *szPage, uint32 uiBlockNo, uint32 uiPageSize);
bool pageChecksumTest(const char *szPage, uint32 uiBlockNo, uint32 uiPageSize, uint32 uiIgnoreWalId, uint32 uiIgnoreWalOffset);
bool pageChecksumBufferTest(
    const char *szPageBuffer, uint32 uiBufferSize, uint32 uiBlockNoStart, uint32 uiPageSize, uint32 uiIgnoreWalId,
    uint32 uiIgnoreWalOffset);

#endif
