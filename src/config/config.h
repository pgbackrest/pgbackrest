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
// Current command
ConfigCommand cfgCommand(void);

// Current command role (async, local, remote)
ConfigCommandRole cfgCommandRole(void);

// Is this command internal-only?
bool cfgCommandInternal(ConfigCommand commandId);

// Get command name by id
const char *cfgCommandName(ConfigCommand commandId);

// Get command:role name
String *cfgCommandRoleName(void);

// Does this command require an immediate lock?
bool cfgLockRequired(void);

// Does the command require a remote lock?
bool cfgLockRemoteRequired(void);

// Lock type required for this command
LockType cfgLockType(void);

// Does this command log to a file?
bool cfgLogFile(void);

// Default log level -- used for log messages that are common to all commands
LogLevel cfgLogLevelDefault(void);

// Does this command allow parameters?
bool cfgParameterAllowed(void);

// Command parameters, if any
const StringList *cfgCommandParam(void);

/***********************************************************************************************************************************
Option Functions

Access option values, indexes, and determine if an option is valid for the current command.
***********************************************************************************************************************************/
// Get config options for various types
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

// Get index for option
unsigned int cfgOptionIndex(ConfigOption optionId);

// Get total indexed values for option
unsigned int cfgOptionIndexTotal(ConfigOption optionDefId);

// Option name by id
const char *cfgOptionName(ConfigOption optionId);

// Is the option valid for this command?
bool cfgOptionValid(ConfigOption optionId);

// Is the option valid for the command and set?
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
// Initialize or reinitialize the configuration data
void cfgInit(void);

// Get the define id for this command. This can be done by just casting the id to the define id. There may be a time when they are
// not one to one and this function can be modified to do the mapping.
ConfigDefineCommand cfgCommandDefIdFromId(ConfigCommand commandId);

// Was help requested?
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

// pgBackRest exe
const String *cfgExe(void);
void cfgExeSet(const String *exe);

// Option default
const Variant *cfgOptionDefault(ConfigOption optionId);

// Set option default. Option defaults are generally not set in advance because the vast majority of them are never used.  It is
// more efficient to generate them when they are requested. Some defaults are (e.g. the exe path) are set at runtime.
void cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue);

// Get the option define for this option
ConfigDefineOption cfgOptionDefIdFromId(ConfigOption optionId);

// Parse a host option and extract the host and port (if it exists)
String *cfgOptionHostPort(ConfigOption optionId, unsigned int *port);

// Get option id by name
int cfgOptionId(const char *optionName);

// Get the id for this option define
ConfigOption cfgOptionIdFromDefId(ConfigDefineOption optionDefId, unsigned int index);

// Was the option negated?
bool cfgOptionNegate(ConfigOption optionId);
void cfgOptionNegateSet(ConfigOption optionId, bool negate);

// Was the option reset?
bool cfgOptionReset(ConfigOption optionId);
void cfgOptionResetSet(ConfigOption optionId, bool reset);

// Set config option
void cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value);

// How was the option set (default, param, config)?
ConfigSource cfgOptionSource(ConfigOption optionId);

// Set option valid
void cfgOptionValidSet(ConfigOption optionId, bool valid);

#endif
