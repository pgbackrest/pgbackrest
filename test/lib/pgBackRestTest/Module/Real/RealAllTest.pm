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

use pgBackRest::Archive::Info;
use pgBackRest::Backup::Info;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Version;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Env::Host::HostBaseTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostDbTest;
use pgBackRestTest::Env::HostEnvTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    foreach my $bS3 (false, true)
    {
    foreach my $bHostBackup ($bS3 ? (true) : (false, true))
    {
    # Standby should only be tested for pg versions that support it
    foreach my $bHostStandby ($bS3 ? (false) : (false, true))
    {
    # Master and standby backup destinations on need to be tested on one db version since it is not version specific
    foreach my $strBackupDestination (
        $bS3 || $bHostBackup ? (HOST_BACKUP) : $bHostStandby ? (HOST_DB_MASTER, HOST_DB_STANDBY) : (HOST_DB_MASTER))
    {
        my $bCompress = $bHostBackup && !$bHostStandby;

        # Increment the run, log, and decide whether this unit test should be run
        next if (!$self->begin(
            "bkp ${bHostBackup}, sby ${bHostStandby}, dst ${strBackupDestination}, cmp ${bCompress}, s3 ${bS3}",
            $self->pgVersion() eq PG_VERSION_96));

        # Skip when s3 and host backup tests when there is more than one version of pg being tested and this is not the last one
        my $hyVm = vmGet();

        if (($bS3 || $bHostBackup) &&
            (@{$hyVm->{$self->vm()}{&VM_DB_TEST}} > 1 && ${$hyVm->{$self->vm()}{&VM_DB_TEST}}[-1] ne $self->pgVersion()))
        {
            &log(INFO,
                'skipped - this test is run this OS using PG ' . ${$hyVm->{$self->vm()}{&VM_DB_TEST}}[-1]);
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
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3) = $self->setup(
            false, $self->expect(),
            {bHostBackup => $bHostBackup, bStandby => $bHostStandby, strBackupDestination => $strBackupDestination,
             bCompress => $bCompress, bArchiveAsync => false, bS3 => $bS3});

        # Create a manifest with the pg version to get version-specific paths
        my $oManifest = new pgBackRest::Manifest(BOGUS, {bLoad => false, strDbVersion => $self->pgVersion()});

        # Only perform extra tests on certain runs to save time
        my $bTestLocal = $self->runCurrent() == 1;
        my $bTestExtra =
            $bTestLocal || $self->runCurrent() == 4 || ($self->runCurrent() == 6 && $self->pgVersion() eq PG_VERSION_96);

        # If S3 set process max to 2.  This seems like the best place for parallel testing since it will help speed S3 processing
        # without slowing down the other tests too much.
        if ($bS3)
        {
            $oHostBackup->configUpdate({&CFGDEF_SECTION_GLOBAL => {cfgOptionName(CFGOPT_PROCESS_MAX) => 2}});
            $oHostDbMaster->configUpdate({&CFGDEF_SECTION_GLOBAL => {cfgOptionName(CFGOPT_PROCESS_MAX) => 2}});
        }

        $oHostDbMaster->clusterCreate();

        # Create the stanza
        $oHostBackup->stanzaCreate('main create stanza info files');

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
        my $strStandbyMessage = 'standby';
        my $strIncrMessage = 'incr';
        my $strTimeMessage = 'time';
        my $strXidMessage = 'xid';
        my $strNameMessage = 'name';
        my $strTimelineMessage = 'timeline3';

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
            $strType = CFGOPTVAL_BACKUP_TYPE_FULL;

            # Remove the files in the archive directory
            forceStorageRemove(storageRepo(), STORAGE_REPO_ARCHIVE, {bRecurse => true});

            $oHostDbMaster->check(
                'fail on missing archive.info file',
                {iTimeout => 0.1, iExpectedExitStatus => ERROR_FILE_MISSING});

            # Backup.info was created earlier so force stanza-create to create archive info file
            $oHostBackup->stanzaCreate('force create stanza info files', {strOptionalParam => ' --' . cfgOptionName(CFGOPT_FORCE)});

            # Check ERROR_ARCHIVE_DISABLED error
            $strComment = 'fail on archive_mode=off';
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true, bArchiveEnabled => false});

            $oHostBackup->backup($strType, $strComment, {iExpectedExitStatus => ERROR_ARCHIVE_DISABLED});
            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_DISABLED});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_DISABLED});
            }

            # Check ERROR_ARCHIVE_COMMAND_INVALID error
            $strComment = 'fail on invalid archive_command';
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true, bArchive => false});

            $oHostBackup->backup($strType, $strComment, {iExpectedExitStatus => ERROR_ARCHIVE_COMMAND_INVALID});
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
                storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}});

            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH});
            }

            # Restore the file to its original condition
            $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE));

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
                storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO),
                {&INFO_BACKUP_SECTION_DB =>
                    {&INFO_BACKUP_KEY_DB_VERSION => '8.0', &INFO_BACKUP_KEY_SYSTEM_ID => 6999999999999999999}});

            # Run the test
            $oHostDbMaster->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_BACKUP_MISMATCH});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_BACKUP_MISMATCH});
            }

            # Restore the file to its original condition
            $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO));

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
            # With data existing in the archive and backup directory, remove info files and confirm failure
            forceStorageRemove(storageRepo(), STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO);
            forceStorageRemove(storageRepo(), STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO . INI_COPY_EXT);
            forceStorageRemove(storageRepo(), STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE);
            forceStorageRemove(storageRepo(), STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE . INI_COPY_EXT);

            if (!$bS3)
            {
                $oHostBackup->stanzaCreate('fail on backup info file missing from non-empty dir',
                    {iExpectedExitStatus => ERROR_PATH_NOT_EMPTY});
            }

            # Force the backup.info file to be recreated
            $oHostBackup->stanzaCreate('verify success with force', {strOptionalParam => ' --' . cfgOptionName(CFGOPT_FORCE)});

            # Remove the backup info file
            forceStorageRemove(storageRepo(), STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO);
            forceStorageRemove(storageRepo(), STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO . INI_COPY_EXT);

            # Change the database version by copying a new pg_control file to a new db-path to use for db mismatch test
            storageDb()->pathCreate(
                $oHostDbMaster->dbPath() . '/testbase/' . DB_PATH_GLOBAL,
                {strMode => '0700', bIgnoreExists => true, bCreateParent => true});

            if ($self->pgVersion() eq PG_VERSION_94)
            {
                storageDb()->copy(
                    $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_95 . '.bin',
                    $oHostDbMaster->dbPath() . '/testbase/' . DB_FILE_PGCONTROL);
            } else
            {
                storageDb()->copy(
                    $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin',
                    $oHostDbMaster->dbPath() . '/testbase/' . DB_FILE_PGCONTROL);
            }

            # Run stanza-create online to confirm proper handling of configValidation error against new db-path
            $oHostBackup->stanzaCreate('fail on database mismatch with directory',
                {strOptionalParam => ' --' . $oHostBackup->optionIndexName(CFGOPT_DB_PATH, 1) . '=' . $oHostDbMaster->dbPath() .
                '/testbase/', iExpectedExitStatus => ERROR_DB_MISMATCH});

            # Stanza Upgrade - tests configValidate code - all other tests in synthetic integration tests
            #-----------------------------------------------------------------------------------------------------------------------
            # Run stanza-create offline with --force to create files needing to be upgraded (using new db-path)
            $oHostBackup->stanzaCreate('successfully create stanza files to be upgraded',
                {strOptionalParam =>
                    ' --' . $oHostBackup->optionIndexName(CFGOPT_DB_PATH, 1) . '=' . $oHostDbMaster->dbPath() .
                    '/testbase/ --no-' .  cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});
            my $oAchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet('archive/' . $self->stanza()));
            my $oBackupInfo = new pgBackRest::Backup::Info(storageRepo()->pathGet('backup/' . $self->stanza()));

            # Read info files to confirm the files were created with a different database version
            if ($self->pgVersion() eq PG_VERSION_94)
            {
                $self->testResult(sub {$oAchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                    PG_VERSION_95)}, true, 'archive upgrade forced with db-mismatch');
                $self->testResult(sub {$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef,
                    PG_VERSION_95)}, true, 'backup upgrade forced with db-mismatch');
            }
            else
            {
                $self->testResult(sub {$oAchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                    PG_VERSION_94)}, true, 'archive create forced with db-mismatch in prep for stanza-upgrade');
                $self->testResult(sub {$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef,
                    PG_VERSION_94)}, true, 'backup create forced with db-mismatch in prep for stanza-upgrade');
            }

            # Run stanza-upgrade online with the default db-path to correct the info files
            $oHostBackup->stanzaUpgrade('upgrade stanza files online');

            # Reread the info files and confirm the result
            $oAchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet('archive/' . $self->stanza()));
            $oBackupInfo = new pgBackRest::Backup::Info(storageRepo()->pathGet('backup/' . $self->stanza()));
            $self->testResult(sub {$oAchiveInfo->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef,
                $self->pgVersion())}, true, 'archive upgrade online corrects db');
            $self->testResult(sub {$oBackupInfo->test(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_DB_VERSION, undef,
                $self->pgVersion())}, true, 'backup upgrade online corrects db');
        }

        # Full backup
        #---------------------------------------------------------------------------------------------------------------------------
        $strType = CFGOPTVAL_BACKUP_TYPE_FULL;

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

            $oHostBackup->backup($strType, 'fail on backup lock exists', {iExpectedExitStatus => ERROR_LOCK_ACQUIRE});

            # Release the backup advisory lock so the next backup will succeed
            if (!$oHostDbMaster->sqlSelectOne('select pg_advisory_unlock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
            {
                confess 'unable to release advisory lock';
            }
        }

        my $oExecuteBackup = $oHostBackup->backupBegin(
            $strType, 'update during backup',
            {strTest => TEST_MANIFEST_BUILD, fTestDelay => $fTestDelay,
                strOptionalParam => ' --' . cfgOptionName(CFGOPT_BUFFER_SIZE) . '=16384'});

        $oHostDbMaster->sqlExecute("update test set message = '$strFullMessage'");

        # Required to set hint bits to be sent to the standby to make the heap match on both sides
        $oHostDbMaster->sqlSelectOneTest('select message from test', $strFullMessage);

        my $strFullBackup = $oHostBackup->backupEnd($strType, $oExecuteBackup);

        # Enabled async archiving
        $oHostBackup->configUpdate({&CFGDEF_SECTION_GLOBAL => {cfgOptionName(CFGOPT_ARCHIVE_ASYNC) => 'y'}});

        # Kick out a bunch of archive logs to excercise async archiving.  Only do this when compressed and remote to slow it
        # down enough to make it evident that the async process is working.
        if ($bTestExtra && $bCompress && $strBackupDestination eq HOST_BACKUP)
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
            $bDelta = false;
            $bForce = false;
            $strType = CFGOPTVAL_RESTORE_TYPE_DEFAULT;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $strTargetAction = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            $strComment = 'restore backup on replica';

            my %oRemapHash;
            $oRemapHash{&MANIFEST_TARGET_PGDATA} = $oHostDbStandby->dbBasePath();

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_92)
            {
                $oHostDbStandby->linkRemap($oManifest->walPath(), $oHostDbStandby->dbPath() . '/' . $oManifest->walPath());
            }

            $oHostDbStandby->restore(
                cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET), undef, \%oRemapHash, $bDelta, $bForce, $strType, $strTarget,
                $bTargetExclusive, $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus,
                ' --recovery-option=standby_mode=on' .
                    ' --recovery-option="primary_conninfo=host=' . HOST_DB_MASTER .
                    ' port=' . $oHostDbMaster->pgPort() . ' user=replicator"');

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
                # If there is only a master and a replica and the replica is the backup destination, then if db2-host and db3-host
                # are BOGUS, confirm failure to reach the master
                if (!$bHostBackup && $bHostStandby && $strBackupDestination eq HOST_DB_STANDBY)
                {
                    my $strStandbyBackup = $oHostBackup->backup(
                        CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby, failure to reach master',
                        {bStandby => true,
                         iExpectedExitStatus => ERROR_DB_CONNECT,
                         strOptionalParam => '--' .
                         $oHostBackup->optionIndexName(CFGOPT_DB_HOST, cfgOptionIndexTotal(CFGOPT_DB_PATH)) . '=' . BOGUS});
                }
                else
                {
                    my $strStandbyBackup = $oHostBackup->backup(
                        CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby, failure to access at least one standby',
                        {bStandby => true,
                         iExpectedExitStatus => ERROR_HOST_CONNECT,
                         strOptionalParam => '--' .
                         $oHostBackup->optionIndexName(CFGOPT_DB_HOST, cfgOptionIndexTotal(CFGOPT_DB_PATH)) . '=' . BOGUS});
                }
            }

            my $strStandbyBackup = $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby',
                {bStandby => true,
                 iExpectedExitStatus => $oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY ? undef : ERROR_CONFIG,
                 strOptionalParam => '--' . cfgOptionName(CFGOPT_RETENTION_FULL) . '=1'});

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY)
            {
                $strFullBackup = $strStandbyBackup;
            }

            # Confirm the check command runs without error on a standby
            $oHostDbStandby->check('verify check command on standby');

            # Shutdown the stanby before creating tablespaces (this will error since paths are different)
            $oHostDbStandby->clusterStop({bIgnoreLogError => true});
        }

        # Execute stop and make sure the backup fails
        #---------------------------------------------------------------------------------------------------------------------------
        # Restart the cluster to check for any errors before continuing since the stop tests will definitely create errors and
        # the logs will to be deleted to avoid causing issues further down the line.
        if ($bTestExtra && !$bS3)
        {
            $strType = CFGOPTVAL_BACKUP_TYPE_INCR;

            $oHostDbMaster->clusterRestart();

            $oHostDbMaster->stop();

            $oHostBackup->backup($strType, 'attempt backup when stopped', {iExpectedExitStatus => ERROR_STOP});

            $oHostDbMaster->start();
        }

        # Setup the time target
        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->sqlExecute("update test set message = '$strTimeMessage'");
        $oHostDbMaster->sqlWalRotate();
        my $strTimeTarget = $oHostDbMaster->sqlSelectOne("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ')");
        &log(INFO, "        time target is ${strTimeTarget}");

        # Incr backup - fail on archive_mode=always when version >= 9.5
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_95)
        {
            $strType = CFGOPTVAL_BACKUP_TYPE_INCR;

            # Set archive_mode=always
            $oHostDbMaster->clusterRestart({bArchiveAlways => true});

            $oHostBackup->backup($strType, 'fail on archive_mode=always', {iExpectedExitStatus => ERROR_FEATURE_NOT_SUPPORTED});

            # Reset the cluster to a normal state so the next test will work
            $oHostDbMaster->clusterRestart();
        }

        # Incr backup
        #---------------------------------------------------------------------------------------------------------------------------
        $strType = CFGOPTVAL_BACKUP_TYPE_INCR;

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

        # Start a backup so the next backup has to restart it.  This test is not required for PostgreSQL >= 9.6 since backups
        # are run in non-exlusive mode.
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_93 && $oHostDbMaster->pgVersion() < PG_VERSION_96)
        {
            $oHostDbMaster->sqlSelectOne("select pg_start_backup('test backup that will cause an error', true)");

            # Verify that an error is returned if the backup is already running
            $oHostBackup->backup($strType, 'fail on backup already running', {iExpectedExitStatus => ERROR_DB_QUERY});

            # Restart the cluster ignoring any errors in the postgresql log
            $oHostDbMaster->clusterRestart({bIgnoreLogError => true});

            # Start a new backup to make the next test restart it
            $oHostDbMaster->sqlSelectOne("select pg_start_backup('test backup that will be restarted', true)");
        }

        $oExecuteBackup = $oHostBackup->backupBegin(
            $strType, 'update during backup',
            {strTest => TEST_MANIFEST_BUILD, fTestDelay => $fTestDelay,
                strOptionalParam => '--' . cfgOptionName(CFGOPT_STOP_AUTO) .  ' --no-' . cfgOptionName(CFGOPT_ARCHIVE_CHECK) .
                ' --' . cfgOptionName(CFGOPT_BUFFER_SIZE) . '=32768'});

        # Drop a table
        $oHostDbMaster->sqlExecute('drop table test_remove');
        $oHostDbMaster->sqlWalRotate();
        $oHostDbMaster->sqlExecute("update test set message = '$strIncrMessage'", {bCommit => true});

        # Check that application name is set
        if ($oHostDbMaster->pgVersion() >= PG_VERSION_APPLICATION_NAME)
        {
            my $strApplicationNameExpected = BACKREST_NAME . ' [' . cfgCommandName(CFGCMD_BACKUP) . ']';
            my $strApplicationName = $oHostDbMaster->sqlSelectOne(
                "select application_name from pg_stat_activity where application_name like '" . BACKREST_NAME . "%'");

            if (!defined($strApplicationName) || $strApplicationName ne $strApplicationNameExpected)
            {
                confess &log(ERROR,
                    "application name '" . (defined($strApplicationName) ? $strApplicationName : '[null]') .
                        "' does not match '" . $strApplicationNameExpected . "'");
            }
        }

        my $strIncrBackup = $oHostBackup->backupEnd($strType, $oExecuteBackup);

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
        if ($bTestLocal)
        {
            $oHostDbMaster->sqlExecute(
                'create table test (id int);' .
                'insert into test values (1);' .
                'create table test_ts1 (id int) tablespace ts1;' .
                'insert into test_ts1 values (2);',
                {strDb => 'test2', bAutoCommit => true});
            $oHostDbMaster->sqlWalRotate();
        }

        # Restore (type = default)
        #---------------------------------------------------------------------------------------------------------------------------
        $bDelta = false;
        $bForce = false;
        $strType = CFGOPTVAL_RESTORE_TYPE_DEFAULT;
        $strTarget = undef;
        $bTargetExclusive = undef;
        $strTargetAction = undef;
        $strTargetTimeline = undef;
        $oRecoveryHashRef = undef;
        $strComment = undef;
        $iExpectedExitStatus = undef;

        if ($bTestLocal)
        {
            # Expect failure because postmaster.pid exists
            $strComment = 'postmaster running';
            $iExpectedExitStatus = ERROR_POSTMASTER_RUNNING;

            $oHostDbMaster->restore(
                cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET), undef, undef, $bDelta, $bForce, $strType, $strTarget,
                $bTargetExclusive, $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);
        }

        $oHostDbMaster->clusterStop();

        if ($bTestLocal)
        {
            # Expect failure because db path is not empty
            $strComment = 'path not empty';
            $iExpectedExitStatus = ERROR_PATH_NOT_EMPTY;

            $oHostDbMaster->restore(
                cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET), undef, undef, $bDelta, $bForce, $strType, $strTarget,
                $bTargetExclusive, $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);
        }

        # Drop and recreate db path
        testPathRemove($oHostDbMaster->dbBasePath());
        storageTest()->pathCreate($oHostDbMaster->dbBasePath(), {strMode => '0700'});
        testPathRemove($oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath());
        storageTest()->pathCreate($oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath(), {strMode => '0700'});
        testPathRemove($oHostDbMaster->tablespacePath(1));
        storageTest()->pathCreate($oHostDbMaster->tablespacePath(1), {strMode => '0700'});

        # Now the restore should work
        $strComment = undef;
        $iExpectedExitStatus = undef;

        $oHostDbMaster->restore(
            cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET), undef, undef, $bDelta, $bForce, $strType, $strTarget,
            $bTargetExclusive, $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus,
            $bTestLocal ? ' --db-include=test1' : undef);

        $oHostDbMaster->clusterStart();
        $oHostDbMaster->sqlSelectOneTest('select message from test', $bTestLocal ? $strNameMessage : $strIncrMessage);

        # The tablespace path should exist and have files in it
        my $strTablespacePath = $oHostDbMaster->tablespacePath(1);

        # Version <= 8.4 always places a PG_VERSION file in the tablespace
        if ($oHostDbMaster->pgVersion() <= PG_VERSION_84)
        {
            if (!storageDb()->exists("${strTablespacePath}/" . DB_FILE_PGVERSION))
            {
                confess &log(ASSERT, "unable to find '" . DB_FILE_PGVERSION . "' in tablespace path '${strTablespacePath}'");
            }
        }
        # Version >= 9.0 creates a special path using the version and catalog number
        else
        {
            # Backup info will have the catalog number
            my $oBackupInfo = new pgBackRest::Common::Ini(
                storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO),
                {bLoad => false, strContent => ${storageRepo()->get(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO)}});

            # Construct the special path
            $strTablespacePath .=
                '/PG_' . $oHostDbMaster->pgVersion() . qw{_} . $oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG);

            # Check that path exists
            if (!storageDb()->pathExists($strTablespacePath))
            {
                confess &log(ASSERT, "unable to find tablespace path '${strTablespacePath}'");
            }
        }

        # Make sure there are some files in the tablespace path (exclude PG_VERSION if <= 8.4 since that was tested above)
        if (grep(!/^PG\_VERSION$/i, storageDb()->list($strTablespacePath)) == 0)
        {
            confess &log(ASSERT, "no files found in tablespace path '${strTablespacePath}'");
        }

        # This table should exist to prove that the tablespace was restored.  It has not been updated since it was created so it
        # should not be created by any full page writes.  Once it is verified to exist it can be dropped.
        $oHostDbMaster->sqlSelectOneTest("select count(*) from test_exists", 0);
        $oHostDbMaster->sqlExecute('drop table test_exists');

        # Now it should be OK to drop database test2
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
        $oHostDbMaster->sqlExecute("drop tablespace ts1", {bAutoCommit => true});

        # Restore (restore type = immediate, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if (($bTestLocal || $bHostStandby) && $oHostDbMaster->pgVersion() >= PG_VERSION_94)
        {
            $bDelta = false;
            $bForce = true;
            $strType = CFGOPTVAL_RESTORE_TYPE_IMMEDIATE;
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
            $oHostDbMaster->sqlSelectOneTest(
                'select message from test', ($bHostStandby ? $strStandbyMessage : $strFullMessage));
        }

        # Restore (restore type = xid, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal)
        {
            $bDelta = false;
            $bForce = true;
            $strType = CFGOPTVAL_RESTORE_TYPE_XID;
            $strTarget = $strXidTarget;
            $bTargetExclusive = undef;
            $strTargetAction = $oHostDbMaster->pgVersion() >= PG_VERSION_91 ? 'promote' : undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            $oHostDbMaster->clusterStop();

            executeTest('rm -rf ' . $oHostDbMaster->dbBasePath() . "/*");
            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath() . '/*');

            $oHostDbMaster->restore(
                $strIncrBackup, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive, $strTargetAction,
                $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus,
                '--tablespace-map-all=../../tablespace', false);

            # Save recovery file to test so we can use it in the next test
            storageDb()->copy(
                $oHostDbMaster->dbBasePath() . qw{/} . DB_FILE_RECOVERYCONF, $self->testPath() . qw{/} . DB_FILE_RECOVERYCONF);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");
        }

        # Restore (restore type = preserve, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal)
        {
            $bDelta = false;
            $bForce = false;
            $strType = CFGOPTVAL_RESTORE_TYPE_PRESERVE;
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
            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . qw{/} . $oManifest->walPath() . '/*');
            executeTest('rm -rf ' . $oHostDbMaster->tablespacePath(1) . "/*");

            # Restore recovery file that was saved in last test
            storageDb()->move($self->testPath . '/recovery.conf', $oHostDbMaster->dbBasePath() . '/recovery.conf');

            $oHostDbMaster->restore(
                cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET), undef, undef, $bDelta, $bForce, $strType, $strTarget,
                $bTargetExclusive, $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");
        }

        # Restore (restore type = time, inclusive) - there is no exclusive time test because I can't find a way to find the
        # exact commit time of a transaction.
        #---------------------------------------------------------------------------------------------------------------------------
        $bDelta = true;
        $bForce = false;
        $strType = CFGOPTVAL_RESTORE_TYPE_TIME;
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
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal)
        {
            $bDelta = true;
            $bForce = false;
            $strType = CFGOPTVAL_RESTORE_TYPE_XID;
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
        }

        # Restore (restore type = name)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_91)
        {
            $bDelta = true;
            $bForce = true;
            $strType = CFGOPTVAL_RESTORE_TYPE_NAME;
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
                cfgDefOptionDefault(CFGCMD_RESTORE, CFGOPT_SET), undef, undef, $bDelta, $bForce, $strType, $strTarget,
                $bTargetExclusive, $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strNameMessage);
        }

        # Restore (restore type = default, timeline = 3)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestLocal && $oHostDbMaster->pgVersion() >= PG_VERSION_84)
        {
            $bDelta = true;
            $bForce = false;
            $strType = CFGOPTVAL_RESTORE_TYPE_DEFAULT;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $strTargetAction = undef;
            $strTargetTimeline = 4;
            $oRecoveryHashRef = $oHostDbMaster->pgVersion() >= PG_VERSION_90 ? {'standby-mode' => 'on'} : undef;
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

        # Stop clusters to catch any errors in the postgres log
        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->clusterStop();

        # Test no-online backups
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra & !$bS3)
        {
            # Create a postmaster.pid file so it appears that the server is running
            storageTest()->put($oHostDbMaster->dbBasePath() . '/postmaster.pid', '99999');

            # Incr backup - make sure a --no-online backup fails
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = CFGOPTVAL_BACKUP_TYPE_INCR;

            $oHostBackup->backup(
                $strType, 'fail on --no-' . cfgOptionName(CFGOPT_ONLINE),
                {iExpectedExitStatus => ERROR_POSTMASTER_RUNNING, strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE)});

            # Incr backup - allow --no-online backup to succeed with --force
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = CFGOPTVAL_BACKUP_TYPE_INCR;

            $oHostBackup->backup(
                $strType, 'succeed on --no-' . cfgOptionName(CFGOPT_ONLINE) . ' with --' . cfgOptionName(CFGOPT_FORCE),
                {strOptionalParam => '--no-' . cfgOptionName(CFGOPT_ONLINE) . ' --' . cfgOptionName(CFGOPT_FORCE)});
        }
    }
    }
    }
    }
}

1;
