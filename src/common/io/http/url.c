/***********************************************************************************************************************************
HTTP URL
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/url.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/type/stringList.h"
#include "common/regExp.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpUrl
{
    HttpUrlPub pub;
    MemContext *memContext;                                         // Mem context
};

OBJECT_DEFINE_FREE(HTTP_URL);

/***********************************************************************************************************************************
***********************************************************************************************************************************/
STRING_STATIC(HTTP_PROTOCOL_HTTP_STR,                               "http");
STRING_STATIC(HTTP_PROTOCOL_HTTPS_STR,                              "https");

static const String *
httpProtocolTypeStr(HttpProtocolType type)
{
    switch (type)
    {
        case httpProtocolTypeHttp:
            return HTTP_PROTOCOL_HTTP_STR;

        case httpProtocolTypeHttps:
            return HTTP_PROTOCOL_HTTPS_STR;

        default:
            return NULL;
    }
}

/**********************************************************************************************************************************/
HttpUrl *
httpUrlNewParse(const String *const url, HttpUrlNewParseParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, url);
        FUNCTION_TEST_PARAM(ENUM, param.type);
    FUNCTION_TEST_END();

    ASSERT(url != NULL);

    HttpUrl *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpUrl")
    {
        // Allocate state and set context
        this = memNew(sizeof(HttpUrl));

        *this = (HttpUrl)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .pub =
            {
                .url = strDup(url),
            },
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Check that URL format is one we accept
            if (!regExpMatchOne(STRDEF("^(http[s]{0,1}:\\/\\/){0,1}[^:\\/?]+(:[1-9][0-9]{0,4}){0,1}(\\/[^?\\/]*)*$"), url))
                THROW_FMT(FormatError, "invalid URL '%s'", strZ(url));

            // Determine whether the first part is protocol or host
            StringList *splitUrl = strLstNewSplitZ(url, "/");

            if (strEqZ(strLstGet(splitUrl, 0), "http:"))
                this->pub.type = httpProtocolTypeHttp;
            else if (strEqZ(strLstGet(splitUrl, 0), "https:"))
                this->pub.type = httpProtocolTypeHttps;

            // If no protocol found then the first part is the host
            if (this->pub.type == httpProtocolTypeAny)
            {
                // Protocol must be set explicitly
                ASSERT(param.type != httpProtocolTypeAny);

                this->pub.type = param.type;
            }
            // Else protocol was found
            else
            {
                // Protocol must match expected
                if (param.type != httpProtocolTypeAny && this->pub.type != param.type)
                    THROW_FMT(FormatError, "expected protocol '%s' in URL '%s'", strZ(httpProtocolTypeStr(param.type)), strZ(url));

                // Remove protocol parts from split
                strLstRemoveIdx(splitUrl, 0);
                strLstRemoveIdx(splitUrl, 0);
            }

            // Get host
            StringList *splitHost = strLstNewSplitZ(strLstGet(splitUrl, 0), ":");
            ASSERT(strLstSize(splitHost) != 0);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                this->pub.host = strDup(strLstGet(splitHost, 0));
            }
            MEM_CONTEXT_PRIOR_END();

            // Get port if specified
            if (strLstSize(splitHost) > 1)
            {
                ASSERT(strLstSize(splitHost) == 2);

                this->pub.port = cvtZToUInt(strZ(strLstGet(splitHost, 1)));
            }
            // Else set default port based on the protocol
            else
            {
                ASSERT(this->pub.type != httpProtocolTypeAny);

                if (this->pub.type == httpProtocolTypeHttp)
                    this->pub.port = 80;
                else
                    this->pub.port = 443;
            }

            // Check for path
            if (strLstSize(splitUrl) > 1)
            {
                // Remove host part so it is easier to construct the path
                strLstRemoveIdx(splitUrl, 0);

                // Construct path
                const String *path = strLstJoin(splitUrl, "/");

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    this->pub.path = strNewFmt("/%s", strZ(path));
                }
                MEM_CONTEXT_PRIOR_END();
            }
            else
                this->pub.path = FSLASH_STR;
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
httpUrlToLog(const HttpUrl *this)
{
    return strNewFmt(
        "{%s://%s:%u%s}", strZ(httpProtocolTypeStr(this->pub.type)), strZ(this->pub.host), this->pub.port, strZ(this->pub.path));
}
