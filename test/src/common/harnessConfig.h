/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "config/config.h"

#define CFGOPT_PG1_PATH                                             "pg1-path"
#define CFGOPT_PG2_HOST                                             "pg2-host"
#define CFGOPT_PG2_LOCAL                                            "pg2-local"
#define CFGOPT_PG2_PATH                                             "pg2-path"
#define CFGOPT_REPO1_CIPHER_PASS                                    "repo1-cipher-pass"
#define CFGOPT_REPO1_CIPHER_TYPE                                    "repo1-cipher-type"
#define CFGOPT_REPO1_PATH                                           "repo1-path"
#define CFGOPT_REPO1_HOST                                           "repo1-host"
#define CFGOPT_REPO1_TYPE                                           "repo1-type"

#define HRN_PGBACKREST_ENV                                          "PGBACKREST_"

/***********************************************************************************************************************************
Load a test configuration without any side effects

There's no need to open log files, acquire locks, reset log levels, etc.
***********************************************************************************************************************************/
// Low-level version used in a few places to simulate special cases and error conditions.  Most tests should use harnessCfgLoad().
void harnessCfgLoadRaw(unsigned int argListSize, const char *argList[]);

// Automatically adds the exe, command (and role), lock-path, and log-path so executing the binary works locally or in a container.
void harnessCfgLoad(ConfigCommand commandId, const StringList *argList);
void harnessCfgLoadRole(ConfigCommand commandId, ConfigCommandRole commandRoleId, const StringList *argList);
