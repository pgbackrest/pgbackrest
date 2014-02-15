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
use threads;
use Thread::Queue;

use threads ('yield',
             'stack_size' => 64 * 4096,
             'exit' => 'threads_only',
             'stringify');

use lib dirname($0);
use pg_backrest_utility;
use pg_backrest_file;
use pg_backrest_db;

use Exporter qw(import);

our @EXPORT = qw(backup_init archive_push archive_pull backup backup_expire archive_list_get);

my $oDb;
my $oFile;
my $strType = "incremental";        # Type of backup: full, differential (diff), incremental (incr)
my $bHardLink;
my $bNoChecksum;
my $iThreadMax;
my $iThreadThreshold = 10;
my $iSmallFileThreshold = 65536;
my $bArchiveRequired;

# Thread variables
my @oThreadQueue;
my %oFileCopyMap;
my @oThreadFile;

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
    my $iThreadMaxParam = shift;
    my $bArchiveRequiredParam = shift;

    $oDb = $oDbParam;
    $oFile = $oFileParam;
    $strType = $strTypeParam;
    $bHardLink = $bHardLinkParam;
    $bNoChecksum = $bNoChecksumParam;
    $iThreadMax = $iThreadMaxParam;
    $bArchiveRequired = $bArchiveRequiredParam;
    
    if (!defined($iThreadMax))
    {
        $iThreadMax = 1;
    }
    
    if ($iThreadMax < 1 || $iThreadMax > 32)
    {
        confess &log(ERROR, "thread_max must be between 1 and 32");
    }
}

####################################################################################################################################
# THREAD_INIT
####################################################################################################################################
sub thread_init
{
    my $iThreadRequestTotal = shift;  # Number of threads that were requested
    
    my $iThreadActualTotal;           # Number of actual threads assigned
    
    if (!defined($iThreadRequestTotal))
    {
        $iThreadActualTotal = $iThreadMax;
    }
    else
    {
        $iThreadActualTotal = $iThreadRequestTotal < $iThreadMax ? $iThreadRequestTotal : $iThreadMax;
        
        if ($iThreadActualTotal < 1)
        {
            $iThreadActualTotal = 1;
        }
    }
    
    for (my $iThreadIdx = 0; $iThreadIdx < $iThreadActualTotal; $iThreadIdx++)
    {
        if (!defined($oThreadQueue[$iThreadIdx]))
        {
            $oThreadQueue[$iThreadIdx] = Thread::Queue->new();
        }

        if (!defined($oThreadFile[$iThreadIdx]))
        {
            $oThreadFile[$iThreadIdx] = $oFile->clone($iThreadIdx);
        }
    }
    
    return $iThreadActualTotal;
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
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}$/ ? true : false;

    # Append the checksum (if requested)
    if ($bArchiveFile && !$bNoChecksum)
    {
        $strDestinationFile .= "-" . $oFile->file_hash_get(PATH_DB_ABSOLUTE, $strSourceFile);
    }

    # Copy the archive file
    $oFile->file_copy(PATH_DB_ABSOLUTE, $strSourceFile, PATH_BACKUP_ARCHIVE, $strDestinationFile,
                      $bArchiveFile ? undef : true);
}

####################################################################################################################################
# ARCHIVE_PULL
####################################################################################################################################
sub archive_pull
{
    my $strArchivePath = shift;
    my $strCompressAsync = shift;

    # Load the archive manifest - all the files that need to be pushed
    my %oManifestHash = $oFile->manifest_get(PATH_DB_ABSOLUTE, $strArchivePath);

    # Get all the files to be transferred and calculate the total size
    my @stryFile;
    my $lFileSize = 0;
    my $lFileTotal = 0;

    foreach my $strFile (sort(keys $oManifestHash{name}))
    {
        if ($strFile =~ /^[0-F]{16}\/[0-F]{24}.*/)
        {
            push @stryFile, $strFile;

            $lFileSize += $oManifestHash{name}{"$strFile"}{size};
            $lFileTotal++;
        }
    }

    if ($lFileTotal == 0)
    {
        &log(DEBUG, "no archive logs to be copied to backup");
        
        return 0;
    }

    # Output files to be moved to backup
    &log(INFO, "archive to be copied to backup total ${lFileTotal}, size " . file_size_format($lFileSize));
    
    # Init the thread variables
    my $iThreadLocalMax = thread_init(int($lFileTotal / $iThreadThreshold) + 1);
    my @oThread;
    my $iThreadIdx = 0;

    &log(DEBUG, "actual threads ${iThreadLocalMax}/${iThreadMax}");

    foreach my $strFile (sort @stryFile)
    {
        $oThreadQueue[$iThreadIdx]->enqueue($strFile);

        $iThreadIdx = ($iThreadIdx + 1 == $iThreadLocalMax) ? 0 : $iThreadIdx + 1;
    }

    # End each thread queue and start the thread
    for ($iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        $oThreadQueue[$iThreadIdx]->enqueue(undef);
        $oThread[$iThreadIdx] = threads->create(\&archive_pull_copy_thread, $iThreadIdx, $strArchivePath);
    }
    
    # Rejoin the threads
    for ($iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        $oThread[$iThreadIdx]->join();
    }

    # If there are errors then compress
    # If compress async then go and compress all uncompressed archive files
#    if ($bCompressAsync)
#    {
#        # Find all the archive files
#        foreach my $strFile (@stryFile)
#        {
#            &log(DEBUG, "SHOULD BE LOGGING ${strFile}");
#        }
#    }


    
    return $lFileTotal;
}

sub archive_pull_copy_thread
{
    my @args = @_;

    my $iThreadIdx = $args[0];
    my $strArchivePath = $args[1];

    while (my $strFile = $oThreadQueue[$iThreadIdx]->dequeue())
    {
        &log(INFO, "thread ${iThreadIdx} backing up archive file ${strFile}");
        
        my $strArchiveFile = "${strArchivePath}/${strFile}";

        # Copy the file
        $oThreadFile[$iThreadIdx]->file_copy(PATH_DB_ABSOLUTE, $strArchiveFile,
                                             PATH_BACKUP_ARCHIVE, basename($strFile),
                                             undef, undef,
                                             undef); # cannot set permissions remotely yet $oFile->{strDefaultFilePermission});
                                             
        #  Remove the source archive file
        unlink($strArchiveFile) or confess &log(ERROR, "unable to remove ${strArchiveFile}");
    }
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

    foreach my $strSection (keys %oBackupManifestFile)
    {
        foreach my $strKey (keys ${oBackupManifestFile}{"${strSection}"})
        {
            my $strValue = ${oBackupManifestFile}{"${strSection}"}{"$strKey"};
            
            if ($strValue =~ /^\{.*\}$/)
            {
                $oBackupManifest{"${strSection}"}{"$strKey"} = decode_json($strValue);
            }
            else
            {
#                &log(ERROR, "scalar key ${strKey}, value ${strValue}");

                $oBackupManifest{"${strSection}"}{"$strKey"} = $strValue;
            }
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

    foreach my $strSection (sort (keys $oBackupManifestRef))
    {
        foreach my $strKey (sort (keys ${$oBackupManifestRef}{"${strSection}"}))
        {
            my $strValue = ${$oBackupManifestRef}{"${strSection}"}{"$strKey"};
            
            if (ref($strValue) eq "HASH")
            {
                $oBackupManifest{"${strSection}"}{"$strKey"} = encode_json($strValue);
            }
            else
            {
                $oBackupManifest{"${strSection}"}{"$strKey"} = $strValue;
            }
        }
    }
    
    tied(%oBackupManifest)->WriteConfig($strBackupManifestFile);
}

####################################################################################################################################
# BACKUP_FILE_NOT_IN_MANIFEST - Find all files in a backup path that are not in the supplied manifest
####################################################################################################################################
sub backup_file_not_in_manifest
{
    my $strPathType = shift;
    my $oManifestRef = shift;
    
    my %oFileHash = $oFile->manifest_get($strPathType);
    my @stryFile;
    my $iFileTotal = 0;

    foreach my $strName (sort(keys $oFileHash{name}))
    {
        # Ignore certain files that will never be in the manifest
        if ($strName eq "backup.manifest" ||
            $strName eq ".")
        {
            next;
        }

        # Get the base directory
        my $strBasePath = (split("/", $strName))[0];

        if ($strBasePath eq $strName)
        {
            my $strSection = $strBasePath eq "tablespace" ? "base:tablespace" : "${strBasePath}:path";

            if (defined(${$oManifestRef}{"${strSection}"}))
            {
                next;
            }
        }
        else
        {
            my $strPath = substr($strName, length($strBasePath) + 1);

            # Create the section from the base path
            my $strSection = $strBasePath;

            if ($strSection eq "tablespace")
            {
                my $strTablespace = (split("/", $strPath))[0];

                $strSection = $strSection . ":" . $strTablespace;

                if ($strTablespace eq $strPath)
                {
                    if (defined(${$oManifestRef}{"${strSection}:path"}))
                    {
                        next;
                    }
                }

                $strPath = substr($strPath, length($strTablespace) + 1);
            }

            my $cType = $oFileHash{name}{"${strName}"}{type};

            if ($cType eq "d")
            {
                if (defined(${$oManifestRef}{"${strSection}:path"}{"${strPath}"}))
                {
                    next;
                }
            }
            else
            {
                if (defined(${$oManifestRef}{"${strSection}:file"}{"${strPath}"}))
                {
                    if (${$oManifestRef}{"${strSection}:file"}{"${strPath}"}{size} ==
                            $oFileHash{name}{"${strName}"}{size} &&
                        ${$oManifestRef}{"${strSection}:file"}{"${strPath}"}{modification_time} == 
                            $oFileHash{name}{"${strName}"}{modification_time})
                    {
                        ${$oManifestRef}{"${strSection}:file"}{"${strPath}"}{exists} = true;
                        next;
                    }
                }
            }
        }

        $stryFile[$iFileTotal] = $strName;
        $iFileTotal++;
    }

    return @stryFile;
}

####################################################################################################################################
# BACKUP_TMP_CLEAN
# 
# Cleans the temp directory from a previous failed backup so it can be reused
####################################################################################################################################
sub backup_tmp_clean
{
    my $oManifestRef = shift;
    
    &log(INFO, "cleaning backup tmp path");

    # Remove the pg_xlog directory since it contains nothing useful for the new backup
    if (-e $oFile->path_get(PATH_BACKUP_TMP, "base/pg_xlog"))
    {
        rmtree($oFile->path_get(PATH_BACKUP_TMP, "base/pg_xlog")) or confess &log(ERROR, "unable to delete tmp pg_xlog path");
    }

    # Remove the pg_tblspc directory since it is trivial to rebuild, but hard to compare
    if (-e $oFile->path_get(PATH_BACKUP_TMP, "base/pg_tblspc"))
    {
        rmtree($oFile->path_get(PATH_BACKUP_TMP, "base/pg_tblspc")) or confess &log(ERROR, "unable to delete tmp pg_tblspc path");
    }

    # Get the list of files that should be deleted from temp
    my @stryFile = backup_file_not_in_manifest(PATH_BACKUP_TMP, $oManifestRef);

    foreach my $strFile (sort {$b cmp $a} @stryFile)
    {
        my $strDelete = $oFile->path_get(PATH_BACKUP_TMP, $strFile);
        
        # If a path then delete it, all the files should have already been deleted since we are going in reverse order
        if (-d $strDelete)
        {
            &log(DEBUG, "remove path ${strDelete}");
            rmdir($strDelete) or confess &log(ERROR, "unable to delete path ${strDelete}, is it empty?");
        }
        # Else delete a file
        else
        {
            &log(DEBUG, "remove file ${strDelete}");
            unlink($strDelete) or confess &log(ERROR, "unable to delete file ${strDelete}");
        }
    }
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

        if ($cType eq "f")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} = $oManifestHash{name}{"${strName}"}{size};
        }

        if ($cType eq "f" || $cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{inode} = $oManifestHash{name}{"${strName}"}{inode};

            if (defined(${$oLastManifestRef}{"${strSection}"}{"$strName"}))
            {
                my $bSizeMatch = true;

                if ($cType eq "f")
                {
                    ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} = $oManifestHash{name}{"${strName}"}{size};

                    $bSizeMatch = ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{size} ==
                                  ${$oLastManifestRef}{"${strSection}"}{"$strName"}{size};
                }

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
                            ${$oLastManifestRef}{backup}{label};
                    }

                    my $strReference = ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{reference};

                    if (!defined(${$oBackupManifestRef}{backup}{reference}))
                    {
                        ${$oBackupManifestRef}{backup}{reference} = $strReference;
                    }
                    else
                    {
                        if (${$oBackupManifestRef}{backup}{reference} !~ /$strReference/)
                        {
                            ${$oBackupManifestRef}{backup}{reference} .= ",$strReference";
                        }
                    }
                }
            }
        }

        if ($cType eq "l")
        {
            ${$oBackupManifestRef}{"${strSection}"}{"$strName"}{link_destination} =
                $oManifestHash{name}{"${strName}"}{link_destination};

            if (index($strName, 'pg_tblspc/') == 0 && $strLevel eq "base")
            {
                my $strTablespaceOid = basename($strName);
                my $strTablespaceName = ${$oTablespaceMapRef}{oid}{"$strTablespaceOid"}{name};

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

    # Variables used for parallel copy
    my $lTablespaceIdx = 0;
    my $lFileTotal = 0;
    my $lFileLargeSize = 0;
    my $lFileLargeTotal = 0;
    my $lFileSmallSize = 0;
    my $lFileSmallTotal = 0;

    # Decide if all the paths will be created in advance
    my $bPathCreate = $bHardLink || $strType eq "full";

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
            $lTablespaceIdx++;
            $strBackupSourcePath = $strDbClusterPath;
            $strBackupDestinationPath = "base";
            $strSectionFile = "base:file";

            # Create the archive log directory
            $oFile->path_create(PATH_BACKUP_TMP, "base/pg_xlog");
        }
        # Process each tablespace
        elsif ($strSectionPath =~ /^tablespace\:/)
        {
            $lTablespaceIdx++;
            my $strTablespaceName = (split(":", $strSectionPath))[1];
            $strBackupSourcePath = ${$oBackupManifestRef}{"base:tablespace"}{"${strTablespaceName}"}{path};
            $strBackupDestinationPath = "tablespace/${strTablespaceName}";
            $strSectionFile = "tablespace:${strTablespaceName}:file";

            # Create the tablespace directory and link
            if ($bPathCreate)
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
        if ($bPathCreate)
        {
            my $strPath;

            foreach $strPath (sort(keys ${$oBackupManifestRef}{"${strSectionPath}"}))
            {
                if (defined(${$oBackupManifestRef}{"${strSectionPath}"}{"$strPath"}{exists}))
                {
                    &log(TRACE, "path ${strPath} already exists from previous backup attempt");
                    ${$oBackupManifestRef}{"${strSectionPath}"}{"$strPath"}{exists} = undef;
                }
                else
                {
                    $oFile->path_create(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strPath}",
                                        ${$oBackupManifestRef}{"${strSectionPath}"}{"$strPath"}{permission});
                }
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
            
            if (defined(${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{exists}))
            {
                &log(TRACE, "file ${strFile} already exists from previous backup attempt");
                ${$oBackupManifestRef}{"${strSectionPath}"}{"$strFile"}{exists} = undef;
            }
            else
            {
                # If the file has a reference it does not need to be copied since it can be retrieved from the referenced backup.
                # However, if hard-linking is turned on the link will need to be created
                my $strReference = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{reference};

                if (defined($strReference))
                {
                    # If hardlinking is turned on then create a hardlink for files that have not changed since the last backup
                    if ($bHardLink)
                    {
                        &log(DEBUG, "hard-linking ${strBackupSourceFile} from ${strReference}");

                        $oFile->link_create(PATH_BACKUP_CLUSTER, "${strReference}/${strBackupDestinationPath}/${strFile}",
                                            PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}", true, false, !$bPathCreate);
                    }
                }
                # Else copy/compress the file and generate a checksum
                else
                {
                    my $lFileSize = ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{size};

                    # Setup variables needed for threaded copy
                    $lFileTotal++;
                    $lFileLargeSize += $lFileSize > $iSmallFileThreshold ? $lFileSize : 0;
                    $lFileLargeTotal += $lFileSize > $iSmallFileThreshold ? 1 : 0;
                    $lFileSmallSize += $lFileSize <= $iSmallFileThreshold ? $lFileSize : 0; 
                    $lFileSmallTotal += $lFileSize <= $iSmallFileThreshold ? 1 : 0; 

                    # Load the hash used by threaded copy
                    my $strKey = sprintf("ts%012x-fs%012x-fn%012x", $lTablespaceIdx,
                                         $lFileSize, $lFileTotal);

                    $oFileCopyMap{"${strKey}"}{db_file} = $strBackupSourceFile;
                    $oFileCopyMap{"${strKey}"}{backup_file} = "${strBackupDestinationPath}/${strFile}";
                    $oFileCopyMap{"${strKey}"}{size} = $lFileSize;
                    $oFileCopyMap{"${strKey}"}{modification_time} = 
                        ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{modification_time};
                }
            }
        }
    }

    # Build the thread queues
    my $iThreadLocalMax = thread_init(int($lFileTotal / $iThreadThreshold) + 1);
    &log(DEBUG, "actual threads ${iThreadLocalMax}/${iThreadMax}");

    # Initialize the thread size array
    my @oyThreadData;

    for (my $iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        $oyThreadData[$iThreadIdx]{size} = 0;
        $oyThreadData[$iThreadIdx]{total} = 0;
        $oyThreadData[$iThreadIdx]{large_size} = 0;
        $oyThreadData[$iThreadIdx]{large_total} = 0;
        $oyThreadData[$iThreadIdx]{small_size} = 0;
        $oyThreadData[$iThreadIdx]{small_total} = 0;
    }
    
    # Assign files to each thread queue
    my $iThreadFileSmallIdx = 0;
    my $iThreadFileSmallTotalMax = int($lFileSmallTotal / $iThreadLocalMax);

    my $iThreadFileLargeIdx = 0;
    my $fThreadFileLargeSizeMax = $lFileLargeSize / $iThreadLocalMax;

    &log(INFO, "file total ${lFileTotal}");
    &log(INFO, "file small total ${lFileSmallTotal}, small size: " . file_size_format($lFileSmallSize) .
         ", small thread avg total " . file_size_format(int($iThreadFileSmallTotalMax)));
    &log(INFO, "file large total ${lFileLargeTotal}, large size: " . file_size_format($lFileLargeSize) .
         ", large thread avg size " . file_size_format(int($fThreadFileLargeSizeMax)));

    foreach my $strFile (sort (keys %oFileCopyMap))
    {
        my $lFileSize = $oFileCopyMap{"${strFile}"}{size};

        if ($lFileSize > $iSmallFileThreshold)
        {
            $oThreadQueue[$iThreadFileLargeIdx]->enqueue($strFile);

            $oyThreadData[$iThreadFileLargeIdx]{large_size} += $lFileSize;
            $oyThreadData[$iThreadFileLargeIdx]{large_total}++;
            $oyThreadData[$iThreadFileLargeIdx]{size} += $lFileSize;

            if ($oyThreadData[$iThreadFileLargeIdx]{large_size} >= $fThreadFileLargeSizeMax &&
                $iThreadFileLargeIdx < $iThreadLocalMax - 1)
            {
                $iThreadFileLargeIdx++;
            }
        }
        else
        {
            $oThreadQueue[$iThreadFileSmallIdx]->enqueue($strFile);
            
            $oyThreadData[$iThreadFileSmallIdx]{small_size} += $lFileSize;
            $oyThreadData[$iThreadFileSmallIdx]{small_total}++;
            $oyThreadData[$iThreadFileSmallIdx]{size} += $lFileSize;

            if ($oyThreadData[$iThreadFileSmallIdx]{small_total} >= $iThreadFileSmallTotalMax &&
                $iThreadFileSmallIdx < $iThreadLocalMax - 1)
            {
                $iThreadFileSmallIdx++;
            }
        }
    }

    # End each thread queue and start the thread
    my @oThread;

    for (my $iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        &log(INFO, "thread ${iThreadIdx} large total $oyThreadData[$iThreadIdx]{large_total}, " . 
                   "size $oyThreadData[$iThreadIdx]{large_size}");
        &log(INFO, "thread ${iThreadIdx} small total $oyThreadData[$iThreadIdx]{small_total}, " . 
                   "size $oyThreadData[$iThreadIdx]{small_size}");

        $oThreadQueue[$iThreadIdx]->enqueue(undef);
        $oThread[$iThreadIdx] = threads->create(\&backup_file_thread, $iThreadIdx, $bNoChecksum, !$bPathCreate,
                                                $oyThreadData[$iThreadIdx]{size});
    }

    # Rejoin the threads
    for (my $iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        $oThread[$iThreadIdx]->join();
    }

    # Look for errors
    for (my $iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        if (defined($oThread[$iThreadIdx]->error()))
        {
            confess &log(ERROR, "error in thread ${iThreadIdx}: check log for details");
        }
    }
}

sub backup_file_thread
{
    my @args = @_;

    my $iThreadIdx = $args[0];
    my $bNoChecksum = $args[1];
    my $bPathCreate = $args[2];
    my $lSizeTotal = $args[3];
    
    my $lSize = 0;
    
    while (my $strFile = $oThreadQueue[$iThreadIdx]->dequeue())
    {
        &log(INFO, "thread ${iThreadIdx} backing up file $oFileCopyMap{$strFile}{db_file} (" . 
                   file_size_format($oFileCopyMap{$strFile}{size}) .
                   ($lSizeTotal > 0 ? ", " . int($lSize * 100 / $lSizeTotal) . "%" : "") . ")");

        $oThreadFile[$iThreadIdx]->file_copy(PATH_DB_ABSOLUTE, $oFileCopyMap{$strFile}{db_file},
                                             PATH_BACKUP_TMP, $oFileCopyMap{$strFile}{backup_file},
                                             undef, $oFileCopyMap{$strFile}{modification_time},
                                             undef, $bPathCreate);

        $lSize += $oFileCopyMap{$strFile}{size};

        #                # Write the hash into the backup manifest (if not suppressed)
        #                if (!$bNoChecksum)
        #                {
        #                    ${$oBackupManifestRef}{"${strSectionFile}"}{"$strFile"}{checksum} =
        #                        $oFile->file_hash_get(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strFile}");
        #                }
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
        
        if (!defined($oLastManifest{backup}{label}))
        {
            confess &log(ERROR, "unable to find label in backup $strBackupLastPath");
        }
        
        &log(INFO, "last backup label: $oLastManifest{backup}{label}");
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

    &log(INFO, "new backup label: ${strBackupPath}");

    # Build backup tmp and config
    my $strBackupTmpPath = $oFile->path_get(PATH_BACKUP_TMP);
    my $strBackupConfFile = $oFile->path_get(PATH_BACKUP_TMP, "backup.manifest");

    # Start backup
    my %oBackupManifest;
    ${oBackupManifest}{backup}{label} = $strBackupPath;

    my $strArchiveStart = $oDb->backup_start($strBackupPath);
    ${oBackupManifest}{backup}{archive_start} = $strArchiveStart;

    &log(INFO, 'archive start: ' . ${oBackupManifest}{backup}{archive_start});

    # Build the backup manifest
    my %oTablespaceMap = $oDb->tablespace_map_get();

    backup_manifest_build($oFile->{strCommandManifest}, $strDbClusterPath, \%oBackupManifest, \%oLastManifest, \%oTablespaceMap);

    # If the backup tmp path already exists, remove invalid files
    if (-e $strBackupTmpPath)
    {
        &log(WARN, "aborted backup already exists, will be cleaned to remove invalid files and resumed");
        
        # Clean the old backup tmp path
        backup_tmp_clean(\%oBackupManifest);
    }
    # Else create the backup tmp path
    else
    {
        &log(DEBUG, "creating backup path $strBackupTmpPath");
        $oFile->path_create(PATH_BACKUP_TMP);
    }

    # Save the backup conf file first time - so we can see what is happening in the backup
    backup_manifest_save($strBackupConfFile, \%oBackupManifest);

    # Perform the backup
    backup_file($strBackupPath, $strDbClusterPath, \%oBackupManifest);
           
    # Stop backup
    my $strArchiveStop = $oDb->backup_stop();

    ${oBackupManifest}{backup}{archive_stop} = $strArchiveStop;
    &log(INFO, 'archive stop: ' . ${oBackupManifest}{backup}{archive_stop});

    # If archive logs are required to complete the backup, then fetch them.  This is the default, but can be overridden if the 
    # archive logs are going to a different server.  Be careful here because there is no way to verify that the backup will be
    # consistent - at least not in this routine.  
    if ($bArchiveRequired)
    {
        # Save the backup conf file second time - before getting archive logs in case that fails
        backup_manifest_save($strBackupConfFile, \%oBackupManifest);

        # After the backup has been stopped, need to make a copy of the archive logs need to make the db consistent
        my @stryArchive = archive_list_get($strArchiveStart, $strArchiveStop, $oDb->version_get() < 9.3);

        foreach my $strArchive (@stryArchive)
        {
            my $strArchivePath = dirname($oFile->path_get(PATH_BACKUP_ARCHIVE, $strArchive));

            wait_for_file($strArchivePath, "^${strArchive}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$", 600);

            my @stryArchiveFile = $oFile->file_list_get(PATH_BACKUP_ABSOLUTE, $strArchivePath, 
                "^${strArchive}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$");

            if (scalar @stryArchiveFile != 1)
            {
                confess &log(ERROR, "Zero or more than one file found for glob: ${strArchivePath}"); 
            }

            &log(DEBUG, "archiving: ${strArchive} (${stryArchiveFile[0]})");

            $oFile->file_copy(PATH_BACKUP_ARCHIVE, $stryArchiveFile[0], PATH_BACKUP_TMP, "base/pg_xlog/${strArchive}");
        }
    }

    # Need some sort of backup validate - create a manifest and compare the backup manifest
    # !!! DO IT
    
    # Save the backup conf file final time
    backup_manifest_save($strBackupConfFile, \%oBackupManifest);

    # Rename the backup tmp path to complete the backup
    &log(DEBUG, "moving ${strBackupTmpPath} to " . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackupPath));
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
        &log(TRACE, "archive_list_get: pre-9.3 database, skipping log FF");
    }
    else
    {
        &log(TRACE, "archive_list_get: post-9.3 database, including log FF");
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

    &log(TRACE, "    archive_list_get: $strArchiveStart-$strArchiveStop (@stryArchive)");

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