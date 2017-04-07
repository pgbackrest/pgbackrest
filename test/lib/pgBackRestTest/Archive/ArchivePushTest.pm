####################################################################################################################################
# ArchivePushTest.pm - Tests for archive-push command
####################################################################################################################################
package pgBackRestTest::Archive::ArchivePushTest;
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
use pgBackRest::Manifest;

use pgBackRestTest::Common::Env::EnvHostTest;
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
    my $oFile = shift;
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
        $bFound = $oFile->exists(PATH_BACKUP_ARCHIVE, $strArchiveCheck);
    }
    while (!$bFound && waitMore($oWait));

    if (!$bFound)
    {
        confess 'unable to find ' . $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveCheck);
    }

    if (defined($strSpoolPath))
    {
        fileRemove("${strSpoolPath}/archive/" . $self->stanza() . "/out/${strArchiveFile}.ok");
    }
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $strArchiveChecksum = '72b9da071c13957fb4ca31f05dbd5c644297c2f7';
    my $iArchiveMax = 3;

    for (my $bRemote = false; $bRemote <= true; $bRemote++)
    {
    for (my $bCompress = false; $bCompress <= true; $bCompress++)
    {
    for (my $bArchiveAsync = false; $bArchiveAsync <= true; $bArchiveAsync++)
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("rmt ${bRemote}, cmp ${bCompress}, arc_async ${bArchiveAsync}", $self->processMax() == 1)) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oFile) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, bCompress => $bCompress, bArchiveAsync => $bArchiveAsync});

        # Create the xlog path
        my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
        filePathCreate($strXlogPath, undef, false, true);

        # Create the test path for pg_control
        filePathCreate(($oHostDbMaster->dbBasePath() . '/' . DB_PATH_GLOBAL), undef, false, true);

        # Copy pg_control for stanza-create
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);

        my $strCommand =
            $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() .
            ($bArchiveAsync ? ' --log-level-console=detail' : '') . ' --stanza=db archive-push';

        # Test missing archive.info file
        &log(INFO, '    test archive.info missing');
        my $strSourceFile1 = $self->walSegment(1, 1, 1);
        filePathCreate("${strXlogPath}/archive_status");
        my $strArchiveFile1 = $self->walGenerate($oFile, $strXlogPath, WAL_VERSION_94, 1, $strSourceFile1);

        $oHostDbMaster->executeSimple($strCommand . " ${strXlogPath}/${strSourceFile1} --archive-max-mb=24",
            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});
        fileRemove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile1}.error") if $bArchiveAsync;

        # Create the archive info file
        $oHostBackup->stanzaCreate('create required data for stanza',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        my @stryExpectedWAL;

        # Loop through backups
        for (my $iBackup = 1; $iBackup <= 3; $iBackup++)
        {
            my $strArchiveFile;

            # Loop through archive files
            for (my $iArchive = 1; $iArchive <= $iArchiveMax; $iArchive++)
            {
                my $strSourceFile;

                # Construct the archive filename
                my $iArchiveNo = (($iBackup - 1) * $iArchiveMax + ($iArchive - 1)) + 1;

                if ($iArchiveNo > 255)
                {
                    confess 'backup total * archive total cannot be greater than 255';
                }

                $strSourceFile = $self->walSegment(1, 1, $iArchiveNo);
                $strArchiveFile = $self->walGenerate($oFile, $strXlogPath, WAL_VERSION_94, 2, $strSourceFile);

                &log(INFO, '    backup ' . sprintf('%02d', $iBackup) .
                           ', archive ' .sprintf('%02x', $iArchive) .
                           " - ${strArchiveFile}");

                my $strArchiveTmp = undef;

                if ($iBackup == 1 && $iArchive == 2)
                {
                    # Should succeed when temp file already exists
                    &log(INFO, '        test archive when tmp file exists');

                    $strArchiveTmp =
                        $oHostBackup->repoPath() . '/archive/' . $self->stanza() . '/' . PG_VERSION_94 . '-1/' .
                        substr($strSourceFile, 0, 16) . "/${strSourceFile}" .
                        ($bCompress ? ".$oFile->{strCompressExtension}" : '') . '.pgbackrest.tmp';

                    executeTest('sudo chmod 770 ' . dirname($strArchiveTmp));
                    fileStringWrite($strArchiveTmp, 'JUNK');

                    if ($bRemote)
                    {
                        executeTest('sudo chown ' . $oHostBackup->userGet() . " ${strArchiveTmp}");
                    }
                }

                $oHostDbMaster->executeSimple(
                    $strCommand .  ($bRemote && $iBackup == $iArchive ? ' --cmd-ssh=/usr/bin/ssh' : '') .
                        " ${strXlogPath}/${strSourceFile}",
                    {oLogTest => $self->expect()});
                push(
                    @stryExpectedWAL, "${strSourceFile}-${strArchiveChecksum}" .
                    ($bCompress ? ".$oFile->{strCompressExtension}" : ''));

                # Make sure the temp file no longer exists
                if (defined($strArchiveTmp))
                {
                    my $oWait = waitInit(5);
                    my $bFound = true;

                    do
                    {
                        $bFound = fileExists($strArchiveTmp);
                    }
                    while ($bFound && waitMore($oWait));

                    if ($bFound)
                    {
                        confess "${strArchiveTmp} should have been removed by archive command";
                    }
                }

                if ($iArchive == $iBackup)
                {
                    fileRemove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.ok") if $bArchiveAsync;

                    &log(INFO, '        test db version mismatch error');

                    $oHostBackup->infoMunge(
                        $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE),
                        {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}});

                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strXlogPath}/${strSourceFile}",
                        {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

                    fileRemove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.error") if $bArchiveAsync;

                    &log(INFO, '        test db system-id mismatch error');

                    $oHostBackup->infoMunge(
                        $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE),
                        {&INFO_ARCHIVE_SECTION_DB => {&INFO_BACKUP_KEY_SYSTEM_ID => 5000900090001855000}});

                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strXlogPath}/${strSourceFile}",
                        {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

                    fileRemove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.error") if $bArchiveAsync;;

                    # Restore the file to its original condition
                    $oHostBackup->infoRestore($oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE));

                    # Fail because the process was killed
                    if ($iBackup == 1 && !$bCompress)
                    {
                        &log(INFO, '        test stop');

                        if ($bArchiveAsync)
                        {
                            my $oExecArchive =
                                $oHostDbMaster->execute(
                                    $strCommand . ' --test --test-delay=5 --test-point=' .
                                    lc(TEST_ARCHIVE_PUSH_ASYNC_START) . "=y ${strSourceFile}",
                                    {oLogTest => $self->expect(), iExpectedExitStatus => ERROR_TERM});
                            $oExecArchive->begin();
                            $oExecArchive->end(TEST_ARCHIVE_PUSH_ASYNC_START);

                            $oHostDbMaster->stop({bForce => true});
                            $oExecArchive->end();
                        }
                        else
                        {
                            $oHostDbMaster->stop({strStanza => $oHostDbMaster->stanza()});
                        }

                        $oHostDbMaster->executeSimple(
                            $strCommand . " ${strXlogPath}/${strSourceFile}",
                            {oLogTest => $self->expect(), iExpectedExitStatus => ERROR_STOP});

                        $oHostDbMaster->start({strStanza => $bArchiveAsync ? undef : $self->stanza()});
                    }

                    # Should succeed because checksum is the same
                    &log(INFO, '        test archive duplicate ok');

                    $oHostDbMaster->executeSimple($strCommand . " ${strXlogPath}/${strSourceFile}", {oLogTest => $self->expect()});

                    fileRemove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.ok") if $bArchiveAsync;

                    # Now it should break on archive duplication (because checksum is different
                    &log(INFO, "        test archive duplicate error");

                    $strArchiveFile = $self->walGenerate($oFile, $strXlogPath, WAL_VERSION_94, 1, $strSourceFile);

                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strXlogPath}/${strSourceFile}",
                        {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

                    fileRemove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.error") if $bArchiveAsync;

                    # Test .partial archive
                    &log(INFO, "        test .partial archive");
                    $strArchiveFile = $self->walGenerate($oFile, $strXlogPath, WAL_VERSION_94, 2, "${strSourceFile}.partial");
                    $oHostDbMaster->executeSimple(
                        $strCommand . " --no-" . OPTION_REPO_SYNC . " ${strXlogPath}/${strSourceFile}.partial",
                        {oLogTest => $self->expect()});
                    $self->archiveCheck(
                        $oFile, "${strSourceFile}.partial", $strArchiveChecksum, $bCompress,
                        $bArchiveAsync ? $oHostDbMaster->spoolPath() : undef);

                    push(
                        @stryExpectedWAL, "${strSourceFile}.partial-${strArchiveChecksum}" .
                        ($bCompress ? ".$oFile->{strCompressExtension}" : ''));

                    # Test .partial archive duplicate
                    &log(INFO, '        test .partial archive duplicate');
                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strXlogPath}/${strSourceFile}.partial", {oLogTest => $self->expect()});
                    $self->archiveCheck(
                        $oFile, "${strSourceFile}.partial", $strArchiveChecksum, $bCompress,
                        $bArchiveAsync ? $oHostDbMaster->spoolPath() : undef);

                    # Test .partial archive with different checksum
                    &log(INFO, '        test .partial archive with different checksum');
                    $strArchiveFile = $self->walGenerate($oFile, $strXlogPath, WAL_VERSION_94, 1, "${strSourceFile}.partial");
                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strXlogPath}/${strSourceFile}.partial",
                        {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

                    fileRemove($oHostDbMaster->spoolPath() . '/archive/' . $self->stanza() . "/out/${strSourceFile}.error") if $bArchiveAsync;
                }
                else
                {
                    $self->archiveCheck(
                        $oFile, $strSourceFile, $strArchiveChecksum, $bCompress,
                        $bArchiveAsync ? $oHostDbMaster->spoolPath() : undef);
                }
            }
        }

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oFile->list(PATH_BACKUP_ARCHIVE, PG_VERSION_94 . '-1/0000000100000001')},
            '(' . join(', ', @stryExpectedWAL) . ')',
            'all WAL in archive', {iWaitSeconds => 5});

        #---------------------------------------------------------------------------------------------------------------------------
        if (defined($self->expect()))
        {
            sleep(1); # Ugly hack to ensure repo is stable before checking files - replace in new tests
            $self->expect()->supplementalAdd($oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE));
        }
    }
    }
    }
}

1;
