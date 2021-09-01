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
#include "common/type/list.h"
#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Document Constructors
***********************************************************************************************************************************/
// Document with the specified root node
XmlDocument *xmlDocumentNew(const String *rootNode);

// Document from Buffer
XmlDocument *xmlDocumentNewBuf(const Buffer *);

/***********************************************************************************************************************************
Document Getters
***********************************************************************************************************************************/
typedef struct XmlDocumentPub
{
    XmlNode *root;                                                  // Root node
} XmlDocumentPub;

// Dump document to a buffer
Buffer *xmlDocumentBuf(const XmlDocument *this);

// Root node
__attribute__((always_inline)) static inline XmlNode *
xmlDocumentRoot(const XmlDocument *const this)
{
    return ((XmlDocumentPub *const)this)->root;
}

/***********************************************************************************************************************************
Document Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
xmlDocumentFree(XmlDocument *const this)
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
XmlNode *xmlNodeChildN(const XmlNode *this, const String *name, unsigned int index, bool errorOnMissing);

__attribute__((always_inline)) static inline XmlNode *
xmlNodeChild(const XmlNode *const this, const String *const name, const bool errorOnMissing)
{
    return xmlNodeChildN(this, name, 0, errorOnMissing);
}

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
__attribute__((always_inline)) static inline XmlNode *
xmlNodeLstGet(const XmlNodeList *const this, const unsigned int listIdx)
{
    return *(XmlNode **)lstGet((List *const)this, listIdx);
}

// Node list size
__attribute__((always_inline)) static inline unsigned int
xmlNodeLstSize(const XmlNodeList *const this)
{
    return lstSize((List *const)this);
}

/***********************************************************************************************************************************
Node List Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
xmlNodeLstFree(XmlNodeList *const this)
{
    lstFree((List *const)this);
}

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
