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
Command Role Enum

Commands may have multiple processes that work together to implement their functionality.  These roles allow each process to know
what it is supposed to do.
***********************************************************************************************************************************/
typedef enum
{
    // Called directly by the user.  This is the main part of the command that may or may not spawn other command roles.
    cfgCmdRoleDefault = 0,

    // Async worker that is spawned so the main process can return a result while work continues.  An async worker may spawn local
    // or remote workers.
    cfgCmdRoleAsync,

    // Local worker for parallelizing jobs.  A local work may spawn a remote worker.
    cfgCmdRoleLocal,

    // Remote worker for accessing resources on another host
    cfgCmdRoleRemote,
} ConfigCommandRole;

// Constants for the command roles
#define CONFIG_COMMAND_ROLE_ASYNC                        "async"
#define CONFIG_COMMAND_ROLE_LOCAL                        "local"
#define CONFIG_COMMAND_ROLE_REMOTE                       "remote"

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

// Current command role (async, local, remote)
ConfigCommandRole cfgCommandRole(void);

bool cfgCommandInternal(ConfigCommand commandId);
const char *cfgCommandName(ConfigCommand commandId);

// Get command:role name
String *cfgCommandRoleName(void);

bool cfgLockRequired(void);
bool cfgLockRemoteRequired(void);
LockType cfgLockType(void);

bool cfgLogFile(void);
LogLevel cfgLogLevelDefault(void);

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
void cfgCommandHelpSet(bool help);

// Get command id by name.  If error is true then assert when the command does not exist.
ConfigCommand cfgCommandId(const char *commandName, bool error);

void cfgCommandParamSet(const StringList *param);
void cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId);

// Get command/role name with custom separator
String *cfgCommandRoleNameParam(ConfigCommand commandId, ConfigCommandRole commandRoleId, const String *separator);

// Convert command role from String to enum and vice versa
ConfigCommandRole cfgCommandRoleEnum(const String *commandRole);
const String *cfgCommandRoleStr(ConfigCommandRole commandRole);

const String *cfgExe(void);
void cfgExeSet(const String *exe);

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
