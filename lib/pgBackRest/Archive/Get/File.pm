####################################################################################################################################
# ARCHIVE GET FILE MODULE
####################################################################################################################################
package pgBackRest::Archive::Get::File;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(basename dirname);

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Info;
use pgBackRest::Db;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::Gzip;
use pgBackRest::Storage::Filter::Sha;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# Given a specific database version and system-id, find a file in the archive. If no database info was passed, the current database
# will be used.
####################################################################################################################################
sub archiveGetCheck
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $strFile,
        $bCheck,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '::archiveGetCheck', \@_,
        {name => 'strDbVersion', required => false},
        {name => 'ullDbSysId', required => false},
        {name => 'strFile', required => false},
        {name => 'bCheck',  required => false, default => true},
    );

    my @stryArchiveId = ();
    my $strArchiveId;
    my $strArchiveFile;
    my $strCipherPass;

    # If the dbVersion/dbSysId are not passed, then we need to retrieve the database information
    if (!defined($strDbVersion) || !defined($ullDbSysId) )
    {
        # get DB info for comparison
        ($strDbVersion, my $iControlVersion, my $iCatalogVersion, $ullDbSysId) = dbMasterGet()->info();
    }

    # Get db info from the repo
    if (!isRepoLocal())
    {
        ($strArchiveId, $strArchiveFile, $strCipherPass) = protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP)->cmdExecute(
            OP_ARCHIVE_GET_CHECK, [$strDbVersion, $ullDbSysId, $strFile, $bCheck], true);
    }
    else
    {
        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), true);

        # Check that the archive info is compatible with the database if required (not required for archive-get)
        if ($bCheck)
        {
            push(@stryArchiveId, $oArchiveInfo->check($strDbVersion, $ullDbSysId));
        }
        # Else if the database version and system-id are in the info history list then get a list of corresponding archiveIds
        else
        {
            @stryArchiveId = $oArchiveInfo->archiveIdList($strDbVersion, $ullDbSysId);
        }

        # Default the returned archiveId to the newest in the event the WAL segment is not found then the most recent archiveID will
        # be returned. If none were found, then the preceding calls will error.
        $strArchiveId = $stryArchiveId[0];

        # If a file was passed to look for, then look for the file starting in the newest matching archiveId to the oldest
        if (defined($strFile))
        {
            foreach my $strId (@stryArchiveId)
            {
                # Then if it is a WAL segment, try to find it
                if (walIsSegment($strFile))
                {
                    $strArchiveFile = walSegmentFind(storageRepo(), $strId, $strFile);
                }
                # Else if not a WAL segment, see if it exists in the archive dir
                elsif (storageRepo()->exists(STORAGE_REPO_ARCHIVE . "/${strId}/${strFile}"))
                {
                    $strArchiveFile = $strFile;
                }

                # If the file was found, then return the archiveId where it was found
                if (defined($strArchiveFile))
                {
                    $strArchiveId = $strId;
                    last;
                }
            }
        }

        # Get the encryption passphrase to read/write files (undefined if the repo is not encrypted)
        $strCipherPass = $oArchiveInfo->cipherPassSub();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId},
        {name => 'strArchiveFile', value => $strArchiveFile},
        {name => 'strCipherPass', value => $strCipherPass, redact => true}
    );
}

push @EXPORT, qw(archiveGetCheck);

####################################################################################################################################
# archiveGetFile
#
# Copy a file from the archive.
####################################################################################################################################
sub archiveGetFile
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourceArchive,
        $strDestinationFile,
        $bAtomic,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::archiveGetFile', \@_,
            {name => 'strSourceArchive'},
            {name => 'strDestinationFile'},
            {name => 'bAtomic'},
        );

    lockStopTest();

    # Get the repo storage
    my $oStorageRepo = storageRepo();

    # Construct absolute path to the WAL file when it is relative
    $strDestinationFile = walPath($strDestinationFile, cfgOption(CFGOPT_PG_PATH, false), cfgCommandName(cfgCommandGet()));

    # Get the wal segment filename
    my ($strArchiveId, $strArchiveFile, $strCipherPass) = archiveGetCheck(undef, undef, $strSourceArchive, false);

    # If there are no matching archive files then there are two possibilities:
    # 1) The end of the archive stream has been reached, this is normal and a 1 will be returned
    # 2) There is a hole in the archive stream and a hard error should be returned.  However, holes are possible due to async
    #    archiving - so when to report a hole?  Since a hard error will cause PG to terminate, for now treat as case #1.
    my $iResult = 0;

    if (!defined($strArchiveFile))
    {
        $iResult = 1;
    }
    else
    {
        # Determine if the source file is already compressed
        my $bSourceCompressed = $strArchiveFile =~ ('^.*\.' . COMPRESS_EXT . '$') ? true : false;

        # Copy the archive file to the requested location
        # If the file is encrypted, then the passphrase from the info file is required to open the archive file in the repo
        $oStorageRepo->copy(
            $oStorageRepo->openRead(
                STORAGE_REPO_ARCHIVE . "/${strArchiveId}/${strArchiveFile}", {bProtocolCompress => !$bSourceCompressed,
                strCipherPass => defined($strCipherPass) ? $strCipherPass : undef}),
            storageDb()->openWrite(
                $strDestinationFile,
                {bAtomic => $bAtomic, rhyFilter => $bSourceCompressed ?
                    [{strClass => STORAGE_FILTER_GZIP, rxyParam => [{strCompressType => STORAGE_DECOMPRESS}]}] : undef}));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult}
    );
}

push @EXPORT, qw(archiveGetFile);

1;
