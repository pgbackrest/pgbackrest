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

    FUNCTION_HARNESS_RETURN(STRING, hrnPackToStr(pckReadNewBuf(buffer)));
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
            case pckTypeUnknown:
                THROW_FMT(AssertError, "invalid type %s", strZ(pckTypeToStr(type)));

            case pckTypeArray:
                pckReadArrayBeginP(read, .id = id);
                strCatFmt(result, "[%s]", strZ(hrnPackToStr(read)));
                pckReadArrayEndP(read);
                break;

            case pckTypeBool:
                strCatZ(result, cvtBoolToConstZ(pckReadBoolP(read, .id = id)));
                break;

            case pckTypeBin:
                strCatFmt(result, "%s", strZ(bufHex(pckReadBinP(read, .id = id))));
                break;

            case pckTypeI32:
                strCatFmt(result, "%d", pckReadI32P(read, .id = id));
                break;

            case pckTypeI64:
                strCatFmt(result, "%" PRId64, pckReadI64P(read, .id = id));
                break;

            case pckTypeObj:
                pckReadObjBeginP(read, .id = id);
                strCatFmt(result, "{%s}", strZ(hrnPackToStr(read)));
                pckReadObjEndP(read);
                break;

            case pckTypePtr:
                strCatFmt(result, "%p", pckReadPtrP(read, .id = id));
                break;

            case pckTypeStr:
                strCatFmt(result, "%s", strZ(pckReadStrP(read, .id = id)));
                break;

            case pckTypeTime:
                strCatFmt(result, "%" PRId64, (int64_t)pckReadTimeP(read, .id = id));
                break;

            case pckTypeU32:
                strCatFmt(result, "%u", pckReadU32P(read, .id = id));
                break;

            case pckTypeU64:
                strCatFmt(result, "%" PRIu64, pckReadU64P(read, .id = id));
                break;
        }

        first = false;
    }

    FUNCTION_HARNESS_RETURN(STRING, result);
}
