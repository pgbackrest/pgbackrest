/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/assert.h"
#include "common/type/convert.h"
#include "common/type/pack.h"
#include "common/type/stringZ.h"

#include "common/harnessDebug.h"
#include "common/harnessPack.h"

/**********************************************************************************************************************************/
String *hrnPackToStr(const Pack *const pack)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK, pack);
    FUNCTION_HARNESS_END();

    FUNCTION_HARNESS_RETURN(STRING, hrnPackReadToStr(pckReadNew(pack)));
}

/**********************************************************************************************************************************/
String *hrnPackReadToStr(PackRead *read)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, read);
    FUNCTION_HARNESS_END();

    String *result = strNew();
    bool first = true;

    while (pckReadNext(read))
    {
        if (!first)
            strCatZ(result, ", ");

        PackType type = pckReadType(read);
        unsigned int id = pckReadId(read);

        strCatFmt(result, "%u:%s:", id, strZ(strIdToStr(type)));

        switch (type)
        {
            case pckTypeArray:
                pckReadArrayBeginP(read, .id = id);
                strCatFmt(result, "[%s]", strZ(hrnPackReadToStr(read)));
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

            case pckTypeMode:
                strCatFmt(result, "%04o", pckReadModeP(read, .id = id));
                break;

            case pckTypeObj:
                pckReadObjBeginP(read, .id = id);
                strCatFmt(result, "{%s}", strZ(hrnPackReadToStr(read)));
                pckReadObjEndP(read);
                break;

            case pckTypePack:
            {
                strCatFmt(result, "<%s>", strZ(hrnPackReadToStr(pckReadPackReadP(read, .id = id))));
                break;
            }

            case pckTypePtr:
                strCatFmt(result, "%p", pckReadPtrP(read, .id = id));
                break;

            case pckTypeStr:
                strCatFmt(result, "%s", strZ(pckReadStrP(read, .id = id)));
                break;

            case pckTypeStrId:
                strCatFmt(result, "%s", strZ(strIdToStr(pckReadStrIdP(read, .id = id))));
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
