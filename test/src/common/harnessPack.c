/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/type/convert.h"
#include "common/type/pack.h"

#include "common/harnessDebug.h"
#include "common/harnessPack.h"

/**********************************************************************************************************************************/
String *pckBufToStr(const Buffer *buffer)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BUFFER, buffer);
    FUNCTION_HARNESS_END();

    FUNCTION_HARNESS_RESULT(STRING, pckToStr(pckReadNewBuf(buffer)));
}

/**********************************************************************************************************************************/
String *pckToStr(PackRead *read)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, read);
    FUNCTION_HARNESS_END();

    String *result = strNew("");
    bool first = true;

    while (pckReadNext(read))
    {
        if (!first)
            strCatZ(result, ", ");

        strCatFmt(result, "%u:%s:", pckReadId(read), strZ(pckTypeToStr(pckReadType(read))));

        if (pckReadType(read) == pckTypeArray)
            strCatFmt(result, "[%s]", strZ(pckToStr(read)));
        else if (pckReadType(read) == pckTypeObj)
            strCatFmt(result, "{%s}", strZ(pckToStr(read)));
        else
        {
            if (pckReadNullP(read))
                strCatZ(result, NULL_Z);
            else
            {
                switch (pckReadType(read))
                {
                    case pckTypeBool:
                    {
                        strCatZ(result, cvtBoolToConstZ(pckReadBoolP(read)));
                        break;
                    }

                    case pckTypeInt32:
                    {
                        strCatFmt(result, "%d", pckReadInt32P(read));
                        break;
                    }

                    case pckTypeInt64:
                    {
                        strCatFmt(result, "%" PRId64, pckReadInt64P(read));
                        break;
                    }

                    case pckTypeStr:
                    {
                        strCatFmt(result, "%s", strZ(pckReadStrP(read)));
                        break;
                    }

                    case pckTypeUInt32:
                    {
                        strCatFmt(result, "%u", pckReadUInt32P(read));
                        break;
                    }

                    case pckTypeUInt64:
                    {
                        strCatFmt(result, "%" PRIu64, pckReadUInt64P(read));
                        break;
                    }

                    default:
                        THROW_FMT(AssertError, "'%s' NOT IMPLEMENTED", strZ(pckTypeToStr(pckReadType(read))));
                }
            }
        }

        first = false;
    }

    FUNCTION_HARNESS_RESULT(STRING, result);
}
