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


        $self->testException(sub {(new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false}))}, ERROR_ASSERT,
            'strDbVersion must be provided with bLoad = false');

        my $oManifest = $self->testResult(sub {(new pgBackRest::Manifest($strBackupManifestFile,
            {bLoad => false, strDbVersion => PG_VERSION_94}))}, "[object]", 'manifest instantiated');

        $self->testResult(sub {$oManifest->test(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef, PG_VERSION_94) &&
            $oManifest->test(INI_SECTION_BACKREST, INI_KEY_FORMAT, undef, $oManifest->{iInitFormat}) &&
            $oManifest->test(INI_SECTION_BACKREST, INI_KEY_VERSION, undef, $oManifest->{strInitVersion})}, true,
            '    manifest initialized');

        storageRepo()->pathCreate($strBackupPath);
        $self->testResult(sub {$oManifest->save()}, "[undef]", 'manifest saved');

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
        my $oBackupManifest = new pgBackRest::Manifest($strBackupManifestFile, {bLoad => false, strDbVersion => PG_VERSION_94});

        $self->testException(sub {$oBackupManifest->build(storageDb(), $strDbMasterPath, undef, false)}, ERROR_FILE_MISSING,
            "unable to stat '" . $strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC . "': No such file or directory");

        # Create pg_tblspc path
        storageTest()->pathCreate($strDbMasterPath . "/" . MANIFEST_TARGET_PGTBLSPC);
        $oBackupManifest->build(storageDb(), $strDbMasterPath, undef, false);
use Data::Dumper; print "MANIFEST: ".Dumper($oBackupManifest);
    }
}

1;
