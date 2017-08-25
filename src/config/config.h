#ifndef CONFIG_H
#define CONFIG_H

#include "common/type.h"
#include "config/config.auto.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
const char *cfgCommandName(uint32 uiCommandId);
int32 cfgOptionIndexTotal(uint32 uiOptionId);
const char *cfgOptionName(uint32 uiCommandId);

#endif
