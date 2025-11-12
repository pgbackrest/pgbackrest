/***********************************************************************************************************************************
Build Preprocessor
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/build/pre.h"
#include "common/debug.h"
#include "common/log.h"

/**********************************************************************************************************************************/
static void
buildPreRecurse(XmlNode *const xml, const BldCfg *bldCfg, const BldHlp *bldHlp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(XML_DOCUMENT, xml);
        FUNCTION_LOG_PARAM_P(VOID, bldCfg);
        FUNCTION_LOG_PARAM_P(VOID, bldHlp);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Iterate all nodes
        const XmlNodeList *const nodeList = xmlNodeChildListAll(xml);

        for (unsigned int nodeIdx = 0; nodeIdx < xmlNodeLstSize(nodeList); nodeIdx++)
        {
            XmlNode *const node = xmlNodeLstGet(nodeList, nodeIdx);

            // Replace option description
            if (strEqZ(xmlNodeName(node), "option-description"))
            {
                const String *const key = xmlNodeAttribute(node, STRDEF("key"));
                ASSERT(key != NULL);
                const BldHlpOption *const hlpOpt = lstFind(bldHlp->optList, &key);
                ASSERT(hlpOpt != NULL);

                xmlNodeChildReplace(node, hlpOpt->description);
            }
            // Replace command description
            else if (strEqZ(xmlNodeName(node), "cmd-description"))
            {
                const String *const key = xmlNodeAttribute(node, STRDEF("key"));
                ASSERT(key != NULL);
                const BldHlpCommand *const hlpCmd = lstFind(bldHlp->cmdList, &key);
                ASSERT(hlpCmd != NULL);

                xmlNodeChildReplace(node, hlpCmd->description);
            }
            // Else recurse
            else
                buildPreRecurse(node, bldCfg, bldHlp);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
XmlDocument *
buildPre(XmlDocument *const xml, const BldCfg *bldCfg, const BldHlp *bldHlp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(XML_DOCUMENT, xml);
        FUNCTION_LOG_PARAM_P(VOID, bldCfg);
        FUNCTION_LOG_PARAM_P(VOID, bldHlp);
    FUNCTION_LOG_END();

    buildPreRecurse(xmlDocumentRoot(xml), bldCfg, bldHlp);

    FUNCTION_LOG_RETURN(XML_DOCUMENT, xml);
}
