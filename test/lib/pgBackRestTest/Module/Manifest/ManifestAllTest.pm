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
    my $strDbMasterPath = cfgOption(cfgOptionIndex(CFGOPT_DB_PATH, 1));
    my $iDbCatalogVersion = 201409291;

    my $lTime = time() - 10000;
    my $strTest = 'test';

    my $oManifestExpected = new pgBackRest::Common::Ini($self->{strExpectedManifest}, {bLoad => false});

    # Section: backup:db
    $oManifestExpected->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_94);
    # Section: target:path
    my $hDefault = {};
    $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, undef, $hDefault);
    $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_GLOBAL, undef, $hDefault);
    # Section: target:path:default
    $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0750);
    $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH . ":default", MANIFEST_SUBKEY_GROUP, undef, TEST_GROUP);
    $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH . ":default", MANIFEST_SUBKEY_USER, undef, TEST_USER);
    # Section backup:target
    $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_TYPE, MANIFEST_VALUE_PATH);
    $oManifestExpected->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH, $self->{strDbPath});

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

        # bOnline = true tests
        #---------------------------------------------------------------------------------------------------------------------------
        $oManifest->build(storageDb(), $strDbMasterPath, undef, true);

        # Compare the base manifest
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'base manifest');

        # Add global/pg_control file and PG_VERSION file and create a directory with a different mode than default
        storageDb()->copy($self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin',
            storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_PGCONTROL,
            {strMode => MODE_0644, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}));
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_FILE_PGVERSION,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), PG_VERSION_94);

# CSHANG !!! creating test file here only to stop target:file:default mode from flapping between 600 and 644 - need to resolve and remove
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . $strTest,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strTest);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest,
            MANIFEST_SUBKEY_SIZE, length($strTest));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . $strTest,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

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

        $oManifest->build(storageDb(), $strDbMasterPath, undef, true);

        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'paths/files and different modes');

# CSHANG Why does base/test file result in "master":false here? This means we will not be copying it? I tried the same test in the doc container where base path mode=700 and base/test mode = 600 and ran a full backup and that results in pg_data/base/test={"checksum":"4e1243bd22c66e76c2ba9eddc1f91394e57f9f83","repo-size":25,"size":5,"timestamp":1510071781} with pg_data/base/test.gz.  I also tried changint the time to be - 100 but that still resulted here in master=false
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_BASE . '/' . $strTest,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strTest);

        # Update expected manifest
        $oManifestExpected->boolSet(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strTest,
            MANIFEST_SUBKEY_MASTER, undef, false);
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strTest,
            MANIFEST_SUBKEY_SIZE, length($strTest));
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_FILE, MANIFEST_TARGET_PGDATA . '/' . DB_PATH_BASE . '/' . $strTest,
            MANIFEST_SUBKEY_TIMESTAMP, $lTime);

        $oManifest->build(storageDb(), $strDbMasterPath, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'master false');

        # Create a pg_config path and file link
        my $strConfFile = '/pg_config/postgresql.conf';
        my $strConfContent = "listen_addresses = *\n";
        storageDb()->pathCreate('pg_config', {strMode => MODE_0700});
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . $strConfFile,
            {strMode => MODE_0600, strUser => TEST_USER, strGroup => TEST_GROUP, lTimestamp => $lTime}), $strConfContent);
        testLinkCreate($self->{strDbPath} . $strConfFile . '.link', './postgresql.conf');

        # Update expected manifest
# CSHANG !!! creating pg_stat here only to stop target:path:default mode from flapping between 750 and 700 - need to resolve and remove. At this point we have global/ 750, base/ 700 and pg_config/ 700 so the target:path:defaul mode flaps between 750 and 700 maybe because target:path contains 4 because of pg_data - which doesn't actually exist unless it is taking the db/ path itself into account for that?
        storageTest()->pathCreate($self->{strDbPath} . '/pg_stat', {strMode => MODE_0750});
        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . '/pg_stat', undef, $hDefault);
        # $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH . ":default", MANIFEST_SUBKEY_MODE, undef, MODE_0700);
        # $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_MODE, MODE_0750);
        # $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_PATH_GLOBAL, MANIFEST_SUBKEY_MODE, MODE_0750);

        $oManifestExpected->set(MANIFEST_SECTION_TARGET_PATH, MANIFEST_TARGET_PGDATA . '/pg_config',
            MANIFEST_SUBKEY_MODE, MODE_0700);
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

        $oManifest->build(storageDb(), $strDbMasterPath, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'link');

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
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/pg_xlog/' . BOGUS), '');
        storageDb()->pathCreate(DB_PATH_PGDYNSHMEM);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGDYNSHMEM . '/' . BOGUS), '');
        storageDb()->pathCreate(DB_PATH_PGNOTIFY);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGNOTIFY . '/' . BOGUS), '');
        storageDb()->pathCreate(DB_PATH_PGREPLSLOT);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGREPLSLOT . '/' . BOGUS), '');
        storageDb()->pathCreate(DB_PATH_PGSERIAL);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSERIAL . '/' . BOGUS), '');
        storageDb()->pathCreate(DB_PATH_PGSNAPSHOTS);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSNAPSHOTS . '/' . BOGUS), '');
        storageDb()->pathCreate(DB_PATH_PGSTATTMP);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSTATTMP . '/' . BOGUS), '');
        storageDb()->pathCreate(DB_PATH_PGSUBTRANS);
        storageDb()->put(storageDb()->openWrite($self->{strDbPath} . '/' . DB_PATH_PGSUBTRANS . '/' . BOGUS), '');
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

        $oManifest->build(storageDb(), $strDbMasterPath, undef, true);
        $self->testResult(sub {$self->manifestCompare($oManifestExpected, $oManifest)}, "", 'skip directories/files');

# CSHANG Why is this correct? the manifest is not really valid yet, is it? What if we saved it at this point - or any point before validating?
        # $self->testResult(sub {$oManifest->validate()}, "[undef]", 'manifest validated');
    # executeTest(
    #     'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $self->{strDbPath} . '/' .
    #     DB_FILE_PGCONTROL);
    #     # Online tests
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $oManifest->build(storageDb(), $strDbMasterPath, undef, true);

        # # Create pg_tblspc path
        # storageTest()->pathCreate($strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC);



        # # Build error if offline = true and no tablespace path
        # #---------------------------------------------------------------------------------------------------------------------------
        # $self->testException(sub {$oManifest->build(storageDb(), $strDbMasterPath, undef, false)}, ERROR_FILE_MISSING,
        #     "unable to stat '" . $strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC . "': No such file or directory");
        #
        # # Create pg_tblspc path
        # storageTest()->pathCreate($strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC);
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
