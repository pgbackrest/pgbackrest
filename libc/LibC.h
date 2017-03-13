#include "EXTERN.h"
#include "perl.h"

/***********************************************************************************************************************************
Older compilers do not define true/false
***********************************************************************************************************************************/
#ifndef false
    #define false 0
#endif

#ifndef true
    #define true 1
#endif

/***********************************************************************************************************************************
Define integer types based on Perl portability
***********************************************************************************************************************************/
typedef U8 uint8;       /* ==  8 bits */
typedef U16 uint16;     /* == 16 bits */
typedef U32 uint32;     /* == 32 bits */
typedef UV uint64;      /* == 64 bits */

typedef I8 int8;       /* ==  8 bits */
typedef I16 int16;     /* == 16 bits */
typedef I32 int32;     /* == 32 bits */
typedef IV int64;      /* == 64 bits */

/***********************************************************************************************************************************
Checksum functions
***********************************************************************************************************************************/
uint16 pageChecksum(const char *szPage, uint32 uiBlockNo, uint32 uiPageSize);
bool pageChecksumTest(const char *szPage, uint32 uiBlockNo, uint32 uiPageSize, uint32 uiIgnoreWalId, uint32 uiIgnoreWalOffset);
bool pageChecksumBufferTest(
    const char *szPageBuffer, uint32 uiBufferSize, uint32 uiBlockNoStart, uint32 uiPageSize, uint32 uiIgnoreWalId,
    uint32 uiIgnoreWalOffset);
