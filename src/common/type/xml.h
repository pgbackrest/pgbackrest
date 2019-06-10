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
#define XML_DOCUMENT_TYPE                                           XmlDocument
#define XML_DOCUMENT_PREFIX                                         xmlDocument

typedef struct XmlDocument XmlDocument;
typedef struct XmlNode XmlNode;
typedef struct XmlNodeList XmlNodeList;

#include "common/memContext.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Document Contructors
***********************************************************************************************************************************/
XmlDocument *xmlDocumentNew(const String *rootNode);
XmlDocument *xmlDocumentNewBuf(const Buffer *);
XmlDocument *xmlDocumentNewC(const unsigned char *buffer, size_t bufferSize);
XmlDocument *xmlDocumentNewZ(const char *string);

/***********************************************************************************************************************************
Document Getters
***********************************************************************************************************************************/
Buffer *xmlDocumentBuf(const XmlDocument *this);
XmlNode *xmlDocumentRoot(const XmlDocument *this);

/***********************************************************************************************************************************
Document Destructor
***********************************************************************************************************************************/
void xmlDocumentFree(XmlDocument *this);

/***********************************************************************************************************************************
Node Functions
***********************************************************************************************************************************/
XmlNode *xmlNodeAdd(XmlNode *this, const String *name);

/***********************************************************************************************************************************
Node Getters/Setters
***********************************************************************************************************************************/
String *xmlNodeAttribute(const XmlNode *this, const String *name);
XmlNode *xmlNodeChild(const XmlNode *this, const String *name, bool errorOnMissing);
XmlNodeList *xmlNodeChildList(const XmlNode *this, const String *name);
XmlNode *xmlNodeChildN(const XmlNode *this, const String *name, unsigned int index, bool errorOnMissing);
unsigned int xmlNodeChildTotal(const XmlNode *this, const String *name);
String *xmlNodeContent(const XmlNode *this);
void xmlNodeContentSet(XmlNode *this, const String *content);

/***********************************************************************************************************************************
Node Destructor
***********************************************************************************************************************************/
void xmlNodeFree(XmlNode *this);

/***********************************************************************************************************************************
Node List Getters
***********************************************************************************************************************************/
XmlNode *xmlNodeLstGet(const XmlNodeList *this, unsigned int listIdx);
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
