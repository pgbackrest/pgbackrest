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
use Getopt::Long;
use Pod::Usage;

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRest::File;
use BackRest::Backup;
use BackRest::Db;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

pg_backrest.pl - Simple Postgres Backup and Restore

=head1 SYNOPSIS

pg_backrest.pl [options] [operation]

 Operation:
   archive-get      retrieve an archive file from backup
   archive-push     push an archive file to backup
   backup           backup a cluster
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
                     Use with extreme caution as this will produce an inconsistent backup!
=cut

####################################################################################################################################
# Operation constants - basic operations that are allowed in backrest
####################################################################################################################################
use constant
{
    OP_ARCHIVE_GET  => 'archive-get',
    OP_ARCHIVE_PUSH => 'archive-push',
    OP_BACKUP       => 'backup',
    OP_EXPIRE       => 'expire'
};

####################################################################################################################################
# Configuration constants - configuration sections and keys
####################################################################################################################################
use constant
{
    CONFIG_SECTION_COMMAND             => 'command',
    CONFIG_SECTION_COMMAND_OPTION      => 'command:option',
    CONFIG_SECTION_LOG                 => 'log',
    CONFIG_SECTION_BACKUP              => 'backup',
    CONFIG_SECTION_ARCHIVE             => 'archive',
    CONFIG_SECTION_RETENTION           => 'retention',
    CONFIG_SECTION_STANZA              => 'stanza',

    CONFIG_KEY_USER                    => 'user',
    CONFIG_KEY_HOST                    => 'host',
    CONFIG_KEY_PATH                    => 'path',

    CONFIG_KEY_THREAD_MAX              => 'thread-max',
    CONFIG_KEY_THREAD_TIMEOUT          => 'thread-timeout',
    CONFIG_KEY_HARDLINK                => 'hardlink',
    CONFIG_KEY_ARCHIVE_REQUIRED        => 'archive-required',
    CONFIG_KEY_ARCHIVE_MAX_MB          => 'archive-max-mb',
    CONFIG_KEY_START_FAST              => 'start_fast',
    CONFIG_KEY_COMPRESS_ASYNC          => 'compress-async',

    CONFIG_KEY_LEVEL_FILE              => 'level-file',
    CONFIG_KEY_LEVEL_CONSOLE           => 'level-console',

    CONFIG_KEY_COMPRESS                => 'compress',
    CONFIG_KEY_CHECKSUM                => 'checksum',
    CONFIG_KEY_PSQL                    => 'psql',
    CONFIG_KEY_REMOTE                  => 'remote',

    CONFIG_KEY_FULL_RETENTION          => 'full-retention',
    CONFIG_KEY_DIFFERENTIAL_RETENTION  => 'differential-retention',
    CONFIG_KEY_ARCHIVE_RETENTION_TYPE  => 'archive-retention-type',
    CONFIG_KEY_ARCHIVE_RETENTION       => 'archive-retention'
};

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $strConfigFile;           # Configuration file
my $strStanza;               # Stanza in the configuration file to load
my $strType;                 # Type of backup: full, differential (diff), incremental (incr)
my $bNoStartStop = false;    # Do not perform start/stop backup (and archive-required gets set to false)
my $bForce = false;          # Force an action that would not normally be allowed (varies by action)
my $bVersion = false;        # Display version and exit
my $bHelp = false;           # Display help and exit

# Test parameters - not for general use
my $bNoFork = false;    # Prevents the archive process from forking when local archiving is enabled
my $bTest = false;      # Enters test mode - not harmful in anyway, but adds special logging and pauses for unit testing
my $iTestDelay = 5;     # Amount of time to delay after hitting a test point (the default would not be enough for manual tests)

GetOptions ('config=s'      => \$strConfigFile,
            'stanza=s'      => \$strStanza,
            'type=s'        => \$strType,
            'no-start-stop' => \$bNoStartStop,
            'force'         => \$bForce,
            'version'       => \$bVersion,
            'help'          => \$bHelp,

            # Test parameters - not for general use (and subject to change without notice)
            'no-fork'       => \$bNoFork,
            'test'          => \$bTest,
            'test-delay=s'  => \$iTestDelay)
    or pod2usage(2);

# Display version and exit if requested
if ($bVersion || $bHelp)
{
    print 'pg_backrest ' . version_get() . "\n";

    if (!$bHelp)
    {
        exit 0;
    }
}

# Display help and exit if requested
if ($bHelp)
{
    print "\n";
    pod2usage();
}

# Set test parameters
test_set($bTest, $iTestDelay);

####################################################################################################################################
# Global variables
####################################################################################################################################
my %oConfig;            # Configuration hash
my $oRemote;            # Remote object
my $strRemote;          # Defines which side is remote, DB or BACKUP

####################################################################################################################################
# CONFIG_LOAD - Get a value from the config and be sure that it is defined (unless bRequired is false)
####################################################################################################################################
sub config_key_load
{
    my $strSection = shift;
    my $strKey = shift;
    my $bRequired = shift;
    my $strDefault = shift;

    # Default is that the key is not required
    if (!defined($bRequired))
    {
        $bRequired = false;
    }

    my $strValue;

    # Look in the default stanza section
    if ($strSection eq CONFIG_SECTION_STANZA)
    {
        $strValue = $oConfig{"${strStanza}"}{"${strKey}"};
    }
    # Else look in the supplied section
    else
    {
        # First check the stanza section
        $strValue = $oConfig{"${strStanza}:${strSection}"}{"${strKey}"};

        # If the stanza section value is undefined then check global
        if (!defined($strValue))
        {
            $strValue = $oConfig{"global:${strSection}"}{"${strKey}"};
        }
    }

    if (!defined($strValue) && $bRequired)
    {
        if (defined($strDefault))
        {
            return $strDefault;
        }

        confess &log(ERROR, 'config value ' . (defined($strSection) ? $strSection : '[stanza]') .  "->${strKey} is undefined");
    }

    if ($strSection eq CONFIG_SECTION_COMMAND)
    {
        my $strOption = config_key_load(CONFIG_SECTION_COMMAND_OPTION, $strKey);

        if (defined($strOption))
        {
            $strValue =~ s/\%option\%/${strOption}/g;
        }
    }

    return $strValue;
}

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
# REMOTE_GET - Get the remote object or create it if not exists
####################################################################################################################################
sub remote_get()
{
    if (!defined($oRemote) && $strRemote ne REMOTE_NONE)
    {
        $oRemote = BackRest::Remote->new
        (
            strHost => config_key_load($strRemote eq REMOTE_DB ? CONFIG_SECTION_STANZA : CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST, true),
            strUser => config_key_load($strRemote eq REMOTE_DB ? CONFIG_SECTION_STANZA : CONFIG_SECTION_BACKUP, CONFIG_KEY_USER, true),
            strCommand => config_key_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_REMOTE, true)
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
# START EVAL BLOCK TO CATCH ERRORS AND STOP THREADS
####################################################################################################################################
eval {

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Get the operation
my $strOperation = $ARGV[0];

# Validate the operation
if (!defined($strOperation))
{
    confess &log(ERROR, 'operation is not defined');
}

if ($strOperation ne OP_ARCHIVE_GET &&
    $strOperation ne OP_ARCHIVE_PUSH &&
    $strOperation ne OP_BACKUP &&
    $strOperation ne OP_EXPIRE)
{
    confess &log(ERROR, "invalid operation ${strOperation}");
}

# Type should only be specified for backups
if (defined($strType) && $strOperation ne OP_BACKUP)
{
    confess &log(ERROR, 'type can only be specified for the backup operation')
}

####################################################################################################################################
# LOAD CONFIG FILE
####################################################################################################################################
if (!defined($strConfigFile))
{
    $strConfigFile = '/etc/pg_backrest.conf';
}

config_load($strConfigFile, \%oConfig);

# Load and check the cluster
if (!defined($strStanza))
{
    confess 'a backup stanza must be specified';
}

# Set the log levels
log_level_set(uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_FILE, true, INFO)),
              uc(config_key_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_CONSOLE, true, ERROR)));

####################################################################################################################################
# DETERMINE IF THERE IS A REMOTE
####################################################################################################################################
# First check if backup is remote
if (defined(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)))
{
    $strRemote = REMOTE_BACKUP;
}
# Else check if db is remote
elsif (defined(config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST)))
{
    # Don't allow both sides to be remote
    if (defined($strRemote))
    {
        confess &log(ERROR, 'db and backup cannot both be configured as remote');
    }

    $strRemote = REMOTE_DB;
}
else
{
    $strRemote = REMOTE_NONE;
}

####################################################################################################################################
# ARCHIVE-PUSH Command
####################################################################################################################################
if ($strOperation eq OP_ARCHIVE_PUSH)
{
    # Make sure the archive push operation happens on the db side
    if ($strRemote eq REMOTE_DB)
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
        $strStopFile = "${strArchivePath}/lock/${strStanza}-archive.stop";
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
        my $oFile = BackRest::File->new
        (
            strStanza => $strStanza,
            strRemote => $bArchiveLocal ? REMOTE_NONE : $strRemote,
            oRemote => $bArchiveLocal ? undef : remote_get(),
            strBackupPath => config_key_load($strSection, CONFIG_KEY_PATH, true)
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

        # Exit if we are archiving local but no backup host has been defined
        if (!($bArchiveLocal && defined(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST))))
        {
            remote_exit(0);
        }

        # Fork and exit the parent process so the async process can continue
        if (!$bNoFork)
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
    if (!defined(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)))
    {
        &log(ERROR, 'archive-push called without an archive file or backup host');
    }

    &log(INFO, 'starting async archive-push');

    # Create a lock file to make sure async archive-push does not run more than once
    my $strLockPath = "${strArchivePath}/lock/${strStanza}-archive.lock";

    if (!lock_file_create($strLockPath))
    {
        &log(DEBUG, 'archive-push process is already running - exiting');
        remote_exit(0);
    }

    # Build the basic command string that will be used to modify the command during processing
    my $strCommand = $^X . ' ' . $0 . " --stanza=${strStanza}";

    # Get the new operational flags
    my $bCompress = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, 'y') eq 'y' ? true : false;
    my $iArchiveMaxMB = config_key_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_ARCHIVE_MAX_MB);

    # eval
    # {
        # Create the file object
        my $oFile = BackRest::File->new
        (
            strStanza => $strStanza,
            strRemote => $strRemote,
            oRemote => remote_get(),
            strBackupPath => config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true)
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
            $iLogTotal = archive_xfer($strArchivePath . "/archive/${strStanza}", $strStopFile, $strCommand, $iArchiveMaxMB);

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
if ($strOperation eq OP_ARCHIVE_GET)
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
    my $oFile = BackRest::File->new
    (
        strStanza => $strStanza,
        strRemote => $strRemote,
        oRemote => remote_get(),
        strBackupPath => config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true)
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
    remote_exit(archive_get($ARGV[1], $ARGV[2]));
}

####################################################################################################################################
# OPEN THE LOG FILE
####################################################################################################################################
if (defined(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)))
{
    confess &log(ASSERT, 'backup/expire operations must be performed locally on the backup server');
}

log_file_set(config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true) . "/log/${strStanza}");

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Make sure backup and expire operations happen on the backup side
if ($strRemote eq REMOTE_BACKUP)
{
    confess &log(ERROR, 'backup and expire operations must run on the backup host');
}

# Set the backup type
if (!defined($strType))
{
    $strType = 'incremental';
}
elsif ($strType eq 'diff')
{
    $strType = 'differential';
}
elsif ($strType eq 'incr')
{
    $strType = 'incremental';
}
elsif ($strType ne 'full' && $strType ne 'differential' && $strType ne 'incremental')
{
    confess &log(ERROR, 'backup type must be full, differential (diff), incremental (incr)');
}

# Get the operational flags
my $bCompress = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, 'y') eq 'y' ? true : false;
my $bChecksum = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_CHECKSUM, true, 'y') eq 'y' ? true : false;

# Set the lock path
my $strLockPath = config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true) .  "/lock/${strStanza}-${strOperation}.lock";

if (!lock_file_create($strLockPath))
{
    &log(ERROR, "backup process is already running for stanza ${strStanza} - exiting");
    remote_exit(0);
}

# Initialize the default file object
my $oFile = BackRest::File->new
(
    strStanza => $strStanza,
    strRemote => $strRemote,
    oRemote => remote_get(),
    strBackupPath => config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true)
);

# Initialize the db object
my $oDb;

if (!$bNoStartStop)
{
    $oDb = BackRest::Db->new
    (
        strDbUser => config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_USER),
        strDbHost => config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST),
        strCommandPsql => config_key_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_PSQL),
        oDbSSH => $oFile->{oDbSSH}
    );
}

# Run backup_init - parameters required for backup and restore operations
backup_init
(
    $oDb,
    $oFile,
    $strType,
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, 'y') eq 'y' ? true : false,
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HARDLINK, true, 'y') eq 'y' ? true : false,
    !$bChecksum,
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX),
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_ARCHIVE_REQUIRED, true, 'y') eq 'y' ? true : false,
    config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_TIMEOUT),
    $bNoStartStop,
    $bForce
);

####################################################################################################################################
# BACKUP
####################################################################################################################################
if ($strOperation eq OP_BACKUP)
{
    backup(config_key_load(CONFIG_SECTION_STANZA, CONFIG_KEY_PATH),
           config_key_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_START_FAST, true, 'n') eq 'y' ? true : false);

    $strOperation = OP_EXPIRE;
}

####################################################################################################################################
# EXPIRE
####################################################################################################################################
if ($strOperation eq OP_EXPIRE)
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

remote_exit(0);
};

####################################################################################################################################
# CHECK FOR ERRORS AND STOP THREADS
####################################################################################################################################
if ($@)
{
    remote_exit();
    confess $@;
}
