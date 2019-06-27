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

use pgBackRest::Archive::Info;
use pgBackRest::Backup::Info;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;

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
        {vm => VM1, remote => false, s3 => false, encrypt => false, compress =>  true, error => 0},
        {vm => VM1, remote =>  true, s3 =>  true, encrypt =>  true, compress => false, error => 1},
        {vm => VM2, remote => false, s3 =>  true, encrypt => false, compress => false, error => 0},
        {vm => VM2, remote =>  true, s3 => false, encrypt =>  true, compress =>  true, error => 0},
        {vm => VM3, remote => false, s3 => false, encrypt =>  true, compress => false, error => 0},
        {vm => VM3, remote =>  true, s3 =>  true, encrypt => false, compress =>  true, error => 1},
        {vm => VM4, remote => false, s3 =>  true, encrypt =>  true, compress =>  true, error => 0},
        {vm => VM4, remote =>  true, s3 => false, encrypt => false, compress => false, error => 0},
    )
    {
        # Only run tests for this vm
        next if ($rhRun->{vm} ne $self->vm());

        # Increment the run, log, and decide whether this unit test should be run
        my $bRemote = $rhRun->{remote};
        my $bS3 = $rhRun->{s3};
        my $bEncrypt = $rhRun->{encrypt};
        my $bCompress = $rhRun->{compress};
        my $iError = $rhRun->{error};

        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("rmt ${bRemote}, cmp ${bCompress}, error " . ($iError ? 'connect' : 'version') . ", s3 ${bS3}, " .
            "enc ${bEncrypt}")) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, bCompress => $bCompress, bArchiveAsync => true, bS3 => $bS3,
            bRepoEncrypt => $bEncrypt});

        # Create compression extension
        my $strCompressExt = $bCompress ? qw{.} . COMPRESS_EXT : '';

        # Create the wal path
        my $strWalPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        storageTest()->pathCreate($strWalPath, {bCreateParent => true});

        # Create the test path for pg_control and generate pg_control for stanza-create
        storageTest()->pathCreate($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_94);

        # Create the archive info file
        $oHostBackup->stanzaCreate('create required data for stanza', {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Push a WAL segment
        &log(INFO, '    push first WAL');
        $oHostDbMaster->archivePush($strWalPath, $strWalTestFile, 1);

        # Break the database version of the archive info file
        if ($iError == 0)
        {
            $oHostBackup->infoMunge(
                storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'},
                 &INFO_ARCHIVE_SECTION_DB_HISTORY => {1 => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}}});
        }

        # Push two more segments with errors to exceed archive-push-queue-max
        &log(INFO, '    push second WAL');

        $oHostDbMaster->archivePush(
            $strWalPath, $strWalTestFile, 2, $iError ? ERROR_UNKNOWN : ERROR_ARCHIVE_MISMATCH);

        &log(INFO, '    push third WAL');

        $oHostDbMaster->archivePush(
            $strWalPath, $strWalTestFile, 3, $iError ? ERROR_UNKNOWN : ERROR_ARCHIVE_MISMATCH);

        # Now this segment will get dropped
        &log(INFO, '    push fourth WAL');

        $oHostDbMaster->archivePush($strWalPath, $strWalTestFile, 4, undef, undef, '--repo1-host=bogus');

        # Fix the database version
        if ($iError == 0)
        {
            $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE));
        }

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageRepo()->list(
                STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . '-1/0000000100000001')},
            "000000010000000100000001-${strWalHash}${strCompressExt}",
            'segment 2-4 not pushed', {iWaitSeconds => 5});

        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->archivePush($strWalPath, $strWalTestFile, 5);

        $self->testResult(
            sub {storageRepo()->list(
                STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . '-1/0000000100000001')},
            "(000000010000000100000001-${strWalHash}${strCompressExt}, " .
                "000000010000000100000005-${strWalHash}${strCompressExt})",
            'segment 5 is pushed', {iWaitSeconds => 5});
    }
}

1;
