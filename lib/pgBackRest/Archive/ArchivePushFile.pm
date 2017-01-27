####################################################################################################################################
# ARCHIVE PUSH FILE MODULE
####################################################################################################################################
package pgBackRest::Archive::ArchivePushFile;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename);

use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Protocol::Common;

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
        $oFile,
        $strArchiveFile,
        $strDbVersion,
        $ullDbSysId,
        $strWalFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::archivePushCheck', \@_,
            {name => 'oFile'},
            {name => 'strArchiveFile'},
            {name => 'strDbVersion', required => false},
            {name => 'ullDbSysId', required => false},
            {name => 'strWalFile', required => false},
        );

    # Set operation and debug strings
    my $strArchiveId;
    my $strChecksum;

    # WAL file is segment?
    my $bWalSegment = walIsSegment($strArchiveFile);

    if ($oFile->isRemote(PATH_BACKUP_ARCHIVE))
    {
        # Execute the command
        ($strArchiveId, $strChecksum) = $oFile->{oProtocol}->cmdExecute(
            OP_ARCHIVE_PUSH_CHECK, [$strArchiveFile, $strDbVersion, $ullDbSysId], true);
    }
    else
    {
        # If a segment check db version and system-id
        if ($bWalSegment)
        {
            # If the info file exists check db version and system-id else error
            $strArchiveId = (new pgBackRest::Archive::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE)))->check(
                $strDbVersion, $ullDbSysId);

            # Check if the WAL segment already exists in the archive
            my $strFoundFile = walSegmentFind($oFile, $strArchiveId, $strArchiveFile);

            if (defined($strFoundFile))
            {
                $strChecksum = substr($strFoundFile, length($strArchiveFile) + 1, 40);
            }
        }
        # Else just get the archive id
        else
        {
            $strArchiveId = (new pgBackRest::Archive::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE)))->archiveId();
        }
    }

    my $strWarning;

    if (defined($strChecksum) && !commandTest(CMD_REMOTE))
    {
        my $strChecksumNew = $oFile->hash(PATH_DB_ABSOLUTE, $strWalFile);

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
        $oFile,
        $strWalPath,
        $strWalFile,
        $bCompress,
        $bRepoSync,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::archivePushFile', \@_,
            {name => 'oFile'},
            {name => 'strWalPath'},
            {name => 'strWalFile'},
            {name => 'bCompress'},
            {name => 'bRepoSync'},
        );

    # Get cluster info from the WAL
    my $strDbVersion;
    my $ullDbSysId;

    if (walIsSegment($strWalFile))
    {
        ($strDbVersion, $ullDbSysId) = walInfo("${strWalPath}/${strWalFile}");
    }

    # Check if the WAL already exists in the repo
    my ($strArchiveId, $strChecksum, $strWarning) = archivePushCheck(
        $oFile, $strWalFile, $strDbVersion, $ullDbSysId, walIsSegment($strWalFile) ? "${strWalPath}/${strWalFile}" : undef);

    # Only copy the WAL segment if checksum is not defined.  If checksum is defined it means that the WAL segment already exists
    # in the repository with the same checksum (else there would have been an error on checksum mismatch).
    if (!defined($strChecksum))
    {
        my $strArchiveFile = "${strArchiveId}/${strWalFile}";

        # Append compression extension
        if (walIsSegment($strWalFile) && $bCompress)
        {
            $strArchiveFile .= '.' . $oFile->{strCompressExtension};
        }

        # Copy the WAL segment
        $oFile->copy(
            PATH_DB_ABSOLUTE, "${strWalPath}/${strWalFile}",        # Source type/file
            PATH_BACKUP_ARCHIVE, $strArchiveFile,                   # Destination type/file
            false,                                                  # Source is not compressed
            walIsSegment($strWalFile) && $bCompress,                # Destination compress is configurable
            undef, undef, undef,                                    # Unused params
            true,                                                   # Create path if it does not exist
            undef, undef,                                           # Default User and group
            walIsSegment($strWalFile),                              # Append checksum if WAL segment
            $bRepoSync);                                            # Sync repo directories?
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
