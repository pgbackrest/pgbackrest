/***********************************************************************************************************************************
Ini Handler
***********************************************************************************************************************************/
#ifndef COMMON_INI_H
#define COMMON_INI_H

/***********************************************************************************************************************************
Ini object
***********************************************************************************************************************************/
typedef struct Ini Ini;

#include "common/type/variant.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Ini *iniNew();
const Variant *iniGet(const Ini *this, const String *section, const String *key);
const Variant *iniGetDefault(const Ini *this, const String *section, const String *key, Variant *defaultValue);
StringList *iniSectionKeyList(const Ini *this, const String *section);
void iniParse(Ini *this, const String *content);
void iniLoad(Ini *this, const String *fileName);
void iniSet(Ini *this, const String *section, const String *key, const Variant *value);

void iniFree(Ini *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_INI_TYPE                                                                                                    \
    Ini *
#define FUNCTION_DEBUG_INI_FORMAT(value, buffer, bufferSize)                                                                       \
    objToLog(value, "Ini", buffer, bufferSize)

#endif
