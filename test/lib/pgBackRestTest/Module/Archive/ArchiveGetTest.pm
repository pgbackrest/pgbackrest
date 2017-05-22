####################################################################################################################################
# ArchiveGetTest.pm - Tests for archive-get command
####################################################################################################################################
package pgBackRestTest::Module::Archive::ArchiveGetTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Backup::Info;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Filter::Gzip;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $strArchiveChecksum = '72b9da071c13957fb4ca31f05dbd5c644297c2f7';
    my $iArchiveMax = 3;
    my $strArchiveTestFile = $self->dataPath() . '/backup.wal2_' . WAL_VERSION_94 . '.bin';

    for (my $bRemote = false; $bRemote <= true; $bRemote++)
    {
    for (my $bCompress = false; $bCompress <= true; $bCompress++)
    {
    for (my $bExists = false; $bExists <= true; $bExists++)
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("rmt ${bRemote}, cmp ${bCompress}, exists ${bExists}")) {next}

        # Create hosts and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, bCompress => $bCompress});

        # Get storage repo
        my $oStorageRepo = storageRepo();
        my $oStorageDb = storageDb();

        # Create the xlog path
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        $oStorageDb->pathCreate($strXlogPath, {bCreateParent => true});

        # Create the test path for pg_control and copy pg_control for stanza-create
        $oStorageDb->pathCreate($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $oHostDbMaster->dbBasePath() .  '/' .
            DB_FILE_PGCONTROL);

        my $strCommand =
            $oHostDbMaster->backrestExe() .
            ' --config=' . $oHostDbMaster->backrestConfig() .
            ' --stanza=' . $self->stanza() . ' archive-get';

        # Fail on missing archive info file
        $oHostDbMaster->executeSimple(
                $strCommand . " 000000010000000100000001 ${strXlogPath}/000000010000000100000001",
                {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});

        # Create the archive info file
	    $oHostBackup->stanzaCreate('create required data for stanza', {strOptionalParam => '--no-' . OPTION_ONLINE});

        if (defined($self->expect()))
        {
            $self->expect()->supplementalAdd($oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE) . '/archive.info');
        }

        if ($bExists)
        {
            # Loop through archive files
            my $strArchiveFile;

            for (my $iArchiveNo = 1; $iArchiveNo <= $iArchiveMax; $iArchiveNo++)
            {
                # Construct the archive filename
                if ($iArchiveNo > 255)
                {
                    confess 'backup total * archive total cannot be greater than 255';
                }

                $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo));

                &log(INFO, '    archive ' .sprintf('%02x', $iArchiveNo) .
                           " - ${strArchiveFile}");

                my $strSourceFile = "${strArchiveFile}-${strArchiveChecksum}";

                if ($bCompress)
                {
                    $strSourceFile .= '.gz';
                }

                # Change the directory permissions to enable file creation
                executeTest('sudo chmod 770 ' . $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE));
                $oStorageRepo->pathCreate(
                    STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . '-1/' . substr($strArchiveFile, 0, 16),
                    {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

                storageTest()->copy(
                    $strArchiveTestFile,
                    $oStorageRepo->openWrite(
                        STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . "-1/${strSourceFile}",
                        {rhyFilter => $bCompress ? [{strClass => STORAGE_FILTER_GZIP}] : undef,
                            strMode => '0660', bCreateParent => true}));

                my ($strActualChecksum) = $oStorageRepo->hashSize(
                    $oStorageRepo->openRead(
                        STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . "-1/${strSourceFile}",
                        {rhyFilter => $bCompress ? [{strClass => STORAGE_FILTER_GZIP}] : undef}));

                if ($strActualChecksum ne $strArchiveChecksum)
                {
                    confess "archive file hash '${strActualChecksum}' does not match expected '${strArchiveChecksum}'";
                }

                my $strDestinationFile = "${strXlogPath}/${strArchiveFile}";

                $oHostDbMaster->executeSimple(
                    $strCommand . ($bRemote && $iArchiveNo == 1 ? ' --cmd-ssh=/usr/bin/ssh' : '') .
                        " ${strArchiveFile} ${strDestinationFile}",
                    {oLogTest => $self->expect()});

                # Check that the destination file exists
                if ($oStorageRepo->exists($strDestinationFile))
                {
                    my ($strActualChecksum) = $oStorageRepo->hashSize($strDestinationFile);

                    if ($strActualChecksum ne $strArchiveChecksum)
                    {
                        confess "recovered file hash '${strActualChecksum}' does not match expected '${strArchiveChecksum}'";
                    }
                }
                else
                {
                    confess 'archive file is not in destination';
                }
            }
        }
        else
        {
            $oHostDbMaster->stop();

            $oHostDbMaster->executeSimple(
                $strCommand . " 000000090000000900000009 ${strXlogPath}/RECOVERYXLOG",
                {iExpectedExitStatus => ERROR_STOP, oLogTest => $self->expect()});

            $oHostDbMaster->start();

            $oHostDbMaster->executeSimple(
                $strCommand . " 000000090000000900000009 ${strXlogPath}/RECOVERYXLOG",
                {iExpectedExitStatus => 1, oLogTest => $self->expect()});
        }
    }
    }
    }
}

1;
