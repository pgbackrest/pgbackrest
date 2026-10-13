/***********************************************************************************************************************************
Fuzz Pack
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/type/pack.h"

#include "fuzz.h"

/***********************************************************************************************************************************
Read every field in a container
***********************************************************************************************************************************/
static void
fuzzPackRead(PackRead *const read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, read);
    FUNCTION_TEST_END();

    while (pckReadNext(read))
    {
        const unsigned int id = pckReadId(read);

        switch (pckReadType(read))
        {
            case pckTypeArray:
                pckReadArrayBeginP(read, .id = id);
                fuzzPackRead(read);
                pckReadArrayEndP(read);
                break;

            case pckTypeBin:
                pckReadBinP(read, .id = id);
                break;

            case pckTypeBool:
                pckReadBoolP(read, .id = id);
                break;

            case pckTypeI32:
                pckReadI32P(read, .id = id);
                break;

            case pckTypeI64:
                pckReadI64P(read, .id = id);
                break;

            case pckTypeMode:
                pckReadModeP(read, .id = id);
                break;

            case pckTypeObj:
                pckReadObjBeginP(read, .id = id);
                fuzzPackRead(read);
                pckReadObjEndP(read);
                break;

            case pckTypePack:
            {
                PackRead *const readPack = pckReadPackReadP(read, .id = id);

                fuzzPackRead(readPack);
                pckReadEndP(readPack);
                break;
            }

            case pckTypeStr:
                pckReadStrP(read, .id = id);
                break;

            case pckTypeStrId:
                pckReadStrIdP(read, .id = id);
                break;

            case pckTypeTime:
                pckReadTimeP(read, .id = id);
                break;

            case pckTypeU32:
                pckReadU32P(read, .id = id);
                break;

            case pckTypeU64:
                pckReadU64P(read, .id = id);
                break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
fuzzTarget(const Buffer *const input)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, input);
    FUNCTION_TEST_END();

    PackRead *const read = pckReadNewC(bufPtrConst(input), bufUsed(input));

    fuzzPackRead(read);
    pckReadEndP(read);

    FUNCTION_TEST_RETURN_VOID();
}
