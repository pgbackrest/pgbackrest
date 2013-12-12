#!/usr/bin/perl

use strict;
use warnings;
use File::Basename;
use Getopt::Long;
use Config::IniFiles;

# Process flags
my $bNoCompression;
my $bNoChecksum;
my $strConfigFile;
my $strCluster;

GetOptions ("no-compression" => \$bNoCompression,
            "no-checksum" => \$bNoChecksum,
            "config=s" => \$strConfigFile,
            "cluster=s" => \$strCluster)
    or die("Error in command line arguments\n");

####################################################################################################################################
# TRIM - trim whitespace off strings
####################################################################################################################################
sub trim
{
    my $strBuffer = shift;

    $strBuffer =~ s/^\s+|\s+$//g;

    return $strBuffer;
}

####################################################################################################################################
# LOG - log messages
####################################################################################################################################
use constant 
{
    DEBUG   => 'DEBUG',
    INFO    => 'INFO',
    WARNING => 'WARNING',
    ERROR   => 'ERROR'
};

sub log
{
    my $strLevel = shift;
    my $strMessage = shift;

    print "${strLevel}: ${strMessage}\n";

    return $strMessage;
}

####################################################################################################################################
# EXECUTE - execute a command
####################################################################################################################################
sub execute
{
    my $strCommand = shift;
    
#   print("$strCommand\n");
    my $strOutput = qx($strCommand) or return 0;
#   print("$strOutput\n");
    
    return($strOutput);
}

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
        die &log(ERROR, 'config value ${strSection}->${strKey} is undefined');
    }
    
    return $strValue;
}

####################################################################################################################################
# FILE_HASH_GET - get the sha1 hash for a file
####################################################################################################################################
sub file_hash_get
{
    my $strCommand = shift;
    my $strFile = shift;
    
    $strCommand =~ s/\%file\%/$strFile/g;
    
    my $strHash = trim(execute($strCommand));
    
    return($strHash);
}

####################################################################################################################################
# BACKUP_MANIFEST - Create the backup manifest
####################################################################################################################################
sub backup_manifest
{
    my $strCommandManifest = shift;
    my $strClusterDataPath = shift;
    my $oBackupConfigRef = shift;
    my $strLevel = shift;
    
    if (!defined($strLevel))
    {
        $strLevel = "base";
    }

    my $strCommand = $strCommandManifest;
    $strCommand =~ s/\%path\%/$strClusterDataPath/g;
    my $strManifest = execute($strCommand);
    
    my @stryFile = split("\n", $strManifest);

    # Flag to only allow the root directory once
    my $bRootDir = 0;
    
    for (my $iFileIdx = 0; $iFileIdx < scalar @stryFile; $iFileIdx++)
    {
        my @stryField = split("\t", $stryFile[$iFileIdx]);
        
        my $dfModifyTime = $stryField[0];
        my $lInode = $stryField[1];
        my $cType = $stryField[2];
        my $strPermission = $stryField[3];
        my $strUser = $stryField[4];
        my $strGroup = $stryField[5];
        my $strSize = $stryField[6];
        my $strName = $stryField[7];
        my $strLinkDestination = $stryField[8];

        if (!defined($strName))
        {
            if ($bRootDir)
            {
                die "Root dir appeared twice - check manifest command"
            }
            
            $bRootDir = 1;
            $strName = ".";
        }
        
        # Don't process anything in pg_xlog
        if (index($strName, 'pg_xlog/') != 0)
        {
            # Process paths
            if ($cType eq "d")
            {
                ${$oBackupConfigRef}{"${strLevel}:path"}{"$strName"} = "$strUser,$strGroup,$strPermission";

                &log(DEBUG, "$strClusterDataPath path: $strName");
            }

            # Process symbolic links (hard links not supported)
            elsif ($cType eq "l")
            {
                &log(DEBUG, "$strClusterDataPath link: $strName -> $strLinkDestination");

                ${$oBackupConfigRef}{"${strLevel}:link"}{"$strName"} = "$dfModifyTime,$strSize,$strUser,$strGroup,$strPermission,$lInode";

                if (index($strName, 'pg_tblspc/') == 0)
                {
                    #${$oBackupConfigRef}{"base:tablespace"}{"$strName"} = $;

                    backup_manifest($strCommandManifest, $strLinkDestination, $oBackupConfigRef, $strName);
                }
            }
        
            # Process files except those in pg_xlog (hard links not supported)
            elsif ($cType eq "f")
            {
                ${$oBackupConfigRef}{"${strLevel}:file"}{"$strName"} = "$dfModifyTime,$strSize,$strUser,$strGroup,$strPermission,$lInode";
                
                &log(DEBUG, "$strClusterDataPath file: $strName");
            }

            # Unrecognized type - fail
            else
            {
                die &log(ERROR, "Unrecognized file type $cType for file $strName");
            }
        }
    }
}

####################################################################################################################################
# START MAIN
####################################################################################################################################
# Get the command
my $strOperation = $ARGV[0];

####################################################################################################################################
# LOAD CONFIG FILE
####################################################################################################################################
if (!defined($strConfigFile))
{
    $strConfigFile = "/etc/pg_backrest.conf";
}

my %oConfig;
tie %oConfig, 'Config::IniFiles', (-file => $strConfigFile) or die "Unable to find config file";

# Load commands required for archive-push
my $strCommandChecksum = config_load(\%oConfig, "command", "checksum", !$bNoChecksum);
my $strCommandCompress = config_load(\%oConfig, "command", "compress", !$bNoCompression);
my $strCommandCopy = config_load(\%oConfig, "command", "copy", $bNoCompression);

####################################################################################################################################
# ARCHIVE-PUSH Command
####################################################################################################################################
if ($strOperation eq "archive-push")
{
    # archive-push command must have three arguments
    if (@ARGV != 3)
    {
        die "not enough arguments - show usage";
    }

    # Get the source dir/file
    my $strSourceFile = $ARGV[1];
    
    unless (-e $strSourceFile)
    {
        die "source file does not exist - show usage";
    }

    # Get the destination dir/file
    my $strDestinationFile = $ARGV[2];

    # Make sure the destination directory exists
    unless (-e dirname($strDestinationFile))
    {
        die "destination dir does not exist - show usage";
    }

    # Make sure the destination file does NOT exist - ignore checksum and extension in case they (or options) have changed
    if (glob("$strDestinationFile*"))
    {
        die "destination file already exists";
    }

    # Calculate sha1 hash for the file (unless disabled)
    if (!$bNoChecksum)
    {
        $strDestinationFile .= "-" . file_hash_get($strCommandChecksum, $strSourceFile);
    }
    
    # Setup the copy command
    my $strCommand = "";

    if ($bNoCompression)
    {
        $strCommand = $strCommandCopy;
        $strCommand =~ s/\%source\%/$strSourceFile/g;
        $strCommand =~ s/\%destination\%/$strDestinationFile/g;
    }
    else
    {
        $strCommand = $strCommandCompress;
        $strCommand =~ s/\%file\%/$strSourceFile/g;
        $strCommand .= " > $strDestinationFile.gz";
    }
    
    # Execute the copy
    execute($strCommand);

    exit 0;
}

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
# Load and check the base backup path
my $strBasePath = $oConfig{common}{backup_path};

if (!defined($strBasePath))
{
    die &log(ERROR, "common:backup_path undefined");
}

unless (-e $strBasePath)
{
    die &log(ERROR, "base path ${strBasePath} does not exist");
}

# Load and check the cluster
if (!defined($strCluster))
{
    $strCluster = "db"; #!!! Modify to load cluster from conf if there is only one, else error
}

my $strBackupClusterPath = "${strBasePath}/${strCluster}";

unless (-e $strBackupClusterPath)
{
    &log (INFO, "creating cluster path ${strBackupClusterPath}");
    mkdir $strBackupClusterPath or die &log(ERROR, "cluster backup path '${strBackupClusterPath}' create failed");
}

# Load commands required for backup
my $strCommandManifest = config_load(\%oConfig, "command", "manifest");

####################################################################################################################################
# BACKUP
####################################################################################################################################
if ($strOperation eq "backup")
{
    # Make sure that the cluster data directory exists
    my $strClusterDataPath = $oConfig{"cluster:$strCluster"}{data_path};

    if (!defined($strClusterDataPath))
    {
        die &log(ERROR, "cluster data path is not defined");
    }

    unless (-e $strClusterDataPath)
    {
        die &log(ERROR, "cluster data path '${strClusterDataPath}' does not exist");
    }

    # Build backup tmp and config
    my $strBackupTmpPath = "${strBackupClusterPath}/backup.tmp";
    my $strBackupConfFile = "${strBackupTmpPath}/backup.conf";

    # If the backup tmp path already exists, delete the conf file
    if (-e $strBackupTmpPath)
    {
        &log(INFO, "backup path $strBackupTmpPath already exists");

        if (-e $strBackupConfFile)
        {
            unlink $strBackupConfFile or die &log(ERROR, "backup config ${strBackupConfFile} could not be deleted");
        }
    }
    # Else create the backup tmp path
    else
    {
        &log(INFO, "creating backup path $strBackupTmpPath");
        mkdir $strBackupTmpPath or die &log(ERROR, "backup path ${strBackupTmpPath} could not be created");
    }

    # Create a new backup conf hash
    my %oBackupConfig;
    tie %oBackupConfig, 'Config::IniFiles' or die &log(ERROR, "Unable to create backup config");

    # Build the backup manifest
    backup_manifest($strCommandManifest, $strClusterDataPath, \%oBackupConfig);

    # Delete files leftover from a partial backup
    # !!! do it

    # Perform the backup
    # !!! do it
    
    # Save the backup conf file
    tied(%oBackupConfig)->WriteConfig($strBackupConfFile);

    # Rename the backup tmp path to complete the backup
    # !!! Still not sure about format, probably YYYYMMDDTHH24MMSS
}
