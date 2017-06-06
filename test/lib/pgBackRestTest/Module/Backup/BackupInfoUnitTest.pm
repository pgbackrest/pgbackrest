####################################################################################################################################
# BackupInfoUnitTest.pm - Unit tests for BackupInfo
####################################################################################################################################
package pgBackRestTest::Module::Backup::BackupInfoUnitTest;
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

use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
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

    $self->{strRepoPath} = $self->testPath() . '/repo';
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Load options
    my $oOption = {};
    $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $self->testPath() . '/repo');
    logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

    # Create the local file object
    $self->{oStorage} = storageRepo();

    # Create backup info path
    $self->{oStorage}->pathCreate(STORAGE_REPO_BACKUP, {bCreateParent => true});
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Increment the run, log, and decide whether this unit test should be run
    ################################################################################################################################
    if ($self->begin("BackupInfo::confirmDb()"))
    {
        my $oBackupInfo = new pgBackRest::Backup::Info($self->{oStorage}->pathGet(STORAGE_REPO_BACKUP), false, false,
            {bIgnoreMissing => true});
        $oBackupInfo->create(PG_VERSION_93, WAL_VERSION_93_SYS_ID, '937', '201306121', true);

        my $strBackupLabel = "20170403-175647F";

        $oBackupInfo->set(INFO_BACKUP_SECTION_BACKUP_CURRENT, $strBackupLabel, INFO_BACKUP_KEY_HISTORY_ID,
            $oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_HISTORY_ID));

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackupInfo->confirmDb($strBackupLabel, PG_VERSION_93, WAL_VERSION_93_SYS_ID,)}, true,
            'backup db matches');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackupInfo->confirmDb($strBackupLabel, PG_VERSION_94, WAL_VERSION_93_SYS_ID,)}, false,
            'backup db wrong version');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oBackupInfo->confirmDb($strBackupLabel, PG_VERSION_93, WAL_VERSION_94_SYS_ID,)}, false,
            'backup db wrong system-id');
    }
}

1;
