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
#include "common/type/list.h"
#include "common/type/object.h"
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
    MemContext *memContext;
    xmlDocPtr xml;
    XmlNode *root;
};

OBJECT_DEFINE_FREE(XML_DOCUMENT);

/***********************************************************************************************************************************
Error handler

For now this is a noop until more detailed error messages are needed.  The function is called multiple times per error, so the
messages need to be accumulated and then returned together.

This empty function is required because without it libxml2 will dump errors to stdout.  Really.
***********************************************************************************************************************************/
static void xmlErrorHandler(void *ctx, const char *format, ...)
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

/***********************************************************************************************************************************
Create node list
***********************************************************************************************************************************/
XmlNodeList *
xmlNodeLstNew(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN((XmlNodeList *)lstNew(sizeof(XmlNode *)));
}

/***********************************************************************************************************************************
Get a node from the list
***********************************************************************************************************************************/
XmlNode *
xmlNodeLstGet(const XmlNodeList *this, unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(*(XmlNode **)lstGet((List *)this, listIdx));
}

/***********************************************************************************************************************************
Get node list size
***********************************************************************************************************************************/
unsigned int
xmlNodeLstSize(const XmlNodeList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE_LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize((List *)this));
}

/***********************************************************************************************************************************
Free node list
***********************************************************************************************************************************/
void
xmlNodeLstFree(XmlNodeList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE_LIST, this);
    FUNCTION_TEST_END();

    lstFree((List *)this);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Create node
***********************************************************************************************************************************/
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

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Add a node
***********************************************************************************************************************************/
XmlNode *
xmlNodeAdd(XmlNode *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    XmlNode *result = xmlNodeNew(xmlNewNode(NULL, BAD_CAST strPtr(name)));
    xmlAddChild(this->node, result->node);

    FUNCTION_TEST_RETURN(result);
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

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Get node attribute
***********************************************************************************************************************************/
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
    xmlChar *value = xmlGetProp(this->node, (unsigned char *)strPtr(name));

    if (value != NULL)
    {
        result = strNew((char *)value);
        xmlFree(value);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get node content
***********************************************************************************************************************************/
String *
xmlNodeContent(const XmlNode *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
    {
        xmlChar *content = xmlNodeGetContent(this->node);
        result = strNew((char *)content);
        xmlFree(content);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Set node content
***********************************************************************************************************************************/
void
xmlNodeContentSet(XmlNode *this, const String *content)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, content);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(content != NULL);

    xmlAddChild(this->node, xmlNewText(BAD_CAST strPtr(content)));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get a list of child nodes
***********************************************************************************************************************************/
XmlNodeList *
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

    FUNCTION_TEST_RETURN(list);
}

/***********************************************************************************************************************************
Get a child node
***********************************************************************************************************************************/
XmlNode *
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
        THROW_FMT(FormatError, "unable to find child '%s':%u in node '%s'", strPtr(name), index, this->node->name);

    FUNCTION_TEST_RETURN(child);
}

XmlNode *
xmlNodeChild(const XmlNode *this, const String *name, bool errorOnMissing)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN(xmlNodeChildN(this, name, 0, errorOnMissing));
}

/***********************************************************************************************************************************
Get child total
***********************************************************************************************************************************/
unsigned int
xmlNodeChildTotal(const XmlNode *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    unsigned int result = 0;

    for (xmlNodePtr currentNode = this->node->children; currentNode != NULL; currentNode = currentNode->next)
    {
        if (currentNode->type == XML_ELEMENT_NODE && strEqZ(name, (char *)currentNode->name))
            result++;
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Free node
***********************************************************************************************************************************/
void
xmlNodeFree(XmlNode *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_NODE, this);
    FUNCTION_TEST_END();

    if (this != NULL)
        memFree(this);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Free document
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(XML_DOCUMENT, LOG, logLevelTrace)
{
    xmlFreeDoc(this->xml);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Create a new document with the specified root node
***********************************************************************************************************************************/
XmlDocument *
xmlDocumentNew(const String *rootName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, rootName);
    FUNCTION_TEST_END();

    ASSERT(rootName != NULL);

    xmlInit();

    // Create object
    XmlDocument *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("XmlDocument")
    {
        this = memNew(sizeof(XmlDocument));

        *this = (XmlDocument)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .xml = xmlNewDoc(BAD_CAST "1.0"),
        };

        // Set callback to ensure xml document is freed
        memContextCallbackSet(this->memContext, xmlDocumentFreeResource, this);

        this->root = xmlNodeNew(xmlNewNode(NULL, BAD_CAST strPtr(rootName)));
        xmlDocSetRootElement(this->xml, this->root->node);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create document from C buffer
***********************************************************************************************************************************/
XmlDocument *
xmlDocumentNewC(const unsigned char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(UCHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);
    ASSERT(bufferSize > 0);

    xmlInit();

    // Create object
    XmlDocument *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("XmlDocument")
    {
        this = memNew(sizeof(XmlDocument));

        *this = (XmlDocument)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        if ((this->xml = xmlReadMemory((const char *)buffer, (int)bufferSize, "noname.xml", NULL, 0)) == NULL)
            THROW_FMT(FormatError, "invalid xml");

        // Set callback to ensure xml document is freed
        memContextCallbackSet(this->memContext, xmlDocumentFreeResource, this);

        // Get the root node
        this->root = xmlNodeNew(xmlDocGetRootElement(this->xml));
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create document from Buffer
***********************************************************************************************************************************/
XmlDocument *
xmlDocumentNewBuf(const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);
    ASSERT(bufSize(buffer) > 0);

    FUNCTION_TEST_RETURN(xmlDocumentNewC(bufPtrConst(buffer), bufUsed(buffer)));
}

/***********************************************************************************************************************************
Create document from zero-terminated string
***********************************************************************************************************************************/
XmlDocument *
xmlDocumentNewZ(const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);
    ASSERT(strlen(string) > 0);

    FUNCTION_TEST_RETURN(xmlDocumentNewC((const unsigned char *)string, strlen(string)));
}

/***********************************************************************************************************************************
Dump document to a buffer
***********************************************************************************************************************************/
Buffer *
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

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get the root node
***********************************************************************************************************************************/
XmlNode *
xmlDocumentRoot(const XmlDocument *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(XML_DOCUMENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->root);
}
