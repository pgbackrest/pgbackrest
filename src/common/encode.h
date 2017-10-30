/***********************************************************************************************************************************
Binary to String Encode/Decode

These high-level functions are preferred to the low-level functions for each encoding type in the encode subdirectory.
***********************************************************************************************************************************/
#ifndef ENCODE_H
#define ENCODE_H

#include "common/type.h"

/***********************************************************************************************************************************
Encoding types
***********************************************************************************************************************************/
typedef enum {encodeBase64} EncodeType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void encodeToStr(EncodeType encodeType, const unsigned char *source, int sourceSize, char *destination);
int encodeToStrSize(EncodeType encodeType, int sourceSize);
void decodeToBin(EncodeType encodeType, const char *source, unsigned char *destination);
int decodeToBinSize(EncodeType encodeType, const char *source);
bool decodeToBinValid(EncodeType encodeType, const char *source);
void decodeToBinValidate(EncodeType encodeType, const char *source);

#endif
