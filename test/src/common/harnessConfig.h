/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "config/config.h"

/***********************************************************************************************************************************
Load a test configuration without any side effects

There's no need to open log files, acquire locks, reset log levels, etc.
***********************************************************************************************************************************/
// Low-level version used in a few places to simulate special cases and error conditions.  Most tests should use harnessCfgLoad().
void harnessCfgLoadRaw(unsigned int argListSize, const char *argList[]);

// Automatically adds the exe, command (and role), lock-path, and log-path so executing the binary works locally or in a container.
void harnessCfgLoad(ConfigCommand commandId, const StringList *argList);
void harnessCfgLoadRole(ConfigCommand commandId, ConfigCommandRole commandRoleId, const StringList *argList);
