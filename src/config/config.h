/***********************************************************************************************************************************
Command and Option Configuration

This module serves as a database for the configuration options.  The configuration rules reside in config/define.c and
config/parse.c sets the command and options and determines which options are valid for a command.
***********************************************************************************************************************************/
#ifndef CONFIG_CONFIG_H
#define CONFIG_CONFIG_H

#include "common/lock.h"
#include "common/log.h"
#include "common/type/stringList.h"
#include "config/define.h"

#include "config/config.auto.h"

/***********************************************************************************************************************************
Constants

Constants for configuration options.
***********************************************************************************************************************************/
#define CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_DIFF              "diff"
#define CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_FULL              "full"
#define CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_INCR              "incr"

/***********************************************************************************************************************************
Command Functions

Access the current command and command parameters.
***********************************************************************************************************************************/
ConfigCommand cfgCommand(void);
bool cfgCommandInternal(ConfigCommand commandId);
const char *cfgCommandName(ConfigCommand commandId);

bool cfgLockRequired(void);
bool cfgLockRemoteRequired(ConfigCommand commandId);
LockType cfgLockType(void);
LockType cfgLockRemoteType(ConfigCommand commandId);

bool cfgLogFile(void);
LogLevel cfgLogLevelDefault(void);
LogLevel cfgLogLevelStdErrMax(void);

bool cfgParameterAllowed(void);

const StringList *cfgCommandParam(void);

/***********************************************************************************************************************************
Option Functions

Access option values, indexes, and determine if an option is valid for the current command.
***********************************************************************************************************************************/
const Variant *cfgOption(ConfigOption optionId);
bool cfgOptionBool(ConfigOption optionId);
double cfgOptionDbl(ConfigOption optionId);
int cfgOptionInt(ConfigOption optionId);
int64_t cfgOptionInt64(ConfigOption optionId);
const KeyValue *cfgOptionKv(ConfigOption optionId);
const VariantList *cfgOptionLst(ConfigOption optionId);
const String *cfgOptionStr(ConfigOption optionId);
unsigned int cfgOptionUInt(ConfigOption optionId);
uint64_t cfgOptionUInt64(ConfigOption optionId);

unsigned int cfgOptionIndex(ConfigOption optionId);
unsigned int cfgOptionIndexTotal(ConfigOption optionDefId);

const char *cfgOptionName(ConfigOption optionId);

bool cfgOptionValid(ConfigOption optionId);
bool cfgOptionTest(ConfigOption optionId);

/***********************************************************************************************************************************
Option Source Enum

Defines where an option was sourced from.  The source is only needed when determining what parameters should be passed to a remote
process.
***********************************************************************************************************************************/
typedef enum
{
    cfgSourceDefault,                                               // Default value
    cfgSourceParam,                                                 // Passed as command-line parameter
    cfgSourceConfig,                                                // From configuration file or environment variable
} ConfigSource;

/***********************************************************************************************************************************
Load Functions

Used primarily by modules that need to manipulate the configuration.  These modules include, but are not limited to, config/parse.c,
config/load.c.
***********************************************************************************************************************************/
void cfgInit(void);

ConfigDefineCommand cfgCommandDefIdFromId(ConfigCommand commandId);
bool cfgCommandHelp(void);
void cfgCommandHelpSet(bool helpParam);
ConfigCommand cfgCommandId(const char *commandName);
void cfgCommandParamSet(const StringList *param);
void cfgCommandSet(ConfigCommand commandParam);

const String *cfgExe(void);
void cfgExeSet(const String *exeParam);

const Variant *cfgOptionDefault(ConfigOption optionId);
void cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue);
ConfigDefineOption cfgOptionDefIdFromId(ConfigOption optionId);
String *cfgOptionHostPort(ConfigOption optionId, unsigned int *port);
int cfgOptionId(const char *optionName);
ConfigOption cfgOptionIdFromDefId(ConfigDefineOption optionDefId, unsigned int index);
bool cfgOptionNegate(ConfigOption optionId);
void cfgOptionNegateSet(ConfigOption optionId, bool negate);
bool cfgOptionReset(ConfigOption optionId);
void cfgOptionResetSet(ConfigOption optionId, bool reset);
void cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value);
ConfigSource cfgOptionSource(ConfigOption optionId);
void cfgOptionValidSet(ConfigOption optionId, bool valid);

#endif
