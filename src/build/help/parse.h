/***********************************************************************************************************************************
Parse Help Xml
***********************************************************************************************************************************/
#ifndef BUILD_HELP_PARSE_H
#define BUILD_HELP_PARSE_H

#include "build/common/xml.h"
#include "build/config/parse.h"

/***********************************************************************************************************************************
Types
***********************************************************************************************************************************/
typedef struct BldHlpSection
{
    const String *id;                                               // Id
    const String *name;                                             // Name
    const XmlNode *introduction;                                    // Introduction
} BldHlpSection;

typedef struct BldHlpCommand
{
    const String *name;                                             // Name
    const String *title;                                            // Title
    const XmlNode *summary;                                         // Summary
    const XmlNode *description;                                     // Description
    const List *optList;                                            // Option list
} BldHlpCommand;

typedef struct BldHlpOption
{
    const String *name;                                             // Name
    const String *section;                                          // Section
    const String *title;                                            // Title
    const XmlNode *summary;                                         // Summary
    const XmlNode *description;                                     // Description
    const StringList *exampleList;                                  // Examples
} BldHlpOption;

typedef struct BldHlp
{
    const List *sctList;                                            // Section list

    const String *cmdTitle;                                         // Command title
    const String *cmdDescription;                                   // Command description
    const XmlNode *cmdIntroduction;                                 // Command introduction
    const List *cmdList;                                            // Command list

    const String *optTitle;                                         // Option title
    const String *optDescription;                                   // Option description
    const XmlNode *optIntroduction;                                 // Option introduction
    const List *optList;                                            // Option list
} BldHlp;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse help.xml
BldHlp bldHlpParse(const Storage *const storageRepo, const BldCfg bldCfg, bool detail);

#endif
