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

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;

use pgBackRestTest::Env::ArchiveInfo;
use pgBackRestTest::Env::BackupInfo;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Env::Manifest;
use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# archiveCheck
#
# Check that a WAL segment is present in the repository.
####################################################################################################################################
sub archiveCheck
{
    my $self = shift;
    my $oHostBackup = shift;
    my $strArchiveFile = shift;
    my $strArchiveChecksum = shift;
    my $strCompressType = shift;
    my $strSpoolPath = shift;

    # Build the archive name to check for at the destination
    my $strArchiveCheck = $oHostBackup->repoArchivePath(
        PG_VERSION_94 . "-1/" . substr($strArchiveFile, 0, 16) . "/${strArchiveFile}-${strArchiveChecksum}");

    if (defined($strCompressType))
    {
        $strArchiveCheck .= ".${strCompressType}";
    }

    my $oWait = waitInit(5);
    my $bFound = false;

    do
    {
        $bFound = storageRepo()->exists($strArchiveCheck);
    }
    while (!$bFound && waitMore($oWait));

    if (!$bFound)
    {
        confess "unable to find ${strArchiveCheck}";
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

    my $strArchiveChecksum = $self->walGenerateContentChecksum(PG_VERSION_94, {iSourceNo => 2});

    foreach my $rhRun
    (
        {vm => VM2, remote => false, storage =>    S3, encrypt =>  true, compress => BZ2},
        {vm => VM2, remote =>  true, storage => POSIX, encrypt =>  true, compress => BZ2},
        {vm => VM3, remote => false, storage => POSIX, encrypt =>  true, compress => LZ4},
        {vm => VM3, remote =>  true, storage =>    S3, encrypt => false, compress => ZST},
        {vm => VM4, remote => false, storage => AZURE, encrypt =>  true, compress => ZST},
        {vm => VM4, remote =>  true, storage => POSIX, encrypt => false, compress =>  GZ},
    )
    {
        # Only run tests for this vm
        next if ($rhRun->{vm} ne vmTest($self->vm()));

        # Increment the run, log, and decide whether this unit test should be run
        my $bRemote = $rhRun->{remote};
        my $strStorage = $rhRun->{storage};
        my $bEncrypt = $rhRun->{encrypt};
        my $strCompressType = $rhRun->{compress};

        if (!$self->begin("rmt ${bRemote}, storage ${strStorage}, enc ${bEncrypt}, cmp ${strCompressType}")) {next}

        # Create hosts, file object, and config
        my ($oHostDbPrimary, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, strStorage => $strStorage, bRepoEncrypt => $bEncrypt,
            strCompressType => NONE});

        # Reduce console logging to detail
        $oHostDbPrimary->configUpdate({&CFGDEF_SECTION_GLOBAL => {'log-level-console' => lc(DETAIL)}});

        # Create the wal path
        my $strWalPath = $oHostDbPrimary->dbBasePath() . '/pg_xlog';
        storageTest()->pathCreate($strWalPath, {bCreateParent => true});

        # Generate pg_control for stanza-create
        storageTest()->pathCreate($oHostDbPrimary->dbBasePath() . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
        $self->controlGenerate($oHostDbPrimary->dbBasePath(), PG_VERSION_94);

        # Create archive-push command
        my $strCommandPush =
            $oHostDbPrimary->backrestExe() . ' --config=' . $oHostDbPrimary->backrestConfig() . ' --stanza=' . $self->stanza() .
            ' archive-push';

        my $strCommandGet =
            $oHostDbPrimary->backrestExe() . ' --config=' . $oHostDbPrimary->backrestConfig() . ' --stanza=' . $self->stanza() .
            ' archive-get';

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    archive.info missing');
        my $strSourceFile1 = $self->walSegment(1, 1, 1);
        storageTest()->pathCreate("${strWalPath}/archive_status");
        my $strArchiveFile1 = $self->walGenerate($strWalPath, PG_VERSION_94, 1, $strSourceFile1);

        $oHostDbPrimary->executeSimple(
            $strCommandPush . " ${strWalPath}/${strSourceFile1}",
            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strWalPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        $oHostBackup->stanzaCreate('stanza create', {strOptionalParam => '--no-online'});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    push first WAL');

        my @stryExpectedWAL;
        my $strSourceFile = $self->walSegment(1, 1, 1);
        my $strArchiveFile = $self->walGenerate($strWalPath, PG_VERSION_94, 2, $strSourceFile);

        $oHostDbPrimary->executeSimple(
            $strCommandPush . ($bRemote ? ' --cmd-ssh=/usr/bin/ssh' : '') .
                " --compress-type=${strCompressType} ${strWalPath}/${strSourceFile}",
            {oLogTest => $self->expect()});
        push(@stryExpectedWAL, "${strSourceFile}-${strArchiveChecksum}.${strCompressType}");

        # Test that the WAL was pushed
        $self->archiveCheck($oHostBackup, $strSourceFile, $strArchiveChecksum, $strCompressType);

        # Remove from archive_status
        storageTest()->remove("${strWalPath}/archive_status/${strSourceFile}.ready");

        # Remove WAL
        storageTest()->remove("${strWalPath}/${strSourceFile}", {bIgnoreMissing => false});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    get missing WAL');

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " 700000007000000070000000 ${strWalPath}/RECOVERYXLOG",
            {iExpectedExitStatus => 1, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    get first WAL');

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " ${strSourceFile} ${strWalPath}/RECOVERYXLOG", {oLogTest => $self->expect()});

        # Check that the destination file exists
        if (storageTest()->exists("${strWalPath}/RECOVERYXLOG"))
        {
            my ($strActualChecksum) = storageTest()->hashSize("${strWalPath}/RECOVERYXLOG");

            if ($strActualChecksum ne $strArchiveChecksum)
            {
                confess "recovered file hash '${strActualChecksum}' does not match expected '${strArchiveChecksum}'";
            }
        }
        else
        {
            confess "archive file '${strWalPath}/RECOVERYXLOG' is not in destination";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    push second WAL');

        # Generate second WAL segment
        $strSourceFile = $self->walSegment(1, 1, 2);
        $strArchiveFile = $self->walGenerate($strWalPath, PG_VERSION_94, 2, $strSourceFile);

        # Create a temp file to make sure it is deleted later (skip when S3 since it doesn't use temp files)
        my $strArchiveTmp;

        if ($strStorage eq POSIX)
        {
            # Should succeed when temp file already exists
            &log(INFO, '    succeed when tmp WAL file exists');

            $strArchiveTmp =
                $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
                    substr($strSourceFile, 0, 16) . "/${strSourceFile}-${strArchiveChecksum}.${strCompressType}" . qw{.} .
                    STORAGE_TEMP_EXT;

            storageTest()->put($strArchiveTmp, 'JUNK');
        }

        # Push the WAL
        $oHostDbPrimary->executeSimple(
            "${strCommandPush} --compress-type=${strCompressType} --archive-async --process-max=2" .
                " ${strWalPath}/${strSourceFile}",
            {oLogTest => $self->expect()});
        push(@stryExpectedWAL, "${strSourceFile}-${strArchiveChecksum}.${strCompressType}");

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
        $self->archiveCheck($oHostBackup, $strSourceFile, $strArchiveChecksum, $strCompressType, $oHostDbPrimary->spoolPath());

        # Remove from archive_status
        storageTest()->remove("${strWalPath}/archive_status/${strSourceFile}.ready");

        # Remove from spool
        storageTest()->remove($oHostDbPrimary->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    push history file');

        storageTest()->put("${strWalPath}/00000002.history", 'HISTORYDATA');
        storageTest()->put("${strWalPath}/archive_status/00000002.history.ready", undef);

        $oHostDbPrimary->executeSimple(
            "${strCommandPush} --archive-async ${strWalPath}/00000002.history",
            {oLogTest => $self->expect()});

        if (!storageRepo()->exists($oHostBackup->repoArchivePath(PG_VERSION_94 . '-1/00000002.history')))
        {
            confess 'unable to find history file in archive';
        }

        storageTest()->remove("${strWalPath}/archive_status/00000002.history.ready");

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    db version mismatch error - archive-get unable to retrieve archiveId');

        # db section and corresponding history munged
        $oHostBackup->infoMunge(
            $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE),
            {&INFO_ARCHIVE_SECTION_DB_HISTORY => {'1' => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}}});

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strWalPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        # Restore the file to its original condition
        $oHostBackup->infoRestore($oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE));

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    db system-id mismatch error');

        $oHostBackup->infoMunge(
            $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE),
            {&INFO_ARCHIVE_SECTION_DB => {&INFO_BACKUP_KEY_SYSTEM_ID => 5000900090001855000},
            &INFO_ARCHIVE_SECTION_DB_HISTORY => {'1' => {&INFO_ARCHIVE_KEY_DB_ID => 5000900090001855000}}});

        $oHostDbPrimary->executeSimple(
            $strCommandPush . " ${strWalPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strWalPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

        # Restore the file to its original condition
        $oHostBackup->infoRestore($oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE));

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    stop');

        $oHostDbPrimary->stop({strStanza => $oHostDbPrimary->stanza()});

        $oHostDbPrimary->executeSimple(
            $strCommandPush . " ${strWalPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_STOP, oLogTest => $self->expect()});

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " ${strSourceFile1} ${strWalPath}/RECOVERYXLOG",
            {iExpectedExitStatus => ERROR_STOP, oLogTest => $self->expect()});

        $oHostDbPrimary->start({strStanza => $oHostDbPrimary->stanza()});

        storageTest->remove("${strWalPath}/RECOVERYXLOG", {bIgnoreMissing => false});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    WAL duplicate ok');

        $oHostDbPrimary->executeSimple($strCommandPush . " ${strWalPath}/${strSourceFile}", {oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    WAL duplicate error');

        $strArchiveFile = $self->walGenerate($strWalPath, PG_VERSION_94, 1, $strSourceFile);

        $oHostDbPrimary->executeSimple(
            $strCommandPush . " ${strWalPath}/${strSourceFile}",
            {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

        # Remove WAL
        storageTest()->remove("${strWalPath}/${strSourceFile}", {bIgnoreMissing => false});

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, "    get second WAL (${strSourceFile})");

        $oHostDbPrimary->executeSimple(
            $strCommandGet . ($bRemote ? ' --cmd-ssh=/usr/bin/ssh' : '') . " --archive-async" .
            ($strStorage eq POSIX ? " --repo-type=cifs" : '') . " --archive-timeout=5 ${strSourceFile} ${strWalPath}/RECOVERYXLOG",
            {oLogTest => $self->expect()});

        # Check that the destination file exists
        if (storageTest()->exists("${strWalPath}/RECOVERYXLOG"))
        {
            my ($strActualChecksum) = storageTest()->hashSize("${strWalPath}/RECOVERYXLOG");

            if ($strActualChecksum ne $strArchiveChecksum)
            {
                confess "recovered file hash '${strActualChecksum}' does not match expected '${strArchiveChecksum}'";
            }
        }
        else
        {
            confess "archive file '${strWalPath}/RECOVERYXLOG' is not in destination";
        }

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, "    get history file");

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " --archive-async 00000001.history ${strWalPath}/00000001.history",
            {iExpectedExitStatus => 1, oLogTest => $self->expect()});

        $oHostDbPrimary->executeSimple(
            $strCommandGet . " --archive-async 00000002.history ${strWalPath}/00000002.history",
            {oLogTest => $self->expect()});

        if (${storageTest()->get("${strWalPath}/00000002.history")} ne 'HISTORYDATA')
        {
            confess 'history contents do not match original';
        }

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL');

        $strArchiveFile = $self->walGenerate($strWalPath, PG_VERSION_94, 2, "${strSourceFile}.partial");
        $oHostDbPrimary->executeSimple(
            $strCommandPush . " ${strWalPath}/${strSourceFile}.partial",
            {oLogTest => $self->expect()});
        $self->archiveCheck($oHostBackup, "${strSourceFile}.partial", $strArchiveChecksum);

        push(@stryExpectedWAL, "${strSourceFile}.partial-${strArchiveChecksum}");

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL duplicate');

        $oHostDbPrimary->executeSimple(
            $strCommandPush . " ${strWalPath}/${strSourceFile}.partial", {oLogTest => $self->expect()});
        $self->archiveCheck($oHostBackup, "${strSourceFile}.partial", $strArchiveChecksum);

        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    .partial WAL with different checksum');

        $strArchiveFile = $self->walGenerate($strWalPath, PG_VERSION_94, 1, "${strSourceFile}.partial");
        $oHostDbPrimary->executeSimple(
            $strCommandPush . " ${strWalPath}/${strSourceFile}.partial",
            {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageRepo()->list($oHostBackup->repoArchivePath(PG_VERSION_94 . '-1/0000000100000001'))},
            '(' . join(', ', @stryExpectedWAL) . ')',
            'all WAL in archive', {iWaitSeconds => 5});
    }
}

1;
