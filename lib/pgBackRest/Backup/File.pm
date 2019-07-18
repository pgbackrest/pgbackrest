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

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Base;
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
                if (defined($rExtra->{valid}))
                {
                    # Store the valid flag
                    $oManifest->boolSet(
                        MANIFEST_SECTION_TARGET_FILE, $strRepoFile, MANIFEST_SUBKEY_CHECKSUM_PAGE, $rExtra->{valid});

                    # If the page was not valid
                    if (!$rExtra->{valid})
                    {
                        # Check for a page misalignment
                        if ($lSizeCopy % PG_PAGE_SIZE != 0)
                        {
                            # Make sure the align flag was set, otherwise there is a bug
                            if (!defined($rExtra->{align}) || $rExtra->{align})
                            {
                                confess &log(ASSERT, 'align flag should have been set for misaligned page');
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
                                dclone($rExtra->{error}));

                            # Build a pretty list of the page errors
                            my $strPageError;
                            my $iPageErrorTotal = 0;

                            foreach my $iyPage (@{$rExtra->{error}})
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
