/***********************************************************************************************************************************
Build Command and Configuration Reference
***********************************************************************************************************************************/
#ifndef DOC_COMMAND_BUILD_REFERENCE_H
#define DOC_COMMAND_BUILD_REFERENCE_H

#include "build/config/parse.h"
#include "build/help/parse.h"
#include "common/type/xml.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Build command reference
XmlDocument *referenceCommandRender(const BldCfg *bldCfg, const BldHlp *bldHlp);

// Build configuration reference
XmlDocument *referenceConfigurationRender(const BldCfg *bldCfg, const BldHlp *bldHlp);

#endif
