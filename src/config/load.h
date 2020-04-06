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
// Load the configuration
void cfgLoad(unsigned int argListSize, const char *argList[]);

// Attempt to set the log file and turn file logging off if the file cannot be opened
void cfgLoadLogFile(void);

// Update options that have complex rules
void cfgLoadUpdateOption(void);

#endif
