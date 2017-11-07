/***********************************************************************************************************************************
Base64 Binary to String Encode/Decode
***********************************************************************************************************************************/
#include <string.h>

#include "common/encode/base64.h"
#include "common/error.h"

/***********************************************************************************************************************************
Encode binary data to a printable string
***********************************************************************************************************************************/
static const char encodeBase64Lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void
encodeToStrBase64(const unsigned char *source, int sourceSize, char *destination)
{
    int destinationIdx = 0;

    // Encode the string from three bytes to four characters
    for (int sourceIdx = 0; sourceIdx < sourceSize; sourceIdx += 3)
    {
        // First two characters are always used
        destination[destinationIdx++] = encodeBase64Lookup[source[sourceIdx] >> 2];
        destination[destinationIdx++] =
            encodeBase64Lookup[((source[sourceIdx] & 0x03) << 4) | ((source[sourceIdx + 1] & 0xf0) >> 4)];

        // Last two may be coded as = if there are less than three characters
        if (sourceSize - sourceIdx > 1)
        {
            destination[destinationIdx++] =
                encodeBase64Lookup[((source[sourceIdx + 1] & 0x0f) << 2) | ((source[sourceIdx + 2] & 0xc0) >> 6)];
        }
        else
            destination[destinationIdx++] = 0x3d;

        if (sourceSize - sourceIdx > 2)
            destination[destinationIdx++] = encodeBase64Lookup[source[sourceIdx + 2] & 0x3f];
        else
            destination[destinationIdx++] = 0x3d;
    }

    // Zero-terminate the string
    destination[destinationIdx] = 0;
}

/***********************************************************************************************************************************
Size of the destination param required by encodeToStrBase64() minus space for the null terminator
***********************************************************************************************************************************/
int
encodeToStrSizeBase64(int sourceSize)
{
    // Calculate how many groups of three are in the source
    int encodeGroupTotal = sourceSize / 3;

    // Increase by one if there is a partial group
    if (sourceSize % 3 != 0)
        encodeGroupTotal++;

    // Four characters are needed to encode each group
    return encodeGroupTotal * 4;
}

/***********************************************************************************************************************************
Decode a string to binary data
***********************************************************************************************************************************/
static const int decodeBase64Lookup[256] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

void
decodeToBinBase64(const char *source, unsigned char *destination)
{
    // Validate encoded string
    decodeToBinValidateBase64(source);

    int destinationIdx = 0;

    // Decode the binary data from four characters to three bytes
    for (unsigned int sourceIdx = 0; sourceIdx < strlen(source); sourceIdx += 4)
    {
        // Always decode the first character
        destination[destinationIdx++] =
            (unsigned char)(decodeBase64Lookup[(int)source[sourceIdx]] << 2 | decodeBase64Lookup[(int)source[sourceIdx + 1]] >> 4);

        // Second character is optional
        if (source[sourceIdx + 2] != 0x3d)
        {
            destination[destinationIdx++] = (unsigned char)
                (decodeBase64Lookup[(int)source[sourceIdx + 1]] << 4 | decodeBase64Lookup[(int)source[sourceIdx + 2]] >> 2);
        }

        // Third character is optional
        if (source[sourceIdx + 3] != 0x3d)
        {
            destination[destinationIdx++] = (unsigned char)
                (((decodeBase64Lookup[(int)source[sourceIdx + 2]] << 6) & 0xc0) | decodeBase64Lookup[(int)source[sourceIdx + 3]]);
        }
    }
}

/***********************************************************************************************************************************
Size of the destination param required by decodeToBinBase64()
***********************************************************************************************************************************/
int
decodeToBinSizeBase64(const char *source)
{
    // Validate encoded string
    decodeToBinValidateBase64(source);

    // Start with size calculated directly from source length
    int sourceSize = strlen(source);
    int destinationSize = sourceSize / 4 * 3;

    // Subtract last character if it is not present
    if (source[sourceSize - 1] == 0x3d)
    {
        destinationSize--;

        // Subtract second to last character if it is not present
        if (source[sourceSize - 2] == 0x3d)
            destinationSize--;
    }

    return destinationSize;
}

/***********************************************************************************************************************************
Validate the encoded string
***********************************************************************************************************************************/
void
decodeToBinValidateBase64(const char *source)
{
    // Check for the correct length
    int sourceSize = strlen(source);

    if (sourceSize % 4 != 0)
        ERROR_THROW(FormatError, "base64 size %d is not evenly divisible by 4", sourceSize);

    // Check all characters
    for (int sourceIdx = 0; sourceIdx < sourceSize; sourceIdx++)
    {
        // Check terminators
        if (source[sourceIdx] == 0x3d)
        {
            // Make sure they are only in the last two positions
            if (sourceIdx < sourceSize - 2)
                ERROR_THROW(FormatError, "base64 '=' character may only appear in last two positions");

            // If second to last char is = then last char must also be
            if (sourceIdx == sourceSize - 2 && source[sourceSize - 1] != 0x3d)
                ERROR_THROW(FormatError, "base64 last character must be '=' if second to last is");
        }
        else
        {
            // Error on any invalid characters
            if (decodeBase64Lookup[(int)source[sourceIdx]] == -1)
                ERROR_THROW(FormatError, "base64 invalid character found at position %d", sourceIdx);
        }
    }
}
