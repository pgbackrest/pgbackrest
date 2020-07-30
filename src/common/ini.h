/***********************************************************************************************************************************
Ini Handler
***********************************************************************************************************************************/
#ifndef COMMON_INI_H
#define COMMON_INI_H

/***********************************************************************************************************************************
Ini object
***********************************************************************************************************************************/
#define INI_TYPE                                                    Ini
#define INI_PREFIX                                                  ini

typedef struct Ini Ini;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Ini *iniNew(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
Ini *iniMove(Ini *this, MemContext *parentNew);

// Parse ini from a string
void iniParse(Ini *this, const String *content);

// Set an ini value
void iniSet(Ini *this, const String *section, const String *key, const String *value);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get an ini value -- error if it does not exist
const String *iniGet(const Ini *this, const String *section, const String *key);

// Get an ini value -- if it does not exist then return specified default
const String *iniGetDefault(const Ini *this, const String *section, const String *key, const String *defaultValue);

// Ini key list
StringList *iniGetList(const Ini *this, const String *section, const String *key);

// The key's value is a list
bool iniSectionKeyIsList(const Ini *this, const String *section, const String *key);

// List of keys for a section
StringList *iniSectionKeyList(const Ini *this, const String *section);

// List of sections
StringList *iniSectionList(const Ini *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void iniFree(Ini *this);

/***********************************************************************************************************************************
Helper Functions
***********************************************************************************************************************************/
// Load an ini file and return data to a callback
void iniLoad(
    IoRead *read,
    void (*callbackFunction)(void *data, const String *section, const String *key, const String *value, const Variant *valueVar),
    void *callbackData);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INI_TYPE                                                                                                      \
    Ini *
#define FUNCTION_LOG_INI_FORMAT(value, buffer, bufferSize)                                                                         \
    objToLog(value, "Ini", buffer, bufferSize)

#endif
