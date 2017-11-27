/***********************************************************************************************************************************
Binary to String Encode/Decode
***********************************************************************************************************************************/
#include <string.h>

#include "common/encode.h"
#include "common/encode/base64.h"
#include "common/error.h"

/***********************************************************************************************************************************
Macro to handle invalid encode type errors
***********************************************************************************************************************************/
#define ENCODE_TYPE_INVALID_ERROR(encodeType)                                                                                      \
    THROW(AssertError, "invalid encode type %d", encodeType);

/***********************************************************************************************************************************
Encode binary data to a printable string
***********************************************************************************************************************************/
void
encodeToStr(EncodeType encodeType, const unsigned char *source, int sourceSize, char *destination)
{
    if (encodeType == encodeBase64)
        encodeToStrBase64(source, sourceSize, destination);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);
}

/***********************************************************************************************************************************
Size of the string returned by encodeToStr()
***********************************************************************************************************************************/
int
encodeToStrSize(EncodeType encodeType, int sourceSize)
{
    int destinationSize = 0;

    if (encodeType == encodeBase64)
        destinationSize = encodeToStrSizeBase64(sourceSize);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);

    return destinationSize;
}

/***********************************************************************************************************************************
Decode a string to binary data
***********************************************************************************************************************************/
void
decodeToBin(EncodeType encodeType, const char *source, unsigned char *destination)
{
    if (encodeType == encodeBase64)
        decodeToBinBase64(source, destination);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);
}

/***********************************************************************************************************************************
Size of the binary data returned by decodeToBin()
***********************************************************************************************************************************/
int
decodeToBinSize(EncodeType encodeType, const char *source)
{
    int destinationSize = 0;

    if (encodeType == encodeBase64)
        destinationSize = decodeToBinSizeBase64(source);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);

    return destinationSize;
}

/***********************************************************************************************************************************
Check that the encoded string is valid
***********************************************************************************************************************************/
bool
decodeToBinValid(EncodeType encodeType, const char *source)
{
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

    return valid;
}

/***********************************************************************************************************************************
Validate the encoded string
***********************************************************************************************************************************/
void
decodeToBinValidate(EncodeType encodeType, const char *source)
{
    if (encodeType == encodeBase64)
        decodeToBinValidateBase64(source);
    else
        ENCODE_TYPE_INVALID_ERROR(encodeType);
}
