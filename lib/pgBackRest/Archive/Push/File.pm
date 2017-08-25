####################################################################################################################################
# ARCHIVE PUSH FILE MODULE
####################################################################################################################################
package pgBackRest::Archive::Push::File;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename dirname);

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Filter::Gzip;
use pgBackRest::Storage::Filter::Sha;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# archivePushCheck
#
# Check that a WAL segment does not already exist in the archive be pushing.  Files that are not segments (e.g. .history, .backup)
# will always be reported as not present and will be overwritten by archivePushFile().
####################################################################################################################################
sub archivePushCheck
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchiveFile,
        $strDbVersion,
        $ullDbSysId,
        $strWalFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::archivePushCheck', \@_,
            {name => 'strArchiveFile'},
            {name => 'strDbVersion', required => false},
            {name => 'ullDbSysId', required => false},
            {name => 'strWalFile', required => false},
        );

    # Set operation and debug strings
    my $oStorageRepo = storageRepo();
    my $strArchiveId;
    my $strChecksum;

    # WAL file is segment?
    my $bWalSegment = walIsSegment($strArchiveFile);

    if (!isRepoLocal())
    {
        # Execute the command
        ($strArchiveId, $strChecksum) = protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP)->cmdExecute(
            OP_ARCHIVE_PUSH_CHECK, [$strArchiveFile, $strDbVersion, $ullDbSysId], true);
    }
    else
    {
        # If a segment check db version and system-id
        if ($bWalSegment)
        {
            # If the info file exists check db version and system-id else error
            $strArchiveId = (new pgBackRest::Archive::Info(
                $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE)))->check($strDbVersion, $ullDbSysId);

            # Check if the WAL segment already exists in the archive
            my $strFoundFile = walSegmentFind($oStorageRepo, $strArchiveId, $strArchiveFile);

            if (defined($strFoundFile))
            {
                $strChecksum = substr($strFoundFile, length($strArchiveFile) + 1, 40);
            }
        }
        # Else just get the archive id
        else
        {
            $strArchiveId = (new pgBackRest::Archive::Info($oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE)))->archiveId();
        }
    }

    my $strWarning;

    if (defined($strChecksum) && !cfgCommandTest(CFGCMD_REMOTE))
    {
        my ($strChecksumNew) = storageDb()->hashSize($strWalFile);

        if ($strChecksumNew ne $strChecksum)
        {
            confess &log(ERROR, "WAL segment " . basename($strWalFile) . " already exists in the archive", ERROR_ARCHIVE_DUPLICATE);
        }

        $strWarning =
            "WAL segment " . basename($strWalFile) . " already exists in the archive with the same checksum\n" .
            "HINT: this is valid in some recovery scenarios but may also indicate a problem.";

        &log(WARN, $strWarning);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId},
        {name => 'strChecksum', value => $strChecksum},
        {name => 'strWarning', value => $strWarning}
    );
}

push @EXPORT, qw(archivePushCheck);

####################################################################################################################################
# archivePushFile
#
# Copy a file from the WAL directory to the archive.
####################################################################################################################################
sub archivePushFile
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strWalPath,
        $strWalFile,
        $bCompress,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::archivePushFile', \@_,
            {name => 'strWalPath'},
            {name => 'strWalFile'},
            {name => 'bCompress'},
        );

    # Get cluster info from the WAL
    my $oStorageRepo = storageRepo();
    my $strDbVersion;
    my $ullDbSysId;

    if (walIsSegment($strWalFile))
    {
        ($strDbVersion, $ullDbSysId) = walInfo("${strWalPath}/${strWalFile}");
    }

    # Check if the WAL already exists in the repo
    my ($strArchiveId, $strChecksum, $strWarning) = archivePushCheck(
        $strWalFile, $strDbVersion, $ullDbSysId, walIsSegment($strWalFile) ? "${strWalPath}/${strWalFile}" : undef);

    # Only copy the WAL segment if checksum is not defined.  If checksum is defined it means that the WAL segment already exists
    # in the repository with the same checksum (else there would have been an error on checksum mismatch).
    if (!defined($strChecksum))
    {
        my $strArchiveFile = "${strArchiveId}/${strWalFile}";

        # If a WAL segment
        if (walIsSegment($strWalFile))
        {
            # Get hash
            my ($strSourceHash) = storageDb()->hashSize("${strWalPath}/${strWalFile}");

            $strArchiveFile .= "-${strSourceHash}";

            # Add compress extension
            if ($bCompress)
            {
                $strArchiveFile .= qw{.} . COMPRESS_EXT;
            }
        }

        # Copy
        $oStorageRepo->copy(
            storageDb()->openRead("${strWalPath}/${strWalFile}",
                {rhyFilter => walIsSegment($strWalFile) && $bCompress ? [{strClass => STORAGE_FILTER_GZIP}] : undef}),
            $oStorageRepo->openWrite(
                STORAGE_REPO_ARCHIVE . "/${strArchiveFile}",
                {bPathCreate => true, bAtomic => true, bProtocolCompress => !walIsSegment($strWalFile) || !$bCompress}));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strWarning', value => $strWarning}
    );
}

push @EXPORT, qw(archivePushFile);

1;
