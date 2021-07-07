/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_CONFIG_H
#define TEST_COMMON_HARNESS_CONFIG_H

#include "config/config.h"

/***********************************************************************************************************************************
Prefix for environment variables
***********************************************************************************************************************************/
#define HRN_PGBACKREST_ENV                                          "PGBACKREST_"

/***********************************************************************************************************************************
Config option constants
***********************************************************************************************************************************/
#define TEST_CIPHER_PASS                                            "xmainx"
#define TEST_CIPHER_PASS_ARCHIVE                                    "xarchivex"

/***********************************************************************************************************************************
Load a test configuration

Automatically adds the exe, command (and role), lock-path, and log-path so executing the binary works locally or in a container.
There is no need to open log files, acquire locks, reset log levels, etc.

HRN_CFG_LOAD() is the preferred macro but if a configuration error is being tested with TEST_ERROR() then use hrnCfgLoadP() instead
since it will not log to the console and clutter the error log message.
***********************************************************************************************************************************/
typedef struct HrnCfgLoadParam
{
    VAR_PARAM_HEADER;
    ConfigCommandRole role;                                         // Command role (defaults to main)
    bool exeBogus;                                                  // Use pgbackrest-bogus as exe parameter
    bool noStd;                                                     // Do not add standard options, e.g. lock-path
    bool log;                                                       // Log parameters? (used internally by HRN_CFG_LOAD())
    const char *comment;                                            // Comment
} HrnCfgLoadParam;

#define hrnCfgLoadP(commandId, argList, ...)                                                                                       \
    hrnCfgLoad(commandId, argList, (HrnCfgLoadParam){VAR_PARAM_INIT, __VA_ARGS__})

#define HRN_CFG_LOAD(commandId, argList, ...)                                                                                      \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnCfgLoad(commandId, argList, (HrnCfgLoadParam){.log = true, __VA_ARGS__});                                               \
    }                                                                                                                              \
    while (0)

void hrnCfgLoad(ConfigCommand commandId, const StringList *argList, const HrnCfgLoadParam param);

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

void hrnCfgArgRawStrId(StringList *argList, ConfigOption optionId, StringId value);
void hrnCfgArgKeyRawStrId(StringList *argList, ConfigOption optionId, unsigned optionKey, StringId);

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
void hrnCfgEnvKeyRaw(ConfigOption optionId, unsigned optionKey, const String *value);

void hrnCfgEnvRawZ(ConfigOption optionId, const char *value);
void hrnCfgEnvKeyRawZ(ConfigOption optionId, unsigned optionKey, const char *value);

void hrnCfgEnvRemoveRaw(ConfigOption optionId);
void hrnCfgEnvKeyRemoveRaw(ConfigOption optionId, unsigned optionKey);

#endif
