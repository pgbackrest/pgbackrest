####################################################################################################################################
# ARCHIVE MODULE
####################################################################################################################################
package BackRest::Archive;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname basename);
use Fcntl qw(SEEK_CUR O_RDONLY O_WRONLY O_CREAT O_EXCL);
use Exporter qw(import);

use lib dirname($0);
use BackRest::Utility;
use BackRest::Exception;
use BackRest::Config;
use BackRest::Lock;
use BackRest::File;
use BackRest::Remote;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant
{
    OP_ARCHIVE_PUSH_CHECK => 'Archive->pushCheck'
};

our @EXPORT = qw(OP_ARCHIVE_PUSH_CHECK);

####################################################################################################################################
# File constants
####################################################################################################################################
use constant
{
    ARCHIVE_INFO_FILE => 'archive.info'
};

push @EXPORT, qw(ARCHIVE_INFO_FILE);

####################################################################################################################################
# constructor
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    return $self;
}

####################################################################################################################################
# process
#
# Process archive commands.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Process push
    if (operationTest(OP_ARCHIVE_PUSH))
    {
        return $self->pushProcess();
    }

    # Process get
    if (operationTest(OP_ARCHIVE_GET))
    {
        return $self->getProcess();
    }

    # Error if any other operation is found
    confess &log(ASSERT, "Archive->process() called with invalid operation: " . operationGet());
}

################################################################################################################################
# getProcess
################################################################################################################################
sub getProcess
{
    my $self = shift;

    # Make sure the archive file is defined
    if (!defined($ARGV[1]))
    {
        confess &log(ERROR, 'WAL segment not provided', ERROR_PARAM_REQUIRED);
    }

    # Make sure the destination file is defined
    if (!defined($ARGV[2]))
    {
        confess &log(ERROR, 'WAL segment destination no provided', ERROR_PARAM_REQUIRED);
    }

    # Info for the Postgres log
    &log(INFO, 'getting WAL segment ' . $ARGV[1]);

    # Get the WAL segment
    return $self->get($ARGV[1], $ARGV[2]);
}

####################################################################################################################################
# walFileName
#
# Returns the filename in the archive of a WAL segment.  Optionally, a wait time can be specified.  In this case an error will be
# thrown when the WAL segment is not found.
####################################################################################################################################
sub walFileName
{
    my $self = shift;
    my $oFile = shift;
    my $strWalSegment = shift;
    my $iWaitSeconds = shift;

    # Record the start time
    my $oWait = waitInit($iWaitSeconds);

    # Determine the path where the requested WAL segment is located
    my $strArchivePath = dirname($oFile->path_get(PATH_BACKUP_ARCHIVE, $strWalSegment));

    do
    {
        # Get the name of the requested WAL segment (may have hash info and compression extension)
        my @stryWalFileName = $oFile->list(PATH_BACKUP_ABSOLUTE, $strArchivePath,
            "^${strWalSegment}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$", undef, true);

        # If there is only one result then return it
        if (@stryWalFileName == 1)
        {
            return $stryWalFileName[0];
        }

        # If there is more than one matching archive file then there is a serious issue - likely a bug in the archiver
        if (@stryWalFileName > 1)
        {
            confess &log(ASSERT, @stryWalFileName . " duplicate files found for ${strWalSegment}", ERROR_ARCHIVE_DUPLICATE);
        }
    }
    while (waitMore($oWait));

    # If waiting and no WAL segment was found then throw an error
    if (defined($iWaitSeconds))
    {
        confess &log(ERROR, "could not find WAL segment ${strWalSegment} after " . waitInterval($oWait)  . ' second(s)');
    }

    return undef;
}

####################################################################################################################################
# walInfo
#
# Retrieve information such as db version and system identifier from a WAL segment.
####################################################################################################################################
sub walInfo
{
    my $self = shift;
    my $strWalFile = shift;

    # Set operation and debug strings
    my $strOperation = 'Archive->walInfo';
    &log(TRACE, "${strOperation}: " . PATH_ABSOLUTE . ":${strWalFile}");

    # Open the WAL segment
    my $hFile;
    my $tBlock;

    sysopen($hFile, $strWalFile, O_RDONLY)
        or confess &log(ERROR, "unable to open ${strWalFile}", ERROR_FILE_OPEN);

    # Read magic
    sysread($hFile, $tBlock, 2) == 2
        or confess &log(ERROR, "unable to read xlog magic");

    my $iMagic = unpack('S', $tBlock);

    # Make sure the WAL magic is supported
    my $strDbVersion;
    my $iSysIdOffset;

    if ($iMagic == hex('0xD07E'))
    {
        $strDbVersion = '9.4';
        $iSysIdOffset = 20;
    }
    elsif ($iMagic == hex('0xD075'))
    {
        $strDbVersion = '9.3';
        $iSysIdOffset = 20;
    }
    elsif ($iMagic == hex('0xD071'))
    {
        $strDbVersion = '9.2';
        $iSysIdOffset = 12;
    }
    elsif ($iMagic == hex('0xD066'))
    {
        $strDbVersion = '9.1';
        $iSysIdOffset = 12;
    }
    elsif ($iMagic == hex('0xD064'))
    {
        $strDbVersion = '9.0';
        $iSysIdOffset = 12;
    }
    elsif ($iMagic == hex('0xD063'))
    {
        $strDbVersion = '8.4';
        $iSysIdOffset = 12;
    }
    elsif ($iMagic == hex('0xD062'))
    {
        $strDbVersion = '8.3';
        $iSysIdOffset = 12;
    }
    # elsif ($iMagic == hex('0xD05E'))
    # {
    #     $strDbVersion = '8.2';
    #     $iSysIdOffset = 12;
    # }
    # elsif ($iMagic == hex('0xD05D'))
    # {
    #     $strDbVersion = '8.1';
    #     $iSysIdOffset = 12;
    # }
    else
    {
        confess &log(ERROR, "unexpected xlog magic 0x" . sprintf("%X", $iMagic) . ' (unsupported PostgreSQL version?)',
                     ERROR_VERSION_NOT_SUPPORTED);
    }

    # Read flags
    sysread($hFile, $tBlock, 2) == 2
        or confess &log(ERROR, "unable to read xlog info");

    my $iFlag = unpack('S', $tBlock);

    $iFlag & 2
        or confess &log(ERROR, "expected long header in flags " . sprintf("%x", $iFlag));

    # Get the database system id
    sysseek($hFile, $iSysIdOffset, SEEK_CUR)
        or confess &log(ERROR, "unable to read padding");

    sysread($hFile, $tBlock, 8) == 8
        or confess &log(ERROR, "unable to read database system identifier");

    length($tBlock) == 8
        or confess &log(ERROR, "block is incorrect length");

    close($hFile);

    my $ullDbSysId = unpack('Q', $tBlock);

    &log(TRACE, sprintf("${strOperation}: WAL magic = 0x%X, database system id = ", $iMagic) . $ullDbSysId);

    return $strDbVersion, $ullDbSysId;
}

####################################################################################################################################
# get
####################################################################################################################################
sub get
{
    my $self = shift;
    my $strSourceArchive = shift;
    my $strDestinationFile = shift;

    # Create the file object
    my $oFile = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        optionRemoteTypeTest(BACKUP) ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
        optionRemoteType(),
        optionRemote()
    );

    # If the destination file path is not absolute then it is relative to the db data path
    if (index($strDestinationFile, '/',) != 0)
    {
        if (!optionTest(OPTION_DB_PATH))
        {
            confess &log(ERROR, 'database path must be set if relative xlog paths are used');
        }

        $strDestinationFile = optionGet(OPTION_DB_PATH) . "/${strDestinationFile}";
    }

    # Get the wal segment filename
    my $strArchiveFile = $self->walFileName($oFile, $strSourceArchive);

    # If there are no matching archive files then there are two possibilities:
    # 1) The end of the archive stream has been reached, this is normal and a 1 will be returned
    # 2) There is a hole in the archive stream and a hard error should be returned.  However, holes are possible due to
    #    async archiving and threading - so when to report a hole?  Since a hard error will cause PG to terminate, for now
    #    treat as case #1.
    if (!defined($strArchiveFile))
    {
        &log(INFO, "${strSourceArchive} was not found in the archive repository");

        return 1;
    }

    &log(DEBUG, "archive_get: cp ${strArchiveFile} ${strDestinationFile}");

    # Determine if the source file is already compressed
    my $bSourceCompressed = $strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$" ? true : false;

    # Copy the archive file to the requested location
    $oFile->copy(PATH_BACKUP_ARCHIVE, $strArchiveFile,     # Source file
                 PATH_DB_ABSOLUTE, $strDestinationFile,    # Destination file
                 $bSourceCompressed,                       # Source compression based on detection
                 false);                                   # Destination is not compressed

    return 0;
}

####################################################################################################################################
# pushProcess
####################################################################################################################################
sub pushProcess
{
    my $self = shift;

    # Make sure the archive push operation happens on the db side
    if (optionRemoteTypeTest(DB))
    {
        confess &log(ERROR, OP_ARCHIVE_PUSH . ' operation must run on the db host');
    }

    # Load the archive object
    use BackRest::Archive;

    # If an archive section has been defined, use that instead of the backup section when operation is OP_ARCHIVE_PUSH
    my $bArchiveAsync = optionGet(OPTION_ARCHIVE_ASYNC);
    my $strArchivePath = optionGet(OPTION_REPO_PATH);

    # If logging locally then create the stop archiving file name
    my $strStopFile;

    if ($bArchiveAsync)
    {
        $strStopFile = "${strArchivePath}/lock/" . optionGet(OPTION_STANZA) . "-archive.stop";
    }

    # If an archive file is defined, then push it
    if (defined($ARGV[1]))
    {
        # If the stop file exists then discard the archive log
        if ($bArchiveAsync)
        {
            if (-e $strStopFile)
            {
                &log(ERROR, "discarding " . basename($ARGV[1]) .
                            " due to the archive store max size exceeded" .
                            " - remove the archive stop file (${strStopFile}) to resume archiving" .
                            " and be sure to take a new backup as soon as possible");
                return 0;
            }
        }

        &log(INFO, 'pushing WAL segment ' . $ARGV[1] . ($bArchiveAsync ? ' asynchronously' : ''));

        $self->push($ARGV[1], $bArchiveAsync);

        # Exit if we are not archiving async
        if (!$bArchiveAsync)
        {
            return 0;
        }

        # Fork and exit the parent process so the async process can continue
        if (!optionTest(OPTION_TEST_NO_FORK) || !optionGet(OPTION_TEST_NO_FORK))
        {
            if (fork())
            {
                return 0;
            }
        }
        # Else the no-fork flag has been specified for testing
        else
        {
            &log(DEBUG, 'No fork on archive local for TESTING');
        }

        # Start the async archive push
        &log(DEBUG, 'starting async archive-push');
    }

    # Create a lock file to make sure async archive-push does not run more than once
    if (!lockAcquire(operationGet(), false))
    {
        &log(DEBUG, 'another async archive-push process is already running - exiting');
        return 0;
    }

    # Open the log file
    log_file_set(optionGet(OPTION_REPO_PATH) . '/log/' . optionGet(OPTION_STANZA) . '-archive-async');

    # Build the basic command string that will be used to modify the command during processing
    my $strCommand = $^X . ' ' . $0 . " --stanza=" . optionGet(OPTION_STANZA);

    # Call the archive_xfer function and continue to loop as long as there are files to process
    my $iLogTotal;

    while (!defined($iLogTotal) || $iLogTotal > 0)
    {
        $iLogTotal = $self->xfer($strArchivePath . "/archive/" . optionGet(OPTION_STANZA) . "/out", $strStopFile);

        if ($iLogTotal > 0)
        {
            &log(DEBUG, "transferred ${iLogTotal} WAL segment(s), calling Archive->xfer() again");
        }
        else
        {
            &log(DEBUG, 'transfer found 0 WAL segments - exiting');
        }
    }

    lockRelease();
    return 0;
}

####################################################################################################################################
# push
####################################################################################################################################
sub push
{
    my $self = shift;
    my $strSourceFile = shift;
    my $bAsync = shift;

    # Create the file object
    my $oFile = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        $bAsync || optionRemoteTypeTest(NONE) ? optionGet(OPTION_REPO_PATH) : optionGet(OPTION_REPO_REMOTE_PATH),
        $bAsync ? NONE : optionRemoteType(),
        optionRemote($bAsync)
    );

    # If the source file path is not absolute then it is relative to the data path
    if (index($strSourceFile, '/',) != 0)
    {
        if (!optionTest(OPTION_DB_PATH))
        {
            confess &log(ERROR, 'database path must be set if relative xlog paths are used');
        }

        $strSourceFile = optionGet(OPTION_DB_PATH) . "/${strSourceFile}";
    }

    # Get the destination file
    my $strDestinationFile = basename($strSourceFile);

    # Get the compress flag
    my $bCompress = $bAsync ? false : optionGet(OPTION_COMPRESS);

    # Determine if this is an archive file (don't do compression or checksum on .backup, .history, etc.)
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}$/ ? true : false;

    # Check that there are no issues with pushing this WAL segment
    if ($bArchiveFile && !$bAsync)
    {
        my ($strDbVersion, $ullDbSysId) = $self->walInfo($strSourceFile);
        $self->pushCheck($oFile, substr(basename($strSourceFile), 0, 24), $strSourceFile, $strDbVersion, $ullDbSysId);
    }

    # Append compression extension
    if ($bArchiveFile && $bCompress)
    {
        $strDestinationFile .= '.' . $oFile->{strCompressExtension};
    }

    # Copy the WAL segment
    $oFile->copy(PATH_DB_ABSOLUTE, $strSourceFile,                          # Source type/file
                 $bAsync ? PATH_BACKUP_ARCHIVE_OUT : PATH_BACKUP_ARCHIVE,   # Destination type
                 $strDestinationFile,                                       # Destination file
                 false,                                                     # Source is not compressed
                 $bArchiveFile && $bCompress,                               # Destination compress is configurable
                 undef, undef, undef,                                       # Unused params
                 true,                                                      # Create path if it does not exist
                 undef, undef,                                              # User and group
                 $bArchiveFile);                                            # Append checksum if archive file
}

####################################################################################################################################
# pushCheck
####################################################################################################################################
sub pushCheck
{
    my $self = shift;
    my $oFile = shift;
    my $strWalSegment = shift;
    my $strWalFile = shift;
    my $strDbVersion = shift;
    my $ullDbSysId = shift;

    # Set operation and debug strings
    my $strOperation = OP_ARCHIVE_PUSH_CHECK;
    &log(DEBUG, "${strOperation}: " . PATH_BACKUP_ARCHIVE . ":${strWalSegment}");
    my $strChecksum;

    if ($oFile->is_remote(PATH_BACKUP_ARCHIVE))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'wal-segment'} = $strWalSegment;
        $oParamHash{'db-version'} = $strDbVersion;
        $oParamHash{'db-sys-id'} = $ullDbSysId;

        # Output remote trace info
        &log(TRACE, "${strOperation}: remote (" . $oFile->{oRemote}->command_param_string(\%oParamHash) . ')');

        # Execute the command
        $strChecksum = $oFile->{oRemote}->command_execute($strOperation, \%oParamHash, true);

        if ($strChecksum eq 'Y')
        {
            undef($strChecksum);
        }
    }
    else
    {
        # Create the archive path if it does not exist
        if (!$oFile->exists(PATH_BACKUP_ARCHIVE))
        {
            $oFile->path_create(PATH_BACKUP_ARCHIVE);
        }

        # If the info file exists check db version and system-id
        my %oDbConfig;

        if ($oFile->exists(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE))
        {
            ini_load($oFile->path_get(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE), \%oDbConfig);

            my $strError = undef;

            if ($oDbConfig{database}{'version'} ne $strDbVersion)
            {
                $strError = "WAL segment version ${strDbVersion} does not match archive version $oDbConfig{database}{'version'}";
            }

            if ($oDbConfig{database}{'system-id'} ne $ullDbSysId)
            {
                $strError = "WAL segment system-id ${ullDbSysId} does not match" .
                            " archive system-id $oDbConfig{database}{'system-id'}";
            }

            if (defined($strError))
            {
                confess &log(ERROR, "${strError}\nHINT: are you archiving to the correct stanza?", ERROR_ARCHIVE_MISMATCH);
            }
        }
        # Else create the info file from the current WAL segment
        else
        {
            $oDbConfig{database}{'system-id'} = $ullDbSysId;
            $oDbConfig{database}{'version'} = $strDbVersion;
            ini_save($oFile->path_get(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE), \%oDbConfig);
        }

        # Check if the WAL segment already exists in the archive
        $strChecksum = $self->walFileName($oFile, $strWalSegment);

        if (defined($strChecksum))
        {
            $strChecksum = substr($strChecksum, 25, 40);
        }
    }

    if (defined($strChecksum) && defined($strWalFile))
    {
        my $strChecksumNew = $oFile->hash(PATH_DB_ABSOLUTE, $strWalFile);

        if ($strChecksumNew ne $strChecksum)
        {
            confess &log(ERROR, "WAL segment ${strWalSegment} already exists in the archive", ERROR_ARCHIVE_DUPLICATE);
        }

        &log(WARN, "WAL segment ${strWalSegment} already exists in the archive with the same checksum\n" .
                   "HINT: this is valid in some recovery scenarios but may also indicate a problem");

        return undef;
    }

    return $strChecksum;
}

####################################################################################################################################
# xfer
####################################################################################################################################
sub xfer
{
    my $self = shift;
    my $strArchivePath = shift;
    my $strStopFile = shift;

    # Create a local file object to read archive logs in the local store
    my $oFile = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        NONE,
        optionRemote(true)
    );

    # Load the archive manifest - all the files that need to be pushed
    my %oManifestHash;
    $oFile->manifest(PATH_DB_ABSOLUTE, $strArchivePath, \%oManifestHash);

    # Get all the files to be transferred and calculate the total size
    my @stryFile;
    my $lFileSize = 0;
    my $lFileTotal = 0;

    foreach my $strFile (sort(keys $oManifestHash{name}))
    {
        if ($strFile =~ "^[0-F]{24}(-[0-f]{40})(\\.$oFile->{strCompressExtension}){0,1}\$" ||
            $strFile =~ /^[0-F]{8}\.history$/ || $strFile =~ /^[0-F]{24}\.[0-F]{8}\.backup$/)
        {
            CORE::push(@stryFile, $strFile);

            $lFileSize += $oManifestHash{name}{$strFile}{size};
            $lFileTotal++;
        }
    }

    if ($lFileTotal == 0)
    {
        &log(DEBUG, 'no archive logs to be copied to backup');

        return 0;
    }

    eval
    {
        # If the archive repo is remote create a new file object to do the copies
        if (!optionRemoteTypeTest(NONE))
        {
            $oFile = new BackRest::File
            (
                optionGet(OPTION_STANZA),
                optionGet(OPTION_REPO_REMOTE_PATH),
                optionRemoteType(),
                optionRemote()
            );
        }

        # Modify process name to indicate async archiving
        $0 = $^X . ' ' . $0 . " --stanza=" . optionGet(OPTION_STANZA) .
             "archive-push-async " . $stryFile[0] . '-' . $stryFile[scalar @stryFile - 1];

        # Output files to be moved to backup
        &log(INFO, "archive to be copied to backup total ${lFileTotal}, size " . file_size_format($lFileSize));

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
            my $bDestinationCompress = $bArchiveFile && optionGet(OPTION_COMPRESS);
            my $strDestinationFile = basename($strFile);

            if (!$bSourceCompressed && $bDestinationCompress)
            {
                $strDestinationFile .= ".$oFile->{strCompressExtension}";
            }
            elsif ($bSourceCompressed && !$bDestinationCompress)
            {
                $strDestinationFile = substr($strDestinationFile, 0, length($strDestinationFile) - 3);
            }

            &log(DEBUG, "archive ${strFile}, is WAL ${bArchiveFile}, source_compressed = ${bSourceCompressed}, " .
                        "destination_compress ${bDestinationCompress}, default_compress = " . optionGet(OPTION_COMPRESS));

            # Check that there are no issues with pushing this WAL segment
            if ($bArchiveFile)
            {
                my ($strDbVersion, $ullDbSysId) = $self->walInfo($strArchiveFile);
                $self->pushCheck($oFile, substr(basename($strArchiveFile), 0, 24), $strArchiveFile, $strDbVersion, $ullDbSysId);
            }

            # Copy the archive file
            $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveFile,         # Source file
                         PATH_BACKUP_ARCHIVE, $strDestinationFile,  # Destination file
                         $bSourceCompressed,                        # Source is not compressed
                         $bDestinationCompress,                     # Destination compress is configurable
                         undef, undef, undef,                       # Unused params
                         true);                                     # Create path if it does not exist

            #  Remove the source archive file
            unlink($strArchiveFile)
                or confess &log(ERROR, "copied ${strArchiveFile} to archive successfully but unable to remove it locally.  " .
                                       'This file will need to be cleaned up manually.  If the problem persists, check if ' .
                                       OP_ARCHIVE_PUSH . ' is being run with different permissions in different contexts.');

            # Remove the copied segment from the total size
            $lFileSize -= $oManifestHash{name}{$strFile}{size};
        }
    };

    my $oException = $@;

    # Create a stop file if the archive store exceeds the max even after xfer
    if (optionTest(OPTION_ARCHIVE_MAX_MB))
    {
        my $iArchiveMaxMB = optionGet(OPTION_ARCHIVE_MAX_MB);

        if ($iArchiveMaxMB < int($lFileSize / 1024 / 1024))
        {
            &log(ERROR, "local archive store max size has exceeded limit of ${iArchiveMaxMB}MB" .
                        " - WAL segments will be discarded until the stop file (${strStopFile}) is removed");

            my $hStopFile;
            open($hStopFile, '>', $strStopFile)
                or confess &log(ERROR, "unable to create stop file file ${strStopFile}");
            close($hStopFile);
        }
    }

    # If there was an exception before throw it now
    if ($oException)
    {
        confess $oException;
    }

    # Return number of files indicating that processing should continue
    return $lFileTotal;
}


####################################################################################################################################
# range
#
# Generates a range of archive log file names given the start and end log file name.  For pre-9.3 databases, use bSkipFF to exclude
# the FF that prior versions did not generate.
####################################################################################################################################
sub range
{
    my $self = shift;
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

1;
