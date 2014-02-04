#!/usr/bin/perl

use strict;
use warnings;

use File::Basename;
use File::Path;
use Getopt::Long;
use Config::IniFiles;
use IPC::System::Simple qw(capture);
use File::Copy;
use File::Remove;
use Carp;
use Cwd qw(abs_path);

use lib dirname($0);
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_backup;

# Command line parameters
my $strConfigFile;
my $strCluster;
my $strType = "incremental";        # Type of backup: full, differential (diff), incremental (incr)
my $bHardLink;
my $bNoChecksum;
my $bNoCompression;

GetOptions ("no-compression" => \$bNoCompression,
            "no-checksum" => \$bNoChecksum,
            "hardlink" => \$bHardLink,
            "config=s" => \$strConfigFile,
            "cluster=s" => \$strCluster,
            "type=s" => \$strType)
    or die("Error in command line arguments\n");

####################################################################################################################################
# CONFIG_LOAD - Get a value from the config and be sure that it is defined (unless bRequired is false)
####################################################################################################################################
sub config_load
{
    my $oConfigRef = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $bRequired = shift;
    
    if (!defined($bRequired))
    {
        $bRequired = 1;
    }
    
    my $strValue = ${$oConfigRef}{"${strSection}"}{"${strKey}"};
    
    if ($bRequired && !defined($strValue))
    {
        confess &log(ERROR, 'config value ${strSection}->${strKey} is undefined');
    }
    
    return $strValue;
}

####################################################################################################################################
# START MAIN
####################################################################################################################################
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

my %oConfig;
tie %oConfig, 'Config::IniFiles', (-file => $strConfigFile) or confess &log(ERROR, "unable to find config file ${strConfigFile}");

# Load and check the cluster
if (!defined($strCluster))
{
    $strCluster = "db"; #!!! Modify to load cluster from conf if there is only one, else error
}

# Run file_init_archive - this is the minimal config needed to run archiving
file_init_archive
(
    $bNoCompression,
    config_load(\%oConfig, "command", "checksum", !$bNoChecksum),
    config_load(\%oConfig, "command", "compress", !$bNoCompression),
    config_load(\%oConfig, "command", "decompress", !$bNoCompression),
    $oConfig{backup}{host},
    $oConfig{backup}{path},
    $strCluster,
);

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

# Run file_init_archive - the rest of the file config 
file_init_backup
(
    config_load(\%oConfig, "command", "manifest"),
    $pg_backrest_file::strCommandPsql = config_load(\%oConfig, "command", "psql"),
    $oConfig{"cluster:$strCluster"}{host}
);

# Run backup_init - parameters required for backup and restore operations
backup_init
(
    $strType,
    $bHardLink,
    $bNoChecksum
);

####################################################################################################################################
# BACKUP
####################################################################################################################################
if ($strOperation eq "backup")
{
    backup($oConfig{"cluster:$strCluster"}{path});
    exit 0;
}
