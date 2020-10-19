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
// Low-level version used in a few places to simulate special cases and error conditions.  Most tests should use harnessCfgLoad().
void harnessCfgLoadRaw(unsigned int argListSize, const char *argList[]);

// Automatically adds the exe, command (and role), lock-path, and log-path so executing the binary works locally or in a container.
void harnessCfgLoad(ConfigCommand commandId, const StringList *argList);
void harnessCfgLoadRole(ConfigCommand commandId, ConfigCommandRole commandRoleId, const StringList *argList);

// Add options to a raw argument list
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

void hrnCfgArgRawReset(StringList *argList, ConfigOption optionId);
void hrnCfgArgKeyRawReset(StringList *argList, ConfigOption optionId, unsigned optionKey);

// Set environment options
void hrnCfgEnvRaw(ConfigOption optionId, const String *value);
void hrnCfgEnvIdRaw(ConfigOption optionId, unsigned optionKey, const String *value);

void hrnCfgEnvRawZ(ConfigOption optionId, const char *value);
void hrnCfgEnvIdRawZ(ConfigOption optionId, unsigned optionKey, const char *value);

void hrnCfgEnvRemoveRaw(ConfigOption optionId);
void hrnCfgEnvIdRemoveRaw(ConfigOption optionId, unsigned optionKey);
