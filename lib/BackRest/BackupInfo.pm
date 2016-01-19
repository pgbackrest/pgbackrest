####################################################################################################################################
# BACKUP INFO MODULE
####################################################################################################################################
package BackRest::BackupInfo;
use parent 'BackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use File::stat;

use lib dirname($0);
use BackRest::Common::Exception;
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Config::Config;
use BackRest::File;
use BackRest::Manifest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_BACKUP_INFO                                         => 'BackupInfo';

use constant OP_INFO_BACKUP_ADD                                     => OP_BACKUP_INFO . "->add";
use constant OP_INFO_BACKUP_CHECK                                   => OP_BACKUP_INFO . "->check";
use constant OP_INFO_BACKUP_CURRENT                                 => OP_BACKUP_INFO . "->current";
use constant OP_INFO_BACKUP_DELETE                                  => OP_BACKUP_INFO . "->delete";
use constant OP_INFO_BACKUP_LIST                                    => OP_BACKUP_INFO . "->list";
use constant OP_INFO_BACKUP_NEW                                     => OP_BACKUP_INFO . "->new";

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
use constant INFO_BACKUP_SECTION_BACKUP_HISTORY                     => INFO_BACKUP_SECTION_BACKUP . ':history';
    push @EXPORT, qw(INFO_BACKUP_SECTION_BACKUP_HISTORY);
use constant INFO_BACKUP_SECTION_DB                                 => 'db';
    push @EXPORT, qw(INFO_BACKUP_SECTION_DB);
use constant INFO_BACKUP_SECTION_DB_HISTORY                         => INFO_BACKUP_SECTION_DB . ':history';
    push @EXPORT, qw(INFO_BACKUP_SECTION_DB_HISTORY);

use constant INFO_BACKUP_KEY_ARCHIVE_CHECK                          => MANIFEST_KEY_ARCHIVE_CHECK;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_CHECK);
use constant INFO_BACKUP_KEY_ARCHIVE_COPY                           => MANIFEST_KEY_ARCHIVE_COPY;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_COPY);
use constant INFO_BACKUP_KEY_ARCHIVE_START                          => MANIFEST_KEY_ARCHIVE_START;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_START);
use constant INFO_BACKUP_KEY_ARCHIVE_STOP                           => MANIFEST_KEY_ARCHIVE_STOP;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_STOP);
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
use constant INFO_BACKUP_KEY_CHECKSUM                               => INI_KEY_CHECKSUM;
    push @EXPORT, qw(INFO_BACKUP_KEY_CHECKSUM);
use constant INFO_BACKUP_KEY_DB_VERSION                             => MANIFEST_KEY_DB_VERSION;
    push @EXPORT, qw(INFO_BACKUP_KEY_DB_VERSION);
use constant INFO_BACKUP_KEY_FORMAT                                 => INI_KEY_FORMAT;
    push @EXPORT, qw(INFO_BACKUP_KEY_FORMAT);
use constant INFO_BACKUP_KEY_HARDLINK                               => MANIFEST_KEY_HARDLINK;
    push @EXPORT, qw(INFO_BACKUP_KEY_HARDLINK);
use constant INFO_BACKUP_KEY_HISTORY_ID                             => 'db-id';
    push @EXPORT, qw(INFO_BACKUP_KEY_HISTORY_ID);
use constant INFO_BACKUP_KEY_LABEL                                  => MANIFEST_KEY_LABEL;
    push @EXPORT, qw(INFO_BACKUP_KEY_LABEL);
use constant INFO_BACKUP_KEY_PRIOR                                  => MANIFEST_KEY_PRIOR;
    push @EXPORT, qw(INFO_BACKUP_KEY_PRIOR);
use constant INFO_BACKUP_KEY_REFERENCE                              => 'backup-reference';
    push @EXPORT, qw(INFO_BACKUP_KEY_REFERENCE);
use constant INFO_BACKUP_KEY_START_STOP                             => MANIFEST_KEY_START_STOP;
    push @EXPORT, qw(INFO_BACKUP_KEY_START_STOP);
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
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strBackupClusterPath                       # Backup cluster path
    ) =
        logDebugParam
        (
            OP_INFO_BACKUP_NEW, \@_,
            {name => 'strBackupClusterPath'}
        );

    # Build the backup info path/file name
    my $strBackupInfoFile = "${strBackupClusterPath}/" . FILE_BACKUP_INFO;
    my $bExists = -e $strBackupInfoFile ? true : false;

    # Init object and store variables
    my $self = $class->SUPER::new($strBackupInfoFile, $bExists);

    $self->{bExists} = $bExists;
    $self->{strBackupClusterPath} = $strBackupClusterPath;

    # Validate the backup info
    $self->validate();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# validate
#
# Compare the backup info against the actual backups on disk.
####################################################################################################################################
sub validate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
    ) =
        logDebugParam
        (
            OP_INFO_BACKUP_NEW
        );

    # Remove backups that no longer exist on disk
    foreach my $strBackup ($self->keys(INFO_BACKUP_SECTION_BACKUP_CURRENT))
    {
        if (!-e "$self->{strBackupClusterPath}/${strBackup}")
        {
            &log(WARN, "backup ${strBackup} is missing from the repository - removed from " . FILE_BACKUP_INFO);
            $self->delete($strBackup);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# check
#
# Check db info and make it is compatible.
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
    ) =
        logDebugParam
        (
            OP_INFO_BACKUP_CHECK, \@_,
            {name => 'strDbVersion', trace => true},
            {name => 'iControlVersion', trace => true},
            {name => 'iCatalogVersion', trace => true},
            {name => 'ullDbSysId', trace => true}
        );

    if (!$self->test(INFO_BACKUP_SECTION_DB))
    {
        my $iHistoryId = 1;

        # Fill db section
        $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG, undef, $iCatalogVersion);
        $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CONTROL, undef, $iControlVersion);
        $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID, undef, $ullDbSysId);
        $self->set(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef, $strDbVersion);
        $self->numericSet(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID, undef, $iHistoryId);

        # Fill db history
        $self->numericSet(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_CATALOG, $iCatalogVersion);
        $self->numericSet(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_CONTROL,  $iControlVersion);
        $self->numericSet(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_SYSTEM_ID, $ullDbSysId);
        $self->set(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_DB_VERSION, $strDbVersion);
    }
    else
    {
        if (!$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID, undef, $ullDbSysId) ||
            !$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef, $strDbVersion))
        {
            confess &log(ERROR, "database version = ${strDbVersion}, system-id ${ullDbSysId} does not match backup version = " .
                                $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION) . ", system-id = " .
                                $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID) .
                                "\nHINT: are you backing up to the correct stanza?", ERROR_BACKUP_MISMATCH);
        }

        if (!$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG, undef, $iCatalogVersion) ||
            !$self->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CONTROL, undef, $iControlVersion))
        {
            confess &log(ERROR, "database control-version = ${iControlVersion}, catalog-version ${iCatalogVersion}" .
                                " does not match backup control-version = " .
                                $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CONTROL) . ", catalog-version = " .
                                $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG) .
                                "\nHINT: this may be a symptom of database or repository corruption!", ERROR_BACKUP_MISMATCH);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        $oFile,
        $oBackupManifest
    ) =
        logDebugParam
        (
            OP_INFO_BACKUP_ADD, \@_,
            {name => 'oFile', trace => true},
            {name => 'oBackupManifest', trace => true}
        );

    # !!! How best to log these follow-on cases?
    my $strBackupLabel = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL);

    # Calculate backup sizes and references
    my $lBackupSize = 0;
    my $lBackupSizeDelta = 0;
    my $lBackupRepoSize = 0;
    my $lBackupRepoSizeDelta = 0;
    my $oReferenceHash = undef;

    my $bCompress = $oBackupManifest->get(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);

    foreach my $strPathKey ($oBackupManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        my $strFileSection = "${strPathKey}:" . MANIFEST_FILE;
        my $strPath = $strPathKey;
        $strPath =~ s/\:/\//g;

        foreach my $strFileKey ($oBackupManifest->keys($strFileSection))
        {
            my $lFileSize = $oBackupManifest->get($strFileSection, $strFileKey, MANIFEST_SUBKEY_SIZE);
            my $strFileReference = $oBackupManifest->get($strFileSection, $strFileKey, MANIFEST_SUBKEY_REFERENCE, false);

            my $strFile = $oFile->pathGet(PATH_BACKUP_CLUSTER,
                                           (defined($strFileReference) ? "${strFileReference}" : $strBackupLabel) .
                                           "/${strPath}/${strFileKey}" .
                                           ($bCompress ? '.' . $oFile->{strCompressExtension} : ''));

            my $oStat = lstat($strFile);

            # Check for errors in stat
            defined($oStat)
                or confess &log(ERROR, "unable to lstat ${strFile}");

            $lBackupSize += $lFileSize;
            $lBackupRepoSize += $oStat->size;

            if (defined($strFileReference))
            {
                $$oReferenceHash{$strFileReference} = true;
            }
            else
            {
                $lBackupSizeDelta += $lFileSize;
                $lBackupRepoSizeDelta += $oStat->size;
            }
        }
    }

    # Add the manifest.backup size
    my $strManifestFile = $oFile->pathGet(PATH_BACKUP_CLUSTER, "/${strBackupLabel}/" . FILE_MANIFEST);
    my $oStat = lstat($strManifestFile);

    # Check for errors in stat
    defined($oStat)
        or confess &log(ERROR, "unable to lstat ${strManifestFile}");

    $lBackupRepoSize += $oStat->size;
    $lBackupRepoSizeDelta += $oStat->size;

    # Set backup size info
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_SIZE, $lBackupSize);
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_SIZE_DELTA, $lBackupSizeDelta);
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_REPO_SIZE, $lBackupRepoSize);
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA,
        $lBackupRepoSizeDelta);

    # Store information about the backup into the backup section
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_CHECK,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_COPY,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_START,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, false));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_STOP,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, false));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_COMPRESS,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS));
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_FORMAT,
        $oBackupManifest->numericGet(INI_SECTION_BACKREST, INI_KEY_FORMAT));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HARDLINK,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK));
    $self->boolSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_START_STOP,
        $oBackupManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_START_STOP));
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TIMESTAMP_START,
        $oBackupManifest->numericGet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START));
    $self->numericSet(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TIMESTAMP_STOP,
        $oBackupManifest->numericGet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TYPE,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_VERSION,
        $oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HISTORY_ID,
        $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID));

    if (!$oBackupManifest->test(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, BACKUP_TYPE_FULL))
    {
        my @stryReference = sort(keys(%$oReferenceHash));

        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_PRIOR,
            $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR));
        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_REFERENCE,
                   \@stryReference);
    }

    # Copy backup info to the history
    $self->set(INFO_BACKUP_SECTION_BACKUP_HISTORY, $strBackupLabel, undef,
        $self->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel));

    $self->save();

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
            OP_INFO_BACKUP_CURRENT, \@_,
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
        $strFilter
    ) =
        logDebugParam
        (
            OP_INFO_BACKUP_LIST, \@_,
            {name => 'strFilter'}
        );

    # List of backups
    my @stryBackup;

    # Iterate through the backups and filter
    for my $strBackup ($self->keys(INFO_BACKUP_SECTION_BACKUP_CURRENT))
    {
        if ($strBackup =~ $strFilter)
        {
            push(@stryBackup, $strBackup)
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
            OP_INFO_BACKUP_DELETE, \@_,
            {name => 'strBackupLabel'}
        );

    $self->remove(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
