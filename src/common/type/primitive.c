/***********************************************************************************************************************************
Primitive Data Types
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/primitive.h"

/***********************************************************************************************************************************
New uint64 primitive
***********************************************************************************************************************************/
PrmUInt64 *
prmUInt64New(uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    PrmUInt64Const *this = memNew(sizeof(PrmUInt64Const));

    *this = (PrmUInt64Const)
    {
        .value = value,
    };

    FUNCTION_TEST_RETURN((PrmUInt64 *)this);
}

/***********************************************************************************************************************************
Return uint64
***********************************************************************************************************************************/
uint64_t
prmUInt64(const PrmUInt64 *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PRM_UINT64, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(((const PrmUInt64Const *)this)->value);
}

/***********************************************************************************************************************************
Convert variant to a zero-terminated string for logging
***********************************************************************************************************************************/
String *
prmUInt64ToLog(const PrmUInt64 *this)
{
    if (this == NULL)
        return strDup(NULL_STR);

    return strNewFmt("%" PRIu64, ((const PrmUInt64Const *)this)->value);
}
