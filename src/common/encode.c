/***********************************************************************************************************************************
Binary to String Encode/Decode
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdbool.h>
#include <string.h>

#include "common/encode.h"
#include "common/debug.h"
#include "common/error.h"

/***********************************************************************************************************************************
Assert that encoding type is valid. This needs to be kept up to date with the last item in the enum.
***********************************************************************************************************************************/
#define ASSERT_ENCODE_TYPE_VALID(type)                                                                                             \
    ASSERT(type <= encodeBase64Url);

/***********************************************************************************************************************************
Base64 encoding/decoding
***********************************************************************************************************************************/
static const char encodeBase64Lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
encodeToStrBase64(const unsigned char *source, size_t sourceSize, char *destination)
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
        destination[destinationIdx++] = encodeBase64Lookup[source[sourceIdx] >> 2];

        // If there is only one byte to encode then the second encoded character is only partly used and the third and fourth
        // encoded characters are padded.
        if (sourceSize - sourceIdx == 1)
        {
            destination[destinationIdx++] = encodeBase64Lookup[(source[sourceIdx] & 0x03) << 4];
            destination[destinationIdx++] = 0x3d;
            destination[destinationIdx++] = 0x3d;
        }
        // Else if more than one byte to encode
        else
        {
            // If there is more than one byte to encode then the second encoded character is used completely
            destination[destinationIdx++] =
                encodeBase64Lookup[((source[sourceIdx] & 0x03) << 4) | ((source[sourceIdx + 1] & 0xf0) >> 4)];

            // If there are only two bytes to encode then the third encoded character is only partly used and the fourth encoded
            // character is padded.
            if (sourceSize - sourceIdx == 2)
            {
                destination[destinationIdx++] = encodeBase64Lookup[(source[sourceIdx + 1] & 0x0f) << 2];
                destination[destinationIdx++] = 0x3d;
            }
            // Else the third and fourth encoded characters are used completely
            else
            {
                destination[destinationIdx++] =
                    encodeBase64Lookup[((source[sourceIdx + 1] & 0x0f) << 2) | ((source[sourceIdx + 2] & 0xc0) >> 6)];
                destination[destinationIdx++] = encodeBase64Lookup[source[sourceIdx + 2] & 0x3f];
            }
        }
    }

    // Zero-terminate the string
    destination[destinationIdx] = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static size_t
encodeToStrSizeBase64(size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    // Calculate how many groups of three are in the source
    size_t encodeGroupTotal = sourceSize / 3;

    // Increase by one if there is a partial group
    if (sourceSize % 3 != 0)
        encodeGroupTotal++;

    // Four characters are needed to encode each group
    FUNCTION_TEST_RETURN(encodeGroupTotal * 4);
}

/**********************************************************************************************************************************/
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

static void
decodeToBinValidateBase64(const char *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    // Check for the correct length
    size_t sourceSize = strlen(source);

    if (sourceSize % 4 != 0)
        THROW_FMT(FormatError, "base64 size %zu is not evenly divisible by 4", sourceSize);

    // Check all characters
    for (unsigned int sourceIdx = 0; sourceIdx < sourceSize; sourceIdx++)
    {
        // Check terminators
        if (source[sourceIdx] == 0x3d)
        {
            // Make sure they are only in the last two positions
            if (sourceIdx < sourceSize - 2)
                THROW(FormatError, "base64 '=' character may only appear in last two positions");

            // If second to last char is = then last char must also be
            if (sourceIdx == sourceSize - 2 && source[sourceSize - 1] != 0x3d)
                THROW(FormatError, "base64 last character must be '=' if second to last is");
        }
        else
        {
            // Error on any invalid characters
            if (decodeBase64Lookup[(int)source[sourceIdx]] == -1)
                THROW_FMT(FormatError, "base64 invalid character found at position %u", sourceIdx);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
decodeToBinBase64(const char *source, unsigned char *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
        FUNCTION_TEST_PARAM_P(UCHARDATA, destination);
    FUNCTION_TEST_END();

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

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static size_t
decodeToBinSizeBase64(const char *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    // Validate encoded string
    decodeToBinValidateBase64(source);

    // Start with size calculated directly from source length
    size_t sourceSize = strlen(source);
    size_t destinationSize = sourceSize / 4 * 3;

    // Subtract last character if it is not present
    if (source[sourceSize - 1] == 0x3d)
    {
        destinationSize--;

        // Subtract second to last character if it is not present
        if (source[sourceSize - 2] == 0x3d)
            destinationSize--;
    }

    FUNCTION_TEST_RETURN(destinationSize);
}

/***********************************************************************************************************************************
Base64 encoding
***********************************************************************************************************************************/
static const char encodeBase64LookupUrl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void
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
static size_t
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

/***********************************************************************************************************************************
Generic encoding/decoding
***********************************************************************************************************************************/
void
encodeToStr(EncodeType type, const unsigned char *source, size_t sourceSize, char *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM_P(UCHARDATA, source);
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
        FUNCTION_TEST_PARAM_P(CHARDATA, destination);
    FUNCTION_TEST_END();

    ASSERT_ENCODE_TYPE_VALID(type);

    switch (type)
    {
        case encodeBase64:
            encodeToStrBase64(source, sourceSize, destination);
            break;

        case encodeBase64Url:
            encodeToStrBase64Url(source, sourceSize, destination);
            break;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
size_t
encodeToStrSize(EncodeType type, size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    ASSERT_ENCODE_TYPE_VALID(type);

    size_t destinationSize = SIZE_MAX;

    switch (type)
    {
        case encodeBase64:
            destinationSize = encodeToStrSizeBase64(sourceSize);
            break;

        case encodeBase64Url:
            destinationSize = encodeToStrSizeBase64Url(sourceSize);
            break;
    }

    FUNCTION_TEST_RETURN(destinationSize);
}

/**********************************************************************************************************************************/
void
decodeToBin(EncodeType type, const char *source, unsigned char *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(STRINGZ, source);
        FUNCTION_TEST_PARAM_P(UCHARDATA, destination);
    FUNCTION_TEST_END();

    ASSERT_ENCODE_TYPE_VALID(type);

    switch (type)
    {
        case encodeBase64:
            decodeToBinBase64(source, destination);
            break;

        case encodeBase64Url:
            THROW(AssertError, "unsupported");
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
size_t
decodeToBinSize(EncodeType type, const char *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    ASSERT_ENCODE_TYPE_VALID(type);

    size_t destinationSize = SIZE_MAX;

    switch (type)
    {
        case encodeBase64:
            destinationSize = decodeToBinSizeBase64(source);
            break;

        case encodeBase64Url:
            THROW(AssertError, "unsupported");
    }

    FUNCTION_TEST_RETURN(destinationSize);
}
