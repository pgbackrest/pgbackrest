#!/usr/bin/perl
####################################################################################################################################
# pg_backrest.pl - Simple Postgres Backup and Restore
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename;

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRest::Config;
use BackRest::Remote;
use BackRest::File;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

pg_backrest.pl - Simple Postgres Backup and Restore

=head1 SYNOPSIS

pg_backrest.pl [options] [operation]

 Operations:
   archive-get      retrieve an archive file from backup
   archive-push     push an archive file to backup
   backup           backup a cluster
   restore          restore a cluster
   expire           expire old backups (automatically run after backup)

 General Options:
   --stanza         stanza (cluster) to operate on (currently required for all operations)
   --config         alternate path for pg_backrest.conf (defaults to /etc/pg_backrest.conf)
   --version        display version and exit
   --help           display usage and exit

 Backup Options:
    --type           type of backup to perform (full, diff, incr)
    --no-start-stop  do not call pg_start/stop_backup().  Postmaster should not be running.
    --force          force backup when --no-start-stop passed and postmaster.pid exists.
                     Use with extreme caution as this will probably produce an inconsistent backup!

 Restore Options:
    --set            backup set to restore (defaults to latest set).
    --delta          perform a delta restore.
    --force          force a restore and overwrite all existing files.
                     with --delta forces size/timestamp deltas.

 Recovery Options:
    --type               type of recovery:
                             default - recover to end of archive log stream
                             name - restore point target
                             time - timestamp target
                             xid - transaction id target
                             preserve - preserve the existing recovery.conf
                             none - no recovery past database becoming consistent
    --target             recovery target if type is name, time, or xid.
    --target-exclusive   stop just before the recovery target (default is inclusive).
    --target-resume      do not pause after recovery (default is to pause).
    --target-timeline    recover into specified timeline (default is current timeline).

=cut

####################################################################################################################################
# Global variables
####################################################################################################################################
my $oRemote;            # Remote protocol object
my $oLocal;             # Local protocol object
my $strRemote;          # Defines which side is remote, DB or BACKUP

####################################################################################################################################
# REMOTE_GET - Get the remote object or create it if not exists
####################################################################################################################################
sub remote_get
{
    my $bForceLocal = shift;
    my $iCompressLevel = shift;
    my $iCompressLevelNetwork = shift;

    # Return the remote if is already defined
    if (defined($oRemote))
    {
        return $oRemote;
    }

    # Return the remote when required
    if ($strRemote ne NONE && !$bForceLocal)
    {
        $oRemote = new BackRest::Remote
        (
            $strRemote eq DB ? optionGet(OPTION_DB_HOST) : optionGet(OPTION_BACKUP_HOST),
            $strRemote eq DB ? optionGet(OPTION_DB_USER) : optionGet(OPTION_BACKUP_USER),
            optionGet(OPTION_COMMAND_REMOTE),
            optionGet(OPTION_BUFFER_SIZE),
            $iCompressLevel, $iCompressLevelNetwork
        );

        return $oRemote;
    }

    # Otherwise return local
    if (!defined($oLocal))
    {
        $oLocal = new BackRest::Remote
        (
            undef, undef, undef,
            optionGet(OPTION_BUFFER_SIZE),
            $iCompressLevel, $iCompressLevelNetwork
        );
    }

    return $oLocal;
}

####################################################################################################################################
# SAFE_EXIT - terminate all SSH sessions when the script is terminated
####################################################################################################################################
sub safe_exit
{
    remote_exit();

    my $iTotal = backup_thread_kill();

    confess &log(ERROR, "process was terminated on signal, ${iTotal} threads stopped");
}

$SIG{TERM} = \&safe_exit;
$SIG{HUP} = \&safe_exit;
$SIG{INT} = \&safe_exit;

####################################################################################################################################
# REMOTE_EXIT - Close the remote object if it exists
####################################################################################################################################
sub remote_exit
{
    my $iExitCode = shift;

    if (defined($oRemote))
    {
        $oRemote->thread_kill()
    }

    if (defined($iExitCode))
    {
        exit $iExitCode;
    }
}

####################################################################################################################################
# START EVAL BLOCK TO CATCH ERRORS AND STOP THREADS
####################################################################################################################################
eval {

####################################################################################################################################
# Load command line parameters and config
####################################################################################################################################
configLoad();

# Set the log levels
log_level_set(optionGet(OPTION_LOG_LEVEL_FILE), optionGet(OPTION_LOG_LEVEL_CONSOLE));

# Set test options
!optionGet(OPTION_TEST) or test_set(optionGet(OPTION_TEST), optionGet(OPTION_TEST_DELAY));

####################################################################################################################################
# DETERMINE IF THERE IS A REMOTE
####################################################################################################################################
# First check if backup is remote
if (optionTest(OPTION_BACKUP_HOST))
{
    $strRemote = BACKUP;
}
# Else check if db is remote
elsif (optionTest(OPTION_DB_HOST))
{
    # Don't allow both sides to be remote
    if (defined($strRemote))
    {
        confess &log(ERROR, 'db and backup cannot both be configured as remote');
    }

    $strRemote = DB;
}
else
{
    $strRemote = NONE;
}

####################################################################################################################################
# ARCHIVE-PUSH Command
####################################################################################################################################
if (operationTest(OP_ARCHIVE_PUSH))
{
    # Make sure the archive push operation happens on the db side
    if ($strRemote eq DB)
    {
        confess &log(ERROR, 'archive-push operation must run on the db host');
    }

    # If an archive section has been defined, use that instead of the backup section when operation is OP_ARCHIVE_PUSH
    my $bArchiveAsync = optionTest(OPTION_ARCHIVE_ASYNC);
    my $strArchivePath = optionGet(OPTION_REPO_PATH);

    # If logging locally then create the stop archiving file name
    my $strStopFile;

    if ($bArchiveAsync)
    {
        $strStopFile = "${strArchivePath}/lock/" . optionGet(OPTION_STANZA) . "-archive.stop";
    }

    # If an archive file is defined, then push it
    if (defined($ARGV[1]))
    {
        # If the stop file exists then discard the archive log
        if (defined($strStopFile))
        {
            if (-e $strStopFile)
            {
                &log(ERROR, "archive stop file (${strStopFile}) exists , discarding " . basename($ARGV[1]));
                remote_exit(0);
            }
        }

        # Get the compress flag
        my $bCompress = $bArchiveAsync ? false : optionGet(OPTION_COMPRESS);

        # Create the file object
        my $oFile = new BackRest::File
        (
            optionGet(OPTION_STANZA),
            $bArchiveAsync || $strRemote eq NONE ? optionGet(OPTION_REPO_PATH) : optionGet(OPTION_REPO_REMOTE_PATH),
            $bArchiveAsync ? NONE : $strRemote,
            remote_get($bArchiveAsync, optionGet(OPTION_COMPRESS_LEVEL),
                                       optionGet(OPTION_COMPRESS_LEVEL_NETWORK))
        );

        # Init backup
        backup_init
        (
            undef,
            $oFile,
            undef,
            $bCompress,
            undef
        );

        &log(INFO, 'pushing archive log ' . $ARGV[1] . ($bArchiveAsync ? ' asynchronously' : ''));

        archive_push(optionGet(OPTION_DB_PATH, false), $ARGV[1], $bArchiveAsync);

        # Exit if we are archiving async
        if (!$bArchiveAsync)
        {
            remote_exit(0);
        }

        # Fork and exit the parent process so the async process can continue
        if (!optionTest(OPTION_TEST_NO_FORK) && fork())
        {
            remote_exit(0);
        }
        # Else the no-fork flag has been specified for testing
        else
        {
            &log(INFO, 'No fork on archive local for TESTING');
        }

        # Start the async archive push
        &log(INFO, 'starting async archive-push');
    }

    # Create a lock file to make sure async archive-push does not run more than once
    my $strLockPath = "${strArchivePath}/lock/" . optionGet(OPTION_STANZA) . "-archive.lock";

    if (!lock_file_create($strLockPath))
    {
        &log(DEBUG, 'archive-push process is already running - exiting');
        remote_exit(0);
    }

    # Build the basic command string that will be used to modify the command during processing
    my $strCommand = $^X . ' ' . $0 . " --stanza=" . optionGet(OPTION_STANZA);

    # Get the new operational flags
    my $bCompress = optionGet(OPTION_COMPRESS);
    my $iArchiveMaxMB = optionGet(OPTION_ARCHIVE_MAX_MB, false);

    # Create the file object
    my $oFile = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        $strRemote eq NONE ? optionGet(OPTION_REPO_PATH) : optionGet(OPTION_REPO_REMOTE_PATH),
        $strRemote,
        remote_get(false, optionGet(OPTION_COMPRESS_LEVEL),
                          optionGet(OPTION_COMPRESS_LEVEL_NETWORK))
    );

    # Init backup
    backup_init
    (
        undef,
        $oFile,
        undef,
        $bCompress,
        undef,
        1, #optionGet(OPTION_THREAD_MAX),
        undef,
        optionGet(OPTION_THREAD_TIMEOUT, false)
    );

    # Call the archive_xfer function and continue to loop as long as there are files to process
    my $iLogTotal;

    while (!defined($iLogTotal) || $iLogTotal > 0)
    {
        $iLogTotal = archive_xfer($strArchivePath . "/archive/" . optionGet(OPTION_STANZA) . "/out", $strStopFile,
                                  $strCommand, $iArchiveMaxMB);

        if ($iLogTotal > 0)
        {
            &log(DEBUG, "${iLogTotal} archive logs were transferred, calling archive_xfer() again");
        }
        else
        {
            &log(DEBUG, 'no more logs to transfer - exiting');
        }
    }

    lock_file_remove();
    remote_exit(0);
}

####################################################################################################################################
# ARCHIVE-GET Command
####################################################################################################################################
if (operationTest(OP_ARCHIVE_GET))
{
    # Make sure the archive file is defined
    if (!defined($ARGV[1]))
    {
        confess &log(ERROR, 'archive file not provided');
    }

    # Make sure the destination file is defined
    if (!defined($ARGV[2]))
    {
        confess &log(ERROR, 'destination file not provided');
    }

    # Init the file object
    my $oFile = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        $strRemote eq BACKUP ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
        $strRemote,
        remote_get(false,
                   optionGet(OPTION_COMPRESS_LEVEL),
                   optionGet(OPTION_COMPRESS_LEVEL_NETWORK))
    );

    # Init the backup object
    backup_init
    (
        undef,
        $oFile
    );

    # Info for the Postgres log
    &log(INFO, 'getting archive log ' . $ARGV[1]);

    # Get the archive file
    remote_exit(archive_get(optionGet(OPTION_DB_PATH, false), $ARGV[1], $ARGV[2]));
}

####################################################################################################################################
# Initialize the default file object
####################################################################################################################################
my $oFile = new BackRest::File
(
    optionGet(OPTION_STANZA),
    $strRemote eq BACKUP ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
    $strRemote,
    remote_get(false,
               operationTest(OP_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL : optionGet(OPTION_COMPRESS_LEVEL),
               operationTest(OP_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK : optionGet(OPTION_COMPRESS_LEVEL_NETWORK))
);

####################################################################################################################################
# RESTORE
####################################################################################################################################
if (operationTest(OP_RESTORE))
{
    if ($strRemote eq DB)
    {
        confess &log(ASSERT, 'restore operation must be performed locally on the db server');
    }

    # Open the log file
    log_file_set(optionGet(OPTION_REPO_PATH) . '/log/' . optionGet(OPTION_STANZA) . '-restore');

    # Set the lock path
    my $strLockPath = optionGet(OPTION_REPO_PATH) .  '/lock/' .
                                      optionGet(OPTION_STANZA) . '-' . operationGet() . '.lock';

    # Do the restore
    use BackRest::Restore;
    new BackRest::Restore
    (
        optionGet(OPTION_DB_PATH),
        optionGet(OPTION_SET),
        optionGet(OPTION_RESTORE_TABLESPACE_MAP, false),
        $oFile,
        optionGet(OPTION_THREAD_MAX),
        optionGet(OPTION_DELTA),
        optionGet(OPTION_FORCE),
        optionGet(OPTION_TYPE),
        optionGet(OPTION_TARGET, false),
        optionGet(OPTION_TARGET_EXCLUSIVE, false),
        optionGet(OPTION_TARGET_RESUME, false),
        optionGet(OPTION_TARGET_TIMELINE, false),
        optionGet(OPTION_RESTORE_RECOVERY_SETTING, false),
        optionGet(OPTION_STANZA),
        $0,
        optionGet(OPTION_CONFIG)
    )->restore;

    remote_exit(0);
}

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Open the log file
log_file_set(optionGet(OPTION_REPO_PATH) . '/log/' . optionGet(OPTION_STANZA));

# Make sure backup and expire operations happen on the backup side
if ($strRemote eq BACKUP)
{
    confess &log(ERROR, 'backup and expire operations must run on the backup host');
}

# Set the lock path
my $strLockPath = optionGet(OPTION_REPO_PATH) .  '/lock/' . optionGet(OPTION_STANZA) . '-' . operationGet() . '.lock';

if (!lock_file_create($strLockPath))
{
    &log(ERROR, 'backup process is already running for stanza ' . optionGet(OPTION_STANZA) . ' - exiting');
    remote_exit(0);
}

# Initialize the db object
use BackRest::Db;
my $oDb;

if (operationTest(OP_BACKUP))
{
    if (!optionGet(OPTION_NO_START_STOP))
    {
        $oDb = new BackRest::Db
        (
            optionGet(OPTION_COMMAND_PSQL),
            optionGet(OPTION_DB_HOST, false),
            optionGet(OPTION_DB_USER, optionTest(OPTION_DB_HOST))
        );
    }

    # Run backup_init - parameters required for backup and restore operations
    backup_init
    (
        $oDb,
        $oFile,
        optionGet(OPTION_TYPE),
        optionGet(OPTION_COMPRESS),
        optionGet(OPTION_HARDLINK),
        optionGet(OPTION_THREAD_MAX),
        optionGet(OPTION_THREAD_TIMEOUT, false),
        optionGet(OPTION_NO_START_STOP),
        optionTest(OPTION_FORCE)
    );
}

####################################################################################################################################
# BACKUP
####################################################################################################################################
if (operationTest(OP_BACKUP))
{
    use BackRest::Backup;
    backup(optionGet(OPTION_DB_PATH), optionGet(OPTION_START_FAST));

    operationSet(OP_EXPIRE);
}

####################################################################################################################################
# EXPIRE
####################################################################################################################################
if (operationTest(OP_EXPIRE))
{
    if (!defined($oDb))
    {
        backup_init
        (
            undef,
            $oFile
        );
    }

    backup_expire
    (
        $oFile->path_get(PATH_BACKUP_CLUSTER),
        optionGet(OPTION_RETENTION_FULL, false),
        optionGet(OPTION_RETENTION_DIFF, false),
        optionGet(OPTION_RETENTION_ARCHIVE_TYPE, false),
        optionGet(OPTION_RETENTION_ARCHIVE, false)
    );

    lock_file_remove();
}

backup_cleanup();
remote_exit(0);
};

####################################################################################################################################
# CHECK FOR ERRORS AND STOP THREADS
####################################################################################################################################
if ($@)
{
    my $oMessage = $@;

    # If a backrest exception then return the code - don't confess
    if ($oMessage->isa('BackRest::Exception'))
    {
        remote_exit($oMessage->code());
    }

    remote_exit();
    confess $@;
}
