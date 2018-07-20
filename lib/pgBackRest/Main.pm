####################################################################################################################################
# pgBackRest - Reliable PostgreSQL Backup & Restore
####################################################################################################################################
package pgBackRest::Main;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

# Convert die to confess to capture the stack trace
$SIG{__DIE__} = sub {Carp::confess @_};

use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Version;

####################################################################################################################################
# Set config JSON separately to avoid exposing secrets in the stack trace
####################################################################################################################################
my $strConfigJson;
my $strConfigBin;
my $bConfigLoaded = false;

sub mainConfigSet
{
    $strConfigBin = shift;
    $strConfigJson = shift;
}

####################################################################################################################################
# Main entry point for the library
####################################################################################################################################
sub main
{
    my $strCommand = shift;
    my @stryCommandArg = @_;

    # Run in eval block to catch errors
    # ------------------------------------------------------------------------------------------------------------------------------
    my $iResult = 0;
    my $bErrorC = false;
    my $strMessage = '';

    eval
    {
        # Load command line parameters and config -- pass config by reference to hide secrets more than for efficiency
        # --------------------------------------------------------------------------------------------------------------------------
        if (!$bConfigLoaded)
        {
            configLoad(undef, $strConfigBin, $strCommand, \$strConfigJson);

            # Set test options
            if (cfgOptionTest(CFGOPT_TEST) && cfgOption(CFGOPT_TEST))
            {
                testSet(cfgOption(CFGOPT_TEST), cfgOption(CFGOPT_TEST_DELAY), cfgOption(CFGOPT_TEST_POINT, false));
            }

            $bConfigLoaded = true;
        }
        else
        {
            cfgCommandSet(cfgCommandId($strCommand));
        }

        # Process archive-push command
        # --------------------------------------------------------------------------------------------------------------------------
        if (cfgCommandTest(CFGCMD_ARCHIVE_PUSH))
        {
            # Load module dynamically
            require pgBackRest::Archive::Push::Push;
            pgBackRest::Archive::Push::Push->import();

            new pgBackRest::Archive::Push::Push()->process($stryCommandArg[0]);
        }

        # Process archive-get command
        # --------------------------------------------------------------------------------------------------------------------------
        elsif (cfgCommandTest(CFGCMD_ARCHIVE_GET))
        {
            # Load module dynamically
            require pgBackRest::Archive::Get::Get;
            pgBackRest::Archive::Get::Get->import();

            $iResult = new pgBackRest::Archive::Get::Get()->process(\@stryCommandArg);
        }

        # Process remote command
        # --------------------------------------------------------------------------------------------------------------------------
        elsif (cfgCommandTest(CFGCMD_REMOTE))
        {
            # Set log levels
            cfgOptionSet(CFGOPT_LOG_LEVEL_STDERR, PROTOCOL, true);
            logLevelSet(OFF, OFF, cfgOption(CFGOPT_LOG_LEVEL_STDERR));

            if (cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_BACKUP) &&
                !cfgOptionTest(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3) &&
                !-e cfgOption(CFGOPT_REPO_PATH))
            {
                confess &log(ERROR,
                    cfgOptionName(CFGOPT_REPO_PATH) . ' \'' . cfgOption(CFGOPT_REPO_PATH) . '\' does not exist',
                    ERROR_PATH_MISSING);
            }

            # Load module dynamically
            require pgBackRest::Protocol::Remote::Minion;
            pgBackRest::Protocol::Remote::Minion->import();

            # Create the remote object
            my $oRemote = new pgBackRest::Protocol::Remote::Minion(
                cfgOption(CFGOPT_BUFFER_SIZE), cfgOption(CFGOPT_PROTOCOL_TIMEOUT));

            # Process remote requests
            $oRemote->process(cfgOption(CFGOPT_LOCK_PATH), cfgOption(CFGOPT_COMMAND), cfgOption(CFGOPT_STANZA, false));
        }

        # Process local command
        # --------------------------------------------------------------------------------------------------------------------------
        elsif (cfgCommandTest(CFGCMD_LOCAL))
        {
            # Set log levels
            cfgOptionSet(CFGOPT_LOG_LEVEL_STDERR, PROTOCOL, true);
            logLevelSet(OFF, OFF, cfgOption(CFGOPT_LOG_LEVEL_STDERR));

            # Load module dynamically
            require pgBackRest::Protocol::Local::Minion;
            pgBackRest::Protocol::Local::Minion->import();

            # Create the local object
            my $oLocal = new pgBackRest::Protocol::Local::Minion();

            # Process local requests
            $oLocal->process();
        }

        # Process check command
        # --------------------------------------------------------------------------------------------------------------------------
        elsif (cfgCommandTest(CFGCMD_CHECK))
        {
            # Load module dynamically
            require pgBackRest::Check::Check;
            pgBackRest::Check::Check->import();

            $iResult = new pgBackRest::Check::Check()->process();
        }

        # Process start/stop commands
        # --------------------------------------------------------------------------------------------------------------------------
        elsif (cfgCommandTest(CFGCMD_START))
        {
            lockStart();
        }
        elsif (cfgCommandTest(CFGCMD_STOP))
        {
            lockStop();
        }
        else
        {
            # Check that the repo path exists
            require pgBackRest::Protocol::Storage::Helper;
            pgBackRest::Protocol::Storage::Helper->import();

            if (isRepoLocal() && !cfgOptionTest(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3) && !storageRepo()->pathExists(''))
            {
                confess &log(ERROR,
                    cfgOptionName(CFGOPT_REPO_PATH) . ' \'' . cfgOption(CFGOPT_REPO_PATH) . '\' does not exist',
                    ERROR_PATH_MISSING);
            }

            # Process info command
            # ----------------------------------------------------------------------------------------------------------------------
            if (cfgCommandTest(CFGCMD_INFO))
            {
                # Load module dynamically
                require pgBackRest::Info;
                pgBackRest::Info->import();

                new pgBackRest::Info()->process();
            }
            else
            {
                # Open log file
                require pgBackRest::Storage::Helper;
                pgBackRest::Storage::Helper->import();

                logFileSet(
                    storageLocal(),
                    cfgOption(CFGOPT_LOG_PATH) . '/' . cfgOption(CFGOPT_STANZA) . '-' . lc(cfgCommandName(cfgCommandGet())));

                # Process delete command
                # --------------------------------------------------------------------------------------------------------------
                if (cfgCommandTest(CFGCMD_STANZA_DELETE))
                {
                    # Load module dynamically
                    require pgBackRest::Stanza;
                    pgBackRest::Stanza->import();

                    new pgBackRest::Stanza()->process();
                }
                # Process restore command
                # ------------------------------------------------------------------------------------------------------------------
                elsif (cfgCommandTest(CFGCMD_RESTORE))
                {
                    # Check locality
                    if (!isDbLocal())
                    {
                        confess &log(ERROR,
                            cfgCommandName(cfgCommandGet()) . ' command must be run on the PostgreSQL host', ERROR_HOST_INVALID);
                    }

                    # Load module dynamically
                    require pgBackRest::Restore;
                    pgBackRest::Restore->import();

                    # Do the restore
                    new pgBackRest::Restore()->process();
                }
                else
                {
                    # Check if processes have been stopped
                    lockStopTest();

                    # Check locality
                    if (!isRepoLocal())
                    {
                        confess &log(ERROR,
                            cfgCommandName(cfgCommandGet()) . ' command must be run on the repository host', ERROR_HOST_INVALID);
                    }

                    # Process stanza-create and stanza-upgrade commands
                    # --------------------------------------------------------------------------------------------------------------
                    if (cfgCommandTest(CFGCMD_STANZA_CREATE) || cfgCommandTest(CFGCMD_STANZA_UPGRADE))
                    {
                        # Load module dynamically
                        require pgBackRest::Stanza;
                        pgBackRest::Stanza->import();

                        $iResult = new pgBackRest::Stanza()->process();
                    }

                    # Process backup command
                    # --------------------------------------------------------------------------------------------------------------
                    elsif (cfgCommandTest(CFGCMD_BACKUP))
                    {
                        # Load module dynamically
                        require pgBackRest::Backup::Backup;
                        pgBackRest::Backup::Backup->import();

                        new pgBackRest::Backup::Backup()->process();
                    }

                    # Process expire command
                    # --------------------------------------------------------------------------------------------------------------
                    elsif (cfgCommandTest(CFGCMD_EXPIRE))
                    {
                        # Load module dynamically
                        require pgBackRest::Expire;
                        pgBackRest::Expire->import();

                        new pgBackRest::Expire()->process();
                    }
                }
            }
        }

        return 1;
    }

    # Check for errors
    # ------------------------------------------------------------------------------------------------------------------------------
    or do
    {
        # Perl 5.10 seems to have a problem propogating errors up through a large call stack, so in the case that the error arrives
        # blank just use the last logged error instead.  Don't do this in all cases because newer Perls seem to work fine and there
        # are other errors that could be arriving in $EVAL_ERROR.
        my $oException = defined($EVAL_ERROR) && length($EVAL_ERROR) > 0 ? $EVAL_ERROR : logErrorLast();

        # If a backrest exception
        if (isException(\$oException))
        {
            $iResult = $oException->code();
            $bErrorC = $oException->errorC();

            # Only return message if we are in an async process since this will not be logged to the console
            if (!$bConfigLoaded && cfgOption(CFGOPT_ARCHIVE_ASYNC))
            {
                $strMessage = $oException->message();
            }
        }
        # Else a regular Perl exception
        else
        {
            $iResult = ERROR_UNHANDLED;
            $strMessage =
                'process terminated due to an unhandled exception' .
                (defined($oException) ? ":\n${oException}" : ': [exception not defined]');
        }
    };

    # Return result and error message if the result is an error
    return $iResult, $bErrorC, $strMessage;
}

####################################################################################################################################
# Do any cleanup required when the perl process is about to be shut down
####################################################################################################################################
sub mainCleanup
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iExitCode,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::mainCleanup', \@_,
            {name => 'iExitCode', required => false},
        );

    # Don't fail if the remote can't be closed
    eval
    {
        protocolDestroy(undef, undef, defined($iExitCode) && ($iExitCode == 0 || $iExitCode == 1));
        return true;
    }
    # this eval exists only to suppress protocol shutdown errors so original error will not be lost
    or do {};

    # Don't fail if the lock can't be released (it will be freed by the system though the file will remain)
    eval
    {
        lockRelease(false);
        return true;
    }
    # this eval exists only to suppress lock errors so original error will not be lost
    or do {};

    # Log return values if any
    return logDebugReturn($strOperation);
}

1;
