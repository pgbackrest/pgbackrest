/***********************************************************************************************************************************
Ini Handler
***********************************************************************************************************************************/
#ifndef COMMON_INI_H
#define COMMON_INI_H

/***********************************************************************************************************************************
Ini object
***********************************************************************************************************************************/
typedef struct Ini Ini;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Ini value
***********************************************************************************************************************************/
typedef struct IniValue
{
    String *section;
    String *key;
    String *value;
} IniValue;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Ini *iniNew(void);
Ini *iniNewIo(IoRead *read);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
FN_INLINE_ALWAYS Ini *
iniMove(Ini *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Parse ini from a string. Comments are ignored and additional whitespace around sections, keys, and values is trimmed. Should be
// used *only* to read user-generated config files, for code-generated info files see iniLoad().
void iniParse(Ini *this, const String *content);

// Get the next value in the ini file. Note that members of IniValue are reused between calls so the members of a previous call may
// change after the next call. Any members that need to be preserved should be copied before the next call.
const IniValue *iniValueNext(Ini *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Get an ini value -- error if it does not exist
const String *iniGet(const Ini *this, const String *section, const String *key);

// Ini key list
StringList *iniGetList(const Ini *this, const String *section, const String *key);

// The key's value is a list
bool iniSectionKeyIsList(const Ini *this, const String *section, const String *key);

// List of keys for a section
StringList *iniSectionKeyList(const Ini *this, const String *section);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
iniFree(Ini *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INI_TYPE                                                                                                      \
    Ini *
#define FUNCTION_LOG_INI_FORMAT(value, buffer, bufferSize)                                                                         \
    objToLog(value, "Ini", buffer, bufferSize)
#define FUNCTION_LOG_INI_VALUE_TYPE                                                                                                \
    IniValue *
#define FUNCTION_LOG_INI_VALUE_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(value, "IniValue", buffer, bufferSize)

#endif
