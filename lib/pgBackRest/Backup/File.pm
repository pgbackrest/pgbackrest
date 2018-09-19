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
use pgBackRest::Storage::Base;
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
use constant BACKUP_FILE_NOOP                                       => 4;
    push @EXPORT, qw(BACKUP_FILE_NOOP);

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
        $strBackupLabel,                            # Label of current backup
        $bCompress,                                 # Compress destination file
        $iCompressLevel,                            # Compress level
        $lModificationTime,                         # File modification time
        $bIgnoreMissing,                            # Is it OK if the file is missing?
        $hExtraParam,                               # Parameter to pass to the extra function
        $bDelta,                                    # Is the delta option on?
        $bHasReference,                             # Does the file exist in the repo in a prior backup in the set?
        $strCipherPass,                             # Passphrase to access the repo file (undefined if repo not encrypted). This
                                                    # parameter must always be last in the parameter list to this function.
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupFile', \@_,
            {name => 'strDbFile', trace => true},
            {name => 'strRepoFile', trace => true},
            {name => 'lSizeFile', trace => true},
            {name => 'strChecksum', required => false, trace => true},
            {name => 'bChecksumPage', trace => true},
            {name => 'strBackupLabel', trace => true},
            {name => 'bCompress', trace => true},
            {name => 'iCompressLevel', trace => true},
            {name => 'lModificationTime', trace => true},
            {name => 'bIgnoreMissing', default => true, trace => true},
            {name => 'hExtraParam', required => false, trace => true},
            {name => 'bDelta', trace => true},
            {name => 'bHasReference', trace => true},
            {name => 'strCipherPass', required => false, trace => true},
        );

    my $oStorageRepo = storageRepo();               # Repo storage
    my $iCopyResult = BACKUP_FILE_COPY;             # Copy result
    my $strCopyChecksum;                            # Copy checksum
    my $rExtra;                                     # Page checksum result
    my $lCopySize;                                  # Copy Size
    my $lRepoSize;                                  # Repo size

    # Add compression suffix if needed
    my $strFileOp = $strRepoFile . ($bCompress ? '.' . COMPRESS_EXT : '');

    my $bCopy = true;

    # If checksum is defined then the file needs to be checked. If delta option then check the DB and possibly the repo, else just
    # check the repo.
    if (defined($strChecksum))
    {
        # If delta, then check the DB checksum and possibly the repo. If the checksum does not match in either case then recopy.
        if ($bDelta)
        {
            ($strCopyChecksum, $lCopySize) = storageDb()->hashSize($strDbFile, {bIgnoreMissing => $bIgnoreMissing});

            # If the DB file exists, then check the checksum
            if (defined($strCopyChecksum))
            {
                $bCopy = !($strCopyChecksum eq $strChecksum && $lCopySize == $lSizeFile);

                # If the database file checksum and size are same and the file is in a prior backup, then no need to copy. If the
                # checksum/size do not match, that is OK, just leave the copy result as COPY so the file will be copied to this
                # backup.
                if (!$bCopy && $bHasReference)
                {
                    $iCopyResult = BACKUP_FILE_NOOP;
                }
            }
            # Else the source file is missing from the database so skip this file
            else
            {
                $iCopyResult = BACKUP_FILE_SKIP;
                $bCopy = false;
            }
        }

        # If this is not a delta backup or it is and the file exists and the checksum from the DB matches, then also test the
        # checksum of the file in the repo (unless it is in a prior backup) and if the checksum doesn't match, then there may be
        # corruption in the repo, so recopy
        if (!$bDelta || !$bHasReference)
        {
            # If this is a delta backup and the file is missing from the DB, then remove it from the repo (backupManifestUpdate will
            # remove it from the manifest)
            if ($iCopyResult == BACKUP_FILE_SKIP)
            {
                $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strFileOp}");
            }
            elsif (!$bDelta || !$bCopy)
            {
                # Add decompression
                my $rhyFilter;

                if ($bCompress)
                {
                    push(@{$rhyFilter}, {strClass => STORAGE_FILTER_GZIP, rxyParam => [{strCompressType => STORAGE_DECOMPRESS}]});
                }

                # Get the checksum
                ($strCopyChecksum, $lCopySize) = $oStorageRepo->hashSize(
                    $oStorageRepo->openRead(STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strFileOp}",
                    {rhyFilter => $rhyFilter, strCipherPass => $strCipherPass}));

                # Determine if the file needs to be recopied
                $bCopy = !($strCopyChecksum eq $strChecksum && $lCopySize == $lSizeFile);

                # Set copy result
                $iCopyResult = $bCopy ? BACKUP_FILE_RECOPY : BACKUP_FILE_CHECKSUM;
            }
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

        # Add compression
        if ($bCompress)
        {
            push(@{$rhyFilter}, {strClass => STORAGE_FILTER_GZIP, rxyParam => [{iLevel => $iCompressLevel}]});
        }

        # Open the file
        my $oSourceFileIo = storageDb()->openRead($strDbFile, {rhyFilter => $rhyFilter, bIgnoreMissing => $bIgnoreMissing});

        # If source file exists
        if (defined($oSourceFileIo))
        {
            # Copy the file
            $oStorageRepo->copy(
                $oSourceFileIo,
                $oStorageRepo->openWrite(
                    STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strFileOp}",
                    {bPathCreate => true, bProtocolCompress => !$bCompress, strCipherPass => $strCipherPass}));

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
        $lRepoSize = ($oStorageRepo->info(STORAGE_REPO_BACKUP . "/${strBackupLabel}/${strFileOp}"))->size();
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

    # If the file is in a prior backup and nothing changed, then nothing needs to be done
    if ($iCopyResult == BACKUP_FILE_NOOP)
    {
        # File copy was not needed so just restore the size and checksum to the manifest
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_SIZE, $lSizeCopy);
        $oManifest->set(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM, $strChecksumCopy);

        &log(DETAIL,
            'match file from prior backup ' . (defined($strHost) ? "${strHost}:" : '') . "${strDbFile} (" .
                fileSizeFormat($lSizeCopy) . ', ' . int($lSizeCurrent * 100 / $lSizeTotal) . '%)' .
                ($lSizeCopy != 0 ? " checksum ${strChecksumCopy}" : ''),
             undef, undef, undef, $iLocalId);
    }
    # Else process the results
    else
    {
        # Log invalid checksum
        if ($iCopyResult == BACKUP_FILE_RECOPY)
        {
            &log(
                WARN,
                "resumed backup file ${strRepoFile} does not have expected checksum ${strChecksum}. The file will be recopied and" .
                " backup will continue but this may be an issue unless the resumed backup path in the repository is known to be" .
                " corrupted.\n" .
                "NOTE: this does not indicate a problem with the PostgreSQL page checksums.");
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

            # If the file was copied, then remove any reference to the file's existence in a prior backup.
            if ($iCopyResult == BACKUP_FILE_COPY || $iCopyResult == BACKUP_FILE_RECOPY)
            {
                $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_REFERENCE);
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
    }

    # Determine whether to save the manifest
    $lManifestSaveCurrent += $lSize;

    if ($lManifestSaveCurrent >= $lManifestSaveSize)
    {
        $oManifest->saveCopy();

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
