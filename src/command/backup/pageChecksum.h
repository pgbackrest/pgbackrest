/***********************************************************************************************************************************
Page Checksum Filter

Check all pages in a PostgreSQL relation to ensure the checksums are valid.
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_PAGE_CHECKSUM_H
#define COMMAND_BACKUP_PAGE_CHECKSUM_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define PAGE_CHECKSUM_FILTER_TYPE                                   STRID5("pg-chksum", 0xdacd681ecf00)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoFilter *pageChecksumNew(unsigned int segmentNo, unsigned int segmentPageTotal, uint64_t lsnLimit);
IoFilter *pageChecksumNewPack(const Buffer *paramList);

#endif
