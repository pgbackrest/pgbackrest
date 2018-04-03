/***********************************************************************************************************************************
Binary to String Encode/Decode

These high-level functions are preferred to the low-level functions for each encoding type in the encode subdirectory.
***********************************************************************************************************************************/
#ifndef COMMON_ENCODE_H
#define COMMON_ENCODE_H

#include "common/type.h"

/***********************************************************************************************************************************
Encoding types
***********************************************************************************************************************************/
typedef enum {encodeBase64} EncodeType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void encodeToStr(EncodeType encodeType, const unsigned char *source, size_t sourceSize, char *destination);
size_t encodeToStrSize(EncodeType encodeType, size_t sourceSize);
void decodeToBin(EncodeType encodeType, const char *source, unsigned char *destination);
size_t decodeToBinSize(EncodeType encodeType, const char *source);
bool decodeToBinValid(EncodeType encodeType, const char *source);
void decodeToBinValidate(EncodeType encodeType, const char *source);

#endif
