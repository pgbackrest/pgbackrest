/***********************************************************************************************************************************
XML Handler Extensions
***********************************************************************************************************************************/
// Include core module
#include "common/type/xml.c"

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
    xmlChar *const value = xmlGetProp(this->node, (unsigned char *)strZ(name));

    if (value != NULL)
    {
        result = strNewZ((const char *)value);
        xmlFree(value);
    }

    FUNCTION_TEST_RETURN(STRING, result);
}
