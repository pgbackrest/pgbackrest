/***********************************************************************************************************************************
Help Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "command/help/help.h"
#include "common/compress/bz2/decompress.h"
#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/fdWrite.h"
#include "common/io/io.h"
#include "common/memContext.h"
#include "common/type/pack.h"
#include "config/config.intern.h"
#include "config/parse.h"
#include "version.h"

/***********************************************************************************************************************************
Define the console width - use a fixed width of 80 since this should be safe on virtually all consoles
***********************************************************************************************************************************/
#define CONSOLE_WIDTH                                               80

/***********************************************************************************************************************************
Helper function to split a string into a string list based on a delimiter and max size per item. In other words each item in the
list will be no longer than size even if multiple delimiters are skipped. This is useful for breaking up text on spaces, for
example.
***********************************************************************************************************************************/
static StringList *
helpRenderSplitSize(const String *const string, const char *const delimiter, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRINGZ, delimiter);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);
    ASSERT(delimiter != NULL);
    ASSERT(size > 0);

    // Create the list
    StringList *const this = strLstNew();

    // Base points to the beginning of the string that is being searched
    const char *stringBase = strZ(string);

    // Match points to the next delimiter match that has been found
    const char *stringMatchLast = NULL;
    const char *stringMatch = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        do
        {
            // Find a delimiter match
            stringMatch = strstr(stringMatchLast == NULL ? stringBase : stringMatchLast, delimiter);

            // If a match was found then add the string
            if (stringMatch != NULL)
            {
                if ((size_t)(stringMatch - stringBase) >= size)
                {
                    if (stringMatchLast != NULL)
                        stringMatch = stringMatchLast - strlen(delimiter);

                    strLstAddZSub(this, stringBase, (size_t)(stringMatch - stringBase));
                    stringBase = stringMatch + strlen(delimiter);
                    stringMatchLast = NULL;
                }
                else
                    stringMatchLast = stringMatch + strlen(delimiter);
            }
            // Else make whatever is left the last string
            else
            {
                if (stringMatchLast != NULL && strlen(stringBase) - strlen(delimiter) >= size)
                {
                    strLstAddZSub(this, stringBase, (size_t)((stringMatchLast - strlen(delimiter)) - stringBase));
                    stringBase = stringMatchLast;
                }

                strLstAddZ(this, stringBase);
            }
        }
        while (stringMatch != NULL);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(STRING_LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
helpRenderText(
    const String *const text, const bool internal, const bool beta, const size_t indent, const bool indentFirst,
    const size_t length)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, text);
        FUNCTION_TEST_PARAM(BOOL, internal);
        FUNCTION_TEST_PARAM(BOOL, beta);
        FUNCTION_TEST_PARAM(SIZE, indent);
        FUNCTION_TEST_PARAM(BOOL, indentFirst);
        FUNCTION_TEST_PARAM(SIZE, length);
    FUNCTION_TEST_END();

    ASSERT(text != NULL);
    ASSERT(length > 0);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Split the text into paragraphs
        const StringList *const lineList = strLstNewSplitZ(
            strNewFmt(
                "%s%s", strZ(text),
                beta ?
                    "\n\nFOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION." :
                    (internal ? "\n\nFOR INTERNAL USE ONLY. DO NOT USE IN PRODUCTION." : "")),
            "\n");

        // Iterate through each paragraph and split the lines according to the line length
        for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
        {
            // Add LF if there is already content
            if (strSize(result) != 0)
                strCat(result, LF_STR);

            // Split the paragraph into lines that don't exceed the line length
            const StringList *const partList = helpRenderSplitSize(strLstGet(lineList, lineIdx), " ", length - indent);

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
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Helper functions for helpRender() to output values as strings
***********************************************************************************************************************************/
static String *
helpRenderValueIdx(const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (cfgOptionIdxSource(optionId, optionIdx) != cfgSourceDefault)
    {
        result = strNew();

        MEM_CONTEXT_TEMP_BEGIN()
        {
            const Variant *const value = cfgOptionIdxVar(optionId, optionIdx);
            ASSERT(value != NULL);

            switch (varType(value))
            {
                case varTypeKeyValue:
                {
                    const KeyValue *const optionKv = varKv(value);
                    const VariantList *const keyList = kvKeyList(optionKv);

                    for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
                    {
                        if (keyIdx != 0)
                            strCatZ(result, ", ");

                        strCatFmt(
                            result, "%s=%s", strZ(varStr(varLstGet(keyList, keyIdx))),
                            strZ(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx)))));
                    }

                    break;
                }

                case varTypeVariantList:
                {
                    const VariantList *const list = varVarLst(value);

                    for (unsigned int listIdx = 0; listIdx < varLstSize(list); listIdx++)
                    {
                        if (listIdx != 0)
                            strCatZ(result, ", ");

                        strCatFmt(result, "%s", strZ(varStr(varLstGet(list, listIdx))));
                    }

                    break;
                }

                default:
                    strCat(result, cfgOptionIdxDisplay(optionId, optionIdx));
                    break;
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

static String *
helpRenderValue(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    String *result = helpRenderValueIdx(optionId, 0);

    if (cfgOptionGroup(optionId))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            for (unsigned int optionIdx = 1; optionIdx < cfgOptionGroupIdxTotal(cfgOptionGroupId(optionId)); optionIdx++)
            {
                const String *const value = helpRenderValueIdx(optionId, optionIdx);

                if (!strEq(result, value))
                {
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        strFree(result);
                        result = strNewZ("<multi>");
                    }
                    MEM_CONTEXT_PRIOR_END();

                    break;
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Determine if the first character of a summary should be lower-case
***********************************************************************************************************************************/
static String *
helpRenderSummary(const String *const summary)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, summary);
    FUNCTION_TEST_END();

    ASSERT(summary != NULL);

    // Strip final period off summary
    String *const result = strCatN(strNew(), summary, strSize(summary) - 1);

    // Lower-case first letter if first word does not appear to be an acronym or proper name
    unsigned int totalLetter = 0;
    unsigned int totalCapital = 0;

    for (unsigned int resultIdx = 0; resultIdx < strSize(result); resultIdx++)
    {
        const char resultChar = strZ(result)[resultIdx];

        if (resultChar == ' ')
            break;

        if (isalpha(resultChar))
            totalLetter++;

        if (isupper(resultChar))
            totalCapital++;
    }

    if (totalCapital == 1 && totalCapital != totalLetter)
        strFirstLower(result);

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Render help to a string
***********************************************************************************************************************************/
// Stored unpacked command data
typedef struct HelpCommandData
{
    bool internal;                                                  // Is the command internal?
    const String *summary;                                          // Short summary of the command
    const String *description;                                      // Full description of the command
} HelpCommandData;

// Stored unpacked option data
typedef struct HelpOptionData
{
    bool internal;                                                  // Is the option internal?
    const String *section;                                          // eg. general, command
    const String *summary;                                          // Short summary of the option
    const String *description;                                      // Full description of the option
    StringList *deprecatedNames;                                    // Deprecated names for the option
} HelpOptionData;

static String *
helpRender(const Buffer *const helpData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, helpData);
    FUNCTION_TEST_END();

    String *const result = strCatZ(strNew(), PROJECT_NAME " " PROJECT_VERSION);

    // Display version only
    if (!cfgCommandHelp() &&
        ((cfgCommand() == cfgCmdHelp && cfgOptionBool(cfgOptVersion) && !cfgOptionBool(cfgOptHelp)) ||
         cfgCommand() == cfgCmdVersion))
    {
        if (cfgCommand() == cfgCmdVersion && cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_NUM)
            FUNCTION_TEST_RETURN(STRING, strCatFmt(strTrunc(result), "%d", PROJECT_VERSION_NUM));
        else
        {
            strCatChr(result, '\n');
            FUNCTION_TEST_RETURN(STRING, result);
        }
    }

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set a small buffer size to minimize memory usage
        ioBufferSizeSet(8192);

        // Read pack from compressed buffer
        IoRead *const helpRead = ioBufferReadNew(helpData);
        ioFilterGroupAdd(ioReadFilterGroup(helpRead), bz2DecompressNew(false));
        ioReadOpen(helpRead);

        PackRead *const pckHelp = pckReadNewIo(helpRead);

        // Unpack command data
        HelpCommandData *const commandData = memNew(sizeof(HelpCommandData) * CFG_COMMAND_TOTAL);

        pckReadArrayBeginP(pckHelp);

        for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
        {
            commandData[commandId] = (HelpCommandData)
            {
                .internal = pckReadBoolP(pckHelp),
                .summary = pckReadStrP(pckHelp),
                .description = pckReadStrP(pckHelp),
            };
        }

        pckReadArrayEndP(pckHelp);

        // Message for more help when it is available
        const String *more = NULL;

        // Display general help
        if (!cfgCommandHelp())
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
                if (commandData[commandId].internal)
                    continue;

                if (strlen(cfgParseCommandName(commandId)) > commandSizeMax)
                    commandSizeMax = strlen(cfgParseCommandName(commandId));
            }

            // Output help for each command
            for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
            {
                if (commandData[commandId].internal)
                    continue;

                strCatFmt(
                    result, "    %s%*s%s\n", cfgParseCommandName(commandId),
                    (int)(commandSizeMax - strlen(cfgParseCommandName(commandId)) + 2), "",
                    strZ(
                        helpRenderText(
                            helpRenderSummary(commandData[commandId].summary), false, false, commandSizeMax + 6, false,
                            CONSOLE_WIDTH)));
            }

            // Construct message for more help
            more = strNewZ("[command]");
        }
        else
        {
            const ConfigCommand commandId = cfgCommand();
            const char *const commandName = cfgParseCommandName(commandId);

            // Unpack option data
            HelpOptionData *const optionData = memNew(sizeof(HelpOptionData) * CFG_OPTION_TOTAL);

            pckReadArrayBeginP(pckHelp);

            for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
            {
                optionData[optionId] = (HelpOptionData)
                {
                    .internal = pckReadBoolP(pckHelp),
                    .section = pckReadStrP(pckHelp, .defaultValue = STRDEF("general")),
                    .summary = pckReadStrP(pckHelp),
                    .description = pckReadStrP(pckHelp),
                };

                // Unpack deprecated names
                if (!pckReadNullP(pckHelp))
                {
                    optionData[optionId].deprecatedNames = strLstNew();

                    pckReadArrayBeginP(pckHelp);

                    while (pckReadNext(pckHelp))
                        strLstAdd(optionData[optionId].deprecatedNames, pckReadStrP(pckHelp));

                    pckReadArrayEndP(pckHelp);
                }

                // Unpack command overrides
                if (!pckReadNullP(pckHelp))
                {
                    pckReadArrayBeginP(pckHelp);

                    while (pckReadNext(pckHelp))
                    {
                        // Get command override id
                        const ConfigCommand commandIdArray = pckReadId(pckHelp) - 1;

                        // Unpack override data
                        pckReadObjBeginP(pckHelp, .id = commandIdArray + 1);

                        const bool internal = pckReadBoolP(pckHelp, .defaultValue = optionData[optionId].internal);
                        const String *const summary = pckReadStrP(pckHelp, .defaultValue = optionData[optionId].summary);
                        const String *const description = pckReadStrP(pckHelp, .defaultValue = optionData[optionId].description);

                        pckReadObjEndP(pckHelp);

                        // Only use overrides for the current command
                        if (commandId == commandIdArray)
                        {
                            optionData[optionId].internal = internal;
                            optionData[optionId].section = NULL;
                            optionData[optionId].summary = summary;
                            optionData[optionId].description = description;
                        }
                    }

                    pckReadArrayEndP(pckHelp);
                }
            }

            pckReadArrayEndP(pckHelp);

            // Output command part of title
            strCatFmt(result, " - '%s' command", commandName);

            // If no additional params then this is command help
            if (strLstEmpty(cfgCommandParam()))
            {
                // Output command summary and description. Add a warning for internal commands.
                CHECK(
                    AssertError, commandData[commandId].summary != NULL && commandData[commandId].description != NULL,
                    "command help missing");

                strCatFmt(
                    result,
                    " help\n"
                    "\n"
                    "%s\n"
                    "\n"
                    "%s\n",
                    strZ(helpRenderText(commandData[commandId].summary, false, false, 0, true, CONSOLE_WIDTH)),
                    strZ(
                        helpRenderText(
                            commandData[commandId].description, commandData[commandId].internal, false, 0, true, CONSOLE_WIDTH)));

                // Construct key/value of sections and options
                KeyValue *const optionKv = kvNew();
                size_t optionSizeMax = 0;

                for (unsigned int optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
                {
                    if (cfgParseOptionValid(commandId, cfgCmdRoleMain, optionId) && !optionData[optionId].internal)
                    {
                        const String *section = optionData[optionId].section;

                        if (section == NULL ||
                            (!strEqZ(section, "general") && !strEqZ(section, "log") && !strEqZ(section, "maintainer") &&
                             !strEqZ(section, "repository") && !strEqZ(section, "stanza")))
                        {
                            section = strNewZ("command");
                        }

                        kvAdd(optionKv, VARSTR(section), VARUINT(optionId));

                        if (strlen(cfgParseOptionName(optionId)) > optionSizeMax)
                            optionSizeMax = strlen(cfgParseOptionName(optionId));
                    }
                }

                // Output sections
                const StringList *const sectionList = strLstSort(strLstNewVarLst(kvKeyList(optionKv)), sortOrderAsc);

                for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
                {
                    const String *const section = strLstGet(sectionList, sectionIdx);

                    strCatFmt(result, "\n%s Options:\n\n", strZ(strFirstUpper(strDup(section))));

                    // Output options
                    const VariantList *const optionList = kvGetList(optionKv, VARSTR(section));

                    for (unsigned int optionIdx = 0; optionIdx < varLstSize(optionList); optionIdx++)
                    {
                        const ConfigOption optionId = varUInt(varLstGet(optionList, optionIdx));
                        String *const summary = helpRenderSummary(optionData[optionId].summary);

                        // Output current and default values if they exist
                        const String *const defaultValue = cfgOptionDefault(optionId);
                        const String *const value = helpRenderValue(optionId);

                        if (value != NULL || defaultValue != NULL)
                        {
                            strCatZ(summary, " [");

                            if (value != NULL)
                                strCatFmt(summary, "current=%s", cfgParseOptionSecure(optionId) ? "<redacted>" : strZ(value));

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
                            strZ(helpRenderText(summary, false, false, optionSizeMax + 6, false, CONSOLE_WIDTH)));
                    }
                }

                // Construct message for more help
                ASSERT(optionSizeMax > 0);
                more = strNewFmt("%s [option]", commandName);
            }
            // Else option help for the specified command
            else
            {
                // Make sure only one option was specified
                if (strLstSize(cfgCommandParam()) > 1)
                    THROW(ParamInvalidError, "only one option allowed for option help");

                // Ensure the option is valid
                const String *const optionName = strLstGet(cfgCommandParam(), 0);
                const CfgParseOptionResult option = cfgParseOptionP(optionName, .ignoreMissingIndex = true);

                // Error when option is not found or is invalid for the current command
                if (!option.found || !cfgParseOptionValid(cfgCommand(), cfgCmdRoleMain, option.id))
                    THROW_FMT(OptionInvalidError, "option '%s' is not valid for command '%s'", strZ(optionName), commandName);

                // Output option summary and description. Add a warning for internal and beta options.
                CHECK(
                    AssertError, optionData[option.id].summary != NULL && optionData[option.id].description != NULL,
                    "option help missing");

                strCatFmt(
                    result,
                    " - '%s' option help\n"
                    "\n"
                    "%s\n"
                    "\n"
                    "%s\n",
                    cfgParseOptionName(option.id),
                    strZ(helpRenderText(optionData[option.id].summary, false, false, 0, true, CONSOLE_WIDTH)),
                    strZ(
                        helpRenderText(
                            optionData[option.id].description, optionData[option.id].internal, option.beta, 0, true,
                            CONSOLE_WIDTH)));

                // Output current and default values if they exist
                const String *const defaultValue = cfgOptionDefault(option.id);
                const String *const value = helpRenderValue(option.id);

                if (value != NULL || defaultValue != NULL)
                {
                    strCat(result, LF_STR);

                    if (value != NULL)
                    {
                        strCatZ(result, "current:");

                        if (cfgParseOptionSecure(option.id))
                            strCatZ(result, " <redacted>\n");
                        else if (!strEqZ(value, "<multi>"))
                            strCatFmt(result, " %s\n", strZ(value));
                        else
                        {
                            const unsigned int groupId = cfgOptionGroupId(option.id);

                            strCatChr(result, '\n');

                            for (unsigned int optionIdx = 0; optionIdx < cfgOptionGroupIdxTotal(groupId); optionIdx++)
                            {
                                const String *const value = helpRenderValueIdx(option.id, optionIdx);

                                if (value != NULL)
                                    strCatFmt(result, "  %s: %s\n", cfgOptionGroupName(groupId, optionIdx), strZ(value));
                            }
                        }
                    }

                    if (defaultValue != NULL)
                        strCatFmt(result, "default: %s\n", strZ(defaultValue));
                }

                // Output alternate name (call it deprecated so the user will know not to use it)
                if (optionData[option.id].deprecatedNames != NULL)
                {
                    strCatFmt(
                        result, "\ndeprecated name%s: %s\n", strLstSize(optionData[option.id].deprecatedNames) > 1 ? "s" : "",
                        strZ(strLstJoin(optionData[option.id].deprecatedNames, ", ")));
                }
            }
        }

        // If there is more help available output a message to let the user know
        if (more != NULL)
            strCatFmt(result, "\nUse '" PROJECT_BIN " help %s' for more information.\n", strZ(more));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdHelp(const Buffer *const helpData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BUFFER, helpData);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioFdWriteOneStr(STDOUT_FILENO, helpRender(helpData));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
