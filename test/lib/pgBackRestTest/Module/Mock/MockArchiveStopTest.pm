####################################################################################################################################
# Tests for archive-push command to make sure aync queue limits are implemented correctly
####################################################################################################################################
package pgBackRestTest::Module::Mock::MockArchiveStopTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;

use pgBackRestTest::Env::ArchiveInfo;
use pgBackRestTest::Env::BackupInfo;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Manifest;
use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Generate test WAL file
    my $strWalTestFile = $self->testPath() . '/test-wal-' . PG_VERSION_94;
    my $strWalHash = $self->walGenerateContentChecksum(PG_VERSION_94);
    storageTest()->put($strWalTestFile, $self->walGenerateContent(PG_VERSION_94));

    foreach my $rhRun
    (
        {vm => VM2, remote => false, tls => false, storage =>    S3, encrypt => false, compress =>  NONE, error => 0},
        {vm => VM2, remote =>  true, tls => false, storage => POSIX, encrypt =>  true, compress =>  NONE, error => 0},
        {vm => VM3, remote => false, tls =>  true, storage =>   GCS, encrypt =>  true, compress =>   BZ2, error => 0},
        {vm => VM3, remote =>  true, tls => false, storage => AZURE, encrypt => false, compress =>   LZ4, error => 1},
        {vm => VM4, remote => false, tls => false, storage =>    S3, encrypt =>  true, compress =>  NONE, error => 0},
        {vm => VM4, remote =>  true, tls =>  true, storage =>   GCS, encrypt => false, compress =>   ZST, error => 0},
    )
    {
        # Only run tests for this vm
        next if ($rhRun->{vm} ne vmTest($self->vm()));

        # Increment the run, log, and decide whether this unit test should be run
        my $bRemote = $rhRun->{remote};
        my $bTls = $rhRun->{tls};
        my $strStorage = $rhRun->{storage};
        my $bEncrypt = $rhRun->{encrypt};
        my $strCompressType = $rhRun->{compress};
        my $iError = $rhRun->{error};

        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin(
                "rmt ${bRemote}, tls ${bTls}, cmp ${strCompressType}, error " . ($iError ? 'connect' : 'version') .
                    ", storage ${strStorage}, enc ${bEncrypt}")) {next}

        # Create hosts, file object, and config
        my ($oHostDbPrimary, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(),
            {bHostBackup => $bRemote, bTls => $bTls, strCompressType => $strCompressType, bArchiveAsync => true,
                strStorage => $strStorage, bRepoEncrypt => $bEncrypt});

        # Create compression extension
        my $strCompressExt = $strCompressType ne NONE ? ".${strCompressType}" : '';

        # Create the wal path
        my $strWalPath = $oHostDbPrimary->dbBasePath() . '/pg_xlog';
        storageTest()->pathCreate($strWalPath, {bCreateParent => true});

        # Create the test path for pg_control and generate pg_control for stanza-create
        storageTest()->pathCreate($oHostDbPrimary->dbBasePath() . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_94);

        # Create the archive info file
        $oHostBackup->stanzaCreate('create required data for stanza', {strOptionalParam => '--no-online'});

        # Push a WAL segment
        &log(INFO, '    push first WAL');
        $oHostDbPrimary->archivePush($strWalPath, $strWalTestFile, 1);

        # Break the database version of the archive info file
        if ($iError == 0)
        {
            $oHostBackup->infoMunge(
                $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'},
                 &INFO_ARCHIVE_SECTION_DB_HISTORY => {1 => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}}});
        }

        # Push two more segments with errors to exceed archive-push-queue-max
        &log(INFO, '    push second WAL');

        $oHostDbPrimary->archivePush(
            $strWalPath, $strWalTestFile, 2, ERROR_REPO_INVALID, undef, $iError ? '--repo1-host=bogus' : undef);

        &log(INFO, '    push third WAL');

        $oHostDbPrimary->archivePush(
            $strWalPath, $strWalTestFile, 3, ERROR_REPO_INVALID, undef, $iError ? '--repo1-host=bogus' : undef);

        # Now this segment will get dropped
        &log(INFO, '    push fourth WAL');

        $oHostDbPrimary->archivePush($strWalPath, $strWalTestFile, 4, undef, undef, '--repo1-host=bogus');

        # Fix the database version
        if ($iError == 0)
        {
            $oHostBackup->infoRestore($oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE));
        }

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageRepo()->list($oHostBackup->repoArchivePath(PG_VERSION_94 . '-1/0000000100000001'))},
            "000000010000000100000001-${strWalHash}${strCompressExt}",
            'segment 2-4 not pushed', {iWaitSeconds => 5});

        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbPrimary->archivePush($strWalPath, $strWalTestFile, 5);

        $self->testResult(
            sub {storageRepo()->list($oHostBackup->repoArchivePath(PG_VERSION_94 . '-1/0000000100000001'))},
            "(000000010000000100000001-${strWalHash}${strCompressExt}, " .
                "000000010000000100000005-${strWalHash}${strCompressExt})",
            'segment 5 is pushed', {iWaitSeconds => 5});
    }
}

1;
