/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "config/config.h"

/***********************************************************************************************************************************
Prefix for environment variables
***********************************************************************************************************************************/
#define HRN_PGBACKREST_ENV                                          "PGBACKREST_"

/***********************************************************************************************************************************
Load a test configuration without any side effects

There's no need to open log files, acquire locks, reset log levels, etc.
***********************************************************************************************************************************/
// Low-level version used when it is important that only the options set explicity by the test are present in the config. The
// additional options set by harnessCfgLoad() can make these tests harder to write. Most tests should use harnessCfgLoad().
void harnessCfgLoadRaw(unsigned int argListSize, const char *argList[]);

// Automatically adds the exe, command (and role), lock-path, and log-path so executing the binary works locally or in a container.
void harnessCfgLoad(ConfigCommand commandId, const StringList *argList);
void harnessCfgLoadRole(ConfigCommand commandId, ConfigCommandRole commandRoleId, const StringList *argList);

/***********************************************************************************************************************************
Configuration helper functions

These functions set options in the argument list using the option IDs rather than string constants. Each function has a "Key"
variant that works with indexed options and allows the key to be specified, e.g. hrnCfgArgKeyRawZ(cfgOptPgpath, 3, "/pg") will add
--pg3-path=/pg to the argument list.
***********************************************************************************************************************************/
void hrnCfgArgRaw(StringList *argList, ConfigOption optionId, const String *value);
void hrnCfgArgKeyRaw(StringList *argList, ConfigOption optionId, unsigned optionKey, const String *value);

void hrnCfgArgRawFmt(StringList *argList, ConfigOption optionId, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
void hrnCfgArgKeyRawFmt(StringList *argList, ConfigOption optionId, unsigned optionKey, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

void hrnCfgArgRawZ(StringList *argList, ConfigOption optionId, const char *value);
void hrnCfgArgKeyRawZ(StringList *argList, ConfigOption optionId, unsigned optionKey, const char *value);

void hrnCfgArgRawBool(StringList *argList, ConfigOption optionId, bool value);
void hrnCfgArgKeyRawBool(StringList *argList, ConfigOption optionId, unsigned optionKey, bool value);

void hrnCfgArgRawNegate(StringList *argList, ConfigOption optionId);
void hrnCfgArgKeyRawNegate(StringList *argList, ConfigOption optionId, unsigned optionKey);

void hrnCfgArgRawReset(StringList *argList, ConfigOption optionId);
void hrnCfgArgKeyRawReset(StringList *argList, ConfigOption optionId, unsigned optionKey);

/***********************************************************************************************************************************
Environment helper functions

Add and remove environment options, which are required to pass secrets, e.g. repo1-cipher-pass.
***********************************************************************************************************************************/
void hrnCfgEnvRaw(ConfigOption optionId, const String *value);
void hrnCfgEnvIdRaw(ConfigOption optionId, unsigned optionKey, const String *value);

void hrnCfgEnvRawZ(ConfigOption optionId, const char *value);
void hrnCfgEnvIdRawZ(ConfigOption optionId, unsigned optionKey, const char *value);

void hrnCfgEnvRemoveRaw(ConfigOption optionId);
void hrnCfgEnvIdRemoveRaw(ConfigOption optionId, unsigned optionKey);
