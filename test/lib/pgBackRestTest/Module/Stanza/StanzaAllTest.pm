####################################################################################################################################
# StanzaAllTest.pm - Unit tests for Stanza.pm
####################################################################################################################################
package pgBackRestTest::Module::Stanza::StanzaAllTest;
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

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Info;
use pgBackRest::Backup::Common;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Cipher;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Stanza;
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
    $self->{strSpoolPath} = "$self->{strArchivePath}/out";
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create archive info path
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});

    # Create backup info path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});

    # Generate pg_control file
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
    $self->controlGenerate($self->{strDbPath}, PG_VERSION_94);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_DB_PATH, $self->{strDbPath});
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());

    $self->optionTestSetBool(CFGOPT_ONLINE, false);

    $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
    $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 6);

    my $iDbControl = 942;
    my $iDbCatalog = 201409291;

    ################################################################################################################################
    if ($self->begin("Stanza::new"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSetBool(CFGOPT_ONLINE, true);
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        $self->testException(sub {(new pgBackRest::Stanza())}, ERROR_DB_CONNECT,
            "could not connect to server: No such file or directory\n");

        $self->optionTestSetBool(CFGOPT_ONLINE, false);
    }

    ################################################################################################################################
    if ($self->begin("Stanza::process()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->configTestLoad(CFGCMD_CHECK);
        my $oStanza = new pgBackRest::Stanza();

        $self->testException(sub {$oStanza->process()}, ERROR_ASSERT,
            "stanza->process() called with invalid command: " . cfgCommandName(CFGCMD_CHECK));

        #---------------------------------------------------------------------------------------------------------------------------
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        rmdir($self->{strArchivePath});
        rmdir($self->{strBackupPath});
        $self->testResult(sub {$oStanza->process()}, 0, 'parent paths recreated successfully');
    }

    ################################################################################################################################
    if ($self->begin("Stanza::stanzaCreate()"))
    {
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        my $oStanza = new pgBackRest::Stanza();

        my $strBackupInfoFile = storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO);
        my $strBackupInfoFileCopy = storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO . INI_COPY_EXT);
        my $strArchiveInfoFile = storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE);

        # No force. Archive dir not empty. No archive.info file. Backup directory empty.
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/9.4-1");
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "archive directory not empty" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. Archive dir not empty. No archive.info file. Backup directory not empty. No backup.info file.
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/12345");
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "backup directory and/or archive directory not empty" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. Archive dir empty. No archive.info file. Backup directory not empty. No backup.info file.
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), STORAGE_REPO_ARCHIVE . "/9.4-1", {bRecurse => true});
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "backup directory not empty" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. No archive.info file and no archive sub-directories or files. Backup.info exists and no backup sub-directories
        # or files
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), STORAGE_REPO_BACKUP . "/12345", {bRecurse => true});
        (new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true}))->create(PG_VERSION_94,
             $self->dbSysId(PG_VERSION_94), $iDbControl, $iDbCatalog, true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_MISSING,
            "archive information missing" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. No backup.info file (backup.info.copy only) and no backup sub-directories or files. Archive.info exists and no
        # archive sub-directories or files
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO);
        (new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true}))->create(PG_VERSION_94,
            $self->dbSysId(PG_VERSION_94), true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_MISSING,
            "backup information missing" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. archive.info DB mismatch. backup.info correct DB.
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strArchiveInfoFile . "*");
        forceStorageRemove(storageRepo(), $strBackupInfoFile . "*");
        (new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true}))->create(PG_VERSION_93,
            $self->dbSysId(PG_VERSION_93), true);
        (new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true}))->create(PG_VERSION_94,
             $self->dbSysId(PG_VERSION_94), $iDbControl, $iDbCatalog, true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_INVALID,
            "backup info file or archive info file invalid\n" .
            "HINT: use stanza-upgrade if the database has been upgraded or use --force");

        # No force. archive.info DB mismatch. backup.info DB mismatch.
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strBackupInfoFile . "*");
        (new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true}))->create(PG_VERSION_93,
             $self->dbSysId(PG_VERSION_93), $iDbControl, $iDbCatalog, true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_INVALID,
            "backup info file or archive info file invalid\n" .
            "HINT: use stanza-upgrade if the database has been upgraded or use --force");

        # No force. Create stanza.
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strBackupInfoFile . "*");
        forceStorageRemove(storageRepo(), $strArchiveInfoFile . "*");
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza without force');

        # No force with .info and .info.copy files already existing
        #--------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "backup directory and/or archive directory not empty" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. Remove only backup.info.copy file - confirm stanza create still throws an error since force was not used
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strBackupInfoFileCopy);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "backup directory and/or archive directory not empty" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # Force on. Valid archive.info exists. Invalid backup.info exists.
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSetBool(CFGOPT_FORCE, true);
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        forceStorageRemove(storageRepo(), $strBackupInfoFile . "*");
        (new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true}))->create(PG_VERSION_94,
            $self->dbSysId(PG_VERSION_93), $iDbControl, $iDbCatalog, true);

        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza with force and existing info files');
        $self->testResult(sub {(new pgBackRest::Backup::Info($self->{strBackupPath}))->check(PG_VERSION_94,
            $iDbControl, $iDbCatalog, $self->dbSysId(PG_VERSION_94))}, 2, '    backup.info reconstructed');

        # Force on, Repo-Sync off. Archive dir empty. No archive.info file. Backup directory not empty. No backup.info file.
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strBackupInfoFile . "*");
        forceStorageRemove(storageRepo(), $strArchiveInfoFile . "*");
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/12345");
        $oStanza = new pgBackRest::Stanza();
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza with force');
        $self->testResult(
            sub {(new pgBackRest::Archive::Info($self->{strArchivePath}))->check(
                PG_VERSION_94,
                $self->dbSysId(PG_VERSION_94)) && (new pgBackRest::Backup::Info($self->{strBackupPath}))->check(PG_VERSION_94,
                $iDbControl, $iDbCatalog, $self->dbSysId(PG_VERSION_94))},
            1, '    new info files correct');
        forceStorageRemove(storageRepo(), storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/12345"), {bRecurse => true});

        # Force on. Attempt to change encryption on the repo
        #---------------------------------------------------------------------------------------------------------------------------
        # Create unencrypted archive file
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1");
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1/0000000100000001");
        my $strArchiveIdPath = storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1");
        my $strArchivedFile = storageRepo()->pathGet($strArchiveIdPath .
            "/0000000100000001/000000010000000100000001-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27");
        executeTest('cp ' . $self->dataPath() . "/filecopy.archive2.bin ${strArchivedFile}");

        # Create unencrypted backup manifest file
        my $strBackupLabel = timestampFileFormat(undef, 1482000000) . 'F';
        my $strBackupPath = storageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strBackupLabel}");

        my $strBackupManifestFile = "$strBackupPath/" . FILE_MANIFEST;
        my $oBackupManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});
        storageRepo()->pathCreate($strBackupPath);
        $oBackupManifest->save();

        # Get the unencrypted content for later encryption
        my $tUnencryptedArchiveContent = ${storageRepo()->get($strArchivedFile)};
        my $tUnencryptedBackupContent = ${storageRepo()->get($strBackupManifestFile)};

        # Change the permissions on the archived file so reconstruction fails
        executeTest('sudo chmod 220 ' . $strArchivedFile);
        $self->testException(sub {(new pgBackRest::Stanza())->stanzaCreate()}, ERROR_FILE_OPEN,
            "unable to open '" . $strArchivedFile . "': Permission denied");
        executeTest('sudo chmod 644 ' . $strArchivedFile);

        # Clear the cached repo settings and change repo settings to encrypted
        storageRepoCacheClear($self->stanza());
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        $self->testException(sub {(new pgBackRest::Stanza())->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "files exist - the encryption type or passphrase cannot be changed");

        # Remove the backup sub directories and files so that the archive file is attempted to be read
        forceStorageRemove(storageRepo(), $strBackupPath, {bRecurse => true});
        $self->testException(sub {(new pgBackRest::Stanza())->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "files exist - the encryption type or passphrase cannot be changed");

        # Remove the archive sub directories and files so that only the info files exist - stanza create is allowed with force
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strArchiveIdPath, {bRecurse => true});
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza with force and new encrypted settings');

        # Confirm encrypted
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE)}, true, '    new archive info encrypted');
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_BACKUP) . '/'
            . FILE_BACKUP_INFO)}, true, '    new backup info encrypted');

        # Store the unencrypted archived file as encrypted and check stanza-create
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1");
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1/0000000100000001");
        storageRepo()->put($strArchivedFile, $tUnencryptedArchiveContent);
        storageRepo()->pathCreate($strBackupPath); # Empty backup path - no backup in progress

        # Confirm encrypted and create the stanza with force
        $self->testResult(sub {storageRepo()->encrypted($strArchivedFile)}, true, 'new archive WAL encrypted');
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, '    successfully recreate stanza with force from encrypted WAL');

        # Confirm the backup and archive info are encrypted and check the contents
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE)}, true, '    new archive info encrypted');
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_BACKUP) . '/'
            . FILE_BACKUP_INFO)}, true, '    new backup info encrypted');
        $self->testResult(
            sub {(new pgBackRest::Archive::Info($self->{strArchivePath}))->check(
                PG_VERSION_94,
                $self->dbSysId(PG_VERSION_94)) && (new pgBackRest::Backup::Info($self->{strBackupPath}))->check(PG_VERSION_94,
                $iDbControl, $iDbCatalog, $self->dbSysId(PG_VERSION_94))},
            1, '    new archive.info and backup.info files correct');

        # Store the unencrypted backup.manifest file as encrypted and check stanza-create
        #---------------------------------------------------------------------------------------------------------------------------
        # Try to create a manifest without a passphrase in an encrypted storage
        $self->testException(sub {new pgBackRest::Manifest($strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94})}, ERROR_CIPHER,
            'passphrase is required when storage is encrypted');

        # Get the encryption passphrase and create the new manifest
        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath});
        $oBackupManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            strCipherPass => $oBackupInfo->cipherPassSub(), strCipherPassSub => cipherPassGen()});

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, $strBackupLabel);
        $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef, true);
        $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef, false);
        $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_BACKUP_STANDBY, undef, false);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, 1);
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef, 1);
        $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_CHECKSUM_PAGE, undef, true);
        $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, true);
        $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef, false);
        $oBackupManifest->boolSet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ONLINE, undef, true);
        $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef, time());
        $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef, time());
        $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef, CFGOPTVAL_BACKUP_TYPE_FULL);

        $oBackupManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_94);
        $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef, $iDbControl);
        $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iDbCatalog);
        $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef, $self->dbSysId(PG_VERSION_94));
        $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID, undef, 1);
        $oBackupManifest->save();

        # Confirm encrypted and create the stanza with force
        $self->testResult(sub {storageRepo()->encrypted($strBackupManifestFile)}, true, 'new backup manifest encrypted');
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0,
            '    successfully recreate stanza with force from encrypted manifest and WAL');

        # Confirm the backup and archive info are encrypted and check the contents
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE)}, true, '    recreated archive info encrypted');
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_BACKUP) . '/'
            . FILE_BACKUP_INFO)}, true, '    recreated backup info encrypted');
        $self->testResult(
            sub {(new pgBackRest::Archive::Info($self->{strArchivePath}))->check(
                PG_VERSION_94,
                $self->dbSysId(PG_VERSION_94)) && (new pgBackRest::Backup::Info($self->{strBackupPath}))->check(PG_VERSION_94,
                $iDbControl, $iDbCatalog, $self->dbSysId(PG_VERSION_94))},
            1, '    recreated archive.info and backup.info files correct');

        # Move the encrypted info files out of the repo so they are missing but backup exists. --force cannot be used
        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('sudo mv ' . storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/' . ARCHIVE_INFO_FILE . '* ' .
            $self->testPath() . '/');
        executeTest('sudo mv ' . storageRepo()->pathGet(STORAGE_REPO_BACKUP) . '/' . FILE_BACKUP_INFO . '* ' .
            $self->testPath() . '/');

        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "backup directory and/or archive directory not empty and repo is encrypted and info file(s) are missing," .
            " --force cannot be used");

        # Move the files back for the next test
        executeTest('sudo mv ' . $self->testPath() . '/' . ARCHIVE_INFO_FILE . '* ' .
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/');
        executeTest('sudo mv ' . $self->testPath() . '/' . FILE_BACKUP_INFO . '* ' .
            storageRepo()->pathGet(STORAGE_REPO_BACKUP) . '/');

        # Change repo encryption settings to unencrypted - stanza create is not allowed even with force
        #---------------------------------------------------------------------------------------------------------------------------
        # Clear the cached repo settings and change repo settings to unencrypted
        storageRepoCacheClear($self->stanza());
        $self->optionTestClear(CFGOPT_REPO_CIPHER_TYPE);
        $self->optionTestClear(CFGOPT_REPO_CIPHER_PASS);
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_PATH_NOT_EMPTY,
            "files exist - the encryption type or passphrase cannot be changed");

        # With only info files - stanza create is allowed with force
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strArchiveIdPath, {bRecurse => true});
        forceStorageRemove(storageRepo(), $strBackupPath, {bRecurse => true});
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza with force and new unencrypted settings');

        # Confirm unencrypted
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE)}, false, '    new archive info unencrypted');
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_BACKUP) . '/'
            . FILE_BACKUP_INFO)}, false, '    new backup info unencrypted');

        # Clear --force
        $self->optionTestClear(CFGOPT_FORCE);
    }

    ################################################################################################################################
    if ($self->begin("Stanza::infoFileCreate"))
    {
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        my $oStanza = new pgBackRest::Stanza();
        my $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true});

        # Archive dir not empty. Warning returned.
        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($self->{strArchivePath} . "/9.3-0", {bIgnoreExists => true, bCreateParent => true});
        $self->testResult(sub {$oStanza->infoFileCreate($oArchiveInfo)}, "(0, [undef])",
            'successful with archive.info file warning',
            {strLogExpect => "WARN: found empty directory " . $self->{strArchivePath} . "/9.3-0"});
    }

    ################################################################################################################################
    if ($self->begin("Stanza::infoObject()"))
    {
        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);
        my $oStanza = new pgBackRest::Stanza();

        $self->testException(sub {$oStanza->infoObject(STORAGE_REPO_BACKUP, $self->{strBackupPath})}, ERROR_FILE_MISSING,
            storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO) .
            " does not exist and is required to perform a backup." .
            "\nHINT: has a stanza-create been performed?");

        # Force valid but not set.
        #---------------------------------------------------------------------------------------------------------------------------
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        $oStanza = new pgBackRest::Stanza();

        $self->testException(sub {$oStanza->infoObject(STORAGE_REPO_BACKUP, $self->{strBackupPath})}, ERROR_FILE_MISSING,
            storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO) .
            " does not exist and is required to perform a backup." .
            "\nHINT: has a stanza-create been performed?" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # Force.
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSetBool(CFGOPT_FORCE, true);
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        $self->testResult(sub {$oStanza->infoObject(STORAGE_REPO_ARCHIVE, $self->{strArchivePath})}, "[object]",
            'archive force successful');
        $self->testResult(sub {$oStanza->infoObject(STORAGE_REPO_BACKUP, $self->{strBackupPath})}, "[object]",
            'backup force successful');

        # Cause an error to be thrown by changing the permissions of the archive directory so it cannot be read
        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('sudo chmod 220 ' . $self->{strArchivePath});
        $self->testException(sub {$oStanza->infoObject(STORAGE_REPO_ARCHIVE, $self->{strArchivePath})}, ERROR_FILE_OPEN,
            "unable to open '" . $self->{strArchivePath} . "/archive.info': Permission denied");
        executeTest('sudo chmod 640 ' . $self->{strArchivePath});

        # Reset force option --------
        $self->optionTestClear(CFGOPT_FORCE);

        # Cause an error to be thrown by changing the permissions of the backup file so it cannot be read
        #---------------------------------------------------------------------------------------------------------------------------
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        (new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true}))->create(PG_VERSION_94,
             $self->dbSysId(PG_VERSION_94), $iDbControl, $iDbCatalog, true);
        forceStorageRemove(storageRepo(), storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO . INI_COPY_EXT));
        executeTest('sudo chmod 220 ' . storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO));
        $self->testException(sub {$oStanza->infoObject(STORAGE_REPO_BACKUP, $self->{strBackupPath})}, ERROR_FILE_OPEN,
            "unable to open '" . storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO) .
            "': Permission denied");
        executeTest('sudo chmod 640 ' . storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO));
    }

    ################################################################################################################################
    if ($self->begin("Stanza::stanzaUpgrade()"))
    {
        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);

        my $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true});
        $oArchiveInfo->create('9.3', '6999999999999999999', true);

        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true});
        $oBackupInfo->create('9.3', '6999999999999999999', '937', '201306121', true);

        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);
        my $oStanza = new pgBackRest::Stanza();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oStanza->stanzaUpgrade()}, 0, 'successfully upgraded');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oStanza->stanzaUpgrade()}, 0, 'upgrade not required');

        # Attempt to change the encryption settings
        #---------------------------------------------------------------------------------------------------------------------------
        # Clear the cached repo settings and change repo settings to encrypted
        storageRepoCacheClear($self->stanza());
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);

        $self->testException(sub {$oStanza->stanzaUpgrade()}, ERROR_CIPHER,
            "unable to parse '" . $self->{strArchivePath} . "/archive.info'" .
            "\nHINT: Is or was the repo encrypted?");

        forceStorageRemove(storageRepo(), storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO) . "*");
        forceStorageRemove(storageRepo(), storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE) . "*");

        # Create encrypted info files with prior passphrase then attempt to change
        #---------------------------------------------------------------------------------------------------------------------------
        $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true,
            strCipherPassSub => cipherPassGen()});
        $oArchiveInfo->create(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), true);

        $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true,
            strCipherPassSub => cipherPassGen()});
        $oBackupInfo->create(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), '937', '201306121', true);

         # Attempt to upgrade with a different passphrase
        storageRepoCacheClear($self->stanza());
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'y');
        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);

        $self->testException(sub {$oStanza->stanzaUpgrade()}, ERROR_CIPHER,
            "unable to parse '" . $self->{strArchivePath} . "/archive.info'" .
            "\nHINT: Is or was the repo encrypted?");

        storageRepoCacheClear($self->stanza());
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);

        # Create encrypted archived file
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_93 . "-1");
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_93 . "-1/0000000100000001");
        my $strArchiveIdPath = storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_93 . "-1");
        my $strArchivedFile = storageRepo()->pathGet($strArchiveIdPath .
            "/0000000100000001/000000010000000100000001-" . $self->walGenerateContentChecksum(PG_VERSION_93));
        storageRepo()->put(
            $strArchivedFile, $self->walGenerateContent(PG_VERSION_93), {strCipherPass => $oArchiveInfo->cipherPassSub()});
        $self->testResult(sub {storageRepo()->encrypted($strArchivedFile)}, true, 'created encrypted archive WAL');

        # Upgrade
        $self->testResult(sub {$oStanza->stanzaUpgrade()}, 0, '    successfully upgraded');
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE) . '/'
            . ARCHIVE_INFO_FILE)}, true, '    upgraded archive info encrypted');
        $self->testResult(sub {storageRepo()->encrypted(storageRepo()->pathGet(STORAGE_REPO_BACKUP) . '/'
            . FILE_BACKUP_INFO)}, true, '    upgraded backup info encrypted');

        $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath});
        $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath});
        my $hHistoryArchive = $oArchiveInfo->dbHistoryList();
        my $hHistoryBackup = $oBackupInfo->dbHistoryList();

        $self->testResult(sub {($hHistoryArchive->{1}{&INFO_DB_VERSION} eq PG_VERSION_93) &&
            ($hHistoryArchive->{1}{&INFO_SYSTEM_ID} eq $self->dbSysId(PG_VERSION_93)) &&
            ($hHistoryArchive->{2}{&INFO_DB_VERSION} eq PG_VERSION_94) &&
            ($hHistoryArchive->{2}{&INFO_SYSTEM_ID} eq $self->dbSysId(PG_VERSION_94)) &&
            ($hHistoryBackup->{1}{&INFO_DB_VERSION} eq PG_VERSION_93) &&
            ($hHistoryBackup->{1}{&INFO_SYSTEM_ID} eq $self->dbSysId(PG_VERSION_93)) &&
            ($hHistoryBackup->{2}{&INFO_DB_VERSION} eq PG_VERSION_94) &&
            ($hHistoryBackup->{2}{&INFO_SYSTEM_ID} eq $self->dbSysId(PG_VERSION_94)) &&
            ($oArchiveInfo->check(PG_VERSION_94, $self->dbSysId(PG_VERSION_94)) eq PG_VERSION_94 . "-2") &&
            ($oBackupInfo->check(PG_VERSION_94, $iDbControl, $iDbCatalog, $self->dbSysId(PG_VERSION_94)) == 2) }, true,
            '    encrypted archive and backup info files upgraded');

        # Clear configuration
        storageRepoCacheClear($self->stanza());
        $self->optionTestClear(CFGOPT_REPO_CIPHER_TYPE);
        $self->optionTestClear(CFGOPT_REPO_CIPHER_PASS);
    }

    ################################################################################################################################
    if ($self->begin("Stanza::upgradeCheck()"))
    {
        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);
        my $oStanza = new pgBackRest::Stanza();

        # Create the archive file with current data
        my $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true});
        $oArchiveInfo->create(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), true);

        # Create the backup file with outdated data
        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true});
        $oBackupInfo->create(PG_VERSION_93, 6999999999999999999, '937', '201306121', true);

        # Confirm upgrade is needed for backup
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_BACKUP_MISMATCH)}, true,
            'backup upgrade needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, false,
            'archive upgrade not needed');

        # Change archive file to contain outdated data
        $oArchiveInfo->create(PG_VERSION_93, 6999999999999999999, true);

        # Confirm upgrade is needed for both
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, true,
            'archive upgrade needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_BACKUP_MISMATCH)}, true,
            'backup upgrade needed');

        # Change the backup file to contain current data
        $oBackupInfo->create(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), $iDbControl, $iDbCatalog, true);

        # Confirm upgrade is needed for archive
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_BACKUP_MISMATCH)}, false,
            'backup upgrade not needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, true,
            'archive upgrade needed');

        #---------------------------------------------------------------------------------------------------------------------------
        # Perform an upgrade and then confirm upgrade is not necessary
        $oStanza->process();

        $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath});
        $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath});

        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, false,
            'archive upgrade not necessary');
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_BACKUP_MISMATCH)}, false,
            'backup upgrade not necessary');

        #---------------------------------------------------------------------------------------------------------------------------
        # Change the DB data
        $oStanza->{oDb}{strDbVersion} = '9.3';
        $oStanza->{oDb}{ullDbSysId} = 6999999999999999999;

        # Pass an expected error that is different than the actual error and confirm an error is thrown
        $self->testException(sub {$oStanza->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ASSERT)},
            ERROR_ARCHIVE_MISMATCH,
            "WAL segment version 9.3 does not match archive version 9.4\n" .
            'WAL segment system-id 6999999999999999999 does not match archive system-id ' . $self->dbSysId(PG_VERSION_94) . "\n" .
            "HINT: are you archiving to the correct stanza?");
        $self->testException(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_ASSERT)}, ERROR_BACKUP_MISMATCH,
            "database version = 9.3, system-id 6999999999999999999 does not match backup version = 9.4, " .
            'system-id = ' . $self->dbSysId(PG_VERSION_94) . "\nHINT: is this the correct stanza?");
    }

    ################################################################################################################################
    if ($self->begin("Stanza::errorForce()"))
    {
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        my $oStanza = new pgBackRest::Stanza();

        my $strMessage = "archive information missing" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.";

        $self->testException(sub {$oStanza->errorForce($strMessage, ERROR_FILE_MISSING, undef, true,
            $self->{strArchivePath}, $self->{strBackupPath})}, ERROR_FILE_MISSING, $strMessage);

        my $strFile = $self->{strArchivePath} . qw{/} . 'file.txt';
        my $strFileContent = 'TESTDATA';

        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        $self->testException(sub {$oStanza->errorForce($strMessage, ERROR_FILE_MISSING, $strFile, true,
            $self->{strArchivePath}, $self->{strBackupPath})}, ERROR_FILE_MISSING, $strMessage);
    }

    ################################################################################################################################
    if ($self->begin("Stanza::stanzaDelete()"))
    {
        # Create the stanza
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        my $oStanza = new pgBackRest::Stanza();
        $oStanza->stanzaCreate();

        # Attempt to delete without running stop
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestClear(CFGOPT_ONLINE);
        $self->configTestLoad(CFGCMD_STANZA_DELETE);

        $self->testException(sub {$oStanza->stanzaDelete()}, ERROR_FILE_MISSING,
            "stop file does not exist for stanza '" . $self->stanza() . "'" .
            "\nHINT: has the pgbackrest stop command been run on this server?");

        # Create a stop file and attempt to delete with postgres running
        #---------------------------------------------------------------------------------------------------------------------------
        lockStop();

        # Simulate postgres still running
        executeTest('touch ' . $self->{strDbPath} . qw(/) . DB_FILE_POSTMASTERPID);

        $self->testException(sub {$oStanza->stanzaDelete()}, ERROR_POSTMASTER_RUNNING,
            DB_FILE_POSTMASTERPID . " exists - looks like the postmaster is running. " .
            "To delete stanza '" . $self->stanza() . "', shutdown the postmaster for stanza '" . $self->stanza() .
            "' and try again, or use --force.");

        # Force deletion
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSetBool(CFGOPT_FORCE, true);
        $self->configTestLoad(CFGCMD_STANZA_DELETE);

        $self->testResult(sub {$oStanza->stanzaDelete()}, 0, 'successfully delete stanza with force');
        $self->testResult(sub {storageRepo()->pathExists($self->{strArchivePath}) ||
            storageRepo()->pathExists($self->{strBackupPath})},
            false, '    neither archive nor backup repo paths for the stanza exist');

        # Remove postmaster.pid and clear force
        storageTest()->remove($self->{strDbPath} . qw(/) . DB_FILE_POSTMASTERPID);
        $self->optionTestClear(CFGOPT_FORCE);

        # Rerun stanza-delete without force and with missing stanza directories
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oStanza->stanzaDelete()}, 0, 'successful - stanza already deleted');

        # Recursive dir delete with archive directory and stanza directory but missing info files
        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1", {bCreateParent => true});
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_94 . "-1/0000000100000001");
        executeTest('touch ' . $self->{strArchivePath} . "/" . PG_VERSION_94 . "-1/0000000100000001/" . BOGUS);

        storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});
        my $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, 1482000000);
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strFullLabel}", {bCreateParent => true});
        executeTest('touch ' . $self->{strBackupPath} . "/${strFullLabel}/" . BOGUS);

        # Create an inaccessible file
        executeTest("sudo chgrp 777 " . $self->{strBackupPath} . "/${strFullLabel}/" . BOGUS);
        executeTest("sudo chown 777 " . $self->{strBackupPath} . "/${strFullLabel}/" . BOGUS);

        lockStop();
        $self->testResult(sub {$oStanza->stanzaDelete()}, 0,
            'successful - recursive delete with missing info files and inaccessible file');
        $self->testResult(sub {storageRepo()->pathExists($self->{strArchivePath}) ||
            storageRepo()->pathExists($self->{strBackupPath})},
            false, '    neither archive nor backup repo paths for the stanza exist');

        # Make the archive directory inaccessible
        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});

        executeTest("sudo chgrp 7777 " . $self->{strArchivePath});
        executeTest("sudo chown 7777 " . $self->{strArchivePath});

        lockStop();
        $self->testException(sub {$oStanza->stanzaDelete()}, ERROR_FILE_OPEN,
            "unable to remove file '" . $self->{strArchivePath} . "/" . ARCHIVE_INFO_FILE . "': Permission denied");

        # Remove the repo
        executeTest("sudo rm -rf " . $self->{strArchivePath});

        # Clear the cached repo settings and change repo settings to encrypted
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepoCacheClear($self->stanza());
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created encrypted stanza');

        # Create encrypted archived file
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_93 . "-1");
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_93 . "-1/0000000100000001");
        my $strArchiveIdPath = storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/" . PG_VERSION_93 . "-1");
        my $strArchivedFile = storageRepo()->pathGet($strArchiveIdPath .
            "/0000000100000001/000000010000000100000001-" . $self->walGenerateContentChecksum(PG_VERSION_93));
        my $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath});
        storageRepo()->put(
            $strArchivedFile, $self->walGenerateContent(PG_VERSION_93), {strCipherPass => $oArchiveInfo->cipherPassSub()});
        $self->testResult(sub {storageRepo()->encrypted($strArchivedFile)}, true, '    created encrypted archive WAL');

        my $strBackupPath = storageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strFullLabel}");
        my $strBackupManifestFile = "$strBackupPath/" . FILE_MANIFEST;
        my $iDbCatalogVersion = 201409291;

        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strFullLabel}", {bCreateParent => true});
        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath});

        $self->testResult(sub {(new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            strCipherPass => $oBackupInfo->cipherPassSub(), strCipherPassSub => 'x'}))->save()},
            "[undef]", '    manifest saved');

        lockStop();
        $self->testResult(sub {$oStanza->stanzaDelete()}, 0,
            '    successful - recursive delete on encrypted repo');
        $self->testResult(sub {storageRepo()->pathExists($self->{strArchivePath}) ||
            storageRepo()->pathExists($self->{strBackupPath})},
            false, '    neither archive nor backup repo paths for the stanza exist');

        # For test coverage: create new stanza with delete command, call process and remove only backup path
        #---------------------------------------------------------------------------------------------------------------------------
        lockStop();
        $self->configTestLoad(CFGCMD_STANZA_DELETE);
        $oStanza = new pgBackRest::Stanza();
        storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});
        $self->testResult(sub {$oStanza->process()}, 0,
            'successfully remove backup path');
        $self->testResult(sub {storageRepo()->pathExists($self->{strArchivePath}) ||
            storageRepo()->pathExists($self->{strBackupPath})},
            false, '    neither archive nor backup repo paths for the stanza exist');
    }
}

1;
