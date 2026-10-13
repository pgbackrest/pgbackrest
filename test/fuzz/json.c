/***********************************************************************************************************************************
Fuzz JSON
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/type/json.h"

#include "fuzz.h"

/**********************************************************************************************************************************/
void
fuzzTarget(const Buffer *const input)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, input);
    FUNCTION_TEST_END();

    const String *const json = strNewBuf(input);

    // Skip the JSON
    TRY_BEGIN()
    {
        jsonValidate(json);
    }
    CATCH_ANY()
    {
        // Errors thrown for invalid input are expected
    }
    TRY_END();

    // Read the JSON into a variant
    jsonToVar(json);

    FUNCTION_TEST_RETURN_VOID();
}
