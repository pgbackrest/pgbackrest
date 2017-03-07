####################################################################################################################################
# ArchiveStopTest.pm - Tests for archive-push command to make sure aync queue limits are implemented correctly
####################################################################################################################################
package pgBackRestTest::Archive::ArchiveStopTest;
use parent 'pgBackRestTest::Common::Env::EnvHostTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Archive::ArchiveInfo;
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

use pgBackRestTest::Common::Env::EnvHostTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

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

        # Create compression extension
        my $strCompressExt = $bCompress ? ".$oFile->{strCompressExtension}" : '';

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

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oFile->list(PATH_BACKUP_ARCHIVE, PG_VERSION_94 . '-1/0000000100000001')},
            "000000010000000100000001-72b9da071c13957fb4ca31f05dbd5c644297c2f7${strCompressExt}",
            'segment 2-4 are not pushed', 5);

        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 5);

        $self->testResult(
            sub {$oFile->list(PATH_BACKUP_ARCHIVE, PG_VERSION_94 . '-1/0000000100000001')},
            "(000000010000000100000001-72b9da071c13957fb4ca31f05dbd5c644297c2f7${strCompressExt}, " .
                ($bRemote ? "000000010000000100000002-72b9da071c13957fb4ca31f05dbd5c644297c2f7${strCompressExt}, " : '') .
                "000000010000000100000005-72b9da071c13957fb4ca31f05dbd5c644297c2f7${strCompressExt})",
            'segment 5 (and 2 if remote) is pushed', 5);
    }
    }
    }
}

1;
