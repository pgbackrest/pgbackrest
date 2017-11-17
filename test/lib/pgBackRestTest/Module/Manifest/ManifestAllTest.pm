####################################################################################################################################
# ManifestAllTest.pm - Unit tests for Manifest module
####################################################################################################################################
package pgBackRestTest::Module::Manifest::ManifestAllTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

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
use pgBackRestTest::Env::HostEnvTest;
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
    #
    # storageTest()->remove($self->{strExpectedManifest});
    # storageTest()->remove("${strTestPath}/actual.manifest");
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
# CSHANG On load, why does load require the db version but nothing else, esp when tablespacePathGet requires catalog to be set?
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # bOnline = true tests - Compare the base manifest
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestBase, $oManifest)}, "", 'base manifest');

        # Create expected manifest from base
        my $oManifestExpected = dclone($oManifestBase);

        # Add global/pg_control file and PG_VERSION file and create a directory with a different modes than default
        storageDb()->copy($self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin',
            storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_PGCONTROL,
            {strMode => MODE_0644, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
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

        $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'paths/files and different modes');

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
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . $strConfFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strConfContent);
        testLinkCreate($self->{strDbPath} . $strConfFile . '.link', './postgresql.conf');

# CSHANG Even though this creates a duplicate link, this error occurs (note the pg_config/pg_config): 'unable to stat '/home/ubuntu/test/test-0/db/pg_config/pg_config/./postgresql.conf': No such file or directory'
# ubuntu@pgbackrest-test:~$ ls -l /home/ubuntu/test/test-0/db/pg_config/
# total 4
# -rw------- 1 ubuntu ubuntu 21 Nov  9 17:07 postgresql.conf
# lrwxrwxrwx 1 ubuntu ubuntu 17 Nov  9 19:53 postgresql.conf.bad.link -> ./postgresql.conf
# lrwxrwxrwx 1 ubuntu ubuntu 17 Nov  9 19:53 postgresql.conf.link -> ./postgresql.conf
        # # This link will cause errors because it points to the same location as above
        # testLinkCreate($self->{strDbPath} . $strConfFile . '.bad.link', './postgresql.conf');
        #
        # $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, true)}, ERROR_LINK_DESTINATION,
        #     'tablespace symlink ../base destination must not be in $PGDATA');
        # testFileRemove($self->{strDbPath} . $strConfFile . '.link.bad');

        # Update expected manifest
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_BASE, MANIFEST_SUBKEY_MODE, MODE_0700);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . '/pg_config', undef, $hDefault)

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
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTGRESQLAUTOCONFTMP ), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_BACKUPLABELOLD), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTMASTEROPTS), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_POSTMASTERPID), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_RECOVERYCONF), '');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_RECOVERYDONE), '');

        # Create directories to skip. Add a bogus file to them for test coverage.
        storageDb()->pathCreate(DB_FILE_PREFIX_TMP);
        storageDb()->pathCreate('pg_xlog');
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/pg_xlog/' . BOGUS, {lTimestamp => $lTime}), '');
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
        my $strTblSpcPath = $self->{strDbPath} . '/' . MANIFEST_TARGET_PGTBLSPC;
        storageDb()->pathCreate($strTblSpcPath, {bCreateParent => true});

        # Create a directory in pg_tablespace
        storageDb()->pathCreate("$strTblSpcPath/" . BOGUS, {strMode => '0700'});
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
        storageTest()->pathCreate('tablespace');

        my $strIntermediateLink = $self->{strDbPath} . "/intermediate_link";

        # Create a link to a link
        testLinkCreate($strIntermediateLink, $self->testPath() . '/tablespace');
        testLinkCreate("${strTblSpcPath}/${strTblspcId}", $strIntermediateLink);

        $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, false)}, ERROR_LINK_DESTINATION,
            "link '${strTblSpcPath}/${strTblspcId}' -> '$strIntermediateLink' cannot reference another link");

        testFileRemove($self->{strDbPath} . "/intermediate_link");
        testFileRemove("${strTblSpcPath}/${strTblspcId}");

#        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'tablespace');

# CSHANG Why is this correct? the manifest is not really valid yet, is it? What if we saved it at this point - or any point before validating? Should we force a validate before save?
        # $self->testResult(sub {$oManifest->validate()}, "[undef]", 'manifest validated');
    # executeTest(
    #     'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $self->{strDbPath} . '/' .
    #     DB_FILE_PGCONTROL);
    #     # Online tests
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $oManifest->build(storageDb(), $self->{strDbPath}, undef, true);





        # # Build error if offline = true and no tablespace path
        # #---------------------------------------------------------------------------------------------------------------------------
        # $self->testException(sub {$oManifest->build(storageDb(), $self->{strDbPath}, undef, false)}, ERROR_FILE_MISSING,
        #     "unable to stat '" . $self->{strDbPath} . "/" . MANIFEST_TARGET_PGTBLSPC . "': No such file or directory");
        #
        # # Create pg_tblspc path
        # storageTest()->pathCreate($self->{strDbPath} . "/" . MANIFEST_TARGET_PGTBLSPC);
# CSHANG Not sure if the result of building the manifest at this point is correct behavior. It builds the following content, but are these the minimum that a manifest can have for it to be valid?
    #  'oContent' => {
    #                  'target:file:default' => {
    #                                             'mode' => '0644',
    #                                             'master' => bless( do{\(my $o = 1)}, 'JSON::PP::Boolean' ), XXXXX
    #                                             'user' => 'ubuntu',
    #                                             'group' => 'ubuntu'
    #                                           },
    #                  'backup' => {
    #                                'backup-timestamp-copy-start' => 1509384645
    #                              },
    #                  'target:file' => {
    #                                     'pg_data/global/pg_control' => {
    #                                                                      'size' => 8192,
    #                                                                      'timestamp' => 1509384645
    #                                                                    }
    #                                   },
    #                  'target:path' => {
    #                                     'pg_data/pg_tblspc' => {},
    #                                     'pg_data' => {},
    #                                     'pg_data/global' => {}
    #                                   },
    #                  'target:path:default' => {
    #                                             'group' => 'ubuntu',
    #                                             'user' => 'ubuntu',
    #                                             'mode' => '0750'
    #                                           },
    #                  'backup:target' => {
    #                                       'pg_data' => {
    #                                                      'path' => '/home/ubuntu/test/test-0/db',
    #                                                      'type' => 'path'
    #                                                    }
    #                                     },
    #                  'backrest' => {
    #                                  'backrest-format' => 5,
    #                                  'backrest-version' => '1.25'
    #                                },
    #                  'backup:db' => {
    #                                   'db-version' => '9.4'
    #                                 }
    #                },

# CSHANG It seems not, so shouldn't the build function check for a minimum set:
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
# CSHANG At least in the validate function? This is what the build provides - so why is there no checksum for pg_control in target:file? - at this point it fails the validate? manifest subvalue 'checksum' not set for file 'pg_data/global/pg_control'

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

        # SubKey not require and not set
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
# CSHANG Should MANIFEST_SUBKEY_TYPE be required for MANIFEST_SUBKEY_PATH? And why is there a MANIFEST_SUBKEY_FILE - shouldn't there be a MANIFEST_VALUE_FILE instead? pg_data/postgresql.conf={"file":"postgresql.conf","path":"../pg_config","type":"link"} is the only time I see it in the mock expect tests and the type is link - in restore, I see it firsts tests for link and then file - is this the only way "file" is used?
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
}

1;
