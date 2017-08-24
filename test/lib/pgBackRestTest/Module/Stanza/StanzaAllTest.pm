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
use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
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

    # Create pg_control path
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});

    # Copy a pg_control file into the pg_control path
    executeTest(
        'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $self->{strDbPath} . '/' .
        DB_FILE_PGCONTROL);
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
             WAL_VERSION_94_SYS_ID, '942', '201409291', true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_MISSING,
            "archive information missing" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. No backup.info file (backup.info.copy only) and no backup sub-directories or files. Archive.info exists and no
        # archive sub-directories or files
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO);
        (new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true}))->create(PG_VERSION_94,
            WAL_VERSION_94_SYS_ID, true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_MISSING,
            "backup information missing" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # No force. Valid archive.info exists. Invalid backup.info exists.
        #---------------------------------------------------------------------------------------------------------------------------
        (new pgBackRest::Backup::Info($self->{strBackupPath}, false, false))->create(PG_VERSION_94, WAL_VERSION_93_SYS_ID, '942',
            '201409291', true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_INVALID,
            "backup info file invalid" .
            "\nHINT: use stanza-upgrade if the database has been upgraded or use --force");

        # No force. Invalid archive.info exists. Invalid backup.info exists.
        #---------------------------------------------------------------------------------------------------------------------------
        (new pgBackRest::Archive::Info($self->{strArchivePath}, false))->create(PG_VERSION_94, WAL_VERSION_93_SYS_ID, true);
        $self->testException(sub {$oStanza->stanzaCreate()}, ERROR_FILE_INVALID,
            "archive info file invalid" .
            "\nHINT: use stanza-upgrade if the database has been upgraded or use --force");

        # Create stanza without force
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strBackupInfoFile . "*");
        forceStorageRemove(storageRepo(), $strArchiveInfoFile . "*");
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza without force');

        # Create stanza successfully with .info and .info.copy files already existing
        #--------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza without force with existing info files');

        # Remove only backup.info.copy file - confirm stanza create does not throw an error
        #---------------------------------------------------------------------------------------------------------------------------
        forceStorageRemove(storageRepo(), $strBackupInfoFileCopy);
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'no error on missing copy file');
        $self->testResult(sub {storageRepo()->exists($strBackupInfoFile) &&
            !storageRepo()->exists($strBackupInfoFileCopy)},
            true, '    and only backup.info exists');

        # Force on, Repo-Sync off. Archive dir empty. No archive.info file. Backup directory not empty. No backup.info file.
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSetBool(CFGOPT_FORCE, true);
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        forceStorageRemove(storageRepo(), $strBackupInfoFile . "*");
        forceStorageRemove(storageRepo(), $strArchiveInfoFile . "*");
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/12345");
        $oStanza = new pgBackRest::Stanza();
        $self->testResult(sub {$oStanza->stanzaCreate()}, 0, 'successfully created stanza with force');
        $self->testResult(sub {(new pgBackRest::Archive::Info($self->{strArchivePath}))->check(PG_VERSION_94,
            WAL_VERSION_94_SYS_ID) && (new pgBackRest::Backup::Info($self->{strBackupPath}))->check(PG_VERSION_94, '942',
            '201409291', WAL_VERSION_94_SYS_ID)}, 1, '    new info files correct');

        $self->optionTestClear(CFGOPT_FORCE);
    }

    ################################################################################################################################
    if ($self->begin("Stanza::infoFileCreate"))
    {
        $self->configTestLoad(CFGCMD_STANZA_CREATE);
        my $oStanza = new pgBackRest::Stanza();

        my @stryFileList = ('anything');
        my $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true});
        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true});

        # If infoFileCreate is ever called directly, confirm it errors if something other than the info file exists in archive dir
        # when --force is not used
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oStanza->infoFileCreate($oArchiveInfo, STORAGE_REPO_ARCHIVE, $self->{strArchivePath},
            \@stryFileList)}, ERROR_PATH_NOT_EMPTY,
            "archive directory not empty" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # If infoFileCreate is ever called directly, confirm it errors if something other than the info file exists in backup dir
        # when --force is not used
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oStanza->infoFileCreate($oBackupInfo, STORAGE_REPO_BACKUP, $self->{strBackupPath},
            \@stryFileList)}, ERROR_PATH_NOT_EMPTY,
            "backup directory not empty" .
            "\nHINT: use stanza-create --force to force the stanza data to be created.");

        # Set force option --------
        $self->optionTestSetBool(CFGOPT_FORCE, true);

        # Force. Invalid archive.info exists.
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionTestSetBool(CFGOPT_FORCE, true);
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        $oArchiveInfo->create(PG_VERSION_94, 12345, true);
        $oStanza = new pgBackRest::Stanza();
        $self->testResult(sub {$oStanza->infoFileCreate($oArchiveInfo, STORAGE_REPO_ARCHIVE, $self->{strArchivePath},
            \@stryFileList)}, "(0, [undef])", 'force successful for invalid info file');

        # Cause an error to be thrown by changing the permissions of the archive file so it cannot be read for the hash comparison
        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('sudo chmod 220 ' . $self->{strArchivePath} . "/archive.info");
        $self->testResult(sub {$oStanza->infoFileCreate($oArchiveInfo, STORAGE_REPO_ARCHIVE, $self->{strArchivePath},
            \@stryFileList)},
            "(" . &ERROR_FILE_OPEN . ", unable to open '" . $self->{strArchivePath} . "/archive.info': Permission denied)",
            'exception code path');
        executeTest('sudo chmod 640 ' . $self->{strArchivePath} . "/archive.info");

        # Force. Archive dir not empty. Warning returned.
        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($self->{strArchivePath} . "/9.3-0", {bIgnoreExists => true, bCreateParent => true});
        $self->testResult(sub {$oStanza->infoFileCreate($oArchiveInfo, STORAGE_REPO_ARCHIVE, $self->{strArchivePath},
            \@stryFileList)}, "(0, [undef])", 'force successful with archive.info file warning',
            {strLogExpect => "WARN: found empty directory " . $self->{strArchivePath} . "/9.3-0"});

        # Reset force option --------
        $self->optionTestClear(CFGOPT_FORCE);
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

        # Reset force option --------
        $self->optionTestClear(CFGOPT_FORCE);

        # Cause an error to be thrown by changing the permissions of the archive file so it cannot be read
        #---------------------------------------------------------------------------------------------------------------------------
        $self->configTestLoad(CFGCMD_STANZA_CREATE);

        (new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true}))->create(PG_VERSION_94,
             WAL_VERSION_94_SYS_ID, '942', '201409291', true);
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
    }

    ################################################################################################################################
    if ($self->begin("Stanza::upgradeCheck()"))
    {
        $self->configTestLoad(CFGCMD_STANZA_UPGRADE);
        my $oStanza = new pgBackRest::Stanza();

        # Create the archive file with current data
        my $oArchiveInfo = new pgBackRest::Archive::Info($self->{strArchivePath}, false, {bIgnoreMissing => true});
        $oArchiveInfo->create('9.4', 6353949018581704918, true);

        # Create the backup file with outdated data
        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath}, false, false, {bIgnoreMissing => true});
        $oBackupInfo->create('9.3', 6999999999999999999, '937', '201306121', true);

        # Confirm upgrade is needed for backup
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_BACKUP_MISMATCH)}, true,
            'backup upgrade needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, false,
            'archive upgrade not needed');

        # Change archive file to contain outdated data
        $oArchiveInfo->create('9.3', 6999999999999999999, true);

        # Confirm upgrade is needed for both
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, true,
            'archive upgrade needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_BACKUP_MISMATCH)}, true,
            'backup upgrade needed');

        # Change the backup file to contain current data
        $oBackupInfo->create('9.4', 6353949018581704918, '942', '201409291', true);

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
            "WAL segment system-id 6999999999999999999 does not match archive system-id 6353949018581704918\n" .
            "HINT: are you archiving to the correct stanza?");
        $self->testException(sub {$oStanza->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_ASSERT)}, ERROR_BACKUP_MISMATCH,
            "database version = 9.3, system-id 6999999999999999999 does not match backup version = 9.4, " .
            "system-id = 6353949018581704918\nHINT: is this the correct stanza?");
    }
}

1;
