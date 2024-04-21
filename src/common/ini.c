/***********************************************************************************************************************************
Ini Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Ini
{
    KeyValue *store;                                                // Key value store that contains the ini data
    IoRead *read;                                                   // Read object for ini data
    IniValue value;                                                 // Current value
    unsigned int lineIdx;                                           // Current line used for error reporting
    bool strict;                                                    // Expect all values to be JSON and do not trim
};

/**********************************************************************************************************************************/
FN_EXTERN Ini *
iniNew(IoRead *const read, const IniNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(BOOL, param.strict);
        FUNCTION_LOG_PARAM(BOOL, param.store);
    FUNCTION_LOG_END();

    OBJ_NEW_BEGIN(Ini, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (Ini)
        {
            .read = read,
            .value =
            {
                .section = strNew(),
                .key = strNew(),
                .value = strNew(),
            },
            .strict = param.strict,
        };

        ioReadOpen(this->read);

        // Build KeyValue store
        if (param.store)
        {
            this->store = kvNew();

            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                const IniValue *value = iniValueNext(this);

                // Add values while not done
                while (value != NULL)
                {
                    const Variant *const sectionKey = VARSTR(value->section);
                    KeyValue *sectionKv = varKv(kvGet(this->store, sectionKey));

                    if (sectionKv == NULL)
                        sectionKv = kvPutKv(this->store, sectionKey);

                    kvAdd(sectionKv, VARSTR(value->key), VARSTR(value->value));

                    MEM_CONTEXT_TEMP_RESET(1000);
                    value = iniValueNext(this);
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INI, this);
}

/**********************************************************************************************************************************/
FN_EXTERN const IniValue *
iniValueNext(Ini *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const IniValue *result = NULL;

    MEM_CONTEXT_TEMP_RESET_BEGIN()
    {
        // Read until a value is found or eof
        while (result == NULL && !ioReadEof(this->read))
        {
            // Read the line and trim if needed
            String *const line = ioReadLineParam(this->read, true);

            if (!this->strict)
                strTrim(line);

            const char *const linePtr = strZ(line);
            this->lineIdx++;

            // Only interested in lines that are not blank or comments
            if (strSize(line) > 0 && (this->strict || linePtr[0] != '#'))
            {
                // The line is a section. Since the value must be valid JSON this means that the value must never be an array.
                if (linePtr[0] == '[' && (!this->strict || linePtr[strSize(line) - 1] == ']'))
                {
                    // Make sure the section ends with ]
                    if (!this->strict && linePtr[strSize(line) - 1] != ']')
                        THROW_FMT(FormatError, "ini section should end with ] at line %u: %s", this->lineIdx, linePtr);

                    // Store the section
                    strCatZN(strTrunc(this->value.section), linePtr + 1, strSize(line) - 2);

                    if (strEmpty(this->value.section))
                        THROW_FMT(FormatError, "invalid empty section at line %u: %s", this->lineIdx, linePtr);
                }
                // Else it is a key/value
                else
                {
                    if (strEmpty(this->value.section))
                        THROW_FMT(FormatError, "key/value found outside of section at line %u: %s", this->lineIdx, linePtr);

                    // Find the =
                    const char *lineEqual = strstr(linePtr, "=");

                    if (lineEqual == NULL)
                        THROW_FMT(FormatError, "missing '=' in key/value at line %u: %s", this->lineIdx, linePtr);

                    // Extract the key/value. For strict this may require some retries if the key includes an = character since this
                    // is also the separator. We know the value must be valid JSON so if it isn't then add the characters up to the
                    // next = to the key and try to parse the value as JSON again. If the value never becomes valid JSON then an
                    // error is thrown.
                    bool retry;

                    do
                    {
                        retry = false;

                        // Get key/value
                        strCatZN(strTrunc(this->value.key), linePtr, (size_t)(lineEqual - linePtr));
                        strCatZ(strTrunc(this->value.value), lineEqual + 1);

                        // Value is expected to be valid JSON
                        if (this->strict)
                        {
                            TRY_BEGIN()
                            {
                                jsonValidate(this->value.value);
                            }
                            CATCH(JsonFormatError)
                            {
                                // If value is not valid JSON look for another =. If not found then nothing to retry.
                                lineEqual = strstr(lineEqual + 1, "=");

                                if (lineEqual == NULL)
                                {
                                    THROW_FMT(
                                        FormatError, "invalid JSON value at line %u '%s': %s", this->lineIdx, linePtr,
                                        errorMessage());
                                }

                                // Try again with = in new position
                                retry = true;
                            }
                            TRY_END();
                        }
                        // Else just trim
                        else
                        {
                            strTrim(this->value.key);
                            strTrim(this->value.value);
                        }
                    }
                    while (retry);

                    // Key may not be zero-length
                    if (strSize(this->value.key) == 0)
                        THROW_FMT(FormatError, "key is zero-length at line %u: %s", this->lineIdx, linePtr);

                    // A value was found so return it
                    result = &this->value;
                }
            }

            MEM_CONTEXT_TEMP_RESET(1000);
        }

        // If no more values found then close
        if (result == NULL)
            ioReadClose(this->read);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_CONST(INI_VALUE, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
iniValid(Ini *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
    FUNCTION_TEST_END();

    while (iniValueNext(this) != NULL);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Internal function to get an ini value
***********************************************************************************************************************************/
static const Variant *
iniGetInternal(const Ini *const this, const String *const section, const String *const key, const bool required)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(BOOL, required);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->store != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);

    const Variant *result = NULL;

    // Get the section
    const KeyValue *const sectionKv = varKv(kvGet(this->store, VARSTR(section)));

    // Section must exist to get the value
    if (sectionKv != NULL)
        result = kvGet(sectionKv, VARSTR(key));

    // If value is null and required then error
    if (result == NULL && required)
        THROW_FMT(FormatError, "section '%s', key '%s' does not exist", strZ(section), strZ(key));

    FUNCTION_TEST_RETURN_CONST(VARIANT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
iniGet(const Ini *const this, const String *const section, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, varStr(iniGetInternal(this, section, key, true)));
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
iniGetList(const Ini *const this, const String *const section, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    // Get the value
    const Variant *const result = iniGetInternal(this, section, key, false);

    FUNCTION_TEST_RETURN(STRING_LIST, result == NULL ? NULL : strLstNewVarLst(varVarLst(result)));
}

/**********************************************************************************************************************************/
FN_EXTERN bool
iniSectionKeyIsList(const Ini *const this, const String *const section, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    // Get the value
    const Variant *const result = iniGetInternal(this, section, key, true);

    FUNCTION_TEST_RETURN(BOOL, varType(result) == varTypeVariantList);
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
iniSectionList(const Ini *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->store != NULL);

    FUNCTION_TEST_RETURN(STRING_LIST, strLstNewVarLst(kvKeyList(this->store)));
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
iniSectionKeyList(const Ini *const this, const String *const section)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, this);
        FUNCTION_TEST_PARAM(STRING, section);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->store != NULL);
    ASSERT(section != NULL);

    StringList *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the section
        const KeyValue *const sectionKv = varKv(kvGet(this->store, VARSTR(section)));

        // Return key list if the section exists
        if (sectionKv != NULL)
        {
            result = strLstNewVarLst(kvKeyList(sectionKv));
        }
        // Otherwise return an empty list
        else
            result = strLstNew();

        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}
