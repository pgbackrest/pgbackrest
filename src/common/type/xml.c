/***********************************************************************************************************************************
Xml Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/xml.h"

/***********************************************************************************************************************************
Encoding constants
***********************************************************************************************************************************/
#define XML_ENCODING_TYPE_UTF8                                      "UTF-8"

/***********************************************************************************************************************************
Node type
***********************************************************************************************************************************/
struct XmlNode
{
    xmlNodePtr node;
};

/***********************************************************************************************************************************
Document type
***********************************************************************************************************************************/
struct XmlDocument
{
    XmlDocumentPub pub;                                             // Publicly accessible variables
    xmlDocPtr xml;
};

/***********************************************************************************************************************************
Error handler

For now this is a noop until more detailed error messages are needed.  The function is called multiple times per error, so the
messages need to be accumulated and then returned together.

This empty function is required because without it libxml2 will dump errors to stdout.  Really.
***********************************************************************************************************************************/
static void
xmlErrorHandler(void *ctx, const char *format, ...)
{
    (void)ctx;
    (void)format;
}

/***********************************************************************************************************************************
Initialize xml
***********************************************************************************************************************************/
static void
xmlInit(void)
{
    FUNCTION_TEST_VOID();

    // Initialize xml if it is not already initialized
    static bool xmlInit = false;

    if (!xmlInit)
    {
        LIBXML_TEST_VERSION;

        // It's a pretty weird that we can't just pass a handler function but instead have to assign it to a var...
        static xmlGenericErrorFunc xmlErrorHandlerFunc = xmlErrorHandler;
        initGenericErrorDefaultFunc(&xmlErrorHandlerFunc);

        xmlInit = true;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static XmlNodeList *
xmlNodeLstNew(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(XML_NODE_LIST, (XmlNodeList *)OBJ_NAME(lstNewP(sizeof(XmlNode *)), XmlNodeList::List));
}

/**********************************************************************************************************************************/
static XmlNode *
xmlNodeNew(xmlNodePtr node)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, node);
    FUNCTION_TEST_END();

    ASSERT(node != NULL);

    XmlNode *this = memNew(sizeof(XmlNode));

    *this = (XmlNode)
    {
        .node = node,
    };

    FUNCTION_TEST_RETURN(XML_NODE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN XmlNode *
xmlNodeAdd(XmlNode *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    XmlNode *result = xmlNodeNew(xmlNewNode(NULL, BAD_CAST strZ(name)));
    xmlAddChild(this->node, result->node);

    FUNCTION_TEST_RETURN(XML_NODE, result);
}

/***********************************************************************************************************************************
Add a node to a node list
***********************************************************************************************************************************/
static XmlNodeList *
xmlNodeLstAdd(XmlNodeList *this, xmlNodePtr node)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE_LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, node);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(node != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        XmlNode *item = xmlNodeNew(node);
        lstAdd((List *)this, &item);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(XML_NODE_LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
xmlNodeContent(const XmlNode *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
    {
        xmlChar *content = xmlNodeGetContent(this->node);
        result = strNewZ((char *)content);
        xmlFree(content);
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN void
xmlNodeContentSet(XmlNode *this, const String *content)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, content);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(content != NULL);

    xmlAddChild(this->node, xmlNewText(BAD_CAST strZ(content)));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN XmlNodeList *
xmlNodeChildList(const XmlNode *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    XmlNodeList *list = xmlNodeLstNew();

    for (xmlNodePtr currentNode = this->node->children; currentNode != NULL; currentNode = currentNode->next)
    {
        if (currentNode->type == XML_ELEMENT_NODE && strEqZ(name, (char *)currentNode->name))
            xmlNodeLstAdd(list, currentNode);
    }

    FUNCTION_TEST_RETURN(XML_NODE_LIST, list);
}

/**********************************************************************************************************************************/
FN_EXTERN XmlNode *
xmlNodeChildN(const XmlNode *this, const String *name, unsigned int index, bool errorOnMissing)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    XmlNode *child = NULL;
    unsigned int childIdx = 0;

    for (xmlNodePtr currentNode = this->node->children; currentNode != NULL; currentNode = currentNode->next)
    {
        if (currentNode->type == XML_ELEMENT_NODE && strEqZ(name, (char *)currentNode->name))
        {
            if (childIdx == index)
            {
                child = xmlNodeNew(currentNode);
                break;
            }

            childIdx++;
        }
    }

    if (child == NULL && errorOnMissing)
        THROW_FMT(FormatError, "unable to find child '%s':%u in node '%s'", strZ(name), index, this->node->name);

    FUNCTION_TEST_RETURN(XML_NODE, child);
}

/***********************************************************************************************************************************
Free document
***********************************************************************************************************************************/
static void
xmlDocumentFreeResource(THIS_VOID)
{
    THIS(XmlDocument);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(XML_DOCUMENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    xmlFreeDoc(this->xml);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN XmlDocument *
xmlDocumentNew(const String *rootName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, rootName);
    FUNCTION_TEST_END();

    ASSERT(rootName != NULL);

    xmlInit();

    // Create object
    XmlDocument *this = NULL;

    OBJ_NEW_BEGIN(XmlDocument, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        this = OBJ_NEW_ALLOC();

        *this = (XmlDocument)
        {
            .xml = xmlNewDoc(BAD_CAST "1.0"),
        };

        // Set callback to ensure xml document is freed
        memContextCallbackSet(objMemContext(this), xmlDocumentFreeResource, this);

        this->pub.root = xmlNodeNew(xmlNewNode(NULL, BAD_CAST strZ(rootName)));
        xmlDocSetRootElement(this->xml, xmlDocumentRoot(this)->node);
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(XML_DOCUMENT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN XmlDocument *
xmlDocumentNewBuf(const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);
    ASSERT(!bufEmpty(buffer));

    xmlInit();

    // Create object
    XmlDocument *this = NULL;

    OBJ_NEW_BEGIN(XmlDocument, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        this = OBJ_NEW_ALLOC();
        *this = (XmlDocument){{0}};                                 // Extra braces are required for older gcc versions

        if ((this->xml = xmlReadMemory((const char *)bufPtrConst(buffer), (int)bufUsed(buffer), "noname.xml", NULL, 0)) == NULL)
            THROW_FMT(FormatError, "invalid xml in %zu byte(s):\n%s", bufUsed(buffer), strZ(strNewBuf(buffer)));

        // Set callback to ensure xml document is freed
        memContextCallbackSet(objMemContext(this), xmlDocumentFreeResource, this);

        // Get the root node
        this->pub.root = xmlNodeNew(xmlDocGetRootElement(this->xml));
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(XML_DOCUMENT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
xmlDocumentBuf(const XmlDocument *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_DOCUMENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    xmlChar *xml;
    int xmlSize;

    xmlDocDumpMemoryEnc(this->xml, &xml, &xmlSize, XML_ENCODING_TYPE_UTF8);
    Buffer *result = bufNewC(xml, (size_t)xmlSize);
    xmlFree(xml);

    FUNCTION_TEST_RETURN(BUFFER, result);
}
