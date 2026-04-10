/***********************************************************************************************************************************
Variable Store
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>

#include "build/common/string.h"
#include "build/common/xml.h"
#include "command/build/varStore.h"
#include "common/debug.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct VarStore
{
    List *varList;                                                  // Variable list
};

typedef struct VarStoreData
{
    const String *variable;                                         // Variable
    const String *value;                                            // Value
} VarStoreData;

/**********************************************************************************************************************************/
FN_EXTERN VarStore *
varStoreNew(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(VarStore, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (VarStore)
        {
            .varList = lstNewP(sizeof(VarStoreData), .comparator = lstComparatorStr),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VAR_STORE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
varStoreAdd(VarStore *const this, const String *const variable, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VAR_STORE, this);
        FUNCTION_TEST_PARAM(STRING, variable);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(variable != NULL);
    ASSERT(value != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        const struct VarStoreData varStoreData =
        {
            .variable = strNewFmt("{[%s]}", strZ(variable)),
            .value = strDup(value),
        };

        if (!lstFind(this->varList, &varStoreData.variable))
        {
            lstAdd(this->varList, &varStoreData);
            lstSort(this->varList, sortOrderAsc);
        }
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN String *
varStoreReplaceStr(VarStore *const this, const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VAR_STORE, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    String *const result = strCat(strNew(), string);
    unsigned int replaceTotal;

    do
    {
        replaceTotal = 0;

        for (unsigned int varIdx = 0; varIdx < lstSize(this->varList); varIdx++)
        {
            const VarStoreData *const varData = lstGet(this->varList, varIdx);
            replaceTotal += strReplace(result, varData->variable, varData->value);
        }
    }
    while (replaceTotal > 0);

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
varStoreReplaceNode(VarStore *const this, XmlNode *const node)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VAR_STORE, this);
        FUNCTION_TEST_PARAM(XML_NODE, node);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(node != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Replace variables in attributes of the current node
        const StringList *const attrList = xmlNodeAttributeList(node);

        for (unsigned int attrIdx = 0; attrIdx < strLstSize(attrList); attrIdx++)
        {
            const String *const attrName = strLstGet(attrList, attrIdx);
            xmlNodeAttributeSet(node, attrName, varStoreReplaceStr(this, xmlNodeAttribute(node, attrName)));
        }

        // Iterate child nodes and replace variables in text nodes
        const XmlNodeList *const childList = xmlNodeChildListAll(node, true);

        for (unsigned int childIdx = 0; childIdx < xmlNodeLstSize(childList); childIdx++)
        {
            XmlNode *const child = xmlNodeLstGet(childList, childIdx);

            // Replace variables in text node content
            if (strEqZ(xmlNodeName(child), "text"))
            {
                xmlNodeTextSet(child, varStoreReplaceStr(this, xmlNodeContent(child)));
            }
            // Recurse into element nodes
            else
                varStoreReplaceNode(this, child);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}
