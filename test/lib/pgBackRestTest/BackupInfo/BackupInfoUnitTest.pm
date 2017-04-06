####################################################################################################################################
# BackupInfoUnitTest.pm - Unit tests for BackupInfo
####################################################################################################################################
package pgBackRestTest::BackupInfo::BackupInfoUnitTest;
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

use pgBackRest::BackupInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;

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

    # Increment the run, log, and decide whether this unit test should be run
    ################################################################################################################################
    if ($self->begin("BackupInfo::confirmDb()"))
    {
        my $oBackupInfo = new pgBackRest::BackupInfo($self->{strBackupPath}, false, false);
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
