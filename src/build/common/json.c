/***********************************************************************************************************************************
JSON Extensions
***********************************************************************************************************************************/
// Include core module
#include "common/type/json.c"

#include "build/common/json.h"

/**********************************************************************************************************************************/
bool
jsonReadUntil(JsonRead *const this, const JsonType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, jsonReadTypeNextIgnoreComma(this) != type);
}
