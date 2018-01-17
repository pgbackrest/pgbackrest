/***********************************************************************************************************************************
Common Command Routines
***********************************************************************************************************************************/
#include <string.h>

#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "version.h"

/***********************************************************************************************************************************
Begin the command
***********************************************************************************************************************************/
void cmdBegin()
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Basic info on command start
        String *info = strNewFmt("%s command begin %s:", cfgCommandName(cfgCommand()), PGBACKREST_VERSION);

        // Loop though options and add the ones that are interesting
        for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            const Variant *option = cfgOption(optionId);

            // Skip the option if it is not valid, or default, or not set
            if (!cfgOptionValid(optionId) ||
                cfgOptionSource(optionId) == cfgSourceDefault ||
                (option == NULL && !cfgOptionNegate(optionId)))
            {
                continue;
            }

            // If option was negated
            if (cfgOptionNegate(optionId))
                strCatFmt(info, " --no-%s", cfgOptionName(optionId));
            // Else not negated
            else
            {
                ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);
                strCatFmt(info, " --%s", cfgOptionName(optionId));

                if (cfgDefOptionType(optionDefId) != cfgDefOptTypeBoolean)
                {
                    if (cfgDefOptionSecure(optionDefId))
                        strCat(info, "=<redacted>");
                    else
                    {
                        const String *optionStr = varStrForce(option);

                        if (strchr(strPtr(optionStr), ' ') != NULL)
                            optionStr = strNewFmt("\"%s\"", strPtr(optionStr));

                        strCatFmt(info, "=%s", strPtr(optionStr));
                    }
                }
            }
        }

        LOG_INFO(strPtr(info));
    }
    MEM_CONTEXT_TEMP_END();
}

/***********************************************************************************************************************************
End the command
***********************************************************************************************************************************/
void cmdEnd(int code)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Basic info on command end
        String *info = strNewFmt("%s command end: ", cfgCommandName(cfgCommand()));

        if (code == 0)
            strCat(info, "completed successfully");
        else
            strCatFmt(info, "aborted with exception [%03d]", code);

        LOG_INFO(strPtr(info));
    }
    MEM_CONTEXT_TEMP_END();
}
