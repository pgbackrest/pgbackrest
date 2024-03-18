/***********************************************************************************************************************************
XML Handler Extensions
***********************************************************************************************************************************/
#ifndef BUILD_COMMON_XML_H
#define BUILD_COMMON_XML_H

#include "common/type/xml.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct XmlDocumentNewParam
{
    VAR_PARAM_HEADER;
    const String *dtdName;                                          // DTD name, if any
    const String *dtdFile;                                          // DTD file (must be set if dtdName is set)
} XmlDocumentNewParam;

#define xmlDocumentNewP(rootNode, ...)                                                                                             \
    xmlDocumentNewParam(rootNode, (XmlDocumentNewParam){VAR_PARAM_INIT, __VA_ARGS__})

XmlDocument *xmlDocumentNewParam(const String *rootNode, XmlDocumentNewParam param);

/***********************************************************************************************************************************
Node Getters/Setters
***********************************************************************************************************************************/
// Get/set node attribute
String *xmlNodeAttribute(const XmlNode *this, const String *name);
void xmlNodeAttributeSet(XmlNode *this, const String *name, const String *value);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add all child nodes to another node
void xmlNodeChildAdd(XmlNode *this, const XmlNode *child);

#endif
