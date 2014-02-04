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

# Global variables
my $strConfigFile;
my $strCluster;
my $strType = "incremental";        # Type of backup: full, differential (diff), incremental (incr)
my $bHardLink;
my $bNoChecksum;

GetOptions ("no-compression" => \$pg_backrest_file::bNoCompression,
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
    config_load(\%oConfig, "command", "checksum", !$bNoChecksum),
    config_load(\%oConfig, "command", "compress", !$pg_backrest_file::bNoCompression),
    config_load(\%oConfig, "command", "decompress", !$pg_backrest_file::bNoCompression),
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

    # Get the source/destination file
    my $strSourceFile = $ARGV[1];
    my $strDestinationFile = basename($strSourceFile);
    
    # Determine if this is an archive file (don't want to do compression or checksum on .backup files)
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}$/;

    # Append the checksum (if requested)
    if ($bArchiveFile && !$bNoChecksum)
    {
        $strDestinationFile .= "-" . file_hash_get(PATH_DB_ABSOLUTE, $strSourceFile);
    }
    
    # Copy the archive file
    file_copy(PATH_DB_ABSOLUTE, $strSourceFile, PATH_BACKUP_ARCHIVE, $strDestinationFile, !$bArchiveFile);

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
    # Make sure that the cluster data directory exists
    $pg_backrest_file::strDbClusterPath = $oConfig{"cluster:$strCluster"}{path};

    if (!defined($pg_backrest_file::strDbClusterPath))
    {
        confess &log(ERROR, "cluster data path is not defined");
    }

    unless (-e $pg_backrest_file::strDbClusterPath)
    {
        confess &log(ERROR, "cluster data path '${pg_backrest_file::strDbClusterPath}' does not exist");
    }

    # Find the previous backup based on the type
    my $strBackupLastPath = backup_type_find($strType, $pg_backrest_file::strBackupClusterPath);

    my %oLastManifest;

    if (defined($strBackupLastPath))
    {
        %oLastManifest = backup_manifest_load("${pg_backrest_file::strBackupClusterPath}/$strBackupLastPath/backup.manifest");
        &log(INFO, "Last backup label: $oLastManifest{common}{backup}{label}");
    }

    # Create the path for the new backup
    my $strBackupPath;

    if ($strType eq "full" || !defined($strBackupLastPath))
    {
        $strBackupPath = date_string_get() . "F";
        $strType = "full";
    }
    else
    {
        $strBackupPath = substr($strBackupLastPath, 0, 16);

        $strBackupPath .= "_" . date_string_get();
        
        if ($strType eq "differential")
        {
            $strBackupPath .= "D";
        }
        else
        {
            $strBackupPath .= "I";
        }
    }

    # Build backup tmp and config
    my $strBackupTmpPath = "${pg_backrest_file::strBackupClusterPath}/backup.tmp";
    my $strBackupConfFile = "${strBackupTmpPath}/backup.manifest";

    # If the backup tmp path already exists, delete the conf file
    if (-e $strBackupTmpPath)
    {
        &log(WARNING, "backup path $strBackupTmpPath already exists");

        # !!! This is temporary until we can clean backup dirs
        rmtree($strBackupTmpPath) or confess &log(ERROR, "unable to delete backup.tmp");
        #if (-e $strBackupConfFile)
        #{
        #    unlink $strBackupConfFile or die &log(ERROR, "backup config ${strBackupConfFile} could not be deleted");
        #}
    }
    # Else create the backup tmp path
    else
    {
        &log(INFO, "creating backup path $strBackupTmpPath");
        mkdir $strBackupTmpPath or confess &log(ERROR, "backup path ${strBackupTmpPath} could not be created");
    }

    # Create a new backup manifest hash
    my %oBackupManifest;

    # Start backup
    my $strLabel = $strBackupPath;
    ${oBackupManifest}{common}{backup}{label} = $strLabel;

    my $strArchiveStart = trim(execute($pg_backrest_file::strCommandPsql .
        " -c \"set client_min_messages = 'warning';copy (select pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}') as xlog) to stdout\" postgres"));
        
    ${oBackupManifest}{archive}{archive_location}{start} = $strArchiveStart;

    &log(INFO, 'Backup archive start: ' . $strArchiveStart);

    # Build the backup manifest
    my %oTablespaceMap = tablespace_map_get($pg_backrest_file::strCommandPsql);
    backup_manifest_build($pg_backrest_file::strCommandManifest, $pg_backrest_file::strDbClusterPath, \%oBackupManifest, \%oLastManifest, \%oTablespaceMap);

    # Delete files leftover from a partial backup
    # !!! do it

    # Perform the backup
    backup(\%oBackupManifest);
           
    # Stop backup
    my $strArchiveStop = trim(execute($pg_backrest_file::strCommandPsql .
        " -c \"set client_min_messages = 'warning'; copy (select pg_xlogfile_name(xlog) from pg_stop_backup() as xlog) to stdout\" postgres"));

    ${oBackupManifest}{archive}{archive_location}{stop} = $strArchiveStop;

    &log(INFO, 'Backup archive stop: ' . $strArchiveStop);

    # After the backup has been stopped, need to 
    my @stryArchive = archive_list_get($strArchiveStart, $strArchiveStop);

    foreach my $strArchive (@stryArchive)
    {
        my $strArchivePath = dirname(path_get(PATH_BACKUP_ARCHIVE, $strArchive));
        my @stryArchiveFile = file_list_get(PATH_BACKUP_ABSOLUTE, $strArchivePath, "^${strArchive}(-[0-f]+){0,1}(\\.${pg_backrest_file::strCompressExtension}){0,1}\$");
        
        if (scalar @stryArchiveFile != 1)
        {
            confess &log(ERROR, "Zero or more than one file found for glob: ${strArchivePath}"); 
        }

        &log(DEBUG, "    archiving: ${strArchive} (${stryArchiveFile[0]})");

        file_copy(PATH_BACKUP_ARCHIVE, $stryArchiveFile[0], PATH_BACKUP_TMP, "base/pg_xlog/${strArchive}");
    }
    
    # Save the backup conf file
    backup_manifest_save($strBackupConfFile, \%oBackupManifest);

    # Rename the backup tmp path to complete the backup
    rename($strBackupTmpPath, "${pg_backrest_file::strBackupClusterPath}/${strBackupPath}") or confess &log(ERROR, "unable to ${strBackupTmpPath} rename to ${strBackupPath}"); 

    # Expire backups (!!! Need to read this from config file)
    backup_expire($pg_backrest_file::strBackupClusterPath, 2, 2, "full", 2);
    
    exit 0;
}
