/***********************************************************************************************************************************
Render Help
***********************************************************************************************************************************/
#ifndef BUILD_HELP_RENDER_H
#define BUILD_HELP_RENDER_H

#include "build/config/parse.h"
#include "build/help/parse.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Render help
void bldHlpRender(const Storage *const storageRepo, const BldCfg bldCfg, const BldHlp bldHlp);

// Render xml as text
String *bldHlpRenderXml(const XmlNode *xml);

#endif
