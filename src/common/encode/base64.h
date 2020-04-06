/***********************************************************************************************************************************
Base64 Binary to String Encode/Decode

The high-level functions in encode.c should be used in preference to these low-level functions.
***********************************************************************************************************************************/
#ifndef COMMON_ENCODE_BASE64_H
#define COMMON_ENCODE_BASE64_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Encode binary data to a printable string
void encodeToStrBase64(const unsigned char *source, size_t sourceSize, char *destination);

// Size of the destination param required by encodeToStrBase64() minus space for the null terminator
size_t encodeToStrSizeBase64(size_t sourceSize);

// Decode a string to binary data
void decodeToBinBase64(const char *source, unsigned char *destination);

// Size of the destination param required by decodeToBinBase64()
size_t decodeToBinSizeBase64(const char *source);

// Validate the encoded string
void decodeToBinValidateBase64(const char *source);

#endif
