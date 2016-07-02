####################################################################################################################################
# BackupTest.pm - Unit Tests for pgBackRest::Backup and pgBackRest::Restore
####################################################################################################################################
package pgBackRestTest::Backup::BackupTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use DBI;
use Exporter qw(import);
use Fcntl ':mode';
use File::Basename qw(dirname);
use File::Copy 'cp';
use File::stat;
use Time::HiRes qw(gettimeofday);

use pgBackRest::Archive;
use pgBackRest::ArchiveInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Db;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::RemoteMaster;
use pgBackRest::Version;

use pgBackRestTest::Backup::BackupCommonTest;
use pgBackRestTest::Backup::Common::ExpireCommonTest;
use pgBackRestTest::Backup::Common::HostBackupTest;
use pgBackRestTest::Backup::Common::HostBaseTest;
use pgBackRestTest::Backup::Common::HostDbTest;
use pgBackRestTest::Backup::Common::HostDbSyntheticTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::HostTest;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::CommonTest;

####################################################################################################################################
# Archive helper functions
####################################################################################################################################
# Generate an archive log for testing
sub archiveGenerate
{
    my $oFile = shift;
    my $strXlogPath = shift;
    my $iSourceNo = shift;
    my $iArchiveNo = shift;
    my $bPartial = shift;

    my $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo)) .
                         (defined($bPartial) && $bPartial ? '.partial' : '');
    my $strArchiveTestFile = testDataPath() . "/test.archive${iSourceNo}.bin";

    my $strSourceFile = "${strXlogPath}/${strArchiveFile}";

    $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile, # Source file
                 PATH_DB_ABSOLUTE, $strSourceFile,      # Destination file
                 false,                                 # Source is not compressed
                 false);                                # Destination is not compressed

    return $strArchiveFile, $strSourceFile;
}

# Check that an archive log is present
sub archiveCheck
{
    my $oFile = shift;
    my $strArchiveFile = shift;
    my $strArchiveChecksum = shift;
    my $bCompress = shift;

    # Build the archive name to check for at the destination
    my $strArchiveCheck = PG_VERSION_93 . "-1/${strArchiveFile}-${strArchiveChecksum}";

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
# backupTestRun
####################################################################################################################################
sub backupTestRun
{
    my $strTest = shift;
    my $iThreadMax = shift;
    my $bVmOut = shift;

    # If no test was specified, then run them all
    if (!defined($strTest))
    {
        $strTest = 'all';
    }

    # Setup global variables
    my $oHostGroup = hostGroupGet();
    my $strTestPath = $oHostGroup->paramGet(HOST_PARAM_TEST_PATH);

    # Setup test variables
    my $iRun;
    my $strModule = 'backup';
    my $strThisTest;
    my $strStanza = HOST_STANZA;
    my $oLogTest;

    my $strArchiveChecksum = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
    my $iArchiveMax = 3;
    my $strArchiveTestFile = testDataPath() . '/test.archive2.bin';
    my $strArchiveTestFile2 = testDataPath() . '/test.archive1.bin';

    pathModeDefaultSet('0700');
    fileModeDefaultSet('0600');

    # Print test banner
    if (!$bVmOut)
    {
        &log(INFO, 'BACKUP MODULE ******************************************************************');
        &log(INFO, "THREAD-MAX: ${iThreadMax}\n");
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-push
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'archive-push';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strThisTest}\n");
        }

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
            for (my $bCompress = false; $bCompress <= true; $bCompress++)
            {
            for (my $bArchiveAsync = false; $bArchiveAsync <= true; $bArchiveAsync++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!testRun(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, " .
                                            "arc_async ${bArchiveAsync}",
                                            $iThreadMax == 1 ? $strModule : undef,
                                            $iThreadMax == 1 ? $strThisTest: undef,
                                            \$oLogTest)) {next}

                # Create hosts, file object, and config
                my ($oHostDbMaster, $oHostBackup, $oFile) = backupTestSetup(
                    $bRemote, true, $oLogTest, {bCompress => $bCompress, bArchiveAsync => $bArchiveAsync});

                # Create the xlog path
                my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
                filePathCreate($strXlogPath, undef, false, true);

                my $strCommand =
                    $oHostDbMaster->backrestExe() . ' --config=' . $oHostDbMaster->backrestConfig() .
                    ' --no-fork --stanza=db archive-push';

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

                        ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 2, $iArchiveNo);
                        &log(INFO, '    backup ' . sprintf('%02d', $iBackup) .
                                   ', archive ' .sprintf('%02x', $iArchive) .
                                   " - ${strArchiveFile}");

                        if ($iBackup == 1 && $iArchive == 2)
                        {
                            # Should succeed when temp file already exists
                            &log(INFO, '        test archive when tmp file exists');

                            my $strArchiveTmp =
                                $oHostBackup->repoPath() . "/archive/${strStanza}/" .
                                PG_VERSION_93 . '-1/' . substr($strArchiveFile, 0, 16) . "/${strArchiveFile}.tmp";

                            executeTest('sudo chmod 770 ' . dirname($strArchiveTmp));
                            fileStringWrite($strArchiveTmp, 'JUNK');
                        }

                        $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});

                        if ($iArchive == $iBackup)
                        {
                            # load the archive info file so it can be munged for testing
                            my $strInfoFile = $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE);
                            executeTest("sudo chmod 660 ${strInfoFile}");
                            my %oInfo;
                            iniLoad($strInfoFile, \%oInfo);
                            my $strDbVersion = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION};
                            my $ullDbSysId = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID};

                            # Break the database version
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = '8.0';
                            testIniSave($strInfoFile, \%oInfo, true);

                            &log(INFO, '        test db version mismatch error');

                            $oHostDbMaster->executeSimple(
                                $strCommand . " ${strSourceFile}",
                                {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $oLogTest});

                            # Break the system id
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = $strDbVersion;
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID} = '5000900090001855000';
                            testIniSave($strInfoFile, \%oInfo, true);

                            &log(INFO, '        test db system-id mismatch error');

                            $oHostDbMaster->executeSimple(
                                $strCommand . " ${strSourceFile}",
                                {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $oLogTest});

                            # Move settings back to original
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID} = $ullDbSysId;
                            testIniSave($strInfoFile, \%oInfo, true);

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
                                            {oLogTest => $oLogTest, iExpectedExitStatus => ERROR_TERM});
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
                                    {oLogTest => $oLogTest, iExpectedExitStatus => ERROR_STOP});

                                $oHostDbMaster->start({strStanza => $bArchiveAsync ? undef : $strStanza});
                            }

                            # Should succeed because checksum is the same
                            &log(INFO, '        test archive duplicate ok');

                            $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});

                            # Now it should break on archive duplication (because checksum is different
                            &log(INFO, '        test archive duplicate error');

                            ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 1, $iArchiveNo);
                            $oHostDbMaster->executeSimple(
                                $strCommand . " ${strSourceFile}",
                                {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $oLogTest});

                            if ($bArchiveAsync)
                            {
                                my $strDuplicateWal =
                                    ($bRemote ? $oHostDbMaster->spoolPath() :
                                                $oHostBackup->repoPath()) .
                                    "/archive/${strStanza}/out/${strArchiveFile}-4518a0fdf41d796760b384a358270d4682589820";

                                fileRemove($strDuplicateWal);
                                    # or confess "unable to remove duplicate WAL segment created for testing: ${strDuplicateWal}";
                            }

                            # Test .partial archive
                            &log(INFO, '        test .partial archive');
                            ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 2, $iArchiveNo, true);
                            $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});
                            archiveCheck($oFile, $strArchiveFile, $strArchiveChecksum, $bCompress);

                            # Test .partial archive duplicate
                            &log(INFO, '        test .partial archive duplicate');
                            $oHostDbMaster->executeSimple($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});

                            # Test .partial archive with different checksum
                            &log(INFO, '        test .partial archive with different checksum');
                            ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 1, $iArchiveNo, true);
                            $oHostDbMaster->executeSimple(
                                $strCommand . " ${strSourceFile}",
                                {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $oLogTest});

                            if ($bArchiveAsync)
                            {
                                my $strDuplicateWal =
                                    ($bRemote ? $oHostDbMaster->spoolPath() : $oHostBackup->repoPath()) .
                                    "/archive/${strStanza}/out/${strArchiveFile}-4518a0fdf41d796760b384a358270d4682589820";

                                fileRemove($strDuplicateWal);
                                    # or confess "unable to remove duplicate WAL segment created for testing: ${strDuplicateWal}";
                            }
                        }

                        archiveCheck($oFile, $strArchiveFile, $strArchiveChecksum, $bCompress);
                    }

                    # Might be nice to add tests for .backup files here (but this is already tested in full backup)
                }

                if (defined($oLogTest))
                {
                    $oLogTest->supplementalAdd($oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/archive.info');
                }
            }
            }
        }

        testCleanup(\$oLogTest);
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-stop
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'archive-stop';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strThisTest}\n");
        }

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
            for (my $bCompress = false; $bCompress <= true; $bCompress++)
            {
            for (my $iError = 0; $iError <= $bRemote; $iError++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!testRun(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}" .
                                            ', error ' . ($iError ? 'connect' : 'version'),
                                            $iThreadMax == 1 ? $strModule : undef,
                                            $iThreadMax == 1 ? $strThisTest: undef,
                                            \$oLogTest)) {next}

                # Create hosts, file object, and config
                my ($oHostDbMaster, $oHostBackup, $oFile) = backupTestSetup(
                    $bRemote, true, $oLogTest, {bCompress => $bCompress, bArchiveAsync => true});

                # Create the xlog path
                my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
                filePathCreate($strXlogPath, undef, false, true);

                # Push a WAL segment
                $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 1);

                # load the archive info file so it can be munged for testing
                my $strInfoFile = $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE);
                executeTest("sudo chmod 660 ${strInfoFile}");
                my %oInfo;
                iniLoad($strInfoFile, \%oInfo);
                my $strDbVersion = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION};
                my $ullDbSysId = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID};

                # Break the database version
                if ($iError == 0)
                {
                    $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = '8.0';
                    testIniSave($strInfoFile, \%oInfo, true);
                }

                # Push two more segments with errors to exceed archive-max-mb
                $oHostDbMaster->archivePush(
                    $strXlogPath, $strArchiveTestFile, 2, $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

                $oHostDbMaster->archivePush(
                    $strXlogPath, $strArchiveTestFile, 3, $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

                # Now this segment will get dropped
                $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 4);

                # Fix the database version
                if ($iError == 0)
                {
                    $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = PG_VERSION_93;
                    testIniSave($strInfoFile, \%oInfo, true);
                }

                # Remove the stop file
                fileRemove($oHostDbMaster->spoolPath() . '/stop/db-archive.stop');

                # Push two more segments - only #4 should be missing from the archive at the end
                $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 5);
                $oHostDbMaster->archivePush($strXlogPath, $strArchiveTestFile, 6);
            }
            }
        }

        testCleanup(\$oLogTest);
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-get
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'archive-get';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strThisTest}\n");
        }

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
            for (my $bCompress = false; $bCompress <= true; $bCompress++)
            {
            for (my $bExists = false; $bExists <= true; $bExists++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!testRun(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, exists ${bExists}",
                                            $iThreadMax == 1 ? $strModule : undef,
                                            $iThreadMax == 1 ? $strThisTest: undef,
                                            \$oLogTest)) {next}

                # Create hosts, file object, and config
                my ($oHostDbMaster, $oHostBackup, $oFile) = backupTestSetup(
                    $bRemote, true, $oLogTest, {bCompress => $bCompress});

                # Create the xlog path
                my $strXlogPath = $oHostDbMaster->dbBasePath() . '/pg_xlog';
                filePathCreate($strXlogPath, undef, false, true);

                my $strCommand =
                    $oHostDbMaster->backrestExe() .
                    ' --config=' . $oHostDbMaster->backrestConfig() .
                    " --stanza=${strStanza} archive-get";

                $oHostDbMaster->executeSimple(
                        $strCommand . " 000000010000000100000001 ${strXlogPath}/000000010000000100000001",
                        {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $oLogTest});

                # Create the archive info file
                filePathCreate($oFile->pathGet(PATH_BACKUP_ARCHIVE), '0770', undef, true);
                (new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE)))->check(PG_VERSION_93, 1234567890123456789);

                if (defined($oLogTest))
                {
                    $oLogTest->supplementalAdd($oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/archive.info');
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

                        filePathCreate(
                            dirname(
                                $oFile->pathGet(PATH_BACKUP_ARCHIVE, PG_VERSION_93 . "-1/${strSourceFile}")), '0770', true, true);

                        $oFile->copy(
                            PATH_DB_ABSOLUTE, $strArchiveTestFile,  # Source file
                            PATH_BACKUP_ARCHIVE, PG_VERSION_93 .    # Destination file
                                "-1/${strSourceFile}",
                            false,                                  # Source is not compressed
                            $bCompress,                             # Destination compress based on test
                            undef, undef,                           # Unused params
                            '0660',                                 # Mode
                            true);                                  # Create path if it does not exist

                        my $strDestinationFile = "${strXlogPath}/${strArchiveFile}";

                        $oHostDbMaster->executeSimple(
                            $strCommand . " ${strArchiveFile} ${strDestinationFile}", {oLogTest => $oLogTest});

                        # Check that the destination file exists
                        if ($oFile->exists(PATH_DB_ABSOLUTE, $strDestinationFile))
                        {
                            if ($oFile->hash(PATH_DB_ABSOLUTE, $strDestinationFile) ne $strArchiveChecksum)
                            {
                                confess "archive file hash does not match ${strArchiveChecksum}";
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
                        {iExpectedExitStatus => ERROR_STOP, oLogTest => $oLogTest});

                    $oHostDbMaster->start();

                    $oHostDbMaster->executeSimple(
                        $strCommand . " 000000090000000900000009 ${strXlogPath}/RECOVERYXLOG",
                        {iExpectedExitStatus => 1, oLogTest => $oLogTest});
                }
            }
            }
        }

        testCleanup(\$oLogTest);
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test expire
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'expire';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strThisTest}\n");
        }

        if (testRun(++$iRun,
                                    "local",
                                    $iThreadMax == 1 ? $strModule : undef,
                                    $iThreadMax == 1 ? $strThisTest: undef,
                                    \$oLogTest))
        {
            # Create hosts, file object, and config
            my ($oHostDbMaster, $oHostBackup, $oFile) = backupTestSetup(false, true, $oLogTest);

            # Create the test object
            my $oExpireTest = new pgBackRestTest::Backup::Common::ExpireCommonTest($oHostBackup, $oFile, $oLogTest);

            $oExpireTest->stanzaCreate($strStanza, PG_VERSION_92);
            use constant SECONDS_PER_DAY => 86400;
            my $lBaseTime = time() - (SECONDS_PER_DAY * 56);

            #-----------------------------------------------------------------------------------------------------------------------
            my $strDescription = 'Nothing to expire';

            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, 246);

            $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest full backup, archive expire falls on segment major boundary';

            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest full backup';

            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, 256);
            $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest diff backup, archive expire does not fall on major segment boundary';

            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, undef, 0);
            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, undef, 0);
            $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest diff backup (cascade to incr)';

            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire archive based on newest incr backup';

            $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_INCR, 1, $strDescription);

            testCleanup(\$oLogTest);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test synthetic
    #
    # Check the backup and restore functionality using synthetic data.
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'synthetic';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strModule} - ${strThisTest}\n");
        }

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
        for (my $bCompress = false; $bCompress <= true; $bCompress++)
        {
        for (my $bHardLink = false; $bHardLink <= true; $bHardLink++)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!testRun(++$iRun,
                                        "rmt ${bRemote}, cmp ${bCompress}, hardlink ${bHardLink}",
                                        $iThreadMax == 1 ? $strModule : undef,
                                        $iThreadMax == 1 ? $strThisTest: undef,
                                        \$oLogTest)) {next}

            # Create hosts, file object, and config
            my ($oHostDbMaster, $oHostBackup, $oFile) = backupTestSetup(
                $bRemote, true, $oLogTest, {bCompress => $bCompress, bHardLink => $bHardLink, iThreadMax => $iThreadMax});

            # Determine if this is a neutral test, i.e. we only want to do it once for local and once for remote.  Neutral means
            # that options such as compression and hardlinks are disabled
            my $bNeutralTest = !$bCompress && !$bHardLink;

            # Get base time
            my $lTime = time() - 100000;

            # Build the manifest
            my %oManifest;

            $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_VERSION} = BACKREST_VERSION;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_CHECK} = JSON::PP::true;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_COPY} = JSON::PP::true;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS} = $bCompress ? JSON::PP::true : JSON::PP::false;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK} = $bHardLink ? JSON::PP::true : JSON::PP::false;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE} = JSON::PP::false;

            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG} = 201306121;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CONTROL} = 937;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_SYSTEM_ID} = 6156904820763115222;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} = PG_VERSION_93;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_ID} = 1;

            $oManifest{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} =
                $oHostDbMaster->dbBasePath();
            $oManifest{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_TYPE} = MANIFEST_VALUE_PATH;

            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA);

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_PGVERSION, PG_VERSION_93,
                                                  'e1f7a3a299f62225cba076fc6d3d6e677f303482', $lTime);

            # Create base path
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base');
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/1');

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/1/12000', 'BASE',
                                                  'a3b357a3e395e43fcfb19bb13f3c1b5179279593', $lTime);
            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/1/' . DB_FILE_PGVERSION,
                                                  PG_VERSION_93, 'e1f7a3a299f62225cba076fc6d3d6e677f303482', $lTime, '660');

            if ($bNeutralTest && !$bRemote)
            {
                executeTest('sudo chown 7777 ' . $oHostDbMaster->dbBasePath() . '/base/1/' . DB_FILE_PGVERSION);
                $oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/base/1/' . DB_FILE_PGVERSION}
                          {&MANIFEST_SUBKEY_USER} = INI_FALSE;
            }

            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384');

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASE',
                                                  'a3b357a3e395e43fcfb19bb13f3c1b5179279593', $lTime);
            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/' . DB_FILE_PGVERSION,
                                                  PG_VERSION_93, 'e1f7a3a299f62225cba076fc6d3d6e677f303482', $lTime);

            if ($bNeutralTest && !$bRemote)
            {
                executeTest('sudo chown :7777 ' . $oHostDbMaster->dbBasePath() . '/base/16384/' . DB_FILE_PGVERSION);
                $oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/base/16384/' . DB_FILE_PGVERSION}
                          {&MANIFEST_SUBKEY_GROUP} = INI_FALSE;
            }

            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768');

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768/33000', '33000',
                                                  '7f4c74dc10f61eef43e6ae642606627df1999b34', $lTime);
            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768/' . DB_FILE_PGVERSION,
                                                  PG_VERSION_93, 'e1f7a3a299f62225cba076fc6d3d6e677f303482', $lTime);

            # Create global path
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'global');

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_PGCONTROL, '[replaceme]',
                                                  '56fe5780b8dca9705e0c22032a83828860a21235', $lTime - 100);

            # Copy pg_control
            executeTest('cp ' . testDataPath() . '/pg_control ' .
                        $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);
            utime($lTime - 100, $lTime - 100, $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL)
                or confess &log(ERROR, "unable to set time");
            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGCONTROL}
                      {&MANIFEST_SUBKEY_SIZE} = 8192;

            # Create tablespace path
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGTBLSPC);

            # Backup Info (with no stanzas)
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostDbMaster->info('no stanzas exist');
            $oHostDbMaster->info('no stanzas exist', {strOutput => INFO_OUTPUT_JSON});

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            my $strType = BACKUP_TYPE_FULL;
            my $strOptionalParam = '--manifest-save-threshold=3';
            my $strTestPoint;

            if ($bNeutralTest && $bRemote)
            {
                $strOptionalParam .= ' --protocol-timeout=2 --db-timeout=1';

                if ($iThreadMax > 1)
                {
                    $strTestPoint = TEST_KEEP_ALIVE;
                }
            }

            # Create a file link
            filePathCreate($oHostDbMaster->dbPath() . '/pg_config', undef, undef, true);
            testFileCreate(
                $oHostDbMaster->dbPath() . '/pg_config/postgresql.conf', "listen_addresses = *\n", $lTime - 100);
            testLinkCreate($oHostDbMaster->dbPath() . '/pg_config/postgresql.conf.link', './postgresql.conf');

            $oHostDbMaster->manifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'postgresql.conf',
                                                  '../pg_config/postgresql.conf');

            # This link will cause errors because it points to the same location as above
            $oHostDbMaster->manifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_config_bad',
                                                  '../../db/pg_config');

            my $strFullBackup = $oHostBackup->backup(
                $strType, 'error on identical link destinations',
                {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail',
                    iExpectedExitStatus => ERROR_LINK_DESTINATION});

            # Remove failing link
            $oHostDbMaster->manifestLinkRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_config_bad');

            # This link will fail because it points to a link
            $oHostDbMaster->manifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'postgresql.conf.bad',
                                                  '../pg_config/postgresql.conf.link');

            # Fail bacause two links point to the same place
            $strFullBackup = $oHostBackup->backup(
                $strType, 'error on link to a link',
                {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail',
                    iExpectedExitStatus => ERROR_LINK_DESTINATION});

            # Remove failing link
            $oHostDbMaster->manifestLinkRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'postgresql.conf.bad');

            # Create stat directory link and file
            filePathCreate($oHostDbMaster->dbPath() . '/pg_stat', undef, undef, true);
            $oHostDbMaster->manifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_stat', '../pg_stat');
            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA . '/pg_stat', 'global.stat', 'stats',
                                                  'e350d5ce0153f3e22d5db21cf2a4eff00f3ee877', $lTime - 100);
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_clog');

            $strFullBackup = $oHostBackup->backup(
                $strType, 'create pg_stat link, pg_clog dir',
                {oExpectedManifest => \%oManifest, strOptionalParam => $strOptionalParam, strTest => $strTestPoint,
                    fTestDelay => 0});

            # Test protocol timeout
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest && $bRemote)
            {
                $oHostBackup->backup(
                    $strType, 'protocol timeout',
                    {oExpectedManifest => \%oManifest, strOptionalParam => '--protocol-timeout=1 --db-timeout=.1',
                     strTest => TEST_BACKUP_START, fTestDelay => 1, iExpectedExitStatus => ERROR_PROTOCOL_TIMEOUT});

                # Remove the aborted backup so the next backup is not a resume
                testPathRemove($oHostBackup->repoPath() . "/temp/${strStanza}.tmp");
            }

            # Stop operations and make sure the correct error occurs
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest)
            {
                # Test a backup abort
                my $oExecuteBackup = $oHostBackup->backupBegin(
                    $strType, 'abort backup - local',
                    {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_START, fTestDelay => 5,
                        iExpectedExitStatus => ERROR_TERM});

                $oHostDbMaster->stop({bForce => true});

                $oHostBackup->backupEnd($strType, $oExecuteBackup, {oExpectedManifest => \%oManifest});

                # Test global stop
                $oHostBackup->backup(
                    $strType, 'global stop',
                    {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_STOP});

                # Test stanza stop
                $oHostDbMaster->stop({strStanza => $oHostDbMaster->stanza()});

                # This time a warning should be generated
                $oHostDbMaster->stop({strStanza => $oHostDbMaster->stanza()});

                $oHostBackup->backup(
                    $strType, 'stanza stop',
                    {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_STOP});

                $oHostDbMaster->start({strStanza => $strStanza});
                $oHostDbMaster->start();

                # This time a warning should be generated
                $oHostDbMaster->start();

                # If the backup is remote then test remote stops
                if ($bRemote)
                {
                    my $oExecuteBackup = $oHostBackup->backupBegin(
                        $strType, 'abort backup - remote',
                        {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_START, fTestDelay => 5,
                            iExpectedExitStatus => ERROR_TERM});

                    $oHostBackup->stop({bForce => true});

                    $oHostBackup->backupEnd($strType, $oExecuteBackup, {oExpectedManifest => \%oManifest});

                    $oHostBackup->backup(
                        $strType, 'global stop',
                        {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_STOP});

                    $oHostBackup->start();
                }
            }

            # Cleanup any garbage left in the temp backup path
            executeTest(
                'sudo rm -rf ' . $oHostBackup->repoPath() . "/temp/${strStanza}.tmp", {bRemote => $bRemote});

            # Resume Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;

            # These files should never be backed up (this requires the next backup to do --force)
            testFileCreate($oHostDbMaster->dbBasePath() . '/' . DB_FILE_POSTMASTERPID, 'JUNK');
            testFileCreate($oHostDbMaster->dbBasePath() . '/' . DB_FILE_BACKUPLABELOLD, 'JUNK');
            testFileCreate($oHostDbMaster->dbBasePath() . '/' . DB_FILE_RECOVERYCONF, 'JUNK');
            testFileCreate($oHostDbMaster->dbBasePath() . '/' . DB_FILE_RECOVERYDONE, 'JUNK');

            # Create files in root tblspc paths that should not be copied or deleted.
            # This will be checked later after a --force restore.
            my $strDoNotDeleteFile = $oHostDbMaster->tablespacePath(1, 2) . '/donotdelete.txt';
            filePathCreate(dirname($strDoNotDeleteFile), undef, undef, true);
            testFileCreate($strDoNotDeleteFile, 'DONOTDELETE-1-2');

            filePathCreate($oHostDbMaster->tablespacePath(1), undef, undef, true);
            testFileCreate($oHostDbMaster->tablespacePath(1) . '/donotdelete.txt', 'DONOTDELETE-1');
            filePathCreate($oHostDbMaster->tablespacePath(2), undef, undef, true);
            testFileCreate($oHostDbMaster->tablespacePath(2) . '/donotdelete.txt', 'DONOTDELETE-2');
            filePathCreate($oHostDbMaster->tablespacePath(2, 2), undef, undef, true);
            testFileCreate($oHostDbMaster->tablespacePath(2, 2) . '/donotdelete.txt', 'DONOTDELETE-2-2');

            my $strTmpPath = $oHostBackup->repoPath() . "/temp/${strStanza}.tmp";
            executeTest("sudo chmod g+w " . dirname($strTmpPath));

            testPathMove($oHostBackup->repoPath() . "/backup/${strStanza}/${strFullBackup}", $strTmpPath);

            my $oMungeManifest = new pgBackRest::Manifest("$strTmpPath/backup.manifest");
            $oMungeManifest->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGVERSION, 'checksum');
            $oMungeManifest->save();

            # Create a temp file in backup temp root to be sure it's deleted correctly
            executeTest("touch ${strTmpPath}/file.tmp" . ($bCompress ? '.gz' : ''),
                        {bRemote => $bRemote});
            executeTest("sudo chmod -R g+w " . dirname($strTmpPath));

            $strFullBackup = $oHostBackup->backup(
                $strType, 'resume',
                {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_RESUME, strOptionalParam => '--force'});

            # Remove postmaster.pid so restore will succeed (the rest will be cleaned up)
            testFileRemove($oHostDbMaster->dbBasePath() . '/' . DB_FILE_POSTMASTERPID);

            # Misconfigure repo-path and check errors
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest)
            {
                $oHostBackup->backup(
                    $strType, 'invalid repo',
                    {oExpectedManifest => \%oManifest, strOptionalParam => '--' . OPTION_REPO_PATH . '=/bogus_path' .
                     '  --log-level-console=detail', iExpectedExitStatus => ERROR_PATH_MISSING});
            }

            # Restore - tests various mode, extra files/paths, missing files/paths
            #-----------------------------------------------------------------------------------------------------------------------
            my $bDelta = true;
            my $bForce = false;

            # Munge permissions/modes on files that will be fixed by the restore
            if ($bNeutralTest && !$bRemote)
            {
                executeTest("sudo chown :7777 " . $oHostDbMaster->dbBasePath() . '/base/1/' . DB_FILE_PGVERSION);
                executeTest("sudo chmod 600 " . $oHostDbMaster->dbBasePath() . '/base/1/' . DB_FILE_PGVERSION);
            }

            # Create a path and file that are not in the manifest
            $oHostDbMaster->dbPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'deleteme');
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'deleteme/deleteme.txt', 'DELETEME');

            # Change path mode
            $oHostDbMaster->dbPathMode(\%oManifest, MANIFEST_TARGET_PGDATA, 'base', '0777');

            # Remove a path
            $oHostDbMaster->dbPathRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_clog');

            # Remove a file
            $oHostDbMaster->dbFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000');

            # Restore will reset invalid user and group so do the same in the manifest
            if ($bNeutralTest && !$bRemote)
            {
                delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/base/1/' . DB_FILE_PGVERSION}
                       {&MANIFEST_SUBKEY_USER});
                delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/base/16384/' . DB_FILE_PGVERSION}
                       {&MANIFEST_SUBKEY_GROUP});
            }

            $oHostDbMaster->restore(
                $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'add and delete files', undef, ' --link-all', undef, $bNeutralTest && !$bRemote ? 'root' : undef);

            # Fix permissions on the restore log
            if ($bNeutralTest && !$bRemote)
            {
                executeTest('sudo chown -R vagrant:postgres ' . $oHostBackup->logPath());
            }

            # Change an existing link to the wrong directory
            $oHostDbMaster->dbFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_stat');
            $oHostDbMaster->dbLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_stat', '../wrong');

            $oHostDbMaster->restore(
                $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'fix broken symlink', undef, ' --link-all --log-level-console=detail');

            # Additional restore tests that don't need to be performed for every permutation
            if ($bNeutralTest && !$bRemote)
            {
                # This time manually restore all links
                $oHostDbMaster->restore(
                    $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                    'restore all links by mapping', undef, '--log-level-console=detail' .
                    ' --link-map=pg_stat=../pg_stat --link-map=postgresql.conf=../pg_config/postgresql.conf');

                # Error when links overlap
                $oHostDbMaster->restore(
                    $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                    'restore all links by mapping', ERROR_LINK_DESTINATION, '--log-level-console=warn' .
                    ' --link-map=pg_stat=../pg_stat --link-map=postgresql.conf=../pg_stat/postgresql.conf');

                # Error when links still exist on non-delta restore
                $bDelta = false;

                executeTest('rm -rf ' . $oHostDbMaster->dbBasePath() . "/*");

                $oHostDbMaster->restore(
                    $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                    'error on existing linked path', ERROR_RESTORE_PATH_NOT_EMPTY, '--log-level-console=warn --link-all');

                executeTest('rm -rf ' . $oHostDbMaster->dbPath() . "/pg_stat/*");

                $oHostDbMaster->restore(
                    $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                    'error on existing linked file', ERROR_RESTORE_PATH_NOT_EMPTY, '--log-level-console=warn --link-all');

                # Now a combination of remapping
                $bDelta = true;

                $oHostDbMaster->restore(
                    $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                    'restore all links --link-all and mapping', undef,
                    '--log-level-console=detail  --link-map=pg_stat=../pg_stat  --link-all');
            }

            # Restore - test errors when $PGDATA cannot be verified
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = true;

            # Remove PG_VERSION
            $oHostDbMaster->dbFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_PGVERSION);

            # Attempt the restore
            $oHostDbMaster->restore(
                $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'fail on missing ' . DB_FILE_PGVERSION, ERROR_RESTORE_PATH_NOT_EMPTY, '--log-level-console=detail');

            # Write a backup.manifest file to make $PGDATA valid
            testFileCreate($oHostDbMaster->dbBasePath() . '/backup.manifest', 'BOGUS');

            # Munge the user to make sure it gets reset on the next run
            $oHostBackup->manifestMunge(
                $strFullBackup, MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_USER, 'bogus');
            $oHostBackup->manifestMunge(
                $strFullBackup, MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_GROUP, 'bogus');

            # Restore succeeds
            $oHostDbMaster->manifestLinkMap(\%oManifest, MANIFEST_TARGET_PGDATA . '/pg_stat');
            $oHostDbMaster->manifestLinkMap(\%oManifest, MANIFEST_TARGET_PGDATA . '/postgresql.conf');

            $oHostDbMaster->restore(
                $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'restore succeeds with backup.manifest file', undef, '--log-level-console=detail');

            # Various broken info tests
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;
            $oHostDbMaster->manifestReference(\%oManifest, $strFullBackup);

            my $strInfoFile = $oHostBackup->repoPath() . "/backup/${strStanza}/backup.info";
            executeTest("sudo chmod 660 $strInfoFile");
            my %oInfo;
            iniLoad($strInfoFile, \%oInfo);

            # Break the database version
            my $strDbVersion = $oInfo{'db'}{&MANIFEST_KEY_DB_VERSION};

            $oInfo{db}{&MANIFEST_KEY_DB_VERSION} = '8.0';
            testIniSave($strInfoFile, \%oInfo, true);

            $oHostBackup->backup(
                $strType, 'invalid database version',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                    strOptionalParam => '--log-level-console=detail'});
            $oInfo{db}{&MANIFEST_KEY_DB_VERSION} = $strDbVersion;

            # Break the database system id
            my $ullDbSysId = $oInfo{'db'}{&MANIFEST_KEY_SYSTEM_ID};
            $oInfo{db}{&MANIFEST_KEY_SYSTEM_ID} = 6999999999999999999;
            testIniSave($strInfoFile, \%oInfo, true);

            $oHostBackup->backup(
                $strType, 'invalid system id',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                    strOptionalParam => '--log-level-console=detail'});
            $oInfo{db}{&MANIFEST_KEY_SYSTEM_ID} = $ullDbSysId;

            # Break the control version
            my $iControlVersion = $oInfo{'db'}{&MANIFEST_KEY_CONTROL};
            $oInfo{db}{&MANIFEST_KEY_CONTROL} = 842;
            testIniSave($strInfoFile, \%oInfo, true);

            $oHostBackup->backup(
                $strType, 'invalid control version',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                    strOptionalParam => '--log-level-console=detail'});
            $oInfo{db}{&MANIFEST_KEY_CONTROL} = $iControlVersion;

            # Break the catalog version
            my $iCatalogVersion = $oInfo{'db'}{&MANIFEST_KEY_CATALOG};
            $oInfo{db}{&MANIFEST_KEY_CATALOG} = 197208141;
            testIniSave($strInfoFile, \%oInfo, true);

            $oHostBackup->backup(
                $strType, 'invalid catalog version',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                    strOptionalParam => '--log-level-console=detail'});

            # Fix up info file for next test
            $oInfo{db}{&MANIFEST_KEY_CATALOG} = $iCatalogVersion;
            testIniSave($strInfoFile, \%oInfo, true);

            # Test broken tablespace configuration
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;
            my $strTblSpcPath = $oHostDbMaster->dbBasePath() . '/' . DB_PATH_PGTBLSPC;

            # Create a directory in pg_tablespace
            filePathCreate("${strTblSpcPath}/path");

            $oHostBackup->backup(
                $strType, 'invalid path in ' . DB_PATH_PGTBLSPC,
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_LINK_EXPECTED,
                 strOptionalParam => '--log-level-console=detail'});

            testPathRemove("${strTblSpcPath}/path");

            # Create a relative link in PGDATA
            testLinkCreate("${strTblSpcPath}/99999", '../invalid_tblspc');

            $oHostBackup->backup(
                $strType, 'invalid relative tablespace in $PGDATA',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                    strOptionalParam => '--log-level-console=detail'});

            testFileRemove("${strTblSpcPath}/99999");

            # Create tablespace in PGDATA
            filePathCreate($oHostDbMaster->dbBasePath() . '/invalid_tblspc');
            testLinkCreate("${strTblSpcPath}/99999", $oHostDbMaster->dbBasePath() . '/invalid_tblspc');

            $oHostBackup->backup(
                $strType, 'invalid tablespace in $PGDATA',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                    strOptionalParam => '--log-level-console=detail'});

            testPathRemove($oHostDbMaster->dbBasePath() . '/invalid_tblspc');
            testFileRemove("${strTblSpcPath}/99999");

            # Incr backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            # Add tablespace 1
            $oHostDbMaster->manifestTablespaceCreate(\%oManifest, 1);
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/1', '16384');

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/1', '16384/tablespace1.txt', 'TBLSPC1',
                                                  'd85de07d6421d90aa9191c11c889bfde43680f0f', $lTime);
            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'badchecksum.txt', 'BADCHECKSUM',
                                                  'f927212cd08d11a42a666b2f04235398e9ceeb51', $lTime);

            my $strBackup = $oHostBackup->backup(
                $strType, 'add tablespace 1', {oExpectedManifest => \%oManifest, strOptionalParam => '--test'});

            # Resume Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            # Move database from backup to temp
            $strTmpPath = $oHostBackup->repoPath() . "/temp/${strStanza}.tmp";

            testPathMove($oHostBackup->repoPath() . "/backup/${strStanza}/${strBackup}", $strTmpPath);
            executeTest("sudo chmod -R g+w " . dirname($strTmpPath));

            $oMungeManifest = new pgBackRest::Manifest("$strTmpPath/" . FILE_MANIFEST);
            $oMungeManifest->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/badchecksum.txt', 'checksum', 'bogus');
            $oMungeManifest->save();

            # Add tablespace 2
            $oHostDbMaster->manifestTablespaceCreate(\%oManifest, 2);
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768');

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2.txt', 'TBLSPC2',
                                                  'dc7f76e43c46101b47acc55ae4d593a9e6983578', $lTime);


            $strBackup = $oHostBackup->backup(
                $strType, 'resume and add tablespace 2', {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_RESUME});

            # Resume Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_DIFF;

            $strTmpPath = $oHostBackup->repoPath() . "/temp/${strStanza}.tmp";

            testPathMove($oHostBackup->repoPath() . "/backup/${strStanza}/${strBackup}", $strTmpPath);
            executeTest("sudo chmod -R g+w " . dirname($strTmpPath));

            $strBackup = $oHostBackup->backup(
                $strType, 'cannot resume - new diff',
                {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_NORESUME,
                    strOptionalParam => '--log-level-console=detail'});

            # Resume Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_DIFF;

            $strTmpPath = $oHostBackup->repoPath() . "/temp/${strStanza}.tmp";

            testPathMove($oHostBackup->repoPath() . "/backup/${strStanza}/${strBackup}", $strTmpPath);
            executeTest("sudo chmod -R g+w " . dirname($strTmpPath));

            $strBackup = $oHostBackup->backup(
                $strType, 'cannot resume - disabled',
                {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_NORESUME,
                    strOptionalParam => '--no-resume --log-level-console=detail'});

            # Restore
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = false;

            # Fail on used path
            $oHostDbMaster->restore(
                $strBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'fail on used path', ERROR_RESTORE_PATH_NOT_EMPTY, '--log-level-console=detail');

            # Fail on undef format
            $oHostBackup->manifestMunge($strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT);

            $oHostDbMaster->restore(
                $strBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'fail on undef format', ERROR_FORMAT, '--log-level-console=detail');

            # Fail on mismatch format
            $oHostBackup->manifestMunge($strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, 0);

            $oHostDbMaster->restore(
                $strBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'fail on mismatch format', ERROR_FORMAT, '--log-level-console=detail');

            $oHostBackup->manifestMunge($strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, BACKREST_FORMAT);

            # Remap the base and tablespace paths
            my %oRemapHash;
            $oRemapHash{&MANIFEST_TARGET_PGDATA} = $oHostDbMaster->dbBasePath(2);
            filePathCreate($oHostDbMaster->dbBasePath(2));
            $oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/1'} = $oHostDbMaster->tablespacePath(1, 2);
            $oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/2'} = $oHostDbMaster->tablespacePath(2, 2);

            $oHostDbMaster->restore(
                $strBackup, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'remap all paths', undef, '--log-level-console=detail');

            # Restore (make sure file in root tablespace path is not deleted by --delta)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;

            $oHostDbMaster->restore(
                $strBackup, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'ensure file in tblspc root remains after --delta', undef, '--log-level-console=detail');

            if (!-e $strDoNotDeleteFile)
            {
                confess "${strDoNotDeleteFile} was deleted by --delta";
            }

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;
            $oHostDbMaster->manifestReference(\%oManifest, $strBackup);

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', 'BASE2',
                                                  '09b5e31766be1dba1ec27de82f975c1b6eea2a92', $lTime);

            $oHostDbMaster->manifestTablespaceDrop(\%oManifest, 1, 2);

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2b.txt',
                                                  'TBLSPC2B', 'e324463005236d83e6e54795dbddd20a74533bf3', $lTime);

            # Munge the version to make sure it gets corrected on the next run
            $oHostBackup->manifestMunge($strBackup, INI_SECTION_BACKREST, INI_KEY_VERSION, undef, '0.00');

            $strBackup = $oHostBackup->backup(
                $strType, 'add files and remove tablespace 2',
                {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail'});

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;
            $oHostDbMaster->manifestReference(\%oManifest, $strBackup);

            # Delete the backup.info and make sure it gets reconstructed correctly
            if ($bNeutralTest)
            {
                executeTest('sudo rm ' . $oHostBackup->repoPath() . "/backup/${strStanza}/backup.info");
            }

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASEUPDT',
                                                  '9a53d532e27785e681766c98516a5e93f096a501', $lTime);

            $strBackup =$oHostBackup->backup(
                $strType, 'update files',
                {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail'});

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_DIFF;
            $oHostDbMaster->manifestReference(\%oManifest, $strFullBackup, true);

            $strBackup = $oHostBackup->backup(
                $strType, 'updates since last full',
                {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail'});

            # Incr Backup
            #
            # Remove a file from the db after the manifest has been built but before files are copied.  The file will not be shown
            # as removed in the log because it had not changed since the last backup so it will only be referenced.
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;
            $oHostDbMaster->manifestReference(\%oManifest, $strBackup);

            my $oBackupExecute = $oHostBackup->backupBegin(
                $strType, 'remove files - but won\'t affect manifest',
                {oExpectedManifest => \%oManifest, strTest => TEST_MANIFEST_BUILD, fTestDelay => 1,
                    strOptionalParam => '--log-level-console=detail'});

            $oHostDbMaster->dbFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000');

            $strBackup = $oHostBackup->backupEnd($strType, $oBackupExecute, {oExpectedManifest => \%oManifest});

            # Diff Backup
            #
            # Remove base2.txt and changed tablespace2c.txt during the backup.  The removed file should be logged and the changed
            # file should have the new, larger size logged and in the manifest.
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostDbMaster->manifestReference(\%oManifest, $strFullBackup, true);

            $strType = BACKUP_TYPE_DIFF;

            $oHostDbMaster->manifestFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000');

            $oHostDbMaster->manifestFileRemove(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2b.txt', true);
            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2c.txt', 'TBLSPC2C',
                                                  'ad7df329ab97a1e7d35f1ff0351c079319121836', $lTime);

            $oBackupExecute = $oHostBackup->backupBegin(
                $strType, 'remove files during backup',
                {oExpectedManifest => \%oManifest, strTest => TEST_MANIFEST_BUILD, fTestDelay => 1,
                    strOptionalParam => '--log-level-console=detail'});

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2c.txt',
                                                  'TBLSPCBIGGER', 'dfcb8679956b734706cf87259d50c88f83e80e66', $lTime);

            $oHostDbMaster->manifestFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', true);

            $strBackup = $oHostBackup->backupEnd($strType, $oBackupExecute, {oExpectedManifest => \%oManifest});

            # Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;

            $oHostDbMaster->manifestReference(\%oManifest);

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASEUPDT2',
                                                  '7579ada0808d7f98087a0a586d0df9de009cdc33', $lTime);

            $strFullBackup = $oHostBackup->backup(
                $strType, 'update file',
                {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail'});

            # Backup Info
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostDbMaster->info('normal output', {strStanza => $oHostDbMaster->stanza()});
            $oHostBackup->info('normal output', {strStanza => $oHostBackup->stanza(), strOutput => INFO_OUTPUT_JSON});

            # Call expire
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bRemote && !$bCompress)
            {
                $oHostBackup->expire({iRetentionFull => 1});
            }
            else
            {
                $oHostDbMaster->expire(
                    {iRetentionFull => 1, iExpectedExitStatus => $bRemote && $bCompress ? ERROR_HOST_INVALID : undef});
            }

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_DIFF;

            $oHostDbMaster->manifestReference(\%oManifest, $strFullBackup);

            $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', 'BASE2UPDT',
                                                  'cafac3c59553f2cfde41ce2e62e7662295f108c0', $lTime);

            $strBackup = $oHostBackup->backup(
                $strType, 'add file', {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail'});

            # Selective Restore
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;

            # Remove mapping for tablespace 1
            delete($oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/1'});

            # Remove checksum to match zeroed files
            delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33000'}{&MANIFEST_SUBKEY_CHECKSUM});
            delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.3_201306121/32768/tablespace2.txt'}
                             {&MANIFEST_SUBKEY_CHECKSUM});
            delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.3_201306121/32768/tablespace2c.txt'}
                             {&MANIFEST_SUBKEY_CHECKSUM});

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'selective restore 16384', undef, '--log-level-console=detail --db-include=16384');

            # Restore checksum values for next test
            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33000'}{&MANIFEST_SUBKEY_CHECKSUM} =
                '7f4c74dc10f61eef43e6ae642606627df1999b34';
            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.3_201306121/32768/tablespace2.txt'}
                      {&MANIFEST_SUBKEY_CHECKSUM} = 'dc7f76e43c46101b47acc55ae4d593a9e6983578';
            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.3_201306121/32768/tablespace2c.txt'}
                      {&MANIFEST_SUBKEY_CHECKSUM} = 'dfcb8679956b734706cf87259d50c88f83e80e66';

            # Remove chacksum to match zeroed file
            delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/16384/17000'}{&MANIFEST_SUBKEY_CHECKSUM});

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'selective restore 32768', undef, '--log-level-console=detail --db-include=32768');

            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/16384/17000'}{&MANIFEST_SUBKEY_CHECKSUM} =
                '7579ada0808d7f98087a0a586d0df9de009cdc33';

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, \%oManifest, \%oRemapHash,
                $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'error on invalid id', ERROR_DB_MISSING, '--log-level-console=warn --db-include=7777');

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'error on system id', ERROR_DB_INVALID, '--log-level-console=warn --db-include=1');

            # Compact Restore
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;

            executeTest('rm -rf ' . $oHostDbMaster->dbBasePath(2) . "/*");

            my $strDbPath = $oHostDbMaster->dbBasePath(2) . '/base';
            filePathCreate($strDbPath);

            $oRemapHash{&MANIFEST_TARGET_PGDATA} = $strDbPath;
            delete($oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/2'});

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'no tablespace remap - error when tablespace dir does not exist', ERROR_PATH_MISSING,
                '--log-level-console=detail --tablespace-map-all=../../tablespace', false);

            filePathCreate($oHostDbMaster->dbBasePath(2) . '/tablespace');

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'no tablespace remap', undef, '--tablespace-map-all=../../tablespace --log-level-console=detail', false);

            # Backup Info (with an empty stanza)
            #-----------------------------------------------------------------------------------------------------------------------
            executeTest('sudo chmod g+w ' . $oHostBackup->repoPath() . '/backup');
            filePathCreate($oHostBackup->repoPath() . '/backup/db_empty', '0770');

            $oHostBackup->info('normal output');
            $oHostDbMaster->info('normal output', {strOutput => INFO_OUTPUT_JSON});
            $oHostBackup->info('bogus stanza', {strStanza => 'bogus'});
            $oHostDbMaster->info('bogus stanza', {strStanza => 'bogus', strOutput => INFO_OUTPUT_JSON});

            # Dump out history path at the end to verify all history files are being recorded.  This test is only performed locally
            # because for some reason sort order is different when this command is executed via ssh (even though the content of the
            # directory is identical).
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest && !$bRemote)
            {
                executeTest('ls -1R ' . $oHostBackup->repoPath() . "/backup/${strStanza}/" . PATH_BACKUP_HISTORY,
                            {oLogTest => $oLogTest, bRemote => $bRemote});
            }

            # Test protocol shutdown timeout
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest && $bRemote)
            {
                $oHostBackup->backup(
                    $strType, 'protocol shutdown timeout',
                    {oExpectedManifest => \%oManifest,
                     strOptionalParam => '--protocol-timeout=1 --db-timeout=.5 --log-level-console=warn',
                     strTest => TEST_PROCESS_EXIT, fTestDelay => 1, bSupplemental => false});
            }
        }
        }
        }

        testCleanup(\$oLogTest);
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test full
    #
    # Check the entire backup mechanism using actual clusters.  Only the archive and start/stop mechanisms need to be tested since
    # everything else was tested in the backup test.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'full')
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test Full Backup\n");
        }

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
        for (my $bArchiveAsync = false; $bArchiveAsync <= true; $bArchiveAsync++)
        {
        for (my $bCompress = false; $bCompress <= true; $bCompress++)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!testRun(++$iRun, "rmt ${bRemote}, arc_async ${bArchiveAsync}, cmp ${bCompress}")) {next}

            # Create hosts, file object, and config
            my ($oHostDbMaster, $oHostBackup, $oFile) = backupTestSetup(
                $bRemote, false, undef,
                {bCompress => $bCompress, iThreadMax => $iThreadMax, bArchiveAsync => $bArchiveAsync});

            # For the 'fail on missing archive.info file' test, the archive.info file must not be found so set archive invalid.
            $oHostDbMaster->clusterCreate({bArchiveInvalid => true});

            # Static backup parameters
            my $fTestDelay = 1;

            # Variable backup parameters
            my $bDelta = true;
            my $bForce = false;
            my $strType = undef;
            my $strTarget = undef;
            my $bTargetExclusive = false;
            my $strTargetAction;
            my $strTargetTimeline = undef;
            my $oRecoveryHashRef = undef;
            my $strComment = undef;
            my $iExpectedExitStatus = undef;

            # Restore test string
            my $strDefaultMessage = 'default';
            my $strFullMessage = 'full';
            my $strIncrMessage = 'incr';
            my $strTimeMessage = 'time';
            my $strXidMessage = 'xid';
            my $strNameMessage = 'name';
            my $strTimelineMessage = 'timeline3';

            # Create two new databases
            $oHostDbMaster->sqlExecute('create database test1', {bAutoCommit => true});
            $oHostDbMaster->sqlExecute('create database test2', {bAutoCommit => true});

            # Test invalid archive command
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;

            # NOTE: This must run before the success test since that will create the archive.info file
            $oHostDbMaster->check(
                'fail on missing archive.info file',
                {iTimeout => 0.1, iExpectedExitStatus => ERROR_FILE_MISSING});

            # Stop the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterStop({bIgnoreLogError => true});

            # Check ERROR_ARCHIVE_COMMAND_INVALID error
            $strComment = 'fail on invalid archive_command';
            $oHostDbMaster->clusterStart({bArchive => false});

            $oHostBackup->backup($strType, $strComment, {iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});

            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});

            # If running the remote tests then also need to run check locally
            if ($bRemote)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});
            }

            # When archive-check=n then ERROR_FILE_MISSING will be raised instead of ERROR_ARCHIVE_COMMAND_INVALID
            $strComment = 'fail on file missing when archive-check=n';
            $oHostDbMaster->check(
                $strComment,
                {iTimeout => 0.1, iExpectedExitStatus => ERROR_FILE_MISSING, strOptionalParam => '--no-archive-check'});

            # Stop the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterStop({bIgnoreLogError => true});

            # Providing a sufficient archive-timeout, verify that the check command runs successfully.
            $strComment = 'verify success';

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->check($strComment, {iTimeout => 5});

            # If running the remote tests then also need to run check locally
            if ($bRemote)
            {
                $oHostBackup->check($strComment, {iTimeout => 5});
            }

            # Check archive_timeout error
            $strComment = 'fail on archive timeout';

            $oHostDbMaster->clusterRestart({bArchiveInvalid => true});
            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_TIMEOUT});

            # If running the remote tests then also need to run check locally
            if ($bRemote)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_TIMEOUT});
            }

            # Restart the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true});

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;

            # Create the table where test messages will be stored
            $oHostDbMaster->sqlExecute("create table test (message text not null)");
            $oHostDbMaster->sqlXlogRotate();
            $oHostDbMaster->sqlExecute("insert into test values ('$strDefaultMessage')");

            # Acquire the backup advisory lock so it looks like a backup is running
            if (!$oHostDbMaster->sqlSelectOne('select pg_try_advisory_lock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to acquire advisory lock for testing';
            }

            $oHostBackup->backup($strType, 'fail on backup lock exists', {iExpectedExitStatus => ERROR_LOCK_ACQUIRE});

            # Release the backup advisory lock so the next backup will succeed
            if (!$oHostDbMaster->sqlSelectOne('select pg_advisory_unlock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to acquire advisory lock for testing';
            }

            my $oExecuteBackup = $oHostBackup->backupBegin(
                $strType, 'update during backup',
                {strTest => TEST_MANIFEST_BUILD, fTestDelay => $fTestDelay});

            $oHostDbMaster->sqlExecute("update test set message = '$strFullMessage'");

            my $strFullBackup = $oHostBackup->backupEnd($strType, $oExecuteBackup);

            # Execute stop and make sure the backup fails
            #-----------------------------------------------------------------------------------------------------------------------
            # Restart the cluster to check for any errors before continuing since the stop tests will definitely create errors and
            # the logs will to be deleted to avoid causing issues further down the line.
            $oHostDbMaster->clusterRestart();

            $oHostDbMaster->stop();

            $oHostBackup->backup($strType, 'attempt backup when stopped', {iExpectedExitStatus => ERROR_STOP});

            $oHostDbMaster->start();

            # Setup the time target
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostDbMaster->sqlExecute("update test set message = '$strTimeMessage'");
            $oHostDbMaster->sqlXlogRotate();
            my $strTimeTarget = $oHostDbMaster->sqlSelectOne("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ')");
            &log(INFO, "        time target is ${strTimeTarget}");

            # Incr backup - fail on archive_mode=always when version >= 9.5
            #-----------------------------------------------------------------------------------------------------------------------
            if ($oHostDbMaster->dbVersion() >= PG_VERSION_95)
            {
                $strType = BACKUP_TYPE_INCR;

                # Set archive_mode=always
                $oHostDbMaster->clusterRestart({bArchiveAlways => true});

                $oHostBackup->backup($strType, 'fail on archive_mode=always', {iExpectedExitStatus => ERROR_FEATURE_NOT_SUPPORTED});

                # Reset the cluster to a normal state so the next test will work
                $oHostDbMaster->clusterRestart();
            }

            # Incr backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            filePathCreate($oHostDbMaster->tablespacePath(1), undef, undef, true);
            $oHostDbMaster->sqlExecute(
                "create tablespace ts1 location '" . $oHostDbMaster->tablespacePath(1) . "'", {bAutoCommit => true});
            $oHostDbMaster->sqlExecute("alter table test set tablespace ts1", {bCheckPoint => true});

            $oHostDbMaster->sqlExecute("create table test_remove (id int)");
            $oHostDbMaster->sqlXlogRotate();
            $oHostDbMaster->sqlExecute("update test set message = '$strDefaultMessage'");
            $oHostDbMaster->sqlXlogRotate();

            # Start a backup so the next backup has to restart it.  This test is not required for PostgreSQL >= 9.6 since backups
            # are run in non-exlusive mode.
            if ($oHostDbMaster->dbVersion() >= PG_VERSION_93 && $oHostDbMaster->dbVersion() < PG_VERSION_96)
            {
                $oHostDbMaster->sqlSelectOne("select pg_start_backup('test backup that will be cancelled', true)");

                # Verify that an error is returned if the backup is already running
                $oHostBackup->backup($strType, 'fail on backup already running', {iExpectedExitStatus => ERROR_DB_QUERY});

                # Restart the cluster ignoring any errors in the postgresql log
                $oHostDbMaster->clusterRestart({bIgnoreLogError => true});
            }

            $oExecuteBackup = $oHostBackup->backupBegin(
                $strType, 'update during backup',
                {strTest => TEST_MANIFEST_BUILD, fTestDelay => $fTestDelay,
                    strOptionalParam => '--' . OPTION_STOP_AUTO . ' --no-' . OPTION_BACKUP_ARCHIVE_CHECK});

            $oHostDbMaster->sqlExecute('drop table test_remove');
            $oHostDbMaster->sqlXlogRotate();
            $oHostDbMaster->sqlExecute("update test set message = '$strIncrMessage'", {bCommit => true});

            my $strIncrBackup = $oHostBackup->backupEnd($strType, $oExecuteBackup);

            # Setup the xid target
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostDbMaster->sqlExecute("update test set message = '$strXidMessage'", {bCommit => false});
            $oHostDbMaster->sqlXlogRotate();
            my $strXidTarget = $oHostDbMaster->sqlSelectOne("select txid_current()");
            $oHostDbMaster->sqlCommit();
            &log(INFO, "        xid target is ${strXidTarget}");

            # Setup the name target
            #-----------------------------------------------------------------------------------------------------------------------
            my $strNameTarget = 'backrest';

            $oHostDbMaster->sqlExecute("update test set message = '$strNameMessage'", {bCommit => true});
            $oHostDbMaster->sqlXlogRotate();

            if ($oHostDbMaster->dbVersion() >= PG_VERSION_91)
            {
                $oHostDbMaster->sqlExecute("select pg_create_restore_point('${strNameTarget}')");
            }

            &log(INFO, "        name target is ${strNameTarget}");

            # Create a table and data in database test2
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostDbMaster->sqlExecute(
                'create table test (id int);' .
                'insert into test values (1);' .
                'create table test_ts1 (id int) tablespace ts1;' .
                'insert into test_ts1 values (2);',
                {strDb => 'test2', bAutoCommit => true});
            $oHostDbMaster->sqlXlogRotate();

            # Restore (type = default)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = false;
            $strType = RECOVERY_TYPE_DEFAULT;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $strTargetAction = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            # Expect failure because postmaster.pid exists
            $strComment = 'postmaster running';
            $iExpectedExitStatus = ERROR_POSTMASTER_RUNNING;

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            $oHostDbMaster->clusterStop();

            # Expect failure because db path is not empty
            $strComment = 'path not empty';
            $iExpectedExitStatus = ERROR_RESTORE_PATH_NOT_EMPTY;

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            # Drop and recreate db path
            testPathRemove($oHostDbMaster->dbBasePath());
            filePathCreate($oHostDbMaster->dbBasePath());
            testPathRemove($oHostDbMaster->dbPath() . '/pg_xlog');
            filePathCreate($oHostDbMaster->dbPath() . '/pg_xlog');
            testPathRemove($oHostDbMaster->tablespacePath(1));
            filePathCreate($oHostDbMaster->tablespacePath(1));

            # Now the restore should work
            $strComment = undef;
            $iExpectedExitStatus = undef;

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus, ' --db-include=test1');

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strNameMessage);

            # Now it should be OK to drop database test2
            $oHostDbMaster->sqlExecute('drop database test2', {bAutoCommit => true});

            # The test table lives in ts1 so it needs to be moved or dropped
            if ($oHostDbMaster->dbVersion() >= PG_VERSION_90)
            {
                $oHostDbMaster->sqlExecute('alter table test set tablespace pg_default');
            }
            # Drop for older versions
            else
            {
                $oHostDbMaster->sqlExecute('drop table test');
            }

            # And drop the tablespace
            $oHostDbMaster->sqlExecute("drop tablespace ts1", {bAutoCommit => true});

            # Restore (restore type = immediate, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            if ($oHostDbMaster->dbVersion() >= PG_VERSION_94)
            {
                $bDelta = false;
                $bForce = true;
                $strType = RECOVERY_TYPE_IMMEDIATE;
                $strTarget = undef;
                $bTargetExclusive = undef;
                $strTargetAction = undef;
                $strTargetTimeline = undef;
                $oRecoveryHashRef = undef;
                $strComment = undef;
                $iExpectedExitStatus = undef;

                &log(INFO, "    testing recovery type = ${strType}");

                $oHostDbMaster->clusterStop();

                $oHostDbMaster->restore(
                    $strFullBackup, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive, $strTargetAction,
                    $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus, undef);

                $oHostDbMaster->clusterStart();
                $oHostDbMaster->sqlSelectOneTest('select message from test', $strFullMessage);
            }

            # Restore (restore type = xid, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = true;
            $strType = RECOVERY_TYPE_XID;
            $strTarget = $strXidTarget;
            $bTargetExclusive = undef;
            $strTargetAction = $oHostDbMaster->dbVersion() >= PG_VERSION_91 ? 'promote' : undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            $oHostDbMaster->clusterStop();

            executeTest('rm -rf ' . $oHostDbMaster->dbBasePath() . "/*");
            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . "/pg_xlog/*");

            $oHostDbMaster->restore(
                $strIncrBackup, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive, $strTargetAction,
                $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus, '--tablespace-map-all=../../tablespace',
                false);

            # Save recovery file to test so we can use it in the next test
            $oFile->copy(PATH_ABSOLUTE, $oHostDbMaster->dbBasePath() . '/recovery.conf',
                         PATH_ABSOLUTE, "${strTestPath}/recovery.conf");

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");

            # Restore (restore type = preserve, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = false;
            $strType = RECOVERY_TYPE_PRESERVE;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $strTargetAction = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            $oHostDbMaster->clusterStop();

            executeTest('rm -rf ' . $oHostDbMaster->dbBasePath() . "/*");
            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . "/pg_xlog/*");
            executeTest('rm -rf ' . $oHostDbMaster->tablespacePath(1) . "/*");

            # Restore recovery file that was saved in last test
            $oFile->move(PATH_ABSOLUTE, "${strTestPath}/recovery.conf",
                         PATH_ABSOLUTE, $oHostDbMaster->dbBasePath() . '/recovery.conf');

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");

            # Restore (restore type = time, inclusive) - there is no exclusive time test because I can't find a way to find the
            # exact commit time of a transaction.
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_TIME;
            $strTarget = $strTimeTarget;
            $bTargetExclusive = undef;
            $strTargetAction = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            $oHostDbMaster->clusterStop();

            $oHostDbMaster->restore(
                $strFullBackup, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive, $strTargetAction,
                $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strTimeMessage);

            # Restore (restore type = xid, exclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_XID;
            $strTarget = $strXidTarget;
            $bTargetExclusive = true;
            $strTargetAction = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            $oHostDbMaster->clusterStop();

            $oHostDbMaster->restore(
                $strIncrBackup, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive, $strTargetAction,
                $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strIncrMessage);

            # Restore (restore type = name)
            #-----------------------------------------------------------------------------------------------------------------------
            if ($oHostDbMaster->dbVersion() >= PG_VERSION_91)
            {
                $bDelta = true;
                $bForce = true;
                $strType = RECOVERY_TYPE_NAME;
                $strTarget = $strNameTarget;
                $bTargetExclusive = undef;
                $strTargetAction = undef;
                $strTargetTimeline = undef;
                $oRecoveryHashRef = undef;
                $strComment = undef;
                $iExpectedExitStatus = undef;

                &log(INFO, "    testing recovery type = ${strType}");

                $oHostDbMaster->clusterStop();

                $oHostDbMaster->restore(
                    OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                        $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

                $oHostDbMaster->clusterStart();
                $oHostDbMaster->sqlSelectOneTest('select message from test', $strNameMessage);
            }

            # Restore (restore type = default, timeline = 3)
            #-----------------------------------------------------------------------------------------------------------------------
            if ($oHostDbMaster->dbVersion() >= PG_VERSION_84)
            {
                $bDelta = true;
                $bForce = false;
                $strType = RECOVERY_TYPE_DEFAULT;
                $strTarget = undef;
                $bTargetExclusive = undef;
                $strTargetAction = undef;
                $strTargetTimeline = 4;
                $oRecoveryHashRef = $oHostDbMaster->dbVersion() >= PG_VERSION_90 ? {'standby-mode' => 'on'} : undef;
                $strComment = undef;
                $iExpectedExitStatus = undef;

                &log(INFO, "    testing recovery type = ${strType}");

                $oHostDbMaster->clusterStop();

                $oHostDbMaster->restore(
                    $strIncrBackup, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive, $strTargetAction,
                    $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

                $oHostDbMaster->clusterStart({bHotStandby => true});
                $oHostDbMaster->sqlSelectOneTest('select message from test', $strTimelineMessage, {iTimeout => 120});
            }

            # Incr backup - make sure a --no-online backup fails
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            $oHostBackup->backup(
                $strType, 'fail on --no-' . OPTION_ONLINE,
                {iExpectedExitStatus => ERROR_POSTMASTER_RUNNING, strOptionalParam => '--no-' . OPTION_ONLINE});

            # Incr backup - allow --no-online backup to succeed with --force
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            $oHostBackup->backup(
                $strType, 'succeed on --no-' . OPTION_ONLINE . ' with --' . OPTION_FORCE,
                {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});
        }
        }
        }

        testCleanup();
    }
}

our @EXPORT = qw(backupTestRun);

1;
