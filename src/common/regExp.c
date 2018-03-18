/***********************************************************************************************************************************
Regular Expression Handler
***********************************************************************************************************************************/
#include <regex.h>
#include <sys/types.h>

#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type.h"

/***********************************************************************************************************************************
Contains information about the regular expression handler
***********************************************************************************************************************************/
struct RegExp
{
    MemContext *memContext;
    regex_t regExp;
};

/***********************************************************************************************************************************
Handle errors
***********************************************************************************************************************************/
static void
regExpError(int error)
{
    char buffer[4096];
    regerror(error, NULL, buffer, sizeof(buffer));
    THROW(FormatError, buffer);
}

/***********************************************************************************************************************************
New regular expression handler
***********************************************************************************************************************************/
RegExp *
regExpNew(const String *expression)
{
    RegExp *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("RegExp")
    {
        this = memNew(sizeof(RegExp));
        this->memContext = MEM_CONTEXT_NEW();

        // Compile the regexp and process errors
        int result = 0;

        if ((result = regcomp(&this->regExp, strPtr(expression), REG_NOSUB | REG_EXTENDED)) != 0)
        {
            memFree(this);
            regExpError(result);
        }

        // Set free callback to ensure cipher context is freed
        memContextCallback(this->memContext, (MemContextCallback)regExpFree, this);
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Match on a regular expression
***********************************************************************************************************************************/
bool
regExpMatch(RegExp *this, const String *string)
{
    // Test for a match
    int result = regexec(&this->regExp, strPtr(string), 0, NULL, 0);

    // Check for an error
    if (result != 0 && result != REG_NOMATCH)
        regExpError(result);                                        // {uncoverable - no error condition known}

    return result == 0;
}

/***********************************************************************************************************************************
Free regular expression
***********************************************************************************************************************************/
void
regExpFree(RegExp *this)
{
    if (this != NULL)
    {
        regfree(&this->regExp);
        memContextFree(this->memContext);
    }
}

/***********************************************************************************************************************************
Match a regular expression in one call for brevity
***********************************************************************************************************************************/
bool
regExpMatchOne(const String *expression, const String *string)
{
    bool result = false;
    RegExp *regExp = regExpNew(expression);

    TRY_BEGIN()
    {
        result = regExpMatch(regExp, string);
    }
    FINALLY()
    {
        regExpFree(regExp);
    }
    TRY_END();

    return result;
}
