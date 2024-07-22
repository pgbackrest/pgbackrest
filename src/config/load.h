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
FN_EXTERN void cfgLoad(unsigned int argListSize, const char *argList[]);

// Load the configuration using the specified stanza
FN_EXTERN void cfgLoadStanza(const String *stanza);

// Generate log file path and name. Only the command role is configurable here because log settings may vary between commands.
FN_EXTERN String *cfgLoadLogFileName(ConfigCommandRole commandRole);

// Attempt to set the log file and turn file logging off if the file cannot be opened
FN_EXTERN void cfgLoadLogFile(void);

// Update options that have complex rules
FN_EXTERN void cfgLoadUpdateOption(void);

#endif
