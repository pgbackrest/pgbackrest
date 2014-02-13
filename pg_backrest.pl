#!/usr/bin/perl

use strict;
use warnings;

use File::Basename;
use Getopt::Long;
use Config::IniFiles;
use Carp;
use Fcntl qw(:DEFAULT :flock);

use lib dirname($0);
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_backup;
use pg_backrest_db;

# Operation constants
use constant
{
    OP_ARCHIVE_PUSH => "archive-push",
    OP_ARCHIVE_PULL => "archive-pull",
    OP_BACKUP       => "backup",
    OP_EXPIRE       => "expire",
};

use constant
{
    CONFIG_SECTION_COMMAND        => "command",
    CONFIG_SECTION_COMMAND_OPTION => "command:option",
    CONFIG_SECTION_BACKUP         => "backup",
    CONFIG_SECTION_ARCHIVE        => "archive",
    CONFIG_SECTION_RETENTION      => "retention",
    CONFIG_SECTION_STANZA         => "stanza",

    CONFIG_KEY_USER               => "user",
    CONFIG_KEY_HOST               => "host",
    CONFIG_KEY_PATH               => "path",

    CONFIG_KEY_COMPRESS           => "compress",
    CONFIG_KEY_DECOMPRESS         => "decompress",
    CONFIG_KEY_CHECKSUM           => "checksum",
    CONFIG_KEY_MANIFEST           => "manifest",
    CONFIG_KEY_PSQL               => "psql"
};

# Command line parameters
my $strConfigFile;      # Configuration file
my $strStanza;          # Stanza in the configuration file to load
my $strType;            # Type of backup: full, differential (diff), incremental (incr)

GetOptions ("config=s" => \$strConfigFile,
            "stanza=s" => \$strStanza,
            "type=s" => \$strType)
    or die("Error in command line arguments\n");
    
# Global variables
my %oConfig;

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
# START MAIN
####################################################################################################################################
# Get the operation
my $strOperation = $ARGV[0];

# Validate the operation
if (!defined($strOperation))
{
    confess &log(ERROR, "operation is not defined");
}

if ($strOperation ne OP_ARCHIVE_PUSH &&
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

# !!! Pick the log file name here (backup, restore, archive-YYYYMMDD)
my $strLogFile = "";

if ($strOperation eq OP_ARCHIVE_PUSH)
{

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

####################################################################################################################################
# ARCHIVE-PUSH Command
####################################################################################################################################
if ($strOperation eq OP_ARCHIVE_PUSH)
{
    # If an archive section has been defined, use that instead of the backup section when operation is OP_ARCHIVE_PUSH
    my $strSection = defined(config_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_PATH)) ? CONFIG_SECTION_ARCHIVE : CONFIG_SECTION_BACKUP;
    
    # Get the operational flags
    my $bCompress = config_load($strSection, CONFIG_KEY_COMPRESS, true, "y") eq "y" ? true : false;
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

    # Call the archive_push function
    if (!defined($ARGV[1]))
    {
        confess &log(ERROR, "source archive file not provided - show usage");
    }

    archive_push($ARGV[1]);

    # Is backup section defined?  If so fork, exit parent, and push archive logs to the backup server asynchronously afterwards
    if ($strSection eq CONFIG_SECTION_ARCHIVE && defined(config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_HOST)) && fork())
    {
        exit 0;
    }

    # Create a lock file to make sure archive-pull does not run more than once
    my $strArchivePath = config_load(CONFIG_SECTION_ARCHIVE, CONFIG_KEY_PATH);
    my $fLockFile;
    
    sysopen($fLockFile, "${strArchivePath}/${strStanza}/archive.lock", O_WRONLY | O_CREAT) or die;

    if (!flock($fLockFile, LOCK_EX | LOCK_NB))
    {
        &log(INFO, "archive-pull process is already running - exiting");
        exit 0
    }

    # Get the new operational flags
    $bCompress = config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_COMPRESS, true, "y") eq "y" ? true : false;
    $bChecksum = config_load(CONFIG_SECTION_BACKUP, CONFIG_KEY_CHECKSUM, true, "y") eq "y" ? true : false;

    # Run file_init_archive - this is the minimal config needed to run archive pulling !!! need to close the old file
    $oFile = pg_backrest_file->new
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
        !$bChecksum
    );

    # Call the archive_pull function
    archive_pull($strArchivePath);

    exit 0;
}

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
    strCommandPsql => config_load(CONFIG_SECTION_COMMAND, CONFIG_KEY_PSQL)
);

# Run backup_init - parameters required for backup and restore operations
backup_init
(
    $oDb,
    $oFile,
    $strType,
    config_load(CONFIG_SECTION_BACKUP, "hardlink", true, "n") eq "y" ? true : false,
    !$bChecksum,
    config_load(CONFIG_SECTION_BACKUP, "thread"),
    config_load(CONFIG_SECTION_BACKUP, "archive_required", true, "y") eq "y" ? true : false
);

####################################################################################################################################
# BACKUP
####################################################################################################################################
if ($strOperation eq OP_BACKUP)
{
    backup(config_load(CONFIG_SECTION_STANZA, CONFIG_KEY_PATH));

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
        config_load(CONFIG_SECTION_RETENTION, "full_retention"),
        config_load(CONFIG_SECTION_RETENTION, "differential_retention"),
        config_load(CONFIG_SECTION_RETENTION, "archive_retention_type"),
        config_load(CONFIG_SECTION_RETENTION, "archive_retention")
    );

    exit 0;
}

confess &log(ASSERT, "invalid operation ${strOperation} - missing handler block");