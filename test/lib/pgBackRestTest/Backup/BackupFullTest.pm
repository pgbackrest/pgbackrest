####################################################################################################################################
# BackupFullTest.pm - Tests for all commands against a real database
####################################################################################################################################
package pgBackRestTest::Backup::BackupFullTest;
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
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;
use pgBackRest::Version;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Backup::BackupCommonTest;
use pgBackRestTest::Common::Host::HostBaseTest;
use pgBackRestTest::Common::Host::HostBackupTest;
use pgBackRestTest::Common::Host::HostDbTest;
use pgBackRestTest::Backup::ExpireCommonTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    foreach my $bHostBackup (false, true)
    {
    foreach my $bHostStandby (false, true)
    {
    foreach my $strBackupDestination (
        $bHostBackup ? (HOST_BACKUP) : $bHostStandby ? (HOST_DB_MASTER, HOST_DB_STANDBY) : (HOST_DB_MASTER))
    {
    foreach my $bArchiveAsync ($bHostStandby ? (false) : (false, true))
    {
    foreach my $bCompress ($bHostStandby ? (false) : (false, true))
    {
        # Increment the run, log, and decide whether this unit test should be run
        next if (!$self->begin(
            "bkp ${bHostBackup}, sby ${bHostStandby}, dst ${strBackupDestination}, asy ${bArchiveAsync}, cmp ${bCompress}",
            $self->processMax() == 1 && $self->pgVersion() eq PG_VERSION_95));

        if ($bHostStandby && $self->pgVersion() < PG_VERSION_HOT_STANDBY)
        {
            &log(INFO, 'skipped - this version of PostgreSQL does not support hot standby');
            next;
        }

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oFile) = $self->setup(
            false, $self->expect(),
            {bHostBackup => $bHostBackup, bStandby => $bHostStandby, strBackupDestination => $strBackupDestination,
             bCompress => $bCompress, bArchiveAsync => $bArchiveAsync});

        # Determine if extra tests are performed.  Extra tests should not be primary tests for compression or async archiving.
        my $bTestExtra = ($self->runCurrent() == 1) || ($self->runCurrent() == 7);

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
        $oHostDbMaster->sqlExecute('create database test1', {bAutoCommit => true});
        $oHostDbMaster->sqlExecute('create database test2', {bAutoCommit => true});

        # Test check command and stanza create
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra)
        {
            $strType = BACKUP_TYPE_FULL;

            # Remove the files in the archive directory
            executeTest('sudo rm -rf ' . $oFile->pathGet(PATH_BACKUP_ARCHIVE) . "/*");
            $oHostDbMaster->check(
                'fail on missing archive.info file',
                {iTimeout => 0.1, iExpectedExitStatus => ERROR_FILE_MISSING});

            # Backup.info was created earlier so force stanza-create to create archive info file
            $oHostBackup->stanzaCreate('force create stanza info files', {strOptionalParam => ' --' . OPTION_FORCE});

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
                $oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB => {&INFO_ARCHIVE_KEY_DB_VERSION => '8.0'}});

            $oHostDbMaster->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH});

            # If running the remote tests then also need to run check locally
            if ($bHostBackup)
            {
                $oHostBackup->check($strComment, {iTimeout => 0.1, iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH});
            }

            # Restore the file to its original condition
            $oHostBackup->infoRestore($oFile->pathGet(PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE));

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

            # First run a successful backup to create the backup.info file
            $oHostBackup->backup($strType, 'run a successful backup');

            # Load the backup.info file and munge it for testing by breaking the database version and system id
            $oHostBackup->infoMunge(
                $oFile->pathGet(PATH_BACKUP_CLUSTER, FILE_BACKUP_INFO),
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
            $oHostBackup->infoRestore($oFile->pathGet(PATH_BACKUP_CLUSTER, FILE_BACKUP_INFO));

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

            # Stanza Create - ??? move to stanza-create tests when can create a backup synthetically
            #-----------------------------------------------------------------------------------------------------------------------
            # With data existing in the archive and backup directory, remove backup info file and confirm failure
            executeTest('sudo rm ' . $oHostBackup->repoPath() . '/backup/' . $self->stanza() . '/backup.info');
            $oHostBackup->stanzaCreate('fail on backup info file missing from non-empty dir',
                {iExpectedExitStatus => ERROR_PATH_NOT_EMPTY});

            # Force the backup.info file to be recreated
            $oHostBackup->stanzaCreate('verify success with force', {strOptionalParam => ' --' . OPTION_FORCE});

            # Remove the backup info file
            executeTest('sudo rm ' . $oHostBackup->repoPath() . '/backup/' . $self->stanza() . '/backup.info');

            # Change the database version by copying a new pg_control file
            executeTest('sudo mv ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL .
                ' ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL . 'save');
            executeTest(
                'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $oHostDbMaster->dbBasePath() . '/' .
                DB_FILE_PGCONTROL);

            # Run stanza-create with --force
            $oHostBackup->stanzaCreate('test force fails on database mismatch with directory',
                {iExpectedExitStatus => ERROR_ARCHIVE_MISMATCH, strOptionalParam => '--no-' . OPTION_ONLINE .
                ' --' . OPTION_FORCE});

            # Restore the database version
            executeTest('sudo rm ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);
            executeTest('sudo mv ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL . 'save' .
                ' ' . $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL);

            # Run stanza-create offline with --force
            $oHostBackup->stanzaCreate('restore stanza files',
                {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});
        }

        # Full backup
        #---------------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_FULL;

        # Create the table where test messages will be stored
        $oHostDbMaster->sqlExecute("create table test (message text not null)");
        $oHostDbMaster->sqlXlogRotate();
        $oHostDbMaster->sqlExecute("insert into test values ('$strDefaultMessage')");

        if ($bTestExtra)
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
                strOptionalParam => ' --' . OPTION_BUFFER_SIZE . '=16384'});

        $oHostDbMaster->sqlExecute("update test set message = '$strFullMessage'");

        # Required to set hint bits to be sent to the standby to make the heap match on both sides
        $oHostDbMaster->sqlSelectOneTest('select message from test', $strFullMessage);

        my $strFullBackup = $oHostBackup->backupEnd($strType, $oExecuteBackup);

        # Kick out a bunch of archive logs to excercise async archiving.  Only do this when compressed and remote to slow it
        # down enough to make it evident that the async process is working.
        if ($bArchiveAsync && $bCompress && $strBackupDestination eq HOST_BACKUP)
        {
            &log(INFO, '    multiple pg_switch_xlog() to exercise async archiving');
            $oHostDbMaster->sqlExecute("create table xlog_activity (id int)");
            $oHostDbMaster->sqlXlogRotate();
            $oHostDbMaster->sqlExecute("insert into xlog_activity values (1)");
            $oHostDbMaster->sqlXlogRotate();
            $oHostDbMaster->sqlExecute("insert into xlog_activity values (2)");
            $oHostDbMaster->sqlXlogRotate();
            $oHostDbMaster->sqlExecute("insert into xlog_activity values (3)");
            $oHostDbMaster->sqlXlogRotate();
            $oHostDbMaster->sqlExecute("insert into xlog_activity values (4)");
            $oHostDbMaster->sqlXlogRotate();
        }

        # Setup replica
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bHostStandby)
        {
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

            $strComment = 'restore backup on replica';

            my %oRemapHash;
            $oRemapHash{&MANIFEST_TARGET_PGDATA} = $oHostDbStandby->dbBasePath();

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_92)
            {
                $oHostDbStandby->linkRemap(DB_PATH_PGXLOG, $oHostDbStandby->dbPath() . '/' . DB_PATH_PGXLOG);
            }

            $oHostDbStandby->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, \%oRemapHash, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus,
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

            my $strStandbyBackup = $oHostBackup->backup(
                BACKUP_TYPE_FULL, 'backup from standby',
                {bStandby => true,
                 iExpectedExitStatus => $oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY ? undef : ERROR_CONFIG,
                 strOptionalParam => '--' . OPTION_RETENTION_FULL . '=1'});

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY)
            {
                $strFullBackup = $strStandbyBackup;
            }

            # Confirm the check command runs without error on a standby
            $oHostDbStandby->check('verify check command on standby');
        }

        # Execute stop and make sure the backup fails
        #---------------------------------------------------------------------------------------------------------------------------
        # Restart the cluster to check for any errors before continuing since the stop tests will definitely create errors and
        # the logs will to be deleted to avoid causing issues further down the line.
        if ($bTestExtra)
        {
            $strType = BACKUP_TYPE_INCR;

            $oHostDbMaster->clusterRestart();

            $oHostDbMaster->stop();

            $oHostBackup->backup($strType, 'attempt backup when stopped', {iExpectedExitStatus => ERROR_STOP});

            $oHostDbMaster->start();
        }

        # Setup the time target
        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->sqlExecute("update test set message = '$strTimeMessage'");
        $oHostDbMaster->sqlXlogRotate();
        my $strTimeTarget = $oHostDbMaster->sqlSelectOne("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ')");
        &log(INFO, "        time target is ${strTimeTarget}");

        # Incr backup - fail on archive_mode=always when version >= 9.5
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra && $oHostDbMaster->pgVersion() >= PG_VERSION_95)
        {
            $strType = BACKUP_TYPE_INCR;

            # Set archive_mode=always
            $oHostDbMaster->clusterRestart({bArchiveAlways => true});

            $oHostBackup->backup($strType, 'fail on archive_mode=always', {iExpectedExitStatus => ERROR_FEATURE_NOT_SUPPORTED});

            # Reset the cluster to a normal state so the next test will work
            $oHostDbMaster->clusterRestart();
        }

        # Incr backup
        #---------------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_INCR;

        # Create a tablespace directory
        filePathCreate($oHostDbMaster->tablespacePath(1), undef, undef, true);

        # Also create it on the standby so replay won't fail
        if (defined($oHostDbStandby))
        {
            filePathCreate($oHostDbStandby->tablespacePath(1), undef, undef, true);
        }

        $oHostDbMaster->sqlExecute(
            "create tablespace ts1 location '" . $oHostDbMaster->tablespacePath(1) . "'", {bAutoCommit => true});
        $oHostDbMaster->sqlExecute("alter table test set tablespace ts1", {bCheckPoint => true});

        # Create a table in the tablespace
        $oHostDbMaster->sqlExecute("create table test_remove (id int)");
        $oHostDbMaster->sqlXlogRotate();
        $oHostDbMaster->sqlExecute("update test set message = '$strDefaultMessage'");
        $oHostDbMaster->sqlXlogRotate();

        # Start a backup so the next backup has to restart it.  This test is not required for PostgreSQL >= 9.6 since backups
        # are run in non-exlusive mode.
        if ($bTestExtra && $oHostDbMaster->pgVersion() >= PG_VERSION_93 && $oHostDbMaster->pgVersion() < PG_VERSION_96)
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
                strOptionalParam => '--' . OPTION_STOP_AUTO . ' --no-' . OPTION_BACKUP_ARCHIVE_CHECK .
                    ' --' . OPTION_BUFFER_SIZE . '=24576'});

        # Drop a table
        $oHostDbMaster->sqlExecute('drop table test_remove');
        $oHostDbMaster->sqlXlogRotate();
        $oHostDbMaster->sqlExecute("update test set message = '$strIncrMessage'", {bCommit => true});

        # Check that application name is set
        if ($oHostDbMaster->pgVersion() >= PG_VERSION_APPLICATION_NAME)
        {
            my $strApplicationNameExpected = BACKREST_NAME . ' [' . CMD_BACKUP . ']';
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

        if ($bTestExtra)
        {
            $oHostDbMaster->sqlExecute("update test set message = '$strXidMessage'", {bCommit => false});
            $oHostDbMaster->sqlXlogRotate();
            $strXidTarget = $oHostDbMaster->sqlSelectOne("select txid_current()");
            $oHostDbMaster->sqlCommit();
            &log(INFO, "        xid target is ${strXidTarget}");
        }

        # Setup the name target
        #---------------------------------------------------------------------------------------------------------------------------
        my $strNameTarget = 'backrest';

        if ($bTestExtra)
        {
            $oHostDbMaster->sqlExecute("update test set message = '$strNameMessage'", {bCommit => true});
            $oHostDbMaster->sqlXlogRotate();

            if ($oHostDbMaster->pgVersion() >= PG_VERSION_91)
            {
                $oHostDbMaster->sqlExecute("select pg_create_restore_point('${strNameTarget}')");
            }

            &log(INFO, "        name target is ${strNameTarget}");
        }

        # Create a table and data in database test2
        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->sqlExecute(
            'create table test (id int);' .
            'insert into test values (1);' .
            'create table test_ts1 (id int) tablespace ts1;' .
            'insert into test_ts1 values (2);',
            {strDb => 'test2', bAutoCommit => true});
        $oHostDbMaster->sqlXlogRotate();

        # Restore (type = default)
        #---------------------------------------------------------------------------------------------------------------------------
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

            # &log(INFO, "    testing recovery type = ${strType}");

        if ($bTestExtra)
        {
            # Expect failure because postmaster.pid exists
            $strComment = 'postmaster running';
            $iExpectedExitStatus = ERROR_POSTMASTER_RUNNING;

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);
        }

        $oHostDbMaster->clusterStop();

        if ($bTestExtra)
        {
            # Expect failure because db path is not empty
            $strComment = 'path not empty';
            $iExpectedExitStatus = ERROR_PATH_NOT_EMPTY;

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);
        }

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
        $oHostDbMaster->sqlSelectOneTest('select message from test', $bTestExtra ? $strNameMessage : $strIncrMessage);

        # Now it should be OK to drop database test2
        $oHostDbMaster->sqlExecute('drop database test2', {bAutoCommit => true});

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
        if (($bTestExtra || $bHostStandby) && $oHostDbMaster->pgVersion() >= PG_VERSION_94)
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
            $oHostDbMaster->sqlSelectOneTest(
                'select message from test', ($bHostStandby ? $strStandbyMessage : $strFullMessage));
        }

        # Restore (restore type = xid, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra)
        {
            $bDelta = false;
            $bForce = true;
            $strType = RECOVERY_TYPE_XID;
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
            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . "/pg_xlog/*");

            $oHostDbMaster->restore(
                $strIncrBackup, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive, $strTargetAction,
                $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus,
                '--tablespace-map-all=../../tablespace', false);

            # Save recovery file to test so we can use it in the next test
            $oFile->copy(PATH_ABSOLUTE, $oHostDbMaster->dbBasePath() . '/recovery.conf',
                         PATH_ABSOLUTE, $self->testPath() . '/recovery.conf');

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");
        }

        # Restore (restore type = preserve, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra)
        {
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
            $oFile->move(PATH_ABSOLUTE, $self->testPath . '/recovery.conf',
                         PATH_ABSOLUTE, $oHostDbMaster->dbBasePath() . '/recovery.conf');

            $oHostDbMaster->restore(
                OPTION_DEFAULT_RESTORE_SET, undef, undef, $bDelta, $bForce, $strType, $strTarget, $bTargetExclusive,
                $strTargetAction, $strTargetTimeline, $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            $oHostDbMaster->clusterStart();
            $oHostDbMaster->sqlSelectOneTest('select message from test', $strXidMessage);

            $oHostDbMaster->sqlExecute("update test set message = '$strTimelineMessage'");
        }

        # Restore (restore type = time, inclusive) - there is no exclusive time test because I can't find a way to find the
        # exact commit time of a transaction.
        #---------------------------------------------------------------------------------------------------------------------------
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
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra)
        {
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
        }

        # Restore (restore type = name)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra && $oHostDbMaster->pgVersion() >= PG_VERSION_91)
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
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra && $oHostDbMaster->pgVersion() >= PG_VERSION_84)
        {
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_DEFAULT;
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

        # Incr backup - make sure a --no-online backup fails
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra)
        {
            $strType = BACKUP_TYPE_INCR;

            $oHostBackup->backup(
                $strType, 'fail on --no-' . OPTION_ONLINE,
                {iExpectedExitStatus => ERROR_POSTMASTER_RUNNING, strOptionalParam => '--no-' . OPTION_ONLINE});
        }

        # Incr backup - allow --no-online backup to succeed with --force
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bTestExtra)
        {
            $strType = BACKUP_TYPE_INCR;

            $oHostBackup->backup(
                $strType, 'succeed on --no-' . OPTION_ONLINE . ' with --' . OPTION_FORCE,
                {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});
        }

        # Stop clusters to catch any errors in the postgres log
        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbMaster->clusterStop({bImmediate => true});

        if (defined($oHostDbStandby))
        {
            $oHostDbStandby->clusterStop({bImmediate => true});
        }
    }
    }
    }
    }
    }
}

1;
