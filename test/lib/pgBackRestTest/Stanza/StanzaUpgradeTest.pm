####################################################################################################################################
# StanzaUpgradeTest.pm - Tests for stanza-upgrade command
#
# db-catalog-version=201510051
# db-control-version=942
# db-system-id=6392579261579036436
# db-version="9.5"
####################################################################################################################################
package pgBackRestTest::Stanza::StanzaUpgradeTest;
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
use pgBackRest::InfoCommon;
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

    for (my $bRemote = false; $bRemote <= true; $bRemote++)
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin($bRemote ? "remote" : "local")) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oFile) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote});

        # Create the test path for pg_control
        filePathCreate(($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL), undef, false, true);

        # Copy pg_control for stanza-create
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);

        # Attempt an upgrade before stanza-create has been performed
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('fail on stanza not initialized since archive.info is missing',
            {iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-' . OPTION_ONLINE});

        # Create the stanza successfully without force
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate('successfully create the stanza', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Perform a stanza upgrade which will indicate already up to date
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('already up to date', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Fail upgrade when backup.info missing
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->executeSimple('rm ' . $oFile->pathGet(PATH_BACKUP_CLUSTER, FILE_BACKUP_INFO));
        $oHostBackup->stanzaUpgrade('fail on stanza not initialized since backup.info is missing',
            {iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-' . OPTION_ONLINE});

        # Create the stanza successfully with force
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate('use force to recreate the stanza',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Perform a successful stanza upgrade noting additional history line in archive.info for new version of the database
        #--------------------------------------------------------------------------------------------------------------------------
        # Modify archive.info to an older database version causing a mismatch between the archive and backup info histories
        $oHostBackup->infoMunge($oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE),
            {&INFO_ARCHIVE_SECTION_DB =>
                {&INFO_ARCHIVE_KEY_DB_VERSION => '9.3', &INFO_ARCHIVE_KEY_DB_SYSTEM_ID => 6999999999999999999},
             &INFO_ARCHIVE_SECTION_DB_HISTORY =>
                {'1' =>
                    {&INFO_ARCHIVE_KEY_DB_VERSION => '9.3', &INFO_ARCHIVE_KEY_DB_ID => 6999999999999999999}}});

        $oHostBackup->stanzaUpgrade('successfully upgrade mismatched files', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # After stanza upgrade, make sure archives are pushed to the new db verion-id directory (9.4-2)
        #--------------------------------------------------------------------------------------------------------------------------
        # Create the xlog path and copy a WAL file
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        filePathCreate($strXlogPath, undef, false, true);
        my $strArchiveTestFile = $self->dataPath() . '/backup.wal2_' . WAL_VERSION_94 . '.bin';

        # Push a WAL segment
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 1);

        $self->testResult(
            sub {$oFile->list(PATH_BACKUP_ARCHIVE, PG_VERSION_94 . '-2/0000000100000001')},
            "000000010000000100000001-72b9da071c13957fb4ca31f05dbd5c644297c2f7.$oFile->{strCompressExtension}",
            'check that WAL is in the archive at -2');

        # Change the pushed WAL segment to done
        fileMove("${strXlogPath}/archive_status/000000010000000100000001.ready",
            "${strXlogPath}/archive_status/000000010000000100000001.done");

        # Confirm successful backup at db-1 although archive at db-2
        #--------------------------------------------------------------------------------------------------------------------------
        # Create the tablespace directory and perform a backup
        filePathCreate($oHostDbMaster->dbBasePath() . '/' . DB_PATH_PGTBLSPC);
        
        $oHostBackup->backup('full', 'create full backup ', {strOptionalParam => '--retention-full=1 --no-' .
            OPTION_ONLINE . ' --log-level-console=detail'}, false);
    }
}

1;
