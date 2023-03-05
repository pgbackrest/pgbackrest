/***********************************************************************************************************************************
Binary to String Encode/Decode
***********************************************************************************************************************************/
#ifndef COMMON_ENCODE_H
#define COMMON_ENCODE_H

#include <stddef.h>

/***********************************************************************************************************************************
Encoding types
***********************************************************************************************************************************/
typedef enum
{
    encodingBase64,
    encodingBase64Url,
    encodingHex,
} EncodingType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Encode binary data to a printable string
FN_EXTERN void encodeToStr(EncodingType type, const unsigned char *source, size_t sourceSize, char *destination);

// Size of the string returned by encodeToStr()
FN_EXTERN size_t encodeToStrSize(EncodingType type, size_t sourceSize);

// Decode a string to binary data
FN_EXTERN void decodeToBin(EncodingType type, const char *source, unsigned char *destination);

// Size of the binary data returned by decodeToBin()
FN_EXTERN size_t decodeToBinSize(EncodingType type, const char *source);

#endif
