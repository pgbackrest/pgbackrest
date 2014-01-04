#!/usr/bin/perl

use strict;
use warnings;
use File::Basename;
use Getopt::Long;
use Config::IniFiles;
use IPC::System::Simple qw(capture);
use JSON;

# Process flags
my $bNoCompression;
my $bNoChecksum;
my $strConfigFile;
my $strCluster;
my $strType = "incremental";        # Type of backup: full, differential (diff), incremental (incr)

GetOptions ("no-compression" => \$bNoCompression,
            "no-checksum" => \$bNoChecksum,
            "config=s" => \$strConfigFile,
            "cluster=s" => \$strCluster,
            "type=s" => \$strType)
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
# DATE_STRING_GET - Get the date and time string
####################################################################################################################################
sub date_string_get
{
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

    return(sprintf("%4d%02d%02d-%02d%02d%02d", $year+1900, $mon+1, $mday, $hour, $min, $sec));
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
    my $strOutput;

#    print("$strCommand");
    $strOutput = capture($strCommand);

#    if ($strOutput eq "")
#    {  
#        print(" ... complete\n\n");
#    }
#    else
#    {
#        print(" ... complete\n$strOutput\n\n");
#    }

    return $strOutput;
}

####################################################################################################################################
# BACKUP_TYPE_FIND - Find the last backup depending on the type
####################################################################################################################################
sub backup_type_find
{
    my $strType = shift;
    my $strBackupClusterPath = shift;
    my $strDirectory;

    my $hDir;
    
    if ($strType eq 'incremental')
    {
        opendir $hDir, $strBackupClusterPath or die &log(ERROR, "unable to open path ${strBackupClusterPath}");
        my @stryFile = sort {$b cmp $a} grep(/^[0-F]{8}\-[0-F]{6}F\_[0-F]{8}\-[0-F]{6}(I|D)$/i, readdir $hDir); 
        close $hDir;
        
        #print "incremental: @stryFile\n";
        $strDirectory = $stryFile[0];
    }

    if (!defined($strDirectory) && $strType ne "full")
    {
        opendir $hDir, $strBackupClusterPath or die &log(ERROR, "unable to open path ${strBackupClusterPath}");
        my @stryFile = sort {$b cmp $a} grep(/^[0-F]{8}\-[0-F]{6}F$/i, readdir $hDir); 
        close $hDir;

        #print "next: @stryFile\n";
        $strDirectory = $stryFile[0];
    }
    
    return $strDirectory;
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
#            print "Hash {$stryHeader[0]}{$stryLine[0]}{$stryHeader[$iColumnIdx]} = " . (defined $oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} ? $oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} : "null") . "\n";
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
    my $strCommandPsql = shift;

    my %oTablespaceMap = data_hash_build("oid\tname\n" . execute($strCommandPsql .
                                         " -c 'copy (select oid, spcname from pg_tablespace) to stdout' postgres"), "\t");
    
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
# BACKUP_MANIFEST_LOAD - Load the backup manifest
####################################################################################################################################
sub backup_manifest_load
{
    my $strBackupManifestFile = shift;
    
    my %oBackupManifestFile;
    tie %oBackupManifestFile, 'Config::IniFiles', (-file => $strBackupManifestFile) or die &log(ERROR, "backup manifest '${strBackupManifestFile}' could not be loaded");

    my %oBackupManifest;
    my $strSection;

    foreach $strSection (sort(keys %oBackupManifestFile))
    {
        my $strKey;
        
        #&log(DEBUG, "section: ${strSection}");

        foreach $strKey (sort(keys ${oBackupManifestFile}{"${strSection}"}))
        {
            my $strValue = ${oBackupManifestFile}{"${strSection}"}{"$strKey"};

            #&log(DEBUG, "    key: ${strKey}=${strValue}");
            $oBackupManifest{"${strSection}"}{"$strKey"} = decode_json($strValue);
        }
    }
    
    return %oBackupManifest;
}
####################################################################################################################################
# BACKUP_MANIFEST_SAVE - Save the backup manifest
####################################################################################################################################
sub backup_manifest_save
{
    my $strBackupManifestFile = shift;
    my $oBackupManifestRef = shift;
    
    my %oBackupManifest;
    tie %oBackupManifest, 'Config::IniFiles' or die &log(ERROR, "Unable to create backup config");

    my $strSection;

    foreach $strSection (sort(keys $oBackupManifestRef))
    {
        my $strKey;
        
        #&log(DEBUG, "section: ${strSection}");

        foreach $strKey (sort(keys ${$oBackupManifestRef}{"${strSection}"}))
        {
            my $strValue = encode_json(${$oBackupManifestRef}{"${strSection}"}{"$strKey"});

            #&log(DEBUG, "    key: ${strKey}=${strValue}");
            $oBackupManifest{"${strSection}"}{"$strKey"} = $strValue;
        }
    }
    
    tied(%oBackupManifest)->WriteConfig($strBackupManifestFile);
}

####################################################################################################################################
# BACKUP_MANIFEST_BUILD - Create the backup manifest
####################################################################################################################################
sub backup_manifest_build
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

    foreach $strName (sort(keys $oManifestHash{name}))
    {
        # Don't process anything in pg_xlog
        if (index($strName, 'pg_xlog/') != 0)
        {
            my $cType = $oManifestHash{name}{"${strName}"}{type};
            my $strLinkDestination = $oManifestHash{name}{"${strName}"}{link_destination};
            my $strSection = "${strLevel}:path";

            #&log(DEBUG, "$strClusterDataPath ${cType}: $strName");

            if ($cType eq "f")
            {
                $strSection = "${strLevel}:file";
            }
            elsif ($cType eq "l")
            {
                $strSection = "${strLevel}:link";
            }
            elsif ($cType ne "d")
            {
                die &log(ERROR, "Unrecognized file type $cType for file $strName");
            }

            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{user} = $oManifestHash{name}{"${strName}"}{user};
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{group} = $oManifestHash{name}{"${strName}"}{group};
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{permission} = $oManifestHash{name}{"${strName}"}{permission};
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time} = 
                (split("\\.", $oManifestHash{name}{"${strName}"}{modification_time}))[0];

#                print("modification time:" . ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time});

            if ($cType eq "f" || $cType eq "l")
            {
                ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{inode} = $oManifestHash{name}{"${strName}"}{inode};
            }

            if ($cType eq "f")
            {
                ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} = $oManifestHash{name}{"${strName}"}{size};
            }

            if ($cType eq "l")
            {
                ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{link_destination} = $oManifestHash{name}{"${strName}"}{link_destination};

                if (index($strName, 'pg_tblspc/') == 0 && $strLevel eq "base")
                {
                    my $strTablespaceOid = basename($strName);
                    my $strTablespaceName = ${$oTablespaceMapRef}{oid}{"$strTablespaceOid"}{name};
                    #&log(DEBUG, "tablespace: ${strTablespace}");

                    ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{oid} = $strTablespaceOid;
                    ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{path} = $strLinkDestination;
                    
                    backup_manifest_build($strCommandManifest, $strLinkDestination, $oBackupManifestRef, $oTablespaceMapRef, "tablespace:${strTablespaceName}");
                }
            }
        }
    }
}

####################################################################################################################################
# BACKUP - Perform the backup
####################################################################################################################################
sub backup
{
    my $strCommandChecksum = shift;
    my $strCommandCompress = shift;
    my $strCommandDecompress = shift;
    my $strCommandCopy = shift;
    my $strClusterDataPath = shift;
    my $strBackupTmpPath = shift;
    my $oBackupManifestRef = shift;

    my $strSectionPath;
    my $strCommand;

    # Create the backup file dir
    my $strBackupDestinationTmpFile = "${strBackupTmpPath}/file.tmp";

    # Iterate through the path sections to backup
    foreach $strSectionPath (sort(keys $oBackupManifestRef))
    {
        # Skip non-path sections
        if ($strSectionPath !~ /\:path$/)
        {
            next;
        }

        # Determine the source and destination backup paths
        my $strBackupSourcePath;
        my $strBackupDestinationPath;
        my $strSectionFile;

        if ($strSectionPath =~ /^base\:/)
        {
            $strBackupSourcePath = "${strClusterDataPath}";
            $strBackupDestinationPath = "${strBackupTmpPath}/base";
            $strSectionFile = "base:file";
        }
        elsif ($strSectionPath =~ /^tablespace\:/)
        {
            my $strTablespaceName = (split(":", $strSectionPath))[1];
            $strBackupSourcePath = ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{path};
            $strBackupDestinationPath = "${strBackupTmpPath}/tablespace";

            # Create the tablespace path
            unless (-e $strBackupDestinationPath)
            {
                execute("mkdir -p -m 0750 ${strBackupDestinationPath}");
            }
            
            $strBackupDestinationPath = "${strBackupTmpPath}/tablespace/${strTablespaceName}";
            $strSectionFile = "tablespace:${strTablespaceName}:file";
            #my $strSectionLink = "tablespace:${strTablespaceName}:link";
            
            # Create link to the tablespace
            my $strTablespaceLink = "pg_tblspc/" . ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{oid};
            my $strBackupTablespaceLink = "${strBackupTmpPath}/base/${strTablespaceLink}";
            #my $strBackupLinkPath = ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{path};
            
            if (-e $strBackupTablespaceLink)
            {
                unlink $strBackupTablespaceLink or die &log(ERROR, "Unable to remove table link '${strBackupTablespaceLink}'");
            }
            
            execute("ln -s ../../tablespace/${strTablespaceName} $strBackupTablespaceLink");
            execute ("chmod " . ${$oBackupManifestRef}{"base:link"}{$strTablespaceLink}{permission} . " ${strBackupTablespaceLink}");
        }
        else
        {
            die "Cannot find type for section ${strSectionPath}";
        }

        # Create the base or tablespace path
        unless (-e $strBackupDestinationPath)
        {
            execute("mkdir -p -m 0750 ${strBackupDestinationPath}");
            #mkdir $strBackupDestinationPath, 0750 or die "Unable to create path ${strBackupDestinationPath}";
        }

        # Create all the paths required to store the files
        my $strPath;
        
        foreach $strPath (sort(keys ${$oBackupManifestRef}{"${strSectionPath}"}))
        {
#            my $lModificationTime = ${$oBackupManifestRef}{"${strSectionPath}"}{"$strPath"}{modification_time};
            my $strBackupDestinationSubPath = "${strBackupDestinationPath}/${strPath}";

            unless (-e $strBackupDestinationSubPath)
            {
                execute("mkdir -p ${strBackupDestinationSubPath}");
            }
            
            execute ("chmod ${$oBackupManifestRef}{$strSectionPath}{$strPath}{permission} ${strBackupDestinationSubPath}");
#            utime($lModificationTime, $lModificationTime, $strBackupDestinationTmpFile) or die "Unable to set time for ${strBackupDestinationTmpFile}";
        }

        #&log(DEBUG, "Backing up ${strBackupSourcePath}");

        # Possible for the path section to exist with no files (i.e. empty tablespace)
        if (!defined(${$oBackupManifestRef}{"${strSectionFile}"}))
        {
            next;
        }

        # Iterate through the files for each backup source path
        my $strFile;
        
        foreach $strFile (sort(keys ${$oBackupManifestRef}{"${strSectionFile}"}))
        {
            my $strBackupSourceFile = "${strBackupSourcePath}/${strFile}";
            my $iSize = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{size};
            my $lModificationTime = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{modification_time};

            #&log(DEBUG, "   Backing up ${strBackupSourceFile}");
            
            # Copy the file and calculate checksum
            my $strBackupDestinationFile = "${strBackupDestinationPath}/${strFile}";
            my $strHash;

            if ($iSize == 0)
            {
                execute("touch ${strBackupDestinationTmpFile}")
            }
            else
            {
                if ($bNoCompression || $iSize == 0)
                {
                    $strCommand = $strCommandCopy;
                    $strCommand =~ s/\%source\%/${strBackupSourceFile}/g;
                    $strCommand =~ s/\%destination\%/${strBackupDestinationTmpFile}/g;

                    execute($strCommand);
                
                    $strHash = file_hash_get($strCommandChecksum, $strBackupDestinationTmpFile);
                }
                else
                {
                    $strCommand = $strCommandCompress;
                    $strCommand =~ s/\%file\%/${strBackupSourceFile}/g;
                    $strCommand .= " > ${strBackupDestinationTmpFile}";

                    execute($strCommand);

                    $strCommand = $strCommandDecompress;
                    $strCommand =~ s/\%file\%/${strBackupDestinationTmpFile}/g;
                    $strCommand .= " | " . $strCommandChecksum;
                    $strCommand =~ s/\%file\%//g;

                    #&log(DEBUG, "       command ${strCommand}");

                    $strHash = trim(execute($strCommand));
                }
            
                #&log(DEBUG, "       Hash ${strHash}");
            
                if (!$bNoCompression)
                {
                    $strBackupDestinationFile .= ".gz";
                }
            }

            # Set permissions and move the file
            execute ("chmod ${$oBackupManifestRef}{$strSectionFile}{$strFile}{permission} ${strBackupDestinationTmpFile}");
            utime($lModificationTime, $lModificationTime, $strBackupDestinationTmpFile) or die "Unable to set time for ${strBackupDestinationTmpFile}";
            rename $strBackupDestinationTmpFile, $strBackupDestinationFile or die "Unable to move ${strBackupDestinationFile}";

            # Write the hash into the backup manifest
            if (defined($strHash))
            {
                ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{checksum} = $strHash;
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
my $strCommandDecompress = config_load(\%oConfig, "command", "decompress", !$bNoCompression);
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

    # Setup the copy command
    my $strCommand = "";

    # !!! Modify this to skip compression and checksum for any file that is not a log file
    if ($strSourceFile =~ /\.backup$/)
    {
        $strCommand = $strCommandCopy;
        $strCommand =~ s/\%source\%/$strSourceFile/g;
        $strCommand =~ s/\%destination\%/$strDestinationFile/g;
    }
    else
    {
        # Calculate sha1 hash for the file (unless disabled)
        if (!$bNoChecksum)
        {
            $strDestinationFile .= "-" . file_hash_get($strCommandChecksum, $strSourceFile);
        }
    
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
    }
    
    # Execute the copy
    execute($strCommand);

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
    die &log(ERROR, "backup type must be full, differential (diff), incremental (incr)");
}

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
my $strCommandPsql = config_load(\%oConfig, "command", "psql");

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

    # Find the previous backup based on the type
    my $strBackupLastPath = backup_type_find($strType, $strBackupClusterPath);

    if (defined($strBackupLastPath))
    {
        &log(INFO, 'Last backup: ' . $strBackupLastPath);
    }

    # Create the path for the new backup
    my $strBackupPath;

    if ($strType eq "full" || !defined($strBackupLastPath))
    {
        $strBackupPath = date_string_get() . "F";
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
    my $strBackupTmpPath = "${strBackupClusterPath}/backup.tmp";
    my $strBackupConfFile = "${strBackupTmpPath}/backup.manifest";

    # If the backup tmp path already exists, delete the conf file
    if (-e $strBackupTmpPath)
    {
        &log(WARNING, "backup path $strBackupTmpPath already exists");

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

    # Create a new backup manifest hash
    my %oBackupManifest;

    # Start backup
    my $strLabel = $strBackupPath;

    my $strArchiveStart = trim(execute($strCommandPsql .
        " -c \"copy (select pg_xlogfile_name(xlog) from pg_start_backup('${strLabel}') as xlog) to stdout\" postgres"));
        
    ${oBackupManifest}{archive}{archive_location}{start} = $strArchiveStart;

    &log(INFO, 'Backup archive start: ' . $strArchiveStart);

    # Build the backup manifest
    my %oTablespaceMap = tablespace_map_get($strCommandPsql);
    backup_manifest_build($strCommandManifest, $strClusterDataPath, \%oBackupManifest, \%oTablespaceMap);

    # Perform the backup
#    backup($strCommandChecksum, $strCommandCompress, $strCommandDecompress, $strCommandCopy, $strClusterDataPath,
#           $strBackupTmpPath, \%oBackupManifest);

    # Stop backup
    my $strArchiveStop = trim(execute($strCommandPsql .
        " -c \"copy (select pg_xlogfile_name(xlog) from pg_stop_backup() as xlog) to stdout\" postgres"));

    ${oBackupManifest}{archive}{archive_location}{stop} = $strArchiveStop;

    &log(INFO, 'Backup archive stop: ' . $strArchiveStop);

    # Fetch the archive logs and backup file
    # !!! do it

    # Delete files leftover from a partial backup
    # !!! do it

    # Save the backup conf file
    backup_manifest_save($strBackupConfFile, \%oBackupManifest);
    backup_manifest_load($strBackupConfFile);

    # Rename the backup tmp path to complete the backup
    rename($strBackupTmpPath, "${strBackupClusterPath}/${strBackupPath}") or die &log(ERROR, "unable to ${strBackupTmpPath} rename to ${strBackupPath}"); 
}
