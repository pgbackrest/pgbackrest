/***********************************************************************************************************************************
Binary to String Encode/Decode

These high-level functions are preferred to the low-level functions for each encoding type in the encode subdirectory.
***********************************************************************************************************************************/
#ifndef COMMON_ENCODE_H
#define COMMON_ENCODE_H

#include <stddef.h>

/***********************************************************************************************************************************
Encoding types
***********************************************************************************************************************************/
typedef enum
{
    encodeBase64
} EncodeType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Encode binary data to a printable string
void encodeToStr(EncodeType encodeType, const unsigned char *source, size_t sourceSize, char *destination);

// Size of the string returned by encodeToStr()
size_t encodeToStrSize(EncodeType encodeType, size_t sourceSize);

// Decode a string to binary data
void decodeToBin(EncodeType encodeType, const char *source, unsigned char *destination);

// Size of the binary data returned by decodeToBin()
size_t decodeToBinSize(EncodeType encodeType, const char *source);

// Check that the encoded string is valid
bool decodeToBinValid(EncodeType encodeType, const char *source);

// Validate the encoded string
void decodeToBinValidate(EncodeType encodeType, const char *source);

#endif
