#!/usr/bin/perl

use strict;
use warnings;

use File::Basename;
use Getopt::Long;
use Config::IniFiles;
use Carp;

use lib dirname($0);
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_backup;
use pg_backrest_db;

# Command line parameters
my $strConfigFile;
my $strStanza;
my $strType = "incremental";        # Type of backup: full, differential (diff), incremental (incr)

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
    if ($strSection eq "stanza")
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

    if ($strSection eq "command")
    {
        my $strOption = config_load("command:option", $strKey);
        
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
# Error if no operation is specified
if (@ARGV < 1)
{
    confess "operation my be specified (backup, expire, archive_push, ...) - show usage";
}

# Get the command
my $strOperation = $ARGV[0];
my $strLogFile = "";

# !!! Pick the log file name here (backup, restore, archive-YYYYMMDD)
# 
if ($strOperation eq "archive-push")
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
if ($strOperation eq "archive-push")
{
    # archive-push command must have two arguments
    if (@ARGV != 2)
    {
        confess "not enough arguments - show usage";
    }

    # If an archive section has been defined, use that instead of the backup section
    my $strSection = defined(config_load("archive", "path")) ? "archive" : "backup";
    
    # Get the operational flags
    my $bCompress = config_load($strSection, "compress", true, "y") eq "y" ? true : false;
    my $bChecksum = config_load($strSection, "checksum", true, "y") eq "y" ? true : false;

    # Run file_init_archive - this is the minimal config needed to run archiving
    my $oFile = pg_backrest_file->new
    (
        strStanza => $strStanza,
        bNoCompression => !$bCompress,
        strBackupUser => config_load($strSection, "user"),
        strBackupHost => config_load($strSection, "host"),
        strBackupPath => config_load($strSection, "path", true),
        strCommandChecksum => config_load("command", "checksum", $bChecksum),
        strCommandCompress => config_load("command", "compress", $bCompress),
        strCommandDecompress => config_load("command", "decompress", $bCompress)
    );

    backup_init
    (
        undef,
        $oFile,
        undef,
        undef,
        !$bChecksum
    );

    # Call the archive function
    archive_push($ARGV[1]);

    exit 0;
}

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Check the backup type
if ($strType eq "diff")
{
    $strType = "differential";
}

if ($strType eq "incr")
{
    $strType = "incremental";
}

if ($strType ne "full" && $strType ne "differential" && $strType ne "incremental")
{
    confess &log(ERROR, "backup type must be full, differential (diff), incremental (incr)");
}

# Get the operational flags
my $bCompress = config_load("backup", "compress", true, "y") eq "y" ? true : false;
my $bChecksum = config_load("backup", "checksum", true, "y") eq "y" ? true : false;

# Run file_init_archive - the rest of the file config required for backup and restore
my $oFile = pg_backrest_file->new
(
    strStanza => $strStanza,
    bNoCompression => !$bCompress,
    strBackupUser => config_load("backup", "user"),
    strBackupHost => config_load("backup", "host"),
    strBackupPath => config_load("backup", "path", true),
    strDbUser => config_load("stanza", "user"),
    strDbHost => config_load("stanza", "host"),
    strCommandChecksum => config_load("command", "checksum", $bChecksum),
    strCommandCompress => config_load("command", "compress", $bCompress),
    strCommandDecompress => config_load("command", "decompress", $bCompress),
    strCommandManifest => config_load("command", "manifest"),
    strCommandPsql => config_load("command", "psql")
);

my $oDb = pg_backrest_db->new
(
    strDbUser => config_load("stanza", "user"),
    strDbHost => config_load("stanza", "host"),
    strCommandPsql => config_load("command", "psql")
);

# Run backup_init - parameters required for backup and restore operations
backup_init
(
    $oDb,
    $oFile,
    $strType,
    config_load("backup", "hardlink", true, "n") eq "y" ? true : false,
    !$bChecksum,
    config_load("backup", "thread"),
    config_load("backup", "archive_required", true, "y") eq "y" ? true : false
);

####################################################################################################################################
# BACKUP
####################################################################################################################################
if ($strOperation eq "backup")
{
    backup(config_load("stanza", "path"));

    $strOperation = "expire";
}

####################################################################################################################################
# EXPIRE
####################################################################################################################################
if ($strOperation eq "expire")
{
    backup_expire
    (
        $oFile->path_get(PATH_BACKUP_CLUSTER),
        config_load("retention", "full_retention"),
        config_load("retention", "differential_retention"),
        config_load("retention", "archive_retention_type"),
        config_load("retention", "archive_retention")
    );

    exit 0;
}