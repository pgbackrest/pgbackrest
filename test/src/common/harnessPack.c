/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/type/convert.h"
#include "common/type/pack.h"
#include "common/type/stringz.h"

#include "common/harnessDebug.h"
#include "common/harnessPack.h"

/**********************************************************************************************************************************/
String *hrnPackBufToStr(const Buffer *buffer)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BUFFER, buffer);
    FUNCTION_HARNESS_END();

    FUNCTION_HARNESS_RESULT(STRING, hrnPackToStr(pckReadNewBuf(buffer)));
}

/**********************************************************************************************************************************/
String *hrnPackToStr(PackRead *read)
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

        PackType type = pckReadType(read);
        unsigned int id = pckReadId(read);

        strCatFmt(result, "%u:%s:", id, strZ(pckTypeToStr(type)));

        switch (type)
        {
            case pckTypeArray:
            {
                pckReadArrayBeginP(read, .id = id);
                strCatFmt(result, "[%s]", strZ(hrnPackToStr(read)));
                pckReadArrayEnd(read);
                break;
            }

            case pckTypeBool:
            {
                strCatZ(result, cvtBoolToConstZ(pckReadBoolP(read, .id = id)));
                break;
            }

            case pckTypeInt32:
            {
                strCatFmt(result, "%d", pckReadInt32P(read, .id = id));
                break;
            }

            case pckTypeInt64:
            {
                strCatFmt(result, "%" PRId64, pckReadInt64P(read, .id = id));
                break;
            }

            case pckTypeObj:
            {
                pckReadObjBeginP(read, .id = id);
                strCatFmt(result, "{%s}", strZ(hrnPackToStr(read)));
                pckReadObjEnd(read);
                break;
            }

            case pckTypeStr:
            {
                strCatFmt(result, "%s", strZ(pckReadStrP(read, .id = id)));
                break;
            }

            case pckTypeUInt32:
            {
                strCatFmt(result, "%u", pckReadUInt32P(read, .id = id));
                break;
            }

            case pckTypeUInt64:
            {
                strCatFmt(result, "%" PRIu64, pckReadUInt64P(read, .id = id));
                break;
            }

            default:
                THROW_FMT(AssertError, "'%s' NOT IMPLEMENTED", strZ(pckTypeToStr(type)));
        }

        first = false;
    }

    FUNCTION_HARNESS_RESULT(STRING, result);
}
