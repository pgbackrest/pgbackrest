####################################################################################################################################
# BackupTest.pm - Unit Tests for pgBackRest::Backup and pgBackRest::Restore
####################################################################################################################################
package pgBackRestTest::BackupTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use DBI;
use Exporter qw(import);
use Fcntl ':mode';
use File::Basename;
use File::Copy 'cp';
use File::stat;
use Time::HiRes qw(gettimeofday);

use lib dirname($0) . '/../lib';
use pgBackRest::Archive;
use pgBackRest::ArchiveInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Db;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::RemoteMaster;

use pgBackRestTest::BackupCommonTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::CommonTest;
use pgBackRestTest::ExpireCommonTest;

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
    my $strArchiveTestFile = BackRestTestCommon_DataPathGet() . "/test.archive${iSourceNo}.bin";

    my $strSourceFile = "${strXlogPath}/${strArchiveFile}";

    $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile, # Source file
                 PATH_DB_ABSOLUTE, $strSourceFile,      # Destination file
                 false,                                 # Source is not compressed
                 false,                                 # Destination is not compressed
                 undef, undef, undef,                   # Unused params
                 true);                                 # Create path if it does not exist

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
    my $strArchiveCheck = "9.3-1/${strArchiveFile}-${strArchiveChecksum}";

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

# Push a log to the archive
sub archivePush
{
    my $oLogTest = shift;
    my $oFile = shift;
    my $strXlogPath = shift;
    my $strArchiveTestFile = shift;
    my $iArchiveNo = shift;
    my $iExpectedError = shift;

    my $strSourceFile = "${strXlogPath}/" . uc(sprintf('0000000100000001%08x', $iArchiveNo));

    $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile, # Source file
                 PATH_DB_ABSOLUTE, $strSourceFile,      # Destination file
                 false,                                 # Source is not compressed
                 false,                                 # Destination is not compressed
                 undef, undef, undef,                   # Unused params
                 true);                                 # Create path if it does not exist

    my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                     '/pgbackrest.conf --archive-max-mb=24 --no-fork --stanza=db archive-push' .
                     (defined($iExpectedError) && $iExpectedError == ERROR_HOST_CONNECT ?
                      "  --backup-host=bogus" : '');

    executeTest($strCommand . " ${strSourceFile}",
                {iExpectedExitStatus => $iExpectedError, oLogTest => $oLogTest});
}

####################################################################################################################################
# BackRestTestBackup_Test
####################################################################################################################################
our @EXPORT = qw(BackRestTestBackup_Test);

sub BackRestTestBackup_Test
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
    my $strTestPath = BackRestTestCommon_TestPathGet();
    my $strHost = BackRestTestCommon_HostGet();
    my $strUser = BackRestTestCommon_UserGet();
    my $strUserBackRest = BackRestTestCommon_UserBackRestGet();
    my $strGroup = BackRestTestCommon_GroupGet();

    # Setup test variables
    my $iRun;
    my $strModule = 'backup';
    my $strThisTest;
    my $bCreate;
    my $strStanza = BackRestTestCommon_StanzaGet();
    my $oLogTest;

    my $strArchiveChecksum = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
    my $iArchiveMax = 3;
    my $strXlogPath = BackRestTestCommon_DbCommonPathGet() . '/pg_xlog';
    my $strArchiveTestFile = BackRestTestCommon_DataPathGet() . '/test.archive2.bin';
    my $strArchiveTestFile2 = BackRestTestCommon_DataPathGet() . '/test.archive1.bin';

    # Print test banner
    if (!$bVmOut)
    {
        &log(INFO, 'BACKUP MODULE ******************************************************************');
        &log(INFO, "THREAD-MAX: ${iThreadMax}\n");
    }

    # Drop any existing cluster
    BackRestTestBackup_Drop(true, true);

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remotes
    #-------------------------------------------------------------------------------------------------------------------------------
    BackRestTestBackup_Create(true, false);

    my $oRemote = new pgBackRest::Protocol::RemoteMaster
    (
        BackRestTestCommon_CommandRemoteFullGet(),  # Remote command
        OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
        $strHost,                                   # Host
        $strUserBackRest,                           # User
        PROTOCOL_TIMEOUT_TEST                       # Protocol timeout
    );

    BackRestTestBackup_Drop();

    my $oLocal = new pgBackRest::Protocol::Common
    (
        OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
        PROTOCOL_TIMEOUT_TEST                       # Protocol timeout
    );

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-push
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'archive-push';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;
        $bCreate = true;
        my $oFile;

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
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, " .
                                            "arc_async ${bArchiveAsync}",
                                            $iThreadMax == 1 ? $strModule : undef,
                                            $iThreadMax == 1 ? $strThisTest: undef,
                                            \$oLogTest)) {next}

                # Create the file object
                if ($bCreate)
                {
                    $oFile = (new pgBackRest::File
                    (
                        $strStanza,
                        BackRestTestCommon_RepoPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : $oLocal
                    ))->clone();

                    $bCreate = false;
                }

                # Create the test directory
                BackRestTestBackup_Create($bRemote, false);
                BackRestTestBackup_Init($bRemote, $oFile, true, $oLogTest);

                BackRestTestCommon_ConfigCreate(DB,
                                                ($bRemote ? BACKUP : undef),
                                                $bCompress,
                                                undef,       # checksum
                                                undef,       # hardlink
                                                undef,       # thread-max
                                                $bArchiveAsync,
                                                undef);

                BackRestTestCommon_ConfigCreate(BACKUP,
                                                ($bRemote ? DB : undef));

                my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                                 '/pgbackrest.conf --no-fork --stanza=db archive-push';

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

                            ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 2, $iArchiveNo);
                            executeTest('touch ' . BackRestTestCommon_RepoPathGet() . "/archive/${strStanza}/9.3-1/" .
                                        substr($strArchiveFile, 0, 16) . "/${strArchiveFile}.tmp",
                                        {bRemote => $bRemote});
                        }

                        executeTest($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});

                        if ($iArchive == $iBackup)
                        {
                            # load the archive info file so it can be munged for testing
                            my $strInfoFile = $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE);
                            my %oInfo;
                            BackRestTestCommon_iniLoad($strInfoFile, \%oInfo, $bRemote);
                            my $strDbVersion = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION};
                            my $ullDbSysId = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID};

                            # Break the database version
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = '8.0';
                            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

                            &log(INFO, '        test db version mismatch error');

                            executeTest($strCommand . " ${strSourceFile}",
                                        {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $oLogTest});

                            # Break the system id
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = $strDbVersion;
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID} = '5000900090001855000';
                            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

                            &log(INFO, '        test db system-id mismatch error');

                            executeTest($strCommand . " ${strSourceFile}",
                                        {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, oLogTest => $oLogTest});

                            # Move settings back to original
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID} = $ullDbSysId;
                            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

                            # Fail because the process was killed
                            if ($iBackup == 1 && !$bCompress)
                            {
                                &log(INFO, '        test stop');

                                if ($bArchiveAsync)
                                {
                                    my $oExecArchive = new pgBackRestTest::Common::ExecuteTest(
                                        $strCommand . ' --test --test-delay=5 --test-point=' . lc(TEST_ARCHIVE_PUSH_ASYNC_START) .
                                        "=y ${strSourceFile}",
                                        {oLogTest => $oLogTest, iExpectedExitStatus => ERROR_TERM});
                                    $oExecArchive->begin();
                                    $oExecArchive->end(TEST_ARCHIVE_PUSH_ASYNC_START);

                                    BackRestTestBackup_Stop(undef, undef, true);

                                    $oExecArchive->end();
                                }
                                else
                                {
                                    BackRestTestBackup_Stop($strStanza);
                                }

                                executeTest($strCommand . " ${strSourceFile}",
                                            {oLogTest => $oLogTest, iExpectedExitStatus => ERROR_STOP});


                                BackRestTestBackup_Start($bArchiveAsync ? undef : $strStanza);
                            }

                            # Should succeed because checksum is the same
                            &log(INFO, '        test archive duplicate ok');

                            executeTest($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});

                            # Now it should break on archive duplication (because checksum is different
                            &log(INFO, '        test archive duplicate error');

                            ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 1, $iArchiveNo);
                            executeTest($strCommand . " ${strSourceFile}",
                                        {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $oLogTest});

                            if ($bArchiveAsync)
                            {
                                my $strDuplicateWal =
                                    ($bRemote ? BackRestTestCommon_LocalPathGet() : BackRestTestCommon_RepoPathGet()) .
                                    "/archive/${strStanza}/out/${strArchiveFile}-4518a0fdf41d796760b384a358270d4682589820";

                                unlink ($strDuplicateWal)
                                        or confess "unable to remove duplicate WAL segment created for testing: ${strDuplicateWal}";
                            }

                            # Test .partial archive
                            &log(INFO, '        test .partial archive');
                            ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 2, $iArchiveNo, true);
                            executeTest($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});
                            archiveCheck($oFile, $strArchiveFile, $strArchiveChecksum, $bCompress);

                            # Test .partial archive duplicate
                            &log(INFO, '        test .partial archive duplicate');
                            executeTest($strCommand . " ${strSourceFile}", {oLogTest => $oLogTest});

                            # Test .partial archive with different checksum
                            &log(INFO, '        test .partial archive with different checksum');
                            ($strArchiveFile, $strSourceFile) = archiveGenerate($oFile, $strXlogPath, 1, $iArchiveNo, true);
                            executeTest($strCommand . " ${strSourceFile}",
                                        {iExpectedExitStatus => ERROR_ARCHIVE_DUPLICATE, oLogTest => $oLogTest});

                            if ($bArchiveAsync)
                            {
                                my $strDuplicateWal =
                                    ($bRemote ? BackRestTestCommon_LocalPathGet() : BackRestTestCommon_RepoPathGet()) .
                                    "/archive/${strStanza}/out/${strArchiveFile}-4518a0fdf41d796760b384a358270d4682589820";

                                unlink ($strDuplicateWal)
                                        or confess "unable to remove duplicate WAL segment created for testing: ${strDuplicateWal}";
                            }
                        }

                        archiveCheck($oFile, $strArchiveFile, $strArchiveChecksum, $bCompress);
                    }

                    # Might be nice to add tests for .backup files here (but this is already tested in full backup)
                }

                if (defined($oLogTest))
                {
                    $oLogTest->supplementalAdd($oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/archive.info', $bRemote);
                }
            }
            }


            $bCreate = true;
        }

        if (BackRestTestCommon_Cleanup(\$oLogTest))
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-stop
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'archive-stop';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;
        $bCreate = true;
        my $oFile;

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
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}" .
                                            ', error ' . ($iError ? 'connect' : 'version'),
                                            $iThreadMax == 1 ? $strModule : undef,
                                            $iThreadMax == 1 ? $strThisTest: undef,
                                            \$oLogTest)) {next}

                # Create the file object
                if ($bCreate)
                {
                    $oFile = (new pgBackRest::File
                    (
                        $strStanza,
                        BackRestTestCommon_RepoPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : $oLocal
                    ))->clone();

                    $bCreate = false;
                }

                # Create the test directory
                BackRestTestBackup_Create($bRemote, false);

                BackRestTestCommon_ConfigCreate(DB,
                                                ($bRemote ? BACKUP : undef),
                                                $bCompress,
                                                undef,      # checksum
                                                undef,      # hardlink
                                                undef,      # thread-max
                                                true,       # archive-async
                                                undef);

                BackRestTestCommon_ConfigCreate(BACKUP,
                                                ($bRemote ? DB : undef));

                # Push a WAL segment
                archivePush($oLogTest, $oFile, $strXlogPath, $strArchiveTestFile, 1);

                # load the archive info file so it can be munged for testing
                my $strInfoFile = $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE);
                my %oInfo;
                BackRestTestCommon_iniLoad($strInfoFile, \%oInfo, $bRemote);
                my $strDbVersion = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION};
                my $ullDbSysId = $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID};

                # Break the database version
                if ($iError == 0)
                {
                    $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = '8.0';
                    BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);
                }

                # Push two more segments with errors to exceed archive-max-mb
                archivePush($oLogTest, $oFile, $strXlogPath, $strArchiveTestFile, 2,
                            $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

                archivePush($oLogTest, $oFile, $strXlogPath, $strArchiveTestFile, 3,
                            $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

                # Now this segment will get dropped
                archivePush($oLogTest, $oFile, $strXlogPath, $strArchiveTestFile, 4);

                # Fix the database version
                if ($iError == 0)
                {
                    $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = '9.3';
                    BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);
                }

                # Remove the stop file
                my $strStopFile = ($bRemote ? BackRestTestCommon_LocalPathGet() : BackRestTestCommon_RepoPathGet()) .
                                  '/stop/db-archive.stop';

                unlink($strStopFile)
                    or die "unable to remove stop file ${strStopFile}";

                # Push two more segments - only #4 should be missing from the archive at the end
                archivePush($oLogTest, $oFile, $strXlogPath, $strArchiveTestFile, 5);
                archivePush($oLogTest, $oFile, $strXlogPath, $strArchiveTestFile, 6);
            }
            }

            $bCreate = true;
        }

        if (BackRestTestCommon_Cleanup(\$oLogTest))
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-get
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'archive-get';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;
        $bCreate = true;
        my $oFile;

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
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, exists ${bExists}",
                                            $iThreadMax == 1 ? $strModule : undef,
                                            $iThreadMax == 1 ? $strThisTest: undef,
                                            \$oLogTest)) {next}

                # Create the test directory
                if ($bCreate)
                {
                    # Create the file object
                    $oFile = (pgBackRest::File->new
                    (
                        $strStanza,
                        BackRestTestCommon_RepoPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : $oLocal
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);
                    BackRestTestBackup_Init($bRemote, $oFile, true, $oLogTest);

                    BackRestTestCommon_ConfigCreate(DB,
                                                    ($bRemote ? BACKUP : undef));

                    BackRestTestCommon_ConfigCreate(BACKUP,
                                                    ($bRemote ? DB : undef));

                    # Create the db/common/pg_xlog directory
                    BackRestTestCommon_PathCreate($strXlogPath);

                    $bCreate = false;
                }

                BackRestTestCommon_ConfigCreate('db',                           # local
                                                ($bRemote ? BACKUP : undef),    # remote
                                                $bCompress,                     # compress
                                                undef,                          # checksum
                                                undef,                          # hardlink
                                                undef,                          # thread-max
                                                undef,                          # archive-async
                                                undef);                         # compress-async

                my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                                 '/pgbackrest.conf --stanza=db archive-get';


                executeTest($strCommand . " 000000010000000100000001 ${strXlogPath}/000000010000000100000001",
                            {iExpectedExitStatus => ERROR_FILE_MISSING, oLogTest => $oLogTest});

                # Create the archive info file
                if ($bRemote)
                {
                    executeTest("chmod g+r,g+x " . BackRestTestCommon_RepoPathGet(),
                                {bRemote => true});
                }

                executeTest('mkdir -p -m 770 ' . $oFile->pathGet(PATH_BACKUP_ARCHIVE),
                            {bRemote => $bRemote});
                (new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE)))->check('9.3', 1234567890123456789);

                if (defined($oLogTest))
                {
                    $oLogTest->supplementalAdd($oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/archive.info');
                }

                if ($bRemote)
                {
                    executeTest("chmod g-r,g-x " . BackRestTestCommon_RepoPathGet(),
                                {bRemote => true});
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

                        $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile,         # Source file
                                     PATH_BACKUP_ARCHIVE, "9.3-1/${strSourceFile}", # Destination file
                                     false,                                         # Source is not compressed
                                     $bCompress,                                    # Destination compress based on test
                                     undef, undef, undef,                           # Unused params
                                     true);                                         # Create path if it does not exist

                        my $strDestinationFile = "${strXlogPath}/${strArchiveFile}";

                        executeTest($strCommand . " ${strArchiveFile} ${strDestinationFile}", {oLogTest => $oLogTest});

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
                    BackRestTestBackup_Stop();

                    executeTest($strCommand . " 000000090000000900000009 ${strXlogPath}/RECOVERYXLOG",
                                {iExpectedExitStatus => ERROR_STOP, oLogTest => $oLogTest});

                    BackRestTestBackup_Start();

                    executeTest($strCommand . " 000000090000000900000009 ${strXlogPath}/RECOVERYXLOG",
                                {iExpectedExitStatus => 1, oLogTest => $oLogTest});
                }

                $bCreate = true;
            }
            }
        }

        if (BackRestTestCommon_Cleanup(\$oLogTest))
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test expire
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'expire';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;
        my $oFile;

        if (!$bVmOut)
        {
            &log(INFO, "Test ${strThisTest}\n");
        }

        if (BackRestTestCommon_Run(++$iRun,
                                    "local",
                                    $iThreadMax == 1 ? $strModule : undef,
                                    $iThreadMax == 1 ? $strThisTest: undef,
                                    \$oLogTest))
        {
            # Create the file object
            $oFile = (pgBackRest::File->new
            (
                $strStanza,
                BackRestTestCommon_RepoPathGet(),
                'db',
                $oLocal
            ))->clone();

            # Create the repo
            BackRestTestCommon_Create();
            BackRestTestCommon_CreateRepo();

            # Create backup config
            BackRestTestCommon_ConfigCreate('backup',       # local
                                            undef,          # remote
                                            false,          # compress
                                            undef,          # checksum
                                            undef,          # hardlink
                                            $iThreadMax,    # thread-max
                                            undef,          # archive-async
                                            undef);         # compress-async

            # Create the test object
            my $oExpireTest = new pgBackRestTest::ExpireCommonTest($oFile, $oLogTest);

            $oExpireTest->stanzaCreate($strStanza, '9.2');
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

            # Cleanup
            if (BackRestTestCommon_Cleanup(\$oLogTest))
            {
                &log(INFO, 'cleanup');
                BackRestTestBackup_Drop(true);
            }
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
        for (my $bHardlink = false; $bHardlink <= true; $bHardlink++)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rmt ${bRemote}, cmp ${bCompress}, hardlink ${bHardlink}",
                                        $iThreadMax == 1 ? $strModule : undef,
                                        $iThreadMax == 1 ? $strThisTest: undef,
                                        \$oLogTest)) {next}

            # Determine if this is a neutral test, i.e. we only want to do it once for local and once for remote.  Neutral means
            # that options such as compression and hardlinks are disabled
            my $bNeutralTest = !$bCompress && !$bHardlink;

            # Get base time
            my $lTime = time() - 100000;

            # Build the manifest
            my %oManifest;

            $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_VERSION} = BACKREST_VERSION;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_CHECK} = JSON::PP::true;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_COPY} = JSON::PP::true;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS} = $bCompress ? JSON::PP::true : JSON::PP::false;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK} = $bHardlink ? JSON::PP::true : JSON::PP::false;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE} = JSON::PP::false;

            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG} = 201306121;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CONTROL} = 937;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_SYSTEM_ID} = 6156904820763115222;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} = '9.3';
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_ID} = 1;

            # Create the test directory
            BackRestTestBackup_Create($bRemote, false);

            $oManifest{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} =
                BackRestTestCommon_DbCommonPathGet();
            $oManifest{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_TYPE} = MANIFEST_VALUE_PATH;

            BackRestTestBackup_ManifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA);

            # Create the file object
            my $oFile = new pgBackRest::File
            (
                $strStanza,
                BackRestTestCommon_RepoPathGet(),
                $bRemote ? 'backup' : undef,
                $bRemote ? $oRemote : $oLocal
            );

            BackRestTestBackup_Init($bRemote, $oFile, true, $oLogTest, $iThreadMax);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'PG_VERSION', '9.3',
                                                  'e1f7a3a299f62225cba076fc6d3d6e677f303482', $lTime);

            # Create base path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base');
            BackRestTestBackup_ManifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384');

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASE',
                                                  'a3b357a3e395e43fcfb19bb13f3c1b5179279593', $lTime);

            BackRestTestBackup_ManifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768');

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768/33000', '33000',
                                                  '7f4c74dc10f61eef43e6ae642606627df1999b34', $lTime);

            # Create global path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'global');

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'global/pg_control', '[replaceme]',
                                                  '56fe5780b8dca9705e0c22032a83828860a21235', $lTime - 100);
            executeTest('cp ' . BackRestTestCommon_DataPathGet() . '/pg_control ' .
                        BackRestTestCommon_DbCommonPathGet() . '/global/pg_control');
            utime($lTime - 100, $lTime - 100, BackRestTestCommon_DbCommonPathGet() . '/global/pg_control')
                or confess &log(ERROR, "unable to set time");
            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/global/pg_control'}{&MANIFEST_SUBKEY_SIZE} = 8192;

            # Create tablespace path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGTBLSPC);

            # Create db config
            BackRestTestCommon_ConfigCreate('db',                           # local
                                            $bRemote ? BACKUP : undef,      # remote
                                            $bCompress,                     # compress
                                            true,                           # checksum
                                            $bRemote ? undef : $bHardlink,  # hardlink
                                            $iThreadMax);                   # thread-max

            # Create backup config
            if ($bRemote)
            {
                BackRestTestCommon_ConfigCreate('backup',                   # local
                                                DB,                         # remote
                                                $bCompress,                 # compress
                                                true,                       # checksum
                                                $bHardlink,                 # hardlink
                                                $iThreadMax);               # thread-max
            }

            # Backup Info (with no stanzas)
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_Info(undef, undef, false);
            BackRestTestBackup_Info(undef, INFO_OUTPUT_JSON, false);

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            my $strType = 'full';
            my $strOptionalParam = '--manifest-save-threshold=3';
            my $strTestPoint;

            if ($bNeutralTest && $bRemote)
            {
                $strOptionalParam .= ' --db-timeout=2';

                if ($iThreadMax > 1)
                {
                    $strTestPoint = TEST_KEEP_ALIVE;
                }
            }

            # Create a file link
            BackRestTestCommon_FileCreate(BackRestTestCommon_DbPathGet() . '/pg_config/postgresql.conf', "listen_addresses = *\n",
                                          $lTime - 100);
            BackRestTestCommon_LinkCreate(BackRestTestCommon_DbPathGet() . '/pg_config/postgresql.conf.link', './postgresql.conf');

            BackRestTestBackup_ManifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'postgresql.conf',
                                                  '../pg_config/postgresql.conf');

            # This link will cause errors because it points to the same location as above
            BackRestTestBackup_ManifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_config_bad',
                                                  '../../db/pg_config');

            my $strFullBackup = BackRestTestBackup_BackupSynthetic(
                                    $strType, $strStanza, \%oManifest, 'error on identical link destinations',
                                    {strOptionalParam => '--log-level-console=detail',
                                     iExpectedExitStatus => ERROR_LINK_DESTINATION});

            # Remove failing link
            BackRestTestBackup_ManifestLinkRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_config_bad');

            # This link will fail because it points to a link
            BackRestTestBackup_ManifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'postgresql.conf.bad',
                                                  '../pg_config/postgresql.conf.link');

            # Fail bacause two links point to the same place
            $strFullBackup = BackRestTestBackup_BackupSynthetic(
                                 $strType, $strStanza, \%oManifest, 'error on link to a link',
                                 {strOptionalParam => '--log-level-console=detail', iExpectedExitStatus => ERROR_LINK_DESTINATION});

            # Remove failing link
            BackRestTestBackup_ManifestLinkRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'postgresql.conf.bad');

            # Create stat directory link and file
            BackRestTestBackup_ManifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_stat', '../pg_stat');
            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA . '/pg_stat', 'global.stat', 'stats',
                                                  'e350d5ce0153f3e22d5db21cf2a4eff00f3ee877', $lTime - 100);
            BackRestTestBackup_ManifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_clog');

            $strFullBackup = BackRestTestBackup_BackupSynthetic(
                                 $strType, $strStanza, \%oManifest, undef,
                                 {strOptionalParam => $strOptionalParam, strTest => $strTestPoint, fTestDelay => 0});

            # Test protocol timeout
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest && $bRemote)
            {
                BackRestTestBackup_BackupSynthetic(
                    $strType, $strStanza, \%oManifest, 'protocol timeout',
                    {strOptionalParam => '--db-timeout=1',
                     strTest => TEST_BACKUP_START,
                     fTestDelay => 1,
                     iExpectedExitStatus => ERROR_PROTOCOL_TIMEOUT});

                # Remove the aborted backup so the next backup is not a resume
                BackRestTestCommon_PathRemove(BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp", $bRemote);
            }

            # Stop operations and make sure the correct error occurs
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest)
            {
                # Test a backup abort
                BackRestTestBackup_BackupBegin($strType, $strStanza, 'abort backup - local',
                                               {strTest => TEST_BACKUP_START, fTestDelay => 5, iExpectedExitStatus => ERROR_TERM});

                BackRestTestBackup_Stop(undef, undef, true);

                BackRestTestBackup_BackupEnd();

                # Test global stop
                BackRestTestBackup_BackupSynthetic($strType, $strStanza, undef, 'global stop',
                                                   {iExpectedExitStatus => ERROR_STOP});

                # Test stanza stop
                BackRestTestBackup_Stop($strStanza);

                # This time a warning should be generated
                BackRestTestBackup_Stop($strStanza);

                BackRestTestBackup_BackupSynthetic($strType, $strStanza, undef, 'stanza stop',
                                                   {iExpectedExitStatus => ERROR_STOP});

                BackRestTestBackup_Start($strStanza);
                BackRestTestBackup_Start();

                # This time a warning should be generated
                BackRestTestBackup_Start();

                # If the backup is remote then test remote stops
                if ($bRemote)
                {
                    BackRestTestBackup_BackupBegin($strType, $strStanza, 'abort backup - remote',
                                                   {strTest => TEST_BACKUP_START, fTestDelay => 5,
                                                    iExpectedExitStatus => ERROR_TERM});

                    BackRestTestBackup_Stop(undef, $bRemote, true);

                    BackRestTestBackup_BackupEnd();

                    BackRestTestBackup_BackupSynthetic($strType, $strStanza, undef, 'global stop',
                                                       {iExpectedExitStatus => ERROR_STOP});

                    BackRestTestBackup_Start(undef, $bRemote, true);
                }
            }

            # Cleanup any garbage left in the temp backup path
            executeTest('rm -rf ' . BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp", {bRemote => $bRemote});

            # Backup Info
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_Info($strStanza, undef, false);
            BackRestTestBackup_Info($strStanza, INFO_OUTPUT_JSON, false);

            # Resume Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'full';

            # Create files in root tblspc paths that should not be copied or deleted.
            # This will be checked later after a --force restore.
            my $strDoNotDeleteFile = BackRestTestCommon_DbTablespacePathGet(1, 2) . '/donotdelete.txt';
            BackRestTestCommon_FileCreate($strDoNotDeleteFile, 'DONOTDELETE-1-2');

            BackRestTestCommon_FileCreate(BackRestTestCommon_DbTablespacePathGet(1) . '/donotdelete.txt', 'DONOTDELETE-1');
            BackRestTestCommon_FileCreate(BackRestTestCommon_DbTablespacePathGet(2) . '/donotdelete.txt', 'DONOTDELETE-2');
            BackRestTestCommon_FileCreate(BackRestTestCommon_DbTablespacePathGet(2, 2) . '/donotdelete.txt', 'DONOTDELETE-2-2');

            my $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strFullBackup}",
                                        $strTmpPath, $bRemote);

            my $oMungeManifest = BackRestTestCommon_manifestLoad("$strTmpPath/backup.manifest", $bRemote);
            $oMungeManifest->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/PG_VERSION', 'checksum');
            BackRestTestCommon_manifestSave("$strTmpPath/backup.manifest", $oMungeManifest, $bRemote);

            # Create a temp file in backup temp root to be sure it's deleted correctly
            executeTest("touch ${strTmpPath}/file.tmp" . ($bCompress ? '.gz' : ''),
                        {bRemote => $bRemote});

            $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'resume',
                                                                {strTest => TEST_BACKUP_RESUME});

            # Misconfigure repo-path and check errors
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest)
            {
                BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'invalid repo',
                    {strOptionalParam => '--' . OPTION_REPO_PATH . '=/bogus_path' .
                     '  --log-level-console=detail', iExpectedExitStatus => ERROR_PATH_MISSING});
            }

            # Restore - tests various mode, extra files/paths, missing files/paths
            #-----------------------------------------------------------------------------------------------------------------------
            my $bDelta = true;
            my $bForce = false;

            # Create a path and file that are not in the manifest
            BackRestTestBackup_PathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'deleteme');
            BackRestTestBackup_FileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'deleteme/deleteme.txt', 'DELETEME');

            # Change path mode
            BackRestTestBackup_PathMode(\%oManifest, MANIFEST_TARGET_PGDATA, 'base', '0777');

            # Change an existing link to the wrong directory
            BackRestTestBackup_FileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_stat');
            BackRestTestBackup_LinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_stat', '../wrong');

            # Remove a path
            BackRestTestBackup_PathRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_clog');

            # Remove a file
            BackRestTestBackup_FileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000');

            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33000'}{&MANIFEST_SUBKEY_CHECKSUM} =
                'a10909c2cdcaf5adb7e6b092a4faba558b62bd96';

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'add and delete files', undef, ' --link-all --db-include=32768');

            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33000'}{&MANIFEST_SUBKEY_CHECKSUM} =
                '7f4c74dc10f61eef43e6ae642606627df1999b34';

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'add and delete files', undef, ' --link-all');

            # Additional restore tests that don't need to be performed for every permutation
            if ($bNeutralTest && !$bRemote)
            {
                # This time manually restore all links
                BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                           undef, undef, undef, undef, undef, undef,
                                           'restore all links by mapping', undef, '--log-level-console=detail' .
                                           ' --link-map=pg_stat=../pg_stat' .
                                           ' --link-map=postgresql.conf=../pg_config/postgresql.conf');

                # Error when links overlap
                BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                           undef, undef, undef, undef, undef, undef,
                                           'restore all links by mapping', ERROR_LINK_DESTINATION, '--log-level-console=warn' .
                                           ' --link-map=pg_stat=../pg_stat' .
                                           ' --link-map=postgresql.conf=../pg_stat/postgresql.conf');

                # Error when links still exist on non-delta restore
                $bDelta = false;

                executeTest('rm -rf ' . BackRestTestCommon_DbCommonPathGet() . "/*");

                BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                           undef, undef, undef, undef, undef, undef,
                                           'error on existing linked path', ERROR_RESTORE_PATH_NOT_EMPTY,
                                           '--log-level-console=warn --link-all');

                executeTest('rm -rf ' . BackRestTestCommon_DbPathGet() . "/pg_stat/*");

                BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                           undef, undef, undef, undef, undef, undef,
                                           'error on existing linked file', ERROR_RESTORE_PATH_NOT_EMPTY,
                                           '--log-level-console=warn --link-all');

                # Now a combination of remapping
                $bDelta = true;

                BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                           undef, undef, undef, undef, undef, undef,
                                           'restore all links --link-all and mapping', undef, '--log-level-console=detail' .
                                           '  --link-map=pg_stat=../pg_stat  --link-all');
            }

            # Restore - test errors when $PGDATA cannot be verified
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = true;

            # Remove PG_VERSION
            BackRestTestBackup_FileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'PG_VERSION');

            # Attempt the restore
            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'fail on missing PG_VERSION', ERROR_RESTORE_PATH_NOT_EMPTY, '--log-level-console=detail');

            # Write a backup.manifest file to make $PGDATA valid
            BackRestTestCommon_FileCreate(BackRestTestCommon_DbCommonPathGet() . '/backup.manifest', 'BOGUS');

            # Munge the user to make sure it gets reset on the next run
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strFullBackup, MANIFEST_SECTION_TARGET_FILE,
                                             MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_USER, 'bogus');
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strFullBackup, MANIFEST_SECTION_TARGET_FILE,
                                             MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_GROUP, 'bogus');

            # Restore succeeds
            BackRestTestBackup_ManifestLinkMap(\%oManifest, MANIFEST_TARGET_PGDATA . '/pg_stat');
            BackRestTestBackup_ManifestLinkMap(\%oManifest, MANIFEST_TARGET_PGDATA . '/postgresql.conf');

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'restore succeeds with backup.manifest file', undef, '--log-level-console=detail');

            # Various broken info tests
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup);

            my $strInfoFile = BackRestTestCommon_RepoPathGet . "/backup/${strStanza}/backup.info";
            my %oInfo;
            BackRestTestCommon_iniLoad($strInfoFile, \%oInfo, $bRemote);

            # Break the database version
            my $strDbVersion = $oInfo{'db'}{&MANIFEST_KEY_DB_VERSION};

            $oInfo{db}{&MANIFEST_KEY_DB_VERSION} = '8.0';
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            BackRestTestBackup_BackupSynthetic($strType, $strStanza, undef, 'invalid database version',
                                               {iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                                                strOptionalParam => '--log-level-console=detail'});
            $oInfo{db}{&MANIFEST_KEY_DB_VERSION} = $strDbVersion;

            # Break the database system id
            my $ullDbSysId = $oInfo{'db'}{&MANIFEST_KEY_SYSTEM_ID};
            $oInfo{db}{&MANIFEST_KEY_SYSTEM_ID} = 6999999999999999999;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            BackRestTestBackup_BackupSynthetic($strType, $strStanza, undef, 'invalid system id',
                                               {iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                                                strOptionalParam => '--log-level-console=detail'});
            $oInfo{db}{&MANIFEST_KEY_SYSTEM_ID} = $ullDbSysId;

            # Break the control version
            my $iControlVersion = $oInfo{'db'}{&MANIFEST_KEY_CONTROL};
            $oInfo{db}{&MANIFEST_KEY_CONTROL} = 842;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            BackRestTestBackup_BackupSynthetic($strType, $strStanza, undef, 'invalid control version',
                                               {iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                                                strOptionalParam => '--log-level-console=detail'});
            $oInfo{db}{&MANIFEST_KEY_CONTROL} = $iControlVersion;

            # Break the catalog version
            my $iCatalogVersion = $oInfo{'db'}{&MANIFEST_KEY_CATALOG};
            $oInfo{db}{&MANIFEST_KEY_CATALOG} = 197208141;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            BackRestTestBackup_BackupSynthetic($strType, $strStanza, undef, 'invalid catalog version',
                                               {iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                                                strOptionalParam => '--log-level-console=detail'});

            # Fix up info file for next test
            $oInfo{db}{&MANIFEST_KEY_CATALOG} = $iCatalogVersion;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            # Test broken tablespace configuration
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            my $strTblSpcPath = BackRestTestCommon_DbCommonPathGet() . '/' . DB_PATH_PGTBLSPC;

            # Create a directory in pg_tablespace
            BackRestTestCommon_PathCreate("${strTblSpcPath}/path");

            BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'invalid path in ' . DB_PATH_PGTBLSPC,
                                               {iExpectedExitStatus => ERROR_LINK_EXPECTED,
                                               strOptionalParam => '--log-level-console=detail'});

            BackRestTestCommon_PathRemove("${strTblSpcPath}/path");

            # Create a relative link in PGDATA
            BackRestTestCommon_LinkCreate("${strTblSpcPath}/99999", '../invalid_tblspc');

            BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'invalid relative tablespace in $PGDATA',
                                               {iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                                                strOptionalParam => '--log-level-console=detail'});

            BackRestTestCommon_FileRemove("${strTblSpcPath}/99999");

            # Create tablespace in PGDATA
            BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet() . '/invalid_tblspc');
            BackRestTestCommon_LinkCreate("${strTblSpcPath}/99999", BackRestTestCommon_DbCommonPathGet() . '/invalid_tblspc');

            BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'invalid tablespace in $PGDATA',
                                               {iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                                                strOptionalParam => '--log-level-console=detail'});

            BackRestTestCommon_PathRemove(BackRestTestCommon_DbCommonPathGet() . '/invalid_tblspc');
            BackRestTestCommon_FileRemove("${strTblSpcPath}/99999");

            # Incr backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';

            # Add tablespace 1
            BackRestTestBackup_ManifestTablespaceCreate(\%oManifest, 1);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/1', 'tablespace1.txt', 'TBLSPC1',
                                                  'd85de07d6421d90aa9191c11c889bfde43680f0f', $lTime);
            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'badchecksum.txt', 'BADCHECKSUM',
                                                  'f927212cd08d11a42a666b2f04235398e9ceeb51', $lTime);

            my $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'add tablespace 1',
                                                               {strOptionalParam => '--test'});

            # Resume Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';

            # Move database from backup to temp
            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            $oMungeManifest = BackRestTestCommon_manifestLoad("$strTmpPath/" . FILE_MANIFEST, $bRemote);
            $oMungeManifest->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/badchecksum.txt', 'checksum', 'bogus');
            BackRestTestCommon_manifestSave("$strTmpPath/" . FILE_MANIFEST, $oMungeManifest, $bRemote);

            # Add tablespace 2
            BackRestTestBackup_ManifestTablespaceCreate(\%oManifest, 2);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', 'tablespace2.txt', 'TBLSPC2',
                                                  'dc7f76e43c46101b47acc55ae4d593a9e6983578', $lTime);


            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'resume and add tablespace 2',
                                                            {strTest => TEST_BACKUP_RESUME});

            # Resume Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';

            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'cannot resume - new diff',
                                                            {strTest => TEST_BACKUP_NORESUME,
                                                             strOptionalParam => '--log-level-console=detail'});

            # Resume Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';

            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'cannot resume - disabled',
                                                            {strTest => TEST_BACKUP_NORESUME,
                                                             strOptionalParam => '--no-resume --log-level-console=detail'});

            # Restore
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = false;

            # Fail on used path
            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'fail on used path', ERROR_RESTORE_PATH_NOT_EMPTY, '--log-level-console=detail');
            # Fail on undef format
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, undef);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                      undef, undef, undef, undef, undef, undef,
                                      'fail on undef format', ERROR_FORMAT, '--log-level-console=detail');

            # Fail on mismatch format
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, 0);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                      undef, undef, undef, undef, undef, undef,
                                      'fail on mismatch format', ERROR_FORMAT, '--log-level-console=detail');

            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef,
                                             BACKREST_FORMAT);

            # Remap the base and tablespace paths
            my %oRemapHash;
            $oRemapHash{&MANIFEST_TARGET_PGDATA} = BackRestTestCommon_DbCommonPathGet(2);
            $oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/1'} = BackRestTestCommon_DbTablespacePathGet(1, 2);
            $oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/2'} = BackRestTestCommon_DbTablespacePathGet(2, 2);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, \%oRemapHash, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef, 'remap all paths', undef,
                                       '--log-level-console=detail');

            # Restore (make sure file in root tablespace path is not deleted by --delta)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, \%oRemapHash, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'ensure file in tblspc root remains after --delta', undef, '--log-level-console=detail');

            if (!-e $strDoNotDeleteFile)
            {
                confess "${strDoNotDeleteFile} was deleted by --delta";
            }

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', 'BASE2',
                                                  '09b5e31766be1dba1ec27de82f975c1b6eea2a92', $lTime);

            BackRestTestBackup_ManifestTablespaceDrop(\%oManifest, 1, 2);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', 'tablespace2b.txt', 'TBLSPC2B',
                                                  'e324463005236d83e6e54795dbddd20a74533bf3', $lTime);

            # Munge the version to make sure it gets corrected on the next run
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, INI_SECTION_BACKREST, INI_KEY_VERSION, undef,
                                             '0.00');

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'add files and remove tablespace 2',
                                                            {strOptionalParam => '--log-level-console=detail'});

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            # Delete the backup.info and make sure it gets reconstructed correctly
            if ($bNeutralTest)
            {
                executeTest('rm ' . BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/backup.info", {bRemote => $bRemote});
            }

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASEUPDT',
                                                  '9a53d532e27785e681766c98516a5e93f096a501', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'update files',
                                                            {strOptionalParam => '--log-level-console=detail'});

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'updates since last full',
                                                            {strOptionalParam => '--log-level-console=detail'});

            # Incr Backup
            #
            # Remove a file from the db after the manifest has been built but before files are copied.  The file will not be shown
            # as removed in the log because it had not changed since the last backup so it will only be referenced.
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_BackupBegin($strType, $strStanza, "remove files - but won't affect manifest",
                                           {strTest => TEST_MANIFEST_BUILD, fTestDelay => 1,
                                            strOptionalParam => '--log-level-console=detail'});

            BackRestTestBackup_FileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000');

            $strBackup = BackRestTestBackup_BackupEnd(\%oManifest);

            # Diff Backup
            #
            # Remove base2.txt and changed tablespace2c.txt during the backup.  The removed file should be logged and the changed
            # file should have the new, larger size logged and in the manifest.
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strType = 'diff';

            BackRestTestBackup_ManifestFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000');

            BackRestTestBackup_ManifestFileRemove(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', 'tablespace2b.txt', true);
            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', 'tablespace2c.txt', 'TBLSPC2C',
                                                  'ad7df329ab97a1e7d35f1ff0351c079319121836', $lTime);

            BackRestTestBackup_BackupBegin($strType, $strStanza, 'remove files during backup',
                                           {strTest => TEST_MANIFEST_BUILD, fTestDelay => 1,
                                            strOptionalParam => '--log-level-console=detail'});

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', 'tablespace2c.txt', 'TBLSPCBIGGER',
                                                  'dfcb8679956b734706cf87259d50c88f83e80e66', $lTime);

            BackRestTestBackup_ManifestFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', true);

            $strBackup = BackRestTestBackup_BackupEnd(\%oManifest);

            # Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'full';
            BackRestTestBackup_ManifestReference(\%oManifest);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASEUPDT2',
                                                  '7579ada0808d7f98087a0a586d0df9de009cdc33', $lTime);

            $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'update file',
                                                                {strOptionalParam => '--log-level-console=detail'});

            # Call expire
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_Expire($strStanza, undef, $oFile, 1);

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', 'BASE2UPDT',
                                                  'cafac3c59553f2cfde41ce2e62e7662295f108c0', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, \%oManifest, 'add file',
                                                            {strOptionalParam => '--log-level-console=detail'});

            # Compact Restore
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;

            executeTest('rm -rf ' . BackRestTestCommon_DbCommonPathGet(2) . "/*");

            my $strDbPath = BackRestTestCommon_DbCommonPathGet(2) . '/base';
            BackRestTestCommon_PathCreate($strDbPath);

            $oRemapHash{&MANIFEST_TARGET_PGDATA} = $strDbPath;
            delete($oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/1'});
            delete($oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/2'});

            BackRestTestBackup_Restore($oFile, OPTION_DEFAULT_RESTORE_SET, $strStanza, $bRemote, \%oManifest, \%oRemapHash,
                                       $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                                       'no tablespace remap - error when tablespace dir does not exist', ERROR_PATH_MISSING,
                                       "--log-level-console=detail --tablespace-map-all=../../tablespace", false);

            BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet(2) . '/tablespace');

            BackRestTestBackup_Restore($oFile, OPTION_DEFAULT_RESTORE_SET, $strStanza, $bRemote, \%oManifest, undef, $bDelta,
                                       $bForce, undef, undef, undef, undef, undef, undef, 'no tablespace remap', undef,
                                       "--tablespace-map-all=../../tablespace --log-level-console=detail", false);

            # Backup Info (with an empty stanza)
            #-----------------------------------------------------------------------------------------------------------------------
            executeTest('mkdir ' . BackRestTestCommon_RepoPathGet . '/backup/db_empty',
                        {bRemote => $bRemote});

            BackRestTestBackup_Info(undef, undef, false);
            BackRestTestBackup_Info(undef, INFO_OUTPUT_JSON, false);
            BackRestTestBackup_Info('bogus', undef, false);
            BackRestTestBackup_Info('bogus', INFO_OUTPUT_JSON, false);

            # Dump out history path at the end to verify all history files are being recorded.  This test is only performed locally
            # because for some reason sort order is different when this command is executed via ssh (even though the content if the
            # directory is identical).
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bNeutralTest && !$bRemote)
            {
                executeTest('ls -1R ' . BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/" . PATH_BACKUP_HISTORY,
                            {oLogTest => $oLogTest, bRemote => $bRemote});
            }
        }
        }
        }

        if (BackRestTestCommon_Cleanup(\$oLogTest))
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
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
        $bCreate = true;

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
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rmt ${bRemote}, arc_async ${bArchiveAsync}, cmp ${bCompress}")) {next}

            # Create the file object
            my $oFile = new pgBackRest::File
            (
                $strStanza,
                BackRestTestCommon_RepoPathGet(),
                $bRemote ? 'backup' : undef,
                $bRemote ? $oRemote : $oLocal
            );

            # Create the test directory
            if ($bCreate)
            {
                BackRestTestBackup_Create($bRemote, false);
            }

            # Create db config
            BackRestTestCommon_ConfigCreate('db',                              # local
                                            $bRemote ? BACKUP : undef,         # remote
                                            $bCompress,                        # compress
                                            undef,                             # checksum
                                            $bRemote ? undef : false,          # hardlink
                                            $iThreadMax,                       # thread-max
                                            $bArchiveAsync,                    # archive-async
                                            undef);                            # compress-async

            # Create backup config
            if ($bRemote)
            {
                BackRestTestCommon_ConfigCreate('backup',                      # local
                                                $bRemote ? DB : undef,         # remote
                                                $bCompress,                    # compress
                                                undef,                         # checksum
                                                false,                         # hardlink
                                                $iThreadMax,                   # thread-max
                                                undef,                         # archive-async
                                                undef);                        # compress-async
            }

            # Create the cluster
            if ($bCreate)
            {
                BackRestTestBackup_ClusterCreate();
                $bCreate = false;
            }

            BackRestTestBackup_Init($bRemote, $oFile, false, undef, $iThreadMax);

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
            BackRestTestBackup_PgExecuteNoTrans("create database test1");
            my $strDbTest1Id = BackRestTestBackup_PgSelectOne("select oid from pg_database where datname = 'test1'");
            BackRestTestBackup_PgExecuteNoTrans("create database test2");
            my $strDbTest2Id = BackRestTestBackup_PgSelectOne("select oid from pg_database where datname = 'test2'");

            # Test invalid archive command
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;
            $strComment = 'fail on invalid archive_command';

            # Check archive_command_not_set error
            BackRestTestBackup_ClusterStop();
            BackRestTestBackup_ClusterStart(undef, undef, undef, false);

            BackRestTestBackup_Backup($strType, $strStanza, $strComment, {iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});

            # Reset the cluster to a normal state so the next test will work
            BackRestTestBackup_ClusterStop();
            BackRestTestBackup_ClusterStart();

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;

            # Create the table where test messages will be stored
            BackRestTestBackup_PgExecute("create table test (message text not null)");
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("insert into test values ('$strDefaultMessage')");

            # Acquire the backup advisory lock so it looks like a backup is running
            if (!BackRestTestBackup_PgSelectOne('select pg_try_advisory_lock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to acquire advisory lock for testing';
            }

            $strComment = 'fail on backup lock exists';
            BackRestTestBackup_Backup($strType, $strStanza, $strComment,
                                      {iExpectedExitStatus => ERROR_LOCK_ACQUIRE});

            # Release the backup advisory lock so the next backup will succeed
            if (!BackRestTestBackup_PgSelectOne('select pg_advisory_unlock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to acquire advisory lock for testing';
            }

            $strComment = 'update during backup';
            BackRestTestBackup_BackupBegin($strType, $strStanza, $strComment,
                                           {strTest => TEST_MANIFEST_BUILD, fTestDelay => $fTestDelay});

            BackRestTestBackup_PgExecute("update test set message = '$strFullMessage'", false);

            my $strFullBackup = BackRestTestBackup_BackupEnd();

            # Execute stop and make sure the backup fails
            #-----------------------------------------------------------------------------------------------------------------------
            $strComment = 'attempt backup when stopped';

            # Stop the cluster to check for any errors before continuing since the stop tests will definitely create errors and
            # the logs will to be deleted to avoid causing issues further down the line.
            BackRestTestBackup_ClusterStop();
            BackRestTestBackup_ClusterStart();

            BackRestTestBackup_Stop();

            BackRestTestBackup_Backup($strType, $strStanza, $strComment,
                                      {iExpectedExitStatus => ERROR_STOP});

            BackRestTestBackup_Start();

            # Setup the time target
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_PgExecute("update test set message = '$strTimeMessage'", false);
            BackRestTestBackup_PgSwitchXlog();
            my $strTimeTarget = BackRestTestBackup_PgSelectOne("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ')");
            &log(INFO, "        time target is ${strTimeTarget}");

            # Incr backup - fail on archive_mode=always when version >= 9.5
            #-----------------------------------------------------------------------------------------------------------------------
            if (BackRestTestCommon_DbVersion() >= 9.5)
            {
                $strType = BACKUP_TYPE_INCR;

                $strComment = 'fail on archive_mode=always';

                # Set archive_mode=always
                BackRestTestBackup_ClusterStop();
                BackRestTestBackup_ClusterStart(undef, undef, undef, undef, true);

                BackRestTestBackup_Backup($strType, $strStanza, $strComment, {iExpectedExitStatus => ERROR_FEATURE_NOT_SUPPORTED});

                # Reset the cluster to a normal state so the next test will work
                BackRestTestBackup_ClusterStop();
                BackRestTestBackup_ClusterStart();
            }

            # Incr backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            BackRestTestBackup_PgExecuteNoTrans("create tablespace ts1 location '" .
                                                BackRestTestCommon_DbTablespacePathGet(1) . "'");
            BackRestTestBackup_PgExecute("alter table test set tablespace ts1", true);

            BackRestTestBackup_PgExecute("create table test_remove (id int)", false);
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("update test set message = '$strDefaultMessage'", false);
            BackRestTestBackup_PgSwitchXlog();

            # Start a backup so the next backup has to restart it
            if (BackRestTestCommon_DbVersion() >= 9.3)
            {
                BackRestTestBackup_PgSelectOne("select pg_start_backup('test backup that will be cancelled', true)");
            }

            # # Can't do this test yet because it puts errors in the Postgres log
            # $strComment = 'fail on backup already running';
            # BackRestTestBackup_Backup($strType, $strStanza, $bRemote, $oFile, $strComment, undef, undef, ERROR_DB_QUERY);

            $strComment = 'update during backup';
            BackRestTestBackup_BackupBegin($strType, $strStanza, $strComment,
                                           {strTest => TEST_MANIFEST_BUILD, fTestDelay => $fTestDelay,
                                            strOptionalParam => '--' . OPTION_STOP_AUTO . ' --no-' . OPTION_BACKUP_ARCHIVE_CHECK});

            BackRestTestBackup_PgExecute("drop table test_remove", false);
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("update test set message = '$strIncrMessage'", false);

            my $strIncrBackup = BackRestTestBackup_BackupEnd();

            # Setup the xid target
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_PgExecute("update test set message = '$strXidMessage'", false, false);
            BackRestTestBackup_PgSwitchXlog();
            my $strXidTarget = BackRestTestBackup_PgSelectOne("select txid_current()");
            BackRestTestBackup_PgCommit();
            &log(INFO, "        xid target is ${strXidTarget}");

            # Setup the name target
            #-----------------------------------------------------------------------------------------------------------------------
            my $strNameTarget = 'backrest';

            BackRestTestBackup_PgExecute("update test set message = '$strNameMessage'", false, true);
            BackRestTestBackup_PgSwitchXlog();

            if (BackRestTestCommon_DbVersion() >= 9.1)
            {
                BackRestTestBackup_PgExecute("select pg_create_restore_point('${strNameTarget}')", false, false);
            }

            &log(INFO, "        name target is ${strNameTarget}");

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

            BackRestTestBackup_Restore($oFile, OPTION_DEFAULT_RESTORE_SET, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStop();

            # Expect failure because db path is not empty
            $strComment = 'path not empty';
            $iExpectedExitStatus = ERROR_RESTORE_PATH_NOT_EMPTY;

            BackRestTestBackup_Restore($oFile, OPTION_DEFAULT_RESTORE_SET, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            # Drop and recreate db path
            BackRestTestCommon_PathRemove(BackRestTestCommon_DbCommonPathGet());
            BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet());
            BackRestTestCommon_PathRemove(BackRestTestCommon_DbPathGet() . '/pg_xlog');
            BackRestTestCommon_PathCreate(BackRestTestCommon_DbPathGet() . '/pg_xlog');
            BackRestTestCommon_PathRemove(BackRestTestCommon_DbTablespacePathGet(1));
            BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet(1));

            # Now the restore should work
            $strComment = undef;
            $iExpectedExitStatus = undef;

            BackRestTestBackup_Restore($oFile, OPTION_DEFAULT_RESTORE_SET, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus, ' --db-include=' . $strDbTest2Id);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strNameMessage);

            # Restore (restore type = immediate, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            if (BackRestTestCommon_DbVersion() >= 9.4)
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

                BackRestTestBackup_ClusterStop();

                BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                           $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                           $oRecoveryHashRef, $strComment, $iExpectedExitStatus, undef);

                BackRestTestBackup_ClusterStart();
                BackRestTestBackup_PgSelectOneTest('select message from test', $strFullMessage);
            }

            # Restore (restore type = xid, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = true;
            $strType = RECOVERY_TYPE_XID;
            $strTarget = $strXidTarget;
            $bTargetExclusive = undef;
            $strTargetAction = BackRestTestCommon_DbVersion() >= 9.1 ? 'promote' : undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            executeTest('rm -rf ' . BackRestTestCommon_DbCommonPathGet() . "/*");
            executeTest('rm -rf ' . BackRestTestCommon_DbPathGet() . "/pg_xlog/*");
            executeTest('rm -rf ' . BackRestTestCommon_DbTablespacePathGet(1));

            BackRestTestBackup_Restore($oFile, $strIncrBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus,
                                       '--tablespace-map-all=../../tablespace', false);

            # Save recovery file to test so we can use it in the next test
            $oFile->copy(PATH_ABSOLUTE, BackRestTestCommon_DbCommonPathGet() . '/recovery.conf',
                         PATH_ABSOLUTE, BackRestTestCommon_TestPathGet() . '/recovery.conf');

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strXidMessage);

            BackRestTestBackup_PgExecute("update test set message = '$strTimelineMessage'", false);

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

            BackRestTestBackup_ClusterStop();

            executeTest('rm -rf ' . BackRestTestCommon_DbCommonPathGet() . "/*");
            executeTest('rm -rf ' . BackRestTestCommon_DbPathGet() . "/pg_xlog/*");
            executeTest('rm -rf ' . BackRestTestCommon_DbTablespacePathGet(1) . "/*");

            # Restore recovery file that was saved in last test
            $oFile->move(PATH_ABSOLUTE, BackRestTestCommon_TestPathGet() . '/recovery.conf',
                         PATH_ABSOLUTE, BackRestTestCommon_DbCommonPathGet() . '/recovery.conf');

            BackRestTestBackup_Restore($oFile, OPTION_DEFAULT_RESTORE_SET, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strXidMessage);

            BackRestTestBackup_PgExecute("update test set message = '$strTimelineMessage'", false);

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

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strTimeMessage);

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

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strIncrBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strIncrMessage);

            # Restore (restore type = name)
            #-----------------------------------------------------------------------------------------------------------------------
            if (BackRestTestCommon_DbVersion() >= 9.1)
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

                BackRestTestBackup_ClusterStop();

                BackRestTestBackup_Restore($oFile, OPTION_DEFAULT_RESTORE_SET, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                           $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                           $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

                BackRestTestBackup_ClusterStart();
                BackRestTestBackup_PgSelectOneTest('select message from test', $strNameMessage);
            }

            # Restore (restore type = default, timeline = 3)
            #-----------------------------------------------------------------------------------------------------------------------
            if (BackRestTestCommon_DbVersion() >= 8.4)
            {
                $bDelta = true;
                $bForce = false;
                $strType = RECOVERY_TYPE_DEFAULT;
                $strTarget = undef;
                $bTargetExclusive = undef;
                $strTargetAction = undef;
                $strTargetTimeline = 4;
                $oRecoveryHashRef = BackRestTestCommon_DbVersion() >= 9.0 ? {'standby-mode' => 'on'} : undef;
                $strComment = undef;
                $iExpectedExitStatus = undef;

                &log(INFO, "    testing recovery type = ${strType}");

                BackRestTestBackup_ClusterStop();

                BackRestTestBackup_Restore($oFile, $strIncrBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                           $strType, $strTarget, $bTargetExclusive, $strTargetAction, $strTargetTimeline,
                                           $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

                BackRestTestBackup_ClusterStart(undef, undef, true);
                BackRestTestBackup_PgSelectOneTest('select message from test', $strTimelineMessage, 120);
            }

            # Incr backup - make sure a --no-online backup fails
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            $strComment = 'fail on --no-' . OPTION_ONLINE;
            BackRestTestBackup_Backup($strType, $strStanza, $strComment,
                                      {iExpectedExitStatus => ERROR_POSTMASTER_RUNNING,
                                       strOptionalParam => '--no-' . OPTION_ONLINE});

            # Incr backup - allow --no-online backup to succeed with --force
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;

            $strComment = 'succeed on --no-' . OPTION_ONLINE . ' with --' . OPTION_FORCE;
            BackRestTestBackup_Backup($strType, $strStanza, $strComment,
                                      {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});

            $bCreate = true;
        }
        }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test collision
    #
    # See if it is possible for a table to be written to, have stop backup run, and be written to again all in the same second.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'collision')
    {
        $iRun = 0;
        my $iRunMax = 1000;

        if (!$bVmOut)
        {
            &log(INFO, "Test Backup Collision\n");
        }

        # Create the file object
        my $oFile = (pgBackRest::File->new
        (
            $strStanza,
            BackRestTestCommon_RepoPathGet(),
            NONE,
            $oLocal
        ))->clone();

        # Create the test database
        BackRestTestBackup_Create(false);

        # Create the config file
        BackRestTestCommon_ConfigCreate('db',                              # local
                                        undef,                             # remote
                                        false,                             # compress
                                        false,                             # checksum
                                        false,                             # hardlink
                                        $iThreadMax,                       # thread-max
                                        false,                             # archive-async
                                        undef);                            # compress-async

        # Create the test table
        BackRestTestBackup_PgExecute("create table test_collision (id int)");

        # Construct filename to test
        my $strFile = BackRestTestCommon_DbCommonPathGet() . "/base";

        # Get the oid of the postgres db
        my $strSql = "select oid from pg_database where datname = 'postgres'";
        my $hStatement = BackRestTestBackup_PgHandleGet()->prepare($strSql);

        $hStatement->execute() or
            confess &log(ERROR, "Unable to execute: ${strSql}");

        my @oyRow = $hStatement->fetchrow_array();
        $strFile .= '/' . $oyRow[0];

        $hStatement->finish();

        # Get the oid of the new table so we can check the file on disk
        $strSql = "select oid from pg_class where relname = 'test_collision'";
        $hStatement = BackRestTestBackup_PgHandleGet()->prepare($strSql);

        $hStatement->execute() or
            confess &log(ERROR, "Unable to execute: ${strSql}");

        @oyRow = $hStatement->fetchrow_array();
        $strFile .= '/' . $oyRow[0];

        &log(INFO, 'table filename = ' . $strFile);

        $hStatement->finish();

        BackRestTestBackup_PgExecute("select pg_start_backup('test');");

        # File modified in the same second after the manifest is taken and file is copied
        while ($iRun < $iRunMax)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "mod after manifest")) {next}

            my $strTestChecksum = $oFile->hash(PATH_DB_ABSOLUTE, $strFile);

            # Insert a row and do a checkpoint
            BackRestTestBackup_PgExecute("insert into test_collision values (1)", true);

            # Stat the file to get size/modtime after the backup has started
            my $strBeginChecksum = $oFile->hash(PATH_DB_ABSOLUTE, $strFile);
            my $oStat = lstat($strFile);
            my $lBeginSize = $oStat->size;
            my $lBeginTime = $oStat->mtime;

            # Sleep .5 seconds to give a reasonable amount of time for the file to be copied after the manifest was generated
            # Sleep for a while to show there is a large window where this can happen
            &log(INFO, 'time ' . gettimeofday());
            waitHiRes(.5);
            &log(INFO, 'time ' . gettimeofday());

            # Insert another row
            BackRestTestBackup_PgExecute("insert into test_collision values (1)");

            # Stop backup, start a new backup
            BackRestTestBackup_PgExecute("select pg_stop_backup();");
            BackRestTestBackup_PgExecute("select pg_start_backup('test');");

            # Stat the file to get size/modtime after the backup has restarted
            my $strEndChecksum = $oFile->hash(PATH_DB_ABSOLUTE, $strFile);
            $oStat = lstat($strFile);
            my $lEndSize = $oStat->size;
            my $lEndTime = $oStat->mtime;

            # Error if the size/modtime are the same between the backups
            &log(INFO, "    begin size = ${lBeginSize}, time = ${lBeginTime}, hash ${strBeginChecksum} - " .
                       "end size = ${lEndSize}, time = ${lEndTime}, hash ${strEndChecksum} - test hash ${strTestChecksum}");

            if ($lBeginSize == $lEndSize && $lBeginTime == $lEndTime &&
                $strTestChecksum ne $strBeginChecksum && $strBeginChecksum ne $strEndChecksum)
            {
                &log(ERROR, "size and mod time are the same between backups");
                $iRun = $iRunMax;
                next;
            }
        }

        BackRestTestBackup_PgExecute("select pg_stop_backup();");

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # rsync-collision
    #
    # See if it is possible for a table to be written to, have stop backup run, and be written to again all in the same second.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'rsync-collision')
    {
        $iRun = 0;
        my $iRunMax = 1000;

        if (!$bVmOut)
        {
            &log(INFO, "Test Rsync Collision\n");
        }

        # Create the file object
        my $oFile = (pgBackRest::File->new
        (
            $strStanza,
            BackRestTestCommon_RepoPathGet(),
            NONE,
            $oLocal
        ))->clone();

        # Create the test database
        BackRestTestBackup_Create(false, false);

        # Create test paths
        my $strPathRsync1 = BackRestTestCommon_TestPathGet() . "/rsync1";
        my $strPathRsync2 = BackRestTestCommon_TestPathGet() . "/rsync2";

        BackRestTestCommon_PathCreate($strPathRsync1);
        BackRestTestCommon_PathCreate($strPathRsync2);

        # Rsync command
        my $strCommand = "rsync -vvrt ${strPathRsync1}/ ${strPathRsync2}";

        # File modified in the same second after the manifest is taken and file is copied
        while ($iRun < $iRunMax)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rsync test")) {next}

            # Create test file
            &log(INFO, "create test file");
            BackRestTestCommon_FileCreate("${strPathRsync1}/test.txt", 'TEST1');

            # Stat the file to get size/modtime after the backup has started
            my $strBeginChecksum = $oFile->hash(PATH_DB_ABSOLUTE, "${strPathRsync1}/test.txt");

            # Rsync
            &log(INFO, "rsync 1st time");
            executeTest($strCommand, {bShowOutput => true});

            # Sleep for a while to show there is a large window where this can happen
            &log(INFO, 'time ' . gettimeofday());
            waitHiRes(.5);
            &log(INFO, 'time ' . gettimeofday());

            # Modify the test file within the same second
            &log(INFO, "modify test file");
            BackRestTestCommon_FileCreate("${strPathRsync1}/test.txt", 'TEST2');

            my $strEndChecksum = $oFile->hash(PATH_DB_ABSOLUTE, "${strPathRsync1}/test.txt");

            # Rsync again
            &log(INFO, "rsync 2nd time");
            excuteTest($strCommand, {bShowOutput => true});

            my $strTestChecksum = $oFile->hash(PATH_DB_ABSOLUTE, "${strPathRsync2}/test.txt");

            # Error if checksums are not the same after rsync
            &log(INFO, "    begin hash ${strBeginChecksum} - end hash ${strEndChecksum} - test hash ${strTestChecksum}");

            if ($strTestChecksum ne $strEndChecksum)
            {
                &log(ERROR, "end and test checksums are not the same");
                $iRun = $iRunMax;
                next;
            }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }
}

1;
