/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#ifndef CONFIG_H
#define CONFIG_H

#include "common/type.h"
#include "config/define.h"

#include "config/config.auto.h"

/***********************************************************************************************************************************
Option source enum - defines where an option value came from
***********************************************************************************************************************************/
typedef enum
{
    cfgSourceDefault,                                               // Default value
    cfgSourceParam,                                                 // Passed as command-line parameter
    cfgSourceConfig,                                                // From configuration file
} ConfigSource;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cfgInit();

ConfigCommand cfgCommand();
bool cfgCommandHelp();
int cfgCommandId(const char *commandName);
const char *cfgCommandName(ConfigCommand commandId);
ConfigDefineCommand cfgCommandDefIdFromId(ConfigCommand commandId);
const StringList *cfgCommandParam();

void cfgCommandHelpSet(bool helpParam);
void cfgCommandParamSet(const StringList *param);
void cfgCommandSet(ConfigCommand commandParam);

const String *cfgExe();
void cfgExeSet(const String *exeParam);

const Variant *cfgOption(ConfigOption optionId);
bool cfgOptionBool(ConfigOption optionId);
const Variant *cfgOptionDefault(ConfigOption optionId);
void cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue);
ConfigDefineOption cfgOptionDefIdFromId(ConfigOption optionId);
double cfgOptionDbl(ConfigOption optionId);
int cfgOptionId(const char *optionName);
ConfigOption cfgOptionIdFromDefId(ConfigDefineOption optionDefId, unsigned int index);
unsigned int cfgOptionIndex(ConfigOption optionId);
unsigned int cfgOptionIndexTotal(ConfigOption optionDefId);
int cfgOptionInt(ConfigOption optionId);
int64 cfgOptionInt64(ConfigOption optionId);
const KeyValue *cfgOptionKv(ConfigOption optionId);
const VariantList *cfgOptionLst(ConfigOption optionId);
const char *cfgOptionName(ConfigOption optionId);
bool cfgOptionNegate(ConfigOption optionId);
bool cfgOptionReset(ConfigOption optionId);
void cfgOptionResetSet(ConfigOption optionId, bool reset);
ConfigSource cfgOptionSource(ConfigOption optionId);
const String *cfgOptionStr(ConfigOption optionId);
bool cfgOptionValid(ConfigOption optionId);

void cfgOptionNegateSet(ConfigOption optionId, bool negate);
void cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value);
void cfgOptionValidSet(ConfigOption optionId, bool valid);

#endif
