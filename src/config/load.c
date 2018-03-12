/***********************************************************************************************************************************
Configuration Load
***********************************************************************************************************************************/
#include <string.h>
#include <sys/stat.h>

#include "command/command.h"
#include "common/memContext.h"
#include "common/log.h"
#include "config/config.h"
#include "config/load.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Load the configuration
***********************************************************************************************************************************/
void
cfgLoad(unsigned int argListSize, const char *argList[])
{
    cfgLoadParam(argListSize, argList, NULL);
}

void
cfgLoadParam(unsigned int argListSize, const char *argList[], String *exe)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Parse config from command line and config file
        configParse(argListSize, argList);

        // Initialize logging
        if (cfgCommand() != cfgCmdLocal && cfgCommand() != cfgCmdRemote)
        {
            LogLevel logLevelConsole = logLevelOff;
            LogLevel logLevelStdErr = logLevelOff;
            LogLevel logLevelFile = logLevelOff;
            bool logTimestamp = true;

            if (cfgOptionValid(cfgOptLogLevelConsole))
                logLevelConsole = logLevelEnum(strPtr(cfgOptionStr(cfgOptLogLevelConsole)));

            if (cfgOptionValid(cfgOptLogLevelStderr))
                logLevelStdErr = logLevelEnum(strPtr(cfgOptionStr(cfgOptLogLevelStderr)));

            if (cfgOptionValid(cfgOptLogLevelFile))
                logLevelFile = logLevelEnum(strPtr(cfgOptionStr(cfgOptLogLevelFile)));

            if (cfgOptionValid(cfgOptLogTimestamp))
                logTimestamp = cfgOptionBool(cfgOptLogTimestamp);

            logInit(logLevelConsole, logLevelStdErr, logLevelFile, logTimestamp);
        }

        // Only continue if a command was set.  If no command is set then help will be displayed
        if (cfgCommand() != cfgCmdNone)
        {
            // Open the log file if this command logs to a file
            if (cfgLogFile())
            {
                logFileSet(
                    strPtr(strNewFmt("%s/%s-%s.log", strPtr(cfgOptionStr(cfgOptLogPath)), strPtr(cfgOptionStr(cfgOptStanza)),
                    cfgCommandName(cfgCommand()))));
            }

            // Begin the command
            cmdBegin();

            // If an exe was passed in the use that
            if (exe != NULL)
                cfgExeSet(exe);

            // Set default for repo-host-cmd
            if (cfgOptionValid(cfgOptRepoHost) && cfgOptionTest(cfgOptRepoHost) &&
                cfgOptionSource(cfgOptRepoHostCmd) == cfgSourceDefault)
            {
                cfgOptionDefaultSet(cfgOptRepoHostCmd, varNewStr(cfgExe()));
            }

            // Set default for pg-host-cmd
            if (cfgOptionValid(cfgOptPgHostCmd))
            {
                for (unsigned int optionIdx = 0; optionIdx < cfgOptionIndexTotal(cfgOptPgHost); optionIdx++)
                {
                    if (cfgOptionTest(cfgOptPgHost + optionIdx) && cfgOptionSource(cfgOptPgHostCmd + optionIdx) == cfgSourceDefault)
                        cfgOptionDefaultSet(cfgOptPgHostCmd + optionIdx, varNewStr(cfgExe()));
                }
            }
        }

        // Neutralize the umask to make the repository file/path modes more consistent
        if (cfgOptionValid(cfgOptNeutralUmask) && cfgOptionBool(cfgOptNeutralUmask))
            umask(0000);

        // Protocol timeout should be greater than db timeout
        if (cfgOptionTest(cfgOptDbTimeout) && cfgOptionTest(cfgOptProtocolTimeout) &&
            cfgOptionDbl(cfgOptProtocolTimeout) <= cfgOptionDbl(cfgOptDbTimeout))
        {
            // If protocol-timeout is default then increase it to be greater than db-timeout
            if (cfgOptionSource(cfgOptProtocolTimeout) == cfgSourceDefault)
                cfgOptionSet(cfgOptProtocolTimeout, cfgSourceDefault, varNewDbl(cfgOptionDbl(cfgOptDbTimeout) + 30));
            else
            {
                THROW(OptionInvalidValueError,
                    "'%f' is not valid for '%s' option\nHINT '%s' option (%f) should be greater than '%s' option (%f).",
                    cfgOptionDbl(cfgOptProtocolTimeout), cfgOptionName(cfgOptProtocolTimeout),
                    cfgOptionName(cfgOptProtocolTimeout), cfgOptionDbl(cfgOptProtocolTimeout), cfgOptionName(cfgOptDbTimeout),
                    cfgOptionDbl(cfgOptDbTimeout));
            }
        }

        // Make sure that repo and pg host settings are not both set - cannot both be remote
        if (cfgOptionValid(cfgOptPgHost) && cfgOptionValid(cfgOptRepoHost))
        {
            bool pgHostFound = false;

            for (unsigned int optionIdx = 0; optionIdx < cfgOptionIndexTotal(cfgOptPgHost); optionIdx++)
            {
                if (cfgOptionTest(cfgOptPgHost + optionIdx))
                {
                    pgHostFound = true;
                    break;
                }
            }

            // If a pg-host was found, see if a repo-host is configured
            if (pgHostFound == true)
            {
                for (unsigned int optionIdx = 0; optionIdx < cfgOptionIndexTotal(cfgOptRepoHost); optionIdx++)
                {
                    if (cfgOptionTest(cfgOptRepoHost + optionIdx))
                        THROW(ConfigError, "pg and repo hosts cannot both be configured as remote");
                }
            }
        }

        // Warn when repo-retention-full is not set on a configured repo
        if (cfgOptionValid(cfgOptRepoRetentionFull))
        {
            for (unsigned int optionIdx = 0; optionIdx < cfgOptionIndexTotal(cfgOptRepoType); optionIdx++)
            {
                // If the repo-type is defined, then see if corresponding retention-full is set
                if (cfgOptionTest(cfgOptRepoType + optionIdx) && !cfgOptionTest(cfgOptRepoRetentionFull + optionIdx))
                {
                    LOG_WARN("option %s is not set, the repository may run out of space\n"
                        "HINT: to retain full backups indefinitely (without warning), set option '%s' to the maximum.",
                        cfgOptionName(cfgOptRepoRetentionFull + optionIdx),
                        cfgOptionName(cfgOptRepoRetentionFull + optionIdx));
                }
            }
        }

        // If archive retention is valid for the command, then set archive settings
        if (cfgOptionValid(cfgOptRepoRetentionArchive))
        {
            // For each possible repo, check and adjust the settings as appropriate
            for (unsigned int optionIdx = 0; optionIdx < cfgOptionIndexTotal(cfgOptRepoType); optionIdx++)
            {
                const String *archiveRetentionType = cfgOptionStr(cfgOptRepoRetentionArchiveType + optionIdx);

                const String *msgArchiveOff = strNewFmt("WAL segments will not be expired: option '%s=%s' but",
                    cfgOptionName(cfgOptRepoRetentionArchiveType), strPtr(archiveRetentionType));

                // If the archive retention is not explicitly set then determine what it should be defaulted to
                // to.
                if (!cfgOptionTest(cfgOptRepoRetentionArchive + optionIdx))
                {
                    // If repo-retention-archive-type is default, then if repo-retention-full is set, set the repo-retention-archive
                    // to this value, else ignore archiving
                    if (strEqZ(archiveRetentionType, CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_FULL))
                    {
                        if (cfgOptionTest(cfgOptRepoRetentionFull + optionIdx))
                        {
                            cfgOptionSet(cfgOptRepoRetentionArchive + optionIdx, cfgSourceDefault,
                                varNewInt(cfgOptionInt(cfgOptRepoRetentionFull + optionIdx)));
                        }
                    }
                    else if (strEqZ(archiveRetentionType, CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_DIFF))
                    {
                        // if repo-retention-diff is set then user must have set it
                        if (cfgOptionTest(cfgOptRepoRetentionDiff + optionIdx))
                        {
                            cfgOptionSet(cfgOptRepoRetentionArchive + optionIdx, cfgSourceDefault,
                                varNewInt(cfgOptionInt(cfgOptRepoRetentionDiff + optionIdx)));
                        }
                        else
                        {
                            LOG_WARN("%s neither option '%s' nor option '%s' is set", strPtr(msgArchiveOff),
                                cfgOptionName(cfgOptRepoRetentionArchive + optionIdx),
                                cfgOptionName(cfgOptRepoRetentionDiff + optionIdx));
                        }
                    }
                    else if (strEqZ(archiveRetentionType, CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_INCR))
                    {
                        LOG_WARN("%s option '%s' is not set", strPtr(msgArchiveOff),
                            cfgOptionName(cfgOptRepoRetentionArchive + optionIdx));
                    }
                }
                else
                {
                    // If repo-retention-archive is set then check repo-retention-archive-type and issue a warning if the
                    // corresponding setting is UNDEF since UNDEF means backups will not be expired but they should be in the
                    // practice of setting this value even though expiring the archive itself is OK and will be performed.
                    if ((strEqZ(archiveRetentionType, CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_DIFF)) &&
                        (!cfgOptionTest(cfgOptRepoRetentionDiff + optionIdx)))
                    {
                        LOG_WARN("option '%s' is not set for '%s=%s'\n"
                            "HINT: to retain differential backups indefinitely (without warning), set option '%s' to the maximum.",
                            cfgOptionName(cfgOptRepoRetentionDiff + optionIdx),
                            cfgOptionName(cfgOptRepoRetentionArchiveType + optionIdx),
                            CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_DIFF,
                            cfgOptionName(cfgOptRepoRetentionDiff + optionIdx));
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();
}
