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

    # Copy a pg_control file into the pg_control path
    executeTest(
        'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $self->{strDbPath} . '/' .
        DB_FILE_PGCONTROL);
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

    if ($self->begin('build()'))
    {
        my $oManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        # Build error
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oManifest->build(storageDb(), $strDbMasterPath, undef, false)}, ERROR_FILE_MISSING,
            "unable to stat '" . $strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC . "': No such file or directory");

        # Create pg_tblspc path
        storageTest()->pathCreate($strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC);

        $oManifest->build(storageDb(), $strDbMasterPath, undef, false);
# CSHANG Not sure if the result of building the manifest at this point is correct behavior. It builds the following content, but are these the minimum that a manifest can have for it to be valid? It seems not, so shouldn't the build function check for a minimum set - like backup-type, backup-timestamp-start, etc? Or at least on the save? Or on load, mabe we should require all the DB section settings - why are we requiring the db version but nothing else?
    #  'oContent' => {
    #                  'target:file:default' => {
    #                                             'mode' => '0644',
    #                                             'master' => bless( do{\(my $o = 1)}, 'JSON::PP::Boolean' ),
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
    }

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

        # Set and get tablespace path
        #---------------------------------------------------------------------------------------------------------------------------
        my $iCatalogVersion = 1234;
        $oManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef, $iCatalogVersion);
        $self->testResult(sub {$oManifest->tablespacePathGet()}, "PG_" . PG_VERSION_94 . "_" . $iCatalogVersion,
            'tablespacePathGet()');

        # Get the correct path for the control file in the DB
        #---------------------------------------------------------------------------------------------------------------------------
# CSHANG Should MANIFEST_SUBKEY_TYPE be required for MANIFEST_SUBKEY_PATH?
        $oManifest->set(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH, $self->{strDbPath});
        $self->testResult(sub {$oManifest->dbPathGet($oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA,
            MANIFEST_SUBKEY_PATH), MANIFEST_FILE_PGCONTROL)},
            $self->{strDbPath} . "/" . DB_FILE_PGCONTROL, 'dbPathGet() - control file');

        # Get filename - no path passed
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oManifest->dbPathGet(undef, BOGUS)}, BOGUS, 'dbPathGet() - filename');
# CSHANG repoPathGet - I think this is more for when there are tablespaces? Else it just returns the same thing it was given, just concatenated with a slash in between. With tablespaces it will return the pg_data (seems to be the only thing ever passed as the first parameter) and then PG_9.4_1234 (e.g. db version and catalog number from the MANIFEST_SECTION_BACKUP_DB section) but in the code, this can never happen - there is only one place it is used (Restore->clean)
# use Data::Dumper; print "KEYS: ".Dumper($oManifest->keys(MANIFEST_SECTION_BACKUP_TARGET));
        # $oManifest->get(MANIFEST_SECTION_BACKUP_TARGET, MANIFEST_TARGET_PGDATA, MANIFEST_SUBKEY_PATH)
    }
}

1;
