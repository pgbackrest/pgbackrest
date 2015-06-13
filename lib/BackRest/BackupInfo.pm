####################################################################################################################################
# BACKUP INFO MODULE
####################################################################################################################################
package BackRest::BackupInfo;
use parent 'BackRest::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname basename);
use File::stat;

use lib dirname($0);
use BackRest::Config;
use BackRest::Exception;
use BackRest::File;
use BackRest::Ini;
use BackRest::Manifest;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_BACKUP_INFO                 => 'BackupInfo';

use constant OP_INFO_BACKUP_NEW             => OP_BACKUP_INFO . "->new";
use constant OP_INFO_BACKUP_BACKUP_ADD      => OP_BACKUP_INFO . "->backupAdd";
use constant OP_INFO_BACKUP_BACKUP_REMOVE   => OP_BACKUP_INFO . "->backupRemove";

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant FILE_BACKUP_INFO => 'backup.info';     our @EXPORT = qw(FILE_BACKUP_INFO);

####################################################################################################################################
# Backup info Constants
####################################################################################################################################
use constant INFO_BACKUP_SECTION_BACKUP             => MANIFEST_SECTION_BACKUP;
    push @EXPORT, qw(INFO_BACKUP_SECTION_BACKUP);
use constant INFO_BACKUP_SECTION_BACKUP_CURRENT     => INFO_BACKUP_SECTION_BACKUP . ':current';
    push @EXPORT, qw(INFO_BACKUP_SECTION_BACKUP_CURRENT);
use constant INFO_BACKUP_SECTION_BACKUP_HISTORY     => INFO_BACKUP_SECTION_BACKUP . ':history';
    push @EXPORT, qw(INFO_BACKUP_SECTION_BACKUP_HISTORY);
use constant INFO_BACKUP_SECTION_DB                 => 'db';
    push @EXPORT, qw(INFO_BACKUP_SECTION_DB);
use constant INFO_BACKUP_SECTION_DB_HISTORY         => INFO_BACKUP_SECTION_DB . ':history';
    push @EXPORT, qw(INFO_BACKUP_SECTION_DB_HISTORY);

use constant INFO_BACKUP_KEY_ARCHIVE_CHECK          => MANIFEST_KEY_ARCHIVE_CHECK;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_CHECK);
use constant INFO_BACKUP_KEY_ARCHIVE_COPY           => MANIFEST_KEY_ARCHIVE_COPY;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_COPY);
use constant INFO_BACKUP_KEY_ARCHIVE_START          => MANIFEST_KEY_ARCHIVE_START;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_START);
use constant INFO_BACKUP_KEY_ARCHIVE_STOP           => MANIFEST_KEY_ARCHIVE_STOP;
    push @EXPORT, qw(INFO_BACKUP_KEY_ARCHIVE_STOP);
use constant INFO_BACKUP_KEY_BACKUP_REPO_SIZE       => 'backup-info-repo-size';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_REPO_SIZE);
use constant INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA => 'backup-info-repo-size-delta';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA);
use constant INFO_BACKUP_KEY_BACKUP_SIZE            => 'backup-info-size';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_SIZE);
use constant INFO_BACKUP_KEY_BACKUP_SIZE_DELTA      => 'backup-info-size-delta';
    push @EXPORT, qw(INFO_BACKUP_KEY_BACKUP_SIZE_DELTA);
use constant INFO_BACKUP_KEY_CATALOG                => MANIFEST_KEY_CATALOG;
    push @EXPORT, qw(INFO_BACKUP_KEY_CATALOG);
use constant INFO_BACKUP_KEY_CONTROL                => MANIFEST_KEY_CONTROL;
    push @EXPORT, qw(INFO_BACKUP_KEY_CONTROL);
use constant INFO_BACKUP_KEY_COMPRESS               => MANIFEST_KEY_COMPRESS;
    push @EXPORT, qw(INFO_BACKUP_KEY_COMPRESS);
use constant INFO_BACKUP_KEY_CHECKSUM               => INI_KEY_CHECKSUM;
    push @EXPORT, qw(INFO_BACKUP_KEY_CHECKSUM);
use constant INFO_BACKUP_KEY_DB_VERSION             => MANIFEST_KEY_DB_VERSION;
    push @EXPORT, qw(INFO_BACKUP_KEY_DB_VERSION);
use constant INFO_BACKUP_KEY_FORMAT                 => INI_KEY_FORMAT;
    push @EXPORT, qw(INFO_BACKUP_KEY_FORMAT);
use constant INFO_BACKUP_KEY_HARDLINK               => MANIFEST_KEY_HARDLINK;
    push @EXPORT, qw(INFO_BACKUP_KEY_HARDLINK);
use constant INFO_BACKUP_KEY_HISTORY_ID             => 'db-id';
    push @EXPORT, qw(INFO_BACKUP_KEY_HISTORY_ID);
use constant INFO_BACKUP_KEY_LABEL                  => MANIFEST_KEY_LABEL;
    push @EXPORT, qw(INFO_BACKUP_KEY_LABEL);
use constant INFO_BACKUP_KEY_PRIOR                  => MANIFEST_KEY_PRIOR;
    push @EXPORT, qw(INFO_BACKUP_KEY_PRIOR);
use constant INFO_BACKUP_KEY_REFERENCE              => 'backup-reference';
    push @EXPORT, qw(INFO_BACKUP_KEY_REFERENCE);
use constant INFO_BACKUP_KEY_START_STOP             => MANIFEST_KEY_START_STOP;
    push @EXPORT, qw(INFO_BACKUP_KEY_START_STOP);
use constant INFO_BACKUP_KEY_SYSTEM_ID              => MANIFEST_KEY_SYSTEM_ID;
    push @EXPORT, qw(INFO_BACKUP_KEY_SYSTEM_ID);
use constant INFO_BACKUP_KEY_TIMESTAMP_START        => MANIFEST_KEY_TIMESTAMP_START;
    push @EXPORT, qw(INFO_BACKUP_KEY_TIMESTAMP_START);
use constant INFO_BACKUP_KEY_TIMESTAMP_STOP         => MANIFEST_KEY_TIMESTAMP_STOP;
    push @EXPORT, qw(INFO_BACKUP_KEY_TIMESTAMP_STOP);
use constant INFO_BACKUP_KEY_TYPE                   => MANIFEST_KEY_TYPE;
    push @EXPORT, qw(INFO_BACKUP_KEY_TYPE);
use constant INFO_BACKUP_KEY_VERSION                => INI_KEY_VERSION;
    push @EXPORT, qw(INFO_BACKUP_KEY_VERSION);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name
    my $strBackupClusterPath = shift;   # Backup cluster path

    &log(DEBUG, OP_INFO_BACKUP_NEW . ": backupClusterPath = ${strBackupClusterPath}");

    # Build the backup info path/file name
    my $strBackupInfoFile = "${strBackupClusterPath}/" . FILE_BACKUP_INFO;
    my $bExists = -e $strBackupInfoFile ? true : false;

    # Init object and store variables
    my $self = $class->SUPER::new($strBackupInfoFile, $bExists);

    $self->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, version_get());

    $self->{bExists} = $bExists;
    $self->{strBackupClusterPath} = $strBackupClusterPath;

    # Validate the backup info
    $self->validate();

    return $self;
}

####################################################################################################################################
# validate
#
# Compare the backup info against the actual backups on disk.
####################################################################################################################################
sub validate
{
    my $self = shift;

    # Remove backups that no longer exist on disk
    foreach my $strBackup ($self->keys(INFO_BACKUP_SECTION_BACKUP_CURRENT))
    {
        if (!-e "$self->{strBackupClusterPath}/${strBackup}")
        {
            &log(WARN, "backup ${strBackup} is missing from the repository - removed from " . FILE_BACKUP_INFO);
            $self->remove(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackup);
        }
    }
}

####################################################################################################################################
# check
#
# Check db info and make it is compatible.
####################################################################################################################################
sub check
{
    my $self = shift;
    my $oBackupManifest = shift;

    my $iCatalogVersion = $oBackupManifest->getNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG);
    my $iControlVersion = $oBackupManifest->getNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL);
    my $ullDbSysId = $oBackupManifest->getNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID);
    my $strDbVersion = $oBackupManifest->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION);

    if (!$self->test(INFO_BACKUP_SECTION_DB))
    {
        my $iHistoryId = 1;

        # Fill db section
        $self->setNumeric(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG, undef, $iCatalogVersion);
        $self->setNumeric(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CONTROL, undef, $iControlVersion);
        $self->setNumeric(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_SYSTEM_ID, undef, $ullDbSysId);
        $self->set(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef, $strDbVersion);
        $self->setNumeric(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID, undef, $iHistoryId);

        # Fill db history
        $self->setNumeric(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_CATALOG, $iCatalogVersion);
        $self->setNumeric(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_CONTROL,  $iControlVersion);
        $self->setNumeric(INFO_BACKUP_SECTION_DB_HISTORY, $iHistoryId, INFO_BACKUP_KEY_SYSTEM_ID, $ullDbSysId);
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
}

####################################################################################################################################
# backupAdd
#
# Add a backup to the info file.
####################################################################################################################################
sub backupAdd
{
    my $self = shift;
    my $oFile = shift;
    my $oBackupManifest = shift;

    my $strBackupLabel = $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL);

    &log(DEBUG, OP_INFO_BACKUP_BACKUP_ADD . ": backupLabel = ${strBackupLabel}");

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

            my $strFile = $oFile->path_get(PATH_BACKUP_CLUSTER,
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
    my $strManifestFile = $oFile->path_get(PATH_BACKUP_CLUSTER, "/${strBackupLabel}/" . FILE_MANIFEST);
    my $oStat = lstat($strManifestFile);

    # Check for errors in stat
    defined($oStat)
        or confess &log(ERROR, "unable to lstat ${strManifestFile}");

    $lBackupRepoSize += $oStat->size;
    $lBackupRepoSizeDelta += $oStat->size;

    # Set backup size info
    $self->setNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_SIZE, $lBackupSize);
    $self->setNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_SIZE_DELTA, $lBackupSizeDelta);
    $self->setNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_REPO_SIZE, $lBackupRepoSize);
    $self->setNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_BACKUP_REPO_SIZE_DELTA,
        $lBackupRepoSizeDelta);

    # Store information about the backup into the backup section
    $self->setBool(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_CHECK,
        $oBackupManifest->getBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK));
    $self->setBool(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_COPY,
        $oBackupManifest->getBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_START,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, false));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_ARCHIVE_STOP,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, false));
    $self->setBool(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_COMPRESS,
        $oBackupManifest->getBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS));
    $self->setNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_FORMAT,
        $oBackupManifest->getNumeric(INI_SECTION_BACKREST, INI_KEY_FORMAT));
    $self->setBool(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HARDLINK,
        $oBackupManifest->getBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK));
    $self->setBool(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_START_STOP,
        $oBackupManifest->getBool(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_START_STOP));
    $self->setNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TIMESTAMP_START,
        $oBackupManifest->getNumeric(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START));
    $self->setNumeric(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TIMESTAMP_STOP,
        $oBackupManifest->getNumeric(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_TYPE,
        $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_VERSION,
        $oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION));
    $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HISTORY_ID,
        $self->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID));

    if (!$oBackupManifest->test(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, BACKUP_TYPE_FULL))
    {
        my @stryReference = sort(keys($oReferenceHash));

        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_PRIOR,
            $oBackupManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR));
        $self->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_REFERENCE,
                   \@stryReference);
    }

    # Copy backup info to the history
    $self->set(INFO_BACKUP_SECTION_BACKUP_HISTORY, $strBackupLabel, undef,
        $self->get(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel));

    $self->save();
}

####################################################################################################################################
# backupRemove
#
# Remove a backup from the info file.
####################################################################################################################################
sub backupRemove
{
    my $self = shift;
    my $strBackupLabel = shift;

    &log(DEBUG, OP_INFO_BACKUP_BACKUP_REMOVE . ": backupLabel = ${strBackupLabel}");

    $self->remove(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel);
    $self->save();
}

1;
