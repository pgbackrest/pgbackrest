/***********************************************************************************************************************************
PostgreSQL Page Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "postgres/interface/static.vendor.h"

/***********************************************************************************************************************************
Include the page checksum code
***********************************************************************************************************************************/
#include "postgres/interface/pageChecksum.vendor.c.inc"

/**********************************************************************************************************************************/
FN_EXTERN uint16_t
pgPageChecksum(unsigned char *page, uint32_t blockNo)
{
	PGChecksummablePage *cpage = (PGChecksummablePage *) page;
	uint16		save_checksum;
	uint32		checksum;

	/*
	 * Save pd_checksum and temporarily set it to zero, so that the checksum
	 * calculation isn't affected by the old checksum stored on the page.
	 * Restore it after, because actually updating the checksum is NOT part of
	 * the API of this function.
	 */
	save_checksum = cpage->phdr.pd_checksum;
	cpage->phdr.pd_checksum = 0;
	checksum = pg_checksum_block(cpage);
	cpage->phdr.pd_checksum = save_checksum;

	/* Mix in the block number to detect transposed pages */
	checksum ^= blockNo;

	/*
	 * Reduce to a uint16 (to fit in the pd_checksum field) with an offset of
	 * one. That avoids checksums of zero, which seems like a good idea.
	 */
	return (uint16) ((checksum % 65535) + 1);
}
