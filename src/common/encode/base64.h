/***********************************************************************************************************************************
Base64 Binary to String Encode/Decode

The high-level functions in encode.c should be used in preference to these low-level functions.
***********************************************************************************************************************************/
#ifndef BASE64_H
#define BASE64_H

#include "common/type.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void encodeToStrBase64(const unsigned char *source, size_t sourceSize, char *destination);
size_t encodeToStrSizeBase64(size_t sourceSize);
void decodeToBinBase64(const char *source, unsigned char *destination);
size_t decodeToBinSizeBase64(const char *source);
void decodeToBinValidateBase64(const char *source);

#endif
