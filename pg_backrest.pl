#!/usr/bin/perl -w

use strict;
use File::Basename;
use Getopt::Long;
use Config::IniFiles;
#use Readonly;

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
#Readonly my $DEBUG = "DEBUG";
#Readonly my $INFO = "INFO";
#Readonly my $WARNING = "WARNING";
#Readonly my $ERROR = "ERROR";

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
        $strDestinationFile .= "-" . file_hash_get($oConfig{command}{checksum}, $strSourceFile);
    }
    
    # Setup the copy command
    my $strCommand = "";

    if ($bNoCompression)
    {
        $strCommand = $oConfig{command}{copy};
        $strCommand =~ s/\%source\%/$strSourceFile/g;
        $strCommand =~ s/\%destination\%/$strDestinationFile/g;
    }
    else
    {
        $strCommand = $oConfig{command}{compress};
        $strCommand =~ s/\%file\%/$strSourceFile/g;
        $strCommand .= " > $strDestinationFile.gz";
    }
    
    # Execute the copy
    execute($strCommand);

#    print "$strCommandManifest\n";
#    print execute($strCommandManifest) . "\n";
    exit 0;
}

####################################################################################################################################
# GET MORE CONFIG INFO
####################################################################################################################################
my $strBasePath = $oConfig{common}{backup_path};

if (!defined($strBasePath))
{
    die 'undefined base path';
}

unless (-e $strBasePath)
{
    die 'base path ${strBackupPath} does not exist';
}

if (!defined($strCluster))
{
    $strCluster = "db";
}

my $strClusterPath = "${strBasePath}/${strCluster}";

unless (-e $strClusterPath)
{
    &log (INFO, "creating cluster path ${strClusterPath}");
    mkdir $strClusterPath or die &log(ERROR, "cluster backup path '${strClusterPath}' create failed");
}

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
    my $strBackupPath = "${strBasePath}/backup.tmp";
    my $strBackupConfFile = "${strBackupPath}/backup.conf";

    # If the backup tmp path already exists, delete the conf file
    if (-e $strBackupPath)
    {
        &log(INFO, "backup path $strBackupPath already exists");

        if (-e $strBackupConfFile)
        {
            unlink $strBackupConfFile or die "Unable to delete backup config";
        }
    }
    # Else create the backup tmp path
    else
    {
        &log(INFO, "creating backup path $strBackupPath");
        mkdir $strBackupPath or die "Unable to create backup path";
    }

    # Create a new backup conf hash
    my %oBackupConfig;
    tie %oBackupConfig, 'Config::IniFiles' or die 'Unable to create backup config';

    # Build the backup manifest
    backup_manifest($oConfig{command}{manifest}, $strClusterDataPath, \%oBackupConfig);
    
    # Save the backup conf file
    tied(%oBackupConfig)->WriteConfig($strBackupConfFile);
}
