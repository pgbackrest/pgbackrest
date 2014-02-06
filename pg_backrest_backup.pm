####################################################################################################################################
# BACKUP MODULE
####################################################################################################################################
package pg_backrest_backup;

use strict;
use warnings;
use Carp;
use File::Basename;
use File::Path;
use JSON;
use Scalar::Util qw(looks_like_number);

use lib dirname($0);
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_db;

use Exporter qw(import);

our @EXPORT = qw(backup_init archive_push backup backup_expire archive_list_get);

my $oDb;
my $oFile;
my $strType = "incremental";        # Type of backup: full, differential (diff), incremental (incr)
my $bHardLink;
my $bNoChecksum;

####################################################################################################################################
# BACKUP_INIT
####################################################################################################################################
sub backup_init
{
    my $oDbParam = shift;
    my $oFileParam = shift;
    my $strTypeParam = shift;
    my $bHardLinkParam = shift;
    my $bNoChecksumParam = shift;

    $oDb = $oDbParam;
    $oFile = $oFileParam;
    $strType = $strTypeParam;
    $bHardLink = $bHardLinkParam;
    $bNoChecksum = $bNoChecksumParam;
}

####################################################################################################################################
# ARCHIVE_PUSH
####################################################################################################################################
sub archive_push
{
    my $strSourceFile = shift;

    # Get the destination file
    my $strDestinationFile = basename($strSourceFile);
    
    # Determine if this is an archive file (don't want to do compression or checksum on .backup files)
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}$/;

    # Append the checksum (if requested)
    if ($bArchiveFile && !$bNoChecksum)
    {
        $strDestinationFile .= "-" . $oFile->file_hash_get(PATH_DB_ABSOLUTE, $strSourceFile);
    }
    
    # Copy the archive file
    $oFile->file_copy(PATH_DB_ABSOLUTE, $strSourceFile, PATH_BACKUP_ARCHIVE, $strDestinationFile, !$bArchiveFile);
}

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
        $strDirectory = ($oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 1), "reverse"))[0];
    }

    if (!defined($strDirectory) && $strType ne "full")
    {
        $strDirectory = ($oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), "reverse"))[0];
    }
    
    return $strDirectory;
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
    
    my %oManifestHash = $oFile->manifest_get(PATH_DB_ABSOLUTE, $strDbClusterPath);
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
# BACKUP_FILE - Performs the file level backup
#
# Uses the information in the manifest to determine which files need to be copied.  Directories and tablespace links are only
# created when needed, except in the case of a full backup or if hardlinks are requested.
####################################################################################################################################
sub backup_file
{
    my $strBackupPath = shift;         # Path where the final backup will go (e.g. 20120101F)
    my $strDbClusterPath = shift;      # Database base data path
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
            $strBackupSourcePath = $strDbClusterPath;
            $strBackupDestinationPath = "base";
            $strSectionFile = "base:file";

            # Create the archive log directory
            $oFile->path_create(PATH_BACKUP_TMP, "base/pg_xlog");
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
                $oFile->path_create(PATH_BACKUP_TMP, $strBackupDestinationPath);

                $oFile->link_create(PATH_BACKUP_TMP, ${strBackupDestinationPath},
                                   PATH_BACKUP_TMP, 
                                   "base/pg_tblspc/" . ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{oid},
                                   false, true);
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
                $oFile->path_create(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strPath}",
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

            # If the file has a reference it does not need to be copied since it can be retrieved from the referenced backup.
            # However, if hard-linking is turned on the link will need to be created
            my $strReference = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{reference};

            if (defined($strReference))
            {
                # If hardlinking is turned on then create a hardlink for files that have not changed since the last backup
                if ($bHardLink)
                {
                    &log(DEBUG, "   hard-linking ${strBackupSourceFile} from ${strReference}");

                    $oFile->link_create(PATH_BACKUP_CLUSTER, "${strReference}/${strBackupDestinationPath}/${strFile}",
                                        PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}", true);
                }
            }
            # Else copy/compress the file and generate a checksum
            else
            {
                # Copy the file from db to backup
                &log(DEBUG, "   backing up ${strBackupSourceFile}");
            
                $oFile->file_copy(PATH_DB_ABSOLUTE, $strBackupSourceFile, PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}",
                                  undef, ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{modification_time},
                                  $oFile->{strDefaultFilePermission});

                # Write the hash into the backup manifest (if not suppressed)
                if (!$bNoChecksum)
                {
                    ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{checksum} =
                        $oFile->file_hash_get(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}");
                }
            }
        }
    }
}

####################################################################################################################################
# BACKUP
# 
# Performs the entire database backup.
####################################################################################################################################
sub backup
{
    my $strDbClusterPath = shift;
    
    # Not supporting remote backup hosts yet
    if ($oFile->is_remote(PATH_BACKUP))
    {
        confess &log(ERROR, "remote backup host not currently supported");
    }
    
    if (!defined($strDbClusterPath))
    {
        confess &log(ERROR, "cluster data path is not defined");
    }
    
    &log(DEBUG, "cluster path is $strDbClusterPath");
    
    # Create the cluster backup path
    $oFile->path_create(PATH_BACKUP_CLUSTER);

    # Find the previous backup based on the type
    my $strBackupLastPath = backup_type_find($strType, $oFile->path_get(PATH_BACKUP_CLUSTER));

    my %oLastManifest;

    if (defined($strBackupLastPath))
    {
        %oLastManifest = backup_manifest_load($oFile->path_get(PATH_BACKUP_CLUSTER) . "/$strBackupLastPath/backup.manifest");
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
    my $strBackupTmpPath = $oFile->path_get(PATH_BACKUP_TMP);
    my $strBackupConfFile = $oFile->path_get(PATH_BACKUP_TMP, "backup.manifest");

    # If the backup tmp path already exists, delete the conf file
    if (-e $strBackupTmpPath)
    {
        &log(WARNING, "backup path $strBackupTmpPath already exists");

        # !!! This is temporary until we can clean backup dirs
        system("rm -rf $strBackupTmpPath") == 0 or confess &log(ERROR, "unable to delete ${strBackupTmpPath}");
#        rmtree($strBackupTmpPath) or confess &log(ERROR, "unable to delete ${strBackupTmpPath}");
        $oFile->path_create(PATH_BACKUP_TMP);
        #if (-e $strBackupConfFile)
        #{
        #    unlink $strBackupConfFile or die &log(ERROR, "backup config ${strBackupConfFile} could not be deleted");
        #}
    }
    # Else create the backup tmp path
    else
    {
        &log(INFO, "creating backup path $strBackupTmpPath");
        $oFile->path_create(PATH_BACKUP_TMP);
    }

    # Create a new backup manifest hash
    my %oBackupManifest;

    # Start backup
    ${oBackupManifest}{common}{backup}{label} = $strBackupPath;

    my $strArchiveStart = $oDb->backup_start($strBackupPath);

    ${oBackupManifest}{archive}{archive_location}{start} = $strArchiveStart;

    &log(INFO, 'Backup archive start: ' . $strArchiveStart);

    # Build the backup manifest
    my %oTablespaceMap = $oDb->tablespace_map_get();
    backup_manifest_build($oFile->{strCommandManifest}, $strDbClusterPath, \%oBackupManifest, \%oLastManifest, \%oTablespaceMap);

    # Delete files leftover from a partial backup
    # !!! do it

    # Perform the backup
    backup_file($strBackupPath, $strDbClusterPath, \%oBackupManifest);
           
    # Stop backup
    my $strArchiveStop = $oDb->backup_stop();

    ${oBackupManifest}{archive}{archive_location}{stop} = $strArchiveStop;

    &log(INFO, 'Backup archive stop: ' . $strArchiveStop);

    # After the backup has been stopped, need to make a copy of the archive logs need to make the db consistent
    my @stryArchive = archive_list_get($strArchiveStart, $strArchiveStop, $oDb->version_get() < 9.3);

    foreach my $strArchive (@stryArchive)
    {
        my $strArchivePath = dirname($oFile->path_get(PATH_BACKUP_ARCHIVE, $strArchive));
        my @stryArchiveFile = $oFile->file_list_get(PATH_BACKUP_ABSOLUTE, $strArchivePath, 
            "^${strArchive}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$");
        
        if (scalar @stryArchiveFile != 1)
        {
            confess &log(ERROR, "Zero or more than one file found for glob: ${strArchivePath}"); 
        }

        &log(DEBUG, "    archiving: ${strArchive} (${stryArchiveFile[0]})");

        $oFile->file_copy(PATH_BACKUP_ARCHIVE, $stryArchiveFile[0], PATH_BACKUP_TMP, "base/pg_xlog/${strArchive}");
    }
    
    # Save the backup conf file
    backup_manifest_save($strBackupConfFile, \%oBackupManifest);

    # Rename the backup tmp path to complete the backup
    &log(DEBUG, "    moving ${strBackupTmpPath} to " . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackupPath));
    $oFile->file_move(PATH_BACKUP_TMP, undef, PATH_BACKUP_CLUSTER, $strBackupPath);
}

####################################################################################################################################
# ARCHIVE_LIST_GET
#
# Generates a range of archive log file names given the start and end log file name.  For pre-9.3 databases, use bSkipFF to exclude
# the FF that prior versions did not generate.
####################################################################################################################################
sub archive_list_get
{
    my $strArchiveStart = shift;
    my $strArchiveStop = shift;
    my $bSkipFF = shift;
    
    # strSkipFF default to false
    $bSkipFF = defined($bSkipFF) ? $bSkipFF : false;
    
    if ($bSkipFF)
    {
        &log(DEBUG, "    archive_list_get: pre-9.3 database, skipping log FF");
    }
    else
    {
        &log(DEBUG, "    archive_list_get: post-9.3 database, including log FF");
    }
    
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

    $stryArchive[$iArchiveIdx] = uc(sprintf("${strTimeline}%08x%08x", $iStartMajor, $iStartMinor));
    $iArchiveIdx += 1;

    while (!($iStartMajor == $iStopMajor && $iStartMinor == $iStopMinor))
    {
        if ($strArchiveStart ne $strArchiveStop)
        {
            $iArchiveIdx += 1;
            $iStartMinor += 1;

            if ($bSkipFF && $iStartMinor == 255 || !$bSkipFF && $iStartMinor == 256)
            {
                $iStartMajor += 1;
                $iStartMinor = 0;
            }
        }

        $stryArchive[$iArchiveIdx] = uc(sprintf("${strTimeline}%08x%08x", $iStartMajor, $iStartMinor));
    }

    &log(DEBUG, "    archive_list_get: $strArchiveStart-$strArchiveStop (@stryArchive)");

    return @stryArchive;
}

####################################################################################################################################
# BACKUP_EXPIRE
# 
# Removes expired backups and archive logs from the backup directory.  Partial backups are not counted for expiration, so if full
# or differential retention is set to 2, there must be three complete backups before the oldest one can be deleted.
#
# iFullRetention - Optional, must be greater than 0 when supplied.
# iDifferentialRetention - Optional, must be greater than 0 when supplied.
# strArchiveRetention - Optional, must be (full,differential/diff,incremental/incr) when supplied
# iArchiveRetention - Required when strArchiveRetention is supplied.  Must be greater than 0.
####################################################################################################################################
sub backup_expire
{
    my $strBackupClusterPath = shift;       # Base path to cluster backup
    my $iFullRetention = shift;             # Number of full backups to keep
    my $iDifferentialRetention = shift;     # Number of differential backups to keep
    my $strArchiveRetentionType = shift;    # Type of backup to base archive retention on
    my $iArchiveRetention = shift;          # Number of backups worth of archive to keep
    
    my $strPath;
    my @stryPath;
    
    # Find all the expired full backups
    if (defined($iFullRetention))
    {
        # Make sure iFullRetention is valid
        if (!looks_like_number($iFullRetention) || $iFullRetention < 1)
        {
            confess &log(ERROR, "full_rentention must be a number >= 1");
        }

        my $iIndex = $iFullRetention;
        @stryPath = $oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), "reverse");

        while (defined($stryPath[$iIndex]))
        {
            &log(INFO, "removed expired full backup: " . $stryPath[$iIndex]);

            # Delete all backups that depend on the full backup.  Done in reverse order so that remaining backups will still
            # be consistent if the process dies
            foreach $strPath ($oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, "^" . $stryPath[$iIndex] . ".*", "reverse"))
            {
                system("rm -rf ${strBackupClusterPath}/${strPath}") == 0 or confess &log(ERROR, "unable to delete backup ${strPath}");
            }
        
            $iIndex++;
        }
    }
    
    # Find all the expired differential backups
    if (defined($iDifferentialRetention))
    {
        # Make sure iDifferentialRetention is valid
        if (!looks_like_number($iDifferentialRetention) || $iDifferentialRetention < 1)
        {
            confess &log(ERROR, "differential_rentention must be a number >= 1");
        }
        
        @stryPath = $oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(0, 1, 0), "reverse");
     
        if (defined($stryPath[$iDifferentialRetention]))
        {
            # Get a list of all differential and incremental backups
            foreach $strPath ($oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(0, 1, 1), "reverse"))
            {
                # Remove all differential and incremental backups before the oldest valid differential
                if (substr($strPath, 0, length($strPath) - 1) lt $stryPath[$iDifferentialRetention])
                {
                    system("rm -rf ${strBackupClusterPath}/${strPath}") == 0 or confess &log(ERROR, "unable to delete backup ${strPath}");
                    &log(INFO, "removed expired diff/incr backup ${strPath}");
                }
            }
        }
    }
    
    # If no archive retention type is set then exit
    if (!defined($strArchiveRetentionType))
    {
        &log(INFO, "archive rentention type not set - archive logs will not be expired");
        return;
    }

    # Determine which backup type to use for archive retention (full, differential, incremental)
    if ($strArchiveRetentionType eq "full")
    {
        @stryPath = $oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), "reverse");
    }
    elsif ($strArchiveRetentionType eq "differential" || $strArchiveRetentionType eq "diff")
    {
        @stryPath = $oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 0), "reverse");
    }
    elsif ($strArchiveRetentionType eq "incremental" || $strArchiveRetentionType eq "incr")
    {
        @stryPath = $oFile->file_list_get(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 1), "reverse");
    }
    else
    {
        confess &log(ERROR, "unknown archive_retention_type '${strArchiveRetentionType}'");
    }

    # Make sure that iArchiveRetention is set and valid
    if (!defined($iArchiveRetention))
    {
        confess &log(ERROR, "archive_rentention must be set if archive_retention_type is set");
        return;
    }

    if (!looks_like_number($iArchiveRetention) || $iArchiveRetention < 1)
    {
        confess &log(ERROR, "archive_rentention must be a number >= 1");
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

    my %oManifest = backup_manifest_load($oFile->path_get(PATH_BACKUP_CLUSTER) . "/$strArchiveRetentionBackup/backup.manifest");
    my $strArchiveLast = ${oManifest}{archive}{archive_location}{start};
    
    if (!defined($strArchiveLast))
    {
        &log(INFO, "invalid archive location retrieved ${$strArchiveRetentionBackup}");
    }
    
    &log(INFO, "archive retention starts at " . $strArchiveLast);

    # Remove any archive directories or files that are out of date
    foreach $strPath ($oFile->file_list_get(PATH_BACKUP_ARCHIVE, undef, "^[0-F]{16}\$"))
    {
        # If less than first 16 characters of current archive file, then remove the directory
        if ($strPath lt substr($strArchiveLast, 0, 16))
        {
            system("rm -rf " . $oFile->{strBackupClusterPath} . "/archive/" . $strPath) == 0
                or confess &log(ERROR, "unable to remove " . $strPath);
            &log(DEBUG, "removed major archive directory " . $strPath);
        }
        # If equals the first 16 characters of the current archive file, then delete individual files instead
        elsif ($strPath eq substr($strArchiveLast, 0, 16))
        {
            my $strSubPath;
        
            # Look for archive files in the archive directory
            foreach $strSubPath ($oFile->file_list_get(PATH_BACKUP_ARCHIVE, $strPath, "^[0-F]{24}.*\$"))
            {
                # Delete if the first 24 characters less than the current archive file
                if ($strSubPath lt substr($strArchiveLast, 0, 24))
                {
                    unlink($oFile->path_get(PATH_BACKUP_ARCHIVE, $strSubPath)) or confess &log(ERROR, "unable to remove " . $strSubPath);
                    &log(DEBUG, "removed expired archive file " . $strSubPath);
                }
            }
        }
    }
}

1;