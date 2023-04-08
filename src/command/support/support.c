/***********************************************************************************************************************************
Support Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/support/support.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"
#include "config/parse.h"

/***********************************************************************************************************************************
In some environments this will not be externed
***********************************************************************************************************************************/
extern char **environ;

/***********************************************************************************************************************************
Render config
***********************************************************************************************************************************/
static void
cmdSupportRenderConfigEnv(JsonWrite *const json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, json);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        jsonWriteKeyStrId(json, STRID5("env", 0x59c50));
        jsonWriteObjectBegin(json);

        // Loop through all environment variables and look for our env vars by matching the prefix
        unsigned int environIdx = 0;
        KeyValue *const keyValue = kvNew();

        while (environ[environIdx] != NULL)
        {
            const char *const environKeyValue = environ[environIdx];
            environIdx++;

            if (strstr(environKeyValue, PGBACKREST_ENV) == environKeyValue)
            {
                // Find the first = char
                const char *const equalPtr = strchr(environKeyValue, '=');
                ASSERT(equalPtr != NULL);

                // Get key and value
                const String *const key = strNewZN(environKeyValue, (size_t)(equalPtr - environKeyValue));
                const String *const value = STR(equalPtr + 1);

                kvPut(keyValue, VARSTR(key), VARSTR(value));
            }
        }

        // Render environment variables
        const StringList *const keyList = strLstSort(strLstNewVarLst(kvKeyList(keyValue)), sortOrderAsc);

        for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
        {
            const String *const key = strLstGet(keyList, keyIdx);
            const String *const value = varStr(kvGet(keyValue, VARSTR(key)));

            jsonWriteKey(json, key);
            jsonWriteObjectBegin(json);

            CfgParseOptionResult option = {.found = false};
            const ErrorType *errType = NULL;
            const String *errMessage = NULL;

            TRY_BEGIN()
            {
                option = cfgParseOptionP(strReplaceChr(strLower(strNewZ(strZ(key) + PGBACKREST_ENV_SIZE)), '_', '-'));
            }
            CATCH_ANY()
            {
                errType = errorType();
                errMessage = strNewZ(errorMessage());
            }
            TRY_END();

            if (errType != NULL)
            {
                jsonWriteKeyStrId(json, STRID5("err", 0x4a450));
                jsonWriteObjectBegin(json);

                jsonWriteKeyStrId(json, STRID5("msg", 0x1e6d0));
                jsonWriteStr(json, errMessage);

                jsonWriteKeyStrId(json, STRID5("type", 0x2c3340));
                jsonWriteZ(json, errorTypeName(errType));

                jsonWriteObjectEnd(json);
            }

            jsonWriteKeyStrId(json, STRID5("val", 0x30360));
            jsonWriteStr(json, value);

            if (errType == NULL && (!option.found || option.negate || option.reset))
            {
                jsonWriteKeyStrId(json, STRID5("warn", 0x748370));

                // Warn if the option not found
                if (!option.found)
                {
                    jsonWriteZ(json, "invalid option");
                }
                // Warn if negate option found in env
                else if (option.negate)
                {
                    jsonWriteZ(json, "invalid negate option");
                }
                // Warn if reset option found in env
                else
                {
                    ASSERT(option.reset);

                    jsonWriteZ(json, "invalid reset option");
                }
            }

            jsonWriteObjectEnd(json);
        }

        jsonWriteObjectEnd(json);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

static void
cmdSupportRenderConfigFile(JsonWrite *const json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, json);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // const Ini *const ini = cfgParseIni();

        jsonWriteKeyStrId(json, STRID5("file", 0x2b1260));

        // if (ini == NULL)
            jsonWriteNull(json);
        // else
        // {
        // }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

static void
cmdSupportRenderConfig(JsonWrite *const json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, json);
    FUNCTION_TEST_END();

    jsonWriteKeyStrId(json, STRID5("cfg", 0x1cc30));
    jsonWriteObjectBegin(json);

    cmdSupportRenderConfigEnv(json);
    cmdSupportRenderConfigFile(json);

    jsonWriteObjectEnd(json);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static String *
cmdSupportRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const json = jsonWriteNewP(.json = result);

        jsonWriteObjectBegin(json);
        cmdSupportRenderConfig(json);
        jsonWriteObjectEnd(json);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdSupport(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    FUNCTION_LOG_RETURN_VOID();
}
