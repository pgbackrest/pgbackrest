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
Ini section/key/value
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
typedef struct IniNewParam
{
    VAR_PARAM_HEADER;
    bool strict;                                                    // Expect all values to be JSON and do not trim
    bool store;                                                     // Store in KeyValue so functions can be used to query
} IniNewParam;

#define iniNewP(read, ...)                                                                                                         \
    iniNew(read, (IniNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN Ini *iniNew(IoRead *read, IniNewParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
FN_INLINE_ALWAYS Ini *
iniMove(Ini *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Check that the ini is valid
FN_EXTERN void iniValid(Ini *this);

// Get the next value in the ini file. Note that members of IniValue are reused between calls so the members of a previous call may
// change after the next call. Any members that need to be preserved should be copied before the next call.
FN_EXTERN const IniValue *iniValueNext(Ini *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Get an ini value -- error if it does not exist
FN_EXTERN const String *iniGet(const Ini *this, const String *section, const String *key);

// Ini key list
FN_EXTERN StringList *iniGetList(const Ini *this, const String *section, const String *key);

// The key's value is a list
FN_EXTERN bool iniSectionKeyIsList(const Ini *this, const String *section, const String *key);

// List of sections
FN_EXTERN StringList *iniSectionList(const Ini *const this);

// List of keys for a section
FN_EXTERN StringList *iniSectionKeyList(const Ini *this, const String *section);

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
    objNameToLog(value, "Ini", buffer, bufferSize)
#define FUNCTION_LOG_INI_VALUE_TYPE                                                                                                \
    IniValue *
#define FUNCTION_LOG_INI_VALUE_FORMAT(value, buffer, bufferSize)                                                                   \
    objNameToLog(value, "IniValue", buffer, bufferSize)

#endif
