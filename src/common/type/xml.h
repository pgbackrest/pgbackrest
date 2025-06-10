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
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Document Constructors
***********************************************************************************************************************************/
// Document with the specified root node
FN_EXTERN XmlDocument *xmlDocumentNew(const String *rootNode);

// Document from Buffer
FN_EXTERN XmlDocument *xmlDocumentNewBuf(const Buffer *);

/***********************************************************************************************************************************
Document Getters
***********************************************************************************************************************************/
typedef struct XmlDocumentPub
{
    XmlNode *root;                                                  // Root node
} XmlDocumentPub;

// Dump document to a buffer
FN_EXTERN Buffer *xmlDocumentBuf(const XmlDocument *this);

// Root node
FN_INLINE_ALWAYS XmlNode *
xmlDocumentRoot(const XmlDocument *const this)
{
    return ((const XmlDocumentPub *const)this)->root;
}

/***********************************************************************************************************************************
Document Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
xmlDocumentFree(XmlDocument *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Node Functions
***********************************************************************************************************************************/
// Add a node
FN_EXTERN XmlNode *xmlNodeAdd(XmlNode *this, const String *name);

/***********************************************************************************************************************************
Node Getters/Setters
***********************************************************************************************************************************/
// Node child (by name or index)
FN_EXTERN XmlNode *xmlNodeChildN(const XmlNode *this, const String *name, unsigned int index, bool errorOnMissing);

FN_INLINE_ALWAYS XmlNode *
xmlNodeChild(const XmlNode *const this, const String *const name, const bool errorOnMissing)
{
    return xmlNodeChildN(this, name, 0, errorOnMissing);
}

// List of child nodes
FN_EXTERN XmlNodeList *xmlNodeChildList(const XmlNode *this, const String *name);

// List of child nodes using multiple names
FN_EXTERN XmlNodeList *xmlNodeChildListMulti(const XmlNode *this, const StringList *nameList);

// Node content
FN_EXTERN String *xmlNodeContent(const XmlNode *this);
FN_EXTERN void xmlNodeContentSet(XmlNode *this, const String *content);

// Node name
FN_EXTERN String *xmlNodeName(const XmlNode *this);

/***********************************************************************************************************************************
Node List Getters
***********************************************************************************************************************************/
// Get a node from the list
FN_INLINE_ALWAYS XmlNode *
xmlNodeLstGet(const XmlNodeList *const this, const unsigned int listIdx)
{
    return *(XmlNode **)lstGet((const List *const)this, listIdx);
}

// Node list size
FN_INLINE_ALWAYS unsigned int
xmlNodeLstSize(const XmlNodeList *const this)
{
    return lstSize((const List *const)this);
}

/***********************************************************************************************************************************
Node List Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
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
    objNameToLog(value, "XmlDocument", buffer, bufferSize)

#define FUNCTION_LOG_XML_NODE_TYPE                                                                                                 \
    XmlNode *
#define FUNCTION_LOG_XML_NODE_FORMAT(value, buffer, bufferSize)                                                                    \
    objNameToLog(value, "XmlNode", buffer, bufferSize)

#define FUNCTION_LOG_XML_NODE_LIST_TYPE                                                                                            \
    XmlNodeList *
#define FUNCTION_LOG_XML_NODE_LIST_FORMAT(value, buffer, bufferSize)                                                               \
    objNameToLog(value, "XmlNodeList", buffer, bufferSize)

#endif
