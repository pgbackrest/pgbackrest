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
} EncodingType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Encode binary data to a printable string
void encodeToStr(EncodingType type, const unsigned char *source, size_t sourceSize, char *destination);

// Size of the string returned by encodeToStr()
size_t encodeToStrSize(EncodingType type, size_t sourceSize);

// Decode a string to binary data
void decodeToBin(EncodingType type, const char *source, unsigned char *destination);

// Size of the binary data returned by decodeToBin()
size_t decodeToBinSize(EncodingType type, const char *source);

#endif
