/***********************************************************************************************************************************
Binary to String Encode/Decode
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdbool.h>
#include <string.h>

#include "common/debug.h"
#include "common/encode.h"

/***********************************************************************************************************************************
Assert that encoding type is valid. This needs to be kept up to date with the last item in the enum.
***********************************************************************************************************************************/
#define ASSERT_ENCODE_TYPE_VALID(type)                                                                                             \
    ASSERT(type <= encodingHex);

/***********************************************************************************************************************************
Base64 encoding/decoding
***********************************************************************************************************************************/
static const char encodeBase64Lookup[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
encodeToStrBase64(const unsigned char *const source, const size_t sourceSize, char *const destination)
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
encodeToStrSizeBase64(const size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    // Four characters are needed to encode each 3 byte group (plus up to two bytes of padding)
    FUNCTION_TEST_RETURN(SIZE, (sourceSize + 2) / 3 * 4);
}

/**********************************************************************************************************************************/
static const int8_t decodeBase64Lookup[256] =
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
decodeToBinValidateBase64(const char *const source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    // Check for the correct length
    const size_t sourceSize = strlen(source);

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
decodeToBinBase64(const char *const source, unsigned char *const destination)
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
            destination[destinationIdx++] =
                (unsigned char)
                ((decodeBase64Lookup[(int)source[sourceIdx + 1]] << 4) | (decodeBase64Lookup[(int)source[sourceIdx + 2]] >> 2));
        }

        // Third character is optional
        if (source[sourceIdx + 3] != 0x3d)
        {
            destination[destinationIdx++] =
                (unsigned char)
                (((decodeBase64Lookup[(int)source[sourceIdx + 2]] << 6) & 0xc0) | decodeBase64Lookup[(int)source[sourceIdx + 3]]);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static size_t
decodeToBinSizeBase64(const char *const source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    // Validate encoded string
    decodeToBinValidateBase64(source);

    // Start with size calculated directly from source length
    const size_t sourceSize = strlen(source);
    size_t destinationSize = sourceSize / 4 * 3;

    // Subtract last character if it is not present
    if (source[sourceSize - 1] == 0x3d)
    {
        destinationSize--;

        // Subtract second to last character if it is not present
        if (source[sourceSize - 2] == 0x3d)
            destinationSize--;
    }

    FUNCTION_TEST_RETURN(SIZE, destinationSize);
}

/***********************************************************************************************************************************
Base64Url encoding
***********************************************************************************************************************************/
static const char encodeBase64LookupUrl[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void
encodeToStrBase64Url(const unsigned char *const source, const size_t sourceSize, char *const destination)
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
encodeToStrSizeBase64Url(const size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    // Four characters are needed to encode each 3 byte group, three characters for 2 bytes, and two characters for 1 byte
    FUNCTION_TEST_RETURN(SIZE, sourceSize / 3 * 4 + (sourceSize % 3 == 0 ? 0 : sourceSize % 3 + 1));
}

/***********************************************************************************************************************************
Hex encoding/decoding
***********************************************************************************************************************************/
static const char encodeHexLookup[512] =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
    "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
    "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
    "c0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

static void
encodeToStrHex(const unsigned char *const source, const size_t sourceSize, char *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(UCHARDATA, source);
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
        FUNCTION_TEST_PARAM_P(CHARDATA, destination);
    FUNCTION_TEST_END();

    ASSERT(source != NULL);
    ASSERT(destination != NULL);

    // Encode the string from one bytes to two characters
    for (unsigned int sourceIdx = 0; sourceIdx < sourceSize; sourceIdx += 1)
    {
        memcpy(destination, &encodeHexLookup[source[sourceIdx] * 2], 2);
        destination += 2;
    }

    // Zero-terminate the string
    *destination = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static size_t
encodeToStrSizeHex(const size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(SIZE, sourceSize * 2);
}

/**********************************************************************************************************************************/
// {uncrustify_off - array alignment}
static const int8_t decodeHexLookup[256] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};
// {uncrustify_on}

static void
decodeToBinValidateHex(const char *const source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    // Check for the correct length
    const size_t sourceSize = strlen(source);

    if (sourceSize % 2 != 0)
        THROW_FMT(FormatError, "hex size %zu is not evenly divisible by 2", sourceSize);

    // Check all characters
    for (unsigned int sourceIdx = 0; sourceIdx < sourceSize; sourceIdx++)
    {
        if (decodeHexLookup[(int)source[sourceIdx]] == -1)
            THROW_FMT(FormatError, "hex invalid character found at position %u", sourceIdx);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
decodeToBinHex(const char *const source, unsigned char *const destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
        FUNCTION_TEST_PARAM_P(UCHARDATA, destination);
    FUNCTION_TEST_END();

    // Validate encoded string
    decodeToBinValidateHex(source);

    int destinationIdx = 0;

    // Decode the binary data from two characters to one byte
    for (unsigned int sourceIdx = 0; sourceIdx < strlen(source); sourceIdx += 2)
    {
        destination[destinationIdx++] =
            (unsigned char)(decodeHexLookup[(int)source[sourceIdx]] << 4 | decodeHexLookup[(int)source[sourceIdx + 1]]);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static size_t
decodeToBinSizeHex(const char *const source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    // Validate encoded string
    decodeToBinValidateHex(source);

    FUNCTION_TEST_RETURN(SIZE, strlen(source) / 2);
}

/***********************************************************************************************************************************
Generic encoding/decoding
***********************************************************************************************************************************/
FN_EXTERN void
encodeToStr(const EncodingType type, const unsigned char *const source, const size_t sourceSize, char *const destination)
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
        case encodingBase64:
            encodeToStrBase64(source, sourceSize, destination);
            break;

        case encodingBase64Url:
            encodeToStrBase64Url(source, sourceSize, destination);
            break;

        case encodingHex:
            encodeToStrHex(source, sourceSize, destination);
            break;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
encodeToStrSize(const EncodingType type, const size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    ASSERT_ENCODE_TYPE_VALID(type);

    size_t destinationSize = SIZE_MAX;

    switch (type)
    {
        case encodingBase64:
            destinationSize = encodeToStrSizeBase64(sourceSize);
            break;

        case encodingBase64Url:
            destinationSize = encodeToStrSizeBase64Url(sourceSize);
            break;

        case encodingHex:
            destinationSize = encodeToStrSizeHex(sourceSize);
            break;
    }

    FUNCTION_TEST_RETURN(SIZE, destinationSize);
}

/**********************************************************************************************************************************/
FN_EXTERN void
decodeToBin(const EncodingType type, const char *const source, unsigned char *const destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(STRINGZ, source);
        FUNCTION_TEST_PARAM_P(UCHARDATA, destination);
    FUNCTION_TEST_END();

    ASSERT_ENCODE_TYPE_VALID(type);

    switch (type)
    {
        case encodingBase64:
            decodeToBinBase64(source, destination);
            break;

        case encodingHex:
            decodeToBinHex(source, destination);
            break;

        default:
            ASSERT_MSG("unsupported");
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
decodeToBinSize(const EncodingType type, const char *const source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(STRINGZ, source);
    FUNCTION_TEST_END();

    ASSERT_ENCODE_TYPE_VALID(type);

    size_t destinationSize = SIZE_MAX;

    switch (type)
    {
        case encodingBase64:
            destinationSize = decodeToBinSizeBase64(source);
            break;

        case encodingHex:
            destinationSize = decodeToBinSizeHex(source);
            break;

        default:
            ASSERT_MSG("unsupported");
    }

    FUNCTION_TEST_RETURN(SIZE, destinationSize);
}
