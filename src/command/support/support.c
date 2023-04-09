/***********************************************************************************************************************************
Support Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/support/support.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"
#include "config/config.h"
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

/***********************************************************************************************************************************
Render stanza
***********************************************************************************************************************************/
static StringList *
cmdSupportRenderStanzaList(void)
{
    FUNCTION_TEST_VOID();

    StringList *const result = strLstNew();

    if (cfgParseIni() != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const StringList *const sectionList = strLstSort(iniSectionList(cfgParseIni()), sortOrderAsc);

            for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
            {
                const String *const section = strLstGet(sectionList, sectionIdx);

                // Skip global sections
                if (strEqZ(section, CFGDEF_SECTION_GLOBAL) || strBeginsWithZ(section, CFGDEF_SECTION_GLOBAL ":"))
                    continue;

                // Extract stanza
                StringList *const sectionPart = strLstNewSplitZ(section, ":");
                ASSERT(strLstSize(sectionPart) <= 2);

                strLstAddIfMissing(result, strLstGet(sectionPart, 0));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}

static void
cmdSupportRenderStanza(JsonWrite *const json, const unsigned int argListSize, const char *argList[])
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, json);
        FUNCTION_TEST_PARAM(UINT, argListSize);
        FUNCTION_TEST_PARAM_P(VOID, argList);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const stanzaList = cmdSupportRenderStanzaList();

        jsonWriteKeyStrId(json, STRID5("stanza", 0x3a706930));
        jsonWriteObjectBegin(json);

        for (unsigned int stanzaIdx = 0; stanzaIdx < strLstSize(stanzaList); stanzaIdx++)
        {
            const String *const stanza = strLstGet(stanzaList, stanzaIdx);
            jsonWriteKey(json, stanza);

            jsonWriteObjectBegin(json);
            jsonWriteObjectEnd(json);

        }

        jsonWriteObjectEnd(json);
    }
    MEM_CONTEXT_TEMP_END();

    // StringList *const result = strLstNew();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static String *
cmdSupportRender(const unsigned int argListSize, const char *argList[])
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, argListSize);
        FUNCTION_TEST_PARAM_P(VOID, argList);
    FUNCTION_TEST_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const json = jsonWriteNewP(.json = result);

        // Replace support command with check command
        StringList *const argListUpdate = strLstNew();

        for (unsigned int argListIdx = 0; argListIdx < argListSize; argListIdx++)
        {
            if (strcmp(argList[argListIdx], CFGCMD_SUPPORT) == 0)
                strLstAddZ(argListUpdate, CFGCMD_CHECK);
            else
                strLstAddZ(argListUpdate, argList[argListIdx]);
        }

        // Render support info
        jsonWriteObjectBegin(json);
        cmdSupportRenderConfig(json);
        cmdSupportRenderStanza(json, argListSize, strLstPtr(argListUpdate));
        jsonWriteObjectEnd(json);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdSupport(const unsigned int argListSize, const char *argList[])
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, argListSize);
        FUNCTION_LOG_PARAM_P(VOID, argList);
    FUNCTION_LOG_END();

    strFree(cmdSupportRender(argListSize, argList));

    FUNCTION_LOG_RETURN_VOID();
}
