####################################################################################################################################
# BACKUP FILE MODULE
####################################################################################################################################
package pgBackRest::BackupFile;

use threads;
use Thread::Queue;
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
        $lSizeTotal,                                # Total size of the files to be copied
        $lSizeCurrent,                              # Size of files copied so far
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupFile', \@_,
            {name => 'oFile', trace => true},
            {name => 'strDbFile', trace => true},
            {name => 'strRepoFile', trace => true},
            {name => 'bDestinationCompress', trace => true},
            {name => 'strChecksum', required => false, trace => true},
            {name => 'lModificationTime', trace => true},
            {name => 'lSizeFile', trace => true},
            {name => 'lSizeTotal', default => 0, trace => true},
            {name => 'lSizeCurrent', required => false, trace => true}
        );

    my $bCopyResult = true;                         # Copy result
    my $strCopyChecksum;                            # Copy checksum
    my $lCopySize;                                  # Copy Size
    my $lRepoSize;                                  # Repo size

    # Add the size of the current file to keep track of percent complete
    $lSizeCurrent += $lSizeFile;

    # If checksum is defined then the file already exists but needs to be checked
    my $bCopy = true;

    # See if there is a host
    my $strHost = $oFile->{oProtocol}->{strHost};

    # Add compression suffix if needed
    my $strFileOp = $strRepoFile . ($bDestinationCompress ? '.' . $oFile->{strCompressExtension} : '');

    if (defined($strChecksum))
    {
        ($strCopyChecksum, $lCopySize) =
            $oFile->hashSize(PATH_BACKUP_TMP, $strFileOp, $bDestinationCompress);

        $bCopy = !($strCopyChecksum eq $strChecksum && $lCopySize == $lSizeFile);

        if ($bCopy)
        {
            &log(WARN, "resumed backup file ${strRepoFile} should have checksum ${strChecksum} but " .
                       "actually has checksum ${strCopyChecksum}.  The file will be recopied and backup will " .
                       "continue but this may be an issue unless the backup temp path is known to be corrupted.");
        }
    }

    if ($bCopy)
    {
        # Copy the file from the database to the backup (will return false if the source file is missing)
        ($bCopyResult, $strCopyChecksum, $lCopySize) =
            $oFile->copy(PATH_DB_ABSOLUTE, $strDbFile,
                         PATH_BACKUP_TMP, $strFileOp,
                         false,                   # Source is not compressed since it is the db directory
                         $bDestinationCompress,   # Destination should be compressed based on backup settings
                         true,                    # Ignore missing files
                         $lModificationTime,      # Set modification time - this is required for resume
                         undef,                   # Do not set original mode
                         true);                   # Create the destination directory if it does not exist

        # If source file is missing then assume the database removed it (else corruption and nothing we can do!)
        if (!$bCopyResult)
        {
            &log(DETAIL, "skip file removed by database: " . $strDbFile);
        }
    }

    # If file was copied or checksum'd then get size in repo.  This has to be checked after the file is at rest because filesystem
    # compression may affect the actual repo size and this cannot be calculated in stream.
    if ($bCopyResult)
    {
        $lRepoSize = (fileStat($oFile->pathGet(PATH_BACKUP_TMP, $strFileOp)))->size;
    }

    # Ouput log
    if ($bCopyResult)
    {
        &log($bCopy ? INFO : DETAIL,
             (defined($strChecksum) && !$bCopy ?
                'checksum resumed file ' : 'backup file ' . (defined($strHost) ? "${strHost}:" : '')) .
             "${strDbFile} (" . fileSizeFormat($lCopySize) .
             ($lSizeTotal > 0 ? ', ' . int($lSizeCurrent * 100 / $lSizeTotal) . '%' : '') . ')' .
             ($lCopySize != 0 ? " checksum ${strCopyChecksum}" : ''));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bCopyResult', value => $bCopyResult, trace => true},
        {name => 'lSizeCurrent', value => $lSizeCurrent, trace => true},
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
        $strFile,
        $bCopied,
        $lSize,
        $lRepoSize,
        $strChecksum,
        $lManifestSaveSize,
        $lManifestSaveCurrent
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupManifestUpdate', \@_,
            {name => 'oManifest', trace => true},
            {name => 'strFile', trace => true},
            {name => 'bCopied', trace => true},
            {name => 'lSize', required => false, trace => true},
            {name => 'lRepoSize', required => false, trace => true},
            {name => 'strChecksum', required => false, trace => true},
            {name => 'lManifestSaveSize', required => false, trace => true},
            {name => 'lManifestSaveCurrent', required => false, trace => true}
        );

    # If copy was successful store the checksum and size
    if ($bCopied)
    {
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_SIZE, $lSize);

        if ($lRepoSize != $lSize)
        {
            $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_REPO_SIZE, $lRepoSize);
        }

        if ($lSize > 0)
        {
            $oManifest->set(MANIFEST_SECTION_TARGET_FILE, $strFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksum);
        }

        # Determine whether to save the manifest
        if (defined($lManifestSaveSize))
        {
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
        }
    }
    # Else the file was removed during backup so remove from manifest
    else
    {
        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strFile);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'lManifestSaveCurrent', value => $lManifestSaveCurrent, trace => true}
    );
}

push @EXPORT, qw(backupManifestUpdate);

1;
