/***********************************************************************************************************************************
Exec Configuration
***********************************************************************************************************************************/
#ifndef CONFIG_EXEC_H
#define CONFIG_EXEC_H

#include "common/type/stringList.h"
#include "config/config.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Generate a list of options required for execution of a new command, overriding options with the values in optionReplace when
// present. If local is set then the new command must have access to the local configuration files and environment since only
// options originally passed on the command-line will be added to the list. Use quote when the options will be used as a
// concatenated string rather than being passed directly to exec*() as a list.
FN_EXTERN StringList *cfgExecParam(
    ConfigCommand commandId, ConfigCommandRole commandRoleId, const KeyValue *optionReplace, bool local, bool quote);

#endif
