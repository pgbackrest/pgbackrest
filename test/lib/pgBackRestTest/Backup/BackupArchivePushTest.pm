####################################################################################################################################
# BackupArchivePushTest.pm - Tests for archive-push command
####################################################################################################################################
package pgBackRestTest::Backup::BackupArchivePushTest;
use parent 'pgBackRestTest::Backup::BackupCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::ArchiveInfo;
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

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Backup::BackupCommonTest;

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
        confess 'unable to find ' . $strArchiveCheck;
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
        if (!$self->begin("rmt ${bRemote}, cmp ${bCompress}, arc_async ${bArchiveAsync}")) {next}

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
            ' --no-fork --stanza=db archive-push';

        # Test missing archive.info file
        &log(INFO, '    test archive.info missing');
        my ($strArchiveFile1, $strSourceFile1) = $self->archiveGenerate($oFile, $strXlogPath, 1, 1, WAL_VERSION_94);
        $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile1}",
            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $self->expect()});

        # Create the archive info file
        $oHostBackup->stanzaCreate('create required data for stanza',
            {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

        if ($bArchiveAsync)
        {
            my $strDuplicateWal =
                ($bRemote ? $oHostDbMaster->spoolPath() :
                            $oHostBackup->repoPath()) .
                '/archive/' . $self->stanza() . "/out/${strArchiveFile1}-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";

            fileRemove($strDuplicateWal);
        }

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

                ($strArchiveFile, $strSourceFile) =
                    $self->archiveGenerate($oFile, $strXlogPath, 2, $iArchiveNo, WAL_VERSION_94);
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
                        substr($strArchiveFile, 0, 16) . "/${strArchiveFile}" .
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
                        " ${strSourceFile}",
                    {oLogTest => $self->expect()});

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
                    # load the archive info file and munge it for testing by breaking the database version
                    $oHostBackup->infoMunge(
                        $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE),
                        {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}});

                    &log(INFO, '        test db version mismatch error');

                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strSourceFile}",
                        {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

                    # Break the system id
                    $oHostBackup->infoMunge(
                        $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE),
                        {&INFO_ARCHIVE_SECTION_DB => {&INFO_BACKUP_KEY_SYSTEM_ID => 5000900090001855000}});

                    &log(INFO, '        test db system-id mismatch error');

                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strSourceFile}",
                        {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $self->expect()});

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
                            $strCommand . " ${strSourceFile}",
                            {oLogTest => $self->expect(), iExpectedExitStatus => ERROR_STOP});

                        $oHostDbMaster->start({strStanza => $bArchiveAsync ? undef : $self->stanza()});
                    }

                    # Should succeed because checksum is the same
                    &log(INFO, '        test archive duplicate ok');

                    $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $self->expect()});

                    # Now it should break on archive duplication (because checksum is different
                    &log(INFO, '        test archive duplicate error');

                    ($strArchiveFile, $strSourceFile) =
                        $self->archiveGenerate($oFile, $strXlogPath, 1, $iArchiveNo, WAL_VERSION_94);

                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strSourceFile}",
                        {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

                    if ($bArchiveAsync)
                    {
                        my $strDuplicateWal =
                            ($bRemote ? $oHostDbMaster->spoolPath() :
                                        $oHostBackup->repoPath()) .
                            '/archive/' . $self->stanza() . "/out/${strArchiveFile}-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";

                        fileRemove($strDuplicateWal);
                    }

                    # Test .partial archive
                    &log(INFO, '        test .partial archive');
                    ($strArchiveFile, $strSourceFile) =
                        $self->archiveGenerate($oFile, $strXlogPath, 2, $iArchiveNo, WAL_VERSION_94, true);
                    $oHostDbMaster->executeSimple(
                        $strCommand . " --no-" . OPTION_REPO_SYNC . " ${strSourceFile}", {oLogTest => $self->expect()});
                    $self->archiveCheck($oFile, $strArchiveFile, $strArchiveChecksum, $bCompress);

                    # Test .partial archive duplicate
                    &log(INFO, '        test .partial archive duplicate');
                    $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $self->expect()});

                    # Test .partial archive with different checksum
                    &log(INFO, '        test .partial archive with different checksum');
                    ($strArchiveFile, $strSourceFile) =
                        $self->archiveGenerate($oFile, $strXlogPath, 1, $iArchiveNo, WAL_VERSION_94, true);
                    $oHostDbMaster->executeSimple(
                        $strCommand . " ${strSourceFile}",
                        {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $self->expect()});

                    if ($bArchiveAsync)
                    {
                        my $strDuplicateWal =
                            ($bRemote ? $oHostDbMaster->spoolPath() : $oHostBackup->repoPath()) .
                            '/archive/' . $self->stanza() . "/out/${strArchiveFile}-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";

                        fileRemove($strDuplicateWal);
                    }
                }

                $self->archiveCheck($oFile, $strArchiveFile, $strArchiveChecksum, $bCompress);
            }

            # Might be nice to add tests for .backup files here (but this is already tested in full backup)
        }

        if (defined($self->expect()))
        {
            $self->expect()->supplementalAdd($oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE));
        }
    }
    }
    }
}

1;
