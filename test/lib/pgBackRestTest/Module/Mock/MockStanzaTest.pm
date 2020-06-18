####################################################################################################################################
# Mock Stanza Module Tests
####################################################################################################################################
package pgBackRestTest::Module::Mock::MockStanzaTest;
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
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Env::InfoCommon;
use pgBackRestTest::Env::Manifest;
use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageBase;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    foreach my $rhRun
    (
        {vm => VM1, remote => false, storage => POSIX, encrypt =>  true, compress => LZ4},
        {vm => VM1, remote =>  true, storage =>    S3, encrypt => false, compress => BZ2},
        {vm => VM2, remote => false, storage =>    S3, encrypt =>  true, compress => BZ2},
        {vm => VM2, remote =>  true, storage => POSIX, encrypt => false, compress =>  GZ},
        {vm => VM3, remote => false, storage => POSIX, encrypt => false, compress => ZST},
        {vm => VM3, remote =>  true, storage =>    S3, encrypt =>  true, compress => LZ4},
        {vm => VM4, remote => false, storage =>    S3, encrypt => false, compress =>  GZ},
        {vm => VM4, remote =>  true, storage => POSIX, encrypt =>  true, compress => ZST},
    )
    {
        # Only run tests for this vm
        next if ($rhRun->{vm} ne vmTest($self->vm()));

        # Increment the run, log, and decide whether this unit test should be run
        my $bRemote = $rhRun->{remote};
        my $strStorage = $rhRun->{storage};
        my $bEncrypt = $rhRun->{encrypt};
        my $strCompressType = $rhRun->{compress};

        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("remote ${bRemote}, storage ${strStorage}, enc ${bEncrypt}, cmp ${strCompressType}")) {next}

        # Create hosts, file object, and config
        my ($oHostDbPrimary, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, strStorage => $strStorage, bRepoEncrypt => $bEncrypt,
            strCompressType => $strCompressType});

        # Archive and backup info file names
        my $strArchiveInfoFile = $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE);
        my $strArchiveInfoCopyFile = $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE . INI_COPY_EXT);
        my $strArchiveInfoOldFile = "${strArchiveInfoFile}.old";
        my $strArchiveInfoCopyOldFile = "${strArchiveInfoCopyFile}.old";

        my $strBackupInfoFile = $oHostBackup->repoBackupPath(FILE_BACKUP_INFO);
        my $strBackupInfoCopyFile = $oHostBackup->repoBackupPath(FILE_BACKUP_INFO . INI_COPY_EXT);
        my $strBackupInfoOldFile = "${strBackupInfoFile}.old";
        my $strBackupInfoCopyOldFile = "${strBackupInfoCopyFile}.old";

        # Create the stanza
        $oHostBackup->stanzaCreate('fail on missing control file', {iExpectedExitStatus => ERROR_FILE_MISSING,
            strOptionalParam => '--no-online --log-level-file=info'});

        # Generate pg_control for stanza-create
        storageTest()->pathCreate(($oHostDbPrimary->dbBasePath() . '/' . DB_PATH_GLOBAL), {bCreateParent => true});
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_93);

        # Fail stanza upgrade before stanza-create has been performed
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('fail on stanza not initialized since archive.info is missing',
            {iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-online'});

        # Create the stanza successfully without force
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate('successfully create the stanza', {strOptionalParam => '--no-online'});

        # Rerun stanza-create and confirm it does not fail
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate(
            'do not fail on rerun of stanza-create - info files exist and DB section ok',
            {strOptionalParam => '--no-online'});

        # Stanza Create fails when not using force - database mismatch with pg_control file
        #--------------------------------------------------------------------------------------------------------------------------
        # Change the database version by copying a new pg_control file
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_94);

        $oHostBackup->stanzaCreate('fail on database mismatch and warn force option deprecated',
            {iExpectedExitStatus => ERROR_FILE_INVALID, strOptionalParam => '--no-online --force'});

        # Restore pg_control
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_93);

        # Perform a stanza upgrade which will indicate already up to date
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('already up to date', {strOptionalParam => '--no-online'});

        # Create the wal path
        my $strWalPath = $oHostDbPrimary->dbBasePath() . '/pg_xlog';
        storageTest()->pathCreate("${strWalPath}/archive_status", {bCreateParent => true});

        # Stanza Create fails - missing archive.info from non-empty archive dir
        #--------------------------------------------------------------------------------------------------------------------------
        # Generate WAL then push to get valid archive data in the archive directory
        my $strArchiveFile = $self->walSegment(1, 1, 1);
        my $strSourceFile = $self->walGenerate($strWalPath, PG_VERSION_93, 1, $strArchiveFile);

        my $strCommand = $oHostDbPrimary->backrestExe() . ' --config=' . $oHostDbPrimary->backrestConfig() .
            ' --stanza=db archive-push';
        $oHostDbPrimary->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $self->expect()});

        # With data existing in the archive dir, move the info files and confirm failure
        forceStorageMove(storageRepo(), $strArchiveInfoFile, $strArchiveInfoOldFile, {bRecurse => false});
        forceStorageMove(storageRepo(), $strArchiveInfoCopyFile, $strArchiveInfoCopyOldFile, {bRecurse => false});

        if (!$bEncrypt)
        {
            $oHostBackup->stanzaCreate('fail on archive info file missing from non-empty dir',
                {iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-online'});
        }

        # Restore info files from copy
        forceStorageMove(storageRepo(), $strArchiveInfoOldFile, $strArchiveInfoFile, {bRecurse => false});
        forceStorageMove(storageRepo(), $strArchiveInfoCopyOldFile, $strArchiveInfoCopyFile, {bRecurse => false});

        # Just before upgrading push one last WAL on the old version to ensure it can be retrieved later
        #--------------------------------------------------------------------------------------------------------------------------
        $strArchiveFile = $self->walSegment(1, 1, 2);
        $strSourceFile = $self->walGenerate($strWalPath, PG_VERSION_93, 1, $strArchiveFile);
        $oHostDbPrimary->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $self->expect()});

        # Fail on archive push due to mismatch of DB since stanza not upgraded
        #--------------------------------------------------------------------------------------------------------------------------
        my $strArchiveTestFile = $self->testPath() . '/test-wal';
        storageTest()->put($strArchiveTestFile, $self->walGenerateContent(PG_VERSION_94));

        # Upgrade the DB by copying new pg_control
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_94);
        forceStorageMode(storageTest(), $oHostDbPrimary->dbBasePath() . '/' . DB_FILE_PGCONTROL, '600');

        # Fail on attempt to push an archive
        $oHostDbPrimary->archivePush($strWalPath, $strArchiveTestFile, 1, ERROR_ARCHIVE_MISMATCH);

        # Perform a successful stanza upgrade noting additional history lines in info files for new version of the database
        #--------------------------------------------------------------------------------------------------------------------------
        #  Save a pre-upgrade copy of archive info fo testing db-id mismatch
        forceStorageMove(storageRepo(), $strArchiveInfoCopyFile, $strArchiveInfoCopyOldFile, {bRecurse => false});

        $oHostBackup->stanzaUpgrade('successful upgrade creates additional history', {strOptionalParam => '--no-online'});

        # Make sure that WAL from the old version can still be retrieved
        #--------------------------------------------------------------------------------------------------------------------------
        # Generate the old pg_control so it looks like the original db has been restored
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_93);

        # Attempt to get the last archive log that was pushed to this repo
        $oHostDbPrimary->executeSimple(
            $oHostDbPrimary->backrestExe() . ' --config=' . $oHostDbPrimary->backrestConfig() .
                " --stanza=db archive-get ${strArchiveFile} " . $oHostDbPrimary->dbBasePath() . '/pg_xlog/RECOVERYXLOG',
            {oLogTest => $self->expect()});

        # Copy the new pg_control back so the tests can continue with the upgraded stanza
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_94);
        forceStorageMode(storageTest(), $oHostDbPrimary->dbBasePath() . '/' . DB_FILE_PGCONTROL, '600');

        # After stanza upgrade, make sure archives are pushed to the new db verion-id directory (9.4-2)
        #--------------------------------------------------------------------------------------------------------------------------
        # Push a WAL segment so have a valid file in the latest DB archive dir only
        $oHostDbPrimary->archivePush($strWalPath, $strArchiveTestFile, 1);
        $self->testResult(
            sub {storageRepo()->list($oHostBackup->repoArchivePath(PG_VERSION_94 . '-2/0000000100000001'))},
            '000000010000000100000001-' . $self->walGenerateContentChecksum(PG_VERSION_94) . ".${strCompressType}",
            'check that WAL is in the archive at -2');

        # Create the tablespace directory and perform a backup
        #--------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($oHostDbPrimary->dbBasePath() . '/' . DB_PATH_PGTBLSPC);
        $oHostBackup->backup(
            'full', 'create first full backup ', {strOptionalParam => '--repo1-retention-full=2 --no-online'}, false);

        # Upgrade the stanza
        #--------------------------------------------------------------------------------------------------------------------------
        # Copy pg_control for 9.5
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_95);
        forceStorageMode(storageTest(), $oHostDbPrimary->dbBasePath() . '/' . DB_FILE_PGCONTROL, '600');

        $oHostBackup->stanzaUpgrade('successfully upgrade', {strOptionalParam => '--no-online'});

        # Copy archive.info and restore really old version
        forceStorageMove(storageRepo(), $strArchiveInfoFile, $strArchiveInfoOldFile, {bRecurse => false});
        forceStorageRemove(storageRepo(), $strArchiveInfoCopyFile, {bRecurse => false});
        forceStorageMove(storageRepo(), $strArchiveInfoCopyOldFile, $strArchiveInfoFile, {bRecurse => false});

        #  Confirm versions
        my $oArchiveInfo = new pgBackRestTest::Env::ArchiveInfo($oHostBackup->repoArchivePath());
        my $oBackupInfo = new pgBackRestTest::Env::BackupInfo($oHostBackup->repoBackupPath());
        $self->testResult(sub {$oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
            PG_VERSION_93)}, true, 'archive at old pg version');
        $self->testResult(sub {$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef,
            PG_VERSION_95)}, true, 'backup at new pg version');

        $oHostBackup->stanzaUpgrade(
            'upgrade fails with mismatched db-ids', {iExpectedExitStatus => ERROR_FILE_INVALID, strOptionalParam => '--no-online'});

        # Restore archive.info
        forceStorageMove(storageRepo(), $strArchiveInfoOldFile, $strArchiveInfoFile, {bRecurse => false});

        # Push a WAL and create a backup in the new DB to confirm diff changed to full
        #--------------------------------------------------------------------------------------------------------------------------
        storageTest()->put($strArchiveTestFile, $self->walGenerateContent(PG_VERSION_95));
        $oHostDbPrimary->archivePush($strWalPath, $strArchiveTestFile, 1);

        # Test backup is changed from type=DIFF to FULL (WARN message displayed)
        my $oExecuteBackup = $oHostBackup->backupBegin(
            'diff', 'diff changed to full backup', {strOptionalParam => '--repo1-retention-full=2 --no-online'});
        $oHostBackup->backupEnd('full', $oExecuteBackup, undef, false);

        # Delete the stanza
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaDelete('fail on missing stop file', {iExpectedExitStatus => ERROR_FILE_MISSING});

        $oHostBackup->stop({strStanza => $self->stanza()});
        $oHostBackup->stanzaDelete('successfully delete the stanza');
    }
}

1;
