####################################################################################################################################
# StanzaCreateTest.pm - Tests for stanza-create command
####################################################################################################################################
package pgBackRestTest::Module::Stanza::StanzaCreateTest;
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
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Env::HostEnvTest;
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
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote});

        # Storage
        my $oStorageDb = storageDb();
        my $oStorageRepo = storageRepo();

        # Create the stanza
        $oHostBackup->stanzaCreate('fail on missing control file', {iExpectedExitStatus => ERROR_FILE_OPEN,
            strOptionalParam => '--no-' . OPTION_ONLINE});

        # Create the test path for pg_control
        $oStorageDb->pathCreate(($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL), {bCreateParent => true});

        # Copy pg_control for stanza-create
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);

        $oHostBackup->stanzaCreate('successfully create the stanza', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Rerun stanza-create and confirm success without the need to use force on empty directories
        $oHostBackup->stanzaCreate('successful rerun of stanza-create', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Create the xlog path
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        $oStorageDb->pathCreate($strXlogPath, {bCreateParent => true});

        # Generate WAL then push to get valid archive data in the archive directory
        my ($strArchiveFile, $strSourceFile) = $self->archiveGenerate($strXlogPath, 1, 1, WAL_VERSION_94);
        my $strCommand = $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() .
            ' --stanza=db archive-push';
        $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $self->expect()});

        # With data existing in the archive dir, remove the info file and confirm failure
        $oHostBackup->executeSimple('rm ' . $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE . qw{*}));
        $oHostBackup->stanzaCreate('fail on archive info file missing from non-empty dir',
            {iExpectedExitStatus => ERROR_PATH_NOT_EMPTY, strOptionalParam => '--no-' . OPTION_ONLINE});

        # Change the permissions of the archive file so it cannot be read
        executeTest('sudo chmod 220 ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
            substr($strArchiveFile, 0, 16) . "/*.gz");

        # Force creation of the info file but fail on gunzip
        $oHostBackup->stanzaCreate('gunzip fail on forced stanza-create',
            {iExpectedExitStatus => ERROR_FILE_OPEN, strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Change permissions back and force creation of archive info from the gz file
        executeTest('sudo chmod 640 ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
            substr($strArchiveFile, 0, 16) . "/*.gz");

        $oHostBackup->stanzaCreate('force create archive.info from gz file',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Rerun without the force to ensure the format is still valid - this will hash check the info files and indicate the
        # stanza already exists
        $oHostBackup->stanzaCreate('repeat create', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Remove the backup info file and confirm success with backup dir empty
        # Backup Full tests will confirm failure when backup dir not empty
        $oHostBackup->executeSimple('rm ' . $oStorageRepo->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO . qw{*}));
        $oHostBackup->stanzaCreate('force not needed when backup dir empty, archive.info exists but backup.info is missing',
            {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Remove the backup.info file then munge and save the archive info file
        $oHostBackup->executeSimple('rm ' . $oStorageRepo->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO . qw{*}));
        $oHostBackup->infoMunge(
            $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
            {&INFO_BACKUP_SECTION_DB => {&INFO_BACKUP_KEY_DB_VERSION => '8.0'}});

        $oHostBackup->stanzaCreate('hash check fails requiring force',
            {iExpectedExitStatus => ERROR_FILE_INVALID, strOptionalParam => '--no-' . OPTION_ONLINE});

        $oHostBackup->stanzaCreate('use force to overwrite the invalid file',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Cleanup the global hash but don't save the file (permission issues may prevent it anyway)
        $oHostBackup->infoRestore($oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE), false);

        # Change the database version by copying a new pg_control file
        executeTest('sudo rm ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);
        executeTest('cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_93 . '.bin ' . $oHostDbMaster->dbBasePath() .
            '/' . DB_FILE_PGCONTROL);

        $oHostBackup->stanzaCreate('fail on database mismatch without force option',
            {iExpectedExitStatus => ERROR_FILE_INVALID, strOptionalParam => '--no-' . OPTION_ONLINE});

        # Restore pg_control
        executeTest('sudo rm ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);
        executeTest('cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' .
            $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);

        # Unzip the archive file and recreate the archive.info file from it
        executeTest('sudo gunzip ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
            substr($strArchiveFile, 0, 16) . "/${strArchiveFile}-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27.gz");
        $oHostBackup->stanzaCreate('force create archive.info from uncompressed file',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Remove the uncompressed WAL archive file and archive.info
        executeTest('sudo rm ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
            substr($strArchiveFile, 0, 16) . "/${strArchiveFile}-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27");
        $oHostBackup->executeSimple('rm ' . $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE . qw{*}));
        $oHostBackup->stanzaCreate('force with missing WAL archive file',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        # Remove the WAL archive directory
        executeTest('sudo rm -rf ' . $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
            substr($strArchiveFile, 0, 16));
        $oHostBackup->executeSimple('rm ' . $oStorageRepo->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE));
        $oHostBackup->stanzaCreate('force with missing WAL archive directory',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});
    }
}

1;
