#!/usr/bin/perl
####################################################################################################################################
# BackupTest.pl - Unit Tests for BackRest::Backup and BackRest::Restore
####################################################################################################################################
package BackRestTest::BackupTest;

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
use BackRest::Archive;
use BackRest::ArchiveInfo;
use BackRest::Common::Exception;
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Common::Wait;
use BackRest::Db;
use BackRest::Config;
use BackRest::File;
use BackRest::Manifest;
use BackRest::Protocol::Common;
use BackRest::Protocol::RemoteMaster;

use BackRestTest::BackupCommonTest;
use BackRestTest::ExpireCommonTest;
use BackRestTest::CommonTest;

####################################################################################################################################
# BackRestTestBackup_Test
####################################################################################################################################
our @EXPORT = qw(BackRestTestBackup_Test);

sub BackRestTestBackup_Test
{
    my $strTest = shift;
    my $iThreadMax = shift;

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

    my $strArchiveChecksum = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
    my $iArchiveMax = 3;
    my $strXlogPath = BackRestTestCommon_DbCommonPathGet() . '/pg_xlog';
    my $strArchiveTestFile = BackRestTestCommon_DataPathGet() . '/test.archive2.bin';
    my $strArchiveTestFile2 = BackRestTestCommon_DataPathGet() . '/test.archive1.bin';

    # Print test banner
    &log(INFO, 'BACKUP MODULE ******************************************************************');

    # Drop any existing cluster
    BackRestTestBackup_Drop(true, true);

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remotes
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oRemote = new BackRest::Protocol::RemoteMaster
    (
        BackRestTestCommon_CommandRemoteFullGet(),  # Command
        OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
        $strHost,                                   # Host
        $strUserBackRest                            # User
    );

    my $oLocal = new BackRest::Protocol::Common
    (
        OPTION_DEFAULT_BUFFER_SIZE,             # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,          # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK   # Compress network level
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

        &log(INFO, "Test ${strThisTest}\n");

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
                                            $iThreadMax == 1 ? $strThisTest: undef)) {next}

                # Create the file object
                if ($bCreate)
                {
                    $oFile = (new BackRest::File
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

                BackRestTestCommon_ConfigCreate('db',
                                                ($bRemote ? BACKUP : undef),
                                                $bCompress,
                                                undef,       # checksum
                                                undef,       # hardlink
                                                undef,       # thread-max
                                                $bArchiveAsync,
                                                undef);

                my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                                 '/pg_backrest.conf --no-fork --stanza=db archive-push';

                # Loop through backups
                for (my $iBackup = 1; $iBackup <= 3; $iBackup++)
                {
                    my $strArchiveFile;

                    # Loop through archive files
                    for (my $iArchive = 1; $iArchive <= $iArchiveMax; $iArchive++)
                    {
                        # Construct the archive filename
                        my $iArchiveNo = (($iBackup - 1) * $iArchiveMax + ($iArchive - 1)) + 1;

                        if ($iArchiveNo > 255)
                        {
                            confess 'backup total * archive total cannot be greater than 255';
                        }

                        $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo));

                        &log(INFO, '    backup ' . sprintf('%02d', $iBackup) .
                                   ', archive ' .sprintf('%02x', $iArchive) .
                                   " - ${strArchiveFile}");

                        my $strSourceFile = "${strXlogPath}/${strArchiveFile}";

                        $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile, # Source file
                                     PATH_DB_ABSOLUTE, $strSourceFile,      # Destination file
                                     false,                                 # Source is not compressed
                                     false,                                 # Destination is not compressed
                                     undef, undef, undef,                   # Unused params
                                     true);                                 # Create path if it does not exist

                        BackRestTestCommon_Execute($strCommand . " ${strSourceFile}");

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

                            BackRestTestCommon_Execute($strCommand . " ${strSourceFile}", undef, undef, undef,
                                                       ERROR_ARCHIVE_MISMATCH);

                            # Break the system id
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = $strDbVersion;
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID} = '5000900090001855000';
                            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

                            &log(INFO, '        test db system-id mismatch error');

                            BackRestTestCommon_Execute($strCommand . " ${strSourceFile}", undef, undef, undef,
                                                       ERROR_ARCHIVE_MISMATCH);

                            # Move settings back to original
                            $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_SYSTEM_ID} = $ullDbSysId;
                            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

                            # Should succeed because checksum is the same
                            &log(INFO, '        test archive duplicate ok');

                            BackRestTestCommon_Execute($strCommand . " ${strSourceFile}");

                            # Now it should break on archive duplication (because checksum is different
                            &log(INFO, '        test archive duplicate error');

                            $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile2, # Source file
                                         PATH_DB_ABSOLUTE, $strSourceFile,       # Destination file
                                         false,                                  # Source is not compressed
                                         false,                                  # Destination is not compressed
                                         undef, undef, undef,                    # Unused params
                                         true);                                  # Create path if it does not exist

                            BackRestTestCommon_Execute($strCommand . " ${strSourceFile}", undef, undef, undef,
                                                       ERROR_ARCHIVE_DUPLICATE);

                            if ($bArchiveAsync)
                            {
                                my $strDuplicateWal =
                                    ($bRemote ? BackRestTestCommon_LocalPathGet() : BackRestTestCommon_RepoPathGet()) .
                                    "/archive/${strStanza}/out/${strArchiveFile}-4518a0fdf41d796760b384a358270d4682589820";

                                unlink ($strDuplicateWal)
                                        or confess "unable to remove duplicate WAL segment created for testing: ${strDuplicateWal}";
                            }
                        }

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

                    # !!! Need to put in tests for .backup files here
                }

                BackRestTestCommon_TestLogAppendFile($oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/archive.info', $bRemote);
            }
            }


            $bCreate = true;
        }

        if (BackRestTestCommon_Cleanup())
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

        &log(INFO, "Test ${strThisTest}\n");

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
                                            $iThreadMax == 1 ? $strThisTest: undef)) {next}

                # Create the file object
                if ($bCreate)
                {
                    $oFile = (new BackRest::File
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

                BackRestTestCommon_ConfigCreate('db',
                                                ($bRemote ? BACKUP : undef),
                                                $bCompress,
                                                undef,      # checksum
                                                undef,      # hardlink
                                                undef,      # thread-max
                                                true,       # archive-async
                                                undef);

                # Helper function to push archive logs
                sub archivePush
                {
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
                                     '/pg_backrest.conf --archive-max-mb=24 --no-fork --stanza=db archive-push' .
                                     (defined($iExpectedError) && $iExpectedError == ERROR_HOST_CONNECT ?
                                      "  --backup-host=bogus" : '');

                    BackRestTestCommon_Execute($strCommand . " ${strSourceFile}", undef, undef, undef,
                                               $iExpectedError);
                }

                # Push a WAL segment
                archivePush($oFile, $strXlogPath, $strArchiveTestFile, 1);

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
                archivePush($oFile, $strXlogPath, $strArchiveTestFile, 2,
                            $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

                archivePush($oFile, $strXlogPath, $strArchiveTestFile, 3,
                            $iError ? ERROR_HOST_CONNECT : ERROR_ARCHIVE_MISMATCH);

                # Now this segment will get dropped
                archivePush($oFile, $strXlogPath, $strArchiveTestFile, 4);

                # Fix the database version
                if ($iError == 0)
                {
                    $oInfo{&INFO_ARCHIVE_SECTION_DB}{&INFO_ARCHIVE_KEY_DB_VERSION} = '9.3';
                    BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);
                }

                # Remove the stop file
                my $strStopFile = ($bRemote ? BackRestTestCommon_LocalPathGet() : BackRestTestCommon_RepoPathGet()) .
                                  '/lock/db-archive.stop';

                unlink($strStopFile)
                    or die "unable to remove stop file ${strStopFile}";

                # Push two more segments - only #4 should be missing from the archive at the end
                archivePush($oFile, $strXlogPath, $strArchiveTestFile, 5);
                archivePush($oFile, $strXlogPath, $strArchiveTestFile, 6);
            }
            }

            $bCreate = true;
        }

        if (BackRestTestCommon_Cleanup())
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

        &log(INFO, "Test ${strThisTest}\n");

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
                                            $iThreadMax == 1 ? $strThisTest: undef)) {next}

                # Create the test directory
                if ($bCreate)
                {
                    # Create the file object
                    $oFile = (BackRest::File->new
                    (
                        $strStanza,
                        BackRestTestCommon_RepoPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : $oLocal
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);

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
                                 '/pg_backrest.conf --stanza=db archive-get';


                BackRestTestCommon_Execute($strCommand . " 000000010000000100000001 ${strXlogPath}/000000010000000100000001",
                                           undef, undef, undef, ERROR_FILE_MISSING);

                # Create the archive info file
                if ($bRemote)
                {
                    BackRestTestCommon_Execute("chmod g+r,g+x " . BackRestTestCommon_RepoPathGet(), $bRemote);
                }

                BackRestTestCommon_Execute('mkdir -p -m 770 ' . $oFile->pathGet(PATH_BACKUP_ARCHIVE), $bRemote);
                (new BackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE)))->check('9.3', 1234567890123456789);
                BackRestTestCommon_TestLogAppendFile($oFile->pathGet(PATH_BACKUP_ARCHIVE) . '/archive.info', $bRemote);

                if ($bRemote)
                {
                    BackRestTestCommon_Execute("chmod g-r,g-x " . BackRestTestCommon_RepoPathGet(), $bRemote);
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

                        BackRestTestCommon_Execute($strCommand . " ${strArchiveFile} ${strDestinationFile}");

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
                    BackRestTestCommon_Execute($strCommand . " 000000090000000900000009 ${strXlogPath}/RECOVERYXLOG",
                                               undef, undef, undef, 1);
                }

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
    # Test expire
    #-------------------------------------------------------------------------------------------------------------------------------
    $strThisTest = 'expire';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;
        my $oFile;

        &log(INFO, "Test ${strThisTest}\n");

        if (!BackRestTestCommon_Run(++$iRun,
                                    "local",
                                    $iThreadMax == 1 ? $strModule : undef,
                                    $iThreadMax == 1 ? $strThisTest: undef)) {next}

        # Create the file object
        $oFile = (BackRest::File->new
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
        my $oExpireTest = new BackRestTest::ExpireCommonTest($oFile);

        $oExpireTest->stanzaCreate($strStanza, '9.2');
        use constant SECONDS_PER_DAY => 86400;
        my $lBaseTime = time() - (SECONDS_PER_DAY * 56);

        #---------------------------------------------------------------------------------------------------------------------------
        my $strDescription = 'Nothing to expire';

        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, 246);

        $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

        #---------------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest full backup, archive expire falls on segment major boundary';

        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

        #---------------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest diff backup';

        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, 256);
        $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

        #---------------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest full backup, archive expire does not fall on major segment boundary';

        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, undef, 0);
        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, undef, 0);
        $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

        #---------------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest diff backup (cascade to incr)';

        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

        #---------------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire archive based on newest incr backup';

        $oExpireTest->backupCreate($strStanza, BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($strStanza, 1, 1, BACKUP_TYPE_INCR, 1, $strDescription);

        # Cleanup
        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
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

        &log(INFO, "Test ${strModule} - ${strThisTest}\n");

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
                                        $iThreadMax == 1 ? $strThisTest: undef)) {next}

            # Get base time
            my $lTime = time() - 100000;

            # Build the manifest
            my %oManifest;

            $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_VERSION} = BACKREST_VERSION;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_CHECK} = JSON::PP::true;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_COPY} = JSON::PP::true;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS} = $bCompress ? JSON::PP::true : JSON::PP::false;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK} = $bHardlink ? JSON::PP::true : JSON::PP::false;
            $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_START_STOP} = JSON::PP::false;

            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG} = 201306121;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CONTROL} = 937;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_SYSTEM_ID} = 6156904820763115222;
            $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} = '9.3';

            # Create the test directory
            BackRestTestBackup_Create($bRemote, false);

            $oManifest{'backup:path'}{base}{&MANIFEST_SUBKEY_PATH} = BackRestTestCommon_DbCommonPathGet();

            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base');

            # Create the file object
            my $oFile = new BackRest::File
            (
                $strStanza,
                BackRestTestCommon_RepoPathGet(),
                $bRemote ? 'backup' : undef,
                $bRemote ? $oRemote : $oLocal
            );

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'PG_VERSION', '9.3',
                                                  'e1f7a3a299f62225cba076fc6d3d6e677f303482', $lTime);

            # Create base path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'base');

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASE',
                                                  'a3b357a3e395e43fcfb19bb13f3c1b5179279593', $lTime);

            # Create global path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'global');

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'global/pg_control', '[replaceme]',
                                                  '56fe5780b8dca9705e0c22032a83828860a21235', $lTime - 100);
            BackRestTestCommon_Execute('cp ' . BackRestTestCommon_DataPathGet() . '/pg_control ' .
                                       BackRestTestCommon_DbCommonPathGet() . '/global/pg_control');
            utime($lTime - 100, $lTime - 100, BackRestTestCommon_DbCommonPathGet() . '/global/pg_control')
                or confess &log(ERROR, "unable to set time");
            $oManifest{'base:file'}{'global/pg_control'}{'size'} = 8192;

            # Create tablespace path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'pg_tblspc');

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

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            my $strType = 'full';

            BackRestTestBackup_ManifestLinkCreate(\%oManifest, 'base', 'link-test', '/test');
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'path-test');

            my $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                                   undef, undef, undef, undef, '--manifest-save-threshold=3');

            # Backup Info
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_Info($strStanza, undef, false);
            BackRestTestBackup_Info($strStanza, INFO_OUTPUT_JSON, false);

            # Resume Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'full';

            my $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strFullBackup}",
                                        $strTmpPath, $bRemote);

            my $oMungeManifest = BackRestTestCommon_manifestLoad("$strTmpPath/backup.manifest", $bRemote);
            $oMungeManifest->remove('base:file', 'PG_VERSION', 'checksum');
            BackRestTestCommon_manifestSave("$strTmpPath/backup.manifest", $oMungeManifest, $bRemote);

            $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                                'resume', TEST_BACKUP_RESUME);

            # Restore - tests various mode, extra files/paths, missing files/paths
            #-----------------------------------------------------------------------------------------------------------------------
            my $bDelta = true;
            my $bForce = false;

            # Create a path and file that are not in the manifest
            BackRestTestBackup_PathCreate(\%oManifest, 'base', 'deleteme');
            BackRestTestBackup_FileCreate(\%oManifest, 'base', 'deleteme/deleteme.txt', 'DELETEME');

            # Change path mode
            BackRestTestBackup_PathMode(\%oManifest, 'base', 'base', '0777');

            # Change an existing link to the wrong directory
            BackRestTestBackup_FileRemove(\%oManifest, 'base', 'link-test');
            BackRestTestBackup_LinkCreate(\%oManifest, 'base', 'link-test', '/wrong');

            # Remove an path
            BackRestTestBackup_PathRemove(\%oManifest, 'base', 'path-test');

            # Remove a file
            BackRestTestBackup_FileRemove(\%oManifest, 'base', 'PG_VERSION');
            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'add and delete files');

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

            my $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                               'invalid database version', undef, undef, ERROR_BACKUP_MISMATCH);
            $oInfo{db}{&MANIFEST_KEY_DB_VERSION} = $strDbVersion;

            # Break the database system id
            my $ullDbSysId = $oInfo{'db'}{&MANIFEST_KEY_SYSTEM_ID};
            $oInfo{db}{&MANIFEST_KEY_SYSTEM_ID} = 6999999999999999999;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'invalid system id', undef, undef, ERROR_BACKUP_MISMATCH);
            $oInfo{db}{&MANIFEST_KEY_SYSTEM_ID} = $ullDbSysId;

            # Break the control version
            my $iControlVersion = $oInfo{'db'}{&MANIFEST_KEY_CONTROL};
            $oInfo{db}{&MANIFEST_KEY_CONTROL} = 842;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'invalid control version', undef, undef, ERROR_BACKUP_MISMATCH);
            $oInfo{db}{&MANIFEST_KEY_CONTROL} = $iControlVersion;

            # Break the catalog version
            my $iCatalogVersion = $oInfo{'db'}{&MANIFEST_KEY_CATALOG};
            $oInfo{db}{&MANIFEST_KEY_CATALOG} = 197208141;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'invalid catalog version', undef, undef, ERROR_BACKUP_MISMATCH);

            # Fix up info file for next test
            $oInfo{db}{&MANIFEST_KEY_CATALOG} = $iCatalogVersion;
            BackRestTestCommon_iniSave($strInfoFile, \%oInfo, $bRemote, true);

            # Incr backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';

            # Add tablespace 1
            BackRestTestBackup_ManifestTablespaceCreate(\%oManifest, 1);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace/1", 'tablespace1.txt', 'TBLSPC1',
                                                  'd85de07d6421d90aa9191c11c889bfde43680f0f', $lTime);
            BackRestTestBackup_ManifestFileCreate(\%oManifest, "base", 'badchecksum.txt', 'BADCHECKSUM',
                                                  'f927212cd08d11a42a666b2f04235398e9ceeb51', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'add tablespace 1');

            # Resume Incr Backup
            #
            # Links are removed in the resume because it's easy to recreate them.
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';

            # Move database from backup to temp
            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            $oMungeManifest = BackRestTestCommon_manifestLoad("$strTmpPath/backup.manifest", $bRemote);
            $oMungeManifest->set('base:file', 'badchecksum.txt', 'checksum', 'bogus');
            BackRestTestCommon_manifestSave("$strTmpPath/backup.manifest", $oMungeManifest, $bRemote);

            # Add tablespace 2
            BackRestTestBackup_ManifestTablespaceCreate(\%oManifest, 2);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace/2", 'tablespace2.txt', 'TBLSPC2',
                                                  'dc7f76e43c46101b47acc55ae4d593a9e6983578', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'resume and add tablespace 2', TEST_BACKUP_RESUME);

            # Resume Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';

            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'cannot resume - new diff', TEST_BACKUP_NORESUME);

            # Resume Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';

            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'cannot resume - disabled', TEST_BACKUP_NORESUME, undef, undef,
                                                            '--no-resume');

            # Restore
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;

            # Fail on used path
            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'fail on used path', ERROR_RESTORE_PATH_NOT_EMPTY);
            # Fail on undef format
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, undef);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                      undef, undef, undef, undef, undef, undef,
                                      'fail on undef format', ERROR_FORMAT);

            # Fail on mismatch format
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, 0);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                      undef, undef, undef, undef, undef, undef,
                                      'fail on mismatch format', ERROR_FORMAT);

            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, INI_SECTION_BACKREST, INI_KEY_FORMAT, undef,
                                             BACKREST_FORMAT);

            # Remap the base path
            my %oRemapHash;
            $oRemapHash{base} = BackRestTestCommon_DbCommonPathGet(2);
            $oRemapHash{1} = BackRestTestCommon_DbTablespacePathGet(1, 2);
            $oRemapHash{2} = BackRestTestCommon_DbTablespacePathGet(2, 2);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, \%oRemapHash, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'remap all paths');

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base2.txt', 'BASE2',
                                                  '09b5e31766be1dba1ec27de82f975c1b6eea2a92', $lTime);

            BackRestTestBackup_ManifestTablespaceDrop(\%oManifest, 1, 2);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace/2", 'tablespace2b.txt', 'TBLSPC2B',
                                                  'e324463005236d83e6e54795dbddd20a74533bf3', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'add files and remove tablespace 2');

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASEUPDT',
                                                  '9a53d532e27785e681766c98516a5e93f096a501', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest, 'update files');

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest, 'no updates');

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, "remove files - but won't affect manifest",
                                           true, true, 1);
            BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD);

            BackRestTestBackup_FileRemove(\%oManifest, 'base', 'base/base1.txt');

            $strBackup = BackRestTestBackup_BackupEnd($strType, $strStanza, $oFile, $bRemote, undef, \%oManifest, true);

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strType = 'diff';

            BackRestTestBackup_ManifestFileRemove(\%oManifest, 'base', 'base/base1.txt');

            BackRestTestBackup_ManifestFileRemove(\%oManifest, "tablespace/2", 'tablespace2b.txt', true);
            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace/2", 'tablespace2c.txt', 'TBLSPC2C',
                                                  'ad7df329ab97a1e7d35f1ff0351c079319121836', $lTime);

            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, "remove files during backup", true, true, 1);
            BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace/2", 'tablespace2c.txt', 'TBLSPCBIGGER',
                                                  'dfcb8679956b734706cf87259d50c88f83e80e66', $lTime);

            BackRestTestBackup_ManifestFileRemove(\%oManifest, 'base', 'base/base2.txt', true);

            $strBackup = BackRestTestBackup_BackupEnd($strType, $strStanza, $oFile, $bRemote, undef, \%oManifest, true);

            # Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'full';
            BackRestTestBackup_ManifestReference(\%oManifest);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASEUPDT2',
                                                  '7579ada0808d7f98087a0a586d0df9de009cdc33', $lTime);

            $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest);

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base2.txt', 'BASE2UPDT',
                                                  'cafac3c59553f2cfde41ce2e62e7662295f108c0', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest, 'add files');

            # Restore
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'no tablespace remap', undef, '--no-tablespace', false);

            # Backup Info
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestCommon_Execute('mkdir ' . BackRestTestCommon_RepoPathGet . '/backup/db_empty', $bRemote);

            BackRestTestBackup_Info(undef, undef, false);
            BackRestTestBackup_Info(undef, INFO_OUTPUT_JSON, false);
            BackRestTestBackup_Info('bogus', undef, false);
            BackRestTestBackup_Info('bogus', INFO_OUTPUT_JSON, false);
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
    # Test full
    #
    # Check the entire backup mechanism using actual clusters.  Only the archive and start/stop mechanisms need to be tested since
    # everything else was tested in the backup test.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'full')
    {
        $iRun = 0;
        $bCreate = true;

        &log(INFO, "Test Full Backup\n");

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
            my $oFile = new BackRest::File
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

            # Static backup parameters
            my $bSynthetic = false;
            my $fTestDelay = .1;

            # Variable backup parameters
            my $bDelta = true;
            my $bForce = false;
            my $strType = undef;
            my $strTarget = undef;
            my $bTargetExclusive = false;
            my $bTargetResume = false;
            my $strTargetTimeline = undef;
            my $oRecoveryHashRef = undef;
            my $strTestPoint = undef;
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

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;
            $strTestPoint = TEST_MANIFEST_BUILD;

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
            BackRestTestBackup_Backup($strType, $strStanza, $bRemote, $oFile, $strComment, undef, undef, ERROR_LOCK_ACQUIRE);

            # Release the backup advisory lock so the next backup will succeed
            if (!BackRestTestBackup_PgSelectOne('select pg_advisory_unlock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to acquire advisory lock for testing';
            }

            $strComment = 'update during backup';
            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, $strComment, $bSynthetic,
                                           defined($strTestPoint), $fTestDelay);

            BackRestTestCommon_ExecuteEnd($strTestPoint);

            BackRestTestBackup_PgExecute("update test set message = '$strFullMessage'", false);

            my $strFullBackup = BackRestTestBackup_BackupEnd($strType, $strStanza, $oFile, $bRemote, undef, undef, $bSynthetic);

            # Setup the time target
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_PgExecute("update test set message = '$strTimeMessage'", false);
            BackRestTestBackup_PgSwitchXlog();
            my $strTimeTarget = BackRestTestBackup_PgSelectOne("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ')");
            &log(INFO, "        time target is ${strTimeTarget}");

            # Incr backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;
            $strTestPoint = TEST_MANIFEST_BUILD;

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
            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, $strComment, $bSynthetic,
                                           defined($strTestPoint), $fTestDelay, '--' . OPTION_STOP_AUTO);
            BackRestTestCommon_ExecuteEnd($strTestPoint);

            BackRestTestBackup_PgExecute("drop table test_remove", false);
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("update test set message = '$strIncrMessage'", false);

            my $strIncrBackup = BackRestTestBackup_BackupEnd($strType, $strStanza, $oFile, $bRemote, undef, undef, $bSynthetic);

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
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            # Expect failure because postmaster.pid exists
            $strComment = 'postmaster running';
            $iExpectedExitStatus = ERROR_POSTMASTER_RUNNING;

            BackRestTestBackup_Restore($oFile, 'latest', $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStop();

            # Expect failure because db path is not empty
            $strComment = 'path not empty';
            $iExpectedExitStatus = ERROR_RESTORE_PATH_NOT_EMPTY;

            BackRestTestBackup_Restore($oFile, 'latest', $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            # Drop and recreate db path
            BackRestTestCommon_PathRemove(BackRestTestCommon_DbCommonPathGet());
            BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet());
            BackRestTestCommon_PathRemove(BackRestTestCommon_DbTablespacePathGet(1));
            BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet(1));

            # Now the restore should work
            $strComment = undef;
            $iExpectedExitStatus = undef;

            BackRestTestBackup_Restore($oFile, 'latest', $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strNameMessage);

            # Restore (restore type = xid, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = true;
            $strType = RECOVERY_TYPE_XID;
            $strTarget = $strXidTarget;
            $bTargetExclusive = undef;
            $bTargetResume = BackRestTestCommon_DbVersion() >= 9.1 && BackRestTestCommon_DbVersion() < 9.5 ? true : undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strIncrBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus, '--no-tablespace', false);

            # Save recovery file to test so we can use it in the next test
            $oFile->copy(PATH_ABSOLUTE, BackRestTestCommon_DbCommonPathGet() . '/recovery.conf',
                         PATH_ABSOLUTE, BackRestTestCommon_TestPathGet() . '/recovery.conf');

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strXidMessage);

            BackRestTestBackup_PgExecute("update test set message = '$strTimelineMessage'", false);

            # Restore (restore type = preserve, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_PRESERVE;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            # Restore recovery file that was save in last test
            $oFile->move(PATH_ABSOLUTE, BackRestTestCommon_TestPathGet() . '/recovery.conf',
                         PATH_ABSOLUTE, BackRestTestCommon_DbCommonPathGet() . '/recovery.conf');

            BackRestTestBackup_Restore($oFile, 'latest', $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
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
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
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
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strIncrBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
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
                $bTargetResume = undef;
                $strTargetTimeline = undef;
                $oRecoveryHashRef = undef;
                $strComment = undef;
                $iExpectedExitStatus = undef;

                &log(INFO, "    testing recovery type = ${strType}");

                BackRestTestBackup_ClusterStop();

                BackRestTestBackup_Restore($oFile, 'latest', $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                           $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
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
                $bTargetResume = undef;
                $strTargetTimeline = 3;
                $oRecoveryHashRef = BackRestTestCommon_DbVersion() >= 9.0 ? {'standby-mode' => 'on'} : undef;
                $strComment = undef;
                $iExpectedExitStatus = undef;

                &log(INFO, "    testing recovery type = ${strType}");

                BackRestTestBackup_ClusterStop();

                BackRestTestBackup_Restore($oFile, $strIncrBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                           $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                           $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

                BackRestTestBackup_ClusterStart(undef, undef, true);
                BackRestTestBackup_PgSelectOneTest('select message from test', $strTimelineMessage, 120);
            }

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

        &log(INFO, "Test Backup Collision\n");

        # Create the file object
        my $oFile = (BackRest::File->new
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

        &log(INFO, "Test Rsync Collision\n");

        # Create the file object
        my $oFile = (BackRest::File->new
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
            BackRestTestCommon_Execute($strCommand, false, false, true);

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
            BackRestTestCommon_Execute($strCommand, false, false, true);

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
