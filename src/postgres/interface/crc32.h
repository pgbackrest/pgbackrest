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
// Generate CRC-32 checksum (required by <= 9.4)
FN_EXTERN uint32_t crc32One(const unsigned char *data, size_t size);

// Generate CRC-32C checksum (required by >= 9.5)
FN_EXTERN uint32_t crc32cOne(const unsigned char *data, size_t size);

FN_EXTERN uint32_t crc32cInit(void);

FN_EXTERN uint32_t crc32cComp(uint32_t crc, const unsigned char *data, size_t size);

FN_EXTERN uint32_t crc32cFinish(uint32_t crc);

#endif
