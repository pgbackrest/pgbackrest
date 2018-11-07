/***********************************************************************************************************************************
Help Command
***********************************************************************************************************************************/
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>  // CSHANG only for debugging

#include "common/debug.h"
#include "common/io/handle.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
CSHANG Do I need this? I copied it from Help
Define the console width - use a fixed with of 80 since this should be safe on virtually all consoles
***********************************************************************************************************************************/
#define CONSOLE_WIDTH                                               80

#define INFO_STANZA_NAME                                            "name"

// static VariantList *
// backupList(String *stanza)
// {
// '<REPO:BACKUP>/backup.info'
//             // Attempt to load the backup info file
//             InfoBackup *info = infoBackupNew(storageRepo(), strNew("backup/stanzaname/" INFO_BACKUP_FILE), false);
//

static VariantList *
stanzaInfo(const String *stanza)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
    FUNCTION_TEST_END();

    VariantList *result = varLstNew();

    // Get a list of stanzas in the backup directory

    StringList *stanzaName = strLstSort(storageListNP(storageRepo(), strNew(STORAGE_PATH_BACKUP)), sortOrderAsc);

    for (unsigned int idx = 0; idx < strLstSize(stanzaName); idx++)
    {
        // If a specific stanza has been requested and this is not it, then continue to the next in the list
        if (stanza != NULL && !strEq(stanza, strLstGet(stanzaName, idx)))
            continue;

        Variant *stanzaInfo = varNewKv();
        kvPut(varKv(stanzaInfo), varNewStr(strNew(INFO_STANZA_NAME)), varNewStr(strLstGet(stanzaName, idx)));

        varLstAdd(result, stanzaInfo);
    }

    FUNCTION_TEST_RESULT(VARIANT_LIST, result);
}

static String *
infoRender(void)
{
    FUNCTION_DEBUG_VOID(logLevelDebug);

    String *result = NULL;

    // Get stanza if specified
    const String *stanza = cfgOptionTest(cfgOptStanza) ? cfgOptionStr(cfgOptStanza) : NULL;

    VariantList *stanzaInfoList = stanzaInfo(stanza);

    // CSHANG I couldn't find the constants for the values - are there IDs or another way to check if text vs json?
    if (strEqZ(cfgOptionStr(cfgOptOutput), "text"))
    {
        // CSHANG call a formatText function and then if it returns NULL the following would be retunred?
        result = strNewFmt("No stanzas exist in %s\n", strPtr(storagePathNP(storageRepo(), NULL)));
    }
    else if (strEqZ(cfgOptionStr(cfgOptOutput), "json"))
    {
        result = varToJson(varNewVarLst(stanzaInfoList), 4);
    }
    else
    {
// CSHANG This should never happen since it will be caught by the command parser so should we just remove? Or is there a way to have this entire ELSE only as test code
        THROW_FMT(AssertError, "invalid info output option '%s'", cfgCommandName(cfgOptOutput));
    }

    FUNCTION_DEBUG_RESULT(STRING, result);
}
// /***********************************************************************************************************************************
// Helper function for helpRender() to make output look good on a console
// ***********************************************************************************************************************************/
// static String *
// helpRenderText(const String *text, size_t indent, bool indentFirst, size_t length)
// {
//     FUNCTION_DEBUG_BEGIN(logLevelTrace);
//         FUNCTION_DEBUG_PARAM(STRING, text);
//         FUNCTION_DEBUG_PARAM(SIZE, indent);
//         FUNCTION_DEBUG_PARAM(BOOL, indentFirst);
//         FUNCTION_DEBUG_PARAM(SIZE, length);
//
//         FUNCTION_DEBUG_ASSERT(text != NULL);
//         FUNCTION_DEBUG_ASSERT(length > 0);
//     FUNCTION_DEBUG_END();
//
//     String *result = strNew("");
//
//     // Split the text into paragraphs
//     StringList *lineList = strLstNewSplitZ(text, "\n");
//
//     // Iterate through each paragraph and split the lines according to the line length
//     for (unsigned int lineIdx = 0; lineIdx < strLstSize(lineList); lineIdx++)
//     {
//         // Add LF if there is already content
//         if (strSize(result) != 0)
//             strCat(result, "\n");
//
//         // Split the paragraph into lines that don't exceed the line length
//         StringList *partList = strLstNewSplitSizeZ(strLstGet(lineList, lineIdx), " ", length - indent);
//
//         for (unsigned int partIdx = 0; partIdx < strLstSize(partList); partIdx++)
//         {
//             // Indent when required
//             if (partIdx != 0 || indentFirst)
//             {
//                 if (partIdx != 0)
//                     strCat(result, "\n");
//
//                 if (strSize(strLstGet(partList, partIdx)))
//                     strCatFmt(result, "%*s", (int)indent, "");
//             }
//
//             // Add the line
//             strCat(result, strPtr(strLstGet(partList, partIdx)));
//         }
//     }
//
//     FUNCTION_DEBUG_RESULT(STRING, result);
// }
//
// /***********************************************************************************************************************************
// Helper function for helpRender() to output values as strings
// ***********************************************************************************************************************************/
// static String *
// helpRenderValue(const Variant *value)
// {
//     FUNCTION_DEBUG_BEGIN(logLevelTrace);
//         FUNCTION_DEBUG_PARAM(VARIANT, value);
//     FUNCTION_DEBUG_END();
//
//     String *result = NULL;
//
//     if (value != NULL)
//     {
//         if (varType(value) == varTypeBool)
//         {
//             if (varBool(value))
//                 result = strNew("y");
//             else
//                 result = strNew("n");
//         }
//         else if (varType(value) == varTypeKeyValue)
//         {
//             result = strNew("");
//
//             const KeyValue *optionKv = varKv(value);
//             const VariantList *keyList = kvKeyList(optionKv);
//
//             for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
//             {
//                 if (keyIdx != 0)
//                     strCat(result, ", ");
//
//                 strCatFmt(
//                     result, "%s=%s", strPtr(varStr(varLstGet(keyList, keyIdx))),
//                     strPtr(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx)))));
//             }
//         }
//         else if (varType(value) == varTypeVariantList)
//         {
//             result = strNew("");
//
//             const VariantList *list = varVarLst(value);
//
//             for (unsigned int listIdx = 0; listIdx < varLstSize(list); listIdx++)
//             {
//                 if (listIdx != 0)
//                     strCat(result, ", ");
//
//                 strCatFmt(result, "%s", strPtr(varStr(varLstGet(list, listIdx))));
//             }
//         }
//         else
//             result = varStrForce(value);
//     }
//
//     FUNCTION_DEBUG_RESULT(STRING, result);
// }
//
// /***********************************************************************************************************************************
// Render help to a string
// ***********************************************************************************************************************************/
// static String *
// helpRender(void)
// {
//     FUNCTION_DEBUG_VOID(logLevelDebug);
//
//     String *result = strNew(PGBACKREST_NAME " " PGBACKREST_VERSION);
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         // Message for more help when it is available
//         String *more = NULL;
//
//         // Display general help
//         if (cfgCommand() == cfgCmdHelp || cfgCommand() == cfgCmdNone)
//         {
//             strCat(
//                 result,
//                 " - General help\n"
//                 "\n"
//                 "Usage:\n"
//                 "    " PGBACKREST_BIN " [options] [command]\n"
//                 "\n"
//                 "Commands:\n");
//
//             // Find size of longest command name
//             size_t commandSizeMax = 0;
//
//             for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
//             {
//                 if (commandId == cfgCmdNone)
//                     continue;
//
//                 if (strlen(cfgCommandName(commandId)) > commandSizeMax)
//                     commandSizeMax = strlen(cfgCommandName(commandId));
//             }
//
//             // Output help for each command
//             for (ConfigCommand commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
//             {
//                 if (commandId == cfgCmdNone)
//                     continue;
//
//                 const char *helpSummary = cfgDefCommandHelpSummary(cfgCommandDefIdFromId(commandId));
//
//                 if (helpSummary != NULL)
//                 {
//                     strCatFmt(
//                         result, "    %s%*s%s\n", cfgCommandName(commandId),
//                         (int)(commandSizeMax - strlen(cfgCommandName(commandId)) + 2), "",
//                         strPtr(helpRenderText(strNew(helpSummary), commandSizeMax + 6, false, CONSOLE_WIDTH)));
//                 }
//             }
//
//             // Construct message for more help
//             more = strNew("[command]");
//         }
//         else
//         {
//             ConfigCommand commandId = cfgCommand();
//             ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(commandId);
//             const char *commandName = cfgCommandName(commandId);
//
//             // Output command part of title
//             strCatFmt(result, " - '%s' command", commandName);
//
//             // If no additional params then this is command help
//             if (strLstSize(cfgCommandParam()) == 0)
//             {
//                 // Output command summary and description
//                 strCatFmt(
//                     result,
//                     " help\n"
//                     "\n"
//                     "%s\n"
//                     "\n"
//                     "%s\n",
//                     strPtr(helpRenderText(strNew(cfgDefCommandHelpSummary(commandDefId)), 0, true, CONSOLE_WIDTH)),
//                     strPtr(helpRenderText(strNew(cfgDefCommandHelpDescription(commandDefId)), 0, true, CONSOLE_WIDTH)));
//
//                 // Construct key/value of sections and options
//                 KeyValue *optionKv = kvNew();
//                 size_t optionSizeMax = 0;
//
//                 for (unsigned int optionDefId = 0; optionDefId < cfgDefOptionTotal(); optionDefId++)
//                 {
//                     if (cfgDefOptionValid(commandDefId, optionDefId) && !cfgDefOptionInternal(commandDefId, optionDefId))
//                     {
//                         String *section = NULL;
//
//                         if (cfgDefOptionHelpSection(optionDefId) != NULL)
//                             section = strNew(cfgDefOptionHelpSection(optionDefId));
//
//                         if (section == NULL ||
//                             (!strEqZ(section, "general") && !strEqZ(section, "log") && !strEqZ(section, "repository") &&
//                              !strEqZ(section, "stanza")))
//                         {
//                             section = strNew("command");
//                         }
//
//                         kvAdd(optionKv, varNewStr(section), varNewInt((int)optionDefId));
//
//                         if (strlen(cfgDefOptionName(optionDefId)) > optionSizeMax)
//                             optionSizeMax = strlen(cfgDefOptionName(optionDefId));
//                     }
//                 }
//
//                 // Output sections
//                 StringList *sectionList = strLstSort(strLstNewVarLst(kvKeyList(optionKv)), sortOrderAsc);
//
//                 for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
//                 {
//                     const String *section = strLstGet(sectionList, sectionIdx);
//
//                     strCatFmt(result, "\n%s Options:\n\n", strPtr(strFirstUpper(strDup(section))));
//
//                     // Output options
//                     VariantList *optionList = kvGetList(optionKv, varNewStr(section));
//
//                     for (unsigned int optionIdx = 0; optionIdx < varLstSize(optionList); optionIdx++)
//                     {
//                         ConfigDefineOption optionDefId = varInt(varLstGet(optionList, optionIdx));
//                         ConfigOption optionId = cfgOptionIdFromDefId(optionDefId, 0);
//
//                         // Get option summary
//                         String *summary = strFirstLower(strNewN(
//                             cfgDefOptionHelpSummary(commandDefId, optionDefId),
//                             strlen(cfgDefOptionHelpSummary(commandDefId, optionDefId)) - 1));
//
//                         // Ouput current and default values if they exist
//                         String *defaultValue = helpRenderValue(cfgOptionDefault(optionId));
//                         String *value = NULL;
//
//                         if (cfgOptionSource(optionId) != cfgSourceDefault)
//                             value = helpRenderValue(cfgOption(optionId));
//
//                         if (value != NULL || defaultValue != NULL)
//                         {
//                             strCat(summary, " [");
//
//                             if (value != NULL)
//                                 strCatFmt(summary, "current=%s", strPtr(value));
//
//                             if (defaultValue != NULL)
//                             {
//                                 if (value != NULL)
//                                     strCat(summary, ", ");
//
//                                 strCatFmt(summary, "default=%s", strPtr(defaultValue));
//                             }
//
//                             strCat(summary, "]");
//                         }
//
//                         // Output option help
//                         strCatFmt(
//                             result, "  --%s%*s%s\n",
//                             cfgDefOptionName(optionDefId), (int)(optionSizeMax - strlen(cfgDefOptionName(optionDefId)) + 2), "",
//                             strPtr(helpRenderText(summary, optionSizeMax + 6, false, CONSOLE_WIDTH)));
//                     }
//                 }
//
//                 // Construct message for more help if there are options
//                 if (optionSizeMax > 0)
//                     more = strNewFmt("%s [option]", commandName);
//             }
//             // Else option help for the specified command
//             else
//             {
//                 // Make sure only one option was specified
//                 if (strLstSize(cfgCommandParam()) > 1)
//                     THROW(ParamInvalidError, "only one option allowed for option help");
//
//                 // Ensure the option is valid
//                 const char *optionName = strPtr(strLstGet(cfgCommandParam(), 0));
//                 ConfigOption optionId = cfgOptionId(optionName);
//
//                 if (cfgOptionId(optionName) == -1)
//                 {
//                     if (cfgDefOptionId(optionName) != -1)
//                         optionId = cfgOptionIdFromDefId(cfgDefOptionId(optionName), 0);
//                     else
//                         THROW_FMT(OptionInvalidError, "option '%s' is not valid for command '%s'", optionName, commandName);
//                 }
//
//                 // Output option summary and description
//                 ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);
//
//                 strCatFmt(
//                     result,
//                     " - '%s' option help\n"
//                     "\n"
//                     "%s\n"
//                     "\n"
//                     "%s\n",
//                     optionName,
//                     strPtr(helpRenderText(strNew(cfgDefOptionHelpSummary(commandDefId, optionDefId)), 0, true, CONSOLE_WIDTH)),
//                     strPtr(helpRenderText(strNew(cfgDefOptionHelpDescription(commandDefId, optionDefId)), 0, true, CONSOLE_WIDTH)));
//
//                 // Ouput current and default values if they exist
//                 String *defaultValue = helpRenderValue(cfgOptionDefault(optionId));
//                 String *value = NULL;
//
//                 if (cfgOptionSource(optionId) != cfgSourceDefault)
//                     value = helpRenderValue(cfgOption(optionId));
//
//                 if (value != NULL || defaultValue != NULL)
//                 {
//                     strCat(result, "\n");
//
//                     if (value != NULL)
//                         strCatFmt(result, "current: %s\n", strPtr(value));
//
//                     if (defaultValue != NULL)
//                         strCatFmt(result, "default: %s\n", strPtr(defaultValue));
//                 }
//
//                 // Output alternate names (call them deprecated so the user will know not to use them)
//                 if (cfgDefOptionHelpNameAlt(optionDefId))
//                 {
//                     strCat(result, "\ndeprecated name");
//
//                     if (cfgDefOptionHelpNameAltValueTotal(optionDefId) > 1) // {uncovered - no option has more than one alt name}
//                         strCat(result, "s");                                // {+uncovered}
//
//                     strCat(result, ": ");
//
//                     for (unsigned int nameAltIdx = 0; nameAltIdx < cfgDefOptionHelpNameAltValueTotal(optionDefId); nameAltIdx++)
//                     {
//                         if (nameAltIdx != 0)                                // {uncovered - no option has more than one alt name}
//                             strCat(result, ", ");                           // {+uncovered}
//
//                         strCat(result, cfgDefOptionHelpNameAltValue(optionDefId, nameAltIdx));
//                     }
//
//                     strCat(result, "\n");
//                 }
//             }
//         }
//
//         // If there is more help available output a message to let the user know
//         if (more != NULL)
//             strCatFmt(result, "\nUse '" PGBACKREST_BIN " help %s' for more information.\n", strPtr(more));
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_DEBUG_RESULT(STRING, result);
// }

/***********************************************************************************************************************************
Render info and output to stdout
***********************************************************************************************************************************/
void
cmdInfo(void)
{
// CSHANG How foes this work - I assume I would use this?
    FUNCTION_DEBUG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioHandleWriteOneStr(STDOUT_FILENO, infoRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}
