####################################################################################################################################
# Test All Commands On PostgreSQL Clusters
####################################################################################################################################
package pgBackRestTest::Module::Real::RealAllTest;
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
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Storage;
use pgBackRestTest::Common::StoragePosix;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::Wait;
use pgBackRestTest::Env::ArchiveInfo;
use pgBackRestTest::Env::BackupInfo;
use pgBackRestTest::Env::InfoCommon;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Env::Manifest;

####################################################################################################################################
# Backup advisory lock
####################################################################################################################################
use constant DB_BACKUP_ADVISORY_LOCK                                => '12340078987004321';

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Should the test use lz4 compression?
    my $bLz4Compress = true;

    foreach my $strStorage (POSIX, S3)
    {
    foreach my $bHostBackup ($strStorage eq S3 ? (true) : (false, true))
    {
    # Standby should only be tested for pg versions that support it
    foreach my $bHostStandby ($strStorage eq S3 ? (false) : (false, true))
    {
    # Master and standby backup destinations on need to be tested on one db version since it is not version specific
    foreach my $strBackupDestination (
        $strStorage eq S3 || $bHostBackup ? (HOST_BACKUP) : $bHostStandby ? (HOST_DB_MASTER, HOST_DB_STANDBY) : (HOST_DB_MASTER))
    {
        my $strCompressType =
            $bHostBackup && !$bHostStandby ?
                (vmWithLz4($self->vm()) && $bLz4Compress ? LZ4 : vmWithZst($self->vm()) ?
                    ZST : ($strStorage eq S3 ? BZ2 : GZ)) : NONE;
        my $bRepoEncrypt = ($strCompressType ne NONE && $strStorage eq POSIX) ? true : false;

        # If compression was used then switch it for the next test that uses compression
        if ($strCompressType ne NONE)
        {
            $bLz4Compress = !$bLz4Compress;
        }

        # Increment the run, log, and decide whether this unit test should be run
        my $hyVm = vmGet();
        my $strDbVersionMostRecent = ${$hyVm->{$self->vm()}{&VM_DB_TEST}}[-1];

        next if (!$self->begin(
            "bkp ${bHostBackup}, sby ${bHostStandby}, dst ${strBackupDestination}, cmp ${strCompressType}" .
                ", storage ${strStorage}, enc ${bRepoEncrypt}",
            # Use the most recent db version on the expect vm for expect testing
            $self->vm() eq VM_EXPECT && $self->pgVersion() eq $strDbVersionMostRecent));

        # Skip when s3 and host backup tests when there is more than one version of pg being tested and this is not the last one
        if (($strStorage eq S3 || $bHostBackup) &&
            (@{$hyVm->{$self->vm()}{&VM_DB_TEST}} > 1 && $strDbVersionMostRecent ne $self->pgVersion()))
        {
            &log(INFO, "skipped - this test is run this OS using PG ${strDbVersionMostRecent}");
            next;
        }

        # Skip hot standby tests if the system does not support hot standby
        if ($bHostStandby && $self->pgVersion() < PG_VERSION_HOT_STANDBY)
        {
            &log(INFO, 'skipped - this version of PostgreSQL does not support hot standby');
            next;
        }

        # Skip backup destinations other than backup host when standby except for one arbitrary db version
        if ($bHostStandby && $strBackupDestination ne HOST_BACKUP && $self->pgVersion() ne PG_VERSION_96)
        {
            &log(INFO, 'skipped - standby with backup destination other than backup host is tested on PG ' . PG_VERSION_96);
            next;
        }

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup) = $self->setup(
            false, $self->expect(),
            {bHostBackup => $bHostBackup, bStandby => $bHostStandby, strBackupDestination => $strBackupDestination,
             strCompressType => $strCompressType, bArchiveAsync => false, strStorage => $strStorage,
             bRepoEncrypt => $bRepoEncrypt});

        # Only perform extra tests on certain runs to save time
        my $bTestLocal = $self->runCurrent() == 1;
        my $bTestExtra =
            $bTestLocal || $self->runCurrent() == 4 || ($self->runCurrent() == 6 && $self->pgVersion() eq PG_VERSION_96);

        # If S3 set process max to 2.  This seems like the best place for parallel testing since it will help speed S3 processing
        # without slowing down the other tests too much.
        if ($strStorage eq S3)
        {
            $oHostBackup->configUpdate({&CFGDEF_SECTION_GLOBAL => {'process-max' => 2}});
            $oHostDbMaster->configUpdate({&CFGDEF_SECTION_GLOBAL => {'process-max' => 2}});
        }

        $oHostDbMaster->clusterCreate();

        # Create the stanza
        $oHostBackup->stanzaCreate('main create stanza info files');

        # Get passphrase to access the Manifest file from backup.info - returns undefined if repo not encrypted
        my $strCipherPass =
            (new pgBackRestTest::Env::BackupInfo($oHostBackup->repoBackupPath()))->cipherPassSub();

        # Create a manifest with the pg version to get version-specific paths
        my $oManifest = new pgBackRestTest::Env::Manifest(BOGUS, {bLoad => false, strDbVersion => $self->pgVersion(),
            iDbCatalogVersion => $self->dbCatalogVersion($self->pgVersion()),
            strCipherPass => $strCipherPass, strCipherPassSub => $bRepoEncrypt ? ENCRYPTION_KEY_BACKUPSET : undef});

        # Static backup parameters
        my $fTestDelay = 1;

        # Restore test string
        my $strDefaultMessage = 'default';
        my $strFullMessage = 'full';
        my $strStandbyMessage = 'standby';
        my $strIncrMessage = 'incr';
        my $strTimeMessage = 'time';
        my $strXidMessage = 'xid';
        my $strNameMessage = 'name';
        my $strTimelineMessage = 'timeline';

        # Create two new databases
        if ($bTestLocal)
        {
            $oHostDbMaster->sqlExecute('create database test1', {bAutoCommit => true});
            $oHostDbMaster->sqlExecute('create database test2', {bAutoCommit => true});
        }

        # Test check command and stanza create
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra)
        {
            # In this section the same comment can be used multiple times so make it a variable that can be set once and reused
            my $strComment = undef;

            # Archive and backup info file names
            my $strArchiveInfoFile = $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE);
            my $strArchiveInfoCopyFile = $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE . INI_COPY_EXT);
            my $strArchiveInfoOldFile = "${strArchiveInfoFile}.old";
            my $strArchiveInfoCopyOldFile = "${strArchiveInfoCopyFile}.old";

            my $strBackupInfoFile = $oHostBackup->repoBackupPath(FILE_BACKUP_INFO);
            my $strBackupInfoCopyFile = $oHostBackup->repoBackupPath(FILE_BACKUP_INFO . INI_COPY_EXT);
            my $strBackupInfoOldFile = "${strBackupInfoFile}.old";
            my $strBackupInfoCopyOldFile = "${strBackupInfoCopyFile}.old";

            # Move the archive.info files to simulate missing file
            forceStorageMove(storageRepo(), $strArchiveInfoFile, $strArchiveInfoOldFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strArchiveInfoCopyFile, $strArchiveInfoCopyOldFile, {bRecurse => false});

            $oHostDbMaster->check(
                'fail on missing archive.info file',
                {iTimeout => 0.1, iExpectedExitStatus => ERROR_FILE_MISSING});

            # Backup.info was created earlier so restore archive info files
            forceStorageMove(storageRepo(), $strArchiveInfoOldFile, $strArchiveInfoFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strArchiveInfoCopyOldFile, $strArchiveInfoCopyFile, {bRecurse => false});

            # Check ERROR_ARCHIVE_DISABLED error
            $strComment = 'fail on archive_mode=off';
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true, bArchiveEnabled => false});

            $oHostBackup->backup(CFGOPTVAL_BACKUP_TYPE_FULL, $strComment, {iExpectedExitStatus => ERROR_ARCHIVE_DISABLED});
            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_DISABLED});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_DISABLED});
            }

            # Check ERROR_ARCHIVE_COMMAND_INVALID error
            $strComment = 'fail on invalid archive_command';
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true, bArchive => false});

            $oHostBackup->backup(CFGOPTVAL_BACKUP_TYPE_FULL, $strComment, {iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});
            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});
            }

            # When archive-check=n then ERROR_ARCHIVE_TIMEOUT will be raised instead of ERROR_ARCHIVE_COMMAND_INVALID
            # ??? But maybe we should error with the fact that that option is not valid
            $strComment = 'fail on archive timeout when archive-check=n';
            $oHostDbMaster->check(
                $strComment,
                {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_TIMEOUT, strOptionalParam => '--no-archive-check'});

            # Stop the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterStop({bIgnoreLogError => true});

            # Providing a sufficient archive-timeout, verify that the check command runs successfully.
            $strComment = 'verify success';

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->check($strComment, {iTimeout => 5});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 5});
            }

            # Check archive mismatch due to upgrade error
            $strComment = 'fail on archive mismatch after upgrade';

            # load the archive info file and munge it for testing by breaking the database version
            $oHostBackup->infoMunge(
                $oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'},
                 &INFO_ARCHIVE_SECTION_DB_HISTORY => {1 => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}}});

            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_FILE_INVALID});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_FILE_INVALID});
            }

            # Restore the file to its original condition
            $oHostBackup->infoRestore($oHostBackup->repoArchivePath(ARCHIVE_INFO_FILE));

            # Check archive_timeout error when WAL segment is not found
            $strComment = 'fail on archive timeout';

            $oHostDbMaster->clusterRestart({bIgnoreLogError => true, bArchiveInvalid => true});
            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_TIMEOUT});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_TIMEOUT});
            }

            # Restart the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true});

            # With a valid archive info, create the backup.info file by running a backup then munge the backup.info file.
            # Check backup mismatch error
            $strComment = 'fail on backup info mismatch';

            # Load the backup.info file and munge it for testing by breaking the database version and system id
            $oHostBackup->infoMunge(
                $oHostBackup->repoBackupPath(FILE_BACKUP_INFO),
                {&INFO_BACKUP_SECTION_DB =>
                    {&INFO_BACKUP_KEY_DB_VERSION => '8.0', &INFO_BACKUP_KEY_SYSTEM_ID => 6999999999999999999},
                &INFO_BACKUP_SECTION_DB_HISTORY =>
                    {1 => {&INFO_BACKUP_KEY_DB_VERSION => '8.0', &INFO_BACKUP_KEY_SYSTEM_ID => 6999999999999999999}}});

            # Run the test
            $oHostDbMaster->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_FILE_INVALID});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_FILE_INVALID});
            }

            # Restore the file to its original condition
            $oHostBackup->infoRestore($oHostBackup->repoBackupPath(FILE_BACKUP_INFO));

            # ??? Removed temporarily until manifest build can be brought back into the check command
            # Create a directory in pg_data location that is only readable by root to ensure manifest->build is called by check
            # my $strDir = $oHostDbMaster->dbBasePath() . '/rootreaddir';
            # executeTest('sudo mkdir ' . $strDir);
            # executeTest("sudo chown root:root ${strDir}");
            # executeTest("sudo chmod 400 ${strDir}");
            #
            # $strComment = 'confirm master manifest->build executed';
            # $oHostDbMaster->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_PATH_OPEN});
            # executeTest("sudo rmdir ${strDir}");

            # Providing a sufficient archive-timeout, verify that the check command runs successfully now with valid
            # archive.info and backup.info files
            $strComment = 'verify success after backup';

            $oHostDbMaster->check($strComment, {iTimeout => 5});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 5});
            }

            # Restart the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true});

            # Stanza Create
            #-----------------------------------------------------------------------------------------------------------------------
            # With data existing in the archive and backup directory, move info files and confirm failure
            forceStorageMove(storageRepo(), $strArchiveInfoFile, $strArchiveInfoOldFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strArchiveInfoCopyFile, $strArchiveInfoCopyOldFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strBackupInfoFile, $strBackupInfoOldFile, {bRecurse => false});
            forceStorageMove(storageRepo(), $strBackupInfoCopyFile, $strBackupInfoCopyOldFile, {bRecurse => false});

            $oHostBackup->stanzaCreate(
                'fail on backup info file missing from non-empty dir', {iExpectedExitStatus => ERROR_PATH_NOT_EMPTY});

            # Change the database version by copying a new pg_control file to a new pg-path to use for db mismatch test
            storageTest()->pathCreate(
                $oHostDbMaster->dbPath() . '/testbase/' . DB_PATH_GLOBAL,
                {strMode => '0700', bIgnoreExists => true, bCreateParent => true});
            $self->controlGenerate(
                $oHostDbMaster->dbPath() . '/testbase', $self->pgVersion() eq PG_VERSION_94 ? PG_VERSION_95 : PG_VERSION_94);

            # Run stanza-create online to confirm proper handling of configValidation error against new pg-path
            $oHostBackup->stanzaCreate('fail on database mismatch with directory',
                {strOptionalParam => ' --pg1-path=' . $oHostDbMaster->dbPath() . '/testbase/',
                    iExpectedExitStatus => ERROR_DB_MISMATCH});

            # Remove the directories to be able to create the stanza
            forceStorageRemove(storageRepo(), $oHostBackup->repoBackupPath(), {bRecurse => true});
            forceStorageRemove(storageRepo(), $oHostBackup->repoArchivePath(), {bRecurse => true});

            # Stanza Upgrade - tests configValidate code - all other tests in synthetic integration tests
            #-----------------------------------------------------------------------------------------------------------------------
            # Run stanza-create offline to create files needing to be upgraded (using new pg-path)
            $oHostBackup->stanzaCreate('successfully create stanza files to be upgraded',
                {strOptionalParam => ' --pg1-path=' . $oHostDbMaster->dbPath() . '/testbase/ --no-online --force'});
            my $oArchiveInfo = new pgBackRestTest::Env::ArchiveInfo($oHostBackup->repoArchivePath());
            my $oBackupInfo = new pgBackRestTest::Env::BackupInfo($oHostBackup->repoBackupPath());

            # Read info files to confirm the files were created with a different database version
            if ($self->pgVersion() eq PG_VERSION_94)
            {
                $self->testResult(sub {$oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                    PG_VERSION_95)}, true, 'archive upgrade forced with pg mismatch');
                $self->testResult(sub {$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef,
                    PG_VERSION_95)}, true, 'backup upgrade forced with pg mismatch');
            }
            else
            {
                $self->testResult(sub {$oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                    PG_VERSION_94)}, true, 'archive create forced with pg mismatch in prep for stanza-upgrade');
                $self->testResult(sub {$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef,
                    PG_VERSION_94)}, true, 'backup create forced with pg mismatch in prep for stanza-upgrade');
            }

            # Run stanza-upgrade online with the default pg-path to correct the info files
            $oHostBackup->stanzaUpgrade('upgrade stanza files online');

            # Reread the info files and confirm the result
            $oArchiveInfo = new pgBackRestTest::Env::ArchiveInfo($oHostBackup->repoArchivePath());
            $oBackupInfo = new pgBackRestTest::Env::BackupInfo($oHostBackup->repoBackupPath());
            $self->testResult(sub {$oArchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                $self->pgVersion())}, true, 'archive upgrade online corrects db');
            $self->testResult(sub {$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef,
                $self->pgVersion())}, true, 'backup upgrade online corrects db');
        }

        # Full backup
        #---------------------------------------------------------------------------------------------------------------------------
        # Create the table where test messages will be stored
        $oHostDbMaster->sqlExecute("create table test (message text not null)");
        $oHostDbMaster->sqlWalRotate();
        $oHostDbMaster->sqlExecute("insert into test values ('$strDefaultMessage')");

        if ($bTestLocal)
        {
            # Acquire the backup advisory lock so it looks like a backup is running
            if (!$oHostDbMaster->sqlSelectOne('select pg_try_advisory_lock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to acquire advisory lock for testing';
            }

            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_FULL, 'fail on backup lock exists', {iExpectedExitStatus => ERROR_LOCK_ACQUIRE});

            # Release the backup advisory lock so the next backup will succeed
            if (!$oHostDbMaster->sqlSelectOne('select pg_advisory_unlock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to release advisory lock';
            }
        }

        $oHostDbMaster->sqlExecute("update test set message = '$strFullMessage'");

        # Required to set hint bits to be sent to the standby to make the heap match on both sides
        $oHostDbMaster->sqlSelectOneTest('select message from test', $strFullMessage);

        my $strFullBackup = $oHostBackup->backup(
            CFGOPTVAL_BACKUP_TYPE_FULL, 'update during backup',
            {strOptionalParam => ' --buffer-size=16384'});

        # Enabled async archiving
        $oHostBackup->configUpdate({&CFGDEF_SECTION_GLOBAL => {'archive-async' => 'y'}});

        # Kick out a bunch of archive logs to exercise async archiving.  Only do this when compressed and remote to slow it
        # down enough to make it evident that the async process is working.
        if ($bTestExtra && $strCompressType ne NONE && $strBackupDestination eq HOST_BACKUP)
        {
            &log(INFO, '    multiple wal switches to exercise async archiving');
            $oHostDbMaster->sqlExecute("create table wal_activity (id int)");
            $oHostDbMaster->sqlWalRotate();
            $oHostDbMaster->sqlExecute("insert into wal_activity values (1)");
            $oHostDbMaster->sqlWalRotate();
            $oHostDbMaster->sqlExecute("insert into wal_activity values (2)");
            $oHostDbMaster->sqlWalRotate();
            $oHostDbMaster->sqlExecute("insert into wal_activity values (3)");
            $oHostDbMaster->sqlWalRotate();
            $oHostDbMaster->sqlExecute("insert into wal_activity values (4)");
            $oHostDbMaster->sqlWalRotate();
        }

        # Setup replica
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bHostStandby)
        {
            my %oRemapHash;
            $oRemapHash{&MANIFEST_TARGET_PGDATA} = $oHostDbStandby->dbBasePath();

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_92)
            {
                $oHostDbStandby->linkRemap($oManifest->walPath(), $oHostDbStandby->dbPath() . '/' . $oManifest->walPath());
            }

            $oHostDbStandby->restore(
                'restore backup on replica', 'latest',
                {rhRemapHash => \%oRemapHash, strType => CFGOPTVAL_RESTORE_TYPE_STANDBY,
                    strOptionalParam =>
                        ' --recovery-option="primary_conninfo=host=' . HOST_DB_MASTER .
                        ' port=' . $oHostDbMaster->pgPort() . ' user=replicator"'});

            $oHostDbStandby->clusterStart({bHotStandby => true});

            # Make sure streaming replication is on
            $oHostDbMaster->sqlSelectOneTest(
                "select client_addr || '-' || state from pg_stat_replication", $oHostDbStandby->ipGet() . '/32-streaming');

            # Check that the cluster was restored properly
            $oHostDbStandby->sqlSelectOneTest('select message from test', $strFullMessage);

            # Update message for standby
            $oHostDbMaster->sqlExecute("update test set message = '$strStandbyMessage'");

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY)
            {
                # If there is only a master and a replica and the replica is the backup destination, then if pg2-host and pg3-host
                # are BOGUS, confirm failure to reach the master
                if (!$bHostBackup && $bHostStandby && $strBackupDestination eq HOST_DB_STANDBY)
                {
                    my $strStandbyBackup = $oHostBackup->backup(
                        CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby, failure to reach master',
                        {bStandby => true, iExpectedExitStatus => ERROR_DB_CONNECT, strOptionalParam => '--pg8-host=' . BOGUS});
                }
                else
                {
                    my $strStandbyBackup = $oHostBackup->backup(
                        CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby, failure to access at least one standby',
                        {bStandby => true, iExpectedExitStatus => ERROR_DB_CONNECT, strOptionalParam => '--pg8-host=' . BOGUS});
                }
            }

            my $strStandbyBackup = $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby',
                {bStandby => true,
                 iExpectedExitStatus => $oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY ? undef : ERROR_CONFIG,
                 strOptionalParam => '--repo1-retention-full=1'});

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY)
            {
                $strFullBackup = $strStandbyBackup;
            }

            # ??? Removed temporarily until manifest build can be brought back into the check command
            # # Create a directory in pg_data location that is only readable by root to ensure manifest->build is called by check
            # my $strDir = $oHostDbStandby->dbBasePath() . '/rootreaddir';
            # executeTest('sudo mkdir ' . $strDir);
            # executeTest("sudo chown root:root ${strDir}");
            # executeTest("sudo chmod 400 ${strDir}");
            #
            # my $strComment = 'confirm standby manifest->build executed';
            #
            # # If there is an invalid host, the final error returned from check will be the inability to resolve the name which is
            # # an open error instead of a read error
            # if (!$oHostDbStandby->bogusHost())
            # {
            #     $oHostDbStandby->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_PATH_OPEN});
            # }
            # else
            # {
            #     $oHostDbStandby->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_FILE_READ});
            # }
            #
            # # Remove the directory in pg_data location that is only readable by root
            # executeTest("sudo rmdir ${strDir}");

            # Confirm the check command runs without error on a standby (when a bogus host is not configured)
            if (!$oHostDbStandby->bogusHost())
            {
                $oHostDbStandby->check('verify check command on standby');
            }

            # Shutdown the standby before creating tablespaces (this will error since paths are different)
            $oHostDbStandby->clusterStop({bIgnoreLogError => true});
        }

        my $strAdhocBackup;

        # Execute stop and make sure the backup fails
        #---------------------------------------------------------------------------------------------------------------------------
        # Restart the cluster to check for any errors before continuing since the stop tests will definitely create errors and
        # the logs will to be deleted to avoid causing issues further down the line.
        if ($bTestExtra && $strStorage eq POSIX)
        {
            $oHostDbMaster->clusterRestart();

            # Add backup for adhoc expire
            $strAdhocBackup = $oHostBackup->backup(CFGOPTVAL_BACKUP_TYPE_DIFF, 'backup for adhoc expire');

            $oHostDbMaster->stop();

            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'attempt backup when stopped',
                {iExpectedExitStatus => $oHostBackup == $oHostDbMaster ? ERROR_STOP : ERROR_DB_CONNECT});

            $oHostDbMaster->start();
        }

        # Setup the time targets
        #---------------------------------------------------------------------------------------------------------------------------
        # If the tests are running quickly then the time target might end up the same as the end time of the prior full backup. That
        # means restore auto-select will not pick it as a candidate and restore the last backup instead causing the restore compare
        # to fail. So, sleep one second.
        sleep(1);

        $oHostDbMaster->sqlExecute("update test set message = '$strTimeMessage'");
        $oHostDbMaster->sqlWalRotate();
        my $strTimeTarget = $oHostDbMaster->sqlSelectOne("select current_timestamp");
        &log(INFO, "        time target is ${strTimeTarget}");

        # Incr backup - fail on archive_mode=always when version >= 9.5
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_95)
        {
            # Set archive_mode=always
            $oHostDbMaster->clusterRestart({bArchiveAlways => true});

            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'fail on archive_mode=always', {iExpectedExitStatus => ERROR_FEATURE_NOT_SUPPORTED});

            # Reset the cluster to a normal state so the next test will work
            $oHostDbMaster->clusterRestart();
        }

        # Incr backup
        #---------------------------------------------------------------------------------------------------------------------------
        # Create a tablespace directory
        storageTest()->pathCreate($oHostDbMaster->tablespacePath(1), {strMode => '0700', bCreateParent => true});

        # Also create it on the standby so replay won't fail
        if (defined($oHostDbStandby))
        {
            storageTest()->pathCreate($oHostDbStandby->tablespacePath(1), {strMode => '0700', bCreateParent => true});
        }

        $oHostDbMaster->sqlExecute(
            "create tablespace ts1 location '" . $oHostDbMaster->tablespacePath(1) . "'", {bAutoCommit => true});
        $oHostDbMaster->sqlExecute("alter table test set tablespace ts1");

        # Create a table in the tablespace that will not be modified again to be sure it does get full page writes in the WAL later
        $oHostDbMaster->sqlExecute("create table test_exists (id int) tablespace ts1", {bCommit => true, bCheckPoint => true});

        # Create a table in the tablespace
        $oHostDbMaster->sqlExecute("create table test_remove (id int)");
        $oHostDbMaster->sqlWalRotate();
        $oHostDbMaster->sqlExecute("update test set message = '$strDefaultMessage'");
        $oHostDbMaster->sqlWalRotate();

        # Create a database in the tablespace and a table to check
        $oHostDbMaster->sqlExecute("create database test3 with tablespace ts1", {bAutoCommit => true});
        $oHostDbMaster->sqlExecute(
            'create table test3_exists (id int);' .
            'insert into test3_exists values (1);',
            {strDb => 'test3', bAutoCommit => true});

        if ($bTestLocal)
        {
            # Create a table in test1 to check - test1 will not be restored
            $oHostDbMaster->sqlExecute(
                'create table test1_zeroed (id int);' .
                'insert into test1_zeroed values (1);',
                {strDb => 'test1', bAutoCommit => true});
        }

        # Start a backup so the next backup has to restart it.  This test is not required for PostgreSQL >= 9.6 since backups
        # are run in non-exclusive mode.
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_93 && $oHostDbMaster->pgVersion() < PG_VERSION_96)
        {
            $oHostDbMaster->sqlSelectOne("select pg_start_backup('test backup that will cause an error', true)");

            # Verify that an error is returned if the backup is already running
            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'fail on backup already running', {iExpectedExitStatus => ERROR_DB_QUERY});

            # Restart the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true});

            # Start a new backup to make the next test restart it
            $oHostDbMaster->sqlSelectOne("select pg_start_backup('test backup that will be restarted', true)");
        }

        if (defined($strAdhocBackup))
        {
            # Adhoc expire the latest backup - no other tests should be affected
            $oHostBackup->expire({strOptionalParam => '--set=' . $strAdhocBackup});
        }

        # Drop a table
        $oHostDbMaster->sqlExecute('drop table test_remove');
        $oHostDbMaster->sqlWalRotate();
        $oHostDbMaster->sqlExecute("update test set message = '$strIncrMessage'", {bCommit => true});

        # Exercise --delta checksum option
        my $strIncrBackup = $oHostBackup->backup(
            CFGOPTVAL_BACKUP_TYPE_INCR, 'update during backup', {strOptionalParam => '--stop-auto --buffer-size=32768 --delta'});

        # Ensure the check command runs properly with a tablespace unless there is a bogus host
        if (!$oHostBackup->bogusHost())
        {
            $oHostBackup->check( 'check command with tablespace', {iTimeout => 5});
        }

        # Setup the xid target
        #---------------------------------------------------------------------------------------------------------------------------
        my $strXidTarget = undef;

        if ($bTestLocal)
        {
            $oHostDbMaster->sqlExecute("update test set message = '$strXidMessage'", {bCommit => false});
            $oHostDbMaster->sqlWalRotate();
            $strXidTarget = $oHostDbMaster->sqlSelectOne("select txid_current()");
            $oHostDbMaster->sqlCommit();
            &log(INFO, "        xid target is ${strXidTarget}");
        }

        # Setup the name target
        #---------------------------------------------------------------------------------------------------------------------------
        my $strNameTarget = 'backrest';

        if ($bTestLocal)
        {
            $oHostDbMaster->sqlExecute("update test set message = '$strNameMessage'", {bCommit => true});
            $oHostDbMaster->sqlWalRotate();

            if ($oHostDbMaster->pgVersion() >= PG_VERSION_91)
            {
                $oHostDbMaster->sqlExecute("select pg_create_restore_point('${strNameTarget}')");
            }

            &log(INFO, "        name target is ${strNameTarget}");
        }

        # Create a table and data in database test2
        #---------------------------------------------------------------------------------------------------------------------------

        # Initialize variables for SHA1 and path of the pg_filenode.map for the database that will not be restored
        my $strDb1TablePath;
        my $strDb1TableSha1;

        if ($bTestLocal)
        {
            $oHostDbMaster->sqlExecute(
                'create table test (id int);' .
                'insert into test values (1);' .
                'create table test_ts1 (id int) tablespace ts1;' .
                'insert into test_ts1 values (2);',
                {strDb => 'test2', bAutoCommit => true});

            $oHostDbMaster->sqlWalRotate();

            # Get the SHA1 and path of the table for the database that will not be restored
            $strDb1TablePath =  $oHostDbMaster->dbBasePath(). "/base/" .
                $oHostDbMaster->sqlSelectOne("select oid from pg_database where datname='test1'") . "/" .
                $oHostDbMaster->sqlSelectOne("select relfilenode from pg_class where relname='test1_zeroed'", {strDb => 'test1'});
            $strDb1TableSha1 = storageTest()->hashSize($strDb1TablePath);
        }

        # Restore (type = default)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal)
        {
            # Expect failure because postmaster.pid exists
            $oHostDbMaster->restore('postmaster running', 'latest', {iExpectedExitStatus => ERROR_POSTMASTER_RUNNING});
        }

        $oHostDbMaster->clusterStop();

        if ($bTestLocal)
        {
            # Expect failure because db path is not empty
            $oHostDbMaster->restore('path not empty', 'latest', {iExpectedExitStatus => ERROR_PATH_NOT_EMPTY});
        }

        # Drop and recreate db path
        testPathRemove($oHostDbMaster->dbBasePath());
        storageTest()->pathCreate($oHostDbMaster->dbBasePath(), {strMode => '0700'});
        testPathRemove($oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath());
        storageTest()->pathCreate($oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath(), {strMode => '0700'});
        testPathRemove($oHostDbMaster->tablespacePath(1));
        storageTest()->pathCreate($oHostDbMaster->tablespacePath(1), {strMode => '0700'});

        # Now the restore should work
        $oHostDbMaster->restore(
            undef, 'latest',
            {strOptionalParam => ($bTestLocal ? ' --db-include=test2 --db-include=test3' : '') . ' --buffer-size=16384'});

        # Test that the first database has not been restored since --db-include did not include test1
        if ($bTestLocal)
        {
            my ($strSHA1, $lSize) = storageTest()->hashSize($strDb1TablePath);

            # Create a zeroed sparse file in the test directory that is the same size as the filenode.map.  We need to use the
            # posix driver directly to do this because handles cannot be passed back from the C code.
            my $oStorageTrunc = new pgBackRestTest::Common::Storage($self->testPath(), new pgBackRestTest::Common::StoragePosix());

            my $strTestTable = $self->testPath() . "/testtable";
            my $oDestinationFileIo = $oStorageTrunc->openWrite($strTestTable);
            $oDestinationFileIo->open();

            # Truncate to the original size which will create a sparse file.
            if (!truncate($oDestinationFileIo->handle(), $lSize))
            {
                confess "unable to truncate '$strTestTable' with handle " . $oDestinationFileIo->handle();
            }
            $oDestinationFileIo->close();

            # Confirm the test filenode.map and the database test1 filenode.map are zeroed
            my ($strSHA1Test, $lSizeTest) = storageTest()->hashSize($strTestTable);
            $self->testResult(sub {($strSHA1Test eq $strSHA1) && ($lSizeTest == $lSize) && ($strSHA1 ne $strDb1TableSha1)},
                true, 'database test1 not restored');
        }

        $oHostDbMaster->clusterStart();
        $oHostDbMaster->sqlSelectOneTest('select message from test', $bTestLocal ? $strNameMessage : $strIncrMessage);

        # Once the cluster is back online, make sure the database & table in the tablespace exists properly
        if ($bTestLocal)
        {
            $oHostDbMaster->sqlSelectOneTest('select id from test_ts1', 2, {strDb => 'test2'});
            $oHostDbMaster->sqlDisconnect({strDb => 'test2'});

            $oHostDbMaster->sqlSelectOneTest('select id from test3_exists', 1, {strDb => 'test3'});
            $oHostDbMaster->sqlDisconnect({strDb => 'test3'});
        }

        # The tablespace path should exist and have files in it
        my $strTablespacePath = $oHostDbMaster->tablespacePath(1);

        # Version <= 8.4 always places a PG_VERSION file in the tablespace
        if ($oHostDbMaster->pgVersion() <= PG_VERSION_84)
        {
            if (!storageTest()->exists("${strTablespacePath}/" . DB_FILE_PGVERSION))
            {
                confess &log(ASSERT, "unable to find '" . DB_FILE_PGVERSION . "' in tablespace path '${strTablespacePath}'");
            }
        }
        # Version >= 9.0 creates a special path using the version and catalog number
        else
        {
            # Backup info will have the catalog number
            my $oBackupInfo = new pgBackRestDoc::Common::Ini(
                storageRepo(), $oHostBackup->repoBackupPath(FILE_BACKUP_INFO),
                {bLoad => false, strContent => ${storageRepo()->get($oHostBackup->repoBackupPath(FILE_BACKUP_INFO))}});

            # Construct the special path
            $strTablespacePath .=
                '/PG_' . $oHostDbMaster->pgVersion() . qw{_} . $oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG);

            # Check that path exists
            if (!storageTest()->pathExists($strTablespacePath))
            {
                confess &log(ASSERT, "unable to find tablespace path '${strTablespacePath}'");
            }
        }

        # Make sure there are some files in the tablespace path (exclude PG_VERSION if <= 8.4 since that was tested above)
        if (grep(!/^PG\_VERSION$/i, storageTest()->list($strTablespacePath)) == 0)
        {
            confess &log(ASSERT, "no files found in tablespace path '${strTablespacePath}'");
        }

        # This table should exist to prove that the tablespace was restored.  It has not been updated since it was created so it
        # should not be created by any full page writes.  Once it is verified to exist it can be dropped.
        $oHostDbMaster->sqlSelectOneTest("select count(*) from test_exists", 0);
        $oHostDbMaster->sqlExecute('drop table test_exists');

        # Now it should be OK to drop database test2 and test3
        if ($bTestLocal)
        {
            $oHostDbMaster->sqlExecute('drop database test2', {bAutoCommit => true});
        }

        # The test table lives in ts1 so it needs to be moved or dropped
        if ($oHostDbMaster->pgVersion() >= PG_VERSION_90)
        {
            $oHostDbMaster->sqlExecute('alter table test set tablespace pg_default');
        }
        # Drop for older versions
        else
        {
            $oHostDbMaster->sqlExecute('drop table test');
        }

        # And drop the tablespace
        $oHostDbMaster->sqlExecute('drop database test3', {bAutoCommit => true});
        $oHostDbMaster->sqlExecute("drop tablespace ts1", {bAutoCommit => true});

        # Restore (restore type = immediate, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if (($bTestLocal || $bHostStandby) && $oHostDbMaster->pgVersion() >= PG_VERSION_94)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_IMMEDIATE);

            $oHostDbMaster->clusterStop();

            $oHostDbMaster->restore(
                undef, $strFullBackup, {bForce => true, strType => CFGOPTVAL_RESTORE_TYPE_IMMEDIATE, strTargetAction => 'promote'});

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest(
                'select message from test', ($bHostStandby ? $strStandbyMessage : $strFullMessage));
        }

        # Restore (restore type = xid, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        my $strRecoveryFile = undef;

        if ($bTestLocal)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_XID);

            $oHostDbMaster->clusterStop();

            executeTest('rm -rf ' . $oHostDbMaster->dbBasePath() . "/*");
            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath() . '/*');

            $oHostDbMaster->restore(
                undef, $strIncrBackup,
                {bForce => true, strType => CFGOPTVAL_RESTORE_TYPE_XID, strTarget => $strXidTarget,
                    strTargetAction => $oHostDbMaster->pgVersion() >= PG_VERSION_91 ? 'promote' : undef,
                    strTargetTimeline => $oHostDbMaster->pgVersion() >= PG_VERSION_12 ? 'current' : undef,
                    strOptionalParam => '--tablespace-map-all=../../tablespace', bTablespace => false});

            # Save recovery file to test so we can use it in the next test
            $strRecoveryFile = $oHostDbMaster->pgVersion() >= PG_VERSION_12 ? 'postgresql.auto.conf' : DB_FILE_RECOVERYCONF;

            storageTest()->copy(
                $oHostDbMaster->dbBasePath() . qw{/} . $strRecoveryFile, $self->testPath() . qw{/} . $strRecoveryFile);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");
        }

        # Restore (restore type = preserve, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_PRESERVE);

            $oHostDbMaster->clusterStop();

            executeTest('rm -rf ' . $oHostDbMaster->dbBasePath() . "/*");
            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath() . '/*');
            executeTest('rm -rf ' . $oHostDbMaster->tablespacePath(1) . "/*");

            # Restore recovery file that was saved in last test
            storageTest()->move($self->testPath . "/${strRecoveryFile}", $oHostDbMaster->dbBasePath() . "/${strRecoveryFile}");

            # Also touch recovery.signal when required
            if ($oHostDbMaster->pgVersion() >= PG_VERSION_12)
            {
                storageTest()->put($oHostDbMaster->dbBasePath() . "/" . DB_FILE_RECOVERYSIGNAL);
            }

            $oHostDbMaster->restore(undef, 'latest', {strType => CFGOPTVAL_RESTORE_TYPE_PRESERVE});

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");
        }

        # Restore (restore type = time, inclusive, automatically select backup) - there is no exclusive time test because I can't
        # find a way to find the exact commit time of a transaction.
        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_TIME);

        $oHostDbMaster->clusterStop();

        $oHostDbMaster->restore(
            undef, 'latest',
            {bDelta => true, strType => CFGOPTVAL_RESTORE_TYPE_TIME, strTarget => $strTimeTarget,
                strTargetAction => $oHostDbMaster->pgVersion() >= PG_VERSION_91 ? 'promote' : undef,
                strTargetTimeline => $oHostDbMaster->pgVersion() >= PG_VERSION_12 ? 'current' : undef,
                strBackupExpected => $strFullBackup});

        $oHostDbMaster->clusterStart();
        $oHostDbMaster->sqlSelectOneTest('select message from test', $strTimeMessage);

        # Restore (restore type = xid, exclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_XID);

            $oHostDbMaster->clusterStop();

            $oHostDbMaster->restore(
                undef, $strIncrBackup,
                {bDelta => true, strType => CFGOPTVAL_RESTORE_TYPE_XID, strTarget => $strXidTarget, bTargetExclusive => true,
                    strTargetAction => $oHostDbMaster->pgVersion() >= PG_VERSION_91 ? 'promote' : undef,
                    strTargetTimeline => $oHostDbMaster->pgVersion() >= PG_VERSION_12 ? 'current' : undef});

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strIncrMessage);
        }

        # Restore (restore type = name)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_91)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_NAME);

            $oHostDbMaster->clusterStop();

            $oHostDbMaster->restore(
                undef, 'latest',
                {bDelta => true, bForce => true, strType => CFGOPTVAL_RESTORE_TYPE_NAME, strTarget => $strNameTarget,
                    strTargetAction => 'promote',
                    strTargetTimeline => $oHostDbMaster->pgVersion() >= PG_VERSION_12 ? 'current' : undef});

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strNameMessage);
        }

        # Restore (restore type = default, timeline = created by type = xid, inclusive recovery)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_84)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_DEFAULT);

            $oHostDbMaster->clusterStop();

            # The timeline to use for this test is subject to change based on tests being added or removed above.  The best thing
            # would be to automatically grab the timeline after the restore, but since this test has been stable for a long time
            # it does not seem worth the effort to automate.
            $oHostDbMaster->restore(
                undef, $strIncrBackup,
                {bDelta => true,
                    strType => $oHostDbMaster->pgVersion() >= PG_VERSION_90 ?
                        CFGOPTVAL_RESTORE_TYPE_STANDBY : CFGOPTVAL_RESTORE_TYPE_DEFAULT,
                    strTargetTimeline => 4});

            $oHostDbMaster->clusterStart({bHotStandby => true});
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strTimelineMessage, {iTimeout => 120});
        }

        # Stop clusters to catch any errors in the postgres log
        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->clusterStop();

        # Test no-online backups
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra & $strStorage eq POSIX)
        {
            # Create a postmaster.pid file so it appears that the server is running
            storageTest()->put($oHostDbMaster->dbBasePath() . '/postmaster.pid', '99999');

            # Incr backup - make sure a --no-online backup fails
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'fail on --no-online',
                {iExpectedExitStatus => ERROR_POSTMASTER_RUNNING, strOptionalParam => '--no-online'});

            # Incr backup - allow --no-online backup to succeed with --force
            #-----------------------------------------------------------------------------------------------------------------------
            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'succeed on --no-online with --force', {strOptionalParam => '--no-online --force'});
        }

        # Stanza-delete --force without access to pgbackrest on database host
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra && $strStorage eq POSIX && $bHostBackup)
        {
            # With stanza-delete --force, allow stanza to be deleted regardless of accessibility of database host
            if ($bHostBackup)
            {
                $oHostDbMaster->stop();
                $oHostBackup->stop({strStanza => $self->stanza});
                $oHostBackup->stanzaDelete(
                    "delete stanza with --force when pgbackrest on pg host not accessible", {strOptionalParam => ' --force'});
                $oHostDbMaster->start();
                $oHostBackup->start();
            }
        }
    }
    }
    }
    }
}

1;
