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

#define CFG_COMMAND_ROLE_TOTAL                           4

/***********************************************************************************************************************************
Constants

Constants for configuration options.
***********************************************************************************************************************************/
#define CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_DIFF              "diff"
#define CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_FULL              "full"
#define CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_INCR              "incr"

#define CFGOPTVAL_TMP_REPO_RETENTION_FULL_TYPE_COUNT                "count"
#define CFGOPTVAL_TMP_REPO_RETENTION_FULL_TYPE_TIME                 "time"

/***********************************************************************************************************************************
Command Functions

Access the current command and command parameters.
***********************************************************************************************************************************/
// Current command
ConfigCommand cfgCommand(void);

// Current command role (async, local, remote)
ConfigCommandRole cfgCommandRole(void);

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

// Command parameters, if any
const StringList *cfgCommandParam(void);

/***********************************************************************************************************************************
Option Group Functions
***********************************************************************************************************************************/
// Get the default index for this group, i.e. the index that will be used if a non-indexed function like cfgOptionTest() is called.
unsigned int cfgOptionGroupIdxDefault(ConfigOptionGroup groupId);

// Convert the group index to a key, i.e. the key that was used in the original configuration file, command-line, etc. Useful for
// messages that do not show an option name but must use an index that the user will recognize. It is preferrable to generate an
// option name with cfgOptionIdxName() when possible.
unsigned int cfgOptionGroupIdxToKey(ConfigOptionGroup groupId, unsigned int groupIdx);

// Total indexes, 0 if the group is not valid. Will be the total configured indexes, no matter which raw indexes were used during
// configuration. e.g., if pg1-path and pg8-path are configured then this function will return 2.
unsigned int cfgOptionGroupIdxTotal(ConfigOptionGroup groupId);

// Are any options in the group valid for the command?
bool cfgOptionGroupValid(ConfigOptionGroup groupId);

/***********************************************************************************************************************************
Option Functions

Access option values, indexes, and determine if an option is valid for the current command. Most functions have a variant that
accepts an index, which currently work with non-indexed options (with optionIdx 0) but they may not always do so.
***********************************************************************************************************************************/
// Get config options for various types
const Variant *cfgOption(ConfigOption optionId);
const Variant *cfgOptionIdx(ConfigOption optionId, unsigned int optionIdx);
bool cfgOptionBool(ConfigOption optionId);
bool cfgOptionIdxBool(ConfigOption optionId, unsigned int optionIdx);
int cfgOptionInt(ConfigOption optionId);
int cfgOptionIdxInt(ConfigOption optionId, unsigned int optionIdx);
int64_t cfgOptionInt64(ConfigOption optionId);
int64_t cfgOptionIdxInt64(ConfigOption optionId, unsigned int optionIdx);
const KeyValue *cfgOptionKv(ConfigOption optionId);
const KeyValue *cfgOptionIdxKv(ConfigOption optionId, unsigned int optionIdx);
const VariantList *cfgOptionLst(ConfigOption optionId);
const VariantList *cfgOptionIdxLst(ConfigOption optionId, unsigned int optionIdx);
const String *cfgOptionStr(ConfigOption optionId);
const String *cfgOptionIdxStr(ConfigOption optionId, unsigned int optionIdx);
const String *cfgOptionStrNull(ConfigOption optionId);
const String *cfgOptionIdxStrNull(ConfigOption optionId, unsigned int optionIdx);
unsigned int cfgOptionUInt(ConfigOption optionId);
unsigned int cfgOptionIdxUInt(ConfigOption optionId, unsigned int optionIdx);
uint64_t cfgOptionUInt64(ConfigOption optionId);
uint64_t cfgOptionIdxUInt64(ConfigOption optionId, unsigned int optionIdx);

// Configuration value suitable for display to the user or passing on a command line. If the value was set by the user via the
// command line, config, etc., then that exact value will be displayed. This makes it easier for the user to recognize the value and
// saves having to format it into something reasonable, especially for time and size option types. Note that cfgOptTypeHash and
// cfgOptTypeList option types are not supported by these functions, but they are generally not displayed to the user as a whole.
const String *cfgOptionDisplay(const ConfigOption optionId);
const String *cfgOptionIdxDisplay(const ConfigOption optionId, const unsigned int optionIdx);

// Option name by id
const char *cfgOptionName(ConfigOption optionId);
const char *cfgOptionIdxName(ConfigOption optionId, unsigned int optionIdx);

// Is the option valid for this command?
bool cfgOptionValid(ConfigOption optionId);

// Is the option valid for the command and also has a value?
bool cfgOptionTest(ConfigOption optionId);
bool cfgOptionIdxTest(ConfigOption optionId, unsigned int optionIdx);

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
// Was help requested?
bool cfgCommandHelp(void);

// Get command id by name
ConfigCommand cfgCommandId(const char *commandName);

void cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId);

// Get command/role name with custom separator
String *cfgCommandRoleNameParam(ConfigCommand commandId, ConfigCommandRole commandRoleId, const String *separator);

// Convert command role from String to enum and vice versa
ConfigCommandRole cfgCommandRoleEnum(const String *commandRole);
const String *cfgCommandRoleStr(ConfigCommandRole commandRole);

// pgBackRest exe
const String *cfgExe(void);

// Option default - should only be called by the help command
const Variant *cfgOptionDefault(ConfigOption optionId);

// Set option default. Option defaults are generally not set in advance because the vast majority of them are never used.  It is
// more efficient to generate them when they are requested. Some defaults are (e.g. the exe path) are set at runtime.
void cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue);

// Parse a host option and extract the host and port (if it exists)
String *cfgOptionHostPort(ConfigOption optionId, unsigned int *port);
String *cfgOptionIdxHostPort(ConfigOption optionId, unsigned int optionIdx, unsigned int *port);

// Was the option negated?
bool cfgOptionNegate(ConfigOption optionId);
bool cfgOptionIdxNegate(ConfigOption optionId, unsigned int optionIdx);

// Was the option reset?
bool cfgOptionReset(ConfigOption optionId);
bool cfgOptionIdxReset(ConfigOption optionId, unsigned int optionIdx);

// Set config option
void cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value);
void cfgOptionIdxSet(ConfigOption optionId, unsigned int optionIdx, ConfigSource source, const Variant *value);

// How was the option set (default, param, config)?
ConfigSource cfgOptionSource(ConfigOption optionId);
ConfigSource cfgOptionIdxSource(ConfigOption optionId, unsigned int optionIdx);

#endif
