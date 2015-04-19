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
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Config;
use BackRest::Remote qw(DB BACKUP NONE);
use BackRest::Db;
use BackRest::File;
use BackRest::Archive;
use BackRest::Backup;
use BackRest::Restore;
use BackRest::ThreadGroup;

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
                             none - no recovery.conf generated
    --target             recovery target if type is name, time, or xid.
    --target-exclusive   stop just before the recovery target (default is inclusive).
    --target-resume      do not pause after recovery (default is to pause).
    --target-timeline    recover into specified timeline (default is current timeline).

=cut

####################################################################################################################################
# SAFE_EXIT - terminate all SSH sessions when the script is terminated
####################################################################################################################################
sub safe_exit
{
    my $iExitCode = shift;

    &log(DEBUG, "safe exit called, terminating threads");

    my $iTotal = threadGroupDestroy();
    remoteDestroy();

    if (defined($iExitCode))
    {
        exit $iExitCode;
    }

    &log(ERROR, "process terminated on signal or exception, ${iTotal} threads stopped");
}

$SIG{TERM} = \&safe_exit;
$SIG{HUP} = \&safe_exit;
$SIG{INT} = \&safe_exit;

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
# Process archive commands
####################################################################################################################################
if (operationTest(OP_ARCHIVE_PUSH) || operationTest(OP_ARCHIVE_GET))
{
    safe_exit(new BackRest::Archive()->process());
}

####################################################################################################################################
# Open the log file
####################################################################################################################################
log_file_set(optionGet(OPTION_REPO_PATH) . '/log/' . optionGet(OPTION_STANZA) . '-' . lc(operationGet()));

####################################################################################################################################
# Create the thread group that will be used for parallel processing
####################################################################################################################################
threadGroupCreate();

####################################################################################################################################
# Initialize the default file object
####################################################################################################################################
my $oFile = new BackRest::File
(
    optionGet(OPTION_STANZA),
    optionRemoteTypeTest(BACKUP) ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
    optionRemoteType(),
    optionRemote()
);

####################################################################################################################################
# RESTORE
####################################################################################################################################
if (operationTest(OP_RESTORE))
{
    if (optionRemoteTypeTest(DB))
    {
        confess &log(ASSERT, 'restore operation must be performed locally on the db server');
    }

    # Set the lock path
    my $strLockPath = optionGet(OPTION_REPO_PATH) .  '/lock/' .
                                      optionGet(OPTION_STANZA) . '-' . operationGet() . '.lock';

    # Do the restore
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

    safe_exit(0);
}

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Make sure backup and expire operations happen on the backup side
if (optionRemoteTypeTest(BACKUP))
{
    confess &log(ERROR, 'backup and expire operations must run on the backup host');
}

# Set the lock path
my $strLockPath = optionGet(OPTION_REPO_PATH) .  '/lock/' . optionGet(OPTION_STANZA) . '-' . operationGet() . '.lock';

if (!lock_file_create($strLockPath))
{
    &log(ERROR, 'backup process is already running for stanza ' . optionGet(OPTION_STANZA) . ' - exiting');
    safe_exit(0);
}

# Initialize the db object
my $oDb;

if (operationTest(OP_BACKUP))
{
    if (!optionGet(OPTION_NO_START_STOP))
    {
        $oDb = new BackRest::Db
        (
            optionGet(OPTION_DB_PATH),
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
safe_exit(0);
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
        safe_exit($oMessage->code());
    }

    safe_exit();
    confess $@;
}
