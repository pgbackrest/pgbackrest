#!/usr/bin/perl

use strict;
use warnings;
use File::Basename;
use Getopt::Long;
use Config::IniFiles;
use JSON;

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
# DATA_HASH_BUILD - Hash a delimited file with header
####################################################################################################################################
sub data_hash_build
{
    my $strData = shift;
    my $strDelimiter = shift;
    my $strUndefinedKey = shift;

    my @stryFile = split("\n", $strData);
    my @stryHeader = split($strDelimiter, $stryFile[0]);
    
#    print "data: $strData\n";

    my %oHash;

    for (my $iLineIdx = 1; $iLineIdx < scalar @stryFile; $iLineIdx++)
    {
        my @stryLine = split($strDelimiter, $stryFile[$iLineIdx]);

        if (!defined($stryLine[0]) || $stryLine[0] eq "")
        {
            $stryLine[0] = $strUndefinedKey;
        }

#        print "line: @stryLine\n";
        
        for (my $iColumnIdx = 1; $iColumnIdx < scalar @stryHeader; $iColumnIdx++)
        {
            if (defined($oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"}))
            {
                die "the first column must be unique to build the hash";
            }
            
            $oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} = $stryLine[$iColumnIdx];
            #print "Hash {$stryHeader[0]}{$stryLine[0]}{$stryHeader[$iColumnIdx]} = " . (defined $oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} ? $oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} : "null") . "\n";
            #$oHash{$stryHeader[$iColumnIdx]}{$stryLine[$iColumnIdx]}{$stryHeader[0]} = $stryLine[0];
        }
    }

    return %oHash;
}

####################################################################################################################################
# TABLESPACE_MAP_GET - Get the mapping between oid and tablespace name
####################################################################################################################################
sub tablespace_map_get
{
    my $strCommandTablespaceMap = shift;

    my %oTablespaceMap = data_hash_build("oid\tname\n" . execute($strCommandTablespaceMap), "\t");
    
    return %oTablespaceMap;
}

####################################################################################################################################
# MANIFEST_GET - Get a directory manifest
####################################################################################################################################
sub manifest_get
{
    my $strCommandManifest = shift;
    my $strPath = shift;

    my $strCommand = $strCommandManifest;
    $strCommand =~ s/\%path\%/$strPath/g;

    my %oManifest = data_hash_build("name\ttype\tuser\tgroup\tpermission\tmodification_time\tinode\tsize\tlink_destination\n" .
                                    execute($strCommand), "\t", ".");
    
    return %oManifest;
}

####################################################################################################################################
# BACKUP_MANIFEST - Create the backup manifest
####################################################################################################################################
sub backup_manifest
{
    my $strCommandManifest = shift;
    my $strClusterDataPath = shift;
    my $oBackupManifestRef = shift;
    my $oTablespaceMapRef = shift;
    my $strLevel = shift;
    
    if (!defined($strLevel))
    {
        $strLevel = "base";
    }
    
    my %oManifestHash = manifest_get($strCommandManifest, $strClusterDataPath);
    my $strName;
    my $cType;
    my $strLinkDestination;

#    print "test: " . $oManifestHash{name}{"."}{type} . "\n";
#    foreach $strName (sort(keys $oManifestHash{name}{base}{inode}))
#    {
#        print "found key $strName\n";
#    }

    foreach $strName (sort(keys $oManifestHash{name}))
    {
        $cType = $oManifestHash{name}{"${strName}"}{type};
        $strLinkDestination = $oManifestHash{name}{"${strName}"}{link_destination};
        
        # Don't process anything in pg_xlog
        if (index($strName, 'pg_xlog/') != 0)
        {
            # Process paths
            if ($cType eq "d")
            {
                ${$oBackupManifestRef}{"${strLevel}:path"}{"$strName"} = "test"; #"$strUser,$strGroup,$strPermission";

                &log(DEBUG, "$strClusterDataPath path: $strName");
            }

            # Process symbolic links (hard links not supported)
            elsif ($cType eq "l")
            {
                &log(DEBUG, "$strClusterDataPath link: $strName -> $strLinkDestination");

                ${$oBackupManifestRef}{"${strLevel}:link"}{"$strName"} = "test"; #"$dfModifyTime,$strSize,$strUser,$strGroup,$strPermission,$lInode";

                if (index($strName, 'pg_tblspc/') == 0)
                {
                    #${$oBackupConfigRef}{"base:tablespace"}{"$strName"} = $;

                    backup_manifest($strCommandManifest, $strLinkDestination, $oBackupManifestRef, $oTablespaceMapRef, $strName);
                }
            }
        
            # Process files except those in pg_xlog (hard links not supported)
            elsif ($cType eq "f")
            {
                ${$oBackupManifestRef}{"${strLevel}:file"}{"$strName"} = "test"; #"$dfModifyTime,$strSize,$strUser,$strGroup,$strPermission,$lInode";
                
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
my $strCommandTablespace = config_load(\%oConfig, "command", "tablespace_map");

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
    my %oBackupManifest;
    tie %oBackupManifest, 'Config::IniFiles' or die &log(ERROR, "Unable to create backup config");
    
    # Build the backup manifest
    backup_manifest($strCommandManifest, $strClusterDataPath, \%oBackupManifest); #, \(tablespace_map_get($strCommandTablespace)));
    #\%oBackupConfig
    # Delete files leftover from a partial backup
    # !!! do it

    # Perform the backup
    # !!! do it
    
    # Save the backup conf file
    tied(%oBackupManifest)->WriteConfig($strBackupConfFile);

    # Rename the backup tmp path to complete the backup
    # !!! Still not sure about format, probably YYYYMMDDTHH24MMSS
}
