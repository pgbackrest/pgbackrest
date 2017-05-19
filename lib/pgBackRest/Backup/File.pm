####################################################################################################################################
# BACKUP FILE MODULE
####################################################################################################################################
package pgBackRest::Backup::File;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::Filter::PageChecksum;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Filter::Gzip;
use pgBackRest::Storage::Filter::Sha;
use pgBackRest::Storage::Helper;

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
        $strDbFile,                                 # Database file to backup
        $strRepoFile,                               # Location in the repository to copy to
        $lSizeFile,                                 # File size
        $strChecksum,                               # File checksum to be checked
        $bChecksumPage,                             # Should page checksums be calculated?
        $bDestinationCompress,                      # Compress destination file
        $lModificationTime,                         # File modification time
        $bIgnoreMissing,                            # Is it OK if the file is missing?
        $hExtraParam,                               # Parameter to pass to the extra function
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupFile', \@_,
            {name => 'strDbFile', trace => true},
            {name => 'strRepoFile', trace => true},
            {name => 'lSizeFile', trace => true},
            {name => 'strChecksum', required => false, trace => true},
            {name => 'bChecksumPage', trace => true},
            {name => 'bDestinationCompress', trace => true},
            {name => 'lModificationTime', trace => true},
            {name => 'bIgnoreMissing', default => true, trace => true},
            {name => 'hExtraParam', required => false, trace => true},
        );

    my $oStorageRepo = storageRepo();               # Repo storage
    my $iCopyResult = BACKUP_FILE_COPY;             # Copy result
    my $strCopyChecksum;                            # Copy checksum
    my $rExtra;                                     # Page checksum result
    my $lCopySize;                                  # Copy Size
    my $lRepoSize;                                  # Repo size

    # If checksum is defined then the file already exists but needs to be checked
    my $bCopy = true;

    # Add compression suffix if needed
    my $strFileOp = $strRepoFile . ($bDestinationCompress ? '.' . COMPRESS_EXT : '');

    if (defined($strChecksum))
    {
        ($strCopyChecksum, $lCopySize) =
            $oStorageRepo->hashSize(STORAGE_REPO_BACKUP_TMP . "/${strFileOp}", $bDestinationCompress);

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

    # Copy the file
    if ($bCopy)
    {
        # Add sha filter
        my $rhyFilter = [{strClass => STORAGE_FILTER_SHA}];

        # Add page checksum filter
        if ($bChecksumPage)
        {
            # Determine which segment no this is by checking for a numeric extension.  No extension means segment 0.
            my $iSegmentNo = ($strDbFile =~ /\.[0-9]+$/) ? substr(($strDbFile =~ m/\.[0-9]+$/g)[0], 1) + 0 : 0;

            push(
                @{$rhyFilter},
                {strClass => BACKUP_FILTER_PAGECHECKSUM,
                    rxyParam => [$iSegmentNo, $hExtraParam->{iWalId}, $hExtraParam->{iWalOffset}]});
        };

        # Open the file
        my $oSourceFileIo = storageDb()->openRead($strDbFile, {rhyFilter => $rhyFilter, bIgnoreMissing => true});

        # If source file exists
        if (defined($oSourceFileIo))
        {
            my $oDestinationFileIo = $oStorageRepo->openWrite(
                STORAGE_REPO_BACKUP_TMP . "/${strFileOp}",
                {rhyFilter => $bDestinationCompress ? [{strClass => STORAGE_FILTER_GZIP}] : undef, bPathCreate => true});

            # Copy the file
            $oStorageRepo->copy($oSourceFileIo, $oDestinationFileIo);

            # Get sha checksum and size
            $strCopyChecksum = $oSourceFileIo->result(STORAGE_FILTER_SHA);
            $lCopySize = $oSourceFileIo->result(COMMON_IO_HANDLE);

            # Get results of page checksum validation
            $rExtra = $bChecksumPage ? $oSourceFileIo->result(BACKUP_FILTER_PAGECHECKSUM) : undef;
        }
        # Else if source file is missing the database removed it
        else
        {
            $iCopyResult = BACKUP_FILE_SKIP;
        }
    }

    # If file was copied or checksum'd then get size in repo.  This has to be checked after the file is at rest because filesystem
    # compression may affect the actual repo size and this cannot be calculated in stream.
    if ($iCopyResult == BACKUP_FILE_COPY || $iCopyResult == BACKUP_FILE_RECOPY || $iCopyResult == BACKUP_FILE_CHECKSUM)
    {
        $lRepoSize = ($oStorageRepo->info(STORAGE_REPO_BACKUP_TMP . "/${strFileOp}"))->size;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iCopyResult', value => $iCopyResult, trace => true},
        {name => 'lCopySize', value => $lCopySize, trace => true},
        {name => 'lRepoSize', value => $lRepoSize, trace => true},
        {name => 'strCopyChecksum', value => $strCopyChecksum, trace => true},
        {name => 'rExtra', value => $rExtra, trace => true},
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
        $strDbFile,
        $strRepoFile,
        $lSize,
        $strChecksum,
        $bChecksumPage,
        $iCopyResult,
        $lSizeCopy,
        $lSizeRepo,
        $strChecksumCopy,
        $rExtra,
        $lSizeTotal,
        $lSizeCurrent,
        $lManifestSaveSize,
        $lManifestSaveCurrent
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupManifestUpdate', \@_,
            {name => 'oManifest', trace => true},
            {name => 'strHost', required => false, trace => true},
            {name => 'iLocalId', required => false, trace => true},

            # Parameters to backupFile()
            {name => 'strDbFile', trace => true},
            {name => 'strRepoFile', trace => true},
            {name => 'lSize', required => false, trace => true},
            {name => 'strChecksum', required => false, trace => true},
            {name => 'bChecksumPage', trace => true},

            # Results from backupFile()
            {name => 'iCopyResult', trace => true},
            {name => 'lSizeCopy', required => false, trace => true},
            {name => 'lSizeRepo', required => false, trace => true},
            {name => 'strChecksumCopy', required => false, trace => true},
            {name => 'rExtra', required => false, trace => true},

            # Accumulators
            {name => 'lSizeTotal', trace => true},
            {name => 'lSizeCurrent', trace => true},
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

        # If the file had page checksums calculated during the copy
        if ($bChecksumPage)
        {
            # The valid flag should be set
            if (defined($rExtra->{bValid}))
            {
                # Store the valid flag
                $oManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM_PAGE, $rExtra->{bValid});

                # If the page was not valid
                if (!$rExtra->{bValid})
                {
                    # Check for a page misalignment
                    if ($lSizeCopy % PG_PAGE_SIZE != 0)
                    {
                        # Make sure the align flag was set, otherwise there is a bug
                        if (!defined($rExtra->{bAlign}) || $rExtra->{bAlign})
                        {
                            confess &log(ASSERT, 'bAlign flag should have been set for misaligned page');
                        }

                        # Emit a warning so the user knows something is amiss
                        &log(WARN,
                            'page misalignment in file ' . (defined($strHost) ? "${strHost}:" : '') .
                            "${strDbFile}: file size ${lSizeCopy} is not divisible by page size " . PG_PAGE_SIZE);
                    }
                    # Else process the page check errors
                    else
                    {
                        $oManifest->set(
                            MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR,
                            dclone($rExtra->{iyPageError}));

                        # Build a pretty list of the page errors
                        my $strPageError;
                        my $iPageErrorTotal = 0;

                        foreach my $iyPage (@{$rExtra->{iyPageError}})
                        {
                            $strPageError .= (defined($strPageError) ? ', ' : '');

                            # If a range of pages
                            if (ref($iyPage))
                            {
                                $strPageError .= $$iyPage[0] . '-' . $$iyPage[1];
                                $iPageErrorTotal += ($$iyPage[1] - $$iyPage[0]) + 1;
                            }
                            # Else a single page
                            else
                            {
                                $strPageError .= $iyPage;
                                $iPageErrorTotal += 1;
                            }
                        }

                        # There should be at least one page in the error list
                        if ($iPageErrorTotal == 0)
                        {
                            confess &log(ASSERT, 'page checksum error list should have at least one entry');
                        }

                        # Emit a warning so the user knows something is amiss
                        &log(WARN,
                            'invalid page checksum' . ($iPageErrorTotal > 1 ? 's' : '') .
                            ' found in file ' . (defined($strHost) ? "${strHost}:" : '') . "${strDbFile} at page" .
                            ($iPageErrorTotal > 1 ? 's' : '') . " ${strPageError}");
                    }
                }
            }
            # If it's not set that's a bug in the code
            elsif (!$oManifest->test(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM_PAGE))
            {
                confess &log(ASSERT, "${strDbFile} should have calculated page checksums");
            }
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
