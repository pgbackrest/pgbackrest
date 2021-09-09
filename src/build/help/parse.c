/***********************************************************************************************************************************
Parse Help Xml
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/type/xml.h"
#include "storage/storage.h"

#include "build/config/parse.h"
#include "build/help/parse.h"

/***********************************************************************************************************************************
Parse option help
***********************************************************************************************************************************/
// Helper to parse options
static void
bldHlpParseOption(XmlNodeList *const xmlOptList, List *const optList, const String *const sectionDefault)
{
    ASSERT(xmlOptList != NULL);
    ASSERT(optList != NULL);

    for (unsigned int optIdx = 0; optIdx < xmlNodeLstSize(xmlOptList); optIdx++)
    {
        const XmlNode *const xmlOpt = xmlNodeLstGet(xmlOptList, optIdx);

        // Get section or use default
        const String *section = xmlNodeAttribute(xmlOpt, STRDEF("section"));

        if (section == NULL)
            section = sectionDefault;

        // Add option to list
        MEM_CONTEXT_BEGIN(lstMemContext(optList))
        {
            lstAdd(
                optList,
                &(BldHlpOption)
                {
                    .name = xmlNodeAttribute(xmlOpt, STRDEF("id")),
                    .section = strDup(section),
                    .summary = xmlNodeChild(xmlOpt, STRDEF("summary"), true),
                    .description = xmlNodeChild(xmlOpt, STRDEF("text"), true),
                });
        }
        MEM_CONTEXT_END();
    }

    lstSort(optList, sortOrderAsc);
}

static List *
bldHlpParseOptionList(XmlNode *const xml)
{
    List *const result = lstNewP(sizeof(BldHlpOption), .comparator = lstComparatorStr);

    // Parse config options
    const XmlNodeList *xmlSectionList = xmlNodeChildList(
        xmlNodeChild(xmlNodeChild(xml, STRDEF("config"), true), STRDEF("config-section-list"), true), STRDEF("config-section"));

    for (unsigned int sectionIdx = 0; sectionIdx < xmlNodeLstSize(xmlSectionList); sectionIdx++)
    {
        const XmlNode *const xmlSection = xmlNodeLstGet(xmlSectionList, sectionIdx);

        bldHlpParseOption(
            xmlNodeChildList(xmlNodeChild(xmlSection, STRDEF("config-key-list"), true), STRDEF("config-key")), result,
            xmlNodeAttribute(xmlSection, STRDEF("id")));
    }

    // Parse command-line only options
    bldHlpParseOption(
        xmlNodeChildList(
            xmlNodeChild(
                xmlNodeChild(xmlNodeChild(xml, STRDEF("operation"), true), STRDEF("operation-general"), true),
                STRDEF("option-list"), true),
            STRDEF("option")),
        result, NULL);

    return result;
}

/***********************************************************************************************************************************
Parse command help
***********************************************************************************************************************************/
static List *
bldHlpParseCommandList(XmlNode *const xml)
{
    List *const result = lstNewP(sizeof(BldHlpCommand), .comparator = lstComparatorStr);

    // Parse commands
    const XmlNodeList *xmlCmdList = xmlNodeChildList(xml, STRDEF("command"));

    for (unsigned int cmdIdx = 0; cmdIdx < xmlNodeLstSize(xmlCmdList); cmdIdx++)
    {
        const XmlNode *const xmlCmd = xmlNodeLstGet(xmlCmdList, cmdIdx);

        // Parse option list if any
        List *cmdOptList = NULL;
        const XmlNode *const xmlCmdOptListParent = xmlNodeChild(xmlCmd, STRDEF("option-list"), false);

        if (xmlCmdOptListParent != NULL)
        {
            cmdOptList = lstNewP(sizeof(BldHlpOption), .comparator = lstComparatorStr);

            bldHlpParseOption(xmlNodeChildList(xmlCmdOptListParent, STRDEF("option")), cmdOptList, NULL);
        }

        // Add command to list
        MEM_CONTEXT_BEGIN(lstMemContext(result))
        {
            lstAdd(
                result,
                &(BldHlpCommand)
                {
                    .name = xmlNodeAttribute(xmlCmd, STRDEF("id")),
                    .summary = xmlNodeChild(xmlCmd, STRDEF("summary"), true),
                    .description = xmlNodeChild(xmlCmd, STRDEF("text"), true),
                    .optList = lstMove(cmdOptList, memContextCurrent()),
                });
        }
        MEM_CONTEXT_END();
    }

    lstSort(result, sortOrderAsc);

    return result;
}

/***********************************************************************************************************************************
Reconcile help
***********************************************************************************************************************************/
static void
bldHlpValidate(const BldHlp bldHlp, const BldCfg bldCfg)
{
    // Validate command help
    for (unsigned int cmdIdx = 0; cmdIdx < lstSize(bldCfg.cmdList); cmdIdx++)
    {
        const BldCfgCommand *const cmd = lstGet(bldCfg.cmdList, cmdIdx);
        const BldHlpCommand *const cmdHlp = lstFind(bldHlp.cmdList, &cmd->name);

        if (cmdHlp == NULL)
            THROW_FMT(FormatError, "command '%s' must have help", strZ(cmd->name));
    }

    // Validate option help
    for (unsigned int optIdx = 0; optIdx < lstSize(bldCfg.optList); optIdx++)
    {
        const BldCfgOption *const opt = lstGet(bldCfg.optList, optIdx);
        const BldHlpOption *const optHlp = lstFind(bldHlp.optList, &opt->name);

        // If help was not found in general command-line or config options then check command overrides
        if (optHlp == NULL)
        {
            for (unsigned int optCmdListIdx = 0; optCmdListIdx < lstSize(opt->cmdList); optCmdListIdx++)
            {
                const BldCfgOptionCommand *const optCmd = lstGet(opt->cmdList, optCmdListIdx);
                const BldHlpCommand *const cmdHlp = lstFind(bldHlp.cmdList, &optCmd->name);
                CHECK(cmdHlp != NULL);

                // Only options with a command role of main require help
                if (!strLstExists(optCmd->roleList, CMD_ROLE_MAIN_STR))
                    continue;

                const BldHlpOption *const cmdOptHlp = cmdHlp->optList != NULL ? lstFind(cmdHlp->optList, &opt->name) : NULL;

                if (cmdOptHlp == NULL)
                    THROW_FMT(FormatError, "option '%s' must have help for command '%s'", strZ(opt->name), strZ(optCmd->name));
            }
        }
    }
}

/**********************************************************************************************************************************/
BldHlp
bldHlpParse(const Storage *const storageRepo, const BldCfg bldCfg)
{
    // Initialize xml
    XmlNode *const xml = xmlDocumentRoot(
        xmlDocumentNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("src/build/help/help.xml")))));

    // Parse help
    BldHlp result =
    {
        .cmdList = bldHlpParseCommandList(
            xmlNodeChild(xmlNodeChild(xml, STRDEF("operation"), true), STRDEF("command-list"), true)),
        .optList = bldHlpParseOptionList(xml)
    };

    bldHlpValidate(result, bldCfg);

    return result;
}
