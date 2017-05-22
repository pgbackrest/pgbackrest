####################################################################################################################################
# StanzaUnitTest.pm - Unit tests for Stanza.pm
####################################################################################################################################
package pgBackRestTest::Module::Stanza::StanzaUnitTest;
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

use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
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

    my $oOption = {};

    $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->optionSetTest($oOption, OPTION_DB_PATH, $self->{strDbPath});
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $self->{strRepoPath});
    $self->optionSetTest($oOption, OPTION_LOG_PATH, $self->testPath());

    $self->optionBoolSetTest($oOption, OPTION_ONLINE, false);

    $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
    $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 6);

    # ??? Currently only contains unit tests for stanza-upgrade. TODO stanza-create

    ################################################################################################################################
    if ($self->begin("Stanza::stanzaUpgrade"))
    {
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_STANZA_UPGRADE); logEnable();

        my $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($self->{strArchivePath}, false);
        $oArchiveInfo->create('9.3', '6999999999999999999', true);

        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath}, false, false);
        $oBackupInfo->create('9.3', '6999999999999999999', '937', '201306121', true);

        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_STANZA_UPGRADE); logEnable();
        my $oStanza = new pgBackRest::Stanza();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oStanza->stanzaUpgrade()}, 0, 'successfully upgraded');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oStanza->stanzaUpgrade()}, 0, 'upgrade not required');
    }

    ################################################################################################################################
    if ($self->begin("Stanza::upgradeCheck"))
    {
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_STANZA_UPGRADE); logEnable();
        my $oStanza = new pgBackRest::Stanza();

        # Create the archive file with current data
        my $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($self->{strArchivePath}, false);
        $oArchiveInfo->create('9.4', 6353949018581704918, true);

        # Create the backup file with outdated data
        my $oBackupInfo = new pgBackRest::Backup::Info($self->{strBackupPath}, false, false);
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

        $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($self->{strArchivePath});
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
