####################################################################################################################################
# FullSyntheticTest.pm - Tests for all commands that can be run against synthetic data
####################################################################################################################################
package pgBackRestTest::Module::Full::FullSyntheticTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(basename dirname);

use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Backup::Common;
use pgBackRest::Backup::Info;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::InfoCommon;
use pgBackRest::LibC qw(:checksum);
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Version;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# Build PostgreSQL pages for testing
####################################################################################################################################
sub pageBuild
{
    my $tPageSource = shift;
    my $iBlockNo = shift;
    my $iWalId = shift;
    my $iWalOffset = shift;

    my $tPage = defined($iWalId) ? pack('I', $iWalId) . pack('I', $iWalOffset) . substr($tPageSource, 8) : $tPageSource;
    my $iChecksum = pageChecksum($tPage, $iBlockNo, length($tPage));

    return substr($tPage, 0, 8) . pack('S', $iChecksum) . substr($tPage, 10);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    for (my $bRemote = false; $bRemote <= true; $bRemote++)
    {
    for (my $bCompress = false; $bCompress <= true; $bCompress++)
    {
    for (my $bHardLink = false; $bHardLink <= true; $bHardLink++)
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("rmt ${bRemote}, cmp ${bCompress}, hardlink ${bHardLink}", $self->processMax() == 1)) {next}

        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup) = $self->setup(
            true, $self->expect(), {bHostBackup => $bRemote, bCompress => $bCompress, bHardLink => $bHardLink});

        # Determine if this is a neutral test, i.e. we only want to do it once for local and once for remote.  Neutral means
        # that options such as compression and hardlinks are disabled
        my $bNeutralTest = !$bCompress && !$bHardLink;

        # Get base time
        my $lTime = time() - 10000;

        # Build the manifest
        my %oManifest;

        $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_VERSION} = BACKREST_VERSION;
        $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_FORMAT} = BACKREST_FORMAT;
        $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_SEQUENCE} = 1;
        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_CHECK} = JSON::PP::true;
        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_COPY} = JSON::PP::true;
        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_BACKUP_STANDBY} = JSON::PP::false;
        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE} = JSON::PP::true;
        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS} = $bCompress ? JSON::PP::true : JSON::PP::false;
        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK} = $bHardLink ? JSON::PP::true : JSON::PP::false;
        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ONLINE} = JSON::PP::false;

        $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG} = 201409291;
        $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CONTROL} = 942;
        $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_SYSTEM_ID} = 6353949018581704918;
        $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} = PG_VERSION_94;
        $oManifest{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_ID} = 1;

        $oManifest{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_PATH} =
            $oHostDbMaster->dbBasePath();
        $oManifest{&MANIFEST_SECTION_BACKUP_TARGET}{&MANIFEST_TARGET_PGDATA}{&MANIFEST_SUBKEY_TYPE} = MANIFEST_VALUE_PATH;

        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA);

        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_PGVERSION, PG_VERSION_94,
                                              '184473f470864e067ee3a22e64b47b0a1c356f29', $lTime, undef, true);

        # Load sample page
        my $tBasePage = ${storageTest()->get($self->dataPath() . '/page.bin')};
        my $iBasePageChecksum = 0x1B99;

        # Create base path
        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base');
        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/1');

        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/1/12000', $tBasePage,
                                              '22c98d248ff548311eda88559e4a8405ed77c003', $lTime);
        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/1/' . DB_FILE_PGVERSION,
                                              PG_VERSION_94, '184473f470864e067ee3a22e64b47b0a1c356f29', $lTime, '660');

        if ($bNeutralTest && !$bRemote)
        {
            executeTest('sudo chown 7777 ' . $oHostDbMaster->dbBasePath() . '/base/1/' . DB_FILE_PGVERSION);
            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/base/1/' . DB_FILE_PGVERSION}
                      {&MANIFEST_SUBKEY_USER} = INI_FALSE;
        }

        my $tPageInvalid17000 = $tBasePage . $tBasePage;

        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384');

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', $tPageInvalid17000,
            'e0101dd8ffb910c9c202ca35b5f828bcb9697bed', $lTime, undef, undef, '1');
        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/' . DB_FILE_PGVERSION,
                                              PG_VERSION_94, '184473f470864e067ee3a22e64b47b0a1c356f29', $lTime);

        if ($bNeutralTest && !$bRemote)
        {
            executeTest('sudo chown :7777 ' . $oHostDbMaster->dbBasePath() . '/base/16384/' . DB_FILE_PGVERSION);
            $oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/base/16384/' . DB_FILE_PGVERSION}
                      {&MANIFEST_SUBKEY_GROUP} = INI_FALSE;
        }

        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768');

        my $tPageValid =
            pageBuild($tBasePage, 0) .
            pageBuild($tBasePage, 1) .
            pageBuild($tBasePage, 2) .
            pageBuild($tBasePage, 0, 0xFFFF, 0xFFFF);

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768/33000', $tPageValid, '4a383e4fb8b5cd2a4e8fab91ef63dce48e532a2f',
            $lTime);

        my $iBlockOffset = 32767 * 131072;

        my $tPageValidSeg32767 =
            pageBuild($tBasePage, $iBlockOffset + 0) .
            pageBuild($tBasePage, $iBlockOffset + 1) .
            ("\0" x 8192) .
            pageBuild($tBasePage, 0, 0xFFFF, 0xFFFF);

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768/33000.32767', $tPageValidSeg32767,
            '21e2c7c1a326682c07053b7d6a5a40dbd49c2ec5', $lTime);

        my $tPageInvalid33001 =
            pageBuild($tBasePage, 1) .
            pageBuild($tBasePage, 1) .
            pageBuild($tBasePage, 2) .
            pageBuild($tBasePage, 0) .
            pageBuild($tBasePage, 0) .
            pageBuild($tBasePage, 0) .
            pageBuild($tBasePage, 6) .
            pageBuild($tBasePage, 0);

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768/33001', $tPageInvalid33001,
            '6bf316f11d28c28914ea9be92c00de9bea6d9a6b', $lTime, undef, undef, '0, [3, 5], 7');

        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/32768/' . DB_FILE_PGVERSION,
                                              PG_VERSION_94, '184473f470864e067ee3a22e64b47b0a1c356f29', $lTime);

        # Create global path
        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'global');

        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_PGCONTROL, '[replaceme]',
                                              '89373d9f2973502940de06bc5212489df3f8a912', $lTime - 100, undef, true);

        # Copy pg_control
        executeTest(
            'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' .  $oHostDbMaster->dbBasePath() . '/' .
            DB_FILE_PGCONTROL);
        utime($lTime - 100, $lTime - 100, $oHostDbMaster->dbBasePath() . '/' . DB_FILE_PGCONTROL)
            or confess &log(ERROR, "unable to set time");
        $oManifest{&MANIFEST_SECTION_TARGET_FILE}{MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGCONTROL}
                  {&MANIFEST_SUBKEY_SIZE} = 8192;

        # Create tablespace path
        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGTBLSPC);

        # Create paths/files to ignore
        if ($bNeutralTest && !$bRemote)
        {
            # Create temp dir and file that will be ignored
            $oHostDbMaster->dbPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/' . DB_FILE_PREFIX_TMP);
            $oHostDbMaster->dbFileCreate(
                \%oManifest, MANIFEST_TARGET_PGDATA, 'base/' . DB_FILE_PREFIX_TMP . '/' . DB_FILE_PREFIX_TMP . '.1', 'IGNORE');

            # Create pg_dynshmem dir and file - only file will be ignored
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGDYNSHMEM);
            # $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGDYNSHMEM . '/anything.tmp', 'IGNORE');

            # Create pg_notify dir and file - only file will be ignored
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGNOTIFY);
            # $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGNOTIFY . '/anything.tmp', 'IGNORE');

            # Create pg_replslot dir and file - only file will be ignored
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGREPLSLOT);
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGREPLSLOT . '/anything.tmp', 'IGNORE');

            # Create pg_serial dir and file - only file will be ignored
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSERIAL);
            # $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSERIAL . '/anything.tmp', 'IGNORE');

            # Create pg_snaphots dir and file - only file will be ignored
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSNAPSHOTS);
            # $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSNAPSHOTS . '/anything.tmp', 'IGNORE');

            # Create pg_stat_tmp dir and file - only file will be ignored
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSTATTMP);
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSTATTMP . '/anything.tmp', 'IGNORE');

            # Create pg_subtrans dir and file - only file will be ignored
            $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSUBTRANS);
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_PATH_PGSUBTRANS . '/anything.tmp', 'IGNORE');

            # More files to ignore
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_POSTGRESQLAUTOCONFTMP, 'IGNORE');
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_POSTMASTEROPTS, 'IGNORE');
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_RECOVERYCONF, 'IGNORE');
            $oHostDbMaster->dbFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, DB_FILE_RECOVERYDONE, 'IGNORE');
        }

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

            # ??? This test is flapping and needs to implemented as a unit test instead
            # if ($self->processMax() > 1)
            # {
            #     $strTestPoint = TEST_KEEP_ALIVE;
            # }
        }

        # Create the archive info file
        $oHostBackup->stanzaCreate('create required data for stanza', {strOptionalParam => '--no-' . OPTION_ONLINE});

        # Create a file link
        storageTest()->pathCreate($oHostDbMaster->dbPath() . '/pg_config', {strMode => '0700', bCreateParent => true});
        testFileCreate(
            $oHostDbMaster->dbPath() . '/pg_config/postgresql.conf', "listen_addresses = *\n", $lTime - 100);
        testLinkCreate($oHostDbMaster->dbPath() . '/pg_config/postgresql.conf.link', './postgresql.conf');

        $oHostDbMaster->manifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'postgresql.conf',
                                              '../pg_config/postgresql.conf', true);

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
        storageTest()->pathCreate($oHostDbMaster->dbPath() . '/pg_stat', {strMode => '0700', bCreateParent => true});
        $oHostDbMaster->manifestLinkCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_stat', '../pg_stat');
        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA . '/pg_stat', 'global.stat', 'stats',
                                              'e350d5ce0153f3e22d5db21cf2a4eff00f3ee877', $lTime - 100, undef, true);
        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'pg_clog');

        $strFullBackup = $oHostBackup->backup(
            $strType, 'create pg_stat link, pg_clog dir',
            {oExpectedManifest => \%oManifest,
             strOptionalParam => $strOptionalParam . ($bRemote ? ' --cmd-ssh=/usr/bin/ssh' : '') .
                ' --no-' . OPTION_REPO_SYNC . ' --' . OPTION_BUFFER_SIZE . '=16384 --' . OPTION_CHECKSUM_PAGE,
             strTest => $strTestPoint,
             fTestDelay => 0});

        # Error on backup option to check logging
        #-----------------------------------------------------------------------------------------------------------------------
        if ($bNeutralTest && !$bRemote)
        {
            $oHostBackup->backup(
                $strType, 'invalid cmd line',
                {oExpectedManifest => \%oManifest, strStanza => BOGUS, iExpectedExitStatus => ERROR_OPTION_REQUIRED});
        }

        # Test protocol timeout
        #-----------------------------------------------------------------------------------------------------------------------
        if ($bNeutralTest && $bRemote)
        {
            $oHostBackup->backup(
                $strType, 'protocol timeout',
                {oExpectedManifest => \%oManifest, strOptionalParam => '--protocol-timeout=1 --db-timeout=.1',
                 strTest => TEST_BACKUP_START, fTestDelay => 1, iExpectedExitStatus => ERROR_FILE_READ});

            # Remove the aborted backup so the next backup is not a resume
            testPathRemove($oHostBackup->repoPath() . '/temp/' . $self->stanza() . '.tmp');
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

            $oHostDbMaster->start({strStanza => $self->stanza()});
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
        storageTest()->pathCreate(dirname($strDoNotDeleteFile), {strMode => '0700', bCreateParent => true});
        testFileCreate($strDoNotDeleteFile, 'DONOTDELETE-1-2');

        storageTest()->pathCreate($oHostDbMaster->tablespacePath(1), {strMode => '0700', bCreateParent => true});
        testFileCreate($oHostDbMaster->tablespacePath(1) . '/donotdelete.txt', 'DONOTDELETE-1');
        storageTest()->pathCreate($oHostDbMaster->tablespacePath(2), {strMode => '0700', bCreateParent => true});
        testFileCreate($oHostDbMaster->tablespacePath(2) . '/donotdelete.txt', 'DONOTDELETE-2');
        storageTest()->pathCreate($oHostDbMaster->tablespacePath(2, 2), {strMode => '0700', bCreateParent => true});
        testFileCreate($oHostDbMaster->tablespacePath(2, 2) . '/donotdelete.txt', 'DONOTDELETE-2-2');
        storageTest()->pathCreate($oHostDbMaster->tablespacePath(11), {strMode => '0700', bCreateParent => true});

        # Resume by copying the valid full backup over the last aborted full backup if it exists, or by creating a new path
        my $strResumeBackup = (storageRepo()->list(
            STORAGE_REPO_BACKUP, {strExpression => backupRegExpGet(true, true, true), strSortOrder => 'reverse'}))[0];
        my $strResumePath = $oHostBackup->repoPath() . '/backup/' . $self->stanza() . '/' .
            ($strResumeBackup ne $strFullBackup ? $strResumeBackup : backupLabel(storageRepo(), $strType, undef, time()));

        executeTest("sudo rm -rf ${strResumePath}");
        executeTest(
            'sudo mv ' . $oHostBackup->repoPath() . '/backup/' . $self->stanza() . "/${strFullBackup} ${strResumePath}");

        $oHostBackup->manifestMunge(
            basename($strResumePath),
            {&MANIFEST_SECTION_TARGET_FILE =>
                {(&MANIFEST_TARGET_PGDATA . '/' . &DB_FILE_PGVERSION) => {&MANIFEST_SUBKEY_CHECKSUM => undef}}},
            false);
        executeTest("sudo rm -f ${strResumePath}/" . FILE_MANIFEST);

        # Create a temp file in backup temp root to be sure it's deleted correctly
        executeTest("sudo touch ${strResumePath}/file.tmp" . ($bCompress ? '.gz' : ''),
                    {bRemote => $bRemote});
        executeTest("sudo chown " . BACKREST_USER . " ${strResumePath}/file.tmp" . ($bCompress ? '.gz' : ''));

        $strFullBackup = $oHostBackup->backup(
            $strType, 'resume',
            {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_RESUME,
                strOptionalParam => '--force --' . OPTION_CHECKSUM_PAGE});

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
            'add and delete files', undef,  ' --link-all' . ($bRemote ? ' --cmd-ssh=/usr/bin/ssh' : ''),
            undef, $bNeutralTest && !$bRemote ? 'root' : undef);

        # Fix permissions on the restore log & remove lock files
        if ($bNeutralTest && !$bRemote)
        {
            executeTest('sudo chown -R ' . TEST_USER . ':' . TEST_GROUP . ' ' . $oHostBackup->logPath());
            executeTest('sudo rm -rf ' . $oHostDbMaster->lockPath() . '/*');
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
                'error on existing linked path', ERROR_PATH_NOT_EMPTY, '--log-level-console=warn --link-all');

            executeTest('rm -rf ' . $oHostDbMaster->dbPath() . "/pg_stat/*");

            $oHostDbMaster->restore(
                $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'error on existing linked file', ERROR_PATH_NOT_EMPTY, '--log-level-console=warn --link-all');

            # Error when postmaster.pid is present
            executeTest('touch ' . $oHostDbMaster->dbBasePath() . qw(/) . DB_FILE_POSTMASTERPID);

            $oHostDbMaster->restore(
                $strFullBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
                'error on postmaster.pid exists', ERROR_POSTMASTER_RUNNING, '--log-level-console=warn');

            executeTest('rm ' . $oHostDbMaster->dbBasePath() . qw(/) . DB_FILE_POSTMASTERPID);

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
            'fail on missing ' . DB_FILE_PGVERSION, ERROR_PATH_NOT_EMPTY, '--log-level-console=detail');

        # Write a backup.manifest file to make $PGDATA valid
        testFileCreate($oHostDbMaster->dbBasePath() . '/backup.manifest', 'BOGUS');

        # Munge the user to make sure it gets reset on the next run
        $oHostBackup->manifestMunge(
            $strFullBackup,
            {&MANIFEST_SECTION_TARGET_FILE =>
                {&MANIFEST_FILE_PGCONTROL => {&MANIFEST_SUBKEY_USER => 'bogus', &MANIFEST_SUBKEY_GROUP => 'bogus'}}},
            false);

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

        # Break the database version
        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO),
            {&INFO_BACKUP_SECTION_DB => {&INFO_BACKUP_KEY_DB_VERSION => '8.0'}});

        $oHostBackup->backup(
            $strType, 'invalid database version',
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                strOptionalParam => '--log-level-console=detail'});

        # Break the database system id
        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO),
            {&INFO_BACKUP_SECTION_DB => {&INFO_BACKUP_KEY_SYSTEM_ID => 6999999999999999999}});

        $oHostBackup->backup(
            $strType, 'invalid system id',
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                strOptionalParam => '--log-level-console=detail'});

        # Break the control version
        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO),
            {&INFO_BACKUP_SECTION_DB => {&INFO_BACKUP_KEY_CONTROL => 842}});

        $oHostBackup->backup(
            $strType, 'invalid control version',
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                strOptionalParam => '--log-level-console=detail'});

        # Break the catalog version
        $oHostBackup->infoMunge(
            storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO),
            {&INFO_BACKUP_SECTION_DB => {&INFO_BACKUP_KEY_CATALOG => 197208141}});

        $oHostBackup->backup(
            $strType, 'invalid catalog version',
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_BACKUP_MISMATCH,
                strOptionalParam => '--log-level-console=detail'});

        # Restore the file to its original condition
        $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_BACKUP . qw{/} . FILE_BACKUP_INFO));

        # Test broken tablespace configuration
        #-----------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_INCR;
        my $strTblSpcPath = $oHostDbMaster->dbBasePath() . '/' . DB_PATH_PGTBLSPC;

        # Create a directory in pg_tablespace
        storageTest()->pathCreate("${strTblSpcPath}/path", {strMode => '0700', bCreateParent => true});

        $oHostBackup->backup(
            $strType, 'invalid path in ' . DB_PATH_PGTBLSPC,
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_LINK_EXPECTED,
             strOptionalParam => '--log-level-console=detail'});

        testPathRemove("${strTblSpcPath}/path");

        if ($bNeutralTest && !$bRemote)
        {
            # Create a relative link in PGDATA
            testLinkCreate("${strTblSpcPath}/99999", '../');

            $oHostBackup->backup(
                $strType, 'invalid relative tablespace is ../',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                    strOptionalParam => '--log-level-console=detail'});

            testFileRemove("${strTblSpcPath}/99999");

            testLinkCreate("${strTblSpcPath}/99999", '..');

            $oHostBackup->backup(
                $strType, 'invalid relative tablespace is ..',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                    strOptionalParam => '--log-level-console=detail'});

            testFileRemove("${strTblSpcPath}/99999");

            testLinkCreate("${strTblSpcPath}/99999", '../../base/');

            $oHostBackup->backup(
                $strType, 'invalid relative tablespace is ../../$PGDATA',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                    strOptionalParam => '--log-level-console=detail'});

            testFileRemove("${strTblSpcPath}/99999");

            testLinkCreate("${strTblSpcPath}/99999", '../../base');

            $oHostBackup->backup(
                $strType, 'invalid relative tablespace is ../../$PGDATA',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                    strOptionalParam => '--log-level-console=detail'});

            testFileRemove("${strTblSpcPath}/99999");

            # Create a link to a link
            testLinkCreate($oHostDbMaster->dbPath() . "/intermediate_link", $oHostDbMaster->dbPath() . '/tablespace/ts1');
            testLinkCreate("${strTblSpcPath}/99999", $oHostDbMaster->dbPath() . "/intermediate_link");

            $oHostBackup->backup(
                $strType, 'tablespace link references a link',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_LINK_DESTINATION,
                    strOptionalParam => '--log-level-console=detail'});

            testFileRemove($oHostDbMaster->dbPath() . "/intermediate_link");
            testFileRemove("${strTblSpcPath}/99999");
        }

        # Create a relative link in PGDATA
        testLinkCreate("${strTblSpcPath}/99999", '../invalid_tblspc');

        $oHostBackup->backup(
            $strType, 'invalid relative tablespace in $PGDATA',
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                strOptionalParam => '--log-level-console=detail'});

        testFileRemove("${strTblSpcPath}/99999");

        # Create tablespace with same initial dir name as $PGDATA
        if ($bNeutralTest && !$bRemote)
        {
            testLinkCreate("${strTblSpcPath}/99999", $oHostDbMaster->dbBasePath() . '_tbs');

            $oHostBackup->backup(
                $strType, '$PGDATA is a substring of valid tblspc excluding / (file missing err expected)',
                {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_FILE_MISSING,
                    strOptionalParam => '--log-level-console=detail'});

            testFileRemove("${strTblSpcPath}/99999");
        }

        # Create tablespace in PGDATA
        testLinkCreate("${strTblSpcPath}/99999", $oHostDbMaster->dbBasePath() . '/invalid_tblspc');

        $oHostBackup->backup(
            $strType, 'invalid tablespace in $PGDATA',
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_TABLESPACE_IN_PGDATA,
                strOptionalParam => '--log-level-console=detail'});

        testFileRemove("${strTblSpcPath}/99999");

        # Incr backup
        #-----------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_INCR;

        # Add tablespace 1
        $oHostDbMaster->manifestTablespaceCreate(\%oManifest, 1);
        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/1', '16384');

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGTBLSPC . '/1', '16384/tablespace1.txt', 'TBLSPC1',
            'd85de07d6421d90aa9191c11c889bfde43680f0f', $lTime, undef, undef, false);
        $oHostDbMaster->manifestFileCreate(\%oManifest, MANIFEST_TARGET_PGDATA, 'badchecksum.txt', 'BADCHECKSUM',
                                              'f927212cd08d11a42a666b2f04235398e9ceeb51', $lTime, undef, true);

        # Create temp dir and file that will be ignored
        if ($bNeutralTest && !$bRemote)
        {
            $oHostDbMaster->dbPathCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/1', DB_FILE_PREFIX_TMP);
            $oHostDbMaster->dbFileCreate(
                \%oManifest, MANIFEST_TARGET_PGTBLSPC . '/1', DB_FILE_PREFIX_TMP . '/' . DB_FILE_PREFIX_TMP . '.1', 'IGNORE');
        }

        my $strBackup = $oHostBackup->backup(
            $strType, 'add tablespace 1', {oExpectedManifest => \%oManifest, strOptionalParam => '--test'});

        # Resume Incr Backup
        #-----------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_INCR;

        # Create resumable backup from last backup
        $strResumePath =
            $oHostBackup->repoPath() . '/backup/' . $self->stanza() . '/' .
            backupLabel(storageRepo(), $strType, substr($strBackup, 0, 16), time());
        executeTest('sudo mv ' . $oHostBackup->repoPath() . '/backup/' . $self->stanza() . "/${strBackup} ${strResumePath}");

        $oHostBackup->manifestMunge(
            basename($strResumePath),
            {&MANIFEST_SECTION_TARGET_FILE =>
                {(&MANIFEST_TARGET_PGDATA . '/badchecksum.txt') => {&MANIFEST_SUBKEY_CHECKSUM => BOGUS}}},
            false);
        executeTest("sudo rm -f ${strResumePath}/" . FILE_MANIFEST);

        # Add tablespace 2
        $oHostDbMaster->manifestTablespaceCreate(\%oManifest, 2);
        $oHostDbMaster->manifestPathCreate(\%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768');

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2.txt', 'TBLSPC2',
            'dc7f76e43c46101b47acc55ae4d593a9e6983578', $lTime, undef, undef, false);

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/' . DB_FILE_PGINTERNALINIT, 'INIT',
            'bc46a4e0420d357db7bfbcb7b5fcbc613dc48c1b', $lTime);

        # Also create tablespace 11 to be sure it does not conflict with path of tablespace 1
        if ($bNeutralTest)
        {
            $oHostDbMaster->manifestTablespaceCreate(\%oManifest, 11);
        }

        $strBackup = $oHostBackup->backup(
            $strType, 'resume and add tablespace 2', {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_RESUME});

        # Resume Diff Backup
        #-----------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_DIFF;

        # Drop tablespace 11
        if ($bNeutralTest)
        {
            $oHostDbMaster->manifestTablespaceDrop(\%oManifest, 11);
        }

        # Create resumable backup from last backup
        $strResumePath = $oHostBackup->repoPath() . '/backup/' . $self->stanza() . "/${strBackup}";
        executeTest("sudo rm -f ${strResumePath}/" . FILE_MANIFEST);

        $strBackup = $oHostBackup->backup(
            $strType, 'cannot resume - new diff',
            {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_NORESUME,
                strOptionalParam => '--log-level-console=detail'});

        # Resume Diff Backup
        #-----------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_DIFF;

        # Create resumable backup from last backup
        $strResumePath = $oHostBackup->repoPath() . '/backup/' . $self->stanza() . "/${strBackup}";
        executeTest("sudo rm -f ${strResumePath}/" . FILE_MANIFEST);

        $strBackup = $oHostBackup->backup(
            $strType, 'cannot resume - disabled / no repo link',
            {oExpectedManifest => \%oManifest, strTest => TEST_BACKUP_NORESUME, bRepoLink => false,
                strOptionalParam => '--no-resume --log-level-console=detail'});

        # Restore
        #-----------------------------------------------------------------------------------------------------------------------
        $bDelta = false;
        $bForce = false;

        # Fail on used path
        $oHostDbMaster->restore(
            $strBackup, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
            'fail on used path', ERROR_PATH_NOT_EMPTY, '--log-level-console=detail');

        # Remap the base and tablespace paths
        my %oRemapHash;
        $oRemapHash{&MANIFEST_TARGET_PGDATA} = $oHostDbMaster->dbBasePath(2);
        storageTest()->pathCreate($oHostDbMaster->dbBasePath(2), {strMode => '0700', bCreateParent => true});
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

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', 'BASE2', '09b5e31766be1dba1ec27de82f975c1b6eea2a92',
            $lTime, undef, undef, false);

        $oHostDbMaster->manifestTablespaceDrop(\%oManifest, 1, 2);

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2b.txt', 'TBLSPC2B',
            'e324463005236d83e6e54795dbddd20a74533bf3', $lTime, undef, undef, false);

        # Munge the version to make sure it gets corrected on the next run
        $oHostBackup->manifestMunge($strBackup, {&INI_SECTION_BACKREST => {&INI_KEY_VERSION => '0.00'}}, false);

        $strBackup = $oHostBackup->backup(
            $strType, 'add files and remove tablespace 2',
            {oExpectedManifest => \%oManifest, strOptionalParam => '--log-level-console=detail'});

        # Incr Backup
        #-----------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_INCR;
        $oHostDbMaster->manifestReference(\%oManifest, $strBackup);

        # Delete the backup.info and make sure the backup fails - the user must then run a stanza-create --force
        if ($bNeutralTest)
        {
            executeTest('sudo rm -f ' . $oHostBackup->repoPath() . '/backup/' . $self->stanza() . '/backup.info*');
        }

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASEUPDT', '9a53d532e27785e681766c98516a5e93f096a501',
            $lTime, undef, undef, false);

        if ($bNeutralTest)
        {
            $strBackup =$oHostBackup->backup(
            $strType, 'update files - fail on missing backup.info',
            {oExpectedManifest => \%oManifest, iExpectedExitStatus => ERROR_FILE_MISSING,
             strOptionalParam => '--log-level-console=detail'});

            # Fail on attempt to create the stanza data since force was not used
            $oHostBackup->stanzaCreate('fail on backup directory not empty and missing backup.info',
                {iExpectedExitStatus => ERROR_PATH_NOT_EMPTY, strOptionalParam => '--no-' . OPTION_ONLINE});

            # Use force to create the stanza
            $oHostBackup->stanzaCreate('create required data for stanza',
                {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});
        }
        else
        {
            $oHostBackup->stanzaCreate('create required data for stanza',
                {strOptionalParam => '--no-' . OPTION_ONLINE . ' --' . OPTION_FORCE});
        }

        # Perform the backup
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
        # as removed in the log because it had not changed since the last backup so it will only be referenced.  This test also
        # checks that everything works when there are no jobs to run.
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
        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2c.txt', 'TBLSPC2C',
            'ad7df329ab97a1e7d35f1ff0351c079319121836', $lTime, undef, undef, false);

        $oBackupExecute = $oHostBackup->backupBegin(
            $strType, 'remove files during backup',
            {oExpectedManifest => \%oManifest, strTest => TEST_MANIFEST_BUILD, fTestDelay => 1,
                strOptionalParam => '--log-level-console=detail'});

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGTBLSPC . '/2', '32768/tablespace2c.txt', 'TBLSPCBIGGER',
            'dfcb8679956b734706cf87259d50c88f83e80e66', $lTime, undef, undef, false);

        $oHostDbMaster->manifestFileRemove(\%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', true);

        $strBackup = $oHostBackup->backupEnd($strType, $oBackupExecute, {oExpectedManifest => \%oManifest});

        # Full Backup
        #-----------------------------------------------------------------------------------------------------------------------
        $strType = BACKUP_TYPE_FULL;

        $oHostDbMaster->manifestReference(\%oManifest);

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/16384/17000', 'BASEUPDT2', '7579ada0808d7f98087a0a586d0df9de009cdc33',
            $lTime, undef, undef, false);

        $oManifest{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_CHECKSUM_PAGE} = JSON::PP::false;

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

        $oHostDbMaster->manifestFileCreate(
            \%oManifest, MANIFEST_TARGET_PGDATA, 'base/base2.txt', 'BASE2UPDT', 'cafac3c59553f2cfde41ce2e62e7662295f108c0',
            $lTime, undef, undef, false);

        # Munge the prior manifest so that option-checksum-page is missing to be sure the logic works for backups before page
        # checksums were introduced
        $oHostBackup->manifestMunge(
            $strFullBackup, {&MANIFEST_SECTION_BACKUP_OPTION => {&MANIFEST_KEY_CHECKSUM_PAGE => undef}}, false);

        $strBackup = $oHostBackup->backup(
            $strType, 'add file', {oExpectedManifest => \%oManifest,
                strOptionalParam => '--log-level-console=detail --' . OPTION_CHECKSUM_PAGE});

        # Selective Restore
        #-----------------------------------------------------------------------------------------------------------------------
        $bDelta = true;

        # Remove mapping for tablespace 1
        delete($oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/1'});

        # Remove checksum to match zeroed files
        delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33000'}{&MANIFEST_SUBKEY_CHECKSUM});
        delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33001'}{&MANIFEST_SUBKEY_CHECKSUM});
        delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.4_201409291/32768/tablespace2.txt'}
                         {&MANIFEST_SUBKEY_CHECKSUM});
        delete($oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.4_201409291/32768/tablespace2c.txt'}
                         {&MANIFEST_SUBKEY_CHECKSUM});

        $oHostDbMaster->restore(
            OPTION_DEFAULT_RESTORE_SET, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
            'selective restore 16384', undef, '--log-level-console=detail --db-include=16384');

        # Restore checksum values for next test
        $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33000'}{&MANIFEST_SUBKEY_CHECKSUM} =
            '4a383e4fb8b5cd2a4e8fab91ef63dce48e532a2f';
        $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_data/base/32768/33001'}{&MANIFEST_SUBKEY_CHECKSUM} =
            '6bf316f11d28c28914ea9be92c00de9bea6d9a6b';
        $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.4_201409291/32768/tablespace2.txt'}
                  {&MANIFEST_SUBKEY_CHECKSUM} = 'dc7f76e43c46101b47acc55ae4d593a9e6983578';
        $oManifest{&MANIFEST_SECTION_TARGET_FILE}{'pg_tblspc/2/PG_9.4_201409291/32768/tablespace2c.txt'}
                  {&MANIFEST_SUBKEY_CHECKSUM} = 'dfcb8679956b734706cf87259d50c88f83e80e66';

        # Remove checksum to match zeroed file
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
        storageTest()->pathCreate($strDbPath, {strMode => '0700'});

        $oRemapHash{&MANIFEST_TARGET_PGDATA} = $strDbPath;
        delete($oRemapHash{&MANIFEST_TARGET_PGTBLSPC . '/2'});

        $oHostDbMaster->restore(
            OPTION_DEFAULT_RESTORE_SET, \%oManifest, \%oRemapHash, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
            'no tablespace remap - error when tablespace dir does not exist', ERROR_PATH_MISSING,
            '--log-level-console=detail --tablespace-map-all=../../tablespace', false);

        storageTest()->pathCreate($oHostDbMaster->dbBasePath(2) . '/tablespace', {strMode => '0700'});

        $oHostDbMaster->restore(
            OPTION_DEFAULT_RESTORE_SET, \%oManifest, undef, $bDelta, $bForce, undef, undef, undef, undef, undef, undef,
            'no tablespace remap', undef, '--tablespace-map-all=../../tablespace --log-level-console=detail', false);

        $oManifest{&MANIFEST_SECTION_BACKUP_TARGET}{'pg_tblspc/2'}{&MANIFEST_SUBKEY_PATH} = '../../tablespace/ts2';
        $oManifest{&MANIFEST_SECTION_TARGET_LINK}{'pg_data/pg_tblspc/2'}{&MANIFEST_SUBKEY_DESTINATION} = '../../tablespace/ts2';

        # Backup Info (with an empty stanza)
        #-----------------------------------------------------------------------------------------------------------------------
        executeTest('sudo chmod g+w ' . $oHostBackup->repoPath() . '/backup');
        storageTest()->pathCreate($oHostBackup->repoPath() . '/backup/db_empty', {strMode => '0770'});

        $oHostBackup->info('normal output');
        $oHostDbMaster->info('normal output', {strOutput => INFO_OUTPUT_JSON});
        $oHostBackup->info('bogus stanza', {strStanza => BOGUS});
        $oHostDbMaster->info('bogus stanza', {strStanza => BOGUS, strOutput => INFO_OUTPUT_JSON});

        # Dump out history path at the end to verify all history files are being recorded.  This test is only performed locally
        # because for some reason sort order is different when this command is executed via ssh (even though the content of the
        # directory is identical).
        #-----------------------------------------------------------------------------------------------------------------------
        if ($bNeutralTest && !$bRemote)
        {
            executeTest('ls -1R ' . $oHostBackup->repoPath() . '/backup/' . $self->stanza() . '/' . PATH_BACKUP_HISTORY,
                        {oLogTest => $self->expect(), bRemote => $bRemote});
        }

        # Test config file validation
        #-----------------------------------------------------------------------------------------------------------------------
        if ($bNeutralTest)
        {
            if ($bRemote)
            {
                # Save off config file and add an invalid option to the remote (DB master) and confirm no warning thrown
                executeTest("cp " . $oHostDbMaster->backrestConfig() . " " . $oHostDbMaster->backrestConfig() . ".save");
                $oHostDbMaster->executeSimple("echo " . BOGUS . "=" . BOGUS . " >> " . $oHostDbMaster->backrestConfig(), undef,
                    'root');

                $strBackup = $oHostBackup->backup(
                    $strType, 'config file not validated on remote', {oExpectedManifest => \%oManifest,
                        strOptionalParam => '--log-level-console=detail'});

                executeTest('sudo rm '. $oHostDbMaster->backrestConfig());
                executeTest("mv " . $oHostDbMaster->backrestConfig() . ".save" . " " . $oHostDbMaster->backrestConfig());
            }
            else
            {
                # Save off config file and add an invalid option to the local backup host and confirm a warning is thrown
                executeTest("cp " . $oHostBackup->backrestConfig() . " " . $oHostBackup->backrestConfig() . ".save");
                $oHostBackup->executeSimple("echo " . BOGUS . "=" . BOGUS . " >> " . $oHostBackup->backrestConfig(), undef, 'root');

                $strBackup = $oHostBackup->backup(
                    $strType, 'config file warning on local', {oExpectedManifest => \%oManifest,
                        strOptionalParam => '--log-level-console=detail 2>&1'});

                executeTest('sudo rm '. $oHostBackup->backrestConfig());
                executeTest("mv " . $oHostBackup->backrestConfig() . ".save" . " " . $oHostBackup->backrestConfig());
            }
        }

        # Test backup from standby warning that standby not configured so option reset
        #-----------------------------------------------------------------------------------------------------------------------
        if (!defined($oHostDbStandby) && $bNeutralTest)
        {
            $strBackup = $oHostBackup->backup(
                $strType, 'option backup-standby reset - backup performed from master', {oExpectedManifest => \%oManifest,
                    strOptionalParam => '--log-level-console=detail --' . OPTION_BACKUP_STANDBY});
        }
    }
    }
    }
}

1;
