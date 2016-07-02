####################################################################################################################################
# ARCHIVE MODULE
####################################################################################################################################
package pgBackRest::Archive;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY O_WRONLY O_CREAT);
use File::Basename qw(dirname basename);
use Scalar::Util qw(blessed);

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::ArchiveInfo;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::File;
use pgBackRest::FileCommon;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_ARCHIVE                                             => 'Archive';

use constant OP_ARCHIVE_GET                                         => OP_ARCHIVE . '->get';
use constant OP_ARCHIVE_GET_CHECK                                   => OP_ARCHIVE . '->getCheck';
    push @EXPORT, qw(OP_ARCHIVE_GET_CHECK);
use constant OP_ARCHIVE_GET_PROCESS                                 => OP_ARCHIVE . '->getProcess';
use constant OP_ARCHIVE_NEW                                         => OP_ARCHIVE . '->new';
use constant OP_ARCHIVE_PROCESS                                     => OP_ARCHIVE . '->process';
use constant OP_ARCHIVE_PUSH                                        => OP_ARCHIVE . '->pushProcess';
use constant OP_ARCHIVE_PUSH_CHECK                                  => OP_ARCHIVE . '->pushCheck';
    push @EXPORT, qw(OP_ARCHIVE_PUSH_CHECK);
use constant OP_ARCHIVE_PUSH_PROCESS                                => OP_ARCHIVE . '->pushProcess';
use constant OP_ARCHIVE_RANGE                                       => OP_ARCHIVE . '->range';
use constant OP_ARCHIVE_WAL_FILE_NAME                               => OP_ARCHIVE . '->walFileName';
use constant OP_ARCHIVE_WAL_INFO                                    => OP_ARCHIVE . '->walInfo';
use constant OP_ARCHIVE_XFER                                        => OP_ARCHIVE . '->xfer';

####################################################################################################################################
# PostgreSQL WAL magic
####################################################################################################################################
my $oWalMagicHash =
{
    hex('0xD062') => PG_VERSION_83,
    hex('0xD063') => PG_VERSION_84,
    hex('0xD064') => PG_VERSION_90,
    hex('0xD066') => PG_VERSION_91,
    hex('0xD071') => PG_VERSION_92,
    hex('0xD075') => PG_VERSION_93,
    hex('0xD07E') => PG_VERSION_94,
    hex('0xD087') => PG_VERSION_95,
    hex('0xD092') => PG_VERSION_96,
};

####################################################################################################################################
# PostgreSQL WAL system id offset
####################################################################################################################################
use constant PG_WAL_SYSTEM_ID_OFFSET_GTE_93                         => 20;
use constant PG_WAL_SYSTEM_ID_OFFSET_LT_93                          => 12;

####################################################################################################################################
# constructor
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_ARCHIVE_NEW
    );

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# process
#
# Process archive commands.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_ARCHIVE_PROCESS
    );

    my $iResult;

    # Process push
    if (commandTest(CMD_ARCHIVE_PUSH))
    {
        $iResult = $self->pushProcess();
    }
    # Process get
    elsif (commandTest(CMD_ARCHIVE_GET))
    {
        $iResult = $self->getProcess();
    }
    # Else error if any other command is found
    else
    {
        confess &log(ASSERT, "Archive->process() called with invalid command: " . commandGet());
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true}
    );
}

################################################################################################################################
# getProcess
################################################################################################################################
sub getProcess
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_ARCHIVE_GET_PROCESS
    );

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
    &log(INFO, 'get WAL segment ' . $ARGV[1]);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $self->get($ARGV[1], $ARGV[2]), trace => true}
    );
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

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strArchiveId,
        $strWalSegment,
        $bPartial,
        $iWaitSeconds
    ) =
        logDebugParam
        (
            OP_ARCHIVE_WAL_FILE_NAME, \@_,
            {name => 'oFile'},
            {name => 'strArchiveId'},
            {name => 'strWalSegment'},
            {name => 'bPartial'},
            {name => 'iWaitSeconds', required => false}
        );

    # Record the start time
    my $oWait = waitInit($iWaitSeconds);
    my @stryWalFileName;
    my $bNoTimeline = $strWalSegment =~ /^[0-F]{16}$/ ? true : false;

    do
    {
        # If the timeline is on the WAL segment then use it, otherwise contruct a regexp with the major WAL part to find paths
        # where the wal could be found.
        my @stryTimelineMajor = ('default');

        if ($bNoTimeline)
        {
            @stryTimelineMajor =
                $oFile->list(PATH_BACKUP_ARCHIVE, $strArchiveId, '[0-F]{8}' . substr($strWalSegment, 0, 8), undef, true);
        }

        foreach my $strTimelineMajor (@stryTimelineMajor)
        {
            my $strWalSegmentFind = $bNoTimeline ? $strTimelineMajor . substr($strWalSegment, 8, 8) : $strWalSegment;

            # Determine the path where the requested WAL segment is located
            my $strArchivePath = dirname($oFile->pathGet(PATH_BACKUP_ARCHIVE, "$strArchiveId/${strWalSegmentFind}"));

            # Get the name of the requested WAL segment (may have hash info and compression extension)
            push(@stryWalFileName, $oFile->list(
                PATH_BACKUP_ABSOLUTE, $strArchivePath,
                "^${strWalSegmentFind}" . ($bPartial ? '\\.partial' : '') .
                    "(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$",
                undef, true));
        }
    }
    while (@stryWalFileName == 0 && waitMore($oWait));

    # If there is more than one matching archive file then there is a serious issue - likely a bug in the archiver
    if (@stryWalFileName > 1)
    {
        confess &log(ASSERT, @stryWalFileName . " duplicate files found for ${strWalSegment}", ERROR_ARCHIVE_DUPLICATE);
    }

    # If waiting and no WAL segment was found then throw an error
    if (@stryWalFileName == 0 && defined($iWaitSeconds))
    {
        confess &log(ERROR, "could not find WAL segment ${strWalSegment} after ${iWaitSeconds} second(s)", ERROR_ARCHIVE_TIMEOUT);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strWalFileName', value => $stryWalFileName[0]}
    );
}

####################################################################################################################################
# walInfo
#
# Retrieve information such as db version and system identifier from a WAL segment.
####################################################################################################################################
sub walInfo
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalFile,
    ) =
        logDebugParam
        (
            OP_ARCHIVE_WAL_INFO, \@_,
            {name => 'strWalFile'}
        );

    # Open the WAL segment and read magic number
    #-------------------------------------------------------------------------------------------------------------------------------
    my $hFile;
    my $tBlock;

    sysopen($hFile, $strWalFile, O_RDONLY)
        or confess &log(ERROR, "unable to open ${strWalFile}", ERROR_FILE_OPEN);

    # Read magic
    sysread($hFile, $tBlock, 2) == 2
        or confess &log(ERROR, "unable to read xlog magic");

    my $iMagic = unpack('S', $tBlock);

    # Map the WAL magic number to the version of PostgreSQL.
    #
    # The magic number can be found in src/include/access/xlog_internal.h The offset can be determined by counting bytes in the
    # XLogPageHeaderData struct, though this value rarely changes.
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strDbVersion = $$oWalMagicHash{$iMagic};

    if (!defined($strDbVersion))
    {
        confess &log(ERROR, "unexpected WAL magic 0x" . sprintf("%X", $iMagic) . "\n" .
                     'HINT: is this version of PostgreSQL supported?',
                     ERROR_VERSION_NOT_SUPPORTED);
    }

    # Map the WAL PostgreSQL version to the system identifier offset.  The offset can be determined by counting bytes in the
    # XLogPageHeaderData struct, though this value rarely changes.
    #-------------------------------------------------------------------------------------------------------------------------------
    my $iSysIdOffset = $strDbVersion >= PG_VERSION_93 ? PG_WAL_SYSTEM_ID_OFFSET_GTE_93 : PG_WAL_SYSTEM_ID_OFFSET_LT_93;

    # Check flags to be sure the long header is present (this is an extra check to be sure the system id exists)
    #-------------------------------------------------------------------------------------------------------------------------------
    sysread($hFile, $tBlock, 2) == 2
        or confess &log(ERROR, "unable to read xlog info");

    my $iFlag = unpack('S', $tBlock);

    # Make sure that the long header is present or there won't be a system id
    $iFlag & 2
        or confess &log(ERROR, "expected long header in flags " . sprintf("%x", $iFlag));

    # Get the system id
    #-------------------------------------------------------------------------------------------------------------------------------
    sysseek($hFile, $iSysIdOffset, SEEK_CUR)
        or confess &log(ERROR, "unable to read padding");

    sysread($hFile, $tBlock, 8) == 8
        or confess &log(ERROR, "unable to read database system identifier");

    length($tBlock) == 8
        or confess &log(ERROR, "block is incorrect length");

    close($hFile);

    my $ullDbSysId = unpack('Q', $tBlock);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strDbVersion', value => $strDbVersion},
        {name => 'ullDbSysId', value => $ullDbSysId}
    );
}

####################################################################################################################################
# get
####################################################################################################################################
sub get
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourceArchive,
        $strDestinationFile
    ) =
        logDebugParam
        (
            OP_ARCHIVE_GET, \@_,
            {name => 'strSourceArchive'},
            {name => 'strDestinationFile'}
        );

    lockStopTest();

    # Create the file object
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        optionRemoteType(),
        protocolGet()
    );

    # If the destination file path is not absolute then it is relative to the db data path
    if (index($strDestinationFile, '/',) != 0)
    {
        # ??? If db-path is required this is can be removed.
        # ??? db-path should be added as a requirement for the remote settings work.
        if (!optionTest(OPTION_DB_PATH))
        {
            confess &log(ERROR, 'option db-path must be set when relative xlog paths are used');
        }

        $strDestinationFile = optionGet(OPTION_DB_PATH) . "/${strDestinationFile}";
    }

    # Get the wal segment filename
    my $strArchiveId = $self->getCheck($oFile);
    my $strArchiveFile = $self->walFileName($oFile, $strArchiveId, $strSourceArchive, false);

    # If there are no matching archive files then there are two possibilities:
    # 1) The end of the archive stream has been reached, this is normal and a 1 will be returned
    # 2) There is a hole in the archive stream and a hard error should be returned.  However, holes are possible due to
    #    async archiving and threading - so when to report a hole?  Since a hard error will cause PG to terminate, for now
    #    treat as case #1.
    my $iResult = 0;

    if (!defined($strArchiveFile))
    {
        &log(INFO, "unable to find ${strSourceArchive} in the archive");

        $iResult = 1;
    }
    else
    {
        # Determine if the source file is already compressed
        my $bSourceCompressed = $strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$" ? true : false;

        # Copy the archive file to the requested location
        $oFile->copy(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strArchiveFile}",  # Source file
                     PATH_DB_ABSOLUTE, $strDestinationFile,                     # Destination file
                     $bSourceCompressed,                                        # Source compression based on detection
                     false);                                                    # Destination is not compressed
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult}
    );
}

####################################################################################################################################
# getCheck
####################################################################################################################################
sub getCheck
{
    my $self = shift;
    my $oFile = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_ARCHIVE_GET_CHECK
    );

    my $strArchiveId;

    if ($oFile->isRemote(PATH_BACKUP_ARCHIVE))
    {
        $strArchiveId = $oFile->{oProtocol}->cmdExecute(OP_ARCHIVE_GET_CHECK, undef, true);
    }
    else
    {
        $strArchiveId = (new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), true))->archiveId();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId, trace => true}
    );
}

####################################################################################################################################
# pushProcess
####################################################################################################################################
sub pushProcess
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_ARCHIVE_PUSH_PROCESS
    );

    # Make sure the archive push command happens on the db side
    if (optionRemoteTypeTest(DB))
    {
        confess &log(ERROR, CMD_ARCHIVE_PUSH . ' operation must run on the db host');
    }

    # Load the archive object
    use pgBackRest::Archive;

    # If an archive section has been defined, use that instead of the backup section when command is CMD_ARCHIVE_PUSH
    my $bArchiveAsync = optionGet(OPTION_ARCHIVE_ASYNC);

    # If logging locally then create the stop archiving file name
    my $strStopFile;

    if ($bArchiveAsync)
    {
        $strStopFile = optionGet(OPTION_SPOOL_PATH) . '/stop/' . optionGet(OPTION_STANZA) . "-archive.stop";
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

        &log(INFO, 'push WAL segment ' . $ARGV[1] . ($bArchiveAsync ? ' asynchronously' : ''));

        $self->push($ARGV[1], $bArchiveAsync);

        # Fork if async archiving is enabled
        if ($bArchiveAsync)
        {
            # Fork and disable the async archive flag if this is the parent process
            if (!optionTest(OPTION_TEST_NO_FORK) || !optionGet(OPTION_TEST_NO_FORK))
            {
                $bArchiveAsync = fork() == 0 ? true : false;
            }
            # Else the no-fork flag has been specified for testing
            else
            {
                logDebugMisc($strOperation, 'no fork on archive local for TESTING');
            }
        }
    }

    if ($bArchiveAsync)
    {
        # Start the async archive push
        logDebugMisc($strOperation, 'start async archive-push');

        # Create a lock file to make sure async archive-push does not run more than once
        if (!lockAcquire(commandGet(), false))
        {
            logDebugMisc($strOperation, 'async archive-push process is already running - exiting');
        }
        else
        {
            # Open the log file
            logFileSet(optionGet(OPTION_LOG_PATH) . '/' . optionGet(OPTION_STANZA) . '-archive-async');

            # Call the archive_xfer function and continue to loop as long as there are files to process
            my $iLogTotal;

            while (!defined($iLogTotal) || $iLogTotal > 0)
            {
                $iLogTotal = $self->xfer(optionGet(OPTION_SPOOL_PATH) . "/archive/" .
                                         optionGet(OPTION_STANZA) . "/out", $strStopFile);

                if ($iLogTotal > 0)
                {
                    logDebugMisc($strOperation, "transferred ${iLogTotal} WAL segment" .
                                 ($iLogTotal > 1 ? 's' : '') . ', calling Archive->xfer() again');
                }
                else
                {
                    logDebugMisc($strOperation, 'transfer found 0 WAL segments - exiting');
                }
            }

            lockRelease();
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => 0, trace => true}
    );
}

####################################################################################################################################
# push
####################################################################################################################################
sub push
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourceFile,
        $bAsync
    ) =
        logDebugParam
        (
            OP_ARCHIVE_PUSH, \@_,
            {name => 'strSourceFile'},
            {name => 'bAsync'}
        );

    # Create the file object
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        $bAsync ? optionGet(OPTION_SPOOL_PATH) : optionGet(OPTION_REPO_PATH),
        $bAsync ? NONE : optionRemoteType(),
        protocolGet($bAsync)
    );

    lockStopTest();

    # If the source file path is not absolute then it is relative to the data path
    if (index($strSourceFile, '/',) != 0)
    {
        if (!optionTest(OPTION_DB_PATH))
        {
            confess &log(ERROR, 'option db-path must be set when relative xlog paths are used');
        }

        $strSourceFile = optionGet(OPTION_DB_PATH) . "/${strSourceFile}";
    }

    # Get the destination file
    my $strDestinationFile = basename($strSourceFile);

    # Get the compress flag
    my $bCompress = $bAsync ? false : optionGet(OPTION_COMPRESS);

    # Determine if this is an archive file (don't do compression or checksum on .backup, .history, etc.)
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}(\.partial){0,1}$/ ? true : false;

    # Determine if this is a partial archive file
    my $bPartial = $bArchiveFile && basename($strSourceFile) =~ /\.partial/ ? true : false;

    # Check that there are no issues with pushing this WAL segment
    my $strArchiveId;
    my $strChecksum = undef;

    if (!$bAsync)
    {
        if ($bArchiveFile)
        {
            my ($strDbVersion, $ullDbSysId) = $self->walInfo($strSourceFile);
            ($strArchiveId, $strChecksum) = $self->pushCheck($oFile, substr(basename($strSourceFile), 0, 24), $bPartial,
                                                             $strSourceFile, $strDbVersion, $ullDbSysId);
        }
        else
        {
            $strArchiveId = $self->getCheck($oFile);
        }
    }

    # Only copy the WAL segment if checksum is not defined.  If checksum is defined it means that the WAL segment already exists
    # in the repository with the same checksum (else there would have been an error on checksum mismatch).
    if (!defined($strChecksum))
    {
        # Append compression extension
        if ($bArchiveFile && $bCompress)
        {
            $strDestinationFile .= '.' . $oFile->{strCompressExtension};
        }

        # Copy the WAL segment
        $oFile->copy(PATH_DB_ABSOLUTE, $strSourceFile,                          # Source type/file
                     $bAsync ? PATH_BACKUP_ARCHIVE_OUT : PATH_BACKUP_ARCHIVE,   # Destination type
                     ($bAsync ? '' : "${strArchiveId}/") . $strDestinationFile, # Destination file
                     false,                                                     # Source is not compressed
                     $bArchiveFile && $bCompress,                               # Destination compress is configurable
                     undef, undef, undef,                                       # Unused params
                     true,                                                      # Create path if it does not exist
                     undef, undef,                                              # User and group
                     $bArchiveFile);                                            # Append checksum if archive file
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# pushCheck
####################################################################################################################################
sub pushCheck
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strWalSegment,
        $bPartial,
        $strWalFile,
        $strDbVersion,
        $ullDbSysId
    ) =
        logDebugParam
        (
            OP_ARCHIVE_PUSH_CHECK, \@_,
            {name => 'oFile'},
            {name => 'strWalSegment'},
            {name => 'bPartial'},
            {name => 'strWalFile', required => false},
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'}
        );

    # Set operation and debug strings
    my $strChecksum;
    my $strArchiveId;

    if ($oFile->isRemote(PATH_BACKUP_ARCHIVE))
    {
        # Build param hash
        my %oParamHash;

        $oParamHash{'wal-segment'} = $strWalSegment;
        $oParamHash{'partial'} = $bPartial;
        $oParamHash{'db-version'} = $strDbVersion;
        $oParamHash{'db-sys-id'} = $ullDbSysId;

        # Execute the command
        my $strResult = $oFile->{oProtocol}->cmdExecute(OP_ARCHIVE_PUSH_CHECK, \%oParamHash, true);

        $strArchiveId = (split("\t", $strResult))[0];
        $strChecksum = (split("\t", $strResult))[1];

        if ($strChecksum eq 'Y')
        {
            undef($strChecksum);
        }
    }
    else
    {
        # Create the archive path if it does not exist
        $oFile->pathCreate(PATH_BACKUP_ARCHIVE, undef, undef, true, true);

        # If the info file exists check db version and system-id
        $strArchiveId = (new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE)))->check($strDbVersion, $ullDbSysId);

        # Check if the WAL segment already exists in the archive
        $strChecksum = $self->walFileName($oFile, $strArchiveId, $strWalSegment, $bPartial);

        if (defined($strChecksum))
        {
            $strChecksum = substr($strChecksum, $bPartial ? 33 : 25, 40);
        }
    }

    if (defined($strChecksum) && defined($strWalFile))
    {
        my $strChecksumNew = $oFile->hash(PATH_DB_ABSOLUTE, $strWalFile);

        if ($strChecksumNew ne $strChecksum)
        {
            confess &log(ERROR, "WAL segment ${strWalSegment}" . ($bPartial ? '.partial' : '') .
                                ' already exists in the archive', ERROR_ARCHIVE_DUPLICATE);
        }

        &log(WARN, "WAL segment ${strWalSegment}" . ($bPartial ? '.partial' : '') .
                   " already exists in the archive with the same checksum\n" .
                   "HINT: this is valid in some recovery scenarios but may also indicate a problem");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId},
        {name => 'strChecksum', value => $strChecksum}
    );
}

####################################################################################################################################
# xfer
####################################################################################################################################
sub xfer
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchivePath,
        $strStopFile
    ) =
        logDebugParam
        (
            OP_ARCHIVE_XFER, \@_,
            {name => 'strArchivePath'},
            {name => 'strStopFile'}
        );

    # Create a local file object to read archive logs in the local store
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        NONE,
        protocolGet(true)
    );

    # Load the archive manifest - all the files that need to be pushed
    my %oManifestHash;
    $oFile->manifest(PATH_DB_ABSOLUTE, $strArchivePath, \%oManifestHash);

    # Get all the files to be transferred and calculate the total size
    my @stryFile;
    my $lFileSize = 0;
    my $lFileTotal = 0;

    foreach my $strFile (sort(keys(%{$oManifestHash{name}})))
    {
        if ($strFile =~ "^[0-F]{24}(\\.partial){0,1}(-[0-f]{40})(\\.$oFile->{strCompressExtension}){0,1}\$" ||
            $strFile =~ /^[0-F]{8}\.history$/ || $strFile =~ /^[0-F]{24}\.[0-F]{8}\.backup$/)
        {
            CORE::push(@stryFile, $strFile);

            $lFileSize += $oManifestHash{name}{$strFile}{size};
            $lFileTotal++;
        }
    }

    if ($lFileTotal == 0)
    {
        logDebugMisc($strOperation, 'no WAL segments to archive');

        return 0;
    }
    else
    {
        eval
        {
            # Start backup test point
            &log(TEST, TEST_ARCHIVE_PUSH_ASYNC_START);

            # If the archive repo is remote create a new file object to do the copies
            if (!optionRemoteTypeTest(NONE))
            {
                $oFile = new pgBackRest::File
                (
                    optionGet(OPTION_STANZA),
                    optionGet(OPTION_REPO_PATH),
                    optionRemoteType(),
                    protocolGet()
                );
            }

            # Modify process name to indicate async archiving
            $0 = $^X . ' ' . $0 . " --stanza=" . optionGet(OPTION_STANZA) .
                 "archive-push-async " . $stryFile[0] . '-' . $stryFile[scalar @stryFile - 1];

            # Output files to be moved to backup
            &log(INFO, "WAL segments to archive: total = ${lFileTotal}, size = " . fileSizeFormat($lFileSize));

            # Transfer each file
            foreach my $strFile (sort @stryFile)
            {
                # Construct the archive filename to backup
                my $strArchiveFile = "${strArchivePath}/${strFile}";

                # Determine if the source file is already compressed
                my $bSourceCompressed = $strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$" ? true : false;

                # Determine if this is an archive file (don't want to do compression or checksum on .backup files)
                my $bArchiveFile = basename($strFile) =~
                    "^[0-F]{24}(\\.partial){0,1}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$" ? true : false;

                # Determine if this is a partial archive file
                my $bPartial = $bArchiveFile && basename($strFile) =~ /\.partial/ ? true : false;

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

                logDebugMisc
                (
                    $strOperation, undef,
                    {name => 'strFile', value => $strFile},
                    {name => 'bArchiveFile', value => $bArchiveFile},
                    {name => 'bSourceCompressed', value => $bSourceCompressed},
                    {name => 'bDestinationCompress', value => $bDestinationCompress}
                );

                # Check that there are no issues with pushing this WAL segment
                my $strArchiveId;
                my $strChecksum = undef;

                if ($bArchiveFile)
                {
                    my ($strDbVersion, $ullDbSysId) = $self->walInfo($strArchiveFile);
                    ($strArchiveId, $strChecksum) = $self->pushCheck($oFile, substr(basename($strArchiveFile), 0, 24), $bPartial,
                                                                     $strArchiveFile, $strDbVersion, $ullDbSysId);
                }
                else
                {
                    $strArchiveId = $self->getCheck($oFile);
                }

                # Only copy the WAL segment if checksum is not defined.  If checksum is defined it means that the WAL segment already
                # exists in the repository with the same checksum (else there would have been an error on checksum mismatch).
                if (!defined($strChecksum))
                {
                    # Copy the archive file
                    $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveFile,         # Source path/file
                                 PATH_BACKUP_ARCHIVE,                       # Destination path
                                 "${strArchiveId}/${strDestinationFile}",   # Destination file
                                 $bSourceCompressed,                        # Source is not compressed
                                 $bDestinationCompress,                     # Destination compress is configurable
                                 undef, undef, undef,                       # Unused params
                                 true);                                     # Create path if it does not exist
                }

                #  Remove the source archive file
                unlink($strArchiveFile)
                    or confess &log(ERROR, "copied ${strArchiveFile} to archive successfully but unable to remove it locally.  " .
                                           'This file will need to be cleaned up manually.  If the problem persists, check if ' .
                                           CMD_ARCHIVE_PUSH . ' is being run with different permissions in different contexts.');

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
                &log(ERROR, "local archive queue has exceeded limit of ${iArchiveMaxMB}MB" .
                            " - WAL segments will be discarded until the stop file (${strStopFile}) is removed");

                filePathCreate(dirname($strStopFile), '0770');

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
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lFileTotal', value => $lFileTotal}
    );
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

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchiveStart,
        $strArchiveStop,
        $bSkipFF
    ) =
        logDebugParam
        (
            OP_ARCHIVE_RANGE, \@_,
            {name => 'strArchiveStart'},
            {name => 'strArchiveStop'},
            {name => 'bSkipFF', default => false}
        );

    # Working variables
    my @stryArchive;
    my $iArchiveIdx = 0;

    # Iterate through all archive logs between start and stop
    my @stryArchiveSplit = split('/', $strArchiveStart);
    my $iStartMajor = hex($stryArchiveSplit[0]);
    my $iStartMinor = hex(substr(sprintf("%08s", $stryArchiveSplit[1]), 0, 2));

    @stryArchiveSplit = split('/', $strArchiveStop);
    my $iStopMajor = hex($stryArchiveSplit[0]);
    my $iStopMinor = hex(substr(sprintf("%08s", $stryArchiveSplit[1]), 0, 2));

    $stryArchive[$iArchiveIdx] = uc(sprintf("%08x%08x", $iStartMajor, $iStartMinor));
    $iArchiveIdx += 1;

    while (!($iStartMajor == $iStopMajor && $iStartMinor == $iStopMinor))
    {
        $iStartMinor += 1;

        if ($bSkipFF && $iStartMinor == 255 || !$bSkipFF && $iStartMinor == 256)
        {
            $iStartMajor += 1;
            $iStartMinor = 0;
        }

        $stryArchive[$iArchiveIdx] = uc(sprintf("%08x%08x", $iStartMajor, $iStartMinor));
        $iArchiveIdx += 1;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryArchive', value => \@stryArchive}
    );
}

####################################################################################################################################
# check
#
# Validates the database configuration and checks that the archive logs can be read by backup. This will alert the user to any
# misconfiguration, particularly of archiving, that would result in the inability of a backup to complete (e.g waiting at the end
# until it times out because it could not find the WAL file).
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->check');

    # Initialize default file object
    $self->{oFile} = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        optionRemoteType(),
        protocolGet()
    );

    # Initialize the database object
    $self->{oDb} = new pgBackRest::Db();

    # Validate the database configuration
    $self->{oDb}->configValidate($self->{oFile}, optionGet(OPTION_DB_PATH));

    # Force archiving
    my $strWalSegment = $self->{oDb}->xlogSwitch();

    # Get the timeout and error message to display - if it is 0 we are testing
    my $iArchiveTimeout = optionGet(OPTION_ARCHIVE_TIMEOUT);

    # Initialize the result variables
    my $iResult = 0;
    my $strResultMessage = undef;

    # Record the start time to wait for the archive.info file to be written
    my $oWait = waitInit($iArchiveTimeout);

    my $strArchiveId = undef;
    my $strArchiveFile = undef;

    # Turn off console logging to control when to display the error
    logLevelSet(undef, OFF);

    # Wait for the archive.info to be written. If it does not get written within the timout period then report the last error.
    do
    {
        eval
        {
            $strArchiveId = $self->getCheck($self->{oFile});

            # Clear any previous errors if we've found the archive.info
            $iResult = 0;
        };

        if ($@)
        {
            my $oMessage = $@;

            # If this is a backrest error then capture the last code and message else confess
            if (blessed($oMessage) && $oMessage->isa('pgBackRest::Common::Exception'))
            {
                $iResult = $oMessage->code();
                $strResultMessage = $oMessage->message();
            }
            else
            {
                confess $oMessage;
            }
        }
    } while (!defined($strArchiveId) && waitMore($oWait));

    # If able to get the archive id then check the archived WAL file with the time remaining
    if ($iResult == 0)
    {
        eval
        {
            $strArchiveFile = $self->walFileName($self->{oFile}, $strArchiveId, $strWalSegment, false, $iArchiveTimeout);
        };

        # If this is a backrest error then capture the last code and message else confess
        if ($@)
        {
            my $oMessage = $@;

            # If a backrest exception then return the code else confess
            if (blessed($oMessage) && $oMessage->isa('pgBackRest::Common::Exception'))
            {
                $iResult = $oMessage->code();
                $strResultMessage = $oMessage->message();
            }
            else
            {
                confess $oMessage;
            }
        }
    }

    # Reset the console logging
    logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));

    # Display results
    if ($iResult == 0)
    {
        &log(INFO,
            "WAL segment ${strWalSegment} successfully stored in the archive at '" .
            $self->{oFile}->pathGet(PATH_BACKUP_ARCHIVE, "$strArchiveId/${strArchiveFile}") . "'");
    }
    else
    {
        &log(ERROR, $strResultMessage, $iResult);
        &log(WARN,
            "WAL segment ${strWalSegment} did not reach the archive:\n" .
            "HINT: Check the archive_command to ensure that all options are correct (especialy --stanza).\n" .
            "HINT: Check the PostreSQL server log for errors.");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true}
    );

}

1;
