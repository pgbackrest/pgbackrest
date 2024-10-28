/***********************************************************************************************************************************
XML Handler Extensions
***********************************************************************************************************************************/
// Include core module
#include "common/type/xml.c"

#include "build/common/xml.h"

/**********************************************************************************************************************************/
XmlDocument *
xmlDocumentNewParam(const String *const rootNode, const XmlDocumentNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, rootNode);
        FUNCTION_TEST_PARAM(STRING, param.dtdName);
        FUNCTION_TEST_PARAM(STRING, param.dtdFile);
    FUNCTION_TEST_END();

    ASSERT(rootNode != NULL);

    XmlDocument *const result = xmlDocumentNew(rootNode);

    if (param.dtdName != NULL)
    {
        ASSERT(param.dtdFile != NULL);
        xmlCreateIntSubset(
            result->xml, (const unsigned char *)strZ(param.dtdName), NULL, (const unsigned char *)strZ(param.dtdFile));
    }

    FUNCTION_TEST_RETURN(XML_DOCUMENT, result);
}

/**********************************************************************************************************************************/
String *
xmlNodeAttribute(const XmlNode *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    String *result = NULL;
    xmlChar *const value = xmlGetProp(this->node, (const unsigned char *)strZ(name));

    if (value != NULL)
    {
        result = strNewZ((const char *)value);
        xmlFree(value);
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
void
xmlNodeAttributeSet(XmlNode *const this, const String *const name, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);
    ASSERT(value != NULL);

    xmlSetProp(this->node, (const unsigned char *)strZ(name), (const unsigned char *)strZ(value));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
xmlNodeChildAdd(XmlNode *const this, const XmlNode *const child)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(XML_NODE, child);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(child != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        for (xmlNodePtr currentNodeRaw = child->node->children; currentNodeRaw != NULL; currentNodeRaw = currentNodeRaw->next)
        {
            // Skip comments
            if (currentNodeRaw->type == XML_COMMENT_NODE)
                continue;

            // Copy all child nodes (only node and text types are copied)
            XmlNode *const currentNode = xmlNodeNew(currentNodeRaw);

            if (currentNode->node->type == XML_ELEMENT_NODE)
            {
                XmlNode *const node = xmlNodeAdd(this, STR((const char *)currentNode->node->name));

                // Copy node attributes
                for (xmlAttrPtr currentAttr = currentNode->node->properties; currentAttr != NULL; currentAttr = currentAttr->next)
                {
                    xmlNodeAttributeSet(
                        node, STR((const char *)currentAttr->name),
                        xmlNodeAttribute(currentNode, STR((const char *)currentAttr->name)));
                }

                // Recurse to copy child nodes
                xmlNodeChildAdd(node, currentNode);
            }
            else
            {
                CHECK_FMT(
                    AssertError, currentNode->node->type == XML_TEXT_NODE, "unknown type %u in node '%s'", currentNode->node->type,
                    currentNode->node->name);

                xmlNodeContentSet(this, xmlNodeContent(currentNode));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}
