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

use pgBackRest::Archive::Info;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::Gzip;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Archive and backup info file names
    my $strArchiveInfoFile = STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE;
    my $strArchiveInfoCopyFile = STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE . INI_COPY_EXT;
    my $strArchiveInfoOldFile = "${strArchiveInfoFile}.old";
    my $strArchiveInfoCopyOldFile = "${strArchiveInfoCopyFile}.old";

    my $strBackupInfoFile = STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO;
    my $strBackupInfoCopyFile = STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO . INI_COPY_EXT;
    my $strBackupInfoOldFile = "${strBackupInfoFile}.old";
    my $strBackupInfoCopyOldFile = "${strBackupInfoCopyFile}.old";

    foreach my $bS3 (false, true)
    {
    foreach my $bRemote ($bS3 ? (false) : (false, true))
    {
        my $bRepoEncrypt = $bRemote && !$bS3 ? true : false;

        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("remote $bRemote, s3 $bS3, enc ${bRepoEncrypt}")) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, bS3 => $bS3, bRepoEncrypt => $bRepoEncrypt});

        # Create the stanza
        $oHostBackup->stanzaCreate('fail on missing control file', {iExpectedExitStatus => ERROR_FILE_OPEN,
            strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Generate pg_control for stanza-create
        storageDb()->pathCreate(($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL), {bCreateParent => true});
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_93);

        # Fail stanza upgrade before stanza-create has been performed
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('fail on stanza not initialized since archive.info is missing',
            {iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Create the stanza successfully without force
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate('successfully create the stanza', {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Rerun stanza-create and confirm it does not fail
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate(
            'do not fail on rerun of stanza-create - info files exist and DB section ok',
            {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Stanza Create fails when not using force - database mismatch with pg_control file
        #--------------------------------------------------------------------------------------------------------------------------
        # Change the database version by copying a new pg_control file
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_94);

        $oHostBackup->stanzaCreate('fail on database mismatch without force option',
            {iExpectedExitStatus => ERROR_FILE_INVALID, strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Restore pg_control
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_93);

        # Perform a stanza upgrade which will indicate already up to date
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('already up to date', {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Create the wal path
        my $strWalPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        storageDb()->pathCreate("${strWalPath}/archive_status", {bCreateParent => true});

        # Stanza Create fails - missing archive.info from non-empty archive dir
        #--------------------------------------------------------------------------------------------------------------------------
        # Generate WAL then push to get valid archive data in the archive directory
        my $strArchiveFile = $self->walSegment(1, 1, 1);
        my $strSourceFile = $self->walGenerate($strWalPath, PG_VERSION_93, 1, $strArchiveFile);

        my $strCommand = $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() .
            ' --stanza=db archive-push';
        $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $self->expect()});

        # With data existing in the archive dir, remove the info files and confirm failure
        if ($bRepoEncrypt)
        {
            forceStorageMove(storageRepo(), $strArchiveInfoFile, $strArchiveInfoOldFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strArchiveInfoCopyFile, $strArchiveInfoCopyOldFile, {bRecurse => false});
        }
        else
        {
            forceStorageRemove(storageRepo(), $strArchiveInfoFile);
            forceStorageRemove(storageRepo(), $strArchiveInfoCopyFile);
        }

        if (!$bRepoEncrypt)
        {
            $oHostBackup->stanzaCreate('fail on archive info file missing from non-empty dir',
                {iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});
        }

        # Stanza Create fails using force - failure to unzip compressed file
        #--------------------------------------------------------------------------------------------------------------------------
        # S3 doesn't support filesystem-style permissions so skip these tests
        if (!$bS3)
        {
            # Change the permissions of the archive file so it cannot be read
            forceStorageMode(
                storageRepo(), STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_93 . '-1/' . substr($strArchiveFile, 0, 16) . '/*.' .
                    COMPRESS_EXT,
                '220');

            # Force creation of the info file but fail on gunzip
            $oHostBackup->stanzaCreate('gunzip fail on forced stanza-create',
                {iExpectedExitStatus => ERROR_FILE_OPEN, strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' .
                cfgOptionName(CFGOPT_FORCE)});

            # Change permissions back
            forceStorageMode(
                storageRepo(), STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_93 . '-1/' . substr($strArchiveFile, 0, 16) . '/*.' .
                    COMPRESS_EXT,
                '640');
        }

        # Stanza Create succeeds when using force - recreates archive.info from compressed archive file
        #--------------------------------------------------------------------------------------------------------------------------
        # Force creation of archive info from the gz file
        $oHostBackup->stanzaCreate('force create archive.info from gz file',
            {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE),
                iExpectedExitStatus => $bRepoEncrypt ? ERROR_FILE_MISSING : undef});

        if (!$bRepoEncrypt)
        {
            $self->testResult(sub {storageRepo()->exists($strArchiveInfoFile)}, true, "    archive.info file was created");
        }

        # Stanza Create succeeds when using force - recreates archive.info from uncompressed archive file
        #--------------------------------------------------------------------------------------------------------------------------
        # Unzip the archive file and recreate the archive.info file from it
        my $strArchiveTest = PG_VERSION_93 . "-1/${strArchiveFile}-" . $self->walGenerateContentChecksum(PG_VERSION_93);

        if (!$bRepoEncrypt)
        {
            forceStorageMode(
                storageRepo(), dirname(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/${strArchiveTest}.gz")), 'g+w',
                {bRecursive => true});

            storageRepo()->copy(
                storageRepo()->openRead(
                    STORAGE_REPO_ARCHIVE . "/${strArchiveTest}.gz",
                    {rhyFilter => [{strClass => STORAGE_FILTER_GZIP, rxyParam => [{strCompressType => STORAGE_DECOMPRESS}]}]}),
                STORAGE_REPO_ARCHIVE . "/${strArchiveTest}");

            $oHostBackup->stanzaCreate('force create archive.info from uncompressed file',
                {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});
        }

        # Stanza Create succeeds when using force - missing archive file
        #--------------------------------------------------------------------------------------------------------------------------
        # Remove the uncompressed WAL archive file and archive.info
        if (!$bRepoEncrypt)
        {
            forceStorageRemove(storageRepo(), STORAGE_REPO_ARCHIVE . "/${strArchiveTest}");
            forceStorageRemove(storageRepo(), $strArchiveInfoFile);
            forceStorageRemove(storageRepo(), $strArchiveInfoCopyFile);

            $oHostBackup->stanzaCreate('force with missing WAL archive file',
                {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});
        }

        # Stanza Create succeeds when using force - missing archive directory
        #--------------------------------------------------------------------------------------------------------------------------
        # Remove the WAL archive directory
        if (!$bRepoEncrypt)
        {
            forceStorageRemove(
                storageRepo(),
                STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_93 . '-1/' . substr($strArchiveFile, 0, 16), {bRecurse => true});
            forceStorageRemove(storageRepo(), $strArchiveInfoFile);
            forceStorageRemove(storageRepo(), $strArchiveInfoCopyFile);

            $oHostBackup->stanzaCreate('force with missing WAL archive directory',
                {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});
        }

        # Encrypted info files could not be reconstructed above so just copy them back
        if ($bRepoEncrypt)
        {
            forceStorageMove(storageRepo(), $strArchiveInfoOldFile, $strArchiveInfoFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strArchiveInfoCopyOldFile, $strArchiveInfoCopyFile, {bRecurse => false});
        }

        # Just before upgrading push one last WAL on the old version to ensure it can be retrieved later
        #--------------------------------------------------------------------------------------------------------------------------
        $strArchiveFile = $self->walSegment(1, 1, 2);
        $strSourceFile = $self->walGenerate($strWalPath, PG_VERSION_93, 1, $strArchiveFile);
        $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $self->expect()});

        # Fail on archive push due to mismatch of DB since stanza not upgraded
        #--------------------------------------------------------------------------------------------------------------------------
        my $strArchiveTestFile = $self->testPath() . '/test-wal';
        storageTest()->put($strArchiveTestFile, $self->walGenerateContent(PG_VERSION_94));

        # Upgrade the DB by copying new pg_control
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_94);
        forceStorageMode(storageDb(), $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL, '600');

        # Fail on attempt to push an archive
        $oHostDbMaster->archivePush($strWalPath, $strArchiveTestFile, 1, ERROR_ARCHIVE_MISMATCH);

        # Perform a successful stanza upgrade noting additional history lines in info files for new version of the database
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaUpgrade('successful upgrade creates additional history', {strOptionalParam => '--no-' .
            cfgOptionName(CFGOPT_ONLINE)});

        # Make sure that WAL from the old version can still be retrieved
        #--------------------------------------------------------------------------------------------------------------------------
        # Generate the old pg_control so it looks like the original db has been restored
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_93);

        # Attempt to get the last archive log that was pushed to this repo
        $oHostDbMaster->executeSimple(
            $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() .
                " --stanza=db archive-get ${strArchiveFile} " . $oHostDbMaster->dbBasePath() . '/pg_xlog/RECOVERYXLOG',
            {oLogTest => $self->expect()});

        # Copy the new pg_control back so the tests can continue with the upgraded stanza
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_94);
        forceStorageMode(storageDb(), $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL, '600');

        # After stanza upgrade, make sure archives are pushed to the new db verion-id directory (9.4-2)
        #--------------------------------------------------------------------------------------------------------------------------
        # Push a WAL segment so have a valid file in the latest DB archive dir only
        $oHostDbMaster->archivePush($strWalPath, $strArchiveTestFile, 1);
        $self->testResult(
            sub {storageRepo()->list(STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . '-2/0000000100000001')},
            '000000010000000100000001-' . $self->walGenerateContentChecksum(PG_VERSION_94) . '.' . COMPRESS_EXT,
            'check that WAL is in the archive at -2');

        # Create a DB history mismatch between the info files
        #--------------------------------------------------------------------------------------------------------------------------
        # Remove the archive info file and force reconstruction
        if (!$bRepoEncrypt)
        {
            forceStorageRemove(storageRepo(), $strArchiveInfoFile);
            forceStorageRemove(storageRepo(), $strArchiveInfoCopyFile);

            $oHostBackup->stanzaCreate('use force to recreate the stanza producing mismatched info history but same current db-id',
                {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});
        }

        # Create a DB-ID mismatch between the info files
        #--------------------------------------------------------------------------------------------------------------------------
        if (!$bRepoEncrypt)
        {
            forceStorageRemove(storageRepo(), $strBackupInfoFile);
            forceStorageRemove(storageRepo(), $strBackupInfoCopyFile);

            $oHostBackup->stanzaCreate('use force to recreate the stanza producing mismatched db-id',
                {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});
        }

        # Confirm successful backup at db-1 although archive at db-2
        #--------------------------------------------------------------------------------------------------------------------------
        # Create the tablespace directory and perform a backup
        storageTest()->pathCreate($oHostDbMaster->dbBasePath() . '/' . DB_PATH_PGTBLSPC);
        $oHostBackup->backup(
            'full', 'create first full backup ',
            {strOptionalParam => '--repo1-retention-full=2 --no-' . cfgOptionName(CFGOPT_ONLINE) . ' --log-level-console=detail'},
            false);

        # Stanza Create fails when not using force - no backup.info but backup exists
        #--------------------------------------------------------------------------------------------------------------------------
        if ($bRepoEncrypt)
        {
            forceStorageMove(storageRepo(), $strBackupInfoFile, $strBackupInfoOldFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strBackupInfoCopyFile, $strBackupInfoCopyOldFile, {bRecurse => false});
        }
        else
        {
            forceStorageRemove(storageRepo(), $strBackupInfoFile);
            forceStorageRemove(storageRepo(), $strBackupInfoCopyFile);
        }

        $oHostBackup->stanzaCreate('fail no force to recreate the stanza from backups',
            {iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Stanza Create succeeds using force - reconstruct backup.info from backup
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate('use force to recreate the stanza from backups',
            {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE),
                iExpectedExitStatus => $bRepoEncrypt ? ERROR_FILE_MISSING : undef});

        # Encrypted info files could not be reconstructed above so just copy them back
        if ($bRepoEncrypt)
        {
            forceStorageMove(storageRepo(), $strBackupInfoOldFile, $strBackupInfoFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strBackupInfoCopyOldFile, $strBackupInfoCopyFile, {bRecurse => false});
        }

        # Test archive dir version XX.Y-Z ensuring sort order of db ids is reconstructed correctly from the directory db-id value
        #--------------------------------------------------------------------------------------------------------------------------
        # Create the 10.0-3 directory and copy a WAL file to it (something that has a different system id)
        forceStorageMode(storageRepo(), STORAGE_REPO_ARCHIVE, '770');
        storageRepo()->pathCreate(STORAGE_REPO_ARCHIVE . '/10.0-3/0000000100000001', {bCreateParent => true});
        storageRepo()->put(
            storageRepo()->openWrite(
                STORAGE_REPO_ARCHIVE . '/10.0-3/0000000100000001/000000010000000100000001',
                {strCipherPass => $oHostBackup->cipherPassArchive()}),
            $self->walGenerateContent(PG_VERSION_94));
        forceStorageOwner(storageRepo(), STORAGE_REPO_ARCHIVE . '/10.0-3', $oHostBackup->userGet(), {bRecurse => true});

        # Copy pg_control for 9.5
        $self->controlGenerate($oHostDbMaster->dbBasePath(), PG_VERSION_95);
        forceStorageMode(storageDb(), $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL, '600');

        $oHostBackup->stanzaUpgrade(
            'successfully upgrade with XX.Y-Z', {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

        # Push a WAL and create a backup in the new DB to confirm diff changed to full and info command displays the JSON correctly
        #--------------------------------------------------------------------------------------------------------------------------
        storageTest()->put($strArchiveTestFile, $self->walGenerateContent(PG_VERSION_95));
        $oHostDbMaster->archivePush($strWalPath, $strArchiveTestFile, 1);

        # Test backup is changed from type=DIFF to FULL (WARN message displayed)
        my $oExecuteBackup = $oHostBackup->backupBegin('diff', 'diff changed to full backup',
            {strOptionalParam => '--repo1-retention-full=2 --no-' . cfgOptionName(CFGOPT_ONLINE) . ' --log-level-console=detail'});
        $oHostBackup->backupEnd('full', $oExecuteBackup, undef, false);

        # Confirm info command displays the JSON correctly
        $oHostDbMaster->info('db upgraded - db-1 and db-2 listed', {strOutput => CFGOPTVAL_INFO_OUTPUT_JSON});

        # Delete the stanza
        #--------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaDelete('fail on missing stop file', {iExpectedExitStatus => ERROR_FILE_MISSING});

        $oHostBackup->stop({strStanza => $self->stanza()});
        $oHostBackup->stanzaDelete('successfully delete the stanza');
    }
    }
}

1;
