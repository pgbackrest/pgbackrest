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
Constructor
***********************************************************************************************************************************/
Ini *iniNew(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Ini *iniMove(Ini *this, MemContext *parentNew);
void iniParse(Ini *this, const String *content);
void iniSet(Ini *this, const String *section, const String *key, const String *value);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const String *iniGet(const Ini *this, const String *section, const String *key);
const String *iniGetDefault(const Ini *this, const String *section, const String *key, const String *defaultValue);
StringList *iniGetList(const Ini *this, const String *section, const String *key);
bool iniSectionKeyIsList(const Ini *this, const String *section, const String *key);
StringList *iniSectionKeyList(const Ini *this, const String *section);
StringList *iniSectionList(const Ini *this);
String *iniFileName(const Ini *this);
bool iniFileExists(const Ini *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void iniFree(Ini *this);

/***********************************************************************************************************************************
Helper Functions
***********************************************************************************************************************************/
void iniLoad(
    IoRead *read, void (*callbackFunction)(void *data, const String *section, const String *key, const String *value),
    void *callbackData);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INI_TYPE                                                                                                      \
    Ini *
#define FUNCTION_LOG_INI_FORMAT(value, buffer, bufferSize)                                                                         \
    objToLog(value, "Ini", buffer, bufferSize)

#endif
