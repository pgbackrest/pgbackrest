/***********************************************************************************************************************************
Cipher
***********************************************************************************************************************************/
#include <openssl/rand.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"
#include "crypto/random.h"

/***********************************************************************************************************************************
Generate random bytes
***********************************************************************************************************************************/
void
randomBytes(unsigned char *buffer, size_t size)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(UCHARP, buffer);
        FUNCTION_DEBUG_PARAM(SIZE, size);

        FUNCTION_DEBUG_ASSERT(buffer != NULL);
        FUNCTION_DEBUG_ASSERT(size > 0);
    FUNCTION_DEBUG_END();

    RAND_bytes(buffer, (int)size);

    FUNCTION_DEBUG_RESULT_VOID();
}
