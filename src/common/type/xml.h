/***********************************************************************************************************************************
Xml Handler

A thin wrapper around the libxml2 library.

There are many capabilities of libxml2 that are not exposed here and may need to be added to when implementing new features.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_XML_H
#define COMMON_TYPE_XML_H

/***********************************************************************************************************************************
Objects
***********************************************************************************************************************************/
typedef struct XmlDocument XmlDocument;
typedef struct XmlNode XmlNode;
typedef struct XmlNodeList XmlNodeList;

#include "common/memContext.h"
#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Document Constructors
***********************************************************************************************************************************/
// Document with the specified root node
XmlDocument *xmlDocumentNew(const String *rootNode);

// Document from Buffer
XmlDocument *xmlDocumentNewBuf(const Buffer *);

// Document from C buffer
XmlDocument *xmlDocumentNewC(const unsigned char *buffer, size_t bufferSize);

// Document from zero-terminated string
XmlDocument *xmlDocumentNewZ(const char *string);

/***********************************************************************************************************************************
Document Getters
***********************************************************************************************************************************/
// Dump document to a buffer
Buffer *xmlDocumentBuf(const XmlDocument *this);

// Root node
XmlNode *xmlDocumentRoot(const XmlDocument *this);

/***********************************************************************************************************************************
Document Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
xmlDocumentFree(XmlDocument *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Node Functions
***********************************************************************************************************************************/
// Add a node
XmlNode *xmlNodeAdd(XmlNode *this, const String *name);

/***********************************************************************************************************************************
Node Getters/Setters
***********************************************************************************************************************************/
// Node attribute
String *xmlNodeAttribute(const XmlNode *this, const String *name);

// Node child (by name or index)
XmlNode *xmlNodeChild(const XmlNode *this, const String *name, bool errorOnMissing);
XmlNode *xmlNodeChildN(const XmlNode *this, const String *name, unsigned int index, bool errorOnMissing);

// List of child nodes
XmlNodeList *xmlNodeChildList(const XmlNode *this, const String *name);

// Node child total
unsigned int xmlNodeChildTotal(const XmlNode *this, const String *name);

// Node content
String *xmlNodeContent(const XmlNode *this);
void xmlNodeContentSet(XmlNode *this, const String *content);

/***********************************************************************************************************************************
Node Destructor
***********************************************************************************************************************************/
void xmlNodeFree(XmlNode *this);

/***********************************************************************************************************************************
Node List Getters
***********************************************************************************************************************************/
// Get a node from the list
XmlNode *xmlNodeLstGet(const XmlNodeList *this, unsigned int listIdx);

// Node list size
unsigned int xmlNodeLstSize(const XmlNodeList *this);

/***********************************************************************************************************************************
Node List Destructor
***********************************************************************************************************************************/
void xmlNodeLstFree(XmlNodeList *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_XML_DOCUMENT_TYPE                                                                                             \
    XmlDocument *
#define FUNCTION_LOG_XML_DOCUMENT_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "XmlDocument", buffer, bufferSize)

#define FUNCTION_LOG_XML_NODE_TYPE                                                                                                 \
    XmlNode *
#define FUNCTION_LOG_XML_NODE_FORMAT(value, buffer, bufferSize)                                                                    \
    objToLog(value, "XmlNode", buffer, bufferSize)

#define FUNCTION_LOG_XML_NODE_LIST_TYPE                                                                                            \
    XmlNodeList *
#define FUNCTION_LOG_XML_NODE_LIST_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "XmlNodeList", buffer, bufferSize)

#endif
