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
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_93 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);

        # Create the xlog path for pushing WAL
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        filePathCreate($strXlogPath, undef, false, true);
        my $strArchiveTestFile = $self->dataPath() . '/backup.wal1_';

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

        # Fail on archive push due to mismatch of DB since stanza not upgraded
        #--------------------------------------------------------------------------------------------------------------------------
        # Push a WAL segment so have a valid file in archive dir
        # $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile . WAL_VERSION_93 . '.bin', 1);

        # Upgrade the DB by copying new pg_control
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);
        executeTest('sudo chmod 600 ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);

        # Fail on attempt to push an archive
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile . WAL_VERSION_94 . '.bin', 1, ERROR_ARCHIVE_MISMATCH);

        # Perform a successful stanza upgrade noting additional history lines in info files for new version of the database
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('successful upgrade creates mismatched files', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # After stanza upgrade, make sure archives are pushed to the new db verion-id directory (9.4-2)
        #--------------------------------------------------------------------------------------------------------------------------
        # Push a WAL segment so have a valid file in the latest DB archive dir only
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile . WAL_VERSION_94 . '.bin', 1);
        $self->testResult(
            sub {$oFile->list(PATH_BACKUP_ARCHIVE, PG_VERSION_94 . '-2/0000000100000001')},
            "000000010000000100000001-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27.$oFile->{strCompressExtension}",
            'check that WAL is in the archive at -2');

        # Create a DB history mismatch between the info files
        #--------------------------------------------------------------------------------------------------------------------------
        # Remove the archive info file and force reconstruction
        $oHostBackup->executeSimple('rm ' . $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE));
        $oHostBackup->stanzaCreate('use force to recreate the stanza producing mismatched info history but same current db-id',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Create a DB-ID mismatch between the info files
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->executeSimple('rm ' . $oFile->pathGet(PATH_BACKUP_CLUSTER, FILE_BACKUP_INFO));
        $oHostBackup->stanzaCreate('use force to recreate the stanza producing mismatched db-id',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Confirm successful backup at db-1 although archive at db-2
        #--------------------------------------------------------------------------------------------------------------------------
        # Create the tablespace directory and perform a backup
        filePathCreate($oHostDbMaster->dbBasePath() . '/' . DB_PATH_PGTBLSPC);
        $oHostBackup->backup('full', 'create first full backup ', {strOptionalParam => '--retention-full=2 --no-' .
            OPTION_ONLINE . ' --log-level-console=detail'}, false);

        # Test archive dir version XX.Y-Z ensuring sort order of db ids is reconstructed correctly from the directory db-id value
        #--------------------------------------------------------------------------------------------------------------------------
        # Create the 10.0-3 directory and copy a WAL file to it (something that has a different system id)
        $oHostBackup->executeSimple('mkdir ' . $oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/' . '10.0-3');
        $oHostBackup->executeSimple('mkdir ' . $oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/' . '10.0-3/0000000100000001');
        $oHostBackup->executeSimple('cp ' . $self->dataPath() . '/backup.wal1_' . WAL_VERSION_92 . '.bin '
            . $oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/' . '10.0-3/0000000100000001/000000010000000100000001');

        # Copy pg_control for 9.5
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_95 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);
        executeTest('sudo chmod 600 ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);

        $oHostBackup->stanzaUpgrade('successfully upgrade with XX.Y-Z', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Push a WAL and create a backup in the new DB to confirm diff changed to full and info command displays the JSON correctly
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile . WAL_VERSION_95 . '.bin', 1);

        # Test backup is changed from type=DIFF to FULL (WARN message displayed)
        my $oExecuteBackup = $oHostBackup->backupBegin('diff', 'diff changed to full backup',
            {strOptionalParam => '--retention-full=2 --no-' . OPTION_ONLINE . ' --log-level-console=detail'});
        $oHostBackup->backupEnd('full', $oExecuteBackup, undef, false);

        # Confirm info command displays the JSON correctly
        $oHostDbMaster->info('db upgraded - db-1 and db-2 listed', {strOutput => INFO_OUTPUT_JSON});
    }
}

1;
