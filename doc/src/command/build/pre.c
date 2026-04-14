/***********************************************************************************************************************************
Build Preprocessor
***********************************************************************************************************************************/
#include <build.h>

#include "build/common/string.h"
#include "command/build/eval.h"
#include "command/build/pre.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Block define
***********************************************************************************************************************************/
typedef struct BuildBlockDefine
{
    const String *id;                                               // Block id
    const XmlNode *block;                                           // Block xml
} BuildBlockDefine;

/**********************************************************************************************************************************/
static void
buildPreRecurse(
    XmlNode *const xml, const BldCfg *bldCfg, const BldHlp *bldHlp, List *blockDefineList, VarStore *const varStore)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(XML_DOCUMENT, xml);
        FUNCTION_LOG_PARAM_P(VOID, bldCfg);
        FUNCTION_LOG_PARAM_P(VOID, bldHlp);
        FUNCTION_LOG_PARAM(LIST, blockDefineList);
        FUNCTION_LOG_PARAM(VAR_STORE, varStore);
    FUNCTION_LOG_END();

    ASSERT(xml != NULL);
    ASSERT(bldCfg != NULL);
    ASSERT(bldHlp != NULL);
    ASSERT(blockDefineList != NULL);
    ASSERT(varStore != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Iterate all nodes
        const XmlNodeList *const nodeList = xmlNodeChildListAll(xml, false);

        for (unsigned int nodeIdx = 0; nodeIdx < xmlNodeLstSize(nodeList); nodeIdx++)
        {
            XmlNode *const node = xmlNodeLstGet(nodeList, nodeIdx);

            // Evaluate if expressions
            const String *const ifExpr = xmlNodeAttribute(node, STRDEF("if"));

            if (ifExpr != NULL)
            {
                // If expression is true then remove if attribute
                if (eval(varStoreReplaceStr(varStore, ifExpr)))
                {
                    xmlNodeAttributeRemove(node, STRDEF("if"));
                }
                // Else remove node
                else
                {
                    xmlNodeRemove(node);
                    continue;
                }
            }

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
            // Store variable
            else if (strEqZ(xmlNodeName(node), "variable"))
            {
                const String *const variable = xmlNodeAttribute(node, STRDEF("key"));
                CHECK(FormatError, variable != NULL, "key is missing");
                const String *const value = varStoreReplaceStr(varStore, xmlNodeContent(node));
                CHECK(FormatError, value != NULL, "value is missing");

                varStoreAdd(varStore, variable, value);
            }
            // Block define
            else if (strEqZ(xmlNodeName(node), "block-define"))
            {
                const String *const id = xmlNodeAttribute(node, STRDEF("id"));
                CHECK(FormatError, id != NULL, "block id is missing");
                CHECK_FMT(FormatError, lstFind(blockDefineList, &id) == NULL, "block '%s' is already defined", strZ(id));

                MEM_CONTEXT_BEGIN(lstMemContext(blockDefineList))
                {
                    const BuildBlockDefine blockDefine =
                    {
                        .id = id,
                        .block = xmlNodeDup(node),
                    };

                    lstAdd(blockDefineList, &blockDefine);
                }
                MEM_CONTEXT_END();

                xmlNodeRemove(node);
            }
            // Block replace
            else if (strEqZ(xmlNodeName(node), "block"))
            {
                const String *const id = xmlNodeAttribute(node, STRDEF("id"));
                CHECK(FormatError, id != NULL, "block id is missing");
                const BuildBlockDefine *const blockDefine = lstFind(blockDefineList, &id);
                CHECK_FMT(FormatError, blockDefine != NULL, "block '%s' does not exist", strZ(id));

                // Get variable replacements
                VarStore *const varBlockStore = varStoreNew();
                const XmlNodeList *const varReplaceList = xmlNodeChildList(node, STRDEF("block-variable-replace"));

                for (unsigned int varReplaceIdx = 0; varReplaceIdx < xmlNodeLstSize(varReplaceList); varReplaceIdx++)
                {
                    const XmlNode *const varReplace = xmlNodeLstGet(varReplaceList, varReplaceIdx);
                    const String *const variable = xmlNodeAttribute(varReplace, STRDEF("key"));
                    CHECK(FormatError, variable != NULL, "key is missing");
                    const String *const value = xmlNodeContent(varReplace);
                    CHECK(FormatError, value != NULL, "value is missing");

                    varStoreAdd(varBlockStore, variable, value);
                }

                // Copy node, do variable replacement, recurse, and replace in document
                XmlNode *const block = xmlNodeDup(blockDefine->block);
                varStoreReplaceNode(varBlockStore, block);
                buildPreRecurse(block, bldCfg, bldHlp, blockDefineList, varStore);
                xmlNodeChildReplace(node, block);
            }
            // Else recurse
            else
                buildPreRecurse(node, bldCfg, bldHlp, blockDefineList, varStore);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
XmlDocument *
buildPre(XmlDocument *const xml, const BldCfg *bldCfg, const BldHlp *bldHlp, VarStore *const varStore)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(XML_DOCUMENT, xml);
        FUNCTION_LOG_PARAM_P(VOID, bldCfg);
        FUNCTION_LOG_PARAM_P(VOID, bldHlp);
        FUNCTION_LOG_PARAM(VAR_STORE, varStore);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        List *const blockDefineList = lstNewP(sizeof(BuildBlockDefine), .comparator = lstComparatorStr);

        buildPreRecurse(xmlDocumentRoot(xml), bldCfg, bldHlp, blockDefineList, varStore);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(XML_DOCUMENT, xml);
}
