####################################################################################################################################
# Unit tests for BackupInfo
####################################################################################################################################
package pgBackRestTest::Module::Info::InfoInfoBackupPerlTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Archive::Info;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Cipher;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Base;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strRepoPath} = $self->testPath() . '/repo';
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Load options
    $self->configTestClear();
    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
    $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

    # Create backup info path
    storageRepo()->pathCreate(STORAGE_REPO_BACKUP, {bCreateParent => true});

    # Create archive info path
    storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE, {bCreateParent => true});
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $i93ControlVersion = 937;
    my $i93CatalogVersion = 201306121;
    my $i94ControlVersion = 942;
    my $i94CatalogVersion = 201409291;

    ################################################################################################################################
    if ($self->begin("BackupInfo::check() && BackupInfo::confirmDb()"))
    {
        my $oBackupInfo = new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP), false, false,
            {bIgnoreMissing => true});
        $oBackupInfo->create(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), $i93ControlVersion, $i93CatalogVersion, true);

        # All DB section matches
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackupInfo->check(
            PG_VERSION_93, $i93ControlVersion, $i93CatalogVersion, $self->dbSysId(PG_VERSION_93), false)}, 1, 'db section matches');

        # DB section version mismatch
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oBackupInfo->check(PG_VERSION_94, $i93ControlVersion, $i93CatalogVersion,
            $self->dbSysId(PG_VERSION_93))}, ERROR_BACKUP_MISMATCH,
            "database version = " . &PG_VERSION_94 . ", system-id " . $self->dbSysId(PG_VERSION_93) .
            " does not match backup version = " . &PG_VERSION_93 . ", " . "system-id = " . $self->dbSysId(PG_VERSION_93) .
            "\nHINT: is this the correct stanza?");

        # DB section system-id mismatch
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$oBackupInfo->check(PG_VERSION_93, $i93ControlVersion, $i93CatalogVersion, $self->dbSysId(PG_VERSION_94))},
            ERROR_BACKUP_MISMATCH,
            "database version = " . &PG_VERSION_93 . ", system-id " . $self->dbSysId(PG_VERSION_94) .
            " does not match backup version = " . &PG_VERSION_93 . ", " . "system-id = " . $self->dbSysId(PG_VERSION_93) .
            "\nHINT: is this the correct stanza?");

        # DB section control version mismatch
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oBackupInfo->check(PG_VERSION_93, 123, $i93CatalogVersion, $self->dbSysId(PG_VERSION_93))},
            ERROR_BACKUP_MISMATCH, "database control-version = 123, catalog-version $i93CatalogVersion " .
            "does not match backup control-version = $i93ControlVersion, catalog-version = $i93CatalogVersion" .
            "\nHINT: this may be a symptom of database or repository corruption!");

        # DB section catalog version mismatch
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oBackupInfo->check(PG_VERSION_93, $i93ControlVersion, 123456789, $self->dbSysId(PG_VERSION_93))},
            ERROR_BACKUP_MISMATCH, "database control-version = $i93ControlVersion, catalog-version 123456789 " .
            "does not match backup control-version = $i93ControlVersion, catalog-version = $i93CatalogVersion" .
            "\nHINT: this may be a symptom of database or repository corruption!");

        my $strBackupLabel = "20170403-175647F";

        $oBackupInfo->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HISTORY_ID,
            $oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID));

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackupInfo->confirmDb($strBackupLabel, PG_VERSION_93, $self->dbSysId(PG_VERSION_93))}, true,
            'backup db matches');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackupInfo->confirmDb($strBackupLabel, PG_VERSION_94, $self->dbSysId(PG_VERSION_93))}, false,
            'backup db wrong version');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackupInfo->confirmDb($strBackupLabel, PG_VERSION_93, $self->dbSysId(PG_VERSION_94))}, false,
            'backup db wrong system-id');
    }

    ################################################################################################################################
    if ($self->begin("BackupInfo::backupArchiveDbHistoryId()"))
    {
        my $oBackupInfo = new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP), false, false,
            {bIgnoreMissing => true});
        $oBackupInfo->create(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), $i93ControlVersion, $i93CatalogVersion, true);

        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true});
        $oArchiveInfo->create(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), true);

        # Map archiveId to Backup history id
        #---------------------------------------------------------------------------------------------------------------------------
        my $strArchiveId = $oArchiveInfo->archiveId({strDbVersion => PG_VERSION_93, ullDbSysId => $self->dbSysId(PG_VERSION_93)});
        $self->testResult(sub {$oBackupInfo->backupArchiveDbHistoryId($strArchiveId,
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE))}, 1, 'backupArchiveDbHistoryId found');

        # Unable to map archiveId to Backup history id
        #---------------------------------------------------------------------------------------------------------------------------
        $strArchiveId = &PG_VERSION_94 . "-2";
        $self->testResult(sub {$oBackupInfo->backupArchiveDbHistoryId($strArchiveId,
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE))}, "[undef]", 'backupArchiveDbHistoryId not found');

        # Same version but different ullsystemid so continue looking until find match
        #---------------------------------------------------------------------------------------------------------------------------
        # Update db section and db history sections
        my $iHistoryId = $oBackupInfo->dbHistoryIdGet(true)+1;
        $oBackupInfo->dbSectionSet(
            PG_VERSION_94, $i94ControlVersion, $i94CatalogVersion, $self->dbSysId(PG_VERSION_93), $iHistoryId);
        $oArchiveInfo->dbSectionSet(PG_VERSION_94, $self->dbSysId(PG_VERSION_93), $iHistoryId);

        $oBackupInfo->save();
        $oArchiveInfo->save();

        $strArchiveId = &PG_VERSION_94 . "-" . $iHistoryId;
        $self->testResult(sub {$oBackupInfo->backupArchiveDbHistoryId($strArchiveId,
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE))}, $iHistoryId, 'same db version but different system-id');

        # Different version but same ullsystemid
        #---------------------------------------------------------------------------------------------------------------------------
        $iHistoryId = $oBackupInfo->dbHistoryIdGet(false)+1;
        $oBackupInfo->dbSectionSet(
            PG_VERSION_94, $i93ControlVersion, $i93CatalogVersion, $self->dbSysId(PG_VERSION_94), $iHistoryId);
        $oArchiveInfo->dbSectionSet(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), $iHistoryId);

        $oBackupInfo->save();
        $oArchiveInfo->save();

        $strArchiveId = &PG_VERSION_93 . "-" . $iHistoryId;
        $self->testResult(sub {$oBackupInfo->backupArchiveDbHistoryId($strArchiveId,
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE))}, "[undef]", 'same db system-id but different version');

        # First and last version and ullsystemid same in backup.info but only 1st in archive info - return last
        #---------------------------------------------------------------------------------------------------------------------------
        $iHistoryId = $oBackupInfo->dbHistoryIdGet(true)+1;
        $oBackupInfo->dbSectionSet(
            PG_VERSION_93, $i93ControlVersion, $i93CatalogVersion, $self->dbSysId(PG_VERSION_93), $iHistoryId);

        $oBackupInfo->save();

        $strArchiveId = &PG_VERSION_93 . "-1";
        $self->testResult(sub {$oBackupInfo->backupArchiveDbHistoryId($strArchiveId,
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE))}, $iHistoryId, 'duplicate 1st and last version/sysid - last chosen');
    }

    ################################################################################################################################
    if ($self->begin("encryption"))
    {
        # Create a backupInfo file
        my $oBackupInfo = new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP), false, false,
            {bIgnoreMissing => true});
        $oBackupInfo->create(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), $i93ControlVersion, $i93CatalogVersion, true);

        my $strFile = $oBackupInfo->{strFileName};

        # Prepend encryption Magic signature to simulate encryption
        executeTest('echo "' . CIPHER_MAGIC . '$(cat ' . $strFile . ')" > ' . $strFile);

        $self->testException(sub {new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP))}, ERROR_CRYPTO,
            "unable to parse '$strFile'" .
            "\nHINT: Is or was the repo encrypted?");

        # Create encrypted files, change the passphrase and attempt to load - ensure flush error returned as parse error
        #---------------------------------------------------------------------------------------------------------------------------
        # Remove the backup info files
        executeTest('sudo rm ' . $oBackupInfo->{strFileName} . '*');

        # Clear the storage repo settings and change the passphrase
        storageRepoCacheClear();

        my $strCipherPass = 'x';
        $self->configTestClear();
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, $strCipherPass);
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        # Confirm exception when not passing a sub passphrase
        $self->testException(sub {new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP), false, false,
            {bIgnoreMissing => true})}, ERROR_ASSERT,
            'a user passphrase and sub passphrase are both required when encrypting');

        my $strCipherPassSub = cipherPassGen();

        # Create encrypted files
        $oBackupInfo = new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP), false, false,
            {bIgnoreMissing => true, strCipherPassSub => $strCipherPassSub});
        $oBackupInfo->create(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), $i93ControlVersion, $i93CatalogVersion, true);

        # Clear the storage repo settings and change the passphrase
        storageRepoCacheClear();
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, BOGUS);
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        $self->testException(sub {new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP))}, ERROR_CRYPTO,
            "unable to parse '" . $oBackupInfo->{strFileName} . "'" .
            "\nHINT: Is or was the repo encrypted?");
    }
}

1;
