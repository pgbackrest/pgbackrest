/***********************************************************************************************************************************
Configuration Load
***********************************************************************************************************************************/
#ifndef CONFIG_LOAD_H
#define CONFIG_LOAD_H

#include <sys/types.h>

#include "config/config.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Load the configuration
void cfgLoad(unsigned int argListSize, const char *argList[]);

// Generate log file path and name. Only the command role is configurable here because log settings may vary between commands.
String *cfgLoadLogFileName(ConfigCommandRole commandRole);

// Attempt to set the log file and turn file logging off if the file cannot be opened
void cfgLoadLogFile(void);

// Update options that have complex rules
void cfgLoadUpdateOption(void);

#endif
