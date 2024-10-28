/***********************************************************************************************************************************
Build Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "build/config/parse.h"
#include "build/help/parse.h"
#include "command/build/build.h"
#include "command/build/reference.h"
#include "common/debug.h"
#include "common/log.h"
#include "storage/posix/storage.h"
#include "storage/storage.h"
#include "version.h"

/***********************************************************************************************************************************
Build option
***********************************************************************************************************************************/
static void
referenceOptionRender(
    XmlNode *const xmlSection, const BldCfgOptionCommand *const optCmdCfg, const BldCfgOption *const optCfg,
    const BldHlpOption *const optHlp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(XML_NODE, xmlSection);
        FUNCTION_LOG_PARAM_P(VOID, optCmdCfg);
        FUNCTION_LOG_PARAM_P(VOID, optCfg);
        FUNCTION_LOG_PARAM_P(VOID, optHlp);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        XmlNode *const xmlOption = xmlNodeAdd(xmlSection, STRDEF("section"));
        XmlNode *const xmlOptionTitle = xmlNodeAdd(xmlOption, STRDEF("title"));

        xmlNodeAttributeSet(xmlOption, STRDEF("id"), strNewFmt("option-%s", strZ(optHlp->name)));
        xmlNodeContentSet(xmlOptionTitle, strNewFmt("%s Option (", strZ(optHlp->title)));
        xmlNodeContentSet(xmlNodeAdd(xmlOptionTitle, STRDEF("id")), strNewFmt("--%s", strZ(optHlp->name)));
        xmlNodeContentSet(xmlOptionTitle, STRDEF(")"));
        xmlNodeChildAdd(xmlNodeAdd(xmlOption, STRDEF("p")), optHlp->summary);

        if (optCfg->beta)
            xmlNodeContentSet(xmlNodeAdd(xmlOption, STRDEF("p")), STRDEF("FOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION."));

        xmlNodeChildAdd(xmlOption, optHlp->description);

        // Add default value
        StringList *const blockList = strLstNew();
        const String *const defaultValue =
            optCmdCfg != NULL && optCmdCfg->defaultValue != NULL ? optCmdCfg->defaultValue : optCfg->defaultValue;

        if (defaultValue != NULL)
        {
            if (strEq(optCfg->type, OPT_TYPE_BOOLEAN_STR))
                strLstAddFmt(blockList, "default: %s", strEqZ(defaultValue, "true") ? "y" : "n");
            else
                strLstAddFmt(blockList, "default: %s", strZ(defaultValue));
        }

        // Add allow range
        if (optCfg->allowRangeMin != NULL)
        {
            ASSERT(optCfg->allowRangeMax != NULL);
            strLstAddFmt(blockList, "allowed: %s-%s", strZ(optCfg->allowRangeMin), strZ(optCfg->allowRangeMax));
        }

        // Add examples
        if (optHlp->exampleList != NULL)
        {
            String *output = strCatZ(strNew(), "example: ");
            const String *const option =
                optCfg->group != NULL ?
                    strNewFmt("%s1%s", strZ(optCfg->group), strZ(strSub(optCfg->name, strSize(optCfg->group)))) : optCfg->name;

            for (unsigned int exampleIdx = 0; exampleIdx < strLstSize(optHlp->exampleList); exampleIdx++)
            {
                const String *const example = strLstGet(optHlp->exampleList, exampleIdx);

                // If Command line example
                if (optCmdCfg != NULL)
                {
                    if (exampleIdx != 0)
                        strCatZ(output, " ");

                    strCatZ(output, "--");

                    if (strEq(optCfg->type, OPT_TYPE_BOOLEAN_STR) && strEqZ(example, "n"))
                        strCatZ(output, "no-");
                }
                // Else configuration file example
                else if (exampleIdx != 0)
                {
                    strLstAdd(blockList, output);
                    output = strCatZ(strNew(), "example: ");
                }

                strCat(output, option);

                if (optCmdCfg == NULL || !strEq(optCfg->type, OPT_TYPE_BOOLEAN_STR))
                    strCatFmt(output, "=%s", strZ(example));
            }

            strLstAdd(blockList, output);
        }

        if (!strLstEmpty(blockList))
            xmlNodeContentSet(xmlNodeAdd(xmlOption, STRDEF("code-block")), strLstJoin(blockList, "\n"));

        // Add deprecated names
        if (optCfg->deprecateList != NULL)
        {
            String *const deprecateStr = strNew();

            for (unsigned int deprecateIdx = 0; deprecateIdx < lstSize(optCfg->deprecateList); deprecateIdx++)
            {
                const BldCfgOptionDeprecate *const deprecate = lstGet(optCfg->deprecateList, deprecateIdx);

                if (!strEq(deprecate->name, optCfg->name))
                    strCatFmt(deprecateStr, "%s %s", !strEmpty(deprecateStr) ? "," : "", strZ(deprecate->name));
            }

            if (!strEmpty(deprecateStr))
            {
                xmlNodeContentSet(
                    xmlNodeAdd(xmlOption, STRDEF("p")),
                    strNewFmt("Deprecated Name%s:%s", lstSize(optCfg->deprecateList) > 1 ? "s" : "", strZ(deprecateStr)));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
XmlDocument *
referenceConfigurationRender(const BldCfg *const bldCfg, const BldHlp *const bldHlp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, bldCfg);
        FUNCTION_LOG_PARAM_P(VOID, bldHlp);
    FUNCTION_LOG_END();

    XmlDocument *const result = xmlDocumentNewP(STRDEF("doc"), .dtdName = STRDEF("doc"), .dtdFile = STRDEF("doc.dtd"));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        XmlNode *const xmlRoot = xmlDocumentRoot(result);

        // Set attributes in root node
        xmlNodeAttributeSet(xmlRoot, STRDEF("title"), STRDEF("{[project]}"));
        xmlNodeAttributeSet(xmlRoot, STRDEF("subtitle"), bldHlp->optTitle);
        xmlNodeAttributeSet(xmlRoot, STRDEF("toc"), STRDEF("y"));

        // Set description
        xmlNodeContentSet(xmlNodeAdd(xmlRoot, STRDEF("description")), bldHlp->optDescription);

        // Set introduction
        XmlNode *const xmlIntro = xmlNodeAdd(xmlRoot, STRDEF("section"));

        xmlNodeAttributeSet(xmlIntro, STRDEF("id"), STRDEF("introduction"));
        xmlNodeContentSet(xmlNodeAdd(xmlIntro, STRDEF("title")), STRDEF("Introduction"));
        xmlNodeChildAdd(xmlIntro, bldHlp->optIntroduction);

        for (unsigned int sctIdx = 0; sctIdx < lstSize(bldHlp->sctList); sctIdx++)
        {
            const BldHlpSection *const section = lstGet(bldHlp->sctList, sctIdx);
            XmlNode *const xmlSection = xmlNodeAdd(xmlRoot, STRDEF("section"));
            XmlNode *const xmlSectionTitle = xmlNodeAdd(xmlSection, STRDEF("title"));

            xmlNodeAttributeSet(xmlSection, STRDEF("id"), strNewFmt("section-%s", strZ(section->id)));
            xmlNodeContentSet(xmlSectionTitle, strNewFmt("%s Options", strZ(section->name)));
            xmlNodeChildAdd(xmlSection, section->introduction);

            for (unsigned int optIdx = 0; optIdx < lstSize(bldHlp->optList); optIdx++)
            {
                const BldHlpOption *const optHlp = lstGet(bldHlp->optList, optIdx);
                const BldCfgOption *const optCfg = lstFind(bldCfg->optList, &optHlp->name);
                ASSERT(optCfg != NULL);

                // Skip if option does not belong in this section
                if (!strEq(optHlp->section == NULL ? STRDEF("general") : optHlp->section, section->id))
                    continue;

                // Skip if option is command-line only
                if (strEq(optCfg->section, SECTION_COMMAND_LINE_STR))
                    continue;

                // Skip if option is internal
                if (optCfg->internal)
                    continue;

                referenceOptionRender(xmlSection, NULL, optCfg, optHlp);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(XML_DOCUMENT, result);
}

/**********************************************************************************************************************************/
// Helper to remap section names for the command reference
static String *
referenceCommandSection(const String *const section)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, section);
    FUNCTION_TEST_END();

    if (section == NULL)
        FUNCTION_TEST_RETURN(STRING, strNewZ("general"));

    if (!strEqZ(section, "general") && !strEqZ(section, "log") && !strEqZ(section, "maintainer") &&
        !strEqZ(section, "repository") && !strEqZ(section, "stanza"))
    {
        FUNCTION_TEST_RETURN(STRING, strNewZ("command"));
    }

    FUNCTION_TEST_RETURN(STRING, strDup(section));
}

XmlDocument *
referenceCommandRender(const BldCfg *const bldCfg, const BldHlp *const bldHlp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, bldCfg);
        FUNCTION_LOG_PARAM_P(VOID, bldHlp);
    FUNCTION_LOG_END();

    XmlDocument *const result = xmlDocumentNewP(STRDEF("doc"), .dtdName = STRDEF("doc"), .dtdFile = STRDEF("doc.dtd"));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        XmlNode *const xmlRoot = xmlDocumentRoot(result);

        // Set attributes in root node
        xmlNodeAttributeSet(xmlRoot, STRDEF("title"), STRDEF("{[project]}"));
        xmlNodeAttributeSet(xmlRoot, STRDEF("subtitle"), bldHlp->cmdTitle);
        xmlNodeAttributeSet(xmlRoot, STRDEF("toc"), STRDEF("y"));

        // Set description
        xmlNodeContentSet(xmlNodeAdd(xmlRoot, STRDEF("description")), bldHlp->cmdDescription);

        // Set introduction
        XmlNode *const xmlIntro = xmlNodeAdd(xmlRoot, STRDEF("section"));

        xmlNodeAttributeSet(xmlIntro, STRDEF("id"), STRDEF("introduction"));
        xmlNodeContentSet(xmlNodeAdd(xmlIntro, STRDEF("title")), STRDEF("Introduction"));
        xmlNodeChildAdd(xmlIntro, bldHlp->cmdIntroduction);

        for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldHlp->cmdList); cmdIdx++)
        {
            const BldHlpCommand *const cmdHlp = lstGet(bldHlp->cmdList, cmdIdx);
            const BldCfgCommand *const cmdCfg = lstFind(bldCfg->cmdList, &cmdHlp->name);
            ASSERT(cmdCfg != NULL);

            // Skip internal commands
            if (cmdCfg->internal)
                continue;

            XmlNode *const xmlSection = xmlNodeAdd(xmlRoot, STRDEF("section"));
            XmlNode *const xmlSectionTitle = xmlNodeAdd(xmlSection, STRDEF("title"));

            xmlNodeAttributeSet(xmlSection, STRDEF("id"), strNewFmt("command-%s", strZ(cmdHlp->name)));
            xmlNodeContentSet(xmlSectionTitle, strNewFmt("%s Command (", strZ(cmdHlp->title)));
            xmlNodeContentSet(xmlNodeAdd(xmlSectionTitle, STRDEF("id")), cmdHlp->name);
            xmlNodeContentSet(xmlSectionTitle, STRDEF(")"));
            xmlNodeChildAdd(xmlSection, cmdHlp->description);

            // Build option list for command
            StringList *const sctList = strLstNew();
            List *const optCfgList = lstNewP(sizeof(BldCfgOption));
            List *const optHlpList = lstNewP(sizeof(BldHlpOption));

            for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg->optList); optIdx++)
            {
                const BldCfgOption *const optCfg = lstGet(bldCfg->optList, optIdx);

                // Skip internal options
                if (optCfg->internal || optCfg->secure)
                    continue;

                // Skip options that are not valid, internal, or do not have the main role for this command
                const BldCfgOptionCommand *const optCmdCfg = lstFind(optCfg->cmdList, &cmdCfg->name);

                if (optCmdCfg == NULL || optCmdCfg->internal || !strLstExists(optCmdCfg->roleList, STRDEF("main")))
                    continue;

                // Get help from command list or general list
                BldHlpOption optHlp = {0};

                if (cmdHlp->optList != NULL && lstFind(cmdHlp->optList, &optCfg->name) != NULL)
                {
                    optHlp = *((const BldHlpOption *)lstFind(cmdHlp->optList, &optCfg->name));
                    optHlp.section = strNewZ("command");
                }
                else
                {
                    ASSERT(lstFind(bldHlp->optList, &optCfg->name) != NULL);
                    optHlp = *((const BldHlpOption *)lstFind(bldHlp->optList, &optCfg->name));
                }

                // Remap section name
                optHlp.section = referenceCommandSection(optHlp.section);

                // Add sections
                strLstAddIfMissing(sctList, optHlp.section);

                // Add options
                lstAdd(optCfgList, optCfg);
                lstAdd(optHlpList, &optHlp);
            }

            strLstSort(sctList, sortOrderAsc);

            for (unsigned int sctIdx = 0; sctIdx < strLstSize(sctList); sctIdx++)
            {
                const String *const section = strLstGet(sctList, sctIdx);
                XmlNode *const xmlCommandCategory = xmlNodeAdd(xmlSection, STRDEF("section"));
                XmlNode *const xmlCommandCategoryTitle = xmlNodeAdd(xmlCommandCategory, STRDEF("title"));

                xmlNodeAttributeSet(xmlCommandCategory, STRDEF("id"), strNewFmt("category-%s", strZ(section)));
                xmlNodeAttributeSet(xmlCommandCategory, STRDEF("toc"), STRDEF("n"));
                xmlNodeContentSet(xmlCommandCategoryTitle, strNewFmt("%s Options", strZ(strFirstUpper(strDup(section)))));

                for (unsigned int optIdx = 0; optIdx < lstSize(optHlpList); optIdx++)
                {
                    const BldHlpOption *const optHlp = lstGet(optHlpList, optIdx);
                    const BldCfgOption *const optCfg = lstGet(optCfgList, optIdx);
                    ASSERT(optCfg != NULL);
                    const BldCfgOptionCommand *const optCmdCfg = lstFind(optCfg->cmdList, &cmdCfg->name);
                    ASSERT(optCmdCfg != NULL);

                    if (!strEq(optHlp->section, section))
                        continue;

                    referenceOptionRender(xmlCommandCategory, optCmdCfg, optCfg, optHlp);
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(XML_DOCUMENT, result);
}
