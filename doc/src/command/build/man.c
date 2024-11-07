/***********************************************************************************************************************************
Build Manual Page Reference
***********************************************************************************************************************************/
#include "build.auto.h"

#include "build/common/string.h"
#include "build/help/render.h"
#include "command/build/man.h"
#include "command/help/help.h"
#include "common/debug.h"
#include "common/log.h"
#include "version.h"

/***********************************************************************************************************************************
Define the console width - use a fixed width of 80 since this should be safe on virtually all consoles
***********************************************************************************************************************************/
#define CONSOLE_WIDTH                                               80

/***********************************************************************************************************************************
Basic variable replacement. This only handles a few cases but will error in case of unknown variables.
***********************************************************************************************************************************/
static String *
referenceManReplace(String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    String *const result = strCat(strNew(), string);

    strReplace(result, STRDEF("{[postgres]}"), STRDEF("PostgreSQL"));
    strReplace(result, STRDEF("{[project]}"), STRDEF("pgBackRest"));

    if (strstr(strZ(result), "{[") != NULL)
        THROW_FMT(AssertError, "unreplaced variable(s) in: %s", strZ(string));

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
typedef struct ReferenceManSectionData
{
    const String *name;                                             // Section name
    List *optList;                                                  // Option list
} ReferenceManSectionData;

// Helper to calculate max width
static size_t
referenceManMaxWidth(const size_t widthMax, const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, widthMax);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    if (strSize(string) > widthMax)
        FUNCTION_TEST_RETURN(SIZE, strSize(string));

    FUNCTION_TEST_RETURN(SIZE, widthMax);
}

String *
referenceManRender(const XmlNode *const indexRoot, const BldCfg *const bldCfg, const BldHlp *const bldHlp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(XML_NODE, indexRoot);
        FUNCTION_LOG_PARAM_P(VOID, bldCfg);
        FUNCTION_LOG_PARAM_P(VOID, bldHlp);
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Output header
        const String *const subtitle = referenceManReplace(xmlNodeAttribute(indexRoot, STRDEF("subtitle")));
        const String *const description = helpRenderText(
            referenceManReplace(xmlNodeContent(xmlNodeChild(indexRoot, STRDEF("description"), true))), false, false, 2, true,
            CONSOLE_WIDTH);

        strCatFmt(
            result,
            "NAME\n"
            "  " PROJECT_NAME " - %s\n\n"
            "SYNOPSIS\n"
            "  " PROJECT_BIN " [options] [command]\n\n"
            "DESCRIPTION\n"
            "%s\n",
            strZ(subtitle), strZ(description));

        // Output commands
        // -------------------------------------------------------------------------------------------------------------------------
        // Determine max command width
        size_t commandWidthMax = 0;

        for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldHlp->cmdList); cmdIdx++)
        {
            const BldHlpCommand *const cmdHlp = lstGet(bldHlp->cmdList, cmdIdx);
            const BldCfgCommand *const cmdCfg = lstFind(bldCfg->cmdList, &cmdHlp->name);
            ASSERT(cmdCfg != NULL);

            // Skip internal commands
            if (cmdCfg->internal)
                continue;

            // Update max command width
            commandWidthMax = referenceManMaxWidth(commandWidthMax, cmdHlp->name);
        }

        // Output commands
        strCatZ(result, "\nCOMMANDS\n");

        for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldHlp->cmdList); cmdIdx++)
        {
            const BldHlpCommand *const cmdHlp = lstGet(bldHlp->cmdList, cmdIdx);
            const BldCfgCommand *const cmdCfg = lstFind(bldCfg->cmdList, &cmdHlp->name);
            ASSERT(cmdCfg != NULL);

            // Skip internal commands
            if (cmdCfg->internal)
                continue;

            strCatFmt(
                result, "  %s  %*s%s\n", strZ(cmdHlp->name),
                (int)(commandWidthMax - strSize(cmdHlp->name)), "",
                strZ(helpRenderText(bldHlpRenderXml(cmdHlp->summary), false, false, commandWidthMax + 4, false, CONSOLE_WIDTH)));
        }

        // Output options
        // -------------------------------------------------------------------------------------------------------------------------
        // Determine max option width and build list of sections/options
        List *const sectionList = lstNewP(sizeof(ReferenceManSectionData), .comparator = lstComparatorStr);
        size_t optionWidthMax = 0;

        for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldHlp->cmdList); cmdIdx++)
        {
            const BldHlpCommand *const cmdHlp = lstGet(bldHlp->cmdList, cmdIdx);
            const BldCfgCommand *const cmdCfg = lstFind(bldCfg->cmdList, &cmdHlp->name);
            ASSERT(cmdCfg != NULL);

            // Skip internal commands
            if (cmdCfg->internal)
                continue;

            if (cmdHlp->optList != NULL)
            {
                // Get section name
                const String *const sectionName = strFirstUpper(strDup(cmdHlp->name));

                for (unsigned int optIdx = 0; optIdx < lstSize(cmdHlp->optList); optIdx++)
                {
                    const BldHlpOption *const optHlp = lstGet(cmdHlp->optList, optIdx);
                    const BldCfgOption *const optCfg = lstFind(bldCfg->optList, &optHlp->name);
                    ASSERT(optCfg != NULL);

                    // Skip internal options
                    const BldCfgOptionCommand *const optCmdCfg = lstFind(optCfg->cmdList, &cmdHlp->name);
                    ASSERT(optCmdCfg != NULL);

                    if (optCmdCfg->internal)
                        continue;

                    // Update max option width
                    optionWidthMax = referenceManMaxWidth(optionWidthMax, optHlp->name);

                    // Add/get section
                    ReferenceManSectionData *section = lstFind(sectionList, &sectionName);

                    if (section == NULL)
                    {
                        MEM_CONTEXT_OBJ_BEGIN(sectionList)
                        {
                            const ReferenceManSectionData sectionNew =
                            {
                                .name = strDup(sectionName),
                                .optList = lstNewP(sizeof(BldHlpOption), .comparator = lstComparatorStr),
                            };

                            section = lstAdd(sectionList, &sectionNew);
                        }
                        MEM_CONTEXT_OBJ_END();
                    }

                    // Add option
                    lstAdd(section->optList, optHlp);
                }
            }
        }

        for (unsigned int optIdx = 0; optIdx < lstSize(bldHlp->optList); optIdx++)
        {
            const BldHlpOption *const optHlp = lstGet(bldHlp->optList, optIdx);
            const BldCfgOption *const optCfg = lstFind(bldCfg->optList, &optHlp->name);
            ASSERT(optCfg != NULL);

            // Skip if option is internal
            if (optCfg->internal)
                continue;

            // Update max option width
            optionWidthMax = referenceManMaxWidth(optionWidthMax, optHlp->name);

            // Get section name
            const String *const sectionName = optHlp->section == NULL ? STRDEF("General") : strFirstUpper(strDup(optHlp->section));

            // Add/get section
            ReferenceManSectionData *section = lstFind(sectionList, &sectionName);

            if (section == NULL)
            {
                MEM_CONTEXT_OBJ_BEGIN(sectionList)
                {
                    const ReferenceManSectionData sectionNew =
                    {
                        .name = strDup(sectionName),
                        .optList = lstNewP(sizeof(BldHlpOption), .comparator = lstComparatorStr),
                    };

                    section = lstAdd(sectionList, &sectionNew);
                }
                MEM_CONTEXT_OBJ_END();
            }

            // Add option
            lstAdd(section->optList, optHlp);
            lstSort(section->optList, sortOrderAsc);
        }

        lstSort(sectionList, sortOrderAsc);

        // Output options
        strCatZ(result, "\nOPTIONS\n");

        for (unsigned int sectionIdx = 0; sectionIdx < lstSize(sectionList); sectionIdx++)
        {
            const ReferenceManSectionData *const section = lstGet(sectionList, sectionIdx);

            if (sectionIdx != 0)
                strCatZ(result, "\n");

            strCatFmt(result, "  %s Options:\n", strZ(section->name));

            for (unsigned int optIdx = 0; optIdx < lstSize(section->optList); optIdx++)
            {
                const BldHlpOption *const optHlp = lstGet(section->optList, optIdx);

                strCatFmt(
                    result, "    --%s  %*s%s\n", strZ(optHlp->name),
                    (int)(optionWidthMax - strSize(optHlp->name)), "",
                    strZ(helpRenderText(bldHlpRenderXml(optHlp->summary), false, false, optionWidthMax + 8, false, CONSOLE_WIDTH)));
            }
        }

        // Output files
        // -------------------------------------------------------------------------------------------------------------------------
        strCatZ(
            result,
            "\nFILES\n"
            "  " CFGOPTDEF_CONFIG_PATH "/" PROJECT_CONFIG_FILE "\n"
            "  /var/lib/pgbackrest\n"
            "  /var/log/pgbackrest\n"
            "  /var/spool/pgbackrest\n"
            "  /tmp/pgbackrest\n");

        // Output examples
        // -------------------------------------------------------------------------------------------------------------------------
        strCatZ(
            result,
            "\nEXAMPLES\n"
            "  * Create a backup of the PostgreSQL `main` cluster:\n"
            "\n"
            "    $ pgbackrest --stanza=main backup\n"
            "\n"
            "    The `main` cluster should be configured in `" CFGOPTDEF_CONFIG_PATH "/" PROJECT_CONFIG_FILE "`\n"
            "\n"
            "  * Show all available backups:\n"
            "\n"
            "    $ pgbackrest info\n"
            "\n"
            "  * Show all available backups for a specific cluster:\n"
            "\n"
            "    $ pgbackrest --stanza=main info\n"
            "\n"
            "  * Show backup specific options:\n"
            "\n"
            "    $ pgbackrest help backup\n");

        // Output see also
        // -------------------------------------------------------------------------------------------------------------------------
        strCatZ(
            result,
            "\nSEE ALSO\n"
            "  /usr/share/doc/pgbackrest-doc/html/index.html\n"
            "  http://www.pgbackrest.org\n");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
