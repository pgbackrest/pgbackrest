/***********************************************************************************************************************************
Base64url Binary to String Encode/Decode

The high-level functions in encode.c should be used in preference to these low-level functions.
***********************************************************************************************************************************/
#ifndef COMMON_ENCODE_BASE64URL_H
#define COMMON_ENCODE_BASE64URL_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Encode binary data to a printable string
void encodeToStrBase64Url(const unsigned char *source, size_t sourceSize, char *destination);

// Size of the destination param required by encodeToStrBase64() minus space for the null terminator
size_t encodeToStrSizeBase64Url(size_t sourceSize);

#endif
