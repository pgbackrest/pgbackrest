/***********************************************************************************************************************************
CRC-32 Calculation

CRC-32 and CRC-32C calculations required to validate the integrity of pg_control.
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_CRC32_H
#define POSTGRES_INTERFACE_CRC32_H

#include <inttypes.h>
#include <stddef.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN uint32_t crc32(const unsigned char *data, size_t size);
FN_EXTERN uint32_t crc32c(const unsigned char *data, size_t size);

#endif
