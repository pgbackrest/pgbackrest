/***********************************************************************************************************************************
Crypto Common
***********************************************************************************************************************************/
#ifndef CRYPTO_CRYPTO_H
#define CRYPTO_CRYPTO_H

#include <stdbool.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cryptoInit(void);
bool cryptoIsInit(void);

void cryptoError(bool error, const char *description);
void cryptoErrorCode(unsigned long code, const char *description);

void cryptoRandomBytes(unsigned char *buffer, size_t size);

#endif
