/***********************************************************************************************************************************
Build Manual Page Reference
***********************************************************************************************************************************/
#ifndef DOC_COMMAND_BUILD_MAN_H
#define DOC_COMMAND_BUILD_MAN_H

#include "build/config/parse.h"
#include "build/help/parse.h"
#include "common/type/xml.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Build manual page reference
String *referenceManRender(const XmlNode *indexRoot, const BldCfg *bldCfg, const BldHlp *bldHlp);

#endif
