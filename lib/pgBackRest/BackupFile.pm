####################################################################################################################################
# BACKUP FILE MODULE
####################################################################################################################################
package pgBackRest::BackupFile;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;

####################################################################################################################################
# Result constants
####################################################################################################################################
use constant BACKUP_FILE_CHECKSUM                                   => 0;
    push @EXPORT, qw(BACKUP_FILE_CHECKSUM);
use constant BACKUP_FILE_COPY                                       => 1;
    push @EXPORT, qw(BACKUP_FILE_COPY);
use constant BACKUP_FILE_RECOPY                                     => 2;
    push @EXPORT, qw(BACKUP_FILE_RECOPY);
use constant BACKUP_FILE_SKIP                                       => 3;
    push @EXPORT, qw(BACKUP_FILE_SKIP);

####################################################################################################################################
# backupFile
####################################################################################################################################
sub backupFile
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,                                     # File object
        $strDbFile,                                 # Database file to backup
        $strRepoFile,                               # Location in the repository to copy to
        $bDestinationCompress,                      # Compress destination file
        $strChecksum,                               # File checksum to be checked
        $lModificationTime,                         # File modification time
        $lSizeFile,                                 # File size
        $bIgnoreMissing,                            # Is it OK if the file is missing?
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupFile', \@_,
            {name => 'oFile', trace => true},
            {name => OP_PARAM_DB_FILE, trace => true},
            {name => OP_PARAM_REPO_FILE, trace => true},
            {name => OP_PARAM_DESTINATION_COMPRESS, trace => true},
            {name => OP_PARAM_CHECKSUM, required => false, trace => true},
            {name => OP_PARAM_MODIFICATION_TIME, trace => true},
            {name => OP_PARAM_SIZE, trace => true},
            {name => OP_PARAM_IGNORE_MISSING, default => true, trace => true},
        );

    my $iCopyResult = BACKUP_FILE_COPY;             # Copy result
    my $strCopyChecksum;                            # Copy checksum
    my $lCopySize;                                  # Copy Size
    my $lRepoSize;                                  # Repo size

    # If checksum is defined then the file already exists but needs to be checked
    my $bCopy = true;

    # Add compression suffix if needed
    my $strFileOp = $strRepoFile . ($bDestinationCompress ? '.' . $oFile->{strCompressExtension} : '');

    if (defined($strChecksum))
    {
        ($strCopyChecksum, $lCopySize) =
            $oFile->hashSize(PATH_BACKUP_TMP, $strFileOp, $bDestinationCompress);

        $bCopy = !($strCopyChecksum eq $strChecksum && $lCopySize == $lSizeFile);

        if ($bCopy)
        {
            $iCopyResult = BACKUP_FILE_RECOPY;
        }
        else
        {
            $iCopyResult = BACKUP_FILE_CHECKSUM;
        }
    }

    if ($bCopy)
    {
        # Copy the file from the database to the backup (will return false if the source file is missing)
        (my $bCopyResult, $strCopyChecksum, $lCopySize) =
            $oFile->copy(PATH_DB_ABSOLUTE, $strDbFile,
                         PATH_BACKUP_TMP, $strFileOp,
                         false,                   # Source is not compressed since it is the db directory
                         $bDestinationCompress,   # Destination should be compressed based on backup settings
                         $bIgnoreMissing,         # Ignore missing files
                         $lModificationTime,      # Set modification time - this is required for resume
                         undef,                   # Do not set original mode
                         true);                   # Create the destination directory if it does not exist

        # If source file is missing then assume the database removed it (else corruption and nothing we can do!)
        if (!$bCopyResult)
        {
            $iCopyResult = BACKUP_FILE_SKIP;
        }
    }

    # If file was copied or checksum'd then get size in repo.  This has to be checked after the file is at rest because filesystem
    # compression may affect the actual repo size and this cannot be calculated in stream.
    if ($iCopyResult == BACKUP_FILE_COPY || $iCopyResult == BACKUP_FILE_RECOPY || $iCopyResult == BACKUP_FILE_CHECKSUM)
    {
        $lRepoSize = (fileStat($oFile->pathGet(PATH_BACKUP_TMP, $strFileOp)))->size;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iCopyResult', value => $iCopyResult, trace => true},
        {name => 'lCopySize', value => $lCopySize, trace => true},
        {name => 'lRepoSize', value => $lRepoSize, trace => true},
        {name => 'strCopyChecksum', value => $strCopyChecksum, trace => true}
    );
}

push @EXPORT, qw(backupFile);

####################################################################################################################################
# backupManifestUpdate
####################################################################################################################################
sub backupManifestUpdate
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oManifest,
        $strHost,
        $iLocalId,
        $strRepoFile,
        $strDbFile,
        $iCopyResult,
        $lSize,
        $lSizeCopy,
        $lSizeRepo,
        $lSizeTotal,
        $lSizeCurrent,
        $strChecksum,
        $strChecksumCopy,
        $lManifestSaveSize,
        $lManifestSaveCurrent
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupManifestUpdate', \@_,
            {name => 'oManifest', trace => true},
            {name => 'strHost', required => false, trace => true},
            {name => 'iLocalId', required => false, trace => true},
            {name => 'strRepoFile', trace => true},
            {name => 'strDbFile', trace => true},
            {name => 'iCopyResult', trace => true},
            {name => 'lSize', required => false, trace => true},
            {name => 'lSizeCopy', required => false, trace => true},
            {name => 'lSizeRepo', required => false, trace => true},
            {name => 'lSizeTotal', trace => true},
            {name => 'lSizeCurrent', trace => true},
            {name => 'strChecksum', required => false, trace => true},
            {name => 'strChecksumCopy', required => false, trace => true},
            {name => 'lManifestSaveSize', trace => true},
            {name => 'lManifestSaveCurrent', trace => true}
        );

    # Increment current backup progress
    $lSizeCurrent += $lSize;

    # Log invalid checksum
    if ($iCopyResult == BACKUP_FILE_RECOPY)
    {
        &log(
            WARN,
            "resumed backup file ${strRepoFile} should have checksum ${strChecksum} but actually has checksum ${strChecksumCopy}." .
            " The file will be recopied and backup will continue but this may be an issue unless the backup temp path is known to" .
            " be corrupted.");
    }

    # If copy was successful store the checksum and size
    if ($iCopyResult == BACKUP_FILE_COPY || $iCopyResult == BACKUP_FILE_RECOPY || $iCopyResult == BACKUP_FILE_CHECKSUM)
    {
        # Log copy or checksum
        &log($iCopyResult == BACKUP_FILE_CHECKSUM ? DETAIL : INFO,
             ($iCopyResult == BACKUP_FILE_CHECKSUM ?
                'checksum resumed file ' : 'backup file ' . (defined($strHost) ? "${strHost}:" : '')) .
             "${strDbFile} (" . fileSizeFormat($lSizeCopy) .
             ', ' . int($lSizeCurrent * 100 / $lSizeTotal) . '%)' .
             ($lSizeCopy != 0 ? " checksum ${strChecksumCopy}" : ''), undef, undef, undef, $iLocalId);

        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_SIZE, $lSizeCopy);

        if ($lSizeRepo != $lSizeCopy)
        {
            $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REPO_SIZE, $lSizeRepo);
        }

        if ($lSizeCopy > 0)
        {
            $oManifest->set(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksumCopy);
        }
    }
    # Else the file was removed during backup so remove from manifest
    elsif ($iCopyResult == BACKUP_FILE_SKIP)
    {
        &log(DETAIL, 'skip file removed by database ' . (defined($strHost) ? "${strHost}:" : '') . $strDbFile);
        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strRepoFile);
    }

    # Determine whether to save the manifest
    $lManifestSaveCurrent += $lSize;

    if ($lManifestSaveCurrent >= $lManifestSaveSize)
    {
        $oManifest->save();
        logDebugMisc
        (
            $strOperation, 'save manifest',
            {name => 'lManifestSaveSize', value => $lManifestSaveSize},
            {name => 'lManifestSaveCurrent', value => $lManifestSaveCurrent}
        );

        $lManifestSaveCurrent = 0;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lSizeCurrent', value => $lSizeCurrent, trace => true},
        {name => 'lManifestSaveCurrent', value => $lManifestSaveCurrent, trace => true},
    );
}

push @EXPORT, qw(backupManifestUpdate);

1;
