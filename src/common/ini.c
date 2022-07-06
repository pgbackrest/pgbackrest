/***********************************************************************************************************************************
Ini Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/ini.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Ini
{
    KeyValue *store;                                                // Key value store that contains the ini data
};

/**********************************************************************************************************************************/
Ini *
iniNew(void)
{
    FUNCTION_TEST_VOID();

    Ini *this = NULL;

    OBJ_NEW_BEGIN(Ini, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        this = OBJ_NEW_ALLOC();

        *this = (Ini)
        {
            .store = kvNew(),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(INI, this);
}

/***********************************************************************************************************************************
Internal function to get an ini value
***********************************************************************************************************************************/
static const Variant *
iniGetInternal(const Ini *this, const String *section, const String *key, bool required)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(BOOL, required);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);

    const Variant *result = NULL;

    // Get the section
    KeyValue *sectionKv = varKv(kvGet(this->store, VARSTR(section)));

    // Section must exist to get the value
    if (sectionKv != NULL)
        result = kvGet(sectionKv, VARSTR(key));

    // If value is null and required then error
    if (result == NULL && required)
        THROW_FMT(FormatError, "section '%s', key '%s' does not exist", strZ(section), strZ(key));

    FUNCTION_TEST_RETURN_CONST(VARIANT, result);
}

/**********************************************************************************************************************************/
const String *
iniGet(const Ini *this, const String *section, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, varStr(iniGetInternal(this, section, key, true)));
}

/**********************************************************************************************************************************/
const String *
iniGetDefault(const Ini *this, const String *section, const String *key, const String *defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, defaultValue);
    FUNCTION_TEST_END();

    // Get the value
    const Variant *result = iniGetInternal(this, section, key, false);

    FUNCTION_TEST_RETURN_CONST(STRING, result == NULL ? defaultValue : varStr(result));
}

/**********************************************************************************************************************************/
StringList *
iniGetList(const Ini *this, const String *section, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    // Get the value
    const Variant *result = iniGetInternal(this, section, key, false);

    FUNCTION_TEST_RETURN(STRING_LIST, result == NULL ? NULL : strLstNewVarLst(varVarLst(result)));
}

/**********************************************************************************************************************************/
bool
iniSectionKeyIsList(const Ini *this, const String *section, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    // Get the value
    const Variant *result = iniGetInternal(this, section, key, true);

    FUNCTION_TEST_RETURN(BOOL, varType(result) == varTypeVariantList);
}

/**********************************************************************************************************************************/
StringList *
iniSectionKeyList(const Ini *this, const String *section)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(section != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the section
        KeyValue *sectionKv = varKv(kvGet(this->store, VARSTR(section)));

        // Return key list if the section exists
        if (sectionKv != NULL)
            result = strLstNewVarLst(kvKeyList(sectionKv));
        // Otherwise return an empty list
        else
            result = strLstNew();

        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
StringList *
iniSectionList(const Ini *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the sections from the keyList
        result = strLstNewVarLst(kvKeyList(this->store));

        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
void
iniParse(Ini *this, const String *content)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, content);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        kvFree(this->store);
        this->store = kvNew();

        if (content != NULL)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Track the current section
                String *section = NULL;

                // Split the content into lines and loop
                StringList *lines = strLstNewSplitZ(content, "\n");

                for (unsigned int lineIdx = 0; lineIdx < strLstSize(lines); lineIdx++)
                {
                    // Get next line
                    const String *line = strTrim(strLstGet(lines, lineIdx));
                    const char *linePtr = strZ(line);

                    // Only interested in lines that are not blank or comments
                    if (strSize(line) > 0 && linePtr[0] != '#')
                    {
                        // Looks like this line is a section
                        if (linePtr[0] == '[')
                        {
                            // Make sure the section ends with ]
                            if (linePtr[strSize(line) - 1] != ']')
                                THROW_FMT(FormatError, "ini section should end with ] at line %u: %s", lineIdx + 1, linePtr);

                            // Assign section
                            section = strNewZN(linePtr + 1, strSize(line) - 2);
                        }
                        // Else it should be a key/value
                        else
                        {
                            if (section == NULL)
                                THROW_FMT(FormatError, "key/value found outside of section at line %u: %s", lineIdx + 1, linePtr);

                            // Find the =
                            const char *lineEqual = strstr(linePtr, "=");

                            if (lineEqual == NULL)
                                THROW_FMT(FormatError, "missing '=' in key/value at line %u: %s", lineIdx + 1, linePtr);

                            // Extract the key
                            String *key = strTrim(strNewZN(linePtr, (size_t)(lineEqual - linePtr)));

                            if (strSize(key) == 0)
                                THROW_FMT(FormatError, "key is zero-length at line %u: %s", lineIdx++, linePtr);

                            // Store the section/key/value
                            iniSet(this, section, key, strTrim(strNewZ(lineEqual + 1)));
                        }
                    }
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
iniSet(Ini *this, const String *section, const String *key, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Variant *sectionKey = VARSTR(section);
        KeyValue *sectionKv = varKv(kvGet(this->store, sectionKey));

        if (sectionKv == NULL)
            sectionKv = kvPutKv(this->store, sectionKey);

        kvAdd(sectionKv, VARSTR(key), VARSTR(value));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
iniLoad(
    IoRead *const read, void (*callbackFunction)(void *data, const String *section, const String *key, const String *value),
    void *const callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Track the current section/key/value
        String *const section = strNew();
        String *const key = strNew();
        String *const value = strNew();

        // Keep track of the line number for error reporting
        unsigned int lineIdx = 0;

        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            ioReadOpen(read);

            do
            {
                const String *line = ioReadLineParam(read, true);
                const char *linePtr = strZ(line);

                // Only interested in lines that are not blank
                if (strSize(line) > 0)
                {
                    // The line is a section. Since the value must be valid JSON this means that the value must never be an array.
                    if (linePtr[0] == '[' && linePtr[strSize(line) - 1] == ']')
                    {
                        strCatZN(strTrunc(section), linePtr + 1, strSize(line) - 2);

                        if (strEmpty(section))
                            THROW_FMT(FormatError, "invalid empty section at line %u: %s", lineIdx + 1, linePtr);
                    }
                    // Else it is a key/value
                    else
                    {
                        if (strEmpty(section))
                            THROW_FMT(FormatError, "key/value found outside of section at line %u: %s", lineIdx + 1, linePtr);

                        // Find the =
                        const char *lineEqual = strstr(linePtr, "=");

                        if (lineEqual == NULL)
                            THROW_FMT(FormatError, "missing '=' in key/value at line %u: %s", lineIdx + 1, linePtr);

                        // Extract the key/value. This may require some retries if the key includes an = character since this is
                        // also the separator. We know the value must be valid JSON so if it isn't then add the characters up to
                        // the next = to the key and try to parse the value as JSON again. If the value never becomes valid JSON
                        // then an error is thrown.
                        bool retry;

                        do
                        {
                            retry = false;

                            // Get key/value
                            strCatZN(strTrunc(key), linePtr, (size_t)(lineEqual - linePtr));
                            strCatZ(strTrunc(value), lineEqual + 1);

                            // Check that the value is valid JSON
                            TRY_BEGIN()
                            {
                                jsonValidate(value);
                            }
                            CATCH(JsonFormatError)
                            {
                                // If value is not valid JSON look for another =. If not found then nothing to retry.
                                lineEqual = strstr(lineEqual + 1, "=");

                                if (lineEqual == NULL)
                                {
                                    THROW_FMT(
                                        FormatError, "invalid JSON value at line %u '%s': %s", lineIdx + 1, linePtr,
                                        errorMessage());
                                }

                                // Try again with = in new position
                                retry = true;
                            }
                            TRY_END();
                        }
                        while (retry);

                        // Key may not be zero-length
                        if (strSize(key) == 0)
                            THROW_FMT(FormatError, "key is zero-length at line %u: %s", lineIdx + 1, linePtr);

                        // Callback with the section/key/value
                        callbackFunction(callbackData, section, key, value);
                    }
                }

                lineIdx++;
                MEM_CONTEXT_TEMP_RESET(1000);
            }
            while (!ioReadEof(read));

            ioReadClose(read);
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
