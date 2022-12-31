/***********************************************************************************************************************************
Command and Option Configuration

This module serves as a database for the configuration options.  The configuration rules reside in config/define.c and
config/parse.c sets the command and options and determines which options are valid for a command.
***********************************************************************************************************************************/
#ifndef CONFIG_CONFIG_H
#define CONFIG_CONFIG_H

#include "common/lock.h"
#include "common/log.h"
#include "common/type/stringId.h"
#include "common/type/stringList.h"
#include "config/config.auto.h"

/***********************************************************************************************************************************
Command Role Enum

Commands may have multiple processes that work together to implement their functionality.  These roles allow each process to know
what it is supposed to do.
***********************************************************************************************************************************/
typedef enum
{
    // Called directly by the user. This is the main process of the command that may or may not spawn other command roles.
    cfgCmdRoleMain = 0,

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
Command Functions

Access the current command and command parameters.
***********************************************************************************************************************************/
// Current command
FV_EXTERN ConfigCommand cfgCommand(void);

// Current command role (async, local, remote)
FV_EXTERN ConfigCommandRole cfgCommandRole(void);

// Get command name
FV_EXTERN const char *cfgCommandName(void);

// Get command:role name
FV_EXTERN String *cfgCommandRoleName(void);

// Does this command require an immediate lock?
FV_EXTERN bool cfgLockRequired(void);

// Does the command require a remote lock?
FV_EXTERN bool cfgLockRemoteRequired(void);

// Lock type required for this command
FV_EXTERN LockType cfgLockType(void);

// Does this command log to a file?
FV_EXTERN bool cfgLogFile(void);

// Default log level -- used for log messages that are common to all commands
FV_EXTERN LogLevel cfgLogLevelDefault(void);

// Command parameters, if any
FV_EXTERN const StringList *cfgCommandParam(void);

/***********************************************************************************************************************************
Option Group Functions
***********************************************************************************************************************************/
// Format group name for display to the user. Useful for messages that do not show an option name but must use an group name that
// the user will recognize.
FV_EXTERN const char *cfgOptionGroupName(ConfigOptionGroup groupId, unsigned int groupIdx);

// Get the default index for this group, i.e. the index that will be used if a non-indexed function like cfgOptionTest() is called.
FV_EXTERN unsigned int cfgOptionGroupIdxDefault(ConfigOptionGroup groupId);

// Convert the group index to a key, i.e. the key that was used in the original configuration file, command-line, etc.
FV_EXTERN unsigned int cfgOptionGroupIdxToKey(ConfigOptionGroup groupId, unsigned int groupIdx);

// Total indexes, 0 if the group is not valid. Will be the total configured indexes, no matter which raw indexes were used during
// configuration. e.g., if pg1-path and pg8-path are configured then this function will return 2.
FV_EXTERN unsigned int cfgOptionGroupIdxTotal(ConfigOptionGroup groupId);

/***********************************************************************************************************************************
Option Functions

Access option values, indexes, and determine if an option is valid for the current command. Most functions have a variant that
accepts an index, which currently work with non-indexed options (with optionIdx 0) but they may not always do so.
***********************************************************************************************************************************/
// Get default index for a group option
FV_EXTERN unsigned int cfgOptionIdxDefault(ConfigOption optionId);

// Get boolean config option
FV_EXTERN bool cfgOptionIdxBool(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS bool
cfgOptionBool(const ConfigOption optionId)
{
    return cfgOptionIdxBool(optionId, cfgOptionIdxDefault(optionId));
}

// Get int config option
FV_EXTERN int cfgOptionIdxInt(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS int
cfgOptionInt(const ConfigOption optionId)
{
    return cfgOptionIdxInt(optionId, cfgOptionIdxDefault(optionId));
}

// Get int64 config option
FV_EXTERN int64_t cfgOptionIdxInt64(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS int64_t
cfgOptionInt64(const ConfigOption optionId)
{
    return cfgOptionIdxInt64(optionId, cfgOptionIdxDefault(optionId));
}

// Get kv config option
FV_EXTERN const KeyValue *cfgOptionIdxKv(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS const KeyValue *
cfgOptionKv(const ConfigOption optionId)
{
    return cfgOptionIdxKv(optionId, cfgOptionIdxDefault(optionId));
}

FV_EXTERN const KeyValue *cfgOptionIdxKvNull(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS const KeyValue *
cfgOptionKvNull(const ConfigOption optionId)
{
    return cfgOptionIdxKvNull(optionId, cfgOptionIdxDefault(optionId));
}

// Get list config option
FV_EXTERN const VariantList *cfgOptionIdxLst(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS const VariantList *
cfgOptionLst(const ConfigOption optionId)
{
    return cfgOptionIdxLst(optionId, cfgOptionIdxDefault(optionId));
}

// Get String config option
FV_EXTERN const String *cfgOptionIdxStr(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS const String *
cfgOptionStr(const ConfigOption optionId)
{
    return cfgOptionIdxStr(optionId, cfgOptionIdxDefault(optionId));
}

FV_EXTERN const String *cfgOptionIdxStrNull(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS const String *
cfgOptionStrNull(const ConfigOption optionId)
{
    return cfgOptionIdxStrNull(optionId, cfgOptionIdxDefault(optionId));
}

// Get StringId config option
FV_EXTERN StringId cfgOptionIdxStrId(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS StringId
cfgOptionStrId(const ConfigOption optionId)
{
    return cfgOptionIdxStrId(optionId, cfgOptionIdxDefault(optionId));
}

// Get uint config option
FV_EXTERN unsigned int cfgOptionIdxUInt(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS unsigned int
cfgOptionUInt(const ConfigOption optionId)
{
    return cfgOptionIdxUInt(optionId, cfgOptionIdxDefault(optionId));
}

// Get uint64 config option
FV_EXTERN uint64_t cfgOptionIdxUInt64(ConfigOption optionId, unsigned int optionIdx);

FN_INLINE_ALWAYS uint64_t
cfgOptionUInt64(const ConfigOption optionId)
{
    return cfgOptionIdxUInt64(optionId, cfgOptionIdxDefault(optionId));
}

// Format the configuration value for display to the user or passing on a command line. If the value was set by the user via the
// command line, config, etc., then that exact value will be displayed. This makes it easier for the user to recognize the value and
// saves having to format it into something reasonable, especially for time and size option types. Note that cfgOptTypeHash and
// cfgOptTypeList option types are not supported by these functions, but they are generally not displayed to the user as a whole.
FV_EXTERN const String *cfgOptionDisplay(const ConfigOption optionId);
FV_EXTERN const String *cfgOptionIdxDisplay(const ConfigOption optionId, const unsigned int optionIdx);

// Option name by id
FV_EXTERN const char *cfgOptionName(ConfigOption optionId);
FV_EXTERN const char *cfgOptionIdxName(ConfigOption optionId, unsigned int optionIdx);

// Is the option valid for this command?
FV_EXTERN bool cfgOptionValid(ConfigOption optionId);

// Is the option valid for the command and also has a value?
FV_EXTERN bool cfgOptionTest(ConfigOption optionId);
FV_EXTERN bool cfgOptionIdxTest(ConfigOption optionId, unsigned int optionIdx);

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
FV_EXTERN bool cfgCommandHelp(void);

FV_EXTERN void cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId);

// pgBackRest exe
FV_EXTERN const String *cfgExe(void);

// Set option default. Option defaults are generally not set in advance because the vast majority of them are never used.  It is
// more efficient to generate them when they are requested. Some defaults are (e.g. the exe path) are set at runtime.
FV_EXTERN void cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue);

// Was the option negated?
FV_EXTERN bool cfgOptionIdxNegate(ConfigOption optionId, unsigned int optionIdx);

// Was the option reset?
FV_EXTERN bool cfgOptionIdxReset(ConfigOption optionId, unsigned int optionIdx);

// Set config option
FV_EXTERN void cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value);
FV_EXTERN void cfgOptionIdxSet(ConfigOption optionId, unsigned int optionIdx, ConfigSource source, const Variant *value);

// How was the option set (default, param, config)?
FV_EXTERN ConfigSource cfgOptionSource(ConfigOption optionId);
FV_EXTERN ConfigSource cfgOptionIdxSource(ConfigOption optionId, unsigned int optionIdx);

#endif
