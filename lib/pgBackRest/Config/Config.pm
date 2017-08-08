####################################################################################################################################
# CONFIG MODULE
####################################################################################################################################
package pgBackRest::Config::Config;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use Getopt::Long qw(GetOptions);
use Storable qw(dclone);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Io::Base;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::LibC qw(:config :configRule);
use pgBackRest::Version;

####################################################################################################################################
# DB/BACKUP Constants
####################################################################################################################################
use constant DB                                                     => 'db';
    push @EXPORT, qw(DB);
use constant BACKUP                                                 => 'backup';
    push @EXPORT, qw(BACKUP);

####################################################################################################################################
# BACKUP Type Constants
####################################################################################################################################
use constant BACKUP_TYPE_FULL                                       => 'full';
    push @EXPORT, qw(BACKUP_TYPE_FULL);
use constant BACKUP_TYPE_DIFF                                       => 'diff';
    push @EXPORT, qw(BACKUP_TYPE_DIFF);
use constant BACKUP_TYPE_INCR                                       => 'incr';
    push @EXPORT, qw(BACKUP_TYPE_INCR);

####################################################################################################################################
# REPO Type Constants
####################################################################################################################################
use constant REPO_TYPE_CIFS                                         => 'cifs';
    push @EXPORT, qw(REPO_TYPE_CIFS);
use constant REPO_TYPE_POSIX                                        => 'posix';
    push @EXPORT, qw(REPO_TYPE_POSIX);
use constant REPO_TYPE_S3                                           => 's3';
    push @EXPORT, qw(REPO_TYPE_S3);

####################################################################################################################################
# INFO Output Constants
####################################################################################################################################
use constant INFO_OUTPUT_TEXT                                       => 'text';
    push @EXPORT, qw(INFO_OUTPUT_TEXT);
use constant INFO_OUTPUT_JSON                                       => 'json';
    push @EXPORT, qw(INFO_OUTPUT_JSON);

####################################################################################################################################
# SOURCE Constants
####################################################################################################################################
use constant SOURCE_CONFIG                                          => 'config';
    push @EXPORT, qw(SOURCE_CONFIG);
use constant SOURCE_PARAM                                           => 'param';
    push @EXPORT, qw(SOURCE_PARAM);
use constant SOURCE_DEFAULT                                         => 'default';
    push @EXPORT, qw(SOURCE_DEFAULT);

####################################################################################################################################
# RECOVERY Type Constants
####################################################################################################################################
use constant RECOVERY_TYPE_NAME                                     => 'name';
    push @EXPORT, qw(RECOVERY_TYPE_NAME);
use constant RECOVERY_TYPE_TIME                                     => 'time';
    push @EXPORT, qw(RECOVERY_TYPE_TIME);
use constant RECOVERY_TYPE_XID                                      => 'xid';
    push @EXPORT, qw(RECOVERY_TYPE_XID);
use constant RECOVERY_TYPE_PRESERVE                                 => 'preserve';
    push @EXPORT, qw(RECOVERY_TYPE_PRESERVE);
use constant RECOVERY_TYPE_NONE                                     => 'none';
    push @EXPORT, qw(RECOVERY_TYPE_NONE);
use constant RECOVERY_TYPE_IMMEDIATE                                => 'immediate';
    push @EXPORT, qw(RECOVERY_TYPE_IMMEDIATE);
use constant RECOVERY_TYPE_DEFAULT                                  => 'default';
    push @EXPORT, qw(RECOVERY_TYPE_DEFAULT);

####################################################################################################################################
# RECOVERY Action Constants
####################################################################################################################################
use constant RECOVERY_ACTION_PAUSE                                  => 'pause';
    push @EXPORT, qw(RECOVERY_ACTION_PAUSE);
use constant RECOVERY_ACTION_PROMOTE                                => 'promote';
    push @EXPORT, qw(RECOVERY_ACTION_PROMOTE);
use constant RECOVERY_ACTION_SHUTDOWN                               => 'shutdown';
    push @EXPORT, qw(RECOVERY_ACTION_SHUTDOWN);

####################################################################################################################################
# Configuration section constants
####################################################################################################################################
use constant CONFIG_SECTION_GLOBAL                                  => 'global';
    push @EXPORT, qw(CONFIG_SECTION_GLOBAL);
use constant CONFIG_SECTION_STANZA                                  => 'stanza';
    push @EXPORT, qw(CONFIG_SECTION_STANZA);

####################################################################################################################################
# Module variables
####################################################################################################################################
my %oOption;                # Option hash
my $strCommand;             # Command (backup, archive-get, ...)
my $strCommandHelp;         # The command that help is being generate for
my $bInitLog = false;       # Has logging been initialized yet?

####################################################################################################################################
# configLogging - configure logging based on options
####################################################################################################################################
sub configLogging
{
    my $bLogInitForce = shift;

    if ($bInitLog || (defined($bLogInitForce) && $bLogInitForce))
    {
        logLevelSet(
            cfgOptionValid(CFGOPT_LOG_LEVEL_FILE) ? cfgOption(CFGOPT_LOG_LEVEL_FILE) : OFF,
            cfgOptionValid(CFGOPT_LOG_LEVEL_CONSOLE) ? cfgOption(CFGOPT_LOG_LEVEL_CONSOLE) : OFF,
            cfgOptionValid(CFGOPT_LOG_LEVEL_STDERR) ? cfgOption(CFGOPT_LOG_LEVEL_STDERR) : OFF,
            cfgOptionValid(CFGOPT_LOG_TIMESTAMP) ? cfgOption(CFGOPT_LOG_TIMESTAMP) : undef);

        $bInitLog = true;
    }
}

push @EXPORT, qw(configLogging);

####################################################################################################################################
# configLoad - load configuration
#
# Additional conditions that cannot be codified by the OptionRule hash are also tested here.
####################################################################################################################################
sub configLoad
{
    my $bInitLogging = shift;

    # Clear option in case it was loaded before
    %oOption = ();

    # Build hash with all valid command-line options
    my @stryOptionAllow;

    for (my $iOptionId = 0; $iOptionId < cfgOptionTotal(); $iOptionId++)
    {
        my $strKey = cfgOptionName($iOptionId);

        foreach my $bAltName (false, true)
        {
            my $strOptionName = $strKey;

            if ($bAltName)
            {
                if (!defined(cfgOptionRuleNameAlt($iOptionId)))
                {
                    next;
                }

                $strOptionName = cfgOptionRuleNameAlt($iOptionId);
            }

            my $strOption = $strOptionName;

            if (cfgOptionRuleType($iOptionId) eq CFGOPTRULE_TYPE_HASH)
            {
                $strOption .= '=s@';
            }
            elsif (cfgOptionRuleType($iOptionId) ne CFGOPTRULE_TYPE_BOOLEAN)
            {
                $strOption .= '=s';
            }

            push(@stryOptionAllow, $strOption);

            # Check if the option can be negated
            if (cfgOptionRuleNegate($iOptionId))
            {
                push(@stryOptionAllow, 'no-' . $strOptionName);
            }
        }
    }

    # Get command-line options
    my %oOptionTest;

    # If nothing was passed on the command line then display help
    if (@ARGV == 0)
    {
        cfgCommandSet(CFGCMD_HELP);
    }
    # Else process command line options
    else
    {
        # Parse command line options
        if (!GetOptions(\%oOptionTest, @stryOptionAllow))
        {
            $strCommand = cfgCommandName(CFGCMD_HELP);
            return false;
        }

        # Validate and store options
        my $bHelp = false;

        if (defined($ARGV[0]) && $ARGV[0] eq cfgCommandName(CFGCMD_HELP) && defined($ARGV[1]))
        {
            $bHelp = true;
            $strCommandHelp = $ARGV[1];
            $ARGV[0] = $ARGV[1];
        }

        optionValidate(\%oOptionTest, $bHelp);

        if ($bHelp)
        {
            cfgCommandSet(CFGCMD_HELP);
        }
    }

    # If this is not the remote and logging is allowed (to not overwrite log levels for tests) then set the log level so that
    # INFO/WARN messages can be displayed (the user may still disable them).  This should be run before any WARN logging is
    # generated.
    if (!defined($bInitLogging) || $bInitLogging)
    {
        configLogging(true);
    }

    # Log the command begin
    commandBegin();

    # Neutralize the umask to make the repository file/path modes more consistent
    if (cfgOptionTest(CFGOPT_NEUTRAL_UMASK) && cfgOption(CFGOPT_NEUTRAL_UMASK))
    {
        umask(0000);
    }

    # Set db-cmd and backup-cmd to defaults if they are not set.  The command depends on the currently running exe so can't be
    # calculated correctly in the C Library -- perhaps in the future this value will be passed in or set some other way
    if (cfgOptionValid(CFGOPT_BACKUP_CMD) && cfgOptionTest(CFGOPT_BACKUP_HOST) && !cfgOptionTest(CFGOPT_BACKUP_CMD))
    {
        cfgOptionSet(CFGOPT_BACKUP_CMD, BACKREST_BIN);
        $oOption{cfgOptionName(CFGOPT_BACKUP_CMD)}{source} = SOURCE_DEFAULT;
    }

    if (cfgOptionValid(CFGOPT_DB_CMD))
    {
        for (my $iOptionIdx = 1; $iOptionIdx <= cfgOptionIndexTotal(CFGOPT_DB_HOST); $iOptionIdx++)
        {
            if (cfgOptionTest(cfgOptionIndex(CFGOPT_DB_HOST, $iOptionIdx)) &&
                !cfgOptionTest(cfgOptionIndex(CFGOPT_DB_CMD, $iOptionIdx)))
            {
                cfgOptionSet(cfgOptionIndex(CFGOPT_DB_CMD, $iOptionIdx), BACKREST_BIN);
                $oOption{cfgOptionIndex(CFGOPT_DB_CMD, $iOptionIdx)}{source} = SOURCE_DEFAULT;
            }
        }
    }

    # Protocol timeout should be greater than db timeout
    if (cfgOptionTest(CFGOPT_DB_TIMEOUT) && cfgOptionTest(CFGOPT_PROTOCOL_TIMEOUT) &&
        cfgOption(CFGOPT_PROTOCOL_TIMEOUT) <= cfgOption(CFGOPT_DB_TIMEOUT))
    {
        # If protocol-timeout is default then increase it to be greater than db-timeout
        if (cfgOptionSource(CFGOPT_PROTOCOL_TIMEOUT) eq SOURCE_DEFAULT)
        {
            cfgOptionSet(CFGOPT_PROTOCOL_TIMEOUT, cfgOption(CFGOPT_DB_TIMEOUT) + 30);
        }
        else
        {
            confess &log(ERROR,
                "'" . cfgOption(CFGOPT_PROTOCOL_TIMEOUT) . "' is not valid for '" .
                    cfgOptionName(CFGOPT_PROTOCOL_TIMEOUT) . "' option\n" .
                    "HINT: 'protocol-timeout' option should be greater than 'db-timeout' option.",
                ERROR_OPTION_INVALID_VALUE);
        }
    }

    # Make sure that backup and db are not both remote
    if (cfgOptionTest(CFGOPT_DB_HOST) && cfgOptionTest(CFGOPT_BACKUP_HOST))
    {
        confess &log(ERROR, 'db and backup cannot both be configured as remote', ERROR_CONFIG);
    }

    # Warn when retention-full is not set
    if (cfgOptionValid(CFGOPT_RETENTION_FULL) && !cfgOptionTest(CFGOPT_RETENTION_FULL))
    {
        &log(WARN,
            "option retention-full is not set, the repository may run out of space\n" .
                "HINT: to retain full backups indefinitely (without warning), set option 'retention-full' to the maximum.");
    }

    # If archive retention is valid for the command, then set archive settings
    if (cfgOptionValid(CFGOPT_RETENTION_ARCHIVE))
    {
        my $strArchiveRetentionType = cfgOption(CFGOPT_RETENTION_ARCHIVE_TYPE, false);
        my $iArchiveRetention = cfgOption(CFGOPT_RETENTION_ARCHIVE, false);
        my $iFullRetention = cfgOption(CFGOPT_RETENTION_FULL, false);
        my $iDifferentialRetention = cfgOption(CFGOPT_RETENTION_DIFF, false);

        my $strMsgArchiveOff = "WAL segments will not be expired: option '" . cfgOptionName(CFGOPT_RETENTION_ARCHIVE_TYPE) .
             "=${strArchiveRetentionType}' but ";

        # If the archive retention is not explicitly set then determine what it should be set to so the user does not have to.
        if (!defined($iArchiveRetention))
        {
            # If retention-archive-type is default, then if retention-full is set, set the retention-archive to this value,
            # else ignore archiving
            if ($strArchiveRetentionType eq BACKUP_TYPE_FULL)
            {
                if (defined($iFullRetention))
                {
                    cfgOptionSet(CFGOPT_RETENTION_ARCHIVE, $iFullRetention);
                }
            }
            elsif ($strArchiveRetentionType eq BACKUP_TYPE_DIFF)
            {
                # if retention-diff is set then user must have set it
                if (defined($iDifferentialRetention))
                {
                    cfgOptionSet(CFGOPT_RETENTION_ARCHIVE, $iDifferentialRetention);
                }
                else
                {
                    &log(WARN,
                        $strMsgArchiveOff . "neither option '" . cfgOptionName(CFGOPT_RETENTION_ARCHIVE) .
                            "' nor option '" .  cfgOptionName(CFGOPT_RETENTION_DIFF) . "' is set");
                }
            }
            elsif ($strArchiveRetentionType eq BACKUP_TYPE_INCR)
            {
                &log(WARN, $strMsgArchiveOff . "option '" . cfgOptionName(CFGOPT_RETENTION_ARCHIVE) . "' is not set");
            }
        }
        else
        {
            # If retention-archive is set then check retention-archive-type and issue a warning if the corresponding setting is
            # UNDEF since UNDEF means backups will not be expired but they should be in the practice of setting this
            # value even though expiring the archive itself is OK and will be performed.
            if ($strArchiveRetentionType eq BACKUP_TYPE_DIFF && !defined($iDifferentialRetention))
            {
                &log(WARN,
                    "option '" . cfgOptionName(CFGOPT_RETENTION_DIFF) . "' is not set for '" .
                        cfgOptionName(CFGOPT_RETENTION_ARCHIVE_TYPE) . "=" . &BACKUP_TYPE_DIFF . "' \n" .
                        "HINT: to retain differential backups indefinitely (without warning), set option '" .
                        cfgOptionName(CFGOPT_RETENTION_DIFF) . "' to the maximum.");
            }
        }
    }

    # Warn if ARCHIVE_MAX_MB is present
    if (cfgOptionValid(CFGOPT_ARCHIVE_MAX_MB) && cfgOptionTest(CFGOPT_ARCHIVE_MAX_MB))
    {
        &log(WARN,
            "'" . cfgOptionName(CFGOPT_ARCHIVE_MAX_MB) . "' is no longer not longer valid, use '" .
            cfgOptionName(CFGOPT_ARCHIVE_QUEUE_MAX) . "' instead");
    }

    return true;
}

push @EXPORT, qw(configLoad);

####################################################################################################################################
# optionValueGet
#
# Find the value of an option using both the regular and alt values.  Error if both are defined.
####################################################################################################################################
sub optionValueGet
{
    my $strOption = shift;
    my $hOption = shift;

    my $strValue = $hOption->{$strOption};

    # Some options have an alternate name so check for that as well
    my $iOptionId = cfgOptionId($strOption);

    if (defined(cfgOptionRuleNameAlt($iOptionId)))
    {
        my $strOptionAlt = cfgOptionRuleNameAlt($iOptionId);
        my $strValueAlt = $hOption->{$strOptionAlt};

        if (defined($strValueAlt))
        {
            if (!defined($strValue))
            {
                $strValue = $strValueAlt;

                delete($hOption->{$strOptionAlt});
                $hOption->{$strOption} = $strValue;
            }
            else
            {
                confess &log(ERROR, "'${strOption}' and '${strOptionAlt}' cannot both be defined", ERROR_OPTION_INVALID_VALUE);
            }
        }
    }

    return $strValue;
}

####################################################################################################################################
# optionValidate
#
# Make sure the command-line options are valid based on the command.
####################################################################################################################################
sub optionValidate
{
    my $oOptionTest = shift;
    my $bHelp = shift;

    # Check that the command is present and valid
    $strCommand = $ARGV[0];

    if (!defined($strCommand))
    {
        confess &log(ERROR, "command must be specified", ERROR_COMMAND_REQUIRED);
    }

    my $iCommandId = cfgCommandId($strCommand);

    if ($iCommandId == -1)
    {
        confess &log(ERROR, "invalid command ${strCommand}", ERROR_COMMAND_INVALID);
    }

    # Hash to store contents of the config file.  The file will be loaded once the config dependency is resolved unless all options
    # are set on the command line or --no-config is specified.
    my $oConfig;
    my $bConfigExists = true;

    # Keep track of unresolved dependencies
    my $bDependUnresolved = true;
    my %oOptionResolved;

    # Loop through all possible options
    while ($bDependUnresolved)
    {
        # Assume that all dependencies will be resolved in this loop
        $bDependUnresolved = false;

        for (my $iOptionId = 0; $iOptionId < cfgOptionTotal(); $iOptionId++)
        {
            my $strOption = cfgOptionName($iOptionId);

            # Skip the option if it has been resolved in a prior loop
            if (defined($oOptionResolved{$strOption}))
            {
                next;
            }

            # Determine if an option is valid for a command
            $oOption{$strOption}{valid} = cfgOptionRuleValid($iCommandId, $iOptionId);

            if (!$oOption{$strOption}{valid})
            {
                $oOptionResolved{$strOption} = true;
                next;
            }

            # Store the option value
            my $strValue = optionValueGet($strOption, $oOptionTest);

            # Check to see if an option can be negated.  Make sure that it is not set and negated at the same time.
            my $bNegate = false;

            if (cfgOptionRuleNegate($iOptionId))
            {
                $bNegate = defined($$oOptionTest{'no-' . $strOption});

                if ($bNegate && defined($strValue))
                {
                    confess &log(ERROR, "option '${strOption}' cannot be both set and negated", ERROR_OPTION_NEGATE);
                }

                if ($bNegate && cfgOptionRuleType($iOptionId) == CFGOPTRULE_TYPE_BOOLEAN)
                {
                    $strValue = false;
                }
            }

            # Check dependency for the command then for the option
            my $bDependResolved = true;
            my $strDependOption;
            my $strDependValue;
            my $strDependType;

            if (cfgOptionRuleDepend($iCommandId, $iOptionId))
            {
                # Check if the depend option has a value
                my $iDependOptionId = cfgOptionRuleDependOption($iCommandId, $iOptionId);
                $strDependOption = cfgOptionName($iDependOptionId);
                $strDependValue = $oOption{$strDependOption}{value};

                # Make sure the depend option has been resolved, otherwise skip this option for now
                if (!defined($oOptionResolved{$strDependOption}))
                {
                    $bDependUnresolved = true;
                    next;
                }

                if (!defined($strDependValue))
                {
                    $bDependResolved = false;
                    $strDependType = 'source';
                }

                # If a depend value exists, make sure the option value matches
                if ($bDependResolved && cfgOptionRuleDependValueTotal($iCommandId, $iOptionId) == 1 &&
                    cfgOptionRuleDependValue($iCommandId, $iOptionId, 0) ne $strDependValue)
                {
                    $bDependResolved = false;
                    $strDependType = 'value';
                }

                # If a depend list exists, make sure the value is in the list
                if ($bDependResolved && cfgOptionRuleDependValueTotal($iCommandId, $iOptionId) > 1 &&
                    !cfgOptionRuleDependValueValid($iCommandId, $iOptionId, $strDependValue))
                {
                    $bDependResolved = false;
                    $strDependType = 'list';
                }
            }

            # If the option value is undefined and not negated, see if it can be loaded from the config file
            if (!defined($strValue) && !$bNegate && $strOption ne cfgOptionName(CFGOPT_CONFIG) &&
                defined(cfgOptionRuleSection($iOptionId)) && $bDependResolved)
            {
                # If the config option has not been resolved yet then continue processing
                if (!defined($oOptionResolved{cfgOptionName(CFGOPT_CONFIG)}) ||
                    !defined($oOptionResolved{cfgOptionName(CFGOPT_STANZA)}))
                {
                    $bDependUnresolved = true;
                    next;
                }

                # If the config option is defined try to get the option from the config file
                if ($bConfigExists && defined($oOption{cfgOptionName(CFGOPT_CONFIG)}{value}))
                {
                    # Attempt to load the config file if it has not been loaded
                    if (!defined($oConfig))
                    {
                        my $strConfigFile = $oOption{cfgOptionName(CFGOPT_CONFIG)}{value};
                        $bConfigExists = -e $strConfigFile;

                        if ($bConfigExists)
                        {
                            if (!-f $strConfigFile)
                            {
                                confess &log(ERROR, "'${strConfigFile}' is not a file", ERROR_FILE_INVALID);
                            }

                            # Load Storage::Helper module
                            require pgBackRest::Storage::Helper;
                            pgBackRest::Storage::Helper->import();

                            $oConfig = iniParse(${storageLocal->('/')->get($strConfigFile)}, {bRelaxed => true});
                        }
                    }

                    # Get the section that the value should be in
                    my $strSection = cfgOptionRuleSection($iOptionId);

                    # Always check for the option in the stanza section first
                    if (cfgOptionTest(CFGOPT_STANZA))
                    {
                        $strValue = optionValueGet($strOption, $$oConfig{cfgOption(CFGOPT_STANZA)});
                    }

                    # Only continue searching when strSection != CONFIG_SECTION_STANZA.  Some options (e.g. db-path) can only be
                    # configured in the stanza section.
                    if (!defined($strValue) && $strSection ne CONFIG_SECTION_STANZA)
                    {
                        # Check the stanza command section
                        if (cfgOptionTest(CFGOPT_STANZA))
                        {
                            $strValue = optionValueGet($strOption, $$oConfig{cfgOption(CFGOPT_STANZA) . ":${strCommand}"});
                        }

                        # Check the global command section
                        if (!defined($strValue))
                        {
                            $strValue = optionValueGet($strOption, $$oConfig{&CONFIG_SECTION_GLOBAL . ":${strCommand}"});
                        }

                        # Finally check the global section
                        if (!defined($strValue))
                        {
                            $strValue = optionValueGet($strOption, $$oConfig{&CONFIG_SECTION_GLOBAL});
                        }
                    }

                    # Fix up data types
                    if (defined($strValue))
                    {
                        # The empty string is undefined
                        if ($strValue eq '')
                        {
                            $strValue = undef;
                        }
                        # Convert Y or N to boolean
                        elsif (cfgOptionRuleType($iOptionId) == CFGOPTRULE_TYPE_BOOLEAN)
                        {
                            if ($strValue eq 'y')
                            {
                                $strValue = true;
                            }
                            elsif ($strValue eq 'n')
                            {
                                $strValue = false;
                            }
                            else
                            {
                                confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option",
                                             ERROR_OPTION_INVALID_VALUE);
                            }
                        }
                        # Convert a list of key/value pairs to a hash
                        elsif (cfgOptionRuleType($iOptionId) == CFGOPTRULE_TYPE_HASH)
                        {
                            my @oValue = ();

                            # If there is only one key/value
                            if (ref(\$strValue) eq 'SCALAR')
                            {
                                push(@oValue, $strValue);
                            }
                            # Else if there is an array of values
                            else
                            {
                                @oValue = @{$strValue};
                            }

                            # Reset the value hash
                            $strValue = {};

                            # Iterate and parse each key/value pair
                            foreach my $strHash (@oValue)
                            {
                                my $iEqualIdx = index($strHash, '=');

                                if ($iEqualIdx < 1 || $iEqualIdx == length($strHash) - 1)
                                {
                                    confess &log(ERROR, "'${strHash}' is not valid for '${strOption}' option",
                                                 ERROR_OPTION_INVALID_VALUE);
                                }

                                my $strHashKey = substr($strHash, 0, $iEqualIdx);
                                my $strHashValue = substr($strHash, length($strHashKey) + 1);

                                $$strValue{$strHashKey} = $strHashValue;
                            }
                        }
                        # In all other cases the value should be scalar
                        elsif (ref(\$strValue) ne 'SCALAR')
                        {
                            confess &log(
                                ERROR, "option '${strOption}' cannot be specified multiple times", ERROR_OPTION_MULTIPLE_VALUE);
                        }

                        $oOption{$strOption}{source} = SOURCE_CONFIG;
                    }
                }
            }

            if (cfgOptionRuleDepend($iCommandId, $iOptionId) && !$bDependResolved && defined($strValue))
            {
                my $strError = "option '${strOption}' not valid without option ";
                my $iDependOptionId = cfgOptionId($strDependOption);

                if ($strDependType eq 'source')
                {
                    confess &log(ERROR, "${strError}'${strDependOption}'", ERROR_OPTION_INVALID);
                }

                # If a depend value exists, make sure the option value matches
                if ($strDependType eq 'value')
                {
                    if (cfgOptionRuleType($iDependOptionId) == CFGOPTRULE_TYPE_BOOLEAN)
                    {
                        $strError .=
                            "'" . (cfgOptionRuleDependValue($iCommandId, $iOptionId, 0) ? '' : 'no-') . "${strDependOption}'";
                    }
                    else
                    {
                        $strError .= "'${strDependOption}' = '" . cfgOptionRuleDependValue($iCommandId, $iOptionId, 0) . "'";
                    }

                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }

                $strError .= "'${strDependOption}'";

                # If a depend list exists, make sure the value is in the list
                if ($strDependType eq 'list')
                {
                    my @oyValue;

                    for (my $iValueId = 0; $iValueId < cfgOptionRuleDependValueTotal($iCommandId, $iOptionId); $iValueId++)
                    {
                        push(@oyValue, "'" . cfgOptionRuleDependValue($iCommandId, $iOptionId, $iValueId) . "'");
                    }

                    $strError .= @oyValue == 1 ? " = $oyValue[0]" : " in (" . join(", ", @oyValue) . ")";
                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }
            }

            # Is the option defined?
            if (defined($strValue))
            {
                # Check that floats and integers are valid
                if (cfgOptionRuleType($iOptionId) == CFGOPTRULE_TYPE_INTEGER ||
                    cfgOptionRuleType($iOptionId) == CFGOPTRULE_TYPE_FLOAT)
                {
                    # Test that the string is a valid float or integer by adding 1 to it.  It's pretty hokey but it works and it
                    # beats requiring Scalar::Util::Numeric to do it properly.
                    my $bError = false;

                    eval
                    {
                        my $strTest = $strValue + 1;
                        return true;
                    }
                    or do
                    {
                        $bError = true;
                    };

                    # Check that integers are really integers
                    if (!$bError && cfgOptionRuleType($iOptionId) == CFGOPTRULE_TYPE_INTEGER &&
                        (int($strValue) . 'S') ne ($strValue . 'S'))
                    {
                        $bError = true;
                    }

                    # Error if the value did not pass tests
                    !$bError
                        or confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow list for the command then for the option
                if (cfgOptionRuleAllowList($iCommandId, $iOptionId) &&
                    !cfgOptionRuleAllowListValueValid($iCommandId, $iOptionId, $strValue))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow range for the command then for the option
                if (cfgOptionRuleAllowRange($iCommandId, $iOptionId) &&
                    ($strValue < cfgOptionRuleAllowRangeMin($iCommandId, $iOptionId) ||
                     $strValue > cfgOptionRuleAllowRangeMax($iCommandId, $iOptionId)))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_RANGE);
                }

                # Set option value
                if (cfgOptionRuleType($iOptionId) == CFGOPTRULE_TYPE_HASH && ref($strValue) eq 'ARRAY')
                {
                    foreach my $strItem (@{$strValue})
                    {
                        my $strKey;
                        my $strValue;

                        # If the keys are expected to have values
                        if (cfgOptionRuleValueHash($iOptionId))
                        {
                            # Check for = and make sure there is a least one character on each side
                            my $iEqualPos = index($strItem, '=');

                            if ($iEqualPos < 1 || length($strItem) <= $iEqualPos + 1)
                            {
                                confess &log(ERROR, "'${strItem}' not valid key/value for '${strOption}' option",
                                                    ERROR_OPTION_INVALID_PAIR);
                            }

                            $strKey = substr($strItem, 0, $iEqualPos);
                            $strValue = substr($strItem, $iEqualPos + 1);
                        }
                        # Else no values are expected so set value to true
                        else
                        {
                            $strKey = $strItem;
                            $strValue = true;
                        }

                        # Check that the key has not already been set
                        if (defined($oOption{$strOption}{$strKey}{value}))
                        {
                            confess &log(ERROR, "'${$strItem}' already defined for '${strOption}' option",
                                                ERROR_OPTION_DUPLICATE_KEY);
                        }

                        # Set key/value
                        $oOption{$strOption}{value}{$strKey} = $strValue;
                    }
                }
                else
                {
                    $oOption{$strOption}{value} = $strValue;
                }

                # If not config sourced then it must be a param
                if (!defined($oOption{$strOption}{source}))
                {
                    $oOption{$strOption}{source} = SOURCE_PARAM;
                }
            }
            # Else try to set a default
            elsif ($bDependResolved)
            {
                # Source is default for this option
                $oOption{$strOption}{source} = SOURCE_DEFAULT;

                # Check for default in command then option
                my $strDefault = cfgOptionRuleDefault($iCommandId, $iOptionId);

                # If default is defined
                if (defined($strDefault))
                {
                    # Only set default if dependency is resolved
                    $oOption{$strOption}{value} = $strDefault if !$bNegate;
                }
                # Else check required
                elsif (cfgOptionRuleRequired($iCommandId, $iOptionId) && !$bHelp)
                {
                    confess &log(ERROR,
                        "${strCommand} command requires option: ${strOption}" .
                        (defined(cfgOptionRuleHint($iCommandId, $iOptionId)) ?
                            "\nHINT: " . cfgOptionRuleHint($iCommandId, $iOptionId) : ''),
                        ERROR_OPTION_REQUIRED);
                }
            }

            $oOptionResolved{$strOption} = true;
        }
    }

    # Make sure all options specified on the command line are valid
    foreach my $strOption (sort(keys(%{$oOptionTest})))
    {
        # Strip "no-" off the option
        $strOption = $strOption =~ /^no\-/ ? substr($strOption, 3) : $strOption;

        if (!$oOption{$strOption}{valid})
        {
            confess &log(ERROR, "option '${strOption}' not valid for command '${strCommand}'", ERROR_OPTION_COMMAND);
        }
    }

    # If a config file was loaded then determine if all options are valid in the config file
    if (defined($oConfig))
    {
        configFileValidate($oConfig);
    }
}

####################################################################################################################################
# configFileValidate
#
# Determine if the configuration file contains any invalid options or placements. Not valid on remote.
####################################################################################################################################
sub configFileValidate
{
    my $oConfig = shift;

    my $bFileValid = true;

    if (!cfgCommandTest(CFGCMD_REMOTE) && !cfgCommandTest(CFGCMD_LOCAL))
    {
        foreach my $strSectionKey (keys(%$oConfig))
        {
            my ($strSection, $strCommand) = ($strSectionKey =~ m/([^:]*):*(\w*-*\w*)/);

            foreach my $strOption (keys(%{$$oConfig{$strSectionKey}}))
            {
                my $strOptionDisplay = $strOption;
                my $strValue = $$oConfig{$strSectionKey}{$strOption};

                # Is the option listed as an alternate name for another option? If so, replace it with the recognized option.
                my $strOptionAltName = optionAltName($strOption);

                if (defined($strOptionAltName))
                {
                    $strOption = $strOptionAltName;
                }

                # Is the option a valid pgbackrest option?
                if (!(cfgOptionId($strOption) != -1 || defined($strOptionAltName)))
                {
                    &log(WARN, cfgOption(CFGOPT_CONFIG) . " file contains invalid option '${strOptionDisplay}'");
                    $bFileValid = false;
                }
                else
                {
                    # Is the option valid for the command section in which it is located?
                    if (defined($strCommand) && $strCommand ne '')
                    {
                        if (!cfgOptionRuleValid(cfgCommandId($strCommand), cfgOptionId($strOption)))
                        {
                            &log(WARN, cfgOption(CFGOPT_CONFIG) . " valid option '${strOptionDisplay}' is not valid for command " .
                                "'${strCommand}'");
                            $bFileValid = false;
                        }
                    }

                    # Is the valid option a stanza-only option and not located in a global section?
                    if (cfgOptionRuleSection(cfgOptionId($strOption)) eq CONFIG_SECTION_STANZA &&
                        $strSection eq CONFIG_SECTION_GLOBAL)
                    {
                        &log(WARN,
                            cfgOption(CFGOPT_CONFIG) .  " valid option '${strOptionDisplay}' is a stanza section option and is" .
                            " not valid in section ${strSection}\n" .
                            "HINT: global options can be specified in global or stanza sections but not visa-versa");
                        $bFileValid = false;
                    }
                }
            }
        }
    }

    return $bFileValid;
}

####################################################################################################################################
# optionAltName
#
# Returns the ALT_NAME for the option if one exists.
####################################################################################################################################
sub optionAltName
{
    my $strOption = shift;

    my $strOptionAltName = undef;

    # Check if the options exists as an alternate name (e.g. db-host has altname db1-host)
    for (my $iOptionId = 0; $iOptionId < cfgOptionTotal(); $iOptionId++)
    {
        my $strKey = cfgOptionName($iOptionId);

        if (defined(cfgOptionRuleNameAlt($iOptionId)) && cfgOptionRuleNameAlt($iOptionId) eq $strOption)
        {
            $strOptionAltName = $strKey;
        }
    }

    return $strOptionAltName;
}

####################################################################################################################################
# cfgOptionIndex - return name for options that can be indexed (e.g. db1-host, db2-host).
####################################################################################################################################
sub cfgOptionIndex
{
    my $iOptionId = shift;
    my $iIndex = shift;
    my $bForce = shift;

    # If the option doesn't have a prefix it can't be indexed
    $iIndex = defined($iIndex) ? $iIndex : 1;
    my $strPrefix = cfgOptionRulePrefix($iOptionId);

    if (!defined($strPrefix))
    {
        if ($iIndex > 1)
        {
            confess &log(ASSERT, "'" . cfgOptionName($iOptionId) . "' option does not allow indexing");
        }

        return $iOptionId;
    }

    return cfgOptionId("${strPrefix}${iIndex}" . substr(cfgOptionName($iOptionId), index(cfgOptionName($iOptionId), '-')));
}

push @EXPORT, qw(cfgOptionIndex);

####################################################################################################################################
# cfgOptionSource - how was the option set?
####################################################################################################################################
sub cfgOptionSource
{
    my $iOptionId = shift;

    cfgOptionValid($iOptionId, true);

    return $oOption{cfgOptionName($iOptionId)}{source};
}

push @EXPORT, qw(cfgOptionSource);

####################################################################################################################################
# cfgOptionValid - is the option valid for the current command?
####################################################################################################################################
sub cfgOptionValid
{
    my $iOptionId = shift;
    my $bError = shift;

    # If defined then this is the command help is being generated for so all valid checks should be against that command
    my $iCommandId;

    if (defined($strCommandHelp))
    {
        $iCommandId = cfgCommandId($strCommandHelp);
    }
    # Else try to use the normal command
    elsif (defined($strCommand))
    {
        $iCommandId = cfgCommandId($strCommand);
    }

    if (defined($iCommandId) && cfgOptionRuleValid($iCommandId, $iOptionId))
    {
        return true;
    }

    if (defined($bError) && $bError)
    {
        my $strOption = cfgOptionName($iOptionId);

        if (!defined($oOption{$strOption}))
        {
            confess &log(ASSERT, "option '${strOption}' does not exist");
        }

        confess &log(ASSERT, "option '${strOption}' not valid for command '" . cfgCommandName(cfgCommandGet()) . "'");
    }

    return false;
}

push @EXPORT, qw(cfgOptionValid);

####################################################################################################################################
# cfgOption - get option value
####################################################################################################################################
sub cfgOption
{
    my $iOptionId = shift;
    my $bRequired = shift;

    cfgOptionValid($iOptionId, true);

    my $strOption = cfgOptionName($iOptionId);

    if (!defined($oOption{$strOption}{value}) && (!defined($bRequired) || $bRequired))
    {
        confess &log(ASSERT, "option ${strOption} is required");
    }

    return $oOption{$strOption}{value};
}

push @EXPORT, qw(cfgOption);

####################################################################################################################################
# cfgOptionDefault - get option default value
####################################################################################################################################
sub cfgOptionDefault
{
    my $iOptionId = shift;

    cfgOptionValid($iOptionId, true);

    return cfgOptionRuleDefault(cfgCommandId($strCommand), $iOptionId);
}

push @EXPORT, qw(cfgOptionDefault);

####################################################################################################################################
# cfgOptionSet - set option value and source
####################################################################################################################################
sub cfgOptionSet
{
    my $iOptionId = shift;
    my $oValue = shift;
    my $bForce = shift;

    my $strOption = cfgOptionName($iOptionId);

    if (!cfgOptionValid($iOptionId, !defined($bForce) || !$bForce))
    {
        $oOption{$strOption}{valid} = true;
    }

    $oOption{$strOption}{source} = SOURCE_PARAM;
    $oOption{$strOption}{value} = $oValue;
}

push @EXPORT, qw(cfgOptionSet);

####################################################################################################################################
# cfgOptionTest - test if an option exists or has a specific value
####################################################################################################################################
sub cfgOptionTest
{
    my $iOptionId = shift;
    my $strValue = shift;

    if (!cfgOptionValid($iOptionId))
    {
        return false;
    }

    if (defined($strValue))
    {
        return cfgOption($iOptionId) eq $strValue ? true : false;
    }

    return defined($oOption{cfgOptionName($iOptionId)}{value}) ? true : false;
}

push @EXPORT, qw(cfgOptionTest);

####################################################################################################################################
# cfgCommandGet - get the current command
####################################################################################################################################
sub cfgCommandGet
{
    return cfgCommandId($strCommand);
}

push @EXPORT, qw(cfgCommandGet);

####################################################################################################################################
# cfgCommandTest - test that the current command is equal to the provided value
####################################################################################################################################
sub cfgCommandTest
{
    my $iCommandIdTest = shift;

    return cfgCommandName($iCommandIdTest) eq $strCommand;
}

push @EXPORT, qw(cfgCommandTest);

####################################################################################################################################
# commandBegin
#
# Log information about the command when it begins.
####################################################################################################################################
sub commandBegin
{
    &log(
        $strCommand eq cfgCommandName(CFGCMD_INFO) ? DEBUG : INFO,
        "${strCommand} command begin " . BACKREST_VERSION . ':' . cfgCommandWrite(cfgCommandId($strCommand), true, '', false));
}

####################################################################################################################################
# commandEnd
#
# Log information about the command that ended.
####################################################################################################################################
sub commandEnd
{
    my $iExitCode = shift;
    my $strSignal = shift;

    if (defined($strCommand))
    {
        &log(
            $strCommand eq cfgCommandName(CFGCMD_INFO) ? DEBUG : INFO,
            "${strCommand} command end: " . (defined($iExitCode) && $iExitCode != 0 ?
                ($iExitCode == ERROR_TERM ? "terminated on signal " .
                    (defined($strSignal) ? "[SIG${strSignal}]" : 'from child process') :
                sprintf('aborted with exception [%03d]', $iExitCode)) :
                'completed successfully'));
    }
}

push @EXPORT, qw(commandEnd);

####################################################################################################################################
# cfgCommandSet - set current command (usually for triggering follow-on commands)
####################################################################################################################################
sub cfgCommandSet
{
    my $iCommandId = shift;

    commandEnd();

    $strCommand = cfgCommandName($iCommandId);

    commandBegin();
}

push @EXPORT, qw(cfgCommandSet);

####################################################################################################################################
# cfgCommandWrite - using the options for the current command, write the command string for another command
#
# For example, this can be used to write the archive-get command for recovery.conf during a restore.
####################################################################################################################################
sub cfgCommandWrite
{
    my $iNewCommandId = shift;
    my $bIncludeConfig = shift;
    my $strExeString = shift;
    my $bIncludeCommand = shift;
    my $oOptionOverride = shift;

    # Set defaults
    $strExeString = defined($strExeString) ? $strExeString : BACKREST_BIN;
    $bIncludeConfig = defined($bIncludeConfig) ? $bIncludeConfig : false;
    $bIncludeCommand = defined($bIncludeCommand) ? $bIncludeCommand : true;

    # Iterate the options to figure out which ones are not default and need to be written out to the new command string
    for (my $iOptionId = 0; $iOptionId < cfgOptionTotal(); $iOptionId++)
    {
        my $strOption = cfgOptionName($iOptionId);

        # Skip option if it is secure and should not be output in logs or the command line
        next if (cfgOptionRuleSecure($iOptionId));

        # Process any option overrides first
        if (defined($$oOptionOverride{$iOptionId}))
        {
            if (defined($$oOptionOverride{$iOptionId}{value}))
            {
                $strExeString .= cfgCommandWriteOptionFormat($strOption, false, {value => $$oOptionOverride{$iOptionId}{value}});
            }
        }
        # else look for non-default options in the current configuration
        elsif (cfgOptionRuleValid($iNewCommandId, $iOptionId) &&
               defined($oOption{$strOption}{value}) &&
               ($bIncludeConfig ? $oOption{$strOption}{source} ne SOURCE_DEFAULT : $oOption{$strOption}{source} eq SOURCE_PARAM))
        {
            my $oValue;
            my $bMulti = false;

            # If this is a hash then it will break up into multple command-line options
            if (ref($oOption{$strOption}{value}) eq 'HASH')
            {
                $oValue = $oOption{$strOption}{value};
                $bMulti = true;
            }
            # Else a single value but store it in a hash anyway to make processing below simpler
            else
            {
                $oValue = {value => $oOption{$strOption}{value}};
            }

            $strExeString .= cfgCommandWriteOptionFormat($strOption, $bMulti, $oValue);
        }
    }

    if ($bIncludeCommand)
    {
        $strExeString .= ' ' . cfgCommandName($iNewCommandId);
    }

    return $strExeString;
}

push @EXPORT, qw(cfgCommandWrite);

# Helper function for cfgCommandWrite() to correctly format options for command-line usage
sub cfgCommandWriteOptionFormat
{
    my $strOption = shift;
    my $bMulti = shift;
    my $oValue = shift;

    # Loops though all keys in the hash
    my $strOptionFormat = '';
    my $strParam;

    foreach my $strKey (sort(keys(%$oValue)))
    {
        # Get the value - if the original value was a hash then the key must be prefixed
        my $strValue = ($bMulti ?  "${strKey}=" : '') . $$oValue{$strKey};

        # Handle the no- prefix for boolean values
        if (cfgOptionRuleType(cfgOptionId($strOption)) == CFGOPTRULE_TYPE_BOOLEAN)
        {
            $strParam = '--' . ($strValue ? '' : 'no-') . $strOption;
        }
        else
        {
            $strParam = "--${strOption}=${strValue}";
        }

        # Add quotes if the value has spaces in it
        $strOptionFormat .= ' ' . (index($strValue, " ") != -1 ? "\"${strParam}\"" : $strParam);
    }

    return $strOptionFormat;
}

1;
