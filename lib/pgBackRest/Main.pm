####################################################################################################################################
# pgBackRest - Reliable PostgreSQL Backup & Restore
####################################################################################################################################
package pgBackRest::Main;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

# Convert die to confess to capture the stack trace
$SIG{__DIE__} = sub { Carp::confess @_ };

use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Exit;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Version;

####################################################################################################################################
# Main entry point for the library
####################################################################################################################################
sub main
{
    my $strBackRestBin = shift;
    @ARGV = @_;

    ################################################################################################################################
    # Run in eval block to catch errors
    ################################################################################################################################
    eval
    {
        ############################################################################################################################
        # Load command line parameters and config
        ############################################################################################################################
        backrestBinSet($strBackRestBin);
        my $bConfigResult = configLoad();

        # Display help and version
        if (cfgCommandTest(CFGCMD_HELP) || cfgCommandTest(CFGCMD_VERSION))
        {
            # Load module dynamically
            require pgBackRest::Config::ConfigHelp;
            pgBackRest::Config::ConfigHelp->import();

            # Generate help and exit
            configHelp($ARGV[1], $ARGV[2], cfgCommandTest(CFGCMD_VERSION), $bConfigResult);
            exitSafe(0);
        }

        # Set test options
        if (cfgOptionTest(CFGOPT_TEST) && cfgOption(CFGOPT_TEST))
        {
            testSet(cfgOption(CFGOPT_TEST), cfgOption(CFGOPT_TEST_DELAY), cfgOption(CFGOPT_TEST_POINT, false));
        }

        ############################################################################################################################
        # Process archive-push command
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_ARCHIVE_PUSH))
        {
            # Load module dynamically
            require pgBackRest::Archive::Push::Push;
            pgBackRest::Archive::Push::Push->import();

            exitSafe(new pgBackRest::Archive::Push::Push()->process($ARGV[1]));
        }

        ############################################################################################################################
        # Process archive-get command
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_ARCHIVE_GET))
        {
            # Load module dynamically
            require pgBackRest::Archive::Get::Get;
            pgBackRest::Archive::Get::Get->import();

            exitSafe(new pgBackRest::Archive::Get::Get()->process());
        }

        ############################################################################################################################
        # Process remote commands
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_REMOTE))
        {
            # Set log levels
            cfgOptionSet(CFGOPT_LOG_LEVEL_STDERR, PROTOCOL, true);
            logLevelSet(OFF, OFF, cfgOption(CFGOPT_LOG_LEVEL_STDERR));

            if (cfgOptionTest(CFGOPT_TYPE, CFGOPTVAL_REMOTE_TYPE_BACKUP) &&
                !cfgOptionTest(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3) &&
                !-e cfgOption(CFGOPT_REPO_PATH))
            {
                confess &log(ERROR, 'repo-path \'' . cfgOption(CFGOPT_REPO_PATH) . '\' does not exist', ERROR_PATH_MISSING);
            }

            # Load module dynamically
            require pgBackRest::Protocol::Remote::Minion;
            pgBackRest::Protocol::Remote::Minion->import();

            # Create the remote object
            my $oRemote = new pgBackRest::Protocol::Remote::Minion(
                cfgOption(CFGOPT_BUFFER_SIZE), cfgOption(CFGOPT_PROTOCOL_TIMEOUT));

            # Acquire a remote lock (except for commands that are read-only or local processes)
            my $strLockName;

            if (!(cfgOptionTest(CFGOPT_COMMAND, cfgCommandName(CFGCMD_ARCHIVE_GET)) ||
                  cfgOptionTest(CFGOPT_COMMAND, cfgCommandName(CFGCMD_INFO)) ||
                  cfgOptionTest(CFGOPT_COMMAND, cfgCommandName(CFGCMD_RESTORE)) ||
                  cfgOptionTest(CFGOPT_COMMAND, cfgCommandName(CFGCMD_CHECK)) ||
                  cfgOptionTest(CFGOPT_COMMAND, cfgCommandName(CFGCMD_LOCAL))))
            {
                $strLockName = cfgOption(CFGOPT_COMMAND);
            }

            # Process remote requests
            exitSafe($oRemote->process($strLockName));
        }

        ############################################################################################################################
        # Process local commands
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_LOCAL))
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
            exitSafe($oLocal->process());
        }

        ############################################################################################################################
        # Process check command
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_CHECK))
        {
            # Load module dynamically
            require pgBackRest::Check::Check;
            pgBackRest::Check::Check->import();

            exitSafe(new pgBackRest::Check::Check()->process());
        }

        ############################################################################################################################
        # Process start/stop commands
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_START))
        {
            lockStart();
            exitSafe(0);
        }
        elsif (cfgCommandTest(CFGCMD_STOP))
        {
            lockStop();
            exitSafe(0);
        }

        # Check that the repo path exists
        require pgBackRest::Protocol::Storage::Helper;
        pgBackRest::Protocol::Storage::Helper->import();

        if (isRepoLocal() && !cfgOptionTest(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3) && !storageRepo()->pathExists(''))
        {
            confess &log(ERROR, 'repo-path \'' . cfgOption(CFGOPT_REPO_PATH) . '\' does not exist', ERROR_PATH_MISSING);
        }

        ############################################################################################################################
        # Process info command
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_INFO))
        {
            # Load module dynamically
            require pgBackRest::Info;
            pgBackRest::Info->import();

            exitSafe(new pgBackRest::Info()->process());
        }

        ############################################################################################################################
        # Acquire the command lock
        ############################################################################################################################
        lockAcquire(cfgCommandName(cfgCommandGet()));

        ############################################################################################################################
        # Open the log file
        ############################################################################################################################
        require pgBackRest::Storage::Helper;
        pgBackRest::Storage::Helper->import();

        logFileSet(
            storageLocal(),
            cfgOption(CFGOPT_LOG_PATH) . '/' . cfgOption(CFGOPT_STANZA) . '-' . lc(cfgCommandName(cfgCommandGet())));

        ############################################################################################################################
        # Process stanza-create command
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_STANZA_CREATE) || cfgCommandTest(CFGCMD_STANZA_UPGRADE))
        {
            if (!isRepoLocal())
            {
                confess &log(ERROR,
                    cfgCommandName(cfgCommandGet()) . ' command must be run on the backup host', ERROR_HOST_INVALID);
            }

            # Load module dynamically
            require pgBackRest::Stanza;
            pgBackRest::Stanza->import();

            exitSafe(new pgBackRest::Stanza()->process());
        }

        ############################################################################################################################
        # RESTORE
        ############################################################################################################################
        if (cfgCommandTest(CFGCMD_RESTORE))
        {
            if (!isDbLocal())
            {
                confess &log(ERROR, 'restore command must be run on the db host', ERROR_HOST_INVALID);
            }

            # Load module dynamically
            require pgBackRest::Restore;
            pgBackRest::Restore->import();

            # Do the restore
            new pgBackRest::Restore()->process();

            exitSafe(0);
        }
        else
        {
            ########################################################################################################################
            # Make sure backup and expire commands happen on the backup side
            ########################################################################################################################
            if (!isRepoLocal())
            {
                confess &log(ERROR, 'backup and expire commands must be run on the backup host', ERROR_HOST_INVALID);
            }

            ########################################################################################################################
            # BACKUP
            ########################################################################################################################
            if (cfgCommandTest(CFGCMD_BACKUP))
            {
                # Load module dynamically
                require pgBackRest::Backup::Backup;
                pgBackRest::Backup::Backup->import();

                new pgBackRest::Backup::Backup()->process();

                cfgCommandSet(CFGCMD_EXPIRE);
            }

            ########################################################################################################################
            # EXPIRE
            ########################################################################################################################
            if (cfgCommandTest(CFGCMD_EXPIRE))
            {
                # Load module dynamically
                require pgBackRest::Expire;
                pgBackRest::Expire->import();

                new pgBackRest::Expire()->process();
            }
        }

        lockRelease();
        exitSafe(0);

        # uncoverable statement - exit should happen above
        &log(ASSERT, 'execution reached invalid location in ' . __FILE__ . ', line ' . __LINE__);
        exit ERROR_ASSERT;                                              # uncoverable statement
    }

    ################################################################################################################################
    # Check for errors
    ################################################################################################################################
    or do
    {
        # Perl 5.10 seems to have a problem propogating errors up through a large call stack, so in the case that the error arrives
        # blank just use the last logged error instead.  Don't do this in all cases because newer Perls seem to work fine and there
        # are other errors that could be arriving in $EVAL_ERROR.
        exitSafe(undef, defined($EVAL_ERROR) && length($EVAL_ERROR) > 0 ? $EVAL_ERROR : logErrorLast());
    };

    # uncoverable statement - errors should be handled in the do block above
    &log(ASSERT, 'execution reached invalid location in ' . __FILE__ . ', line ' . __LINE__);
    exit ERROR_ASSERT;                                                  # uncoverable statement
}

1;
