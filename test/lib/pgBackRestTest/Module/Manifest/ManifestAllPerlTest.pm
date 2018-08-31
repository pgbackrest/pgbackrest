####################################################################################################################################
# Unit tests for Manifest module
####################################################################################################################################
package pgBackRestTest::Module::Manifest::ManifestAllPerlTest;
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

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# Issues / Improvements
#
# While working on the unit tests a number of possible issues/improvements have come up.  Issues are documented in the tests where
# they are relevant with an INVESTIGATE tag.  General improvements are listed here to be addressed later:
#
# 1. Assignment of required values to the manifest is haphazard.  Db version is in the constructor but the rest is assigned in
#    Backup.pm.  These should probably all be passed in the constructor.  It may be like it is now in part because of test needs
#    but that should be changed if so.
#
# 2. Improve validate function to check that all values that are collected actually got set.  Some things are being checked now,
#    but others like backup-lsn-start, backup-wal-start, etc. are not.  Include all options like option-compress, etc. and db info
#    like db-id, system-id, etc.  Basically, validate should check everything in the manifest before it is saved (for the last time
#    since intermediate saves may be missing data) and make sure nothing is missing or extraneous.  Also check the contents of
#    values, i.e. make sure that checksum is really a 40-character hex string.
#
# 3. In fileAdd() the mode should be based on a mask (remove execute bit) of the dir mode, rather than a constant.  i.e. dir mode
#    0700 should become 0600, dir mode 0750 should become 0640, etc.
####################################################################################################################################

####################################################################################################################################
# Constants
####################################################################################################################################
use constant MODE_0750                                  => '0750';
use constant MODE_0700                                  => '0700';
use constant MODE_0644                                  => '0644';
use constant MODE_0600                                  => '0600';
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

    # Create backup info path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});

    # Create pg_control global path
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
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
    $self->optionTestSet(CFGOPT_PG_PATH, $self->{strDbPath});
    $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

    my $strBackupLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, 1482000000);
    my $strBackupPath = storageRepo->pathGet(STORAGE_REPO_BACKUP . "/${strBackupLabel}");
    my $strBackupManifestFile = "$strBackupPath/" . FILE_MANIFEST;

    my $lTime = time() - 10000;
    my $strTest = 'test';

    my $oManifestBase = new pgBackRest::Common::Ini($self->{strExpectedManifest}, {bLoad => false});

    # Section: backup:db
    $oManifestBase->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_94);
    $oManifestBase->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $self->dbCatalogVersion(PG_VERSION_94));
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
        # Missing DB and DB Catalog version
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {(new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false}))}, ERROR_ASSERT,
            'strDbVersion and iDbCatalogVersion must be provided with bLoad = false');

        # Missing DB version
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {(new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)}))}, ERROR_ASSERT,
            'strDbVersion and iDbCatalogVersion must be provided with bLoad = false');

        # Missing DB Catalog version
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {(new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false,
            strDbVersion => PG_VERSION_94}))}, ERROR_ASSERT,
            'strDbVersion and iDbCatalogVersion must be provided with bLoad = false');

        # Successfully instantiate
        #---------------------------------------------------------------------------------------------------------------------------
        my $oManifest = $self->testResult(sub {(new pgBackRest::Manifest($strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)}))},
            "[object]", 'manifest instantiated');

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
        my $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

        # Build error if offline = true and no tablespace path
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, false, false)}, ERROR_FILE_MISSING,
            "unable to stat '" . $self->{strDbPath} . "/" . MANIFEST_TARGET_PGTBLSPC . "': No such file or directory");

        # bOnline = true tests - Compare the base manifest
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestBase, $oManifest)}, "", 'base manifest');
        $self->testException(
            sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_ASSERT,
            'manifest has already been built');

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

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'paths/files and different modes');

        # Master = false (what can be copied from a standby vs the master)
        #---------------------------------------------------------------------------------------------------------------------------
        my $strOidFile = '11111';
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_BASE . '/' . $strOidFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strOidFile);

        # Update expected manifest
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strOidFile,
            MANIFEST_SUBKEY_MASTER, undef, false);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strOidFile,
            MANIFEST_SUBKEY_SIZE, length($strOidFile));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strOidFile,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'master false');

        # Create a pg_config path and file link
        my $strConfFile = '/pg_config/postgresql.conf';
        my $strConfContent = "listen_addresses = *\n";
        storageDb()->pathCreate('pg_config');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . $strConfFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strConfContent);

        # link db/pg_config/postgresql.conf.link -> ./postgresql.conf
        testLinkCreate($self->{strDbPath} . $strConfFile . '.link', './postgresql.conf');

        # INVESTIGATE: on the command line, these links appear to be fine but in the code, a debug line prior to the recursive call to build() produces:
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

        # INVESTIGATE: Even though the below code creates a duplicate link, this error occurs (note the pg_config/pg_config):
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

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'link');

        # Create a link loop and expect an error
        #---------------------------------------------------------------------------------------------------------------------------
        testLinkCreate($self->{strDbPath} . '/pgdata', $self->{strDbPath});

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(
            sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_FORMAT,
            'recursion in manifest build exceeds depth of 16: pg_data/pgdata/pgdata/pgdata/pgdata/pgdata/pgdata/pgdata/pgdata/' .
                "pgdata/pgdata/pgdata/pgdata/pgdata/pgdata/pgdata/pg_config/postgresql.conf.link\n" .
                'HINT: is there a link loop in $PGDATA?');

        testFileRemove($self->{strDbPath} . '/pgdata');

        # Test skip files/directories
        #---------------------------------------------------------------------------------------------------------------------------
        # Create files to skip
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTGRESQLAUTOCONFTMP ), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_BACKUPLABELOLD), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTMASTEROPTS), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTMASTERPID), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_RECOVERYCONF), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_RECOVERYDONE), '');

        # Create temp table files in a data directory - these will be skipped
        my $strDbDataDirBasePath = "/" . DB_PATH_BASE . "/12134";
        my $strDbDataDir = $self->{strDbPath} . $strDbDataDirBasePath;
        storageDb()->pathCreate($strDbDataDir);

        my $strTempFileOid = '/t123_456';
        storageDb()->put(storageDb()->openWrite($strDbDataDir . $strTempFileOid,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strDbDataDir . $strTempFileOid . '.1',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strDbDataDir . $strTempFileOid . '_fsm',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strDbDataDir . $strTempFileOid . '_vm.12',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');

        # Create unlogged files to skip
        my $strUnlogFileOid = '/12345';
        my $strUnlogFile = $strDbDataDir . $strUnlogFileOid;
        storageDb()->put(storageDb()->openWrite($strUnlogFile . '_init',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), ''); # will not be skipped
        storageDb()->put(storageDb()->openWrite($strUnlogFile . '_init.1',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), ''); # will not be skipped
        storageDb()->put(storageDb()->openWrite($strUnlogFile . '_vm.1_vm',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), ''); # will not be skipped
        storageDb()->put(storageDb()->openWrite($strUnlogFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strUnlogFile . '.11',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strUnlogFile . '_fsm.1',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strUnlogFile . '_vm',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');

        # Create an OID file and instead of OID_init file create a directory - the OID file and directory will NOT be skipped
        my $strNotUnlogFileOid = '/54321';
        my $strNotUnlogFile = $strDbDataDir . $strNotUnlogFileOid;
        storageDb()->put(storageDb()->openWrite($strNotUnlogFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->pathCreate($strNotUnlogFile . '_init');

        # Create a temp-looking file not in the database directory - this will not be skipped
        my $strTempNoSkip = '/' . DB_PATH_BASE . '/t111_111';
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . $strTempNoSkip,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');

        # Create directories to skip. Add a bogus file to them for test coverage.
        storageDb()->pathCreate(DB_FILE_PREFIX_TMP);
        storageDb()->pathCreate('pg_xlog');
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

        # Update expected manifest with files and paths that will not be skipped
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . '/pg_xlog', undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGDYNSHMEM, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGNOTIFY, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGREPLSLOT, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSERIAL, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSNAPSHOTS, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSTATTMP, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_PGSUBTRANS, undef, $hDefault);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strTempNoSkip,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strTempNoSkip,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strUnlogFileOid .
            '_init', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strUnlogFileOid .
            '_init', MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strUnlogFileOid .
            '_init.1', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strUnlogFileOid .
            '_init.1', MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strUnlogFileOid .
            '_vm.1_vm', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strUnlogFileOid .
            '_vm.1_vm', MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strNotUnlogFileOid,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strNotUnlogFileOid,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath . $strNotUnlogFileOid .
            '_init', undef, $hDefault);

        # The number of files that can be retrieved from the standby has increased so default for [target:file:default] "master"
        # setting is now expected to be false and the files that must be copied from the master will individually change to true
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, false);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/" .  DB_PATH_BASE . "/" . $strOidFile,
            MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGVERSION,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strConfFile,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MASTER, true);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'skip directories/files');

        # Unskip code path coverage
        #---------------------------------------------------------------------------------------------------------------------------
        my $oManifestExpectedUnskip = dclone($oManifestExpected);

        # Change DB version to 93
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_93, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_93)});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_93);
        $oManifestExpectedUnskip->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
            $self->dbCatalogVersion(PG_VERSION_93));

        # Update expected manifest
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_93, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_93)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 94 directories');

        # Change DB version to 91
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_91, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_91)});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_91);
        $oManifestExpectedUnskip->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
            $self->dbCatalogVersion(PG_VERSION_91));

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_91, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_91)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 92 directories');

        # Change DB version to 90
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_90, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_90)});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_90);
        $oManifestExpectedUnskip->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
            $self->dbCatalogVersion(PG_VERSION_90));

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSERIAL . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSERIAL . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        # Any file that has a pattern for unlogged tables will not be skipped.
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid, MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid, MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '.11', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '.11', MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_fsm.1', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_fsm.1', MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_vm', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_vm', MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        # The number of files that can be retrieved from the standby has increased so default for [target:file:default] "master"
        # setting is now expected to be false and the files that must be copied from the master will individually change to true
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, false);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/" .  DB_FILE_PGVERSION,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strConfFile,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGDYNSHMEM . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGREPLSLOT . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSNAPSHOTS . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSERIAL . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpectedUnskip->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' .
            $strOidFile, MANIFEST_SUBKEY_MASTER);
        $oManifestExpectedUnskip->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_init', MANIFEST_SUBKEY_MASTER);
        $oManifestExpectedUnskip->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_init.1', MANIFEST_SUBKEY_MASTER);
        $oManifestExpectedUnskip->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_vm.1_vm', MANIFEST_SUBKEY_MASTER);
        $oManifestExpectedUnskip->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strTempNoSkip,
            MANIFEST_SUBKEY_MASTER);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_90, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_90)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 91 directories');

        # Change DB version to 84
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_84, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_84)});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_84);
        $oManifestExpectedUnskip->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
            $self->dbCatalogVersion(PG_VERSION_84));

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGNOTIFY . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGNOTIFY . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGNOTIFY . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);

        # Files that look like "temp" object files will no longer be skipped
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid, MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid, MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid . '.1', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid . '.1', MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid . '_fsm', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid . '_fsm', MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid . '_vm.12', MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strTempFileOid . '_vm.12', MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_84, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_84)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 90 directories');

        # Change DB version to 83
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_83, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_83)});
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_83);
        $oManifestExpectedUnskip->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
            $self->dbCatalogVersion(PG_VERSION_83));

        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSTATTMP . '/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpectedUnskip->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSTATTMP . '/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpectedUnskip->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_PATH_PGSTATTMP . '/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_83, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_83)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpectedUnskip, $oManifest)}, "", 'unskip 84 directories');

        # Reset Manifest for next tests
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

        # Remove files and expect manifest entries not necessary for the rest of the tests
        testPathRemove($strDbDataDir);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_init.1');
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_init');
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strUnlogFileOid . '_vm.1_vm');
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, true);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strOidFile,
            MANIFEST_SUBKEY_MASTER, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strTempNoSkip,
            MANIFEST_SUBKEY_MASTER, false);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/" .  DB_FILE_PGVERSION,
            MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strConfFile,
            MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strNotUnlogFileOid);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . $strDbDataDirBasePath .
            $strNotUnlogFileOid . '_init');

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'manifest reset');

        # Tablespaces
        #---------------------------------------------------------------------------------------------------------------------------
        # Create pg_tblspc path
        my $strTblSpcPath = $self->{strDbPath} . '/' . DB_PATH_PGTBLSPC;
        storageDb()->pathCreate($strTblSpcPath, {bCreateParent => true});

        # Create a directory in pg_tblspc
        storageDb()->pathCreate("$strTblSpcPath/" . BOGUS, {strMode => '0700'});
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_LINK_EXPECTED,
            MANIFEST_TARGET_PGTBLSPC . "/" . BOGUS . " is not a symlink - " . DB_PATH_PGTBLSPC . " should contain only symlinks");

        testPathRemove("${strTblSpcPath}/" . BOGUS);

        my $strTblspcId = '99999';

        # Invalid relative tablespace is ../
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '../');
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_TABLESPACE_IN_PGDATA,
            'tablespace symlink ../ destination must not be in $PGDATA');
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Invalid relative tablespace is ..
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '..');
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_TABLESPACE_IN_PGDATA,
            'tablespace symlink .. destination must not be in $PGDATA');
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Invalid relative tablespace is ../base - a subdirectory of $PGDATA
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '../base');
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_TABLESPACE_IN_PGDATA,
            'tablespace symlink ../base destination must not be in $PGDATA');
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Invalid absolute tablespace is $self->{strDbPath} . /base
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", $self->{strDbPath} . '/base');
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(
            sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_TABLESPACE_IN_PGDATA,
            "tablespace symlink $self->{strDbPath}/base destination must not be in \$PGDATA");
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Invalid relative tablespace is ../../BOGUS - which is not in $PGDATA and does not exist
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", '../../' . BOGUS);
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, ERROR_ASSERT,
            "tablespace with oid ${strTblspcId} not found in tablespace map\n" .
            "HINT: was a tablespace created or dropped during the backup?");
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Create tablespace directory outside PGDATA
        my $strTablespace = 'tablespace';
        storageTest()->pathCreate($strTablespace);

        my $strIntermediateLink = $self->testPath() . "/intermediate_link";

        # Create a link to a link
        testLinkCreate($strIntermediateLink, $self->testPath() . '/' . $strTablespace);
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", $strIntermediateLink);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, false, false)}, ERROR_LINK_DESTINATION,
            "link '${strTblSpcPath}/${strTblspcId}' -> '$strIntermediateLink' cannot reference another link");

        testFileRemove($strIntermediateLink);
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

        # Reload the manifest otherwise it will contain invalid data from the above exception tests
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

        # Create a valid symlink pg_tblspc/1 to tablespace/ts1/1 directory
        my $strTablespaceOid = '1';
        my $strTablespaceName = "ts${strTablespaceOid}";
        storageTest()->pathCreate("${strTablespace}/${strTablespaceName}/${strTablespaceOid}", {bCreateParent => true});
        my $strTablespacePath = storageTest()->pathGet("${strTablespace}/${strTablespaceName}/${strTablespaceOid}");
        testLinkCreate("${strTblSpcPath}/${strTablespaceOid}", $strTablespacePath);

        # Create the directory PG would create when the sql CREATE TABLESPACE is run
        my $strTblspcVersion = 'PG_9.4_201409291';
        my $strTblspcDir = $strTblspcVersion . '/11';
        storageTest()->pathCreate("$strTablespacePath/$strTblspcDir", {bCreateParent => true});

        # Create unlogged and temp files
        storageDb()->put(storageDb()->openWrite($strTablespacePath . '/' . $strTblspcDir . $strUnlogFileOid . '_init',
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strTablespacePath . '/' . $strTblspcDir . $strUnlogFileOid,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strTablespacePath . '/' . $strTblspcDir . $strTempFileOid,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');
        storageDb()->put(storageDb()->openWrite($strTablespacePath . '/' . $strTblspcDir . '/' . $strOidFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), '');

        # The number of files that can be retrieved from the standby has increased so default for [target:file:default] "master"
        # setting is now expected to be false and the files that must be copied from the master will individually change to true
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_FILE_PGVERSION,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strConfFile,
            MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MASTER, true);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/" .  DB_PATH_BASE . "/" . $strOidFile,
            MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strTempNoSkip,
            MANIFEST_SUBKEY_MASTER);

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
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, $strMfTs . '/' . $strTblspcVersion, undef, $hDefault);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, $strMfTs . '/' . $strTblspcDir, undef, $hDefault);

        # Don't skip unlog init file or normal oid files
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strUnlogFileOid . '_init',
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strUnlogFileOid . '_init',
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . '/' . $strOidFile,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . '/' . $strOidFile,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        # In offline mode, do not skip the db WAL path
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MODE, MODE_0644);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_MODE, MODE_0644);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_MASTER, true);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'offline with valid tablespace - do not skip database WAL directory and only copy unlogged init file');

        # Create tablespace and database maps
        my $hTablespaceMap = {};
        $hTablespaceMap->{$strTablespaceOid} = $strTablespaceName;

        my $hDatabaseMap = {};
        $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_ID} = 12345;
        $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_LAST_SYSTEM_ID} = 67890;

        $oManifestExpected->numericSet(MANIFEST_SECTION_DB, BOGUS, MANIFEST_KEY_DB_ID, $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_ID});
        $oManifestExpected->numericSet(MANIFEST_SECTION_DB, BOGUS, MANIFEST_KEY_DB_LAST_SYSTEM_ID,
            $hDatabaseMap->{&BOGUS}{&MANIFEST_KEY_DB_LAST_SYSTEM_ID});

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false, false, $hTablespaceMap, $hDatabaseMap);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'offline passing tablespace map and database map');

        # Exclusions
        #---------------------------------------------------------------------------------------------------------------------------
        # Excluded links
        storageDb()->linkCreate('/dev/null', 'postgresql.auto.conf');
        storageDb()->linkCreate('/etc/hosts', 'hosts');

        # Exclude log files but not directory
        storageDb()->pathCreate('pg_log');
        storageDb()->put(storageDb()->openWrite('pg_log/logfile'), 'EXCLUDE');
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . '/pg_log', undef, $hDefault);

        # Exclude directory and all contents
        storageDb()->pathCreate('global/exclude');
        storageDb()->put(storageDb()->openWrite('global/exclude/exclude.txt'), 'EXCLUDE');

        # Test exclusions against the ideal manifest
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(
            storageDb(), $self->{strDbPath}, undef, false, false, $hTablespaceMap, $hDatabaseMap,
            {'postgresql.auto.conf' => true, 'hosts' => true, 'pg_log/' => true, 'global/exclude' => true});
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'check exclusions');

        # Remove excluded files so we don't have to pass exclusions to the rest of the tests
        storageDb()->remove('postgresql.auto.conf');
        storageDb()->remove('hosts');
        storageDb()->remove('pg_log/logfile');
        storageDb()->remove('global/exclude', {bRecurse => true});

        # Reload the manifest with version < 9.0
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_84, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_84)});

        # Catalog not stored in < 9.0
        $oManifestExpected->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_84);
        $oManifestExpected->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
            $self->dbCatalogVersion(PG_VERSION_84));

        # The number of files that can be retrieved from the standby has been superseded so [target:file:default] "master"
        # setting is now expected to be true and the files that must be copied from the master will individually change to false
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE . ":default", MANIFEST_SUBKEY_MASTER, undef, true);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . "/" .  DB_FILE_PGVERSION,
            MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strConfFile,
            MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL, MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->remove(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_MASTER);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' .
            $strOidFile, MANIFEST_SUBKEY_MASTER, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . $strTempNoSkip,
            MANIFEST_SUBKEY_MASTER, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strUnlogFileOid . '_init',
            MANIFEST_SUBKEY_MASTER, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . '/' . $strOidFile,
            MANIFEST_SUBKEY_MASTER, false);

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

        # Add unskip of temp and unlog file patterns
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strUnlogFileOid,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strUnlogFileOid,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strUnlogFileOid,
            MANIFEST_SUBKEY_MASTER, false);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strTempFileOid,
            MANIFEST_SUBKEY_SIZE, 0);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strTempFileOid,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, $strMfTs . '/' . $strTblspcDir . $strTempFileOid,
            MANIFEST_SUBKEY_MASTER, false);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false, false, $hTablespaceMap, $hDatabaseMap);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'tablespace with version < 9.0');

        $oManifestExpected->remove(MANIFEST_SECTION_DB);

        # Undefined user/group
        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("sudo chgrp 777 " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
        executeTest("sudo chown 777 " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_GROUP, false);
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/pg_xlog/' . BOGUS,
            MANIFEST_SUBKEY_USER, false);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_84, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_84)});
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, false, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'undefined user/group');

        # Reset the group / owner
        executeTest("sudo chgrp " . TEST_GROUP . " " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
        executeTest("sudo chown " . TEST_USER . " " . $self->{strDbPath} . '/pg_xlog/' . BOGUS);
    }

    ################################################################################################################################
    if ($self->begin('get/set'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

        # SubKey required but has not been set
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE)},
            ERROR_ASSERT, "strSection '" . MANIFEST_SECTION_TARGET_FILE . "', strKey '" . BOGUS . "', strSubKey '" .
            MANIFEST_SUBKEY_CHECKSUM_PAGE . "' is required but not defined");

        # SubKey not required and not set
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE, false)},
            false, 'boolGet() - false');

        # Set and get a boolean value
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->boolSet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE, true);
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_CHECKSUM_PAGE)}, true,
            'boolGet() - true');

        # Get default boolean value
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, false, true)},
            true, 'boolGet() - default true');

        # Get but don't return a default boolean value even if there is supposed to be one
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef, false, false)},
            false, 'boolGet() - default false');

        # Get default numeric value
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->numericGet(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_SIZE, false, 0)},
            0, 'numericGet() - default 0');

        # Coverage for $self->SUPER::get("${strSection}:default"
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->get(MANIFEST_SECTION_TARGET_FILE, BOGUS, MANIFEST_SUBKEY_USER, false)},
            "[undef]", 'get() - default section');

        # Get the correct path for the control file in the DB
        #---------------------------------------------------------------------------------------------------------------------------
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
        $self->testResult(sub {$oManifest->repoPathGet($strTablespace)}, $strTablespace,
            'repoPathGet() - tablespace - no tablepace-id nor subpath');

        # repoPathGet - fully qualified tablespace target
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->tablespacePathGet()}, "PG_" . PG_VERSION_94 . "_" .
            $self->dbCatalogVersion(PG_VERSION_94), 'tablespacePathGet()');

        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTablespace, MANIFEST_SUBKEY_TABLESPACE_ID, $strTablespaceId);
        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTablespace, MANIFEST_SUBKEY_TABLESPACE_NAME, $strTablespaceName);
        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, $strTablespace, MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_LINK);

        $self->testResult(sub {$oManifest->repoPathGet($strTablespace, BOGUS)}, $strTablespace . "/PG_" . PG_VERSION_94 . "_" .
            $self->dbCatalogVersion(PG_VERSION_94) . "/" . BOGUS, 'repoPathGet() - tablespace valid with subpath');

        # Set the DB version to < 9.0 - there is no special sudirectory in earlier PG versions
        $oManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_84);
        $self->testResult(sub {$oManifest->repoPathGet($strTablespace, BOGUS)}, $strTablespace . "/" . BOGUS,
            'repoPathGet() - tablespace in 8.4 valid with subpath');
        $oManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_94);

        # isTargetLink
        #---------------------------------------------------------------------------------------------------------------------------
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
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

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
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

        $self->testResult(sub {$oManifest->dbVersion()}, PG_VERSION_94, 'dbVersion < 10');
        $self->testResult(sub {$oManifest->xactPath()}, 'pg_clog', '    xactPath - pg_clog');
        $self->testResult(sub {$oManifest->walPath()}, 'pg_xlog', '    walPath - pg_xlog');

        # dbVersion, xactPath and walPath - PG >= 10
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_10,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_10)});

        $self->testResult(sub {$oManifest->dbVersion()}, PG_VERSION_10, 'dbVersion >= 10');
        $self->testResult(sub {$oManifest->xactPath()}, 'pg_xact', '    xactPath - pg_xact');
        $self->testResult(sub {$oManifest->walPath()}, 'pg_wal', '    walPath - pg_wal');
    }

    ################################################################################################################################
    if ($self->begin('validate()'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

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
        my $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

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

        $self->testResult(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false)}, "[undef]",
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
        $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $self->testResult(sub {$oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true, false)}, "[undef]",
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

        # Check reference
        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true, false);
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

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'updates from last manifest');

# CSHANG Add tests to:
# 1) Update the timestamp in the lastManifest for $strTestNew so it is different and pass delta=true - expected manifest should not change
# 2) Set delta to false - the file will not be kept and will instead be recopied and updated in manifest
# 3) 0 byte file in last manifest with different timestamp with/without delta will always get reference to prior backup - expected manifest should have 'bogus' backup label as reference



        # MANIFEST_SUBKEY_CHECKSUM_PAGE = false and MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR set/not set
        #---------------------------------------------------------------------------------------------------------------------------
        $oLastManifest->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, false);

        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE, false);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'checksum-page false, checksum-page-error not set');

        my @iyChecksumPageError = (1);
        $oLastManifest->set(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR, \@iyChecksumPageError);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE,  MANIFEST_TARGET_PGDATA . '/' . $strTestNew,
            MANIFEST_SUBKEY_CHECKSUM_PAGE_ERROR, \@iyChecksumPageError);

        $oManifest = new pgBackRest::Manifest(
            $strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94, iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        $oManifest->build(storageDb(), $self->{strDbPath}, $oLastManifest, true, false);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "",
            'checksum-page false, checksum-page-error set');
    }

    ################################################################################################################################
    if ($self->begin('fileAdd()'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94,
            iDbCatalogVersion => $self->dbCatalogVersion(PG_VERSION_94)});
        my $oManifestExpected = dclone($oManifestBase);

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true, false);

        # Add a file after building manifest
        my $lTimeTest = $lTime + 10;
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . $strTest,
            {strMode => MODE_0644, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTimeTest}), $strTest);

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
