/***********************************************************************************************************************************
Info Handler for pgbackrest information
***********************************************************************************************************************************/
#ifndef INFO_INFO_H
#define INFO_INFO_H

/***********************************************************************************************************************************
Info object
***********************************************************************************************************************************/
typedef struct Info Info;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Ini *infoNew();
// const Variant *iniGet(const Ini *this, const String *section, const String *key);
// const Variant *iniGetDefault(const Ini *this, const String *section, const String *key, Variant *defaultValue);
// StringList *iniSectionKeyList(const Ini *this, const String *section);
// void iniParse(Ini *this, const String *content);
// void iniLoad(Ini *this, const String *fileName);
// void iniSet(Ini *this, const String *section, const String *key, const Variant *value);

void infoFree(Info *this);

#endif
