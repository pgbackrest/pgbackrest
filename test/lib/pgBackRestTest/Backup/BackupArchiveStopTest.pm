####################################################################################################################################
# BackupArchiveStopTest.pm - Tests for archive-push command to make sure aync queue limits are implemented correctly
####################################################################################################################################
package pgBackRestTest::Backup::BackupArchiveStopTest;
use parent 'pgBackRestTest::Backup::BackupCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::ArchiveInfo;
use pgBackRest::BackupInfo;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Backup::BackupCommonTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $strArchiveTestFile = $self->dataPath() . '/backup.wal2_' . WAL_VERSION_94 . '.bin';

    for (my $bRemote = false; $bRemote <= true; $bRemote++)
    {
    for (my $bCompress = false; $bCompress <= true; $bCompress++)
    {
    for (my $iError = 0; $iError <= $bRemote; $iError++)
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("rmt ${bRemote}, cmp ${bCompress}, error " . ($iError ? 'connect' : 'version'))) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oFile) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, bCompress => $bCompress, bArchiveAsync => true});

        # Create the xlog path
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        filePathCreate($strXlogPath, undef, false, true);

        # Create the test path for pg_control and copy pg_control for stanza-create
        filePathCreate(($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL), undef, false, true);
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);

        # Create the archive info file
        $oHostBackup->stanzaCreate('create required data for stanza', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Push a WAL segment
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 1);

        # Break the database version of the archive info file
        if ($iError == 0)
        {
            $oHostBackup->infoMunge(
                $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}});
        }

        # Push two more segments with errors to exceed archive-max-mb
        $oHostDbMaster->archivePush(
            $strXlogPath, $strArchiveTestFile, 2, $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

        $oHostDbMaster->archivePush(
            $strXlogPath, $strArchiveTestFile, 3, $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

        # Now this segment will get dropped
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 4);

        # Fix the database version
        if ($iError == 0)
        {
            $oHostBackup->infoRestore($oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE));
        }

        # Remove the stop file
        fileRemove($oHostDbMaster->spoolPath() . '/stop/db-archive.stop');

        # Check the dir to be sure that segment 2 and 3 were not pushed yet
        executeTest(
            'ls -1R ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . "-1/0000000100000001",
            {oLogTest => $self->expect(), bRemote => $bRemote});

        # Push segment 5
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 5, undef, false);

        # Check that 5 is pushed
        executeTest(
            'ls -1R ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . "-1/0000000100000001",
            {oLogTest => $self->expect(), bRemote => $bRemote});

        # Call push without a segment
        $oHostDbMaster->archivePush($strXlogPath);

        # Check the dir to be sure that segment 2 and 3 were pushed
        executeTest(
            'ls -1R ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . "-1/0000000100000001",
            {oLogTest => $self->expect(), bRemote => $bRemote});
    }
    }
    }
}

1;
