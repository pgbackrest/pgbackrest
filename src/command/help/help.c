/***********************************************************************************************************************************
Help Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/memContext.h"
#include "config/config.h"
#include "config/define.h"
#include "config/parse.h"
#include "version.h"

/***********************************************************************************************************************************
Define the console width - use a fixed with of 80 since this should be safe on virtually all consoles
***********************************************************************************************************************************/
#define CONSOLE_WIDTH                                               80

/***********************************************************************************************************************************
Helper function for helpRender() to make output look good on a console
***********************************************************************************************************************************/
static String *
helpRenderText(const String *text, size_t indent, bool indentFirst, size_t length)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, text);
        FUNCTION_LOG_PARAM(SIZE, indent);
        FUNCTION_LOG_PARAM(BOOL, indentFirst);
        FUNCTION_LOG_PARAM(SIZE, length);
    FUNCTION_LOG_END();

    ASSERT(text != NULL);
    ASSERT(length > 0);

    String *result = strNew("");

    // Split the text into paragraphs
    StringList *lineList = strLstNewSplitZ(text, "\n");

    // Iterate through each paragraph and split the lines according to the line length
    for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
    {
        // Add LF if there is already content
        if (strSize(result) != 0)
            strCat(result, LF_STR);

        // Split the paragraph into lines that don't exceed the line length
        StringList *partList = strLstNewSplitSizeZ(strLstGet(lineList, lineIdx), " ", length - indent);

        for (unsigned int partIdx = 0; partIdx < strLstSize(partList); partIdx++)
        {
            // Indent when required
            if (partIdx != 0 || indentFirst)
            {
                if (partIdx != 0)
                    strCat(result, LF_STR);

                if (strSize(strLstGet(partList, partIdx)))
                    strCatFmt(result, "%*s", (int)indent, "");
            }

            // Add the line
            strCat(result, strLstGet(partList, partIdx));
        }
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Helper function for helpRender() to output values as strings
***********************************************************************************************************************************/
static const String *
helpRenderValue(const Variant *value, ConfigDefineOptionType type)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, value);
        FUNCTION_LOG_PARAM(ENUM, type);
    FUNCTION_LOG_END();

    const String *result = NULL;

    if (value != NULL)
    {
    if (varType(value) == varTypeBool)
    {
        if (varBool(value))
            result = Y_STR;
        else
            result = N_STR;
    }
    else if (varType(value) == varTypeKeyValue)
    {
        String *resultTemp = strNew("");

        const KeyValue *optionKv = varKv(value);
        const VariantList *keyList = kvKeyList(optionKv);

        for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
        {
            if (keyIdx != 0)
                strCatZ(resultTemp, ", ");

            strCatFmt(
                resultTemp, "%s=%s", strZ(varStr(varLstGet(keyList, keyIdx))),
                strZ(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx)))));
        }

        result = resultTemp;
    }
    else if (varType(value) == varTypeVariantList)
    {
        String *resultTemp = strNew("");

        const VariantList *list = varVarLst(value);

        for (unsigned int listIdx = 0; listIdx < varLstSize(list); listIdx++)
        {
            if (listIdx != 0)
                strCatZ(resultTemp, ", ");

            strCatFmt(resultTemp, "%s", strZ(varStr(varLstGet(list, listIdx))));
        }

        result = resultTemp;
    }
    else if (type == cfgDefOptTypeTime)
        result = cvtDoubleToStr((double)varInt64(value) / MSEC_PER_SEC);
    else
        result = varStrForce(value);
    }

    FUNCTION_LOG_RETURN_CONST(STRING, result);
}

/***********************************************************************************************************************************
Render help to a string
***********************************************************************************************************************************/
static String *
helpRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *result = strNew(PROJECT_NAME " " PROJECT_VERSION);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Message for more help when it is available
        const String *more = NULL;

        // Display general help
        if (cfgCommand() == cfgCmdHelp || cfgCommand() == cfgCmdNone)
        {
            strCatZ(
                result,
                " - General help\n"
                "\n"
                "Usage:\n"
                "    " PROJECT_BIN " [options] [command]\n"
                "\n"
                "Commands:\n");

            // Find size of longest command name
            size_t commandSizeMax = 0;

            for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
            {
                if (commandId == cfgCmdNone || cfgCommandInternal(commandId))
                    continue;

                if (strlen(cfgCommandName(commandId)) > commandSizeMax)
                    commandSizeMax = strlen(cfgCommandName(commandId));
            }

            // Output help for each command
            for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
            {
                if (commandId == cfgCmdNone || cfgCommandInternal(commandId))
                    continue;

                strCatFmt(
                    result, "    %s%*s%s\n", cfgCommandName(commandId),
                    (int)(commandSizeMax - strlen(cfgCommandName(commandId)) + 2), "",
                    strZ(helpRenderText(STR(cfgDefCommandHelpSummary(commandId)), commandSizeMax + 6, false, CONSOLE_WIDTH)));
            }

            // Construct message for more help
            more = strNew("[command]");
        }
        else
        {
            ConfigCommand commandId = cfgCommand();
            const char *commandName = cfgCommandName(commandId);

            // Output command part of title
            strCatFmt(result, " - '%s' command", commandName);

            // If no additional params then this is command help
            if (strLstSize(cfgCommandParam()) == 0)
            {
                // Output command summary and description
                strCatFmt(
                    result,
                    " help\n"
                    "\n"
                    "%s\n"
                    "\n"
                    "%s\n",
                    strZ(helpRenderText(STR(cfgDefCommandHelpSummary(commandId)), 0, true, CONSOLE_WIDTH)),
                    strZ(helpRenderText(STR(cfgDefCommandHelpDescription(commandId)), 0, true, CONSOLE_WIDTH)));

                // Construct key/value of sections and options
                KeyValue *optionKv = kvNew();
                size_t optionSizeMax = 0;

                for (unsigned int optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
                {
                    if (cfgParseOptionValid(commandId, optionId) && !cfgDefOptionInternal(commandId, optionId))
                    {
                        const String *section = NULL;

                        if (cfgDefOptionHelpSection(optionId) != NULL)
                            section = strNew(cfgDefOptionHelpSection(optionId));

                        if (section == NULL ||
                            (!strEqZ(section, "general") && !strEqZ(section, "log") && !strEqZ(section, "repository") &&
                             !strEqZ(section, "stanza")))
                        {
                            section = strNew("command");
                        }

                        kvAdd(optionKv, VARSTR(section), VARINT((int)optionId));

                        if (strlen(cfgParseOptionName(optionId)) > optionSizeMax)
                            optionSizeMax = strlen(cfgParseOptionName(optionId));
                    }
                }

                // Output sections
                StringList *sectionList = strLstSort(strLstNewVarLst(kvKeyList(optionKv)), sortOrderAsc);

                for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
                {
                    const String *section = strLstGet(sectionList, sectionIdx);

                    strCatFmt(result, "\n%s Options:\n\n", strZ(strFirstUpper(strDup(section))));

                    // Output options
                    VariantList *optionList = kvGetList(optionKv, VARSTR(section));

                    for (unsigned int optionIdx = 0; optionIdx < varLstSize(optionList); optionIdx++)
                    {
                        ConfigOption optionId = varInt(varLstGet(optionList, optionIdx));

                        // Get option summary
                        String *summary = strFirstLower(strNewN(
                            cfgDefOptionHelpSummary(commandId, optionId),
                            strlen(cfgDefOptionHelpSummary(commandId, optionId)) - 1));

                        // Ouput current and default values if they exist
                        const String *defaultValue = helpRenderValue(cfgOptionDefault(optionId), cfgDefOptionType(optionId));
                        const String *value = NULL;

                        if (cfgOptionSource(optionId) != cfgSourceDefault)
                            value = helpRenderValue(cfgOption(optionId), cfgDefOptionType(optionId));

                        if (value != NULL || defaultValue != NULL)
                        {
                            strCatZ(summary, " [");

                            if (value != NULL)
                                strCatFmt(summary, "current=%s", cfgDefOptionSecure(optionId) ? "<redacted>" : strZ(value));

                            if (defaultValue != NULL)
                            {
                                if (value != NULL)
                                    strCatZ(summary, ", ");

                                strCatFmt(summary, "default=%s", strZ(defaultValue));
                            }

                            strCatZ(summary, "]");
                        }

                        // Output option help
                        strCatFmt(
                            result, "  --%s%*s%s\n",
                            cfgParseOptionName(optionId), (int)(optionSizeMax - strlen(cfgParseOptionName(optionId)) + 2), "",
                            strZ(helpRenderText(summary, optionSizeMax + 6, false, CONSOLE_WIDTH)));
                    }
                }

                // Construct message for more help if there are options
                if (optionSizeMax > 0)
                    more = strNewFmt("%s [option]", commandName);
            }
            // Else option help for the specified command
            else
            {
                // Make sure only one option was specified
                if (strLstSize(cfgCommandParam()) > 1)
                    THROW(ParamInvalidError, "only one option allowed for option help");

                // Ensure the option is valid
                const String *optionName = strLstGet(cfgCommandParam(), 0);
                CfgParseOptionResult option = cfgParseOption(optionName);

                if (!option.found)
                {
                    int optionId = cfgParseOptionId(strZ(optionName));

                    if (optionId == -1)
                        THROW_FMT(OptionInvalidError, "option '%s' is not valid for command '%s'", strZ(optionName), commandName);
                    else
                        option.id = (unsigned int)optionId;
                }

                // Output option summary and description
                strCatFmt(
                    result,
                    " - '%s' option help\n"
                    "\n"
                    "%s\n"
                    "\n"
                    "%s\n",
                    cfgParseOptionName(option.id),
                    strZ(helpRenderText(STR(cfgDefOptionHelpSummary(commandId, option.id)), 0, true, CONSOLE_WIDTH)),
                    strZ(helpRenderText(STR(cfgDefOptionHelpDescription(commandId, option.id)), 0, true, CONSOLE_WIDTH)));

                // Ouput current and default values if they exist
                const String *defaultValue = helpRenderValue(cfgOptionDefault(option.id), cfgDefOptionType(option.id));
                const String *value = NULL;

                if (cfgOptionSource(option.id) != cfgSourceDefault)
                    value = helpRenderValue(cfgOption(option.id), cfgDefOptionType(option.id));

                if (value != NULL || defaultValue != NULL)
                {
                    strCat(result, LF_STR);

                    if (value != NULL)
                        strCatFmt(result, "current: %s\n", cfgDefOptionSecure(option.id) ? "<redacted>" : strZ(value));

                    if (defaultValue != NULL)
                        strCatFmt(result, "default: %s\n", strZ(defaultValue));
                }

                // Output alternate name (call it deprecated so the user will know not to use it)
                if (cfgDefOptionHelpNameAlt(option.id))
                    strCatFmt(result, "\ndeprecated name: %s\n", cfgDefOptionHelpNameAltValue(option.id, 0));
            }
        }

        // If there is more help available output a message to let the user know
        if (more != NULL)
            strCatFmt(result, "\nUse '" PROJECT_BIN " help %s' for more information.\n", strZ(more));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
void
cmdHelp(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioFdWriteOneStr(STDOUT_FILENO, helpRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
