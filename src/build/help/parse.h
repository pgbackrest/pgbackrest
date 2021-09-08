/***********************************************************************************************************************************
Parse Help Xml
***********************************************************************************************************************************/
#ifndef BUILD_HELP_PARSE_H
#define BUILD_HELP_PARSE_H

#include "build/config/parse.h"
#include "common/type/xml.h"

/***********************************************************************************************************************************
Types
***********************************************************************************************************************************/
typedef struct BldHlpCommand
{
    const String *name;                                             // Name
    const XmlNode *summary;                                         // Summary
    const XmlNode *description;                                     // Description
    const List *optList;                                            // Option list
} BldHlpCommand;

typedef struct BldHlpOption
{
    const String *name;                                             // Name
    const String *section;                                          // Section
    const XmlNode *summary;                                         // Summary
    const XmlNode *description;                                     // Description
} BldHlpOption;

typedef struct BldHlp
{
    const List *cmdList;                                            // Command list
    const List *optList;                                            // Option list
} BldHlp;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse help.xml
BldHlp bldHlpParse(const Storage *const storageRepo, const BldCfg bldCfg);

#endif
