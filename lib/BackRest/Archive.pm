####################################################################################################################################
# ARCHIVE MODULE
####################################################################################################################################
package BackRest::Archive;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY O_WRONLY O_CREAT);
use File::Basename qw(dirname basename);

use lib dirname($0);
use BackRest::Common::Exception;
use BackRest::Common::Ini;
use BackRest::Common::Lock;
use BackRest::Common::Log;
use BackRest::ArchiveInfo;
use BackRest::Common::String;
use BackRest::Common::Wait;
use BackRest::Config;
use BackRest::File;

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
        $iWaitSeconds
    ) =
        logDebugParam
        (
            OP_ARCHIVE_WAL_FILE_NAME, \@_,
            {name => 'oFile'},
            {name => 'strArchiveId'},
            {name => 'strWalSegment'},
            {name => 'iWaitSeconds', required => false}
        );

    # Record the start time
    my $oWait = waitInit($iWaitSeconds);

    # Determine the path where the requested WAL segment is located
    my $strArchivePath = dirname($oFile->pathGet(PATH_BACKUP_ARCHIVE, "$strArchiveId/${strWalSegment}"));
    my @stryWalFileName;

    do
    {
        # Get the name of the requested WAL segment (may have hash info and compression extension)
        @stryWalFileName = $oFile->list(PATH_BACKUP_ABSOLUTE, $strArchivePath,
            "^${strWalSegment}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$", undef, true);

        # If there is more than one matching archive file then there is a serious issue - likely a bug in the archiver
        if (@stryWalFileName > 1)
        {
            confess &log(ASSERT, @stryWalFileName . " duplicate files found for ${strWalSegment}", ERROR_ARCHIVE_DUPLICATE);
        }
    }
    while (@stryWalFileName == 0 && waitMore($oWait));

    # If waiting and no WAL segment was found then throw an error
    if (@stryWalFileName == 0 && defined($iWaitSeconds))
    {
        confess &log(ERROR, "could not find WAL segment ${strWalSegment} after " . waitInterval($oWait)  . ' second(s)');
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

    if ($iMagic == hex('0xD085'))
    {
        $strDbVersion = '9.5';
        $iSysIdOffset = 20;
    }
    elsif ($iMagic == hex('0xD07E'))
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

    # Make sure that the long header is present or there won't be a system id
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

    # Create the file object
    my $oFile = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        optionRemoteTypeTest(BACKUP) ? optionGet(OPTION_REPO_REMOTE_PATH) : optionGet(OPTION_REPO_PATH),
        optionRemoteType(),
        protocolGet()
    );

    # If the destination file path is not absolute then it is relative to the db data path
    if (index($strDestinationFile, '/',) != 0)
    {
        # !!! If db-path is required this is can be removed.
        # !!! db-path should be added as a requirement for the remote settings work.
        if (!optionTest(OPTION_DB_PATH))
        {
            confess &log(ERROR, 'option db-path must be set when relative xlog paths are used');
        }

        $strDestinationFile = optionGet(OPTION_DB_PATH) . "/${strDestinationFile}";
    }

    # Get the wal segment filename
    my $strArchiveId = $self->getCheck($oFile);
    my $strArchiveFile = $self->walFileName($oFile, $strArchiveId, $strSourceArchive);

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
        $strArchiveId = (new BackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), true))->archiveId();
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
    use BackRest::Archive;

    # If an archive section has been defined, use that instead of the backup section when command is CMD_ARCHIVE_PUSH
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

        &log(INFO, 'push WAL segment ' . $ARGV[1] . ($bArchiveAsync ? ' asynchronously' : ''));

        $self->push($ARGV[1], $bArchiveAsync);

        # Fork is async archiving is enabled
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
            logFileSet(optionGet(OPTION_REPO_PATH) . '/log/' . optionGet(OPTION_STANZA) . '-archive-async');

            # Call the archive_xfer function and continue to loop as long as there are files to process
            my $iLogTotal;

            while (!defined($iLogTotal) || $iLogTotal > 0)
            {
                $iLogTotal = $self->xfer($strArchivePath . "/archive/" . optionGet(OPTION_STANZA) . "/out", $strStopFile);

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
    my $oFile = new BackRest::File
    (
        optionGet(OPTION_STANZA),
        $bAsync || optionRemoteTypeTest(NONE) ? optionGet(OPTION_REPO_PATH) : optionGet(OPTION_REPO_REMOTE_PATH),
        $bAsync ? NONE : optionRemoteType(),
        protocolGet($bAsync)
    );

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
    my $bArchiveFile = basename($strSourceFile) =~ /^[0-F]{24}$/ ? true : false;

    # Check that there are no issues with pushing this WAL segment
    my $strArchiveId;
    my $strChecksum = undef;

    if (!$bAsync)
    {
        if ($bArchiveFile)
        {
            my ($strDbVersion, $ullDbSysId) = $self->walInfo($strSourceFile);
            ($strArchiveId, $strChecksum) = $self->pushCheck($oFile, substr(basename($strSourceFile), 0, 24), $strSourceFile,
                                                             $strDbVersion, $ullDbSysId);
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
        $strWalFile,
        $strDbVersion,
        $ullDbSysId
    ) =
        logDebugParam
        (
            OP_ARCHIVE_PUSH_CHECK, \@_,
            {name => 'oFile'},
            {name => 'strWalSegment'},
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
        if (!$oFile->exists(PATH_BACKUP_ARCHIVE))
        {
            $oFile->pathCreate(PATH_BACKUP_ARCHIVE);
        }

        # If the info file exists check db version and system-id
        $strArchiveId = (new BackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE)))->check($strDbVersion, $ullDbSysId);

        # Check if the WAL segment already exists in the archive
        $strChecksum = $self->walFileName($oFile, $strArchiveId, $strWalSegment);

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
    my $oFile = new BackRest::File
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
        logDebugMisc($strOperation, 'no WAL segments to archive');

        return 0;
    }
    else
    {
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
                    ($strArchiveId, $strChecksum) = $self->pushCheck($oFile, substr(basename($strArchiveFile), 0, 24),
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

    # Get the timelines and make sure they match
    my $strTimeline = substr($strArchiveStart, 0, 8);
    my @stryArchive;
    my $iArchiveIdx = 0;

    if ($strTimeline ne substr($strArchiveStop, 0, 8))
    {
        confess &log(ERROR, "timelines differ between ${strArchiveStart} and ${strArchiveStop}");
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryArchive', value => \@stryArchive}
    );
}

1;
