#!/usr/bin/perl
####################################################################################################################################
# pg_backrest.pl - Simple Postgres Backup and Restore
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use threads;

use File::Basename;
use Getopt::Long;
use Config::IniFiles;
use Carp;

use lib dirname($0);
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_backup;
use pg_backrest_db;

####################################################################################################################################
# Operation constants - basic operations that are allowed in backrest
####################################################################################################################################
use constant
{
    OP_ARCHIVE_GET  => "archive-get",
    OP_ARCHIVE_PUSH => "archive-push",
    OP_ARCHIVE_PULL => "archive-pull",
    OP_BACKUP       => "backup",
    OP_EXPIRE       => "expire"
};

####################################################################################################################################
# Configuration constants - configuration sections and keys
####################################################################################################################################
use constant
{
    CONFIG_SECTION_COMMAND        => "command",
    CONFIG_SECTION_COMMAND_OPTION => "command:option",
    CONFIG_SECTION_LOG            => "log",
    CONFIG_SECTION_BACKUP         => "backup",
    CONFIG_SECTION_ARCHIVE        => "archive",
    CONFIG_SECTION_RETENTION      => "retention",
    CONFIG_SECTION_STANZA         => "stanza",

    CONFIG_KEY_USER               => "user",
    CONFIG_KEY_HOST               => "host",
    CONFIG_KEY_PATH               => "path",

    CONFIG_KEY_THREAD_MAX         => "thread-max",
    CONFIG_KEY_THREAD_TIMEOUT     => "thread-timeout",
    CONFIG_KEY_HARDLINK           => "hardlink",
    CONFIG_KEY_ARCHIVE_REQUIRED   => "archive-required",
    CONFIG_KEY_ARCHIVE_MAX_MB     => "archive-max-mb",
    CONFIG_KEY_START_FAST         => "start_fast",

    CONFIG_KEY_LEVEL_FILE         => "level-file",
    CONFIG_KEY_LEVEL_CONSOLE      => "level-console",

    CONFIG_KEY_COMPRESS           => "compress",
    CONFIG_KEY_COMPRESS_ASYNC     => "compress-async",
    CONFIG_KEY_DECOMPRESS         => "decompress",
    CONFIG_KEY_CHECKSUM           => "checksum",
    CONFIG_KEY_MANIFEST           => "manifest",
    CONFIG_KEY_PSQL               => "psql"
};

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $strConfigFile;      # Configuration file
my $strStanza;          # Stanza in the configuration file to load
my $strType;            # Type of backup: full, differential (diff), incremental (incr)

GetOptions ("config=s" => \$strConfigFile,
            "stanza=s" => \$strStanza,
            "type=s" => \$strType)
    or die("Error in command line arguments\n");

####################################################################################################################################
# Global variables
####################################################################################################################################
my %oConfig;            # Configuration hash
    
####################################################################################################################################
# CONFIG_LOAD - Get a value from the config and be sure that it is defined (unless bRequired is false)
####################################################################################################################################
sub config_load
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
        
        confess &log(ERROR, "config value " . (defined($strSection) ? $strSection : "[stanza]") .  "->${strKey} is undefined");
    }

    if ($strSection eq CONFIG_SECTION_COMMAND)
    {
        my $strOption = config_load(CONFIG_SECTION_COMMAND_OPTION, $strKey);
        
        if (defined($strOption))
        {
            $strValue =~ s/\%option\%/${strOption}/g;
        } 
    }

    return $strValue;
}

####################################################################################################################################
# SAFE_EXIT - terminate all SSH sessions when the script is terminated
####################################################################################################################################
sub safe_exit
{
    my $iTotal = backup_thread_kill();

    confess &log(ERROR, "process was terminated on signal, ${iTotal} threads stopped");
}

$SIG{TERM} = \&safe_exit;
$SIG{HUP} = \&safe_exit;
$SIG{INT} = \&safe_exit;

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Get the operation
my $strOperation = $ARGV[0];

# Validate the operation
if (!defined($strOperation))
{
    confess &log(ERROR, "operation is not defined");
}

if ($strOperation ne OP_ARCHIVE_GET &&
    $strOperation ne OP_ARCHIVE_PUSH &&
    $strOperation ne OP_ARCHIVE_PULL &&
    $strOperation ne OP_BACKUP &&
    $strOperation ne OP_EXPIRE)
{
    confess &log(ERROR, "invalid operation ${strOperation}");
}

# Type should only be specified for backups
if (defined($strType) && $strOperation ne OP_BACKUP)
{
    confess &log(ERROR, "type can only be specified for the backup operation")
}

####################################################################################################################################
# LOAD CONFIG FILE
####################################################################################################################################
if (!defined($strConfigFile))
{
    $strConfigFile = "/etc/pg_backrest.conf";
}

tie %oConfig, 'Config::IniFiles', (-file => $strConfigFile) or confess &log(ERROR, "unable to find config file ${strConfigFile}");

# Load and check the cluster
if (!defined($strStanza))
{
    confess "a backup stanza must be specified - show usage";
}

# Set the log levels
log_level_set(uc(config_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_FILE, true, "INFO")),
              uc(config_load(CONFIG_SECTION_LOG, CONFIG_KEY_LEVEL_CONSOLE, true, "ERROR")));

####################################################################################################################################
# ARCHIVE-GET Command
####################################################################################################################################
if ($strOperation eq OP_ARCHIVE_GET)
{
    # Make sure the archive file is defined
    if (!defined($ARGV[1]))
    {
        confess &log(ERROR, "archive file not provided - show usage");
    }

    # Make sure the destination file is defined
    if (!defined($ARGV[2]))
    {
        confess &log(ERROR, "destination file not provided - show usage");
    }

    # Init the file object
    my $oFile = pg_backrest_file->new
    (
        strStanza => $strStanza,
        bNoCompression => true,
        strBackupUser => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_USER),
        strBackupHost => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST),
        strBackupPath => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
        strCommandDecompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_DECOMPRESS, true)
    );

    # Init the backup object
    backup_init
    (
        undef,
        $oFile
    );

    # Info for the Postgres log
    &log(INFO, "getting archive log " . $ARGV[1]);

    # Get the archive file
    exit archive_get($ARGV[1], $ARGV[2]);
}

####################################################################################################################################
# ARCHIVE-PUSH and ARCHIVE-PULL Commands
####################################################################################################################################
if ($strOperation eq OP_ARCHIVE_PUSH || $strOperation eq OP_ARCHIVE_PULL)
{
    # If an archive section has been defined, use that instead of the backup section when operation is OP_ARCHIVE_PUSH
    my $strSection = defined(config_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_PATH)) ? CONFIG_SECTION_ARCHIVE : CONFIG_SECTION_BACKUP;

    # Get the async compress flag.  If compress_async=y then compression is off for the initial push
    my $bCompressAsync = config_load($strSection, CONFIG_KEY_COMPRESS_ASYNC, true, "n") eq "n" ? false : true;
    
    # Get the async compress flag.  If compress_async=y then compression is off for the initial push
    my $strStopFile;
    my $strArchivePath;

    # If logging locally then create the stop archiving file name
    if ($strSection eq CONFIG_SECTION_ARCHIVE)
    {
        $strArchivePath = config_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_PATH);
        $strStopFile = "${strArchivePath}/lock/${strStanza}-archive.stop";
    }

    # Perform the archive-push
    if ($strOperation eq OP_ARCHIVE_PUSH)
    {
        # Call the archive_push function
        if (!defined($ARGV[1]))
        {
            confess &log(ERROR, "source archive file not provided - show usage");
        }

        # If the stop file exists then discard the archive log
        if (defined($strStopFile))
        {
            if (-e $strStopFile)
            {
                &log(ERROR, "archive stop file exists ($strStopFile), discarding " . basename($ARGV[1]));
                exit 0;
            }
        }
    
        # Make sure that archive-push is running locally
        if (defined(config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST)))
        {
            confess &log(ERROR, "stanza host cannot be set on archive-push - must be run locally on db server");
        }

        # Get the compress flag
        my $bCompress = $bCompressAsync ? false : config_load($strSection, CONFIG_KEY_COMPRESS, true, "y") eq "y" ? true : false;

        # Get the checksum flag
        my $bChecksum = config_load($strSection, CONFIG_KEY_CHECKSUM, true, "y") eq "y" ? true : false;

        # Run file_init_archive - this is the minimal config needed to run archiving
        my $oFile = pg_backrest_file->new
        (
            strStanza => $strStanza,
            bNoCompression => !$bCompress,
            strBackupUser => config_load($strSection, CONFIG_KEY_USER),
            strBackupHost => config_load($strSection, CONFIG_KEY_HOST),
            strBackupPath => config_load($strSection, CONFIG_KEY_PATH, true),
            strCommandChecksum => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_CHECKSUM, $bChecksum),
            strCommandCompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_COMPRESS, $bCompress),
            strCommandDecompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_DECOMPRESS, $bCompress)
        );

        backup_init
        (
            undef,
            $oFile,
            undef,
            undef,
            !$bChecksum
        );

        &log(INFO, "pushing archive log " . $ARGV[1] . ($bCompressAsync ? " asynchronously" : ""));

        archive_push($ARGV[1]);

        # Only continue if we are archiving local and a backup server is defined 
        if (!($strSection eq CONFIG_SECTION_ARCHIVE && defined(config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST))))
        {
            exit 0;
        }

        # Set the operation so that archive-pull will be called next
        $strOperation = OP_ARCHIVE_PULL;
        
        # fork and exit the parent process
        if (fork())
        {
            exit 0;
        }
    }

    # Perform the archive-pull
    if ($strOperation eq OP_ARCHIVE_PULL)
    {
        # Make sure that archive-pull is running on the db server
        if (defined(config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST)))
        {
            confess &log(ERROR, "stanza host cannot be set on archive-pull - must be run locally on db server");
        }
        
        # Create a lock file to make sure archive-pull does not run more than once
        my $strLockFile = "${strArchivePath}/lock/${strStanza}-archive.lock";

        if (!lock_file_create($strLockFile))
        {
            &log(DEBUG, "archive-pull process is already running - exiting");
            exit 0
        }

        # Build the basic command string that will be used to modify the command during processing
        my $strCommand = $^X . " " . $0 . " --stanza=${strStanza}";

        # Get the new operational flags
        my $bCompress = config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, "y") eq "y" ? true : false;
        my $bChecksum = config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_CHECKSUM, true, "y") eq "y" ? true : false;
        my $iArchiveMaxMB = config_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_ARCHIVE_MAX_MB);

        eval
        {
            # Run file_init_archive - this is the minimal config needed to run archive pulling
            my $oFile = pg_backrest_file->new
            (
                strStanza => $strStanza,
                bNoCompression => !$bCompress,
                strBackupUser => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_USER),
                strBackupHost => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST),
                strBackupPath => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
                strCommandChecksum => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_CHECKSUM, $bChecksum),
                strCommandCompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_COMPRESS, $bCompress),
                strCommandDecompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_DECOMPRESS, $bCompress),
                strCommandManifest => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_MANIFEST)
            );

            backup_init
            (
                undef,
                $oFile,
                undef,
                undef,
                !$bChecksum,
                config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX),
                undef,
                config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_TIMEOUT)
            );

            # Call the archive_pull function  Continue to loop as long as there are files to process.
            while (archive_pull($strArchivePath . "/archive/${strStanza}", $strStopFile, $strCommand, $iArchiveMaxMB))
            {
                &log(DEBUG, "archive logs were transferred, calling archive_pull() again");
            }
        };

        # If there were errors above then start compressing
        if ($@)
        {
            if ($bCompressAsync)
            {
                &log(ERROR, "error during transfer: $@");
                &log(WARN, "errors during transfer, starting compression");

                # Run file_init_archive - this is the minimal config needed to run archive pulling !!! need to close the old file
                my $oFile = pg_backrest_file->new
                (
                    strStanza => $strStanza,
                    bNoCompression => false,
                    strBackupPath => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
                    strCommandChecksum => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_CHECKSUM, $bChecksum),
                    strCommandCompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_COMPRESS, $bCompress),
                    strCommandDecompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_DECOMPRESS, $bCompress),
                    strCommandManifest => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_MANIFEST)
                );

                backup_init
                (
                    undef,
                    $oFile,
                    undef,
                    undef,
                    !$bChecksum,
                    config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX),
                    undef,
                    config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_TIMEOUT)
                );

                archive_compress($strArchivePath . "/archive/${strStanza}", $strCommand, 256);
            }
            else
            {
                confess $@;
            }
        }

        lock_file_remove();
    }

    exit 0;
}

####################################################################################################################################
# OPEN THE LOG FILE
####################################################################################################################################
if (defined(config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)))
{
    confess &log(ASSERT, "backup/expire operations must be performed locally on the backup server");
}

log_file_set(config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true) . "/log/${strStanza}");

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Set the backup type
if (!defined($strType))
{
    $strType = "incremental";
}
elsif ($strType eq "diff")
{
    $strType = "differential";
}
elsif ($strType eq "incr")
{
    $strType = "incremental";
}
elsif ($strType ne "full" && $strType ne "differential" && $strType ne "incremental")
{
    confess &log(ERROR, "backup type must be full, differential (diff), incremental (incr)");
}

# Get the operational flags
my $bCompress = config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, "y") eq "y" ? true : false;
my $bChecksum = config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_CHECKSUM, true, "y") eq "y" ? true : false;

# Run file_init_archive - the rest of the file config required for backup and restore
my $oFile = pg_backrest_file->new
(
    strStanza => $strStanza,
    bNoCompression => !$bCompress,
    strBackupUser => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_USER),
    strBackupHost => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST),
    strBackupPath => config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_PATH, true),
    strDbUser => config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_USER),
    strDbHost => config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST),
    strCommandChecksum => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_CHECKSUM, $bChecksum),
    strCommandCompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_COMPRESS, $bCompress),
    strCommandDecompress => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_DECOMPRESS, $bCompress),
    strCommandManifest => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_MANIFEST),
    strCommandPsql => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_PSQL)
);

my $oDb = pg_backrest_db->new
(
    strDbUser => config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_USER),
    strDbHost => config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_HOST),
    strCommandPsql => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_PSQL),
    oDbSSH => $oFile->{oDbSSH}
);

# Run backup_init - parameters required for backup and restore operations
backup_init
(
    $oDb,
    $oFile,
    $strType,
    config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HARDLINK, true, "n") eq "y" ? true : false,
    !$bChecksum,
    config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_MAX),
    config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_ARCHIVE_REQUIRED, true, "y") eq "y" ? true : false,
    config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_THREAD_TIMEOUT)
);

####################################################################################################################################
# BACKUP
####################################################################################################################################
if ($strOperation eq OP_BACKUP)
{
    my $strLockFile = $oFile->path_get(PATH_BACKUP, "lock/${strStanza}-backup.lock");

    if (!lock_file_create($strLockFile))
    {
        &log(ERROR, "backup process is already running for stanza ${strStanza} - exiting");
        exit 0
    }

    backup(config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_PATH),
           config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_START_FAST, true, "n") eq "y" ? true : false);

    $strOperation = OP_EXPIRE;

    lock_file_remove();
}

####################################################################################################################################
# EXPIRE
####################################################################################################################################
if ($strOperation eq OP_EXPIRE)
{
    my $strLockFile = $oFile->path_get(PATH_BACKUP, "lock/${strStanza}-expire.lock");

    if (!lock_file_create($strLockFile))
    {
        &log(ERROR, "expire process is already running for stanza ${strStanza} - exiting");
        exit 0
    }

    backup_expire
    (
        $oFile->path_get(PATH_BACKUP_CLUSTER),
        config_load(CONFIG_SECTION_RETENTION, "full_retention"),
        config_load(CONFIG_SECTION_RETENTION, "differential_retention"),
        config_load(CONFIG_SECTION_RETENTION, "archive_retention_type"),
        config_load(CONFIG_SECTION_RETENTION, "archive_retention")
    );

    lock_file_remove();

    exit 0;
}

confess &log(ASSERT, "invalid operation ${strOperation} - missing handler block");