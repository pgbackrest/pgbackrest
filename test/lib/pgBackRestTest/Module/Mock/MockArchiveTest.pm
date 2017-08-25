####################################################################################################################################
# Mock Archive Tests
####################################################################################################################################
package pgBackRestTest::Module::Mock::MockArchiveTest;
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

####################################################################################################################################
# archiveCheck
#
# Check that a WAL segment is present in the repository.
####################################################################################################################################
sub archiveCheck
{
    my $self = shift;
    my $strArchiveFile = shift;
    my $strArchiveChecksum = shift;
    my $bCompress = shift;
    my $strSpoolPath = shift;

    # Build the archive name to check for at the destination
    my $strArchiveCheck = PG_VERSION_94 . "-1/${strArchiveFile}-${strArchiveChecksum}";

    if ($bCompress)
    {
        $strArchiveCheck .= '.gz';
    }

    my $oWait = waitInit(5);
    my $bFound = false;

    do
    {
        $bFound = storageRepo()->exists(STORAGE_REPO_ARCHIVE . "/${strArchiveCheck}");
    }
    while (!$bFound && waitMore($oWait));

    if (!$bFound)
    {
        confess 'unable to find ' . storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/${strArchiveCheck}");
    }

    if (defined($strSpoolPath))
    {
        storageTest()->remove("${strSpoolPath}/archive/" . $self->stanza() . "/out/${strArchiveFile}.ok");
    }
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $strArchiveChecksum = '72b9da071c13957fb4ca31f05dbd5c644297c2f7';

    foreach my $bS3 (false, true)
    {
    foreach my $bRemote ($bS3 ? (true) : (false, true))
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("rmt ${bRemote}, s3 ${bS3}")) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, bCompress => false, bS3 => $bS3});

        # Reduce console logging to detail
        $oHostDbMaster->configUpdate({&CFGDEF_SECTION_GLOBAL => {cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE) => lc(DETAIL)}});
        my $strLogDebug = '--' . cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE) . qw{=} . lc(DEBUG);

        # If S3 set process max to 2.  This seems like the best place for parallel testing since it will help speed S3 processing
        # without slowing down the other tests too much.
        if ($bS3)
        {
            $oHostBackup->configUpdate({&CFGDEF_SECTION_GLOBAL => {cfgOptionName(CFGOPT_PROCESS_MAX) => 2}});
            $oHostDbMaster->configUpdate({&CFGDEF_SECTION_GLOBAL => {cfgOptionName(CFGOPT_PROCESS_MAX) => 2}});

            # Reduce console logging to warn (even for debug exceptions)
            $oHostDbMaster->configUpdate({&CFGDEF_SECTION_GLOBAL => {cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE) => lc(WARN)}});
            $strLogDebug = '--' . cfgOptionName(CFGOPT_LOG_LEVEL_CONSOLE) . qw{=} . lc(WARN);
        }

        # Create the xlog path
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        storageTest()->pathCreate($strXlogPath, {bCreateParent => true});

        # Create the test path for pg_control
        storageTest()->pathCreate($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL, {bCreateParent => true});

        # Copy pg_control for stanza-create
        storageTest()->copy(
            $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin',
            $oHostDbMaster->dbBasePath() . qw{/} . DB_FILE_PGCONTROL);

        # Create archive-push command
        my $strCommandPush =
            $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() . ' --stanza=' . $self->stanza() .
            ' ' . cfgCommandName(CFGCMD_ARCHIVE_PUSH);

        my $strCommandGet =
            $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() . ' --stanza=' . $self->stanza() .
            ' ' . cfgCommandName(CFGCMD_ARCHIVE_GET);

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    archive.info missing');
        my $strSourceFile1 = $self->walSegment(1, 1, 1);
        storageTest()->pathCreate("${strXlogPath}/archive_status");
        my $strArchiveFile1 = $self->walGenerate($strXlogPath, WAL_VERSION_94, 1, $strSourceFile1);

        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile1}",
            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});

        $oHostDbMaster->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strXlogPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate(
            'stanza create',
            {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    push first WAL');

        my @stryExpectedWAL;
        my $strSourceFile = $self->walSegment(1, 1, 1);
        my $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 2, $strSourceFile);

        $oHostDbMaster->executeSimple(
            $strCommandPush . ($bRemote ? ' --cmd-ssh=/usr/bin/ssh' : '') . " ${strLogDebug} ${strXlogPath}/${strSourceFile}",
            {oLogTest => $self->expect()});
        push(@stryExpectedWAL, "${strSourceFile}-${strArchiveChecksum}");

        # Test that the WAL was pushed
        $self->archiveCheck($strSourceFile, $strArchiveChecksum, false);

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    get first WAL');

        # Remove WAL so it can be recovered
        storageTest()->remove("${strXlogPath}/${strSourceFile}", {bIgnoreMissing => false});

        $oHostDbMaster->executeSimple(
            $strCommandGet . " ${strLogDebug} ${strSourceFile} ${strXlogPath}/RECOVERYXLOG",
            {oLogTest => $self->expect()});

        # Check that the destination file exists
        if (storageDb()->exists("${strXlogPath}/RECOVERYXLOG"))
        {
            my ($strActualChecksum) = storageDb()->hashSize("${strXlogPath}/RECOVERYXLOG");

            if ($strActualChecksum ne $strArchiveChecksum)
            {
                confess "recovered file hash '${strActualChecksum}' does not match expected '${strArchiveChecksum}'";
            }
        }
        else
        {
            confess "archive file '${strXlogPath}/RECOVERYXLOG' is not in destination";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    push second WAL');

        # Generate second WAL segment
        $strSourceFile = $self->walSegment(1, 1, 2);
        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 2, $strSourceFile);

        # Create a temp file to make sure it is deleted later (skip when S3 since it doesn't use temp files)
        my $strArchiveTmp;

        if (!$bS3)
        {
            # Should succeed when temp file already exists
            &log(INFO, '    succeed when tmp WAL file exists');

            $strArchiveTmp =
                $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
                    substr($strSourceFile, 0, 16) . "/${strSourceFile}-${strArchiveChecksum}." . COMPRESS_EXT . qw{.} .
                    STORAGE_TEMP_EXT;

            executeTest('sudo chmod 770 ' . dirname($strArchiveTmp));
            storageTest()->put($strArchiveTmp, 'JUNK');

            if ($bRemote)
            {
                executeTest('sudo chown ' . $oHostBackup->userGet() . " ${strArchiveTmp}");
            }
        }

        # Push the WAL
        $oHostDbMaster->executeSimple(
            "${strCommandPush} --compress --archive-async --process-max=2 ${strXlogPath}/${strSourceFile}",
            {oLogTest => $self->expect()});
        push(@stryExpectedWAL, "${strSourceFile}-${strArchiveChecksum}." . COMPRESS_EXT);

        # Make sure the temp file no longer exists if it was created
        if (defined($strArchiveTmp))
        {
            my $oWait = waitInit(5);
            my $bFound = true;

            do
            {
                $bFound = storageTest()->exists($strArchiveTmp);
            }
            while ($bFound && waitMore($oWait));

            if ($bFound)
            {
                confess "${strArchiveTmp} should have been removed by archive command";
            }
        }

        # Test that the WAL was pushed
        $self->archiveCheck($strSourceFile, $strArchiveChecksum, true, $oHostDbMaster->spoolPath());

        # Remove from spool
        storageTest()->remove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    db version mismatch error');

        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
            {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}});

        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        $oHostDbMaster->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strXlogPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    db system-id mismatch error');

        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_BACKUP_KEY_SYSTEM_ID => 5000900090001855000}});

        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        $oHostDbMaster->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strXlogPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        # Restore the file to its original condition
        $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE));

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    stop');

        $oHostDbMaster->stop({strStanza => $oHostDbMaster->stanza()});

        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_STOP, oLogTest => $self->expect()});

        $oHostDbMaster->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strXlogPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_STOP, oLogTest => $self->expect()});

        $oHostDbMaster->start({strStanza => $oHostDbMaster->stanza()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    WAL duplicate ok');

        $oHostDbMaster->executeSimple($strCommandPush . " ${strXlogPath}/${strSourceFile}", {oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    WAL duplicate error');

        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 1, $strSourceFile);

        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    get second WAL');

        # Remove WAL so it can be recovered
        storageTest()->remove("${strXlogPath}/${strSourceFile}", {bIgnoreMissing => false});

        $oHostDbMaster->executeSimple(
            $strCommandGet . ($bRemote ? ' --cmd-ssh=/usr/bin/ssh' : '') . " ${strSourceFile} ${strXlogPath}/RECOVERYXLOG",
            {oLogTest => $self->expect()});

        # Check that the destination file exists
        if (storageDb()->exists("${strXlogPath}/RECOVERYXLOG"))
        {
            my ($strActualChecksum) = storageDb()->hashSize("${strXlogPath}/RECOVERYXLOG");

            if ($strActualChecksum ne $strArchiveChecksum)
            {
                confess "recovered file hash '${strActualChecksum}' does not match expected '${strArchiveChecksum}'";
            }
        }
        else
        {
            confess "archive file '${strXlogPath}/RECOVERYXLOG' is not in destination";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL');

        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 2, "${strSourceFile}.partial");
        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile}.partial",
            {oLogTest => $self->expect()});
        $self->archiveCheck("${strSourceFile}.partial", $strArchiveChecksum, false);

        push(@stryExpectedWAL, "${strSourceFile}.partial-${strArchiveChecksum}");

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL duplicate');

        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile}.partial", {oLogTest => $self->expect()});
        $self->archiveCheck(
            "${strSourceFile}.partial", $strArchiveChecksum, false);

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL with different checksum');

        $strArchiveFile = $self->walGenerate($strXlogPath, WAL_VERSION_94, 1, "${strSourceFile}.partial");
        $oHostDbMaster->executeSimple(
            $strCommandPush . " ${strXlogPath}/${strSourceFile}.partial",
            {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageRepo()->list(STORAGE_REPO_ARCHIVE . qw{/} . PG_VERSION_94 . '-1/0000000100000001')},
            '(' . join(', ', @stryExpectedWAL) . ')',
            'all WAL in archive', {iWaitSeconds => 5});
    }
    }
}

1;
