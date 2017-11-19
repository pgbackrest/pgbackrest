####################################################################################################################################
# BackupInfoUnitTest.pm - Unit tests for BackupInfo
####################################################################################################################################
package pgBackRestTest::Module::Archive::ArchiveInfoUnitTest;
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
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
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

    # Clear cache from the previous test
    storageRepoCacheClear($self->stanza());

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

    my $strArchiveTestFile = $self->dataPath() . '/backup.wal1_';

    ################################################################################################################################
    if ($self->begin("Archive::Info::new()"))
    {
        $self->testException(sub {new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE))}, ERROR_FILE_MISSING,
            ARCHIVE_INFO_FILE . " does not exist but is required to push/get WAL segments\n" .
            "HINT: is archive_command configured in postgresql.conf?\n" .
            "HINT: has a stanza-create been performed?\n" .
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.");
    }

    ################################################################################################################################
    if ($self->begin("Archive::Info::reconstruct()"))
    {
        my $tWalContent = $self->walGenerateContent(PG_VERSION_94);
        my $strWalChecksum = $self->walGenerateContentChecksum(PG_VERSION_94);

        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true});

        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1/0000000100000001", {bCreateParent => true});
        my $strArchiveFile = storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1/0000000100000001/") .
            "000000010000000100000001-${strWalChecksum}";
        storageTest()->put($strArchiveFile, $tWalContent);

        $self->testResult(sub {$oArchiveInfo->reconstruct(PG_VERSION_94, $self->dbSysId(PG_VERSION_94))}, "[undef]", 'reconstruct');
        $self->testResult(sub {$oArchiveInfo->check(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), false)}, PG_VERSION_94 . "-1",
            '    check reconstruct');

        # Attempt to reconstruct from an encypted archived WAL for an unencrypted repo
        #---------------------------------------------------------------------------------------------------------------------------
        # Prepend encryption Magic signature to simulate encryption
        executeTest('echo "' . CIPHER_MAGIC . '$(cat ' . $strArchiveFile . ')" > ' . $strArchiveFile);

        $self->testException(sub {$oArchiveInfo->reconstruct(PG_VERSION_94, $self->dbSysId(PG_VERSION_94))}, ERROR_CIPHER,
            "encryption incompatible for '$strArchiveFile'" .
            "\nHINT: Is or was the repo encrypted?");

        executeTest('sudo rm ' . $strArchiveFile);

        # Attempt to reconstruct from an encypted archived WAL with an encrypted repo
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepoCacheClear($self->stanza());
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        # Instantiate an archive.info object with a sub passphrase for the archived WAL
        my $strCipherPassSub = 'y';
        $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true, strCipherPassSub => $strCipherPassSub});

        # Create an encrypted archived WAL
        storageRepo()->put($strArchiveFile, $tWalContent, {strCipherPass => $strCipherPassSub});

        $self->testResult(sub {$oArchiveInfo->reconstruct(PG_VERSION_94, $self->dbSysId(PG_VERSION_94))}, "[undef]", 'reconstruct');
        $self->testResult(sub {$oArchiveInfo->check(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), false)}, PG_VERSION_94 . "-1",
            '    check reconstruction from encrypted archive');

        $oArchiveInfo->save();

        # Confirm encrypted
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE) && ($oArchiveInfo->cipherPassSub() eq $strCipherPassSub)}, true,
            '    new archive info encrypted');
    }

    ################################################################################################################################
    if ($self->begin("Archive::Info::archiveIdList(), check(), archiveId()"))
    {
        my @stryArchiveId;
        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true});

        $oArchiveInfo->create(PG_VERSION_92, $self->dbSysId(PG_VERSION_92), false);
        $oArchiveInfo->dbSectionSet(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), $oArchiveInfo->dbHistoryIdGet(false) + 1);
        $oArchiveInfo->dbSectionSet(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), $oArchiveInfo->dbHistoryIdGet(false) + 10);
        $oArchiveInfo->dbSectionSet(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), $oArchiveInfo->dbHistoryIdGet(false) + 1);
        $oArchiveInfo->save();

        # Check gets only the latest DB and returns only that archiveId
        push(@stryArchiveId, $oArchiveInfo->check(PG_VERSION_93, $self->dbSysId(PG_VERSION_93)));
        $self->testResult(sub {(@stryArchiveId == 1) && ($stryArchiveId[0] eq PG_VERSION_93 . "-13")}, true,
            'check - return only newest archiveId');

        $self->testResult(sub {$oArchiveInfo->archiveId(
            {strDbVersion => PG_VERSION_93, ullDbSysId => $self->dbSysId(PG_VERSION_93)})},
            PG_VERSION_93 . "-13", 'archiveId - return only newest archiveId for multiple histories');

        $self->testException(sub {$oArchiveInfo->archiveId({strDbVersion => PG_VERSION_94, ullDbSysId => BOGUS})}, ERROR_UNKNOWN,
            "unable to retrieve the archive id for database version '" . PG_VERSION_94 . "' and system-id '" . BOGUS . "'");

        $self->testException(sub {$oArchiveInfo->check(PG_VERSION_94, $self->dbSysId(PG_VERSION_94))}, ERROR_ARCHIVE_MISMATCH,
            "WAL segment version " . PG_VERSION_94 . " does not match archive version " . PG_VERSION_93 .
            "\nWAL segment system-id " . $self->dbSysId(PG_VERSION_94) . " does not match archive system-id " .
            $self->dbSysId(PG_VERSION_93) . "\nHINT: are you archiving to the correct stanza?");

        @stryArchiveId = $oArchiveInfo->archiveIdList(PG_VERSION_93, $self->dbSysId(PG_VERSION_93));
        $self->testResult(sub {(@stryArchiveId == 2) && ($stryArchiveId[0] eq PG_VERSION_93 . "-13") &&
            ($stryArchiveId[1] eq PG_VERSION_93 . "-2")}, true, 'archiveIdList - returns multiple archiveId - newest first');

        @stryArchiveId = $oArchiveInfo->archiveIdList(PG_VERSION_94, $self->dbSysId(PG_VERSION_94));
        $self->testResult(sub {(@stryArchiveId == 1) && ($stryArchiveId[0] eq PG_VERSION_94 . "-12")}, true,
            'archiveIdList - returns older archiveId');

        $self->testException(sub {$oArchiveInfo->archiveIdList(PG_VERSION_95, $self->dbSysId(PG_VERSION_94))}, ERROR_UNKNOWN,
            "unable to retrieve the archive id for database version '" . PG_VERSION_95 . "' and system-id '" .
            $self->dbSysId(PG_VERSION_94) . "'");
    }

    ################################################################################################################################
    if ($self->begin("encryption"))
    {
        # Create an unencrypted archive.info file
        #---------------------------------------------------------------------------------------------------------------------------
        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true});
        $oArchiveInfo->create(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), true);

        # Confirm unencrypted
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE)}, false, '    new archive info unencrypted');

        my $strFile = $oArchiveInfo->{strFileName};

        # Prepend encryption Magic signature to simulate encryption
        executeTest('echo "' . CIPHER_MAGIC . '$(cat ' . $strFile . ')" > ' . $strFile);

        $self->testException(sub {new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE))}, ERROR_CIPHER,
            "unable to parse '$strFile'" .
            "\nHINT: Is or was the repo encrypted?");

        # Remove the archive info files
        executeTest('sudo rm ' . $oArchiveInfo->{strFileName} . '*');

        # Create an encrypted storage and archive.info file
        #---------------------------------------------------------------------------------------------------------------------------
        my $strCipherPass = 'x';
        $self->configTestClear();
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, $strCipherPass);
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        # Clear the storage repo settings
        storageRepoCacheClear($self->stanza());

        # Create an encrypted storage and generate an encyption sub passphrase to store in the file
        my $strCipherPassSub = storageRepo()->cipherPassGen();

        # Error on encrypted repo but no passphrase passed to store in the file
        $self->testException(sub {new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true})}, ERROR_ASSERT,
            'a user passphrase and sub passphrase are both required when encrypting');

        # Create an encrypted archiveInfo file
        $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true, strCipherPassSub => $strCipherPassSub});
        $oArchiveInfo->create(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), true);

        # Confirm encrypted
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE)}, true, '    new archive info encrypted');

        $self->testResult(sub {$oArchiveInfo->test(INI_SECTION_CIPHER, INI_KEY_CIPHER_PASS, undef, $strCipherPassSub)},
            true, '    generated passphrase stored');
    }
}

1;
