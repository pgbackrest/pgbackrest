#!/usr/bin/perl

use strict;
use warnings;

use File::Basename;
use File::Path;
use Getopt::Long;
use Config::IniFiles;
use IPC::System::Simple qw(capture);
use JSON;
use File::Copy;
use File::Remove;
use Carp;
use Cwd qw(abs_path);

use lib dirname($0);
use pg_backrest_utility;
use pg_backrest_file;

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
####################################################################################################################################
## BACKUP FUNCTIONS
####################################################################################################################################
####################################################################################################################################

####################################################################################################################################
# BACKUP_REGEXP_GET - Generate a regexp depending on the backups that need to be found
####################################################################################################################################
sub backup_regexp_get
{
    my $bFull = shift;
    my $bDifferential = shift;
    my $bIncremental = shift;
    
    if (!$bFull && !$bDifferential && !$bIncremental)
    {
        confess &log(ERROR, 'one parameter must be true');
    }
    
    my $strDateTimeRegExp = "[0-9]{8}\\-[0-9]{6}";
    my $strRegExp = "^";
    
    if ($bFull || $bDifferential || $bIncremental)
    {
        $strRegExp .= $strDateTimeRegExp . "F";
    }
    
    if ($bDifferential || $bIncremental)
    {
        if ($bFull)
        {
            $strRegExp .= "(\\_";
        }
        
        $strRegExp .= $strDateTimeRegExp;
        
        if ($bDifferential && $bIncremental)
        {
            $strRegExp .= "(D|I)";
        }
        elsif ($bDifferential)
        {
            $strRegExp .= "D";
        }
        else
        {
            $strRegExp .= "I";
        }

        if ($bFull)
        {
            $strRegExp .= "){0,1}";
        }
    }
    
    $strRegExp .= "\$";
    
#    &log(DEBUG, "backup_regexp_get($bFull, $bDifferential, $bIncremental): $strRegExp");
    
    return $strRegExp;
}

####################################################################################################################################
# BACKUP_TYPE_FIND - Find the last backup depending on the type
####################################################################################################################################
sub backup_type_find
{
    my $strType = shift;
    my $strBackupClusterPath = shift;
    my $strDirectory;

    if ($strType eq 'incremental')
    {
        $strDirectory = (file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 1), "reverse"))[0];
    }

    if (!defined($strDirectory) && $strType ne "full")
    {
        $strDirectory = (file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), "reverse"))[0];
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
        confess &log(ERROR, 'config value ${strSection}->${strKey} is undefined');
    }
    
    return $strValue;
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
# BACKUP_MANIFEST_LOAD - Load the backup manifest
####################################################################################################################################
sub backup_manifest_load
{
    my $strBackupManifestFile = shift;
    
    my %oBackupManifestFile;
    tie %oBackupManifestFile, 'Config::IniFiles', (-file => $strBackupManifestFile) or confess &log(ERROR, "backup manifest '${strBackupManifestFile}' could not be loaded");

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
    tie %oBackupManifest, 'Config::IniFiles' or confess &log(ERROR, "Unable to create backup config");

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
    my $strDbClusterPath = shift;
    my $oBackupManifestRef = shift;
    my $oLastManifestRef = shift;
    my $oTablespaceMapRef = shift;
    my $strLevel = shift;
    
    if (!defined($strLevel))
    {
        $strLevel = "base";
    }
    
    my %oManifestHash = manifest_get(PATH_DB_ABSOLUTE, $strDbClusterPath);
    my $strName;

    foreach $strName (sort(keys $oManifestHash{name}))
    {
        # Skip certain files during backup
        if ($strName =~ /^pg\_xlog\/.*/ ||    # pg_xlog/ - this will be reconstructed
            $strName =~ /^postmaster\.pid$/)  # postmaster.pid - to avoid confusing postgres when restoring
        {
            next;
        }
        
        my $cType = $oManifestHash{name}{"${strName}"}{type};
        my $strLinkDestination = $oManifestHash{name}{"${strName}"}{link_destination};
        my $strSection = "${strLevel}:path";

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
            confess &log(ASSERT, "unrecognized file type $cType for file $strName");
        }

        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{user} = $oManifestHash{name}{"${strName}"}{user};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{group} = $oManifestHash{name}{"${strName}"}{group};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{permission} = $oManifestHash{name}{"${strName}"}{permission};
        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time} = 
            (split("\\.", $oManifestHash{name}{"${strName}"}{modification_time}))[0];

        if ($cType eq "f" || $cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{inode} = $oManifestHash{name}{"${strName}"}{inode};

            my $bSizeMatch = true;
            
            if ($cType eq "f")
            {
                $bSizeMatch = ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} =
                              ${$oLastManifestRef}{"${strSection}"}{"$strName"}{size};
            }

            if (defined(${$oLastManifestRef}{"${strSection}"}{"$strName"}))
            {
                if ($bSizeMatch &&
                    ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{modification_time} ==
                        ${$oLastManifestRef}{"${strSection}"}{"$strName"}{modification_time})
                {
                    if (defined(${$oLastManifestRef}{"${strSection}"}{"$strName"}{reference}))
                    {
                        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference} =
                            ${$oLastManifestRef}{"${strSection}"}{"$strName"}{reference};
                    }
                    else
                    {
                        ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference} =
                            ${$oLastManifestRef}{common}{backup}{label};
                    }
                    
                    my $strReference = ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference};
                    
                    if (!defined(${$oBackupManifestRef}{common}{backup}{reference}))
                    {
                        ${$oBackupManifestRef}{common}{backup}{reference} = $strReference;
                    }
                    else
                    {
                        if (${$oBackupManifestRef}{common}{backup}{reference} !~ /$strReference/)
                        {
                            ${$oBackupManifestRef}{common}{backup}{reference} .= ",$strReference";
                        }
                    }
                }
            }
        }

        if ($cType eq "f")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} = $oManifestHash{name}{"${strName}"}{size};
        }

        if ($cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{link_destination} =
                $oManifestHash{name}{"${strName}"}{link_destination};

            if (index($strName, 'pg_tblspc/') == 0 && $strLevel eq "base")
            {
                my $strTablespaceOid = basename($strName);
                my $strTablespaceName = ${$oTablespaceMapRef}{oid}{"$strTablespaceOid"}{name};
                #&log(DEBUG, "tablespace: ${strTablespace}");

                ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{oid} = $strTablespaceOid;
                ${$oBackupManifestRef}{"${strLevel}:tablespace"}{"${strTablespaceName}"}{path} = $strLinkDestination;
                
                backup_manifest_build($strCommandManifest, $strLinkDestination, $oBackupManifestRef, $oLastManifestRef,
                                      $oTablespaceMapRef, "tablespace:${strTablespaceName}");
            }
        }
    }
}

####################################################################################################################################
# BACKUP - Perform the backup
#
# Uses the information in the manifest to determine which files need to be copied.  Directories and tablespace links are only
# created when needed, except in the case of a full backup or if hardlinks are requested.
####################################################################################################################################
sub backup
{
    my $oBackupManifestRef = shift;    # Manifest for the current backup

    # Iterate through the path sections of the manifest to backup
    my $strSectionPath;

    foreach $strSectionPath (sort(keys $oBackupManifestRef))
    {
        # Skip non-path sections
        if ($strSectionPath !~ /\:path$/)
        {
            next;
        }

        # Determine the source and destination backup paths
        my $strBackupSourcePath;        # Absolute path to the database base directory or tablespace to backup
        my $strBackupDestinationPath;   # Relative path to the backup directory where the data will be stored
        my $strSectionFile;             # Manifest section that contains the file data

        # Process the base database directory
        if ($strSectionPath =~ /^base\:/)
        {
            $strBackupSourcePath = $pg_backrest_file::strDbClusterPath;
            $strBackupDestinationPath = "base";
            $strSectionFile = "base:file";

            # Create the archive log directory
            path_create(PATH_BACKUP_TMP, "base/pg_xlog");
        }
        # Process each tablespace
        elsif ($strSectionPath =~ /^tablespace\:/)
        {
            my $strTablespaceName = (split(":", $strSectionPath))[1];
            $strBackupSourcePath = ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{path};
            $strBackupDestinationPath = "tablespace/${strTablespaceName}";
            $strSectionFile = "tablespace:${strTablespaceName}:file";

            # Create the tablespace directory and link
            if ($bHardLink || $strType eq "full")
            {
                path_create(PATH_BACKUP_TMP, $strBackupDestinationPath);

                link_create(PATH_BACKUP_TMP, $strBackupDestinationPath,
                            PATH_BACKUP_TMP, "pg_tblspc/" . ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{oid});
            }
        }
        else
        {
            confess &log(ASSERT, "cannot find type for section ${strSectionPath}");
        }

        # Create all the sub paths if this is a full backup or hardlinks are requested
        if ($bHardLink || $strType eq "full")
        {
            my $strPath;
            
            foreach $strPath (sort(keys ${$oBackupManifestRef}{"${strSectionPath}"}))
            {
                path_create(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strPath}",
                            ${$oBackupManifestRef}{"${strSectionPath}"}{"$strPath"}{permission});
            }
        }

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

            # If the file is a has reference it does not need to be copied since it can be retrieved from the referenced backup.
            # However, if hard-linking is turned on the link will need to be created
            my $strReference = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{reference};

            if (defined($strReference))
            {
                # If hardlinking is turned on then create a hardlink for files that have not changed since the last backup
                if ($bHardLink)
                {
                    &log(DEBUG, "   hard-linking ${strBackupSourceFile} from ${strReference}");

                    link_create(PATH_BACKUP_CLUSTER, "${strReference}/${strBackupDestinationPath}/${strFile}",
                                PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}", true);
                }
            }
            # Else copy/compress the file and generate a checksum
            else
            {
                # Copy the file from db to backup
                &log(DEBUG, "   backing up ${strBackupSourceFile}");
            
                my $lModificationTime = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{modification_time};
                file_copy(PATH_DB_ABSOLUTE, $strBackupSourceFile, PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}");

                # Write the hash into the backup manifest (if not suppressed)
                if (!$bNoChecksum)
                {
                    ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{checksum} =
                        file_hash_get(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}");
                }
            }
        }
    }
}

####################################################################################################################################
# ARCHIVE_LIST_GET
# 
# !!! Need to put code in here to cover pre-9.3 skipping log FF.
####################################################################################################################################
sub archive_list_get
{
    my $strArchiveStart = shift;
    my $strArchiveStop = shift;
    
    my $strTimeline = substr($strArchiveStart, 0, 8);
    my @stryArchive;
    my $iArchiveIdx = 0;

    if ($strTimeline ne substr($strArchiveStop, 0, 8))
    {
        confess "Timelines between ${strArchiveStart} and ${strArchiveStop} differ";
    }
        
    my $iStartMajor = hex substr($strArchiveStart, 8, 8);
    my $iStartMinor = hex substr($strArchiveStart, 16, 8);

    my $iStopMajor = hex substr($strArchiveStop, 8, 8);
    my $iStopMinor = hex substr($strArchiveStop, 16, 8);

    do
    {
        $stryArchive[$iArchiveIdx] = uc(sprintf("${strTimeline}%08x%08x", $iStartMajor, $iStartMinor));

        if ($strArchiveStart ne $strArchiveStop)
        {
            $iArchiveIdx += 1;
            $iStartMinor += 1;

            if ($iStartMinor == 256)
            {
                $iStartMajor += 1;
                $iStartMinor = 0;
            }
        }
    }
    while !($iStartMajor == $iStopMajor && $iStartMinor == $iStopMinor);    

    return @stryArchive;
}

####################################################################################################################################
# BACKUP_EXPIRE
####################################################################################################################################
sub backup_expire
{
    my $strBackupClusterPath = shift;       # Base path to cluster backup
    my $iFullRetention = shift;             # Number of full backups to keep
    my $iDifferentialRetention = shift;     # Number of differential backups to keep
    my $strArchiveRetentionType = shift;    # Type of backup to base archive retention on
    my $iArchiveRetention = shift;          # Number of backups worth of archive to keep
    
    # Find all the expired full backups
    my $iIndex = $iFullRetention;
    my $strPath;
    my @stryPath = file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), "reverse");

    while (defined($stryPath[$iIndex]))
    {
        &log(INFO, "removed expired full backup: " . $stryPath[$iIndex]);

        # Delete all backups that depend on the full backup.  Done in reverse order so that remaining backups will still
        # be consistent if the process dies
        foreach $strPath (file_list_get(PATH_BACKUP_CLUSTER, undef, "^" . $stryPath[$iIndex] . ".*", "reverse"))
        {
            rmtree("$pg_backrest_file::strBackupClusterPath/$strPath") or confess &log(ERROR, "unable to delete backup ${strPath}");
        }
        
        $iIndex++;
    }
    
    # Find all the expired differential backups
    @stryPath = file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(0, 1, 0), "reverse");
     
    if (defined($stryPath[$iDifferentialRetention]))
    {
        # Get a list of all differential and incremental backups
        foreach $strPath (file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(0, 1, 1), "reverse"))
        {
            # Remove all differential and incremental backups before the oldest valid differential
            if (substr($strPath, 0, length($strPath) - 1) lt $stryPath[$iDifferentialRetention])
            {
                rmtree("$pg_backrest_file::strBackupClusterPath/$strPath") or confess &log(ERROR, "unable to delete backup ${strPath}");
                &log(INFO, "removed expired diff/incr backup ${strPath}");
            }
        }
    }
    
    # Determine which backup type to use for archive retention (full, differential, incremental)
    if ($strArchiveRetentionType eq "full")
    {
        @stryPath = file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), "reverse");
    }
    elsif ($strArchiveRetentionType eq "differential")
    {
        @stryPath = file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 0), "reverse");
    }
    else
    {
        @stryPath = file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 1), "reverse");
    }
    
    # if no backups were found then preserve current archive logs - too scary to delete them!
    my $iBackupTotal = scalar @stryPath;
    
    if ($iBackupTotal == 0)
    {
        return;
    }
    
    # See if enough backups exist for expiration to start
    my $strArchiveRetentionBackup = $stryPath[$iArchiveRetention - 1];
    
    if (!defined($strArchiveRetentionBackup))
    {
        return;
    }

    # Get the archive logs that need to be kept.  To be cautious we will keep all the archive logs starting from this backup
    # even though they are also in the pg_xlog directory (since they have been copied more than once).
    &log(INFO, "archive retention based on backup " . $strArchiveRetentionBackup);

    my %oManifest = backup_manifest_load("${pg_backrest_file::strBackupClusterPath}/$strArchiveRetentionBackup/backup.manifest");
    my $strArchiveLast = ${oManifest}{archive}{archive_location}{start};
    
    if (!defined($strArchiveLast))
    {
        &log(INFO, "invalid archive location retrieved ${$strArchiveRetentionBackup}");
    }
    
    &log(INFO, "archive retention starts at " . $strArchiveLast);

    # Remove any archive directories or files that are out of date
    foreach $strPath (file_list_get(PATH_BACKUP_ARCHIVE, undef, "^[0-F]{16}\$"))
    {
        # If less than first 16 characters of current archive file, then remove the directory
        if ($strPath lt substr($strArchiveLast, 0, 16))
        {
            rmtree($pg_backrest_file::strBackupClusterPath . "/archive/" . $strPath) or confess &log(ERROR, "unable to remove " . $strPath);
            &log(DEBUG, "removed major archive directory " . $strPath);
        }
        # If equals the first 16 characters of the current archive file, then delete individual files instead
        elsif ($strPath eq substr($strArchiveLast, 0, 16))
        {
            my $strSubPath;
        
            # Look for archive files in the archive directory
            foreach $strSubPath (file_list_get(PATH_BACKUP_ARCHIVE, $strPath, "^[0-F]{24}.*\$"))
            {
                # Delete if the first 24 characters less than the current archive file
                if ($strSubPath lt substr($strArchiveLast, 0, 24))
                {
                    unlink("${pg_backrest_file::strBackupClusterPath}/archive/${strPath}/${strSubPath}") or confess &log(ERROR, "unable to remove " . $strSubPath);
                    &log(DEBUG, "removed expired archive file " . $strSubPath);
                }
            }
        }
    }
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

# Run file_init_archive - this is the minimal config needed to run archiving
file_init_backup
(
    config_load(\%oConfig, "command", "manifest"),
    $pg_backrest_file::strCommandPsql = config_load(\%oConfig, "command", "psql"),
    $oConfig{"cluster:$strCluster"}{host}
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
