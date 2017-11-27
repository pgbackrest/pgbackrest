####################################################################################################################################
# ManifestAllTest.pm - Unit tests for Manifest module
####################################################################################################################################
package pgBackRestTest::Module::Manifest::ManifestAllTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::Common;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant MODE_0750                                  => '0750';
use constant MODE_0700                                  => '0700';
use constant MODE_0644                                  => '0644';
use constant MODE_0600                                  => '0600';
use constant TEST_USER                                  => 'ubuntu';
use constant TEST_GROUP                                 => 'ubuntu';
use constant PGCONTROL_SIZE                             => 8192;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . "/db";
    $self->{strRepoPath} = $self->testPath() . "/repo";
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
    $self->{strSpoolPath} = "$self->{strArchivePath}/out";
    $self->{strExpectedManifest} = $self->testPath() . "/expected.manifest";
    $self->{strActualManifest} = $self->testPath() . "/actual.manifest";
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create archive info path
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});
    executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strArchivePath});
    executeTest("sudo chown " . TEST_USER . " " . $self->{strArchivePath});

    # Create backup info path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});
    executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strBackupPath});
    executeTest("sudo chown " . TEST_USER . " " . $self->{strBackupPath});

    # Create pg_control global path
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
    executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strDbPath} . '/' . DB_PATH_GLOBAL);
    executeTest("sudo chown " . TEST_USER . " " . $self->{strDbPath} . '/' . DB_PATH_GLOBAL);
}

####################################################################################################################################
# manifestCompare
####################################################################################################################################
sub manifestCompare
{
    my $self = shift;
    my $oManifestExpected = shift;
    my $oManifestActual = shift;

    # Remove manifest files if exist
    storageTest()->remove($self->{strExpectedManifest}, {bIgnoreMissing => true});
    storageTest()->remove($self->{strActualManifest}, {bIgnoreMissing => true});

    # Section backup. Confirm the copy-start timestamp is set for the actual manifest then copy it to the expected so files can
    # be compared
    if ($oManifestActual->test(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START))
    {
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
            $oManifestActual->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START));
    }
    else
    {
        return MANIFEST_KEY_TIMESTAMP_COPY_START . " not set in actual manifest";
    }

    storageTest()->put($self->{strActualManifest}, iniRender($oManifestActual->{oContent}));
    storageTest()->put($self->{strExpectedManifest}, iniRender($oManifestExpected->{oContent}));

    return executeTest("diff " . $self->{strExpectedManifest} . " " . $self->{strActualManifest});
}


####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
    $self->optionTestSet(CFGOPT_DB_PATH, $self->{strDbPath});
    $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

    my $strBackupLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, 1482000000);
    my $strBackupPath = storageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strBackupLabel}");
    my $strBackupManifestFile = "$strBackupPath/" . FILE_MANIFEST;
    my $iDbCatalogVersion = 201409291;

    my $lTime = time() - 10000;
    my $strTest = 'test';

    my $oManifestBase = new pgBackRest::Common::Ini($self->{strExpectedManifest}, {bLoad => false});

    # Section: backup:db
    $oManifestBase->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_94);
    # Section: target:path
    my $hDefault = {};
    $oManifestBase->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, undef, $hDefault);
    $oManifestBase->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_GLOBAL, undef, $hDefault);
    # Section: target:path:default
    $oManifestBase->set(MANIFEST_SECTION_TARGET_PATH . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0750);
    $oManifestBase->set(MANIFEST_SECTION_TARGET_PATH . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
    $oManifestBase->set(MANIFEST_SECTION_TARGET_PATH . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);
    # Section backup:target
    $oManifestBase->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_PATH);
    $oManifestBase->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH, $self->{strDbPath});

    ################################################################################################################################
    if ($self->begin('new()'))
    {
        # Missing DB version
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {(new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false}))}, ERROR_ASSERT,
            'strDbVersion must be provided with bLoad = false');

        # Successfully instantiate
        #---------------------------------------------------------------------------------------------------------------------------
        my $oManifest = $self->testResult(sub {(new pgBackRest::Manifest($strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94}))}, "[object]", 'manifest instantiated');

        # Initialize
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_94) &&
            $oManifest->test(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, $oManifest->{iInitFormat}) &&
            $oManifest->test(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $oManifest->{strInitVersion})}, true,
            '    manifest initialized');

        # Attempt to save without proper path
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->save()}, ERROR_PATH_MISSING,
            "unable to open '" . $strBackupManifestFile . "': No such file or directory");

        # Create path and save
        #---------------------------------------------------------------------------------------------------------------------------
        storageRepo()->pathCreate($strBackupPath);
        $self->testResult(sub {$oManifest->save()}, "[undef]", 'manifest saved');

        # Load and check the saved file
        #---------------------------------------------------------------------------------------------------------------------------
        my $oBackupManifest = $self->testResult(sub {(new pgBackRest::Manifest($strBackupManifestFile))}, "[object]",
            '    saved manifest loaded');
        $self->testResult(sub {($oManifest->numericGet(INI_SECTION_BACKREST, INI_KEY_FORMAT) ==
            $oBackupManifest->numericGet(INI_SECTION_BACKREST, INI_KEY_FORMAT)) &&
            ($oManifest->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM) eq
            $oBackupManifest->get(INI_SECTION_BACKREST, INI_KEY_CHECKSUM))}, true,
            '    saved manifest equals initial manifest');
    }

    ################################################################################################################################
    if ($self->begin('build()'))
    {
# CSHANG On load, why does load require the db version but nothing else, esp when tablespacePathGet requires catalog to be set? Is there no catalog number prior to 9.0?
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # Build error if offline = true and no tablespace path
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, false)}, ERROR_FILE_MISSING,
            "unable to stat '" . $self->{strDbPath} . "/" . MANIFEST_TARGET_PGTBLSPC . "': No such file or directory");

        # bOnline = true tests - Compare the base manifest
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestBase, $oManifest)}, "", 'base manifest');

        # Create expected manifest from base
        my $oManifestExpected = dclone($oManifestBase);

        # Add global/pg_control file and PG_VERSION file and create a directory with a different modes than default
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_PGCONTROL,
            {strMode => MODE_0644, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}),
            $self->controlGenerateContent(PG_VERSION_94));

        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_PGVERSION,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), PG_VERSION_94);

        # Create base path with different mode than default
        storageDb()->pathCreate(DB_PATH_BASE, {strMode => MODE_0700});
        executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strDbPath} . '/' . DB_PATH_BASE);
        executeTest("sudo chown " . TEST_USER . " " . $self->{strDbPath} . '/' . DB_PATH_BASE);

        # Update expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0600);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, true);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_SIZE, PGCONTROL_SIZE);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MODE, MODE_0644);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' .DB_FILE_PGVERSION,
            MANIFEST_SUBKEY_SIZE, length(&PG_VERSION_94));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' .DB_FILE_PGVERSION,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_BASE, MANIFEST_SUBKEY_MODE, MODE_0700);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'paths/files and different modes');

# CSHANG Should the build function check for a minimum set or at least in the validate function? At this point there is no checksum for pg_control in target:file. Validate will fail on manifest subvalue 'checksum' not set for file 'pg_data/global/pg_control'

#CSHANG Also, what is the [backup:option] section - is it for the Clibrary? Do we test this anywhere - should we somehow test it here? Should these be a "minimum set"?
        # * backup:
        #     * backup-type
        #     * backup-timestamp-start
        #     * backup-archive-start
        #     * backup-lsn-start
        # * backup:option
        #     * option-backup-standby
        #     * option-compress
        #     * option-hardlink
        #     * option-online
        #     * option-archive-copy
        #     * option-archive-check
        #     * option-checksum-page
        # * backup:db
        #     * db-id
        #     * db-system-id
        #     * db-catalog-version
        #     * db-control-version?

        # Master = false
        #---------------------------------------------------------------------------------------------------------------------------
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_BASE . '/' . $strTest,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strTest);

        # Update expected manifest
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strTest,
            MANIFEST_SUBKEY_MASTER, undef, false);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strTest,
            MANIFEST_SUBKEY_SIZE, length($strTest));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strTest,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'master false');

        # Create a pg_config path and file link
        my $strConfFile = '/pg_config/postgresql.conf';
        my $strConfContent = "listen_addresses = *\n";
        storageDb()->pathCreate('pg_config');
        executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strDbPath} . '/pg_config');
        executeTest("sudo chown " . TEST_USER . " " . $self->{strDbPath} . '/pg_config');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . $strConfFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strConfContent);

        # link db/pg_config/postgresql.conf.link -> ./postgresql.conf
        testLinkCreate($self->{strDbPath} . $strConfFile . '.link', './postgresql.conf');

# CSHANG on the command line, these links appear to be fine but in the code, a debug line prior to the recursive call to build() produces:
        # STRPATH BEFORE BUILD: /home/ubuntu/test/test-0/db/base, STRLEVEL PASSED: $VAR1 = 'pg_data/base/pg_config_bad';
        # STRFILTER: $VAR1 = undef;
        # STRPATH BEFORE BUILD: /home/ubuntu/test/test-0/db/pg_config, STRLEVEL PASSED: $VAR1 = 'pg_data/base/pg_config_bad/postgresql.conf.link';
        # STRFILTER: $VAR1 = undef;
        # STRPATH BEFORE BUILD: /home/ubuntu/test/test-0/db/base/base, STRLEVEL PASSED: $VAR1 = 'pg_data/base/postgresql.conf';
        # STRFILTER: $VAR1 = undef;
        #
        # and right here throws: 'unable to stat '/home/ubuntu/test/test-0/db/base/pg_config/postgresql.conf': No such file or directory'
        # -- note the extra "base" here - it should just be /home/ubuntu/test/test-0/db/pg_config/postgresql.conf
        # # link db/base/pg_config_bad -> ../../db/pg_config
        # testLinkCreate($self->{strDbPath} . '/'. DB_PATH_BASE . '/pg_config_bad', '../../db/pg_config');
        # # link db/base/postgresql.conf -> ../pg_config/postgresql.conf
        # testLinkCreate($self->{strDbPath} . '/'. DB_PATH_BASE . '/postgresql.conf', '..' . $strConfFile);
        # $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_LINK_DESTINATION,
        #     'TEST THIS');

# CSHANG Even though the below code creates a duplicate link, this error occurs (note the pg_config/pg_config):
        # 'unable to stat '/home/ubuntu/test/test-0/db/pg_config/pg_config/./postgresql.conf': No such file or directory'
        # ubuntu@pgbackrest-test:~$ ls -l /home/ubuntu/test/test-0/db/pg_config/
        # total 4
        # -rw------- 1 ubuntu ubuntu 21 Nov  9 17:07 postgresql.conf
        # lrwxrwxrwx 1 ubuntu ubuntu 17 Nov  9 19:53 postgresql.conf.bad.link -> ./postgresql.conf
        # lrwxrwxrwx 1 ubuntu ubuntu 17 Nov  9 19:53 postgresql.conf.link -> ./postgresql.conf
        # # This link will cause errors because it points to the same location as above
        # testLinkCreate($self->{strDbPath} . $strConfFile . '.bad.link', './postgresql.conf');
        #
        # $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_LINK_DESTINATION,
        #     'TEST ERROR');
        # testFileRemove($self->{strDbPath} . $strConfFile . '.link.bad');

        # Update expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_BASE, MANIFEST_SUBKEY_MODE, MODE_0700);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . '/pg_config', undef, $hDefault);

        # Section backup:target
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA . $strConfFile . '.link',
            MANIFEST_SUBKEY_FILE, 'postgresql.conf');
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA . $strConfFile . '.link',
            MANIFEST_SUBKEY_PATH, '.');
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA . $strConfFile . '.link',
            MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_LINK);
        # Section target:file
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strConfFile,
            MANIFEST_SUBKEY_SIZE, length($strConfContent));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strConfFile,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        # Section target:link
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_LINK, MANIFEST_TARGET_PGDATA . $strConfFile . '.link',
            MANIFEST_SUBKEY_DESTINATION, './postgresql.conf');
        # Section target:link:default
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_LINK . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_LINK . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'link');

        # Test skip files/directories
        #---------------------------------------------------------------------------------------------------------------------------
        # Create files to skip
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTGRESQLAUTOCONFTMP ), '',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime});
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_BACKUPLABELOLD), '',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime});
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTMASTEROPTS), '',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime});
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTMASTERPID), '',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime});
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_RECOVERYCONF), '',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime});
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_RECOVERYDONE), '',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime});

        # Create directories to skip. Add a bogus file to them for test coverage.
        storageDb()->pathCreate(DB_FILE_PREFIX_TMP);
        executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strDbPath} . '/' . DB_FILE_PREFIX_TMP);
        executeTest("sudo chown " . TEST_USER . " " . $self->{strDbPath} . '/' . DB_FILE_PREFIX_TMP);
        storageDb()->pathCreate('pg_xlog');
        executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strDbPath} . '/pg_xlog');
        executeTest("sudo chown " . TEST_USER . " " . $self->{strDbPath} . '/pg_xlog');

        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/pg_xlog/' . BOGUS,
            {strMode => MODE_0644, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_PATH_PGDYNSHMEM);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGDYNSHMEM . '/' . BOGUS,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_PATH_PGNOTIFY);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGNOTIFY . '/' . BOGUS,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_PATH_PGREPLSLOT);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGREPLSLOT . '/' . BOGUS,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_PATH_PGSERIAL);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSERIAL . '/' . BOGUS,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_PATH_PGSNAPSHOTS);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSNAPSHOTS . '/' . BOGUS,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_PATH_PGSTATTMP);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSTATTMP . '/' . BOGUS,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_PATH_PGSUBTRANS);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSUBTRANS . '/' . BOGUS,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate(DB_FILE_PGINTERNALINIT);

        # Update expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . '/pg_xlog', undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGDYNSHMEM, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGNOTIFY, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGREPLSLOT, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSERIAL, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSNAPSHOTS, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSTATTMP, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSUBTRANS, undef, $hDefault);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'skip directories/files');

        # Unskip code path coverage
        #---------------------------------------------------------------------------------------------------------------------------
        my $oManifestExpectedUnskip = dclone($oManifestExpected);

        # Change DB version to 93
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_93});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_93);

        # Update expected manifest
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0600);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, true);

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 94 directories');

        # Change DB version to 91
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_91});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_91);

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 92 directories');

        # Change DB version to 90
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_90});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_90);

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSERIAL . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSERIAL . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 91 directories');

        # Change DB version to 84
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_84});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_84);

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGNOTIFY . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGNOTIFY . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 90 directories');

        # Change DB version to 83
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_83});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_83);

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSTATTMP . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSTATTMP . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 84 directories');

        # Reset Manifest for next tests
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'manifest reset');

        # Tablespaces
        #---------------------------------------------------------------------------------------------------------------------------
        # Create pg_tblspc path
        my $strTblSpcPath = $self->{strDbPath} . '/' . DB_PATH_PGTBLSPC;
        storageDb()->pathCreate($strTblSpcPath, {bCreateParent => true});
        executeTest("sudo chgrp " . TEST_GROUP . " " . $strTblSpcPath);
        executeTest("sudo chown " . TEST_USER . " " . $strTblSpcPath);

        # Create a directory in pg_tblspc
        storageDb()->pathCreate("${strTblSpcPath}/" . BOGUS, {strMode => '0700'});
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_LINK_EXPECTED,
            MANIFEST_TARGET_PGTBLSPC . "/" . BOGUS . " is not a symlink - " . DB_PATH_PGTBLSPC . " should contain only symlinks");

        testPathRemove("${strTblSpcPath}/" . BOGUS);

        my $strTblspcId = '99999';

        # Invalid relative tablespace is ../
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '../');
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_TABLESPACE_IN_PGDATA,
            'tablespace symlink ../ destination must not be in $PGDATA');
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Invalid relative tablespace is ..
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '..');
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_TABLESPACE_IN_PGDATA,
            'tablespace symlink .. destination must not be in $PGDATA');
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Invalid relative tablespace is ../base - a subdirectory of $PGDATA
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '../base');
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_TABLESPACE_IN_PGDATA,
            'tablespace symlink ../base destination must not be in $PGDATA');
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Create the catalog key for the tablespace construction
        $oManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iDbCatalogVersion);

        # Invalid absolute tablespace is  $self->{strDbPath} . /base
# CSHANG But this should fail because the link points to a directory in pg_data but instead it passes the index($hManifest->{$strName}{link_destination}, '/') != 0 and then fails later. It WILL fail "destination must not be in $PGDATA" if an ending slash is added - so maybe the comment in Manifest.pm "# Make sure that DB_PATH_PGTBLSPC contains only absolute links that do not point inside PGDATA" is not exactly correct?
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", $self->{strDbPath} . '/base');
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_ASSERT,
            "tablespace with oid ${strTblspcId} not found in tablespace map\n" .
            "HINT: was a tablespace created or dropped during the backup?");
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Invalid relative tablespace is ../../BOGUS - which is not in $PGDATA and does not exist
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '../../' . BOGUS);
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_ASSERT,
            "tablespace with oid ${strTblspcId} not found in tablespace map\n" .
            "HINT: was a tablespace created or dropped during the backup?");
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Create tablespace directory outside PGDATA
        my $strTablespace = 'tablespace';
        storageTest()->pathCreate($strTablespace);

        my $strIntermediateLink = $self->{strDbPath} . "/intermediate_link";

        # Create a link to a link
        testLinkCreate($strIntermediateLink, $self->testPath() . '/' . $strTablespace);
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", $strIntermediateLink);

        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, false)}, ERROR_LINK_DESTINATION,
            "link '${strTblSpcPath}/${strTblspcId}' -> '$strIntermediateLink' cannot reference another link");

        testFileRemove($self->{strDbPath} . "/intermediate_link");
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Reload the manifest otherwise it will contain invalid data from the above exception tests
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # Set the required db catalog version for tablespaces
        $oManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iDbCatalogVersion);
        $oManifestExpected->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iDbCatalogVersion);

        # Create a valid symlink pg_tblspc/1 to tablespace/ts1/1 directory
        my $strTablespaceOid = '1';
        my $strTablespaceName = "ts${strTablespaceOid}";
        storageTest()->pathCreate("${strTablespace}/${strTablespaceName}/${strTablespaceOid}", {bCreateParent => true});
        executeTest("sudo chgrp " . TEST_GROUP . " " . $self->testPath() .
            "/${strTablespace}/${strTablespaceName}/${strTablespaceOid}");
        executeTest("sudo chown " . TEST_USER . " " . $self->testPath() .
            "/${strTablespace}/${strTablespaceName}/${strTablespaceOid}");

        my $strTablespacePath = storageTest()->pathGet("${strTablespace}/${strTablespaceName}/${strTablespaceOid}");
        testLinkCreate("${strTblSpcPath}/${strTablespaceOid}", $strTablespacePath);

        # Create the tablespace info in expected manifest
        my $strMfTs = MANIFEST_TARGET_PGTBLSPC . "/" . $strTablespaceOid;
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, $strMfTs, MANIFEST_SUBKEY_PATH, $strTablespacePath);
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, $strMfTs, MANIFEST_SUBKEY_TABLESPACE_ID, $strTablespaceOid);
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, $strMfTs, MANIFEST_SUBKEY_TABLESPACE_NAME, $strTablespaceName);
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, $strMfTs, MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_LINK);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_LINK, MANIFEST_TARGET_PGDATA . "/${strMfTs}", MANIFEST_SUBKEY_DESTINATION,
            $strTablespacePath);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGTBLSPC, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGTBLSPC, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, $strMfTs, undef, $hDefault);

        # In offline mode, do not skip the db WAL path
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MODE, MODE_0644);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_MODE, MODE_0644);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'offline with valid tablespace - do not skip database WAL directory');

        # Create tablespace and database maps
        my $hTablespaceMap = {};
        $hTablespaceMap->{$strTablespaceOid} = $strTablespaceName;

        my $hDatabaseMap = {};
        $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_ID} = 12345;
        $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_LAST_SYSTEM_ID} = 67890;

        $oManifestExpected->numericSet(MANIFEST_SECTION_DB, BOGUS, MANIFEST_KEY_DB_ID, $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_ID});
        $oManifestExpected->numericSet(MANIFEST_SECTION_DB, BOGUS, MANIFEST_KEY_DB_LAST_SYSTEM_ID,
            $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_LAST_SYSTEM_ID});

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false, $hTablespaceMap, $hDatabaseMap);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'offline passing tablespace map and database map');

        # Reload the manifest with version < 9.0
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_84});

        # Catalog not stored in < 9.0
        $oManifestExpected->remove(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG);
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_84);

        # Add unskip directories
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSERIAL . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSERIAL . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGNOTIFY . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGNOTIFY . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false, $hTablespaceMap, $hDatabaseMap);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'tablespace with version < 9.0');

        # Undefined user/group
        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("sudo chgrp 777 " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
        executeTest("sudo chown 777 " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_GROUP, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_USER, false);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'undefined user/group');

        # Reset the group / owner
        executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
        executeTest("sudo chown " . TEST_USER . " " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
    }

    ################################################################################################################################
    if ($self->begin('get/set'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # SubKey required but has not been set
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE)},
            ERROR_ASSERT, "strSection '" . MANIFEST_SECTION_TARGET_FILE . "', strKey '" . BOGUS . "', strSubKey '" .
            MANIFEST_SUBKEY_CHECKSUM_PAGE . "' is required but not defined");

        # SubKey not required and not set
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG Why would we allow this?
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE, false)},
            false, 'boolGet() - false');

        # Set and get a boolean value
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE, true);
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE)}, true,
            'boolGet() - true');

        # Get default boolean value
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG But this doesn't actually set the default value, so should we be allowing this? Why?
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, false, true)},
            true, 'boolGet() - default true');

        # Get but don't return a default boolean value even if there is supposed to be one
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG Is this right? Should the calling routine be passing whether the option has a default or not?
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, false, false)},
            false, 'boolGet() - default false');

        # Get default numeric value
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG But this doesn't actually set the default value, so should we be allowing this? Why?
        $self->testResult(sub {$oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_SIZE, false, 0)},
            0, 'numericGet() - default 0');

        # Coverage for $self->SUPER::get("${strSection}:default"
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG This returns undef because required = false - is that good?
        $self->testResult(sub {$oManifest->get(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_USER, false)},
            "[undef]", 'get() - default section');

        # Get the correct path for the control file in the DB
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG Should MANIFEST_SUBKEY_TYPE be required for MANIFEST_SUBKEY_PATH? And why is there a MANIFEST_SUBKEY_FILE - shouldn't there be a MANIFEST_VALUE_FILE instead? pg_data/postgresql.conf={"file":"postgresql.conf","path":"../pg_config","type":"link"} is the only time I see it in the mock expect tests and the type is link - in restore, I see it first tests for link and then file - is this the only way "file" is used?
        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH, $self->{strDbPath});
        $self->testResult(sub {$oManifest->dbPathGet($oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA,
            MANIFEST_SUBKEY_PATH), MANIFEST_FILE_PGCONTROL)},
            $self->{strDbPath} . "/" . DB_FILE_PGCONTROL, 'dbPathGet() - control file');

        # Get filename - no path passed
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->dbPathGet(undef, BOGUS)}, BOGUS, 'dbPathGet() - filename');

        # repoPathGet - no tablespace
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->repoPathGet(MANIFEST_TARGET_PGDATA, DB_FILE_PGCONTROL)},
            MANIFEST_TARGET_PGDATA . "/" . DB_FILE_PGCONTROL, 'repoPathGet() - pg_control');

        # repoPathGet - tablespace - no subpath
        #---------------------------------------------------------------------------------------------------------------------------
        my $strTablespaceId = "1";
        my $strTablespace = MANIFEST_TARGET_PGTBLSPC . "/" . $strTablespaceId;
        my $strTablespaceName = "ts" . $strTablespaceId;

        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTablespace, MANIFEST_SUBKEY_PATH, $self->{strDbPath} .
            "/tablespace/" . $strTablespaceName);
# CSHANG But this isn't right - shouldn't it error if there is no tablespace-id or tablespace-name or type is something other than link?
        $self->testResult(sub {$oManifest->repoPathGet($strTablespace)}, $strTablespace,
            'repoPathGet() - tablespace - no tablepace-id nor subpath');

        # repoPathGet - fully qualified tablespace target
        #---------------------------------------------------------------------------------------------------------------------------
        # Set the catalog for the DB since that is what is expected to be returned
        $oManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iDbCatalogVersion);
        $self->testResult(sub {$oManifest->tablespacePathGet()}, "PG_" . PG_VERSION_94 . "_" . $iDbCatalogVersion,
            'tablespacePathGet()');

        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTablespace, MANIFEST_SUBKEY_TABLESPACE_ID, $strTablespaceId);
        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTablespace, MANIFEST_SUBKEY_TABLESPACE_NAME, $strTablespaceName);
        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTablespace, MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_LINK);

        $self->testResult(sub {$oManifest->repoPathGet($strTablespace, BOGUS)}, $strTablespace . "/PG_" . PG_VERSION_94 . "_" .
            $iDbCatalogVersion . "/" . BOGUS, 'repoPathGet() - tablespace valid with subpath');

        # isTargetLink
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG Shouldn't this error since no type has been set?
        $self->testResult(sub {$oManifest->isTargetLink(MANIFEST_TARGET_PGDATA)}, false, "isTargetLink - false");
        $self->testResult(sub {$oManifest->isTargetLink($strTablespace)}, true, "isTargetLink - true");

        # isTargetLink
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->isTargetFile(MANIFEST_TARGET_PGDATA)}, false, "isTargetFile - false");

        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_FILE, BOGUS);
        $self->testResult(sub {$oManifest->isTargetFile(MANIFEST_TARGET_PGDATA)}, true, "isTargetFile - true");
    }

    ################################################################################################################################
    if ($self->begin('isTarget - exceptions'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # Target not defined
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->isTargetValid()}, ERROR_ASSERT, 'target is not defined');
        $self->testException(sub {$oManifest->isTargetFile()}, ERROR_ASSERT, 'target is not defined');
        $self->testException(sub {$oManifest->isTargetLink()}, ERROR_ASSERT, 'target is not defined');
        $self->testException(sub {$oManifest->isTargetTablespace()}, ERROR_ASSERT, 'target is not defined');

        # Target not valid
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->isTargetValid(BOGUS, true)}, ERROR_ASSERT, BOGUS . ' is not a valid target');
        $self->testResult(sub {$oManifest->isTargetValid(BOGUS, false)}, false, 'isTargetValid - bError = false, return false');
        $self->testResult(sub {$oManifest->isTargetValid(BOGUS)}, false, 'isTargetValid - bError = undef, false');
    }

    ################################################################################################################################
    if ($self->begin('dbVerion(), xactPath(), walPath()'))
    {
        # dbVersion, xactPath and walPath - PG < 10
        #---------------------------------------------------------------------------------------------------------------------------
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        $self->testResult(sub {$oManifest->dbVersion()}, PG_VERSION_94, 'dbVersion < 10');
        $self->testResult(sub {$oManifest->xactPath()}, 'pg_clog', '    xactPath - pg_clog');
        $self->testResult(sub {$oManifest->walPath()}, 'pg_xlog', '    walPath - pg_xlog');

        # dbVersion, xactPath and walPath - PG >= 10
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_10});

        $self->testResult(sub {$oManifest->dbVersion()}, PG_VERSION_10, 'dbVersion >= 10');
        $self->testResult(sub {$oManifest->xactPath()}, 'pg_xact', '    xactPath - pg_xact');
        $self->testResult(sub {$oManifest->walPath()}, 'pg_wal', '    walPath - pg_wal');
    }

    ################################################################################################################################
    if ($self->begin('validate()'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # Set a target:file with only a timestamp - fail size not set
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_TIMESTAMP, 1509384645);
        $self->testException(sub {$oManifest->validate()}, ERROR_ASSERT,
            "manifest subvalue 'size' not set for file '" . BOGUS . "'");

        # Set target:file size - fail checksum not set
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_SIZE, 1);
        $self->testException(sub {$oManifest->validate()}, ERROR_ASSERT,
            "manifest subvalue 'checksum' not set for file '" . BOGUS . "'");

        # Set target:file checksum - validate passes when size > 0 and checksum set
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG Is a zero checksum valid?
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM, 0);
        $self->testResult(sub {$oManifest->validate()}, "[undef]", 'manifest validated - size 1, checksum 0');

        # Set target:file size to 0 - validate passes when size 0
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_SIZE, 0);
        $self->testResult(sub {$oManifest->validate()}, "[undef]", 'manifest validated - size 0');
    }

    ################################################################################################################################
    if ($self->begin('future file and last manifest'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # Create expected manifest from base
        my $oManifestExpected = dclone($oManifestBase);

        # Future timestamp on file
        #---------------------------------------------------------------------------------------------------------------------------
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . $strTest,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime + 20000}), $strTest);

        # Update expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0600);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, true);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_SIZE,
            length($strTest));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_TIMESTAMP,
            $lTime + 20000);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_FUTURE, 'y');

        $self->testResult(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, "[undef]",
            'future timestamp warning', {strLogExpect =>
            "WARN: some files have timestamps in the future - they will be copied to prevent possible race conditions"});
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'manifest future subkey=y');

        # Future timestamp in last manifest
        #---------------------------------------------------------------------------------------------------------------------------
        my $oLastManifest = dclone($oManifestExpected);

        # Set a backup label
        $oLastManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef, BOGUS);
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, BOGUS);

        # Remove the file and recreate it without it being in the future
        storageTest()->remove($self->{strDbPath} . '/' . $strTest);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . $strTest,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strTest);

        # Remove the future subkey from the expected manifest and reset the timestamp
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_FUTURE);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_TIMESTAMP,
            $lTime);

        # Create a new manifest
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        $self->testResult(sub {$oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true)}, "[undef]",
            'last manifest future timestamp warning', {strLogExpect =>
            "WARN: some files have timestamps in the future - they will be copied to prevent possible race conditions"});
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
        'last manifest future subkey=y, new manifest future subkey removed');

        # File info in last manifest same as current
        #---------------------------------------------------------------------------------------------------------------------------
        $oLastManifest->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_FUTURE);
        $oLastManifest->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_TIMESTAMP,
            $lTime);

        # Update reference in expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest, MANIFEST_SUBKEY_REFERENCE,
            BOGUS);
# CSHANG build() expects the checksum & repo size in the lastManifest to exist and does nothing if they don't - should it ASSERT?
        # Check reference
        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
        'reference set to prior backup label');

        # Create a new file reference
        my $strTestNew = $strTest . 'new';
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . $strTestNew,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strTestNew);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_SIZE, length($strTestNew));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oLastManifest->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_SIZE, length($strTestNew));
        $oLastManifest->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        # Set a reference, checksum, repo size, master and page checksum in the last manifest
        my $strCheckSum = '1234567890';
        my $lRepoSize = 10000;
        $oLastManifest->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_REFERENCE, BOGUS . BOGUS);
        $oLastManifest->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM, $strCheckSum);
        $oLastManifest->numericSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_REPO_SIZE, $lRepoSize);
        $oLastManifest->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_MASTER, false);
        $oLastManifest->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, true);

        # Update expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_REFERENCE, BOGUS . BOGUS);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM, $strCheckSum);
        $oManifestExpected->numericSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_REPO_SIZE, $lRepoSize);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, true);

        # Default "master" is flipping because it's not something we read from disk
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTest,
            MANIFEST_SUBKEY_MASTER, true);

        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'updates from last manifest');

# CSHANG What is the [backup:option] option-checksum-page setting? Why are the checksum-page/error settings per file not dependent on this? What are these for?
        # MANIFEST_SUBKEY_CHECKSUM_PAGE = false and MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR set/not set
        #---------------------------------------------------------------------------------------------------------------------------
        $oLastManifest->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, false);

        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, false);

        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'checksum-page false, checksum-page-error not set');

        my @iyChecksumPageError = (1);
        $oLastManifest->set(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR, \@iyChecksumPageError);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR, \@iyChecksumPageError);

        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'checksum-page false, checksum-page-error set');
    }

    ################################################################################################################################
    if ($self->begin('fileAdd()'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});
        my $oManifestExpected = dclone($oManifestBase);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);

        # Add a file after building manifest
        my $lTimeTest = $lTime + 10;
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . $strTest,
            {strMode => MODE_0644, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTimeTest}), $strTest);

# CSHANG The file user, group and mode are not passed in - why? the default target path is used unless there is already a targe:file:default then that is used. I'm guessing we can't get the actual stats for the file?
        $oManifest->fileAdd($strTest, $lTimeTest, 0, 0, true);

        # Update expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_MODE, MODE_0600);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_USER, TEST_USER);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_GROUP, TEST_GROUP);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_TIMESTAMP, $lTimeTest);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_CHECKSUM, 0);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_MASTER, true);

        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'file added to manifest');

        # Remove the file user, mode and group from the actual and expected manifest
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_MODE);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_USER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_GROUP);

        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_MODE);
        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_USER);
        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_GROUP);

        # Add a target:file:default section to the manifests with a mode other than 0600
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0750);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);

        $oManifest->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0750);
        $oManifest->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
        $oManifest->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);

        # Set the expected mode to 0600
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_MODE, MODE_0600);

        $oManifest->fileAdd($strTest, $lTimeTest, 0, 0, true);

        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'file added to manifest - file:default values set');

        # Remove the file mode from the manifests and change the default so it need not be set for the file
        $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_MODE);
        $oManifest->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0600);

        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, $strTest, MANIFEST_SUBKEY_MODE);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0600);

        $oManifest->fileAdd($strTest, $lTimeTest, 0, 0, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'file added to manifest - default mode set 0600');
    }

    ################################################################################################################################
    if ($self->begin('isChecksumPage()'))
    {
        my $strFile = BOGUS;

        $self->testResult(sub {isChecksumPage($strFile)}, false, "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . DB_FILE_PGVERSION;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . DB_FILE_PGINTERNALINIT;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE. '/' . DB_FILE_PGFILENODEMAP;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGTBLSPC . '/' . DB_FILE_PGFILENODEMAP;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGTBLSPC . '/' . DB_FILE_PGINTERNALINIT;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGTBLSPC . '/' . DB_FILE_PGVERSION;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_GLOBAL. '/' . DB_FILE_PGFILENODEMAP;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_GLOBAL. '/' . DB_FILE_PGINTERNALINIT;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_GLOBAL. '/' . DB_FILE_PGVERSION;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_GLOBAL. '/' . DB_FILE_PGCONTROL;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGCONTROL;
        $self->testResult(sub {isChecksumPage($strFile)}, false,
            "file '${strFile}' isChecksumPage=false");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . BOGUS;
        $self->testResult(sub {isChecksumPage($strFile)}, true,
            "file '${strFile}' isChecksumPage=true");

        $strFile = MANIFEST_TARGET_PGDATA . '/' . DB_PATH_GLOBAL . '/' . BOGUS;
        $self->testResult(sub {isChecksumPage($strFile)}, true,
            "file '${strFile}' isChecksumPage=true");
    }

}

1;
