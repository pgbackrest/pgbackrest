####################################################################################################################################
# ArchivePushUnitTest.pm - Unit tests for ArchivePush and ArchivePush Async
####################################################################################################################################
package pgBackRestTest::Stanza::StanzaUnitTest;
use parent 'pgBackRestTest::Common::Env::EnvHostTest';

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
use pgBackRest::BackupInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;
use pgBackRest::Manifest;
use pgBackRest::Stanza;

use pgBackRestTest::Common::Env::EnvHostTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db'; # *
    $self->{strWalPath} = "$self->{strDbPath}/pg_xlog";
    $self->{strWalStatusPath} = "$self->{strWalPath}/archive_status";
    $self->{strWalHash} = "1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";
    $self->{strRepoPath} = $self->testPath() . '/repo'; # *
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza(); # *
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza(); # *
    $self->{strSpoolPath} = "$self->{strArchivePath}/out";

    # Create the local file object
    $self->{oFile} =
        new pgBackRest::File
        (
            $self->stanza(),
            $self->{strRepoPath},
            new pgBackRest::Protocol::Common
            (
                OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
                OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
                OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
                HOST_PROTOCOL_TIMEOUT                       # Protocol timeout
            )
        );
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create WAL path
    filePathCreate($self->{strWalStatusPath}, undef, true, true);

    # Create archive info path
    filePathCreate($self->{strArchivePath}, undef, true, true);

    # Create backup info path
    filePathCreate($self->{strBackupPath}, undef, true, true);

    # Create pg_control path
    filePathCreate(($self->{strDbPath} . '/' . DB_PATH_GLOBAL), undef, false, true);

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

    ################################################################################################################################
    if ($self->begin("Stanza::stanzaUpgrade"))
    {
        my $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($self->{strArchivePath}, false);
        $oArchiveInfo->create('9.3', '6999999999999999999', true);

        my $oBackupInfo = new pgBackRest::BackupInfo($self->{strBackupPath}, false, false);
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
        my $oBackupInfo = new pgBackRest::BackupInfo($self->{strBackupPath}, false, false);
        $oBackupInfo->create('9.3', 6999999999999999999, '937', '201306121', true);

        # Confirm upgrade is needed for backup
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, PATH_BACKUP_CLUSTER, ERROR_BACKUP_MISMATCH)}, true,
            'backup upgrade needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, PATH_BACKUP_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, false,
            'archive upgrade not needed');

        # Change archive file to contain outdated data
        $oArchiveInfo->create('9.3', 6999999999999999999, true);

        # Confirm upgrade is needed for both
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, PATH_BACKUP_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, true,
            'archive upgrade needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, PATH_BACKUP_CLUSTER, ERROR_BACKUP_MISMATCH)}, true,
            'backup upgrade needed');

        # Change the backup file to contain current data
        $oBackupInfo->create('9.4', 6353949018581704918, '942', '201409291', true);

        # Confirm upgrade is needed for archive
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, PATH_BACKUP_CLUSTER, ERROR_BACKUP_MISMATCH)}, false,
            'backup upgrade not needed');
        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, PATH_BACKUP_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, true,
            'archive upgrade needed');

        #---------------------------------------------------------------------------------------------------------------------------
        # Perform an upgrade and then confirm upgrade is not necessary
        $oStanza->process();

        $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($self->{strArchivePath});
        $oBackupInfo = new pgBackRest::BackupInfo($self->{strBackupPath});

        $self->testResult(sub {$oStanza->upgradeCheck($oArchiveInfo, PATH_BACKUP_ARCHIVE, ERROR_ARCHIVE_MISMATCH)}, false,
            'archive upgrade not necessary');
        $self->testResult(sub {$oStanza->upgradeCheck($oBackupInfo, PATH_BACKUP_CLUSTER, ERROR_BACKUP_MISMATCH)}, false,
            'backup upgrade not necessary');

        #---------------------------------------------------------------------------------------------------------------------------
        # Change the DB data
        $oStanza->{oDb}{strDbVersion} = '9.3';
        $oStanza->{oDb}{ullDbSysId} = 6999999999999999999;

        # Pass an expected error that is different than the actual error and confirm an error is thrown
        $self->testException(sub {$oStanza->upgradeCheck($oArchiveInfo, PATH_BACKUP_ARCHIVE, ERROR_ASSERT)}, ERROR_ARCHIVE_MISMATCH,
            "WAL segment version 9.3 does not match archive version 9.4\n" .
            "WAL segment system-id 6999999999999999999 does not match archive system-id 6353949018581704918\n" .
            "HINT: are you archiving to the correct stanza?");
        $self->testException(sub {$oStanza->upgradeCheck($oBackupInfo, PATH_BACKUP_CLUSTER, ERROR_ASSERT)}, ERROR_BACKUP_MISMATCH,
            "database version = 9.3, system-id 6999999999999999999 does not match backup version = 9.4, " .
            "system-id = 6353949018581704918\nHINT: is this the correct stanza?");
    }
}

1;
