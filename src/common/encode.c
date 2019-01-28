/***********************************************************************************************************************************
Binary to String Encode/Decode
***********************************************************************************************************************************/
#include <stdbool.h>
#include <string.h>

#include "common/encode.h"
#include "common/encode/base64.h"
#include "common/debug.h"
#include "common/error.h"

/***********************************************************************************************************************************
Macro to handle invalid encode type errors
***********************************************************************************************************************************/
#define ENCODE_TYPE_INVALID_ERROR(encodeType)                                                                                      \
    THROW_FMT(AssertError, "invalid encode type %u", encodeType);

/***********************************************************************************************************************************
Encode binary data to a printable string
***********************************************************************************************************************************/
void
encodeToStr(EncodeType encodeType, const unsigned char *source, size_t sourceSize, char *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, encodeType);
        FUNCTION_TEST_PARAM(UCHARP, source);
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
        FUNCTION_TEST_PARAM(CHARP, destination);
    FUNCTION_TEST_END();

    if (encodeType == encodeBase64)
        encodeToStrBase64(source, sourceSize, destination);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Size of the string returned by encodeToStr()
***********************************************************************************************************************************/
size_t
encodeToStrSize(EncodeType encodeType, size_t sourceSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, encodeType);
        FUNCTION_TEST_PARAM(SIZE, sourceSize);
    FUNCTION_TEST_END();

    size_t destinationSize = 0;

    if (encodeType == encodeBase64)
        destinationSize = encodeToStrSizeBase64(sourceSize);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);

    FUNCTION_TEST_RETURN(destinationSize);
}

/***********************************************************************************************************************************
Decode a string to binary data
***********************************************************************************************************************************/
void
decodeToBin(EncodeType encodeType, const char *source, unsigned char *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, encodeType);
        FUNCTION_TEST_PARAM(CHARP, source);
        FUNCTION_TEST_PARAM(UCHARP, destination);
    FUNCTION_TEST_END();

    if (encodeType == encodeBase64)
        decodeToBinBase64(source, destination);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Size of the binary data returned by decodeToBin()
***********************************************************************************************************************************/
size_t
decodeToBinSize(EncodeType encodeType, const char *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, encodeType);
        FUNCTION_TEST_PARAM(CHARP, source);
    FUNCTION_TEST_END();

    size_t destinationSize = 0;

    if (encodeType == encodeBase64)
        destinationSize = decodeToBinSizeBase64(source);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);

    FUNCTION_TEST_RETURN(destinationSize);
}

/***********************************************************************************************************************************
Check that the encoded string is valid
***********************************************************************************************************************************/
bool
decodeToBinValid(EncodeType encodeType, const char *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, encodeType);
        FUNCTION_TEST_PARAM(CHARP, source);
    FUNCTION_TEST_END();

    bool valid = true;

    TRY_BEGIN()
    {
        decodeToBinValidate(encodeType, source);
    }
    CATCH(FormatError)
    {
        valid = false;
    }
    TRY_END();

    FUNCTION_TEST_RETURN(valid);
}

/***********************************************************************************************************************************
Validate the encoded string
***********************************************************************************************************************************/
void
decodeToBinValidate(EncodeType encodeType, const char *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, encodeType);
        FUNCTION_TEST_PARAM(CHARP, source);
    FUNCTION_TEST_END();

    if (encodeType == encodeBase64)
        decodeToBinValidateBase64(source);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);

    FUNCTION_TEST_RETURN_VOID();
}
