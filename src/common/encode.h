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
void encodeToStr(EncodeType type, const unsigned char *source, size_t sourceSize, char *destination);

// Size of the string returned by encodeToStr()
size_t encodeToStrSize(EncodeType type, size_t sourceSize);

// Decode a string to binary data
void decodeToBin(EncodeType type, const char *source, unsigned char *destination);

// Size of the binary data returned by decodeToBin()
size_t decodeToBinSize(EncodeType type, const char *source);

// Check that the encoded string is valid
bool decodeToBinValid(EncodeType type, const char *source);

// Validate the encoded string
void decodeToBinValidate(EncodeType type, const char *source);

#endif
