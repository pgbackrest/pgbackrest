/***********************************************************************************************************************************
Ini Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/ini.h"
#include "common/type/keyValue.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Contains information about the ini
***********************************************************************************************************************************/
struct Ini
{
    MemContext *memContext;                                         // Context that contains the ini
    KeyValue *store;                                                // Key value store that contains the ini data
    String *fileName;                                               // File name (if one has been set)
};

/***********************************************************************************************************************************
Create a new string from a zero-terminated string
***********************************************************************************************************************************/
Ini *
iniNew()
{
    Ini *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ini")
    {
        // Create object
        this = memNew(sizeof(Ini));
        this->memContext = MEM_CONTEXT_NEW();

        // Allocate key value store
        this->store = kvNew();
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Internal function to get an ini value
***********************************************************************************************************************************/
static const Variant *
iniGetInternal(const Ini *this, const String *section, const String *key)
{
    const Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the section
        KeyValue *sectionKv = varKv(kvGet(this->store, varNewStr(section)));

        // Section must exist to get the value
        if (sectionKv != NULL)
            result = kvGet(sectionKv, varNewStr(key));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Get an ini value -- error if it does not exist
***********************************************************************************************************************************/
const Variant *
iniGet(const Ini *this, const String *section, const String *key)
{
    // Get the value
    const Variant *result = iniGetInternal(this, section, key);

    // If value is null replace it with default
    if (result == NULL)
        THROW_FMT(FormatError, "section '%s', key '%s' does not exist", strPtr(section), strPtr(key));

    return result;
}

/***********************************************************************************************************************************
Get an ini value -- if it does not exist then return specified default
***********************************************************************************************************************************/
const Variant *
iniGetDefault(const Ini *this, const String *section, const String *key, Variant *defaultValue)
{
    // Get the value
    const Variant *result = iniGetInternal(this, section, key);

    // If value is null replace it with default
    if (result == NULL)
        result = defaultValue;

    return result;
}

/***********************************************************************************************************************************
Get a list of keys for a section
***********************************************************************************************************************************/
StringList *
iniSectionKeyList(const Ini *this, const String *section)
{
    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the section
        KeyValue *sectionKv = varKv(kvGet(this->store, varNewStr(section)));

        // Return key list of the section exists
        if (sectionKv != NULL)
            result = strLstNewVarLst(kvKeyList(sectionKv));
        // Otherwise return an empty list
        else
            result = strLstNew();

        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Parse ini from a string
***********************************************************************************************************************************/
void
iniParse(Ini *this, const String *content)
{
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        if (this->store != NULL)
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
                    const char *linePtr = strPtr(line);

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
                            section = strNewN(linePtr + 1, strSize(line) - 2);
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
                            String *key = strTrim(strNewN(linePtr, (size_t)(lineEqual - linePtr)));

                            if (strSize(key) == 0)
                                THROW_FMT(FormatError, "key is zero-length at line %u: %s", lineIdx++, linePtr);

                            // Extract the value
                            Variant *value = varNewStr(strTrim(strNew(lineEqual + 1)));

                            // Store the section/key/value
                            iniSet(this, section, key, value);
                        }
                    }
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    MEM_CONTEXT_END()
}

/***********************************************************************************************************************************
Load ini from a file
***********************************************************************************************************************************/
void
iniLoad(Ini *this, const String *fileName)
{
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Set the filename
        this->fileName = strDup(fileName);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            iniParse(this, strNewBuf(storageGetNP(storageNewReadNP(storageLocal(), this->fileName))));
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_END()
}

/***********************************************************************************************************************************
Set an ini value
***********************************************************************************************************************************/
void
iniSet(Ini *this, const String *section, const String *key, const Variant *value)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        Variant *sectionKey = varNewStr(section);
        KeyValue *sectionKv = varKv(kvGet(this->store, sectionKey));

        if (sectionKv == NULL)
            sectionKv = kvPutKv(this->store, sectionKey);

        kvAdd(sectionKv, varNewStr(key), value);
    }
    MEM_CONTEXT_TEMP_END();
}

/***********************************************************************************************************************************
Free the ini
***********************************************************************************************************************************/
void
iniFree(Ini *this)
{
    memContextFree(this->memContext);
}
