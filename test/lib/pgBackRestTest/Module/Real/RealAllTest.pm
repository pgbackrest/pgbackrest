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

    foreach my $rhRun
    (
        {pg => '9.0', dst => 'db-primary', tls => 0, stg =>   GCS, enc => 1, cmp =>  BZ2, rt => 2, bnd => 1},
        {pg => '9.1', dst => 'db-standby', tls => 1, stg =>   GCS, enc => 0, cmp =>   GZ, rt => 1, bnd => 0},
        {pg => '9.2', dst => 'db-standby', tls => 0, stg => POSIX, enc => 1, cmp => NONE, rt => 1, bnd => 1},
        {pg => '9.3', dst =>     'backup', tls => 0, stg => AZURE, enc => 0, cmp => NONE, rt => 2, bnd => 0},
        {pg => '9.4', dst => 'db-standby', tls => 0, stg => POSIX, enc => 1, cmp =>  LZ4, rt => 1, bnd => 1},
        {pg => '9.5', dst =>     'backup', tls => 1, stg =>    S3, enc => 0, cmp =>  BZ2, rt => 1, bnd => 0},
        {pg => '9.6', dst =>     'backup', tls => 0, stg => POSIX, enc => 0, cmp => NONE, rt => 2, bnd => 1},
        {pg =>  '10', dst => 'db-standby', tls => 1, stg =>    S3, enc => 1, cmp =>   GZ, rt => 2, bnd => 0},
        {pg =>  '11', dst =>     'backup', tls => 1, stg => AZURE, enc => 0, cmp =>  ZST, rt => 2, bnd => 1},
        {pg =>  '12', dst =>     'backup', tls => 0, stg =>    S3, enc => 1, cmp =>  LZ4, rt => 1, bnd => 0},
        {pg =>  '13', dst => 'db-standby', tls => 1, stg =>   GCS, enc => 0, cmp =>  ZST, rt => 1, bnd => 1},
        {pg =>  '14', dst =>     'backup', tls => 0, stg => POSIX, enc => 1, cmp =>  LZ4, rt => 2, bnd => 0},
        {pg =>  '15', dst => 'db-standby', tls => 0, stg => AZURE, enc => 0, cmp => NONE, rt => 2, bnd => 1},
    )
    {
        # Only run tests for this pg version
        next if ($rhRun->{pg} ne $self->pgVersion());

        # Get run parameters
        my $bHostBackup = $rhRun->{dst} eq HOST_BACKUP ? true : false;
        my $bHostStandby = $self->pgVersion() >= PG_VERSION_HOT_STANDBY ? true : false;
        my $bTls = $rhRun->{tls};
        my $strBackupDestination = $rhRun->{dst};
        my $strStorage = $rhRun->{stg};
        my $bRepoEncrypt = $rhRun->{enc};
        my $strCompressType = $rhRun->{cmp};
        my $iRepoTotal = $rhRun->{rt};
        my $bBundle = $rhRun->{bnd};

        # Some tests are not version specific so only run them on a single version of PostgreSQL
        my $bNonVersionSpecific = $self->pgVersion() eq PG_VERSION_96;

        # Increment the run, log, and decide whether this unit test should be run
        next if !$self->begin(
            "bkp ${bHostBackup}, sby ${bHostStandby}, tls ${bTls}, dst ${strBackupDestination}, cmp ${strCompressType}" .
                ", storage ${strStorage}, enc ${bRepoEncrypt}");

        # Create hosts, file object, and config
        my ($oHostDbPrimary, $oHostDbStandby, $oHostBackup) = $self->setup(
            false,
            {bHostBackup => $bHostBackup, bStandby => $bHostStandby, bTls => $bTls, strBackupDestination => $strBackupDestination,
                strCompressType => $strCompressType, bArchiveAsync => false, strStorage => $strStorage,
                bRepoEncrypt => $bRepoEncrypt, iRepoTotal => $iRepoTotal, bBundle => $bBundle});

        # Some commands will fail because of the bogus host created when a standby is present. These options reset the bogus host
        # so it won't interfere with commands that won't tolerate a connection failure.
        my $strBogusReset = $oHostBackup->bogusHost() ?
            ' --reset-pg2-host --reset-pg2-host-type --reset-pg2-host-cmd --reset-pg2-host-config --reset-pg2-host-user' .
                ' --reset-pg2-path' :
            '';

        # If S3 set process max to 2.  This seems like the best place for parallel testing since it will help speed S3 processing
        # without slowing down the other tests too much.
        if ($strStorage eq S3)
        {
            $oHostBackup->configUpdate({&CFGDEF_SECTION_GLOBAL => {'process-max' => 2}});
            $oHostDbPrimary->configUpdate({&CFGDEF_SECTION_GLOBAL => {'process-max' => 2}});
        }

        $oHostDbPrimary->clusterCreate();

        # Create the stanza
        $oHostDbPrimary->stanzaCreate('main create stanza info files');

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
        $oHostDbPrimary->sqlExecute('create database test1', {bAutoCommit => true});
        $oHostDbPrimary->sqlExecute('create database test2', {bAutoCommit => true});

        # ??? Removed temporarily until manifest build can be brought back into the check command
        # Create a directory in pg_data location that is only readable by root to ensure manifest->build is called by check
        # --------------------------------------------------------------------------------------------------------------------------
        # my $strDir = $oHostDbPrimary->dbBasePath() . '/rootreaddir';
        # executeTest('sudo mkdir ' . $strDir);
        # executeTest("sudo chown root:root ${strDir}");
        # executeTest("sudo chmod 400 ${strDir}");
        #
        # $strComment = 'confirm primary manifest->build executed';
        # $oHostDbPrimary->check($strComment, {iTimeout => 5, iExpectedExitStatus => ERROR_PATH_OPEN});
        # executeTest("sudo rmdir ${strDir}");

        # --------------------------------------------------------------------------------------------------------------------------
        my $strComment = 'verify check command runs successfully';

        $oHostDbPrimary->check($strComment, {iTimeout => 5});

        # Also run check on the backup host when present
        if ($bHostBackup)
        {
            $oHostBackup->check($strComment, {iTimeout => 5, strOptionalParam => $strBogusReset});
        }

        # Restart the cluster ignoring any errors in the postgresql log
        $oHostDbPrimary->clusterRestart({bIgnoreLogError => true});

        # Full backup
        #---------------------------------------------------------------------------------------------------------------------------
        # Create the table where test messages will be stored
        $oHostDbPrimary->sqlExecute("create table test (message text not null)");
        $oHostDbPrimary->sqlWalRotate();
        $oHostDbPrimary->sqlExecute("insert into test values ('$strDefaultMessage')");

        # Acquire the backup advisory lock so it looks like a backup is running
        if (!$oHostDbPrimary->sqlSelectOne('select pg_try_advisory_lock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
        {
            confess 'unable to acquire advisory lock for testing';
        }

        $oHostBackup->backup(
            CFGOPTVAL_BACKUP_TYPE_FULL, 'fail on backup lock exists', {iExpectedExitStatus => ERROR_LOCK_ACQUIRE});

        # Release the backup advisory lock so the next backup will succeed
        if (!$oHostDbPrimary->sqlSelectOne('select pg_advisory_unlock(' . DB_BACKUP_ADVISORY_LOCK . ')'))
        {
            confess 'unable to release advisory lock';
        }

        $oHostDbPrimary->sqlExecute("update test set message = '$strFullMessage'");

        # Required to set hint bits to be sent to the standby to make the heap match on both sides
        $oHostDbPrimary->sqlSelectOneTest('select message from test', $strFullMessage);

        # Backup to repo1
        my $strFullBackup = $oHostBackup->backup(
            CFGOPTVAL_BACKUP_TYPE_FULL, 'repo1',
            {strOptionalParam => ' --buffer-size=16384'});

        # Backup to repo2 if it exists
        if ($iRepoTotal == 2)
        {
            $oHostBackup->backup(CFGOPTVAL_BACKUP_TYPE_FULL, 'repo2', {iRepo => 2});
        }

        # Make a new backup with expire-auto disabled then run the expire command and compare backup numbers to ensure that expire
        # was really disabled. This test is not version specific so is run on only one version.
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bNonVersionSpecific)
        {
            my $oBackupInfo = new pgBackRestTest::Env::BackupInfo($oHostBackup->repoBackupPath());
            push(my @backupLst1, $oBackupInfo->list());

            $strFullBackup = $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_FULL, 'with disabled expire-auto',
                {strOptionalParam => ' --repo1-retention-full='.scalar(@backupLst1). ' --no-expire-auto'});

            $oBackupInfo = new pgBackRestTest::Env::BackupInfo($oHostBackup->repoBackupPath());
            push(my @backupLst2, $oBackupInfo->list());

            &log(INFO, "    run the expire command");
            $oHostBackup->expire({iRetentionFull => scalar(@backupLst1)});
            $oBackupInfo = new pgBackRestTest::Env::BackupInfo($oHostBackup->repoBackupPath());
            push(my @backupLst3, $oBackupInfo->list());

            unless (scalar(@backupLst2) == scalar(@backupLst1) + 1 && scalar(@backupLst1) == scalar(@backupLst3))
            {
                confess "expire-auto option didn't work as expected";
            }
        }

        # Enabled async archiving
        $oHostBackup->configUpdate({&CFGDEF_SECTION_GLOBAL => {'archive-async' => 'y'}});

        # Kick out a bunch of archive logs to exercise async archiving.  Only do this when compressed and remote to slow it
        # down enough to make it evident that the async process is working.
        if ($strCompressType ne NONE && $strBackupDestination eq HOST_BACKUP)
        {
            &log(INFO, '    multiple wal switches to exercise async archiving');
            $oHostDbPrimary->sqlExecute("create table wal_activity (id int)");
            $oHostDbPrimary->sqlWalRotate();
            $oHostDbPrimary->sqlExecute("insert into wal_activity values (1)");
            $oHostDbPrimary->sqlWalRotate();
            $oHostDbPrimary->sqlExecute("insert into wal_activity values (2)");
            $oHostDbPrimary->sqlWalRotate();
            $oHostDbPrimary->sqlExecute("insert into wal_activity values (3)");
            $oHostDbPrimary->sqlWalRotate();
            $oHostDbPrimary->sqlExecute("insert into wal_activity values (4)");
            $oHostDbPrimary->sqlWalRotate();
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
                        ' --recovery-option="primary_conninfo=host=' . HOST_DB_PRIMARY .
                        ' port=' . $oHostDbPrimary->pgPort() . ' user=replicator"'});

            $oHostDbStandby->clusterStart({bHotStandby => true});

            # Make sure streaming replication is on
            $oHostDbPrimary->sqlSelectOneTest(
                "select client_addr || '-' || state from pg_stat_replication", $oHostDbStandby->ipGet() . '/32-streaming');

            # Check that the cluster was restored properly
            $oHostDbStandby->sqlSelectOneTest('select message from test', $strFullMessage);

            # Update message for standby
            $oHostDbPrimary->sqlExecute("update test set message = '$strStandbyMessage'");

            if ($oHostDbStandby->pgVersion() >= PG_VERSION_BACKUP_STANDBY && !$bTls)
            {
                # If there is only a primary and a replica and the replica is the backup destination, then if pg2-host and
                # pg256-host are BOGUS, confirm failure to reach the primary
                if (!$bHostBackup && $bHostStandby && $strBackupDestination eq HOST_DB_STANDBY)
                {
                    my $strStandbyBackup = $oHostBackup->backup(
                        CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby, failure to reach primary',
                        {bStandby => true, iExpectedExitStatus => ERROR_DB_CONNECT, strOptionalParam => '--pg256-host=' . BOGUS});
                }
                else
                {
                    my $strStandbyBackup = $oHostBackup->backup(
                        CFGOPTVAL_BACKUP_TYPE_FULL, 'backup from standby, failure to access at least one standby',
                        {bStandby => true, iExpectedExitStatus => ERROR_DB_CONNECT, strOptionalParam => '--pg256-host=' . BOGUS});
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
            $oHostDbStandby->check('verify check command on standby', {strOptionalParam => $strBogusReset});

            # Shutdown the standby before creating tablespaces (this will error since paths are different)
            $oHostDbStandby->clusterStop({bIgnoreLogError => true});
        }

        my $strAdhocBackup;

        # Execute stop and make sure the backup fails
        #---------------------------------------------------------------------------------------------------------------------------
        # Restart the cluster to check for any errors before continuing since the stop tests will definitely create errors and the
        # logs will need to be deleted to avoid causing issues further down the line. This test is not version specific so is run on
        # only one version.
        if ($bNonVersionSpecific)
        {
            confess "test must be performed on posix storage" if $strStorage ne POSIX;

            $oHostDbPrimary->clusterRestart();

            # Add backup for adhoc expire
            $strAdhocBackup = $oHostBackup->backup(CFGOPTVAL_BACKUP_TYPE_DIFF, 'backup for adhoc expire');

            $oHostDbPrimary->stop();

            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'attempt backup when stopped',
                {iExpectedExitStatus => $oHostBackup == $oHostDbPrimary ? ERROR_STOP : ERROR_DB_CONNECT});

            $oHostDbPrimary->start();
        }

        # Setup the time targets
        #---------------------------------------------------------------------------------------------------------------------------
        # If the tests are running quickly then the time target might end up the same as the end time of the prior full backup. That
        # means restore auto-select will not pick it as a candidate and restore the last backup instead causing the restore compare
        # to fail. So, sleep one second.
        sleep(1);

        $oHostDbPrimary->sqlExecute("update test set message = '$strTimeMessage'");
        $oHostDbPrimary->sqlWalRotate();
        my $strTimeTarget = $oHostDbPrimary->sqlSelectOne("select current_timestamp");
        &log(INFO, "        time target is ${strTimeTarget}");

        # Incr backup - fail on archive_mode=always when version >= 9.5
        #---------------------------------------------------------------------------------------------------------------------------
        if ($oHostDbPrimary->pgVersion() >= PG_VERSION_95)
        {
            # Set archive_mode=always
            $oHostDbPrimary->clusterRestart({bArchiveAlways => true});

            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'fail on archive_mode=always', {iExpectedExitStatus => ERROR_FEATURE_NOT_SUPPORTED});

            # Reset the cluster to a normal state so the next test will work
            $oHostDbPrimary->clusterRestart();
        }

        # Incr backup
        #---------------------------------------------------------------------------------------------------------------------------
        # Create a tablespace directory
        storageTest()->pathCreate($oHostDbPrimary->tablespacePath(1), {strMode => '0700', bCreateParent => true});

        # Also create it on the standby so replay won't fail
        if (defined($oHostDbStandby))
        {
            storageTest()->pathCreate($oHostDbStandby->tablespacePath(1), {strMode => '0700', bCreateParent => true});
        }

        $oHostDbPrimary->sqlExecute(
            "create tablespace ts1 location '" . $oHostDbPrimary->tablespacePath(1) . "'", {bAutoCommit => true});
        $oHostDbPrimary->sqlExecute("alter table test set tablespace ts1");

        # Create a table in the tablespace that will not be modified again to be sure it does get full page writes in the WAL later
        $oHostDbPrimary->sqlExecute("create table test_exists (id int) tablespace ts1", {bCommit => true, bCheckPoint => true});

        # Create a table in the tablespace
        $oHostDbPrimary->sqlExecute("create table test_remove (id int)");
        $oHostDbPrimary->sqlWalRotate();
        $oHostDbPrimary->sqlExecute("update test set message = '$strDefaultMessage'");
        $oHostDbPrimary->sqlWalRotate();

        # Create a database in the tablespace and a table to check
        $oHostDbPrimary->sqlExecute("create database test3 with tablespace ts1", {bAutoCommit => true});
        $oHostDbPrimary->sqlExecute(
            'create table test3_exists (id int);' .
            'insert into test3_exists values (1);',
            {strDb => 'test3', bAutoCommit => true});

        # Create a table in test1 to check - test1 will not be restored
        $oHostDbPrimary->sqlExecute(
            'create table test1_zeroed (id int);' .
            'insert into test1_zeroed values (1);',
            {strDb => 'test1', bAutoCommit => true});

        # Start a backup so the next backup has to restart it.  This test is not required for PostgreSQL >= 9.6 since backups
        # are run in non-exclusive mode.
        if ($oHostDbPrimary->pgVersion() >= PG_VERSION_93 && $oHostDbPrimary->pgVersion() < PG_VERSION_96)
        {
            $oHostDbPrimary->sqlSelectOne("select pg_start_backup('test backup that will cause an error', true)");

            # Verify that an error is returned if the backup is already running
            $oHostBackup->backup(
                CFGOPTVAL_BACKUP_TYPE_INCR, 'fail on backup already running', {iExpectedExitStatus => ERROR_DB_QUERY});

            # Restart the cluster ignoring any errors in the postgresql log
            $oHostDbPrimary->clusterRestart({bIgnoreLogError => true});

            # Start a new backup to make the next test restarts it
            $oHostDbPrimary->sqlSelectOne("select pg_start_backup('test backup that will be restarted', true)");
        }

        if (defined($strAdhocBackup))
        {
            # Adhoc expire the latest backup - no other tests should be affected
            $oHostBackup->expire({strOptionalParam => '--set=' . $strAdhocBackup});
        }

        # Drop a table
        $oHostDbPrimary->sqlExecute('drop table test_remove');
        $oHostDbPrimary->sqlWalRotate();
        $oHostDbPrimary->sqlExecute("update test set message = '$strIncrMessage'", {bCommit => true});

        # Exercise --delta checksum option
        my $strIncrBackup = $oHostBackup->backup(
            CFGOPTVAL_BACKUP_TYPE_INCR, 'delta',
            {strOptionalParam => '--stop-auto --buffer-size=32768 --delta', iRepo => $iRepoTotal});

        # Ensure the check command runs properly with a tablespace
        $oHostBackup->check( 'check command with tablespace', {iTimeout => 5, strOptionalParam => $strBogusReset});

        # Setup the xid target
        #---------------------------------------------------------------------------------------------------------------------------
        my $strXidTarget = undef;

        $oHostDbPrimary->sqlExecute("update test set message = '$strXidMessage'", {bCommit => false});
        $oHostDbPrimary->sqlWalRotate();
        $strXidTarget = $oHostDbPrimary->sqlSelectOne("select txid_current()");
        $oHostDbPrimary->sqlCommit();
        &log(INFO, "        xid target is ${strXidTarget}");

        # Setup the name target
        #---------------------------------------------------------------------------------------------------------------------------
        my $strNameTarget = 'backrest';

        $oHostDbPrimary->sqlExecute("update test set message = '$strNameMessage'", {bCommit => true});
        $oHostDbPrimary->sqlWalRotate();

        if ($oHostDbPrimary->pgVersion() >= PG_VERSION_91)
        {
            $oHostDbPrimary->sqlExecute("select pg_create_restore_point('${strNameTarget}')");
        }

        &log(INFO, "        name target is ${strNameTarget}");

        # Create a table and data in database test2
        #---------------------------------------------------------------------------------------------------------------------------
        # Initialize variables for SHA1 and path of the pg_filenode.map for the database that will not be restored
        my $strDb1TablePath;
        my $strDb1TableSha1;

        $oHostDbPrimary->sqlExecute(
            'create table test (id int);' .
            'insert into test values (1);' .
            'create table test_ts1 (id int) tablespace ts1;' .
            'insert into test_ts1 values (2);',
            {strDb => 'test2', bAutoCommit => true});

        $oHostDbPrimary->sqlWalRotate();

        # Get the SHA1 and path of the table for the database that will not be restored
        $strDb1TablePath = $oHostDbPrimary->dbBasePath(). "/base/" .
            $oHostDbPrimary->sqlSelectOne("select oid from pg_database where datname='test1'") . "/" .
            $oHostDbPrimary->sqlSelectOne("select relfilenode from pg_class where relname='test1_zeroed'", {strDb => 'test1'});
        $strDb1TableSha1 = storageTest()->hashSize($strDb1TablePath);

        # Restore (type = default)
        #---------------------------------------------------------------------------------------------------------------------------
        # Expect failure because pg (appears to be) running
        $oHostDbPrimary->restore('pg running', 'latest', {iExpectedExitStatus => ERROR_PG_RUNNING});

        $oHostDbPrimary->clusterStop();

        # Expect failure because db path is not empty
        $oHostDbPrimary->restore('path not empty', 'latest', {iExpectedExitStatus => ERROR_PATH_NOT_EMPTY});

        # Drop and recreate db path
        testPathRemove($oHostDbPrimary->dbBasePath());
        storageTest()->pathCreate($oHostDbPrimary->dbBasePath(), {strMode => '0700'});
        testPathRemove($oHostDbPrimary->dbPath() . qw{/} . $oManifest->walPath());
        storageTest()->pathCreate($oHostDbPrimary->dbPath() . qw{/} . $oManifest->walPath(), {strMode => '0700'});
        testPathRemove($oHostDbPrimary->tablespacePath(1));
        storageTest()->pathCreate($oHostDbPrimary->tablespacePath(1), {strMode => '0700'});

        # Now the restore should work
        $oHostDbPrimary->restore(
            undef, 'latest',
            {strOptionalParam => ' --db-include=test2 --db-include=test3 --buffer-size=16384', iRepo => $iRepoTotal});

        # Test that the first database has not been restored since --db-include did not include test1
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

        $oHostDbPrimary->clusterStart();
        $oHostDbPrimary->sqlSelectOneTest('select message from test', $strNameMessage);

        # Once the cluster is back online, make sure the database & table in the tablespace exists properly
        $oHostDbPrimary->sqlSelectOneTest('select id from test_ts1', 2, {strDb => 'test2'});
        $oHostDbPrimary->sqlDisconnect({strDb => 'test2'});

        $oHostDbPrimary->sqlSelectOneTest('select id from test3_exists', 1, {strDb => 'test3'});
        $oHostDbPrimary->sqlDisconnect({strDb => 'test3'});

        # The tablespace path should exist and have files in it
        my $strTablespacePath = $oHostDbPrimary->tablespacePath(1);

        # Backup info will have the catalog number
        my $oBackupInfo = new pgBackRestDoc::Common::Ini(
            storageRepo(), $oHostBackup->repoBackupPath(FILE_BACKUP_INFO),
            {bLoad => false, strContent => ${storageRepo()->get($oHostBackup->repoBackupPath(FILE_BACKUP_INFO))}});

        # Construct the special path
        $strTablespacePath .=
            '/PG_' . $oHostDbPrimary->pgVersion() . qw{_} . $oBackupInfo->get(INFO_BACKUP_SECTION_DB, INFO_BACKUP_KEY_CATALOG);

        # Check that path exists
        if (!storageTest()->pathExists($strTablespacePath))
        {
            confess &log(ASSERT, "unable to find tablespace path '${strTablespacePath}'");
        }

        # Make sure there are some files in the tablespace path
        if (grep(!/^PG\_VERSION$/i, storageTest()->list($strTablespacePath)) == 0)
        {
            confess &log(ASSERT, "no files found in tablespace path '${strTablespacePath}'");
        }

        # This table should exist to prove that the tablespace was restored.  It has not been updated since it was created so it
        # should not be created by any full page writes.  Once it is verified to exist it can be dropped.
        $oHostDbPrimary->sqlSelectOneTest("select count(*) from test_exists", 0);
        $oHostDbPrimary->sqlExecute('drop table test_exists');

        # Now it should be OK to drop database test2 and test3
        $oHostDbPrimary->sqlExecute('drop database test2', {bAutoCommit => true});

        # The test table lives in ts1 so it needs to be moved or dropped
        $oHostDbPrimary->sqlExecute('alter table test set tablespace pg_default');

        # And drop the tablespace
        $oHostDbPrimary->sqlExecute('drop database test3', {bAutoCommit => true});
        $oHostDbPrimary->sqlExecute("drop tablespace ts1", {bAutoCommit => true});

        # Restore (restore type = immediate, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($oHostDbPrimary->pgVersion() >= PG_VERSION_94)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_IMMEDIATE);

            $oHostDbPrimary->clusterStop();

            $oHostDbPrimary->restore(
                undef, $strFullBackup, {bForce => true, strType => CFGOPTVAL_RESTORE_TYPE_IMMEDIATE, strTargetAction => 'promote'});

            $oHostDbPrimary->clusterStart();
            $oHostDbPrimary->sqlSelectOneTest(
                'select message from test', ($bHostStandby ? $strStandbyMessage : $strFullMessage));
        }

        # Restore (restore type = xid, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        my $strRecoveryFile = undef;

        &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_XID);

        $oHostDbPrimary->clusterStop();

        executeTest('rm -rf ' . $oHostDbPrimary->dbBasePath() . "/*");
        executeTest('rm -rf ' . $oHostDbPrimary->dbPath() . qw{/} . $oManifest->walPath() . '/*');

        $oHostDbPrimary->restore(
            undef, $strIncrBackup,
            {bForce => true, strType => CFGOPTVAL_RESTORE_TYPE_XID, strTarget => $strXidTarget,
                strTargetAction => $oHostDbPrimary->pgVersion() >= PG_VERSION_91 ? 'promote' : undef,
                strTargetTimeline => $oHostDbPrimary->pgVersion() >= PG_VERSION_12 ? 'current' : undef,
                strOptionalParam => '--tablespace-map-all=../../tablespace', bTablespace => false,
                iRepo => $iRepoTotal});

        # Save recovery file to test so we can use it in the next test
        $strRecoveryFile = $oHostDbPrimary->pgVersion() >= PG_VERSION_12 ? 'postgresql.auto.conf' : DB_FILE_RECOVERYCONF;

        storageTest()->copy(
            $oHostDbPrimary->dbBasePath() . qw{/} . $strRecoveryFile, $self->testPath() . qw{/} . $strRecoveryFile);

        $oHostDbPrimary->clusterStart();
        $oHostDbPrimary->sqlSelectOneTest('select message from test', $strXidMessage);

        $oHostDbPrimary->sqlExecute("update test set message = '$strTimelineMessage'");

        # Restore (restore type = preserve, inclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_PRESERVE);

        $oHostDbPrimary->clusterStop();

        executeTest('rm -rf ' . $oHostDbPrimary->dbBasePath() . "/*");
        executeTest('rm -rf ' . $oHostDbPrimary->dbPath() . qw{/} . $oManifest->walPath() . '/*');
        executeTest('rm -rf ' . $oHostDbPrimary->tablespacePath(1) . "/*");

        # Restore recovery file that was saved in last test
        storageTest()->move($self->testPath . "/${strRecoveryFile}", $oHostDbPrimary->dbBasePath() . "/${strRecoveryFile}");

        # Also touch recovery.signal when required
        if ($oHostDbPrimary->pgVersion() >= PG_VERSION_12)
        {
            storageTest()->put($oHostDbPrimary->dbBasePath() . "/" . DB_FILE_RECOVERYSIGNAL);
        }

        $oHostDbPrimary->restore(undef, 'latest', {strType => CFGOPTVAL_RESTORE_TYPE_PRESERVE});

        $oHostDbPrimary->clusterStart();
        $oHostDbPrimary->sqlSelectOneTest('select message from test', $strXidMessage);

        $oHostDbPrimary->sqlExecute("update test set message = '$strTimelineMessage'");

        # Restore (restore type = time, inclusive, automatically select backup) - there is no exclusive time test because I can't
        # find a way to find the exact commit time of a transaction.
        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_TIME);

        $oHostDbPrimary->clusterStop();

        $oHostDbPrimary->restore(
            undef, 'latest',
            {bDelta => true, strType => CFGOPTVAL_RESTORE_TYPE_TIME, strTarget => $strTimeTarget,
                strTargetAction => $oHostDbPrimary->pgVersion() >= PG_VERSION_91 ? 'promote' : undef,
                strTargetTimeline => $oHostDbPrimary->pgVersion() >= PG_VERSION_12 ? 'current' : undef,
                strBackupExpected => $strFullBackup});

        $oHostDbPrimary->clusterStart();
        $oHostDbPrimary->sqlSelectOneTest('select message from test', $strTimeMessage);

        # Restore (restore type = xid, exclusive)
        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_XID);

        $oHostDbPrimary->clusterStop();

        $oHostDbPrimary->restore(
            undef, $strIncrBackup,
            {bDelta => true, strType => CFGOPTVAL_RESTORE_TYPE_XID, strTarget => $strXidTarget, bTargetExclusive => true,
                strTargetAction => $oHostDbPrimary->pgVersion() >= PG_VERSION_91 ? 'promote' : undef,
                strTargetTimeline => $oHostDbPrimary->pgVersion() >= PG_VERSION_12 ? 'current' : undef,
                iRepo => $iRepoTotal});

        $oHostDbPrimary->clusterStart();
        $oHostDbPrimary->sqlSelectOneTest('select message from test', $strIncrMessage);

        # Restore (restore type = name)
        #---------------------------------------------------------------------------------------------------------------------------
        if ($oHostDbPrimary->pgVersion() >= PG_VERSION_91)
        {
            &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_NAME);

            $oHostDbPrimary->clusterStop();

            $oHostDbPrimary->restore(
                undef, 'latest',
                {bDelta => true, bForce => true, strType => CFGOPTVAL_RESTORE_TYPE_NAME, strTarget => $strNameTarget,
                    strTargetAction => 'promote',
                    strTargetTimeline => $oHostDbPrimary->pgVersion() >= PG_VERSION_12 ? 'current' : undef});

            $oHostDbPrimary->clusterStart();
            $oHostDbPrimary->sqlSelectOneTest('select message from test', $strNameMessage);
        }

        # Restore (restore type = default, timeline = created by type = xid, inclusive recovery)
        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO, '    testing recovery type = ' . CFGOPTVAL_RESTORE_TYPE_DEFAULT);

        $oHostDbPrimary->clusterStop();

        # The timeline to use for this test is subject to change based on tests being added or removed above.  The best thing
        # would be to automatically grab the timeline after the restore, but since this test has been stable for a long time
        # it does not seem worth the effort to automate.
        $oHostDbPrimary->restore(
            undef, $strIncrBackup,
            {bDelta => true, strType => CFGOPTVAL_RESTORE_TYPE_STANDBY, strTargetTimeline => 4, iRepo => $iRepoTotal});

        $oHostDbPrimary->clusterStart({bHotStandby => true});
        $oHostDbPrimary->sqlSelectOneTest('select message from test', $strTimelineMessage, {iTimeout => 120});

        # Stop clusters to catch any errors in the postgres log. Stopping the cluster has started consistently running out of memory
        # on PostgreSQL 9.1. This seems to have happened after pulling in new packages at some point so it might be build related.
        # Stopping the cluster is not critical for 9.1 so skip it.
        #---------------------------------------------------------------------------------------------------------------------------
        $oHostDbPrimary->clusterStop({bStop => $oHostDbPrimary->pgVersion() != PG_VERSION_91});

        # Stanza-delete --force without access to pgbackrest on database host. This test is not version specific so is run on only
        # one version.
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bNonVersionSpecific)
        {
            # Make sure this test has a backup host to work with
            confess "test must run with backup dst = " . HOST_BACKUP if !$bHostBackup;

            $oHostDbPrimary->stop();
            $oHostBackup->stop({strStanza => $self->stanza});
            $oHostBackup->stanzaDelete(
                "delete stanza with --force when pgbackrest on pg host not accessible", {strOptionalParam => ' --force'});
            $oHostDbPrimary->start();
            $oHostBackup->start();
        }
    }
}

1;
