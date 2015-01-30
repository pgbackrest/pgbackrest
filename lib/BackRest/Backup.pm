####################################################################################################################################
# BACKUP MODULE
####################################################################################################################################
package BackRest::Backup;

use threads;
use strict;
use warnings;
use Carp;

use File::Basename;
use File::Path qw(remove_tree);
use Scalar::Util qw(looks_like_number);
use Thread::Queue;

use lib dirname($0);
use BackRest::Utility;
use BackRest::Exception;
use BackRest::Config;
use BackRest::Manifest;
use BackRest::File;
use BackRest::Db;

use Exporter qw(import);

our @EXPORT = qw(backup_init backup_thread_kill archive_push archive_xfer archive_get archive_compress
                 backup backup_expire archive_list_get);

my $oDb;
my $oFile;
my $strType;        # Type of backup: full, differential (diff), incremental (incr)
my $bCompress;
my $bHardLink;
my $bNoChecksum;
my $iThreadMax;
my $iThreadLocalMax;
my $iThreadThreshold = 10;
my $iSmallFileThreshold = 65536;
my $bArchiveRequired;
my $bNoStartStop;
my $bForce;
my $iThreadTimeout;

# Thread variables
my @oThread;
my @oThreadQueue;
my @oMasterQueue;
my %oFileCopyMap;

####################################################################################################################################
# BACKUP_INIT
####################################################################################################################################
sub backup_init
{
    my $oDbParam = shift;
    my $oFileParam = shift;
    my $strTypeParam = shift;
    my $bCompressParam = shift;
    my $bHardLinkParam = shift;
    my $bNoChecksumParam = shift;
    my $iThreadMaxParam = shift;
    my $bArchiveRequiredParam = shift;
    my $iThreadTimeoutParam = shift;
    my $bNoStartStopParam = shift;
    my $bForceParam = shift;

    $oDb = $oDbParam;
    $oFile = $oFileParam;
    $strType = $strTypeParam;
    $bCompress = $bCompressParam;
    $bHardLink = $bHardLinkParam;
    $bNoChecksum = $bNoChecksumParam;
    $bArchiveRequired = $bArchiveRequiredParam;
    $iThreadMax = $iThreadMaxParam;
    $iThreadTimeout = $iThreadTimeoutParam;
    $bNoStartStop = $bNoStartStopParam;
    $bForce = $bForceParam;

    # If no-start-stop is specified then archive-required must be false
    if ($bNoStartStop)
    {
        $bArchiveRequired = false;
    }

    if (!defined($iThreadMax))
    {
        $iThreadMax = 1;
    }

    if ($iThreadMax < 1 || $iThreadMax > 32)
    {
        confess &log(ERROR, 'thread_max must be between 1 and 32');
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
        $oThreadQueue[$iThreadIdx] = Thread::Queue->new();
        $oMasterQueue[$iThreadIdx] = Thread::Queue->new();
    }

    return $iThreadActualTotal;
}

####################################################################################################################################
# BACKUP_THREAD_KILL
####################################################################################################################################
sub backup_thread_kill
{
    my $iTotal = 0;

    for (my $iThreadIdx = 0; $iThreadIdx < scalar @oThread; $iThreadIdx++)
    {
        if (defined($oThread[$iThreadIdx]))
        {
            if ($oThread[$iThreadIdx]->is_running())
            {
                $oThread[$iThreadIdx]->kill('KILL')->join();
            }
            elsif ($oThread[$iThreadIdx]->is_joinable())
            {
                $oThread[$iThreadIdx]->join();
            }

            undef($oThread[$iThreadIdx]);
            $iTotal++;
        }
    }

    return($iTotal);
}

####################################################################################################################################
# BACKUP_THREAD_COMPLETE
####################################################################################################################################
sub backup_thread_complete
{
    my $iTimeout = shift;
    my $bConfessOnError = shift;

    if (!defined($bConfessOnError))
    {
        $bConfessOnError = true;
    }

#    if (!defined($iTimeout))
#    {
#        &log(WARN, "no thread timeout was set");
#    }

    # Wait for all threads to complete and handle errors
    my $iThreadComplete = 0;
    my $lTimeBegin = time();

    # Rejoin the threads
    while ($iThreadComplete < $iThreadLocalMax)
    {
        # If a timeout has been defined, make sure we have not been running longer than that
        if (defined($iTimeout))
        {
            if (time() - $lTimeBegin >= $iTimeout)
            {
                confess &log(ERROR, "threads have been running more than ${iTimeout} seconds, exiting...");

                #backup_thread_kill();

                #confess &log(WARN, "all threads have exited, aborting...");
            }
        }

        for (my $iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
        {
            if (defined($oThread[$iThreadIdx]))
            {
                if (defined($oThread[$iThreadIdx]->error()))
                {
                    backup_thread_kill();

                    if ($bConfessOnError)
                    {
                        confess &log(ERROR, 'error in thread ' . (${iThreadIdx} + 1) . ': check log for details');
                    }
                    else
                    {
                        return false;
                    }
                }

                if ($oThread[$iThreadIdx]->is_joinable())
                {
                    &log(DEBUG, "thread ${iThreadIdx} exited");
                    $oThread[$iThreadIdx]->join();
                    &log(TRACE, "thread ${iThreadIdx} object undef");
                    undef($oThread[$iThreadIdx]);
                    $iThreadComplete++;
                }
            }
        }

        # Sleep before trying again
        hsleep(.1);
    }

    &log(DEBUG, 'all threads exited');

    return true;
}

####################################################################################################################################
# ARCHIVE_GET
####################################################################################################################################
sub archive_get
{
    my $strDbClusterPath = shift;
    my $strSourceArchive = shift;
    my $strDestinationFile = shift;

    # If the destination file path is not absolute then it is relative to the data path
    if (index($strDestinationFile, '/',) != 0)
    {
        if (!defined($strDbClusterPath))
        {
            confess &log(ERROR, 'database path must be set if relative xlog paths are used');
        }

        $strDestinationFile = "${strDbClusterPath}/${strDestinationFile}";
    }

    # Determine the path where the requested archive file is located
    my $strArchivePath = dirname($oFile->path_get(PATH_BACKUP_ARCHIVE, $strSourceArchive));

    # Get the name of the requested archive file (may have hash info and compression extension)
    my @stryArchiveFile = $oFile->list(PATH_BACKUP_ABSOLUTE, $strArchivePath,
        "^${strSourceArchive}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$", undef, true);

    # If there is more than one matching archive file then there is a serious issue - likely a bug in the archiver
    if (scalar @stryArchiveFile > 1)
    {
        confess &log(ASSERT, (scalar @stryArchiveFile) . " archive files found for ${strSourceArchive}.");
    }

    # If there are no matching archive files then there are two possibilities:
    # 1) The end of the archive stream has been reached, this is normal and a 1 will be returned
    # 2) There is a hole in the archive stream so return a hard error (!!! However if turns out that due to race conditions this
    #    is harder than it looks.  Postponed and added to the backlog.  For now treated as case #1.)
    elsif (scalar @stryArchiveFile == 0)
    {
        &log(INFO, "${strSourceArchive} was not found in the archive repository");

        return 1;
    }

    &log(DEBUG, "archive_get: cp ${stryArchiveFile[0]} ${strDestinationFile}");

    # Determine if the source file is already compressed
    my $bSourceCompressed = $stryArchiveFile[0] =~ "^.*\.$oFile->{strCompressExtension}\$" ? true : false;

    # Copy the archive file to the requested location
    $oFile->copy(PATH_BACKUP_ARCHIVE, $stryArchiveFile[0], # Source file
                 PATH_DB_ABSOLUTE, $strDestinationFile,    # Destination file
                 $bSourceCompressed,                       # Source compression based on detection
                 false);                                   # Destination is not compressed

    return 0;
}

####################################################################################################################################
# ARCHIVE_PUSH
####################################################################################################################################
sub archive_push
{
    my $strDbClusterPath = shift;
    my $strSourceFile = shift;

    # If the source file path is not absolute then it is relative to the data path
    if (index($strSourceFile, '/',) != 0)
    {
        if (!defined($strDbClusterPath))
        {
            confess &log(ERROR, 'database path must be set if relative xlog paths are used');
        }

        $strSourceFile = "${strDbClusterPath}/${strSourceFile}";
    }

    # Get the destination file
    my $strDestinationFile = basename($strSourceFile);

    # Determine if this is an archive file (don't want to do compression or checksum on .backup files)
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}$/ ? true : false;

    # Append the checksum (if requested)
    if ($bArchiveFile && !$bNoChecksum)
    {
        $strDestinationFile .= '-' . $oFile->hash(PATH_DB_ABSOLUTE, $strSourceFile);
    }

    # Append compression extension
    if ($bArchiveFile && $bCompress)
    {
        $strDestinationFile .= ".$oFile->{strCompressExtension}";
    }

    # Copy the archive file
    $oFile->copy(PATH_DB_ABSOLUTE, $strSourceFile,             # Source file
                 PATH_BACKUP_ARCHIVE, $strDestinationFile,     # Destination file
                 false,                                        # Source is not compressed
                 $bArchiveFile && $bCompress,                  # Destination compress is configurable
                 undef, undef, undef,                          # Unused params
                 true);                                        # Create path if it does not exist
}

####################################################################################################################################
# ARCHIVE_XFER
####################################################################################################################################
sub archive_xfer
{
    my $strArchivePath = shift;
    my $strStopFile = shift;
    my $strCommand = shift;
    my $iArchiveMaxMB = shift;

    # Load the archive manifest - all the files that need to be pushed
    my %oManifestHash;
    $oFile->manifest(PATH_DB_ABSOLUTE, $strArchivePath, \%oManifestHash);

    # Get all the files to be transferred and calculate the total size
    my @stryFile;
    my $lFileSize = 0;
    my $lFileTotal = 0;

    foreach my $strFile (sort(keys $oManifestHash{name}))
    {
        if ($strFile =~ /^[0-F]{16}\/[0-F]{24}.*/)
        {
            push @stryFile, $strFile;

            $lFileSize += $oManifestHash{name}{"${strFile}"}{size};
            $lFileTotal++;
        }
    }

    if (defined($iArchiveMaxMB))
    {
        if ($iArchiveMaxMB < int($lFileSize / 1024 / 1024))
        {
            &log(ERROR, "local archive store has exceeded limit of ${iArchiveMaxMB}MB, archive logs will be discarded");

            my $hStopFile;
            open($hStopFile, '>', $strStopFile) or confess &log(ERROR, "unable to create stop file file ${strStopFile}");
            close($hStopFile);
        }
    }

    if ($lFileTotal == 0)
    {
        &log(DEBUG, 'no archive logs to be copied to backup');

        return 0;
    }

    $0 = "${strCommand} archive-push-async " . substr($stryFile[0], 17, 24) . '-' . substr($stryFile[scalar @stryFile - 1], 17, 24);

    # Output files to be moved to backup
    &log(INFO, "archive to be copied to backup total ${lFileTotal}, size " . file_size_format($lFileSize));

    # # Init the thread variables
    # $iThreadLocalMax = thread_init(int($lFileTotal / $iThreadThreshold) + 1);
    # my $iThreadIdx = 0;
    #
    # &log(DEBUG, "actual threads ${iThreadLocalMax}/${iThreadMax}");
    #
    # # Distribute files among the threads
    # foreach my $strFile (sort @stryFile)
    # {
    #     $oThreadQueue[$iThreadIdx]->enqueue($strFile);
    #
    #     $iThreadIdx = ($iThreadIdx + 1 == $iThreadLocalMax) ? 0 : $iThreadIdx + 1;
    # }
    #
    # # End each thread queue and start the thread
    # for ($iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    # {
    #     $oThreadQueue[$iThreadIdx]->enqueue(undef);
    #     $oThread[$iThreadIdx] = threads->create(\&archive_pull_copy_thread, $iThreadIdx, $strArchivePath);
    # }
    #
    # backup_thread_complete($iThreadTimeout);

    # Transfer each file
    foreach my $strFile (sort @stryFile)
    {
        # Construct the archive filename to backup
        my $strArchiveFile = "${strArchivePath}/${strFile}";

        # Determine if the source file is already compressed
        my $bSourceCompressed = $strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$" ? true : false;

        # Determine if this is an archive file (don't want to do compression or checksum on .backup files)
        my $bArchiveFile = basename($strFile) =~
            "^[0-F]{24}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$" ? true : false;

        # Figure out whether the compression extension needs to be added or removed
        my $bDestinationCompress = $bArchiveFile && $bCompress;
        my $strDestinationFile = basename($strFile);

        if (!$bSourceCompressed && $bDestinationCompress)
        {
            $strDestinationFile .= ".$oFile->{strCompressExtension}";
        }
        elsif ($bSourceCompressed && !$bDestinationCompress)
        {
            $strDestinationFile = substr($strDestinationFile, 0, length($strDestinationFile) - 3);
        }

        &log(DEBUG, "backup archive file ${strFile}, archive ${bArchiveFile}, source_compressed = ${bSourceCompressed}, " .
                    "destination_compress ${bDestinationCompress}, default_compress = ${bCompress}");

        # Copy the archive file
        $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveFile,         # Source file
                     PATH_BACKUP_ARCHIVE, $strDestinationFile,  # Destination file
                     $bSourceCompressed,                        # Source is not compressed
                     $bDestinationCompress,                     # Destination compress is configurable
                     undef, undef, undef,                       # Unused params
                     true);                                     # Create path if it does not exist

        #  Remove the source archive file
        unlink($strArchiveFile) or confess &log(ERROR, "unable to remove ${strArchiveFile}");
    }

    # Find the archive paths that need to be removed
    my $strPathMax = substr((sort {$b cmp $a} @stryFile)[0], 0, 16);

    &log(DEBUG, "local archive path max = ${strPathMax}");

    foreach my $strPath ($oFile->list(PATH_DB_ABSOLUTE, $strArchivePath, "^[0-F]{16}\$"))
    {
        if ($strPath lt $strPathMax)
        {
            &log(DEBUG, "removing local archive path ${strPath}");
            rmdir($strArchivePath . '/' . $strPath) or &log(WARN, "unable to remove archive path ${strPath}, is it empty?");
        }

        # If the dir is not empty check if the files are in the manifest
        # If they are error - there has been some issue
        # If not, they are new - continue processing without error - they'll be picked up on the next run
    }

    # Return number of files indicating that processing should continue
    return $lFileTotal;
}

# sub archive_pull_copy_thread
# {
#     my @args = @_;
#
#     my $iThreadIdx = $args[0];
#     my $strArchivePath = $args[1];
#
#     my $oFileThread = $oFile->clone($iThreadIdx);   # Thread local file object
#
#     # When a KILL signal is received, immediately abort
#     $SIG{'KILL'} = sub {threads->exit();};
#
#     while (my $strFile = $oThreadQueue[$iThreadIdx]->dequeue())
#     {
#         &log(INFO, "thread ${iThreadIdx} backing up archive file ${strFile}");
#
#         my $strArchiveFile = "${strArchivePath}/${strFile}";
#
#         # Copy the file
#         $oFileThread->file_copy(PATH_DB_ABSOLUTE, $strArchiveFile,
#                                            PATH_BACKUP_ARCHIVE, basename($strFile),
#                                            undef, undef,
#                                            undef); # cannot set permissions remotely yet $oFile->{strDefaultFilePermission});
#
#         #  Remove the source archive file
#         unlink($strArchiveFile) or confess &log(ERROR, "unable to remove ${strArchiveFile}");
#     }
# }

sub archive_compress
{
    my $strArchivePath = shift;
    my $strCommand = shift;
    my $iFileCompressMax = shift;

    # Load the archive manifest - all the files that need to be pushed
    my %oManifestHash = $oFile->manifest_get(PATH_DB_ABSOLUTE, $strArchivePath);

    # Get all the files to be compressed and calculate the total size
    my @stryFile;
    my $lFileSize = 0;
    my $lFileTotal = 0;

    foreach my $strFile (sort(keys $oManifestHash{name}))
    {
        if ($strFile =~ /^[0-F]{16}\/[0-F]{24}(\-[0-f]+){0,1}$/)
        {
            push @stryFile, $strFile;

            $lFileSize += $oManifestHash{name}{"${strFile}"}{size};
            $lFileTotal++;

            if ($lFileTotal >= $iFileCompressMax)
            {
                last;
            }
        }
    }

    if ($lFileTotal == 0)
    {
        &log(DEBUG, 'no archive logs to be compressed');

        return;
    }

    $0 = "${strCommand} archive-compress-async " . substr($stryFile[0], 17, 24) . '-' . substr($stryFile[scalar @stryFile - 1], 17, 24);

    # Output files to be compressed
    &log(INFO, "archive to be compressed total ${lFileTotal}, size " . file_size_format($lFileSize));

    # Init the thread variables
    $iThreadLocalMax = thread_init(int($lFileTotal / $iThreadThreshold) + 1);
    my $iThreadIdx = 0;

    # Distribute files among the threads
    foreach my $strFile (sort @stryFile)
    {
        $oThreadQueue[$iThreadIdx]->enqueue($strFile);

        $iThreadIdx = ($iThreadIdx + 1 == $iThreadLocalMax) ? 0 : $iThreadIdx + 1;
    }

    # End each thread queue and start the thread
    for ($iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        $oThreadQueue[$iThreadIdx]->enqueue(undef);
        $oThread[$iThreadIdx] = threads->create(\&archive_pull_compress_thread, $iThreadIdx, $strArchivePath);
    }

    # Complete the threads
    backup_thread_complete($iThreadTimeout);
}

sub archive_pull_compress_thread
{
    my @args = @_;

    my $iThreadIdx = $args[0];
    my $strArchivePath = $args[1];

    my $oFileThread = $oFile->clone($iThreadIdx);   # Thread local file object

    # When a KILL signal is received, immediately abort
    $SIG{'KILL'} = sub {threads->exit();};

    while (my $strFile = $oThreadQueue[$iThreadIdx]->dequeue())
    {
        &log(INFO, "thread ${iThreadIdx} compressing archive file ${strFile}");

        # Compress the file
        $oFileThread->file_compress(PATH_DB_ABSOLUTE, "${strArchivePath}/${strFile}");
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
    my $strRegExp = '^';

    if ($bFull || $bDifferential || $bIncremental)
    {
        $strRegExp .= $strDateTimeRegExp . 'F';
    }

    if ($bDifferential || $bIncremental)
    {
        if ($bFull)
        {
            $strRegExp .= "(\\_";
        }
        else
        {
            $strRegExp .= "\\_";
        }

        $strRegExp .= $strDateTimeRegExp;

        if ($bDifferential && $bIncremental)
        {
            $strRegExp .= '(D|I)';
        }
        elsif ($bDifferential)
        {
            $strRegExp .= 'D';
        }
        else
        {
            $strRegExp .= 'I';
        }

        if ($bFull)
        {
            $strRegExp .= '){0,1}';
        }
    }

    $strRegExp .= "\$";

    &log(DEBUG, "backup_regexp_get($bFull, $bDifferential, $bIncremental): $strRegExp");

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

    if ($strType eq BACKUP_TYPE_INCR)
    {
        $strDirectory = ($oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 1), 'reverse'))[0];
    }

    if (!defined($strDirectory) && $strType ne BACKUP_TYPE_FULL)
    {
        $strDirectory = ($oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), 'reverse'))[0];
    }

    return $strDirectory;
}

####################################################################################################################################
# BACKUP_FILE_NOT_IN_MANIFEST - Find all files in a backup path that are not in the supplied manifest
####################################################################################################################################
sub backup_file_not_in_manifest
{
    my $strPathType = shift;
    my $oManifest = shift;
    my $oAbortedManifest = shift;

    my %oFileHash;
    $oFile->manifest($strPathType, undef, \%oFileHash);

    my @stryFile;
    my $iFileTotal = 0;

    foreach my $strName (sort(keys $oFileHash{name}))
    {
        # Ignore certain files that will never be in the manifest
        if ($strName eq 'backup.manifest' ||
            $strName eq 'version' ||
            $strName eq '.')
        {
            next;
        }

        # Get the base directory
        my $strBasePath = (split('/', $strName))[0];

        if ($strBasePath eq $strName)
        {
            my $strSection = $strBasePath eq 'tablespace' ? 'backup:tablespace' : "${strBasePath}:path";

            if ($oManifest->test($strSection))
            {
                next;
            }
        }
        else
        {
            my $strPath = substr($strName, length($strBasePath) + 1);

            # Create the section from the base path
            my $strSection = $strBasePath;

            if ($strSection eq 'tablespace')
            {
                my $strTablespace = (split('/', $strPath))[0];

                $strSection = $strSection . ':' . $strTablespace;

                if ($strTablespace eq $strPath)
                {
                    if ($oManifest->test("${strSection}:path"))
                    {
                        next;
                    }
                }

                $strPath = substr($strPath, length($strTablespace) + 1);
            }

            my $cType = $oFileHash{name}{"${strName}"}{type};

            if ($cType eq 'd')
            {
                if ($oManifest->test("${strSection}:path", "${strPath}"))
                {
                    next;
                }
            }
            else
            {
                if ($oManifest->test("${strSection}:file", "${strPath}"))
                {
                    if ($oManifest->get("${strSection}:file", $strPath, MANIFEST_SUBKEY_SIZE) ==
                            $oFileHash{name}{"${strName}"}{size} &&
                        $oManifest->get("${strSection}:file", $strPath, MANIFEST_SUBKEY_MODIFICATION_TIME) ==
                            $oFileHash{name}{"${strName}"}{modification_time})
                    {
                        my $strChecksum = $oAbortedManifest->get("${strSection}:file", $strPath, MANIFEST_SUBKEY_CHECKSUM, false);

                        if (defined($strChecksum))
                        {
                            $oManifest->set("${strSection}:file", $strPath, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
                        }

                        $oManifest->set("${strSection}:file", $strPath, MANIFEST_SUBKEY_EXISTS, true);
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
    my $oManifest = shift;
    my $oAbortedManifest = shift;

    &log(INFO, 'cleaning backup tmp path');

    # Remove the pg_xlog directory since it contains nothing useful for the new backup
    if (-e $oFile->path_get(PATH_BACKUP_TMP, 'base/pg_xlog'))
    {
        remove_tree($oFile->path_get(PATH_BACKUP_TMP, 'base/pg_xlog')) or confess &log(ERROR, 'unable to delete tmp pg_xlog path');
    }

    # Remove the pg_tblspc directory since it is trivial to rebuild, but hard to compare
    if (-e $oFile->path_get(PATH_BACKUP_TMP, 'base/pg_tblspc'))
    {
        remove_tree($oFile->path_get(PATH_BACKUP_TMP, 'base/pg_tblspc')) or confess &log(ERROR, 'unable to delete tmp pg_tblspc path');
    }

    # Get the list of files that should be deleted from temp
    my @stryFile = backup_file_not_in_manifest(PATH_BACKUP_TMP, $oManifest, $oAbortedManifest);

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
    my $strDbClusterPath = shift;
    my $oBackupManifest = shift;
    my $oLastManifest = shift;
    my $oTablespaceMapRef = shift;
    my $strLevel = shift;

    # If no level is defined then it must be base
    if (!defined($strLevel))
    {
        $strLevel = 'base';
    }

    # Get the manifest for this level
    my %oManifestHash;
    $oFile->manifest(PATH_DB_ABSOLUTE, $strDbClusterPath, \%oManifestHash);

    $oBackupManifest->set(MANIFEST_SECTION_BACKUP_PATH, $strLevel, undef, $strDbClusterPath);

    # Loop though all paths/files/links in the manifest
    foreach my $strName (sort(keys $oManifestHash{name}))
    {
        # Skip certain files during backup
        if (($strName =~ /^pg\_xlog\/.*/ && !$bNoStartStop) || # pg_xlog/ - this will be reconstructed
            $strName =~ /^postmaster\.pid$/ ||                 # postmaster.pid - to avoid confusing postgres when restoring
            $strName =~ /^recovery\.conf$/)                    # recovery.conf - doesn't make sense to backup this file
        {
            next;
        }

        my $cType = $oManifestHash{name}{"${strName}"}{type};
        my $strLinkDestination = $oManifestHash{name}{"${strName}"}{link_destination};
        my $strSection = "${strLevel}:path";

        if ($cType eq 'f')
        {
            $strSection = "${strLevel}:file";
        }
        elsif ($cType eq 'l')
        {
            $strSection = "${strLevel}:link";
        }
        elsif ($cType ne 'd')
        {
            confess &log(ASSERT, "unrecognized file type $cType for file $strName");
        }

        # User and group required for all types
        $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_USER, $oManifestHash{name}{"${strName}"}{user});
        $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_GROUP, $oManifestHash{name}{"${strName}"}{group});

        # Mode for required file and path type only
        if ($cType eq 'f' || $cType eq 'd')
        {
            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_MODE, $oManifestHash{name}{"${strName}"}{permission});
        }

        # Modification time and size required for file type only
        if ($cType eq 'f')
        {
            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME,
                                  $oManifestHash{name}{"${strName}"}{modification_time} + 0);
            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_SIZE, $oManifestHash{name}{"${strName}"}{size} + 0);
        }

        # Link destination required for link type only
        if ($cType eq 'l')
        {
            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_DESTINATION,
                                  $oManifestHash{name}{"${strName}"}{link_destination});

            # If this is a tablespace then follow the link
            if (index($strName, 'pg_tblspc/') == 0 && $strLevel eq 'base')
            {
                my $strTablespaceOid = basename($strName);
                my $strTablespaceName = ${$oTablespaceMapRef}{oid}{"${strTablespaceOid}"}{name};

                $oBackupManifest->set(MANIFEST_SECTION_BACKUP_TABLESPACE, $strTablespaceName,
                                      MANIFEST_SUBKEY_LINK, $strTablespaceOid);
                $oBackupManifest->set(MANIFEST_SECTION_BACKUP_TABLESPACE, $strTablespaceName,
                                      MANIFEST_SUBKEY_PATH, $strLinkDestination);

                backup_manifest_build($strLinkDestination, $oBackupManifest, undef,
                                      $oTablespaceMapRef, "tablespace:${strTablespaceName}");
            }
        }
    }

    # If this is the base level then do post-processing
    if ($strLevel eq 'base')
    {
        my $bTimeInFuture = false;

        my $lTimeBegin = $oFile->wait(PATH_DB_ABSOLUTE);

        # Loop through all backup paths (base and tablespaces)
        foreach my $strPathKey ($oBackupManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
        {
            my $strSection = "${strPathKey}:file";

            # Make sure file section exists
            if ($oBackupManifest->test($strSection))
            {
                # Loop though all files
                foreach my $strName ($oBackupManifest->keys($strSection))
                {
                    # If modification time is in the future (in this backup OR the last backup) set warning flag and do not
                    # allow a reference
                    if ($oBackupManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME) > $lTimeBegin ||
                        (defined($oLastManifest) && $oLastManifest->test($strSection, $strName, MANIFEST_SUBKEY_FUTURE, 'y')))
                    {
                        $bTimeInFuture = true;

                        # Only mark as future if still in the future in the current backup
                        if ($oBackupManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME) > $lTimeBegin)
                        {
                            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_FUTURE, 'y');
                        }
                    }
                    # Else check if modification time and size are unchanged since last backup
                    elsif (defined($oLastManifest) && $oLastManifest->test($strSection, $strName) &&
                           $oBackupManifest->get($strSection, $strName, MANIFEST_SUBKEY_SIZE) ==
                               $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_SIZE) &&
                           $oBackupManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME) ==
                               $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME))
                    {
                        # Copy reference from previous backup if possible
                        if ($oLastManifest->test($strSection, $strName, MANIFEST_SUBKEY_REFERENCE))
                        {
                            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_REFERENCE,
                                                  $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_REFERENCE));
                        }
                        # Otherwise the reference is to the previous backup
                        else
                        {
                            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_REFERENCE,
                                                  $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
                        }

                        # Copy the checksum from previous manifest
                        if ($oLastManifest->test($strSection, $strName, MANIFEST_SUBKEY_CHECKSUM))
                        {
                            $oBackupManifest->set($strSection, $strName, MANIFEST_SUBKEY_CHECKSUM,
                                                  $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_CHECKSUM));
                        }

                        # Build the manifest reference list - not used for processing but is useful for debugging
                        my $strFileReference = $oBackupManifest->get($strSection, $strName, MANIFEST_SUBKEY_REFERENCE);

                        my $strManifestReference = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_REFERENCE,
                                                                         undef, false);

                        if (!defined($strManifestReference))
                        {
                            $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_REFERENCE, undef, $strFileReference);
                        }
                        else
                        {
                            if ($strManifestReference !~ /^$strFileReference|,$strFileReference/)
                            {
                                $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_REFERENCE, undef,
                                                      $strManifestReference . ",${strFileReference}");
                            }
                        }
                    }
                }
            }
        }

        # Warn if any files in the current backup are in the future
        if ($bTimeInFuture)
        {
            &log(WARN, "some files have timestamps in the future - they will be copied to prevent possible race conditions");
        }

        # Record the time when copying will start
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
                              timestamp_string_get(undef, $lTimeBegin + 1));
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
    my $strDbClusterPath = shift;   # Database base data path
    my $oBackupManifest = shift;    # Manifest for the current backup

    # Variables used for parallel copy
    my $lTablespaceIdx = 0;
    my $lFileTotal = 0;
    my $lFileLargeSize = 0;
    my $lFileLargeTotal = 0;
    my $lFileSmallSize = 0;
    my $lFileSmallTotal = 0;

    # Decide if all the paths will be created in advance
    my $bPathCreate = $bHardLink || $strType eq BACKUP_TYPE_FULL;

    # Iterate through the path sections of the manifest to backup
    foreach my $strSectionPath ($oBackupManifest->keys())
    {
        # Skip non-path sections
        if ($strSectionPath !~ /\:path$/ || $strSectionPath =~ /^backup\:path$/)
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
            $strBackupDestinationPath = 'base';
            $strSectionFile = 'base:file';

            # Create the archive log directory
            $oFile->path_create(PATH_BACKUP_TMP, 'base/pg_xlog');
        }
        # Process each tablespace
        elsif ($strSectionPath =~ /^tablespace\:/)
        {
            $lTablespaceIdx++;
            my $strTablespaceName = (split(':', $strSectionPath))[1];
            $strBackupSourcePath = $oBackupManifest->get(MANIFEST_SECTION_BACKUP_TABLESPACE, $strTablespaceName,
                                                         MANIFEST_SUBKEY_PATH);
            $strBackupDestinationPath = "tablespace/${strTablespaceName}";
            $strSectionFile = "tablespace:${strTablespaceName}:file";

            # Create the tablespace directory and link
            if ($bPathCreate)
            {
                $oFile->path_create(PATH_BACKUP_TMP, $strBackupDestinationPath);

                $oFile->link_create(PATH_BACKUP_TMP, ${strBackupDestinationPath},
                                   PATH_BACKUP_TMP,
                                   'base/pg_tblspc/' . $oBackupManifest->get(MANIFEST_SECTION_BACKUP_TABLESPACE, $strTablespaceName,
                                                                             MANIFEST_SUBKEY_LINK),
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
            foreach my $strPath ($oBackupManifest->keys($strSectionPath))
            {
                $oFile->path_create(PATH_BACKUP_TMP, "${strBackupDestinationPath}/${strPath}", undef, true);
            }
        }

        # Possible for the path section to exist with no files (i.e. empty tablespace)
        if (!$oBackupManifest->test($strSectionFile))
        {
            next;
        }

        # Iterate through the files for each backup source path
        foreach my $strFile ($oBackupManifest->keys($strSectionFile))
        {
            my $strBackupSourceFile = "${strBackupSourcePath}/${strFile}";

            my $bProcess = false;
            my $bProcessChecksumOnly = false;

            if ($oBackupManifest->test($strSectionFile, $strFile, MANIFEST_SUBKEY_EXISTS, true))
            {
                &log(TRACE, "file ${strFile} already exists from previous backup attempt");
                $oBackupManifest->remove($strSectionFile, $strFile, MANIFEST_SUBKEY_EXISTS);

                $bProcess = !$bNoChecksum && !$oBackupManifest->test($strSectionFile, $strFile, MANIFEST_SUBKEY_CHECKSUM);
                $bProcessChecksumOnly = $bProcess;
            }
            else
            {
                # If the file has a reference it does not need to be copied since it can be retrieved from the referenced backup.
                # However, if hard-linking is turned on the link will need to be created
                my $strReference = $oBackupManifest->get($strSectionFile, $strFile, MANIFEST_SUBKEY_REFERENCE, false);

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
                    $bProcess = true;
                }
            }

            if ($bProcess)
            {
                my $lFileSize = $oBackupManifest->get($strSectionFile, $strFile, MANIFEST_SUBKEY_SIZE);

                # Setup variables needed for threaded copy
                $lFileTotal++;
                $lFileLargeSize += $lFileSize > $iSmallFileThreshold ? $lFileSize : 0;
                $lFileLargeTotal += $lFileSize > $iSmallFileThreshold ? 1 : 0;
                $lFileSmallSize += $lFileSize <= $iSmallFileThreshold ? $lFileSize : 0;
                $lFileSmallTotal += $lFileSize <= $iSmallFileThreshold ? 1 : 0;

                # Load the hash used by threaded copy
                my $strKey = sprintf('ts%012x-fs%012x-fn%012x', $lTablespaceIdx,
                                     $lFileSize, $lFileTotal);

                $oFileCopyMap{"${strKey}"}{db_file} = $strBackupSourceFile;
                $oFileCopyMap{"${strKey}"}{file_section} = $strSectionFile;
                $oFileCopyMap{"${strKey}"}{file} = ${strFile};
                $oFileCopyMap{"${strKey}"}{backup_file} = "${strBackupDestinationPath}/${strFile}";
                $oFileCopyMap{"${strKey}"}{size} = $lFileSize;
                $oFileCopyMap{"${strKey}"}{modification_time} =
                    $oBackupManifest->get($strSectionFile, $strFile, MANIFEST_SUBKEY_MODIFICATION_TIME);
                $oFileCopyMap{"${strKey}"}{checksum_only} = $bProcessChecksumOnly;
                $oFileCopyMap{"${strKey}"}{checksum} =
                    $oBackupManifest->get($strSectionFile, $strFile, MANIFEST_SUBKEY_CHECKSUM, false);
            }
        }
    }

    # Build the thread queues
    $iThreadLocalMax = thread_init(int($lFileTotal / $iThreadThreshold) + 1);
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
    &log(DEBUG, "file small total ${lFileSmallTotal}, small size: " . file_size_format($lFileSmallSize) .
                ', small thread avg total ' . file_size_format(int($iThreadFileSmallTotalMax)));
    &log(DEBUG, "file large total ${lFileLargeTotal}, large size: " . file_size_format($lFileLargeSize) .
                ', large thread avg size ' . file_size_format(int($fThreadFileLargeSizeMax)));

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


    # Create threads to process the thread queues
    my $oThreadGroup = new BackRest::ThreadGroup('backup');

    # End each thread queue and start the backup_file threads
    for (my $iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        # Output info about how much work each thread is going to do
        &log(DEBUG, "thread ${iThreadIdx} large total $oyThreadData[$iThreadIdx]{large_total}, " .
                    "size $oyThreadData[$iThreadIdx]{large_size}");
        &log(DEBUG, "thread ${iThreadIdx} small total $oyThreadData[$iThreadIdx]{small_total}, " .
                    "size $oyThreadData[$iThreadIdx]{small_size}");

        # End each queue
        $oThreadQueue[$iThreadIdx]->enqueue(undef);

        # Start the thread
        $oThreadGroup->add(threads->create(\&backup_file_thread, $iThreadIdx, !$bNoChecksum, !$bPathCreate,
                                           $oyThreadData[$iThreadIdx]{size}));
    }

    # Wait for the threads to complete
    # backup_thread_complete($iThreadTimeout);
    $oThreadGroup->complete(30);

    # Read the messages that we passed back from the threads.  These should be two types:
    # 1) remove - files that were skipped because they were removed from the database during backup
    # 2) checksum - file checksums calculated by the threads
    for (my $iThreadIdx = 0; $iThreadIdx < $iThreadLocalMax; $iThreadIdx++)
    {
        while (my $strMessage = $oMasterQueue[$iThreadIdx]->dequeue_nb())
        {
            &log (DEBUG, "message received in master queue: ${strMessage}");

            # Split the message.  Currently using | as the split character.  Not ideal, but it will do for now.
            my @strSplit = split(/\|/, $strMessage);

            my $strCommand = $strSplit[0];      # Command to execute on a file
            my $strFileSection = $strSplit[1];  # File section where the file is located
            my $strFile = $strSplit[2];         # The file to act on

            # These three parts are required
            if (!defined($strCommand) || !defined($strFileSection) || !defined($strFile))
            {
                confess &log(ASSERT, 'thread messages must have strCommand, strFileSection and strFile defined');
            }

            &log (DEBUG, "command = ${strCommand}, file_section = ${strFileSection}, file = ${strFile}");

            # If command is 'remove' then mark the skipped file in the manifest
            if ($strCommand eq 'remove')
            {
                $oBackupManifest->remove($strFileSection, $strFile);

                &log (INFO, "removed file ${strFileSection}:${strFile} from the manifest (it was removed by db during backup)");
            }
            # If command is 'checksum' then record the checksum in the manifest
            elsif ($strCommand eq 'checksum')
            {
                my $strChecksum = $strSplit[3];  # File checksum calculated by the thread

                # Checksum must be defined
                if (!defined($strChecksum))
                {
                    confess &log(ASSERT, 'thread checksum messages must have strChecksum defined');
                }

                $oBackupManifest->set($strFileSection, $strFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);

                # Log the checksum
                &log (DEBUG, "write checksum ${strFileSection}:${strFile} into manifest: ${strChecksum}");
            }
        }
    }
}

sub backup_file_thread
{
    my @args = @_;

    my $iThreadIdx = $args[0];      # Defines the index of this thread
    my $bChecksum = $args[1];       # Should checksums be generated on files after they have been backed up?
    my $bPathCreate = $args[2];     # Should paths be created automatically?
    my $lSizeTotal = $args[3];      # Total size of the files to be copied by this thread

    my $lSize = 0;                                  # Size of files currently copied by this thread
    my $strLog;                                     # Store the log message
    my $oFileThread = $oFile->clone($iThreadIdx);   # Thread local file object

    # When a KILL signal is received, immediately abort
    $SIG{'KILL'} = sub {threads->exit();};

    # Iterate through all the files in this thread's queue to be copied from the database to the backup
    while (my $strFile = $oThreadQueue[$iThreadIdx]->dequeue())
    {
        # Add the size of the current file to keep track of percent complete
        $lSize += $oFileCopyMap{$strFile}{size};

        if (!$oFileCopyMap{$strFile}{checksum_only})
        {
            # Output information about the file to be copied
            $strLog = "thread ${iThreadIdx} backing up file $oFileCopyMap{$strFile}{db_file} (" .
                      file_size_format($oFileCopyMap{$strFile}{size}) .
                      ($lSizeTotal > 0 ? ', ' . int($lSize * 100 / $lSizeTotal) . '%' : '') . ')';

            # Copy the file from the database to the backup (will return false if the source file is missing)
            unless($oFileThread->copy(PATH_DB_ABSOLUTE, $oFileCopyMap{$strFile}{db_file},
                                      PATH_BACKUP_TMP, $oFileCopyMap{$strFile}{backup_file} .
                                          ($bCompress ? '.' . $oFile->{strCompressExtension} : ''),
                                      false,        # Source is not compressed since it is the db directory
                                      $bCompress,   # Destination should be compressed based on backup settings
                                      true,         # Ignore missing files
                                      $oFileCopyMap{$strFile}{modification_time}, # Set modification time
                                      undef,        # Do not set original permissions
                                      true))        # Create the destiation directory if it does not exist
            {
                # If file is missing assume the database removed it (else corruption and nothing we can do!)
                &log(INFO, "thread ${iThreadIdx} skipped file removed by database: " . $oFileCopyMap{$strFile}{db_file});

                # Write a message into the master queue to have the file removed from the manifest
                $oMasterQueue[$iThreadIdx]->enqueue("remove|$oFileCopyMap{$strFile}{file_section}|$oFileCopyMap{$strFile}{file}");

                # Move on to the next file
                next;
            }
        }

        # Generate checksum for file if configured
        if ($bChecksum && $lSize != 0)
        {
            # Generate the checksum
            my $strChecksum = $oFileThread->hash(PATH_BACKUP_TMP,
                                                 $oFileCopyMap{$strFile}{backup_file} . ($bCompress ? '.gz' : ''), $bCompress);

            # Write the checksum message into the master queue
            $oMasterQueue[$iThreadIdx]->enqueue("checksum|$oFileCopyMap{$strFile}{file_section}|$oFileCopyMap{$strFile}{file}|${strChecksum}");

            # Output information about the file to be checksummed
            if (!defined($strLog))
            {
                $strLog = "thread ${iThreadIdx} checksum-only $oFileCopyMap{$strFile}{db_file} (" .
                          file_size_format($oFileCopyMap{$strFile}{size}) .
                          ($lSizeTotal > 0 ? ', ' . int($lSize * 100 / $lSizeTotal) . '%' : '') . ')';
            }

            &log(INFO, $strLog . " checksum ${strChecksum}");
        }
        else
        {
            &log(INFO, $strLog);
        }
    }

    &log(DEBUG, "thread ${iThreadIdx} exiting");
    # $oFileThread = undef;
}

####################################################################################################################################
# BACKUP
#
# Performs the entire database backup.
####################################################################################################################################
sub backup
{
    my $strDbClusterPath = shift;
    my $bStartFast = shift;

    # Record timestamp start
    my $strTimestampStart = timestamp_string_get();

    # Not supporting remote backup hosts yet
    if ($oFile->is_remote(PATH_BACKUP))
    {
        confess &log(ERROR, 'remote backup host not currently supported');
    }

    if (!defined($strDbClusterPath))
    {
        confess &log(ERROR, 'cluster data path is not defined');
    }

    &log(DEBUG, "cluster path is $strDbClusterPath");

    # Create the cluster backup path
    $oFile->path_create(PATH_BACKUP_CLUSTER, undef, undef, true);

    # Build backup tmp and config
    my $strBackupTmpPath = $oFile->path_get(PATH_BACKUP_TMP);
    my $strBackupConfFile = $oFile->path_get(PATH_BACKUP_TMP, 'backup.manifest');

    # Declare the backup manifest
    my $oBackupManifest = new BackRest::Manifest($strBackupConfFile, false);

    # Find the previous backup based on the type
    my $oLastManifest;

    my $strBackupLastPath = backup_type_find($strType, $oFile->path_get(PATH_BACKUP_CLUSTER));

    if (defined($strBackupLastPath))
    {
        $oLastManifest = new BackRest::Manifest($oFile->path_get(PATH_BACKUP_CLUSTER) . "/${strBackupLastPath}/backup.manifest");

        my $strLastBackupLabel = $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL);

        &log(INFO, "last backup label: ${strLastBackupLabel}, version " .
             $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_VERSION));
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, $strLastBackupLabel);
    }
    else
    {
        if ($strType eq BACKUP_TYPE_DIFF)
        {
            &log(WARN, 'No full backup exists, differential backup has been changed to full');
        }
        elsif ($strType eq BACKUP_TYPE_INCR)
        {
            &log(WARN, 'No prior backup exists, incremental backup has been changed to full');
        }

        $strType = BACKUP_TYPE_FULL;
    }

    # Backup settings
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, $strType);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, $strTimestampStart);
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, $bCompress ? 'y' : 'n');
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_CHECKSUM, undef, !$bNoChecksum ? 'y' : 'n');
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, $bHardLink ? 'y' : 'n');

    # Start backup (unless no-start-stop is set)
    my $strArchiveStart;

    if ($bNoStartStop)
    {
        if ($oFile->exists(PATH_DB_ABSOLUTE, $strDbClusterPath . '/' . FILE_POSTMASTER_PID))
        {
            if ($bForce)
            {
                &log(WARN, '--no-start-stop passed and ' . FILE_POSTMASTER_PID . ' exists but --force was passed so backup will ' .
                           'continue though it looks like the postmaster is running and the backup will probably not be ' .
                           'consistent');
            }
            else
            {
                &log(ERROR, '--no-start-stop passed but ' . FILE_POSTMASTER_PID . ' exists - looks like the postmaster is ' .
                            'running. Shutdown the postmaster and try again, or use --force.');
                exit 1;
            }
        }
    }
    else
    {
        my $strTimestampDbStart;

        ($strArchiveStart, $strTimestampDbStart) =
            $oDb->backup_start('pg_backrest backup started ' . $strTimestampStart, $bStartFast);

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_DB_START, undef, $strTimestampDbStart);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
        &log(INFO, "archive start: ${strArchiveStart}");
    }

    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_VERSION, undef, version_get());

    # Build the backup manifest
    my %oTablespaceMap;

    if ($bNoStartStop)
    {
        my %oTablespaceManifestHash;
        $oFile->manifest(PATH_DB_ABSOLUTE, $strDbClusterPath . '/pg_tblspc', \%oTablespaceManifestHash);

        foreach my $strName (sort(keys $oTablespaceManifestHash{name}))
        {
            if ($strName eq '.' or $strName eq '..')
            {
                next;
            }

            if ($oTablespaceManifestHash{name}{"${strName}"}{type} ne 'l')
            {
                confess &log(ERROR, "pg_tblspc/${strName} is not a link");
            }

            &log(DEBUG, "Found tablespace ${strName}");

            $oTablespaceMap{oid}{"${strName}"}{name} = $strName;
        }
    }
    else
    {
        $oDb->tablespace_map_get(\%oTablespaceMap);
    }

    backup_manifest_build($strDbClusterPath, $oBackupManifest, $oLastManifest, \%oTablespaceMap);
    &log(TEST, TEST_MANIFEST_BUILD);

    # Check if an aborted backup exists for this stanza
    if (-e $strBackupTmpPath)
    {
        my $bUsable = false;

        my $strType = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
        my $strPrior = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
        my $strVersion = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_VERSION);

        my $strAbortedType = '<undef>';
        my $strAbortedPrior = '<undef>';
        my $strAbortedVersion = '<undef>';
        my $oAbortedManifest;

        # Attempt to read the manifest file in the aborted backup to see if the backup type and prior backup are the same as the
        # new backup that is being started.  If any error at all occurs then the backup will be considered unusable and a resume
        # will not be attempted.
        eval
        {
            # Load the aborted manifest
            $oAbortedManifest = new BackRest::Manifest("${strBackupTmpPath}/backup.manifest");

            # Default values if they are not set
            $strAbortedType = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
            $strAbortedPrior = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
            $strAbortedVersion = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_VERSION);

            # The backup is usable if between the current backup and the aborted backup:
            # 1) The version matches
            # 2) The type of both is full or the types match and prior matches
            if ($strAbortedVersion eq $strVersion)
            {
                if ($strAbortedType eq BACKUP_TYPE_FULL
                    && $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE) eq BACKUP_TYPE_FULL)
                {
                    $bUsable = true;
                }
                elsif ($strAbortedType eq $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE) &&
                       $strAbortedPrior eq $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR))
                {
                    $bUsable = true;
                }
            }
        };

        # If the aborted backup is usable then clean it
        if ($bUsable)
        {
            &log(WARN, 'aborted backup of same type exists, will be cleaned to remove invalid files and resumed');
            &log(TEST, TEST_BACKUP_RESUME);

            # Clean the old backup tmp path
            backup_tmp_clean($oBackupManifest, $oAbortedManifest);
        }
        # Else remove it
        else
        {
            my $strReason = "new version '${strVersion}' does not match aborted version '${strVersion}'";

            if ($strVersion eq $strAbortedVersion)
            {
                if ($strType ne $strAbortedType)
                {
                    $strReason = "new type '${strType}' does not match aborted type '${strAbortedType}'";
                }
                else
                {
                    $strReason = "new prior '${strPrior}' does not match aborted prior '${strAbortedPrior}'";
                }
            }

            &log(WARN, "aborted backup exists, but cannot be resumed (${strReason}) - will be dropped and recreated");
            &log(TEST, TEST_BACKUP_NORESUME);

            remove_tree($oFile->path_get(PATH_BACKUP_TMP))
                or confess &log(ERROR, "unable to delete tmp path: ${strBackupTmpPath}");
            $oFile->path_create(PATH_BACKUP_TMP);
        }
    }
    # Else create the backup tmp path
    else
    {
        &log(DEBUG, "creating backup path ${strBackupTmpPath}");
        $oFile->path_create(PATH_BACKUP_TMP);
    }

    # Write the VERSION file
    my $hVersionFile;
    open($hVersionFile, '>', "${strBackupTmpPath}/" . FILE_VERSION) or confess 'unable to open version file';
    print $hVersionFile version_get();
    close($hVersionFile);

    # Save the backup manifest
    $oBackupManifest->save();

    # Perform the backup
    backup_file($strDbClusterPath, $oBackupManifest);

    # Stop backup (unless no-start-stop is set)
    my $strArchiveStop;

    if (!$bNoStartStop)
    {
        my $strTimestampDbStop;
        ($strArchiveStop, $strTimestampDbStop) = $oDb->backup_stop();

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_DB_STOP, undef, $strTimestampDbStop);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, $strArchiveStop);

        &log(INFO, 'archive stop: ' . $strArchiveStop);
    }

    # If archive logs are required to complete the backup, then fetch them.  This is the default, but can be overridden if the
    # archive logs are going to a different server.  Be careful here because there is no way to verify that the backup will be
    # consistent - at least not in this routine.
    if ($bArchiveRequired)
    {
        # Save the backup manifest a second time - before getting archive logs in case that fails
        $oBackupManifest->save();

        # Create the modification time for the archive logs
        my $lModificationTime = time();

        # After the backup has been stopped, need to make a copy of the archive logs need to make the db consistent
        &log(DEBUG, "retrieving archive logs ${strArchiveStart}:${strArchiveStop}");
        my @stryArchive = archive_list_get($strArchiveStart, $strArchiveStop, $oDb->db_version_get() < 9.3);

        foreach my $strArchive (@stryArchive)
        {
            my $strArchivePath = dirname($oFile->path_get(PATH_BACKUP_ARCHIVE, $strArchive));

            wait_for_file($strArchivePath, "^${strArchive}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$", 600);

            my @stryArchiveFile = $oFile->list(PATH_BACKUP_ABSOLUTE, $strArchivePath,
                "^${strArchive}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$");

            if (scalar @stryArchiveFile != 1)
            {
                confess &log(ERROR, "Zero or more than one file found for glob: ${strArchivePath}");
            }

            &log(DEBUG, "archiving: ${strArchive} (${stryArchiveFile[0]})");

            # Copy the log file from the archive repo to the backup
            my $strDestinationFile = "base/pg_xlog/${strArchive}" . ($bCompress ? ".$oFile->{strCompressExtension}" : '');

            $oFile->copy(PATH_BACKUP_ARCHIVE, $stryArchiveFile[0],
                         PATH_BACKUP_TMP, $strDestinationFile,
                         $stryArchiveFile[0] =~ "^.*\.$oFile->{strCompressExtension}\$",
                         $bCompress, undef, $lModificationTime);

            # Add the archive file to the manifest so it can be part of the restore and checked in validation
            my $strPathSection = 'base:path';
            my $strPathLog = 'pg_xlog';
            my $strFileSection = 'base:file';
            my $strFileLog = "pg_xlog/${strArchive}";

            # Get the checksum and compare against the one already on log log file (if there is one)
            my $strChecksum = undef;

            if (!$bNoChecksum)
            {
                $strChecksum = $oFile->hash(PATH_BACKUP_TMP, $strDestinationFile, $bCompress);

                if ($stryArchiveFile[0] =~ "^${strArchive}-[0-f]+(\\.$oFile->{strCompressExtension}){0,1}\$" &&
                    $stryArchiveFile[0] !~ "^${strArchive}-${strChecksum}(\\.$oFile->{strCompressExtension}){0,1}\$")
                {
                    confess &log(ERROR, "error copying log '$stryArchiveFile[0]' to backup - checksum recored with file does " .
                                        "not match actual checksum of '${strChecksum}'", ERROR_CHECKSUM);
                }
            }

            # Set manifest values
            $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_USER,
                                  $oBackupManifest->get($strPathSection, $strPathLog, MANIFEST_SUBKEY_USER));
            $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_GROUP,
                                  $oBackupManifest->get($strPathSection, $strPathLog, MANIFEST_SUBKEY_GROUP));
            $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_MODE, '0700');
            $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_MODIFICATION_TIME, $lModificationTime);
            $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_SIZE, 16777216);

            if (defined($strChecksum))
            {
                $oBackupManifest->set($strFileSection, $strFileLog, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
            }
        }
    }

    # Create the path for the new backup
    my $strBackupPath;

    if ($strType eq BACKUP_TYPE_FULL || !defined($strBackupLastPath))
    {
        $strBackupPath = timestamp_file_string_get() . 'F';
        $strType = BACKUP_TYPE_FULL;
    }
    else
    {
        $strBackupPath = substr($strBackupLastPath, 0, 16);

        $strBackupPath .= '_' . timestamp_file_string_get();

        if ($strType eq BACKUP_TYPE_DIFF)
        {
            $strBackupPath .= 'D';
        }
        else
        {
            $strBackupPath .= 'I';
        }
    }

    # Record timestamp stop in the config
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, timestamp_string_get());
    $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupPath);

    # Save the backup manifest final time
    $oBackupManifest->save();

    &log(INFO, "new backup label: ${strBackupPath}");

    # Rename the backup tmp path to complete the backup
    &log(DEBUG, "moving ${strBackupTmpPath} to " . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackupPath));
    $oFile->move(PATH_BACKUP_TMP, undef, PATH_BACKUP_CLUSTER, $strBackupPath);

    # Create a link to the most recent backup
    $oFile->remove(PATH_BACKUP_CLUSTER, "latest");
    $oFile->link_create(PATH_BACKUP_CLUSTER, $strBackupPath, PATH_BACKUP_CLUSTER, "latest", undef, true);
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
        &log(TRACE, 'archive_list_get: pre-9.3 database, skipping log FF');
    }
    else
    {
        &log(TRACE, 'archive_list_get: post-9.3 database, including log FF');
    }

    # Get the timelines and make sure they match
    my $strTimeline = substr($strArchiveStart, 0, 8);
    my @stryArchive;
    my $iArchiveIdx = 0;

    if ($strTimeline ne substr($strArchiveStop, 0, 8))
    {
        confess &log(ERROR, "Timelines between ${strArchiveStart} and ${strArchiveStop} differ");
    }

    # Iterate through all archive logs between start and stop
    my $iStartMajor = hex substr($strArchiveStart, 8, 8);
    my $iStartMinor = hex substr($strArchiveStart, 16, 8);

    my $iStopMajor = hex substr($strArchiveStop, 8, 8);
    my $iStopMinor = hex substr($strArchiveStop, 16, 8);

    $stryArchive[$iArchiveIdx] = uc(sprintf("${strTimeline}%08x%08x", $iStartMajor, $iStartMinor));
    $iArchiveIdx += 1;

    while (!($iStartMajor == $iStopMajor && $iStartMinor == $iStopMinor))
    {
        $iStartMinor += 1;

        if ($bSkipFF && $iStartMinor == 255 || !$bSkipFF && $iStartMinor == 256)
        {
            $iStartMajor += 1;
            $iStartMinor = 0;
        }

        $stryArchive[$iArchiveIdx] = uc(sprintf("${strTimeline}%08x%08x", $iStartMajor, $iStartMinor));
        $iArchiveIdx += 1;
    }

    &log(TRACE, "    archive_list_get: $strArchiveStart:$strArchiveStop (@stryArchive)");

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
            confess &log(ERROR, 'full_rentention must be a number >= 1');
        }

        my $iIndex = $iFullRetention;
        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), 'reverse');

        while (defined($stryPath[$iIndex]))
        {
            # Delete all backups that depend on the full backup.  Done in reverse order so that remaining backups will still
            # be consistent if the process dies
            foreach $strPath ($oFile->list(PATH_BACKUP_CLUSTER, undef, '^' . $stryPath[$iIndex] . '.*', 'reverse'))
            {
                system("rm -rf ${strBackupClusterPath}/${strPath}") == 0 or confess &log(ERROR, "unable to delete backup ${strPath}");
            }

            &log(INFO, 'removed expired full backup: ' . $stryPath[$iIndex]);

            $iIndex++;
        }
    }

    # Find all the expired differential backups
    if (defined($iDifferentialRetention))
    {
        # Make sure iDifferentialRetention is valid
        if (!looks_like_number($iDifferentialRetention) || $iDifferentialRetention < 1)
        {
            confess &log(ERROR, 'differential_rentention must be a number >= 1');
        }

        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(0, 1, 0), 'reverse');

        if (defined($stryPath[$iDifferentialRetention - 1]))
        {
            &log(DEBUG, 'differential expiration based on ' . $stryPath[$iDifferentialRetention - 1]);

            # Get a list of all differential and incremental backups
            foreach $strPath ($oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(0, 1, 1), 'reverse'))
            {
                &log(DEBUG, "checking ${strPath} for differential expiration");

                # Remove all differential and incremental backups before the oldest valid differential
                if ($strPath lt $stryPath[$iDifferentialRetention - 1])
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
        &log(INFO, 'archive rentention type not set - archive logs will not be expired');
        return;
    }

    # Determine which backup type to use for archive retention (full, differential, incremental)
    if ($strArchiveRetentionType eq BACKUP_TYPE_FULL)
    {
        if (!defined($iArchiveRetention))
        {
            $iArchiveRetention = $iFullRetention;
        }

        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 0, 0), 'reverse');
    }
    elsif ($strArchiveRetentionType eq BACKUP_TYPE_DIFF)
    {
        if (!defined($iArchiveRetention))
        {
            $iArchiveRetention = $iDifferentialRetention;
        }

        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 0), 'reverse');
    }
    elsif ($strArchiveRetentionType eq BACKUP_TYPE_INCR)
    {
        @stryPath = $oFile->list(PATH_BACKUP_CLUSTER, undef, backup_regexp_get(1, 1, 1), 'reverse');
    }
    else
    {
        confess &log(ERROR, "unknown archive_retention_type '${strArchiveRetentionType}'");
    }

    # Make sure that iArchiveRetention is set and valid
    if (!defined($iArchiveRetention))
    {
        confess &log(ERROR, 'archive_rentention must be set if archive_retention_type is set');
        return;
    }

    if (!looks_like_number($iArchiveRetention) || $iArchiveRetention < 1)
    {
        confess &log(ERROR, 'archive_rentention must be a number >= 1');
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
        if ($strArchiveRetentionType eq BACKUP_TYPE_FULL && scalar @stryPath > 0)
        {
            &log(INFO, 'fewer than required backups for retention, but since archive_retention_type = full using oldest full backup');
            $strArchiveRetentionBackup = $stryPath[scalar @stryPath - 1];
        }

        if (!defined($strArchiveRetentionBackup))
        {
            return;
        }
    }

    # Get the archive logs that need to be kept.  To be cautious we will keep all the archive logs starting from this backup
    # even though they are also in the pg_xlog directory (since they have been copied more than once).
    &log(INFO, 'archive retention based on backup ' . $strArchiveRetentionBackup);

    my $oManifest = new BackRest::Manifest($oFile->path_get(PATH_BACKUP_CLUSTER) . "/${strArchiveRetentionBackup}/backup.manifest");
    my $strArchiveLast = $oManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START);

    if (!defined($strArchiveLast))
    {
        confess &log(ERROR, "invalid archive location retrieved ${strArchiveRetentionBackup}");
    }

    &log(INFO, 'archive retention starts at ' . $strArchiveLast);

    # Remove any archive directories or files that are out of date
    foreach $strPath ($oFile->list(PATH_BACKUP_ARCHIVE, undef, "^[0-F]{16}\$"))
    {
        &log(DEBUG, 'found major archive path ' . $strPath);

        # If less than first 16 characters of current archive file, then remove the directory
        if ($strPath lt substr($strArchiveLast, 0, 16))
        {
            my $strFullPath = $oFile->path_get(PATH_BACKUP_ARCHIVE) . "/${strPath}";

            remove_tree($strFullPath) > 0 or confess &log(ERROR, "unable to remove ${strFullPath}");

            &log(DEBUG, 'removed major archive path ' . $strFullPath);
        }
        # If equals the first 16 characters of the current archive file, then delete individual files instead
        elsif ($strPath eq substr($strArchiveLast, 0, 16))
        {
            my $strSubPath;

            # Look for archive files in the archive directory
            foreach $strSubPath ($oFile->list(PATH_BACKUP_ARCHIVE, $strPath, "^[0-F]{24}.*\$"))
            {
                # Delete if the first 24 characters less than the current archive file
                if ($strSubPath lt substr($strArchiveLast, 0, 24))
                {
                    unlink($oFile->path_get(PATH_BACKUP_ARCHIVE, $strSubPath)) or confess &log(ERROR, 'unable to remove ' . $strSubPath);
                    &log(DEBUG, 'removed expired archive file ' . $strSubPath);
                }
            }
        }
    }
}

1;
