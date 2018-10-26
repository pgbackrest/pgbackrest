/***********************************************************************************************************************************
Configuration Load
***********************************************************************************************************************************/
#ifndef CONFIG_LOAD_H
#define CONFIG_LOAD_H

#include <sys/types.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cfgLoad(unsigned int argListSize, const char *argList[]);
void cfgLoadLogSetting(void);
void cfgLoadLogFile(const String *logFile);
void cfgLoadUpdateOption(void);

#endif
