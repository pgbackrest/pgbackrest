####################################################################################################################################
# BACKUP INFO MODULE
####################################################################################################################################
package pgBackRest::Backup::Info;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use File::stat;

use pgBackRest::Archive::Info;
use pgBackRest::Backup::Common;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant FILE_BACKUP_INFO                                       => 'backup.info';
    push @EXPORT, qw(FILE_BACKUP_INFO);

####################################################################################################################################
# Backup info constants
####################################################################################################################################
use constant INFO_BACKUP_SECTION_BACKUP                             => MANIFEST_SECTION_BACKUP;
    push @EXPORT, qw(INFO_BACKUP_SECTION_BACKUP);
use constant INFO_BACKUP_SECTION_BACKUP_CURRENT                     => INFO_BACKUP_SECTION_BACKUP . ':current';
    push @EXPORT, qw(INFO_BACKUP_SECTION_BACKUP_CURRENT);

use constant INFO_BACKUP_KEY_ARCHIVE_CHECK                          => MANIFEST_KEY_ARCHIVE_CHECK;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_CHECK);
use constant INFO_BACKUP_KEY_ARCHIVE_COPY                           => MANIFEST_KEY_ARCHIVE_COPY;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_COPY);
use constant INFO_BACKUP_KEY_ARCHIVE_START                          => MANIFEST_KEY_ARCHIVE_START;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_START);
use constant INFO_BACKUP_KEY_ARCHIVE_STOP                           => MANIFEST_KEY_ARCHIVE_STOP;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_STOP);
use constant INFO_BACKUP_KEY_BACKUP_STANDBY                         => MANIFEST_KEY_BACKUP_STANDBY;
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_STANDBY);
use constant INFO_BACKUP_KEY_BACKUP_REPO_SIZE                       => 'backup-info-repo-size';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_REPO_SIZE);
use constant INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA                 => 'backup-info-repo-size-delta';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA);
use constant INFO_BACKUP_KEY_BACKUP_SIZE                            => 'backup-info-size';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_SIZE);
use constant INFO_BACKUP_KEY_BACKUP_SIZE_DELTA                      => 'backup-info-size-delta';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_SIZE_DELTA);
use constant INFO_BACKUP_KEY_CATALOG                                => MANIFEST_KEY_CATALOG;
    push @EXPORT, qw(INFO_BACKUP_KEY_CATALOG);
use constant INFO_BACKUP_KEY_CONTROL                                => MANIFEST_KEY_CONTROL;
    push @EXPORT, qw(INFO_BACKUP_KEY_CONTROL);
use constant INFO_BACKUP_KEY_COMPRESS                               => MANIFEST_KEY_COMPRESS;
    push @EXPORT, qw(INFO_BACKUP_KEY_COMPRESS);
use constant INFO_BACKUP_KEY_CHECKSUM_PAGE                          => MANIFEST_KEY_CHECKSUM_PAGE;
    push @EXPORT, qw(INFO_BACKUP_KEY_CHECKSUM_PAGE);
use constant INFO_BACKUP_KEY_DB_VERSION                             => MANIFEST_KEY_DB_VERSION;
    push @EXPORT, qw(INFO_BACKUP_KEY_DB_VERSION);
use constant INFO_BACKUP_KEY_FORMAT                                 => INI_KEY_FORMAT;
    push @EXPORT, qw(INFO_BACKUP_KEY_FORMAT);
use constant INFO_BACKUP_KEY_HARDLINK                               => MANIFEST_KEY_HARDLINK;
    push @EXPORT, qw(INFO_BACKUP_KEY_HARDLINK);
use constant INFO_BACKUP_KEY_HISTORY_ID                             => MANIFEST_KEY_DB_ID;
    push @EXPORT, qw(INFO_BACKUP_KEY_HISTORY_ID);
use constant INFO_BACKUP_KEY_LABEL                                  => MANIFEST_KEY_LABEL;
    push @EXPORT, qw(INFO_BACKUP_KEY_LABEL);
use constant INFO_BACKUP_KEY_PRIOR                                  => MANIFEST_KEY_PRIOR;
    push @EXPORT, qw(INFO_BACKUP_KEY_PRIOR);
use constant INFO_BACKUP_KEY_REFERENCE                              => 'backup-reference';
    push @EXPORT, qw(INFO_BACKUP_KEY_REFERENCE);
use constant INFO_BACKUP_KEY_ONLINE                                 => MANIFEST_KEY_ONLINE;
    push @EXPORT, qw(INFO_BACKUP_KEY_ONLINE);
use constant INFO_BACKUP_KEY_SYSTEM_ID                              => MANIFEST_KEY_SYSTEM_ID;
    push @EXPORT, qw(INFO_BACKUP_KEY_SYSTEM_ID);
use constant INFO_BACKUP_KEY_TIMESTAMP_START                        => MANIFEST_KEY_TIMESTAMP_START;
    push @EXPORT, qw(INFO_BACKUP_KEY_TIMESTAMP_START);
use constant INFO_BACKUP_KEY_TIMESTAMP_STOP                         => MANIFEST_KEY_TIMESTAMP_STOP;
    push @EXPORT, qw(INFO_BACKUP_KEY_TIMESTAMP_STOP);
use constant INFO_BACKUP_KEY_TYPE                                   => MANIFEST_KEY_TYPE;
    push @EXPORT, qw(INFO_BACKUP_KEY_TYPE);
use constant INFO_BACKUP_KEY_VERSION                                => INI_KEY_VERSION;
    push @EXPORT, qw(INFO_BACKUP_KEY_VERSION);

####################################################################################################################################
# Global variables
####################################################################################################################################
my $strBackupInfoMissingMsg =
    FILE_BACKUP_INFO . " does not exist and is required to perform a backup.\n" .
    "HINT: has a stanza-create been performed?";

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackupClusterPath,
        $bRequired,
        $oStorage,
        $bLoad,                                     # Should the file attemp to be loaded?
        $bIgnoreMissing,                            # Don't error on missing files
        $strCipherPassSub,                          # Generated passphrase to encrypt manifest files if the repo is encrypted
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strBackupClusterPath'},
            {name => 'bRequired', default => true},
            {name => 'oStorage', optional => true, default => storageRepo()},
            {name => 'bLoad', optional => true, default => true},
            {name => 'bIgnoreMissing', optional => true, default => false},
            {name => 'strCipherPassSub', optional => true},
        );

    # Build the backup info path/file name
    my $strBackupInfoFile = "${strBackupClusterPath}/" . FILE_BACKUP_INFO;
    my $self = {};
    my $iResult = 0;
    my $strResultMessage;

    # Init object and store variables
    eval
    {
        $self = $class->SUPER::new(
            $oStorage, $strBackupInfoFile,
            {bLoad => $bLoad, bIgnoreMissing => $bIgnoreMissing, strCipherPassSub => $strCipherPassSub});
        return true;
    }
    or do
    {
        # Capture error information
        $iResult = exceptionCode($EVAL_ERROR);
        $strResultMessage = exceptionMessage($EVAL_ERROR);
    };

    if ($iResult != 0)
    {
        # If the backup info file does not exist and is required, then throw an error
        # The backup info is only allowed not to exist when running a stanza-create on a new install
        if ($iResult == ERROR_FILE_MISSING)
        {
            if ($bRequired)
            {
                confess &log(ERROR, "${strBackupClusterPath}/$strBackupInfoMissingMsg", ERROR_FILE_MISSING);
            }
        }
        elsif ($iResult == ERROR_CRYPTO && $strResultMessage =~ "^unable to flush")
        {
            confess &log(ERROR, "unable to parse '$strBackupInfoFile'\nHINT: is or was the repo encrypted?", $iResult);
        }
        else
        {
            confess $EVAL_ERROR;
        }
    }

    $self->{strBackupClusterPath} = $strBackupClusterPath;
    $self->{oStorage} = $oStorage;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# check
#
# Check db info and make sure it matches what is already in the repository. Return the db-id if everything matches.
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $iControlVersion,
        $iCatalogVersion,
        $ullDbSysId,
        $bRequired,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strDbVersion', trace => true},
            {name => 'iControlVersion', trace => true},
            {name => 'iCatalogVersion', trace => true},
            {name => 'ullDbSysId', trace => true},
            {name => 'bRequired', default => true},
        );

    # Confirm the info file exists with the DB section
    if ($bRequired)
    {
        $self->confirmExists();
    }

    if (!$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID, undef, $ullDbSysId) ||
        !$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef, $strDbVersion))
    {
        confess &log(ERROR, "database version = ${strDbVersion}, system-id ${ullDbSysId} does not match backup version = " .
                            $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION) . ", system-id = " .
                            $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID) . "\n" .
                            "HINT: is this the correct stanza?", ERROR_BACKUP_MISMATCH);
    }

    if (!$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG, undef, $iCatalogVersion) ||
        !$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CONTROL, undef, $iControlVersion))
    {
        confess &log(ERROR, "database control-version = ${iControlVersion}, catalog-version ${iCatalogVersion}" .
                            " does not match backup control-version = " .
                            $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CONTROL) . ", catalog-version = " .
                            $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG) . "\n" .
                            "HINT: this may be a symptom of database or repository corruption!", ERROR_BACKUP_MISMATCH);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iDbHistoryId', value => $self->numericGet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID)}
    );
}

####################################################################################################################################
# add
#
# Add a backup to the info file.
####################################################################################################################################
sub add
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oBackupManifest,
        $bSave,
        $bRequired,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->add', \@_,
            {name => 'oBackupManifest', trace => true},
            {name => 'bSave', default => true, trace => true},
            {name => 'bRequired', default => true, trace => true},
        );

    # Confirm the info file exists with the DB section
    if ($bRequired)
    {
        $self->confirmExists();
    }

    # Get the backup label
    my $strBackupLabel = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL);

    # Calculate backup sizes and references
    my $lBackupSize = 0;
    my $lBackupSizeDelta = 0;
    my $lBackupRepoSize = 0;
    my $lBackupRepoSizeDelta = 0;
    my $oReferenceHash = undef;

    foreach my $strFileKey ($oBackupManifest->keys(MANIFEST_SECTION_TARGET_FILE))
    {
        my $lFileSize =
            $oBackupManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_SIZE);
        my $lRepoSize =
            $oBackupManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_REPO_SIZE, false, $lFileSize);
        my $strFileReference =
            $oBackupManifest->get(MANIFEST_SECTION_TARGET_FILE, $strFileKey, MANIFEST_SUBKEY_REFERENCE, false);

        # Temporary until compressed size is back in
        $lBackupSize += $lFileSize;
        $lBackupRepoSize += $lRepoSize;

        if (defined($strFileReference))
        {
            $$oReferenceHash{$strFileReference} = true;
        }
        else
        {
            $lBackupSizeDelta += $lFileSize;
            $lBackupRepoSizeDelta += $lRepoSize;
        }
    }

    # Set backup size info
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_SIZE, $lBackupSize);
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_SIZE_DELTA, $lBackupSizeDelta);
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_REPO_SIZE, $lBackupRepoSize);
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA,
        $lBackupRepoSizeDelta);

    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_CHECK,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_COPY,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_START,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, false));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_STOP,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, false));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_STANDBY,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_CHECKSUM_PAGE,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_CHECKSUM_PAGE));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_COMPRESS,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS));
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_FORMAT,
        $oBackupManifest->numericGet(INI_SECTION_BACKREST, INI_KEY_FORMAT));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HARDLINK,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ONLINE,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE));
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TIMESTAMP_START,
        $oBackupManifest->numericGet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START));
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TIMESTAMP_STOP,
        $oBackupManifest->numericGet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TYPE,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_VERSION,
        $oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION));

    if ($bRequired)
    {
        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HISTORY_ID,
            $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID));
    }
    # If we are reconstructing, then the history id must be taken from the manifest
    else
    {
        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HISTORY_ID,
            $oBackupManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID));
    }

    if (!$oBackupManifest->test(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, CFGOPTVAL_BACKUP_TYPE_FULL))
    {
        my @stryReference = sort(keys(%$oReferenceHash));

        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_PRIOR,
            $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR));
        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_REFERENCE,
                   \@stryReference);
    }

    if ($bSave)
    {
        $self->save();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# current
#
# Test if a backup is current.
####################################################################################################################################
sub current
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->current', \@_,
            {name => 'strBackup'}
        );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bTest', value => $self->test(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup)}
    );
}

####################################################################################################################################
# list
#
# Get backup keys.
####################################################################################################################################
sub list
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFilter,
        $strOrder
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->list', \@_,
            {name => 'strFilter', required => false},
            {name => 'strOrder', default => 'forward'}
        );

    # List of backups
    my @stryBackup;

    # Iterate through the backups and filter
    for my $strBackup ($self->keys(INFO_BACKUP_SECTION_BACKUP_CURRENT))
    {
        if (!defined($strFilter) || $strBackup =~ $strFilter)
        {
            if ($strOrder eq 'reverse')
            {
                unshift(@stryBackup, $strBackup)
            }
            else
            {
                push(@stryBackup, $strBackup)
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryBackup', value => \@stryBackup}
    );
}

####################################################################################################################################
# backupArchiveDbHistoryId
#
# Gets the backup.info db-id for the archiveId passed.
####################################################################################################################################
sub backupArchiveDbHistoryId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchiveId,
        $strPathBackupArchive,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->backupArchiveDbHistoryId', \@_,
            {name => 'strArchiveId'},
            {name => 'strPathBackupArchive'},
        );

    # List of backups associated with the db-id provided
    my @stryArchiveBackup;

    # Build the db list from the history in the backup info and archive info file
    my $oArchiveInfo = new pgBackRest::Archive::Info($strPathBackupArchive, true);
    my $hDbListArchive = $oArchiveInfo->dbHistoryList();
    my $hDbListBackup = $self->dbHistoryList();
    my $iDbHistoryId = undef;

    # Get the db-version and db-id (history id) from the archiveId
    my ($strDbVersionArchive, $iDbIdArchive) = split("-", $strArchiveId);

    # Get the DB system ID to map back to the backup info if it exists in the archive info file
    if (exists($hDbListArchive->{$iDbIdArchive}))
    {
        my $ullDbSysIdArchive = $$hDbListArchive{$iDbIdArchive}{&INFO_SYSTEM_ID};

        # Get the db-id from backup info history that corresponds to the archive db-version and db-system-id
        # Sort from newest (highest db-id) to oldest
        foreach my $iDbIdBackup (sort {$b <=> $a} keys %{$hDbListBackup})
        {
            if ($$hDbListBackup{$iDbIdBackup}{&INFO_SYSTEM_ID} == $ullDbSysIdArchive &&
                $$hDbListBackup{$iDbIdBackup}{&INFO_DB_VERSION} eq $strDbVersionArchive)
            {
                $iDbHistoryId = $iDbIdBackup;
                last;
            }
        }
    }

    # If the database is not found in the backup.info history list
    if (!defined($iDbHistoryId))
    {
        # Check to see that the current DB sections match for the archive and backup info files
        if (!($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                ($self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION)))) ||
            !($oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef,
                ($self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID)))))
        {
            # This should never happen unless the backup.info file is corrupt
            confess &log(ASSERT, "the archive and backup database sections do not match", ERROR_FILE_INVALID);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iDbHistoryId', value => $iDbHistoryId}
    );
}

####################################################################################################################################
# listByArchiveId
#
# Filters a list of backups by the archiveId passed.
####################################################################################################################################
sub listByArchiveId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchiveId,
        $strPathBackupArchive,
        $stryBackup,
        $strOrder,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->listByArchiveId', \@_,
            {name => 'strArchiveId'},
            {name => 'strPathBackupArchive'},
            {name => 'stryBackup'},
            {name => 'strOrder', default => 'forward'}
        );

    # List of backups associated with the db-id provided
    my @stryArchiveBackup;

    my $iDbHistoryId = $self->backupArchiveDbHistoryId($strArchiveId, $strPathBackupArchive);

    # If history found, then build list of backups associated with the archive id passed, else return empty array
    if (defined($iDbHistoryId))
    {
        # Iterate through the backups and filter
        foreach my $strBackup (@$stryBackup)
        {
            # From the backup.info current backup section, get the db-id for the backup and if it is the same, add to the list
            if ($self->test(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID, $iDbHistoryId))
            {
                if ($strOrder eq 'reverse')
                {
                    unshift(@stryArchiveBackup, $strBackup)
                }
                else
                {
                    push(@stryArchiveBackup, $strBackup)
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryArchiveBackup', value => \@stryArchiveBackup}
    );
}

####################################################################################################################################
# last
#
# Find the last backup depending on the type.
####################################################################################################################################
sub last
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->last', \@_,
            {name => 'strType'}
        );

    my $strFilter = backupRegExpGet(true, $strType ne CFGOPTVAL_BACKUP_TYPE_FULL, $strType eq CFGOPTVAL_BACKUP_TYPE_INCR);
    my $strBackup = ($self->list($strFilter, 'reverse'))[0];

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackup', value => $strBackup}
    );
}

####################################################################################################################################
# delete
#
# Delete a backup from the info file.
####################################################################################################################################
sub delete
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackupLabel
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->delete', \@_,
            {name => 'strBackupLabel'}
        );

    $self->remove(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# create
#
# Create the info file. WARNING - this file should only be called from stanza-create or test modules.
####################################################################################################################################
sub create
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $iControlVersion,
        $iCatalogVersion,
        $bSave,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->create', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
            {name => 'iControlVersion'},
            {name => 'iCatalogVersion'},
            {name => 'bSave', default => true},
        );

    # Fill db section and db history section
    $self->dbSectionSet($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId, $self->dbHistoryIdGet(false));

    if ($bSave)
    {
        $self->save();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# dbHistoryIdGet
#
# Get the db history ID
####################################################################################################################################
sub dbHistoryIdGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bFileRequired,
    ) = logDebugParam
        (
            __PACKAGE__ . '->dbHistoryIdGet', \@_,
            {name => 'bFileRequired', default => true},
        );

    # Confirm the info file exists if it is required
    if ($bFileRequired)
    {
        $self->confirmExists();
    }

    # If the DB section does not exist, initialize the history to one, else return the latest ID
    my $iDbHistoryId = (!$self->test(INFO_BACKUP_SECTION_DB))
                        ? 1 : $self->numericGet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iDbHistoryId', value => $iDbHistoryId}
    );
}

####################################################################################################################################
# dbHistoryList
#
# Get the data from the db history section.
####################################################################################################################################
sub dbHistoryList
{
    my $self = shift;
    my
    (
        $strOperation,
    ) = logDebugParam
        (
            __PACKAGE__ . '->dbHistoryList',
        );

    my %hDbHash;

    foreach my $iHistoryId ($self->keys(INFO_BACKUP_SECTION_DB_HISTORY))
    {
        $hDbHash{$iHistoryId}{&INFO_DB_VERSION} =
            $self->get(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_DB_VERSION);
        $hDbHash{$iHistoryId}{&INFO_SYSTEM_ID} =
            $self->get(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_SYSTEM_ID);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hDbHash', value => \%hDbHash}
    );
}

####################################################################################################################################
# dbSectionSet
#
# Set the db and db:history sections.
####################################################################################################################################
sub dbSectionSet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $iControlVersion,
        $iCatalogVersion,
        $ullDbSysId,
        $iDbHistoryId,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->dbSectionSet', \@_,
            {name => 'strDbVersion', trace => true},
            {name => 'iControlVersion', trace => true},
            {name => 'iCatalogVersion', trace => true},
            {name => 'ullDbSysId', trace => true},
            {name => 'iDbHistoryId', trace => true},
        );

    # Fill db section
    $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG, undef, $iCatalogVersion);
    $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CONTROL, undef, $iControlVersion);
    $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID, undef, $ullDbSysId);
    # Force the version to a string since newer versions of JSON::PP lose track of the fact that it is one
    $self->set(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef, $strDbVersion . '');
    $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID, undef, $iDbHistoryId);

    # Fill db history
    $self->numericSet(INFO_BACKUP_SECTION_DB_HISTORY, $iDbHistoryId, INFO_BACKUP_KEY_CATALOG, $iCatalogVersion);
    $self->numericSet(INFO_BACKUP_SECTION_DB_HISTORY, $iDbHistoryId, INFO_BACKUP_KEY_CONTROL,  $iControlVersion);
    $self->numericSet(INFO_BACKUP_SECTION_DB_HISTORY, $iDbHistoryId, INFO_BACKUP_KEY_SYSTEM_ID, $ullDbSysId);
    $self->set(INFO_BACKUP_SECTION_DB_HISTORY, $iDbHistoryId, INFO_BACKUP_KEY_DB_VERSION, $strDbVersion . '');

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# confirmDb
#
# Ensure that the backup is associated with the database passed.
# NOTE: The backup must exist in the backup:current section.
####################################################################################################################################
sub confirmDb
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackup,
        $strDbVersion,
        $ullDbSysId,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->confirmDb', \@_,
            {name => 'strBackup', trace => true},
            {name => 'strDbVersion', trace => true},
            {name => 'ullDbSysId', trace => true},
        );

    my $bConfirmDb = undef;

    # Get the db-id associated with the backup
    my $iDbHistoryId = $self->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup, INFO_BACKUP_KEY_HISTORY_ID);

    # Get the version and system-id for all known databases
    my $hDbList = $self->dbHistoryList();

    # If the db-id for the backup exists in the list
    if (exists $hDbList->{$iDbHistoryId})
    {
        # If the version and system-id match then database is confirmed for the backup
        if (($hDbList->{$iDbHistoryId}{&INFO_DB_VERSION} eq $strDbVersion) &&
            ($hDbList->{$iDbHistoryId}{&INFO_SYSTEM_ID} eq $ullDbSysId))
        {
            $bConfirmDb = true;
        }
        else
        {
            $bConfirmDb = false;
        }
    }
    # If not, the backup.info file must be corrupt
    else
    {
        confess &log(ERROR, "backup info file is missing database history information for an existing backup", ERROR_FILE_INVALID);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bConfirmDb', value => $bConfirmDb}
    );
}

####################################################################################################################################
# confirmExists
#
# Ensure that the backup.info file and the db section exist.
####################################################################################################################################
sub confirmExists
{
    my $self = shift;

    # Confirm the file exists and the DB section is filled out
    if (!$self->test(INFO_BACKUP_SECTION_DB) || !$self->{bExists})
    {
        confess &log(ERROR, $self->{strBackupClusterPath} . "/" . $strBackupInfoMissingMsg, ERROR_FILE_MISSING);
    }
}

1;
