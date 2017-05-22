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

use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common::Common;

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
# Load the C library if present
####################################################################################################################################
my $bLibC = false;

eval
{
    # Load the C library only if page checksums are required
    require pgBackRest::LibC;
    pgBackRest::LibC->import(qw(:checksum));

    $bLibC = true;

    return 1;
} or do {};

####################################################################################################################################
# isLibC
#
# Does the C library exist?
####################################################################################################################################
sub isLibC
{
    return $bLibC;
}

push @EXPORT, qw(isLibC);

####################################################################################################################################
# backupChecksumPage
####################################################################################################################################
sub backupChecksumPage
{
    my $rExtraParam = shift;
    my $tBufferRef = shift;
    my $iBufferSize = shift;
    my $iBufferOffset = shift;
    my $hExtra = shift;

    # Initialize the extra hash
    if (!defined($hExtra->{bValid}))
    {
        $hExtra->{bValid} = true;
    }

    # Return when buffer is 0
    if ($iBufferSize == 0)
    {
        # Make sure valid is set for 0 length files
        if ($iBufferOffset == 0 && !defined($hExtra->{bValid}))
        {
            $hExtra->{bValid} = true;
        }

        return;
    }

    # Error if offset is not divisible by page size
    if ($iBufferOffset % PG_PAGE_SIZE != 0)
    {
        confess &log(ASSERT, "should not be possible to see misaligned buffer offset ${iBufferOffset}, buffer size ${iBufferSize}");
    }

    # If the buffer is not divisible by 0 then it's not valid
    if ($iBufferSize % PG_PAGE_SIZE != 0)
    {
        if (defined($hExtra->{bAlign}))
        {
            confess &log(ASSERT, "should not be possible to see two misaligned blocks in a row");
        }

        $hExtra->{bValid} = false;
        $hExtra->{bAlign} = false;
        delete($hExtra->{iyPageError});
    }
    elsif ($iBufferSize > 0)
    {
        # Calculate offset to the first block in the buffer
        my $iBlockOffset = int($iBufferOffset / PG_PAGE_SIZE) + ($rExtraParam->{iSegmentNo} * 131072);

        if (!pageChecksumBufferTest(
                $$tBufferRef, $iBufferSize, $iBlockOffset, PG_PAGE_SIZE, $rExtraParam->{iWalId},
                $rExtraParam->{iWalOffset}))
        {
            $hExtra->{bValid} = false;

            # Now figure out exactly where the errors occurred.  It would be more efficient if the checksum function returned an
            # array, but we're hoping there won't be that many errors to scan so this should work fine.
            for (my $iBlockNo = 0; $iBlockNo < int($iBufferSize / PG_PAGE_SIZE); $iBlockNo++)
            {
                my $iBlockNoStart = $iBlockOffset + $iBlockNo;

                if (!pageChecksumTest(
                        substr($$tBufferRef, $iBlockNo * PG_PAGE_SIZE, PG_PAGE_SIZE), $iBlockNoStart, PG_PAGE_SIZE,
                        $rExtraParam->{iWalId}, $rExtraParam->{iWalOffset}))
                {
                    my $iLastIdx = defined($hExtra->{iyPageError}) ? @{$hExtra->{iyPageError}} - 1 : 0;
                    my $iyLast = defined($hExtra->{iyPageError}) ? $hExtra->{iyPageError}[$iLastIdx] : undef;

                    if (!defined($iyLast) || (!ref($iyLast) && $iyLast != $iBlockNoStart - 1) ||
                        (ref($iyLast) && $iyLast->[1] != $iBlockNoStart - 1))
                    {
                        push(@{$hExtra->{iyPageError}}, $iBlockNoStart);
                    }
                    elsif (!ref($iyLast))
                    {
                        $hExtra->{iyPageError}[$iLastIdx] = undef;
                        push(@{$hExtra->{iyPageError}[$iLastIdx]}, $iyLast);
                        push(@{$hExtra->{iyPageError}[$iLastIdx]}, $iBlockNoStart);
                    }
                    else
                    {
                        $hExtra->{iyPageError}[$iLastIdx][1] = $iBlockNoStart;
                    }
                }
            }
        }
    }
}

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
            {name => 'oFile', trace => true},
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

    my $iCopyResult = BACKUP_FILE_COPY;             # Copy result
    my $strCopyChecksum;                            # Copy checksum
    my $rExtra;                                     # Page checksum result
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
        # Determine which segment no this is by checking for a numeric extension.  No extension means segment 0.
        if ($bChecksumPage)
        {
            $hExtraParam->{iSegmentNo} = ($strDbFile =~ /\.[0-9]+$/) ? substr(($strDbFile =~ m/\.[0-9]+$/g)[0], 1) + 0 : 0;
        }

        # Copy the file from the database to the backup (will return false if the source file is missing)
        (my $bCopyResult, $strCopyChecksum, $lCopySize, $rExtra) = $oFile->copy(
            PATH_DB_ABSOLUTE, $strDbFile,
            PATH_BACKUP_TMP, $strFileOp,
            false,                                                  # Source is not compressed since it is the db directory
            $bDestinationCompress,                                  # Destination should be compressed based on backup settings
            $bIgnoreMissing,                                        # Ignore missing files
            undef,                                                  # Do not set modification time
            undef,                                                  # Do not set original mode
            true,                                                   # Create the destination directory if it does not exist
            undef, undef, undef, undef,                             # Unused
            $bChecksumPage ?                                        # Function to process page checksums
                'pgBackRest::Backup::File::backupChecksumPage' : undef,
            $hExtraParam,                                           # Start LSN to pass to extra function
            false);                                                 # Don't copy via a temp file

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
