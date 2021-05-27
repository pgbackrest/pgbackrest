/***********************************************************************************************************************************
HTTP URL
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/url.h"
#include "common/memContext.h"
#include "common/type/stringList.h"
#include "common/regExp.h"

/***********************************************************************************************************************************
Regular expression for URLs. This is not intended to be completely comprehensive, e.g. it is still possible to enter bad hostnames.
The goal is to make sure the syntax is correct enough for the rest of the parsing to succeed.
***********************************************************************************************************************************/
STRING_STATIC(
    HTTP_URL_REGEXP_STR,
    "^(http[s]{0,1}:\\/\\/){0,1}"                                   // Optional protocol (http or https)
    "([^\\[\\:\\/?]+|\\[[a-fA-F0-9:]+\\])"                          // host/ipv4/ipv6
    "(:[1-9][0-9]{0,4}){0,1}"                                       // Optional port
    "(\\/[^?\\/]*)*$");                                             // Optional path

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpUrl
{
    HttpUrlPub pub;
};

/***********************************************************************************************************************************
Convert protocol type to a string
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
            .pub =
            {
                .memContext = MEM_CONTEXT_NEW(),
                .url = strDup(url),
            },
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Check that URL format is one we accept
            if (!regExpMatchOne(HTTP_URL_REGEXP_STR, url))
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
            const String *host = strLstGet(splitUrl, 0);
            const String *port = NULL;

            // If an IPv6 address
            if (strBeginsWithZ(host, "["))
            {
                // Split closing bracket
                StringList *splitHost = strLstNewSplitZ(host, "]");
                ASSERT(strLstSize(splitHost) == 2);

                // Remove opening bracket
                host = strSub(strLstGet(splitHost, 0), 1);

                // Get port if specified
                if (strSize(strLstGet(splitHost, 1)) > 0)
                    port = strSub(strLstGet(splitHost, 1), 1);
            }
            // Else IPv4 or host name
            else
            {
                // Split on colon
                StringList *splitHost = strLstNewSplitZ(host, ":");
                ASSERT(strLstSize(splitHost) != 0);

                // First part is the host
                host = strLstGet(splitHost, 0);

                // Second part is the port, if it exists
                if (strLstSize(splitHost) > 1)
                {
                    ASSERT(strLstSize(splitHost) == 2);
                    port = strLstGet(splitHost, 1);
                }
            }

            // Copy host into object context
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                this->pub.host = strDup(host);
            }
            MEM_CONTEXT_PRIOR_END();

            // Get port if specified
            if (port != NULL)
            {
                this->pub.port = cvtZToUInt(strZ(port));
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

                // Construct path and copy into local context
                const String *path = strLstJoin(splitUrl, "/");

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    this->pub.path = strNewFmt("/%s", strZ(path));
                }
                MEM_CONTEXT_PRIOR_END();
            }
            // Else default path is /
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
    // Is IPv6 address?
    bool ipv6 = strChr(this->pub.host, ':') != -1;

    return strNewFmt(
        "{%s://%s%s%s:%u%s}", strZ(httpProtocolTypeStr(this->pub.type)), ipv6 ? "[" : "", strZ(this->pub.host), ipv6 ? "]" : "",
        this->pub.port, strZ(this->pub.path));
}
