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
cmdSupportRenderConfigVal(JsonWrite *const json, const String *const optionName, const StringList *valueList, const bool env)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, json);
        FUNCTION_TEST_PARAM(STRING, optionName);
        FUNCTION_TEST_PARAM(STRING_LIST, valueList);
        FUNCTION_TEST_PARAM(BOOL, env);
    FUNCTION_TEST_END();

    ASSERT(json != NULL);
    ASSERT(optionName != NULL);
    ASSERT(valueList != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        jsonWriteObjectBegin(json);

        CfgParseOptionResult option = cfgParseOptionP(optionName);

        if (option.found)
        {
            jsonWriteKeyStrId(json, STRID5("val", 0x30360));

            if (option.multi)
            {
                ASSERT(strLstSize(valueList) >= 1);

                if (env)
                {
                    ASSERT(strLstSize(valueList) == 1);
                    valueList = strLstNewSplitZ(strLstGet(valueList, 0), ":");
                }

                jsonWriteArrayBegin(json);

                for (unsigned int valueIdx = 0; valueIdx < strLstSize(valueList); valueIdx++)
                    jsonWriteStr(json, strLstGet(valueList, valueIdx));

                jsonWriteArrayEnd(json);
            }
            else
            {
                ASSERT(strLstSize(valueList) == 1);
                jsonWriteStr(json, strLstGet(valueList, 0));
            }
        }

        if (!option.found || option.negate || option.reset)
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
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

static void
cmdSupportRenderConfigEnv(JsonWrite *const json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, json);
    FUNCTION_TEST_END();

    ASSERT(json != NULL);

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
            StringList *const valueList = strLstNew();
            strLstAdd(valueList, varStr(kvGet(keyValue, VARSTR(key))));

            jsonWriteKey(json, key);
            cmdSupportRenderConfigVal(
                json, strReplaceChr(strLower(strNewZ(strZ(key) + PGBACKREST_ENV_SIZE)), '_', '-'), valueList, true);
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

    ASSERT(json != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Ini *const ini = cfgParseIni();

        jsonWriteKeyStrId(json, STRID5("file", 0x2b1260));

        if (ini == NULL)
            jsonWriteNull(json);
        else
        {
            const StringList *const sectionList = strLstSort(iniSectionList(ini), sortOrderAsc);

            jsonWriteObjectBegin(json);

            for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
            {
                const String *const section = strLstGet(sectionList, sectionIdx);

                jsonWriteKey(json, section);
                jsonWriteObjectBegin(json);

                const StringList *const keyList = strLstSort(iniSectionKeyList(ini, section), sortOrderAsc);

                for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                {
                    const String *const key = strLstGet(keyList, keyIdx);
                    StringList *valueList;

                    if (iniSectionKeyIsList(ini, section, key))
                        valueList = iniGetList(ini, section, key);
                    else
                    {
                        valueList = strLstNew();
                        strLstAdd(valueList, iniGet(ini, section, key));
                    }

                    jsonWriteKey(json, key);
                    cmdSupportRenderConfigVal(json, key, valueList, false);
                }

                jsonWriteObjectEnd(json);
            }

            jsonWriteObjectEnd(json);
        }
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

    ASSERT(json != NULL);

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

    strFree(cmdSupportRender());

    FUNCTION_LOG_RETURN_VOID();
}
