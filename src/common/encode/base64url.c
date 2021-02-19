/***********************************************************************************************************************************
Base64url Binary to String Encode/Decode
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/encode/base64url.h"
#include "common/debug.h"
#include "common/error.h"

/**********************************************************************************************************************************/
static const char encodeBase64LookupUrl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

void
encodeToStrBase64Url(const unsigned char *source, size_t sourceSize, char *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(UCHARDATA, source);
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
        FUNCTION_TEST_PARAM_P(CHARDATA, destination);
    FUNCTION_TEST_END();

    ASSERT(source != NULL);
    ASSERT(destination != NULL);

    unsigned int destinationIdx = 0;

    // Encode the string from three bytes to four characters
    for (unsigned int sourceIdx = 0; sourceIdx < sourceSize; sourceIdx += 3)
    {
        // First encoded character is always used completely
        destination[destinationIdx++] = encodeBase64LookupUrl[source[sourceIdx] >> 2];

        // If there is only one byte to encode then the second encoded character is only partly used
        if (sourceSize - sourceIdx == 1)
        {
            destination[destinationIdx++] = encodeBase64LookupUrl[(source[sourceIdx] & 0x03) << 4];
        }
        // Else if more than one byte to encode
        else
        {
            // If there is more than one byte to encode then the second encoded character is used completely
            destination[destinationIdx++] =
                encodeBase64LookupUrl[((source[sourceIdx] & 0x03) << 4) | ((source[sourceIdx + 1] & 0xf0) >> 4)];

            // If there are only two bytes to encode then the third encoded character is only partly used
            if (sourceSize - sourceIdx == 2)
            {
                destination[destinationIdx++] = encodeBase64LookupUrl[(source[sourceIdx + 1] & 0x0f) << 2];
            }
            // Else the third and fourth encoded characters are used completely
            else
            {
                destination[destinationIdx++] =
                    encodeBase64LookupUrl[((source[sourceIdx + 1] & 0x0f) << 2) | ((source[sourceIdx + 2] & 0xc0) >> 6)];
                destination[destinationIdx++] = encodeBase64LookupUrl[source[sourceIdx + 2] & 0x3f];
            }
        }
    }

    // Zero-terminate the string
    destination[destinationIdx] = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
size_t
encodeToStrSizeBase64Url(size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    // Calculate how many groups of three are in the source. Each group of three is encoded with four bytes.
    size_t encodeTotal = sourceSize / 3 * 4;

    // Dertermine additional required bytes for the partial group, if any
    switch (sourceSize % 3)
    {
        // One byte requires two characters to encode
        case 1:
            encodeTotal += 2;
            break;

        // Two bytes require three characters to encode
        case 2:
            encodeTotal += 3;
            break;

        // If mod is zero then sourceSize was evenly divisible and no additional bytes are required
        case 0:
            break;
    }

    FUNCTION_TEST_RETURN(encodeTotal);
}
