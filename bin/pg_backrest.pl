#!/usr/bin/perl
####################################################################################################################################
# pg_backrest.pl - Simple Postgres Backup and Restore
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use threads;
use strict;
use warnings;
use Carp;

use File::Basename;
use Pod::Usage;

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRest::Config;
use BackRest::Remote;
use BackRest::File;
use BackRest::Backup;
use BackRest::Restore;
use BackRest::Db;

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
    --delta          perform a delta restore using checksums when present.
    --force          force a restore and overwrite all existing files.
                     with --delta forces size/timestamp delta even if checksums are present.

 Recovery Options:
    --type               type of recovery:
                             name - restore point target
                             time - timestamp target
                             xid - transaction id target
                             preserve - preserve the existing recovery.conf
                             none - no recovery past database becoming consistent
                             default - recover to end of archive log stream
    --target             recovery target if type is name, time, or xid.
    --target-exclusive   stop just before the recovery target (default is inclusive).
    --target-resume      do not pause after recovery (default is to pause).
    --target-timeline    recover into specified timeline (default is current timeline).

=cut

####################################################################################################################################
# Global variables
####################################################################################################################################
my $oRemote;            # Remote object
my $strRemote;          # Defines which side is remote, DB or BACKUP

####################################################################################################################################
# REMOTE_GET - Get the remote object or create it if not exists
####################################################################################################################################
sub remote_get
{
    if (!defined($oRemote) && $strRemote ne NONE)
    {
        $oRemote = new BackRest::Remote
        (
            config_key_load($strRemote eq DB ? CONFIG_SECTION_STANZA : CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST, true),
            config_key_load($strRemote eq DB ? CONFIG_SECTION_STANZA : CONFIG_SECTION_BACKUP, CONFIG_KEY_USER, true),
            config_key_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_REMOTE, true)
        );
    }

    return $oRemote;
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
config_load();

####################################################################################################################################
# DETERMINE IF THERE IS A REMOTE
####################################################################################################################################
# First check if backup is remote
if (defined(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)))
{
    $strRemote = BACKUP;
}
# Else check if db is remote
elsif (defined(config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST)))
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
if (operation_get() eq OP_ARCHIVE_PUSH)
{
    # Make sure the archive push operation happens on the db side
    if ($strRemote eq DB)
    {
        confess &log(ERROR, 'archive-push operation must run on the db host');
    }

    # If an archive section has been defined, use that instead of the backup section when operation is OP_ARCHIVE_PUSH
    my $bArchiveLocal = defined(config_key_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_PATH));
    my $strSection =  $bArchiveLocal ? CONFIG_SECTION_ARCHIVE : CONFIG_SECTION_BACKUP;
    my $strArchivePath = config_key_load($strSection, CONFIG_KEY_PATH);

    # Get checksum flag
    my $bChecksum = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_CHECKSUM, true, 'y') eq 'y' ? true : false;

    # Get the async compress flag.  If compress_async=y then compression is off for the initial push when archiving locally
    my $bCompressAsync = false;

    if ($bArchiveLocal)
    {
        config_key_load($strSection, CONFIG_KEY_COMPRESS_ASYNC, true, 'n') eq 'n' ? false : true;
    }

    # If logging locally then create the stop archiving file name
    my $strStopFile;

    if ($bArchiveLocal)
    {
        $strStopFile = "${strArchivePath}/lock/" . param_get(PARAM_STANZA) . "-archive.stop";
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
        my $bCompress = $bCompressAsync ? false : config_key_load($strSection, CONFIG_KEY_COMPRESS, true, 'y') eq 'y' ? true : false;

        # Create the file object
        my $oFile = new BackRest::File
        (
            param_get(PARAM_STANZA),
            config_key_load($strSection, CONFIG_KEY_PATH, true),
            $bArchiveLocal ? NONE : $strRemote,
            $bArchiveLocal ? undef : remote_get()
        );

        # Init backup
        backup_init
        (
            undef,
            $oFile,
            undef,
            $bCompress,
            undef,
            !$bChecksum
        );

        &log(INFO, 'pushing archive log ' . $ARGV[1] . ($bArchiveLocal ? ' asynchronously' : ''));

        archive_push(config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_PATH), $ARGV[1]);

        # Exit if we are archiving async
        if (!$bArchiveLocal)
        {
            remote_exit(0);
        }

        # Fork and exit the parent process so the async process can continue
        if (!param_get(PARAM_TEST_NO_FORK))
        {
            if (fork())
            {
                remote_exit(0);
            }
        }
        # Else the no-fork flag has been specified for testing
        else
        {
            &log(INFO, 'No fork on archive local for TESTING');
        }
    }

    # If no backup host is defined it makes no sense to run archive-push without a specified archive file so throw an error
    # if (!defined(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)))
    # {
    #     &log(ERROR, 'archive-push called without an archive file or backup host');
    # }

    &log(INFO, 'starting async archive-push');

    # Create a lock file to make sure async archive-push does not run more than once
    my $strLockPath = "${strArchivePath}/lock/" . param_get(PARAM_STANZA) . "-archive.lock";

    if (!lock_file_create($strLockPath))
    {
        &log(DEBUG, 'archive-push process is already running - exiting');
        remote_exit(0);
    }

    # Build the basic command string that will be used to modify the command during processing
    my $strCommand = $^X . ' ' . $0 . " --stanza=" . param_get(PARAM_STANZA);

    # Get the new operational flags
    my $bCompress = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, 'y') eq 'y' ? true : false;
    my $iArchiveMaxMB = config_key_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_ARCHIVE_MAX_MB);

    # eval
    # {
        # Create the file object
        my $oFile = new BackRest::File
        (
            param_get(PARAM_STANZA),
            config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
            $strRemote,
            remote_get()
        );

        # Init backup
        backup_init
        (
            undef,
            $oFile,
            undef,
            $bCompress,
            undef,
            !$bChecksum,
            config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX),
            undef,
            config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_TIMEOUT)
        );

        # Call the archive_xfer function and continue to loop as long as there are files to process
        my $iLogTotal;

        while (!defined($iLogTotal) || $iLogTotal > 0)
        {
            $iLogTotal = archive_xfer($strArchivePath . "/archive/" . param_get(PARAM_STANZA), $strStopFile,
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
    #
    # };

    # # If there were errors above then start compressing
    # if ($@)
    # {
    #     if ($bCompressAsync)
    #     {
    #         &log(ERROR, "error during transfer: $@");
    #         &log(WARN, "errors during transfer, starting compression");
    #
    #         # Run file_init_archive - this is the minimal config needed to run archive pulling !!! need to close the old file
    #         my $oFile = BackRest::File->new
    #         (
    #             # strStanza => $strStanza,
    #             # bNoCompression => false,
    #             # strBackupPath => config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
    #             # strCommand => $0,
    #             # strCommandCompress => config_key_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_COMPRESS, $bCompress),
    #             # strCommandDecompress => config_key_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_DECOMPRESS, $bCompress)
    #         );
    #
    #         backup_init
    #         (
    #             undef,
    #             $oFile,
    #             undef,
    #             $bCompress,
    #             undef,
    #             !$bChecksum,
    #             config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX),
    #             undef,
    #             config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_TIMEOUT)
    #         );
    #
    #         archive_compress($strArchivePath . "/archive/${strStanza}", $strCommand, 256);
    #     }
    #     else
    #     {
    #         confess $@;
    #     }
    # }

    lock_file_remove();
    remote_exit(0);
}

####################################################################################################################################
# ARCHIVE-GET Command
####################################################################################################################################
if (operation_get() eq OP_ARCHIVE_GET)
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
        param_get(PARAM_STANZA),
        config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
        $strRemote,
        remote_get()
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
    remote_exit(archive_get(config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_PATH), $ARGV[1], $ARGV[2]));
}

####################################################################################################################################
# Initialize the default file object
####################################################################################################################################
my $oFile = new BackRest::File
(
    param_get(PARAM_STANZA),
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
    $strRemote,
    remote_get()
);

####################################################################################################################################
# RESTORE
####################################################################################################################################
if (operation_get() eq OP_RESTORE)
{
    if ($strRemote eq DB)
    {
        confess &log(ASSERT, 'restore operation must be performed locally on the db server');
    }

    # Open the log file
    log_file_set(config_key_load(CONFIG_SECTION_RESTORE, CONFIG_KEY_PATH, true) . '/log/' . param_get(PARAM_STANZA) . '-restore');

    # Set the lock path
    my $strLockPath = config_key_load(CONFIG_SECTION_RESTORE, CONFIG_KEY_PATH, true) .  '/lock/' .
                                      param_get(PARAM_STANZA) . '-' . operation_get() . '.lock';

    # Do the restore
    new BackRest::Restore
    (
        config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_PATH, true),
        param_get(PARAM_SET),
        config_section_load(CONFIG_SECTION_TABLESPACE_MAP),
        $oFile,
        config_key_load(CONFIG_SECTION_RESTORE, CONFIG_KEY_THREAD_MAX, true),
        param_get(PARAM_DELTA),
        param_get(PARAM_FORCE),
        param_get(PARAM_TYPE),
        param_get(PARAM_TARGET),
        param_get(PARAM_TARGET_EXCLUSIVE),
        param_get(PARAM_TARGET_RESUME),
        param_get(PARAM_TARGET_TIMELINE),
        config_section_load(CONFIG_SECTION_RECOVERY_OPTION),
        param_get(PARAM_STANZA),
        $0,
        param_get(PARAM_CONFIG)
    )->restore;

    remote_exit(0);
}

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Open the log file
log_file_set(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true) . '/log/' . param_get(PARAM_STANZA));

# Make sure backup and expire operations happen on the backup side
if ($strRemote eq BACKUP)
{
    confess &log(ERROR, 'backup and expire operations must run on the backup host');
}

# Get the operational flags
my $bCompress = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, 'y') eq 'y' ? true : false;
my $bChecksum = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_CHECKSUM, true, 'y') eq 'y' ? true : false;

# Set the lock path
my $strLockPath = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true) .  '/lock/' .
                                  param_get(PARAM_STANZA) . '-' . operation_get() . '.lock';

if (!lock_file_create($strLockPath))
{
    &log(ERROR, 'backup process is already running for stanza ' . param_get(PARAM_STANZA) . ' - exiting');
    remote_exit(0);
}

# Initialize the db object
my $oDb;

if (!param_get(PARAM_NO_START_STOP))
{
    $oDb = new BackRest::Db
    (
        config_key_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_PSQL),
        config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST),
        config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_USER)
    );
}

# Run backup_init - parameters required for backup and restore operations
backup_init
(
    $oDb,
    $oFile,
    param_get(PARAM_TYPE),
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, 'y') eq 'y' ? true : false,
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HARDLINK, true, 'y') eq 'y' ? true : false,
    !$bChecksum,
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX),
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_ARCHIVE_REQUIRED, true, 'y') eq 'y' ? true : false,
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_TIMEOUT),
    param_get(PARAM_NO_START_STOP),
    param_get(PARAM_FORCE)
);

####################################################################################################################################
# BACKUP
####################################################################################################################################
if (operation_get() eq OP_BACKUP)
{
    backup(config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_PATH),
           config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_START_FAST, true, 'n') eq 'y' ? true : false);

    operation_set(OP_EXPIRE);
}

####################################################################################################################################
# EXPIRE
####################################################################################################################################
if (operation_get() eq OP_EXPIRE)
{
    backup_expire
    (
        $oFile->path_get(PATH_BACKUP_CLUSTER),
        config_key_load(CONFIG_SECTION_RETENTION, CONFIG_KEY_FULL_RETENTION),
        config_key_load(CONFIG_SECTION_RETENTION, CONFIG_KEY_DIFFERENTIAL_RETENTION),
        config_key_load(CONFIG_SECTION_RETENTION, CONFIG_KEY_ARCHIVE_RETENTION_TYPE),
        config_key_load(CONFIG_SECTION_RETENTION, CONFIG_KEY_ARCHIVE_RETENTION)
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
