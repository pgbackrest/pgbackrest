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

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::FileTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
    $self->{strSpoolPath} = "$self->{strArchivePath}/out";
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

    # Create pg_control path
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});

    # # Copy a pg_control file into the pg_control path
    # executeTest(
    #     'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $self->{strDbPath} . '/' .
    #     DB_FILE_PGCONTROL);
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

# CSHANG Why is this correct? the manifest is not really valid yet, is it? What if we saved it at this point - or any point before validating?
        $self->testResult(sub {$oManifest->validate()}, "[undef]", 'manifest validated');

        # # Build error --- DO LATER - want to start with empty directory
        # #---------------------------------------------------------------------------------------------------------------------------
        # $self->testException(sub {$oManifest->build(storageDb(), $strDbMasterPath, undef, false)}, ERROR_FILE_MISSING,
        #     "unable to stat '" . $strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC . "': No such file or directory");
        #
        # # Create pg_tblspc path
        # storageTest()->pathCreate($strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC);

        $oManifest->build(storageDb(), $strDbMasterPath, undef, false);

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
        foreach my $strFile ($oManifest->keys(MANIFEST_SECTION_TARGET_FILE))
        {
            $self->testException(sub {$oManifest->validate()}, ERROR_ASSERT,
                "manifest subvalue 'checksum' not set for file '${strFile}'");
        }
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
