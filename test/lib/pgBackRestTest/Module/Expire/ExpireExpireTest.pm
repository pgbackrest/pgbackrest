####################################################################################################################################
# ExpireExpireTest.pm - Tests for expire command
####################################################################################################################################
package pgBackRestTest::Module::Expire::ExpireExpireTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Backup::Info;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Expire;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::ExpireEnvTest;
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Env::HostEnvTest;

####################################################################################################################################
# initStanzaOption
####################################################################################################################################
sub initStanzaOption
{
    my $self = shift;
    my $oOption = shift;
    my $strDbBasePath = shift;
    my $strRepoPath = shift;
    my $oHostS3 = shift;

    $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->optionSetTest($oOption, OPTION_DB_PATH, $strDbBasePath);
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $strRepoPath);
    $self->optionSetTest($oOption, OPTION_LOG_PATH, $self->testPath());

    $self->optionBoolSetTest($oOption, OPTION_ONLINE, false);

    $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
    $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 6);

    if (defined($oHostS3))
    {
        $self->optionSetTest($oOption, OPTION_REPO_TYPE, REPO_TYPE_S3);
        $self->optionSetTest($oOption, OPTION_REPO_S3_KEY, HOST_S3_ACCESS_KEY);
        $self->optionSetTest($oOption, OPTION_REPO_S3_KEY_SECRET, HOST_S3_ACCESS_SECRET_KEY);
        $self->optionSetTest($oOption, OPTION_REPO_S3_BUCKET, HOST_S3_BUCKET);
        $self->optionSetTest($oOption, OPTION_REPO_S3_ENDPOINT, HOST_S3_ENDPOINT);
        $self->optionSetTest($oOption, OPTION_REPO_S3_REGION, HOST_S3_REGION);
        $self->optionSetTest($oOption, OPTION_REPO_S3_HOST, $oHostS3->ipGet());
        $self->optionBoolSetTest($oOption, OPTION_REPO_S3_VERIFY_SSL, false);
    }
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    use constant SECONDS_PER_DAY => 86400;
    my $lBaseTime = time() - (SECONDS_PER_DAY * 56);
    my $strDescription;
    my $oOption = {};

    my $bS3 = false;

    if ($self->begin("local"))
    {
        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3) = $self->setup(true, $self->expect(), {bS3 => $bS3});

        $self->initStanzaOption($oOption, $oHostDbMaster->dbBasePath(), $oHostBackup->{strRepoPath}, $oHostS3);
        $self->configLoadExpect(dclone($oOption), CMD_STANZA_CREATE);

        # Create the test object
        my $oExpireTest = new pgBackRestTest::Env::ExpireEnvTest(
            $oHostBackup, $self->backrestExe(), storageRepo(), $self->expect(), $self);

        $oExpireTest->stanzaCreate($self->stanza(), PG_VERSION_92);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Nothing to expire';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, 246);

        $oExpireTest->process($self->stanza(), 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest full backup, archive expire falls on segment major boundary';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest full backup';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, 256);
        $oExpireTest->process($self->stanza(), 1, 1, BACKUP_TYPE_FULL, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest diff backup, archive expire does not fall on major segment boundary';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, undef, 0);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, undef, 0);
        $oExpireTest->process($self->stanza(), 1, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire oldest diff backup (cascade to incr)';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 1, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire archive based on newest incr backup';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 1, 1, BACKUP_TYPE_INCR, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire diff treating full as diff';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 2, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire diff with retention-archive with warning retention-diff not set';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), undef, undef, BACKUP_TYPE_DIFF, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire full with retention-archive with warning retention-full not set';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), undef, undef, BACKUP_TYPE_FULL, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire no archive with warning since retention-archive not set for INCR';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 1, 1, BACKUP_TYPE_INCR, undef, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire no archive with warning since neither retention-archive nor retention-diff is set';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), undef, undef, BACKUP_TYPE_DIFF, undef, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Use oldest full backup for archive retention';
        $oExpireTest->process($self->stanza(), 10, 10, BACKUP_TYPE_FULL, 10, $strDescription);
    }

    if ($self->begin("Expire::stanzaUpgrade"))
    {
        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3) = $self->setup(true, $self->expect(), {bS3 => $bS3});

        $self->initStanzaOption($oOption, $oHostDbMaster->dbBasePath(), $oHostBackup->{strRepoPath}, $oHostS3);
        $self->configLoadExpect(dclone($oOption), CMD_STANZA_CREATE);

        # Create the test object
        my $oExpireTest = new pgBackRestTest::Env::ExpireEnvTest(
            $oHostBackup, $self->backrestExe(), storageRepo(), $self->expect(), $self);

        $oExpireTest->stanzaCreate($self->stanza(), PG_VERSION_92);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Create backups in current db version';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), undef, undef, BACKUP_TYPE_DIFF, undef, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Upgrade stanza and expire only earliest db backup and archive';

        $oExpireTest->stanzaUpgrade($self->stanza(), PG_VERSION_93);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, 246);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 3, undef, BACKUP_TYPE_FULL, undef, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Upgrade the stanza, create full back - earliest db orphaned archive removed and earliest full backup ' .
            'and archive in previous db version removed';

        $oExpireTest->stanzaUpgrade($self->stanza(), PG_VERSION_95);
        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 2, undef, BACKUP_TYPE_FULL, undef, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire all archive last full backup through pitr';

        $oExpireTest->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
        $oExpireTest->process($self->stanza(), 3, 1, BACKUP_TYPE_DIFF, 1, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $strDescription = 'Expire all archive except for the current database';

        $oExpireTest->process($self->stanza(), 2, undef, BACKUP_TYPE_FULL, undef, $strDescription);

        #-----------------------------------------------------------------------------------------------------------------------
        $self->optionReset($oOption, OPTION_DB_PATH);
        $self->optionReset($oOption, OPTION_ONLINE);
        $self->optionSetTest($oOption, OPTION_RETENTION_FULL, 1);
        $self->optionSetTest($oOption, OPTION_RETENTION_DIFF, 1);
        $self->optionSetTest($oOption, OPTION_RETENTION_ARCHIVE_TYPE, BACKUP_TYPE_FULL);
        $self->optionSetTest($oOption, OPTION_RETENTION_ARCHIVE, 1);
        $self->configLoadExpect(dclone($oOption), CMD_EXPIRE);

        $strDescription = 'Expiration cannot occur due to info file db mismatch';
        my $oExpire = new pgBackRest::Expire();

        # Mismatched version
        $oHostBackup->infoMunge(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
            {&INFO_ARCHIVE_SECTION_DB =>
                {&INFO_ARCHIVE_KEY_DB_VERSION => PG_VERSION_93, &INFO_ARCHIVE_KEY_DB_SYSTEM_ID => WAL_VERSION_95_SYS_ID},
             &INFO_ARCHIVE_SECTION_DB_HISTORY =>
                {'3' =>
                    {&INFO_ARCHIVE_KEY_DB_VERSION => PG_VERSION_93, &INFO_ARCHIVE_KEY_DB_ID => WAL_VERSION_95_SYS_ID}}});

        $self->testException(sub {$oExpire->process()},
            ERROR_FILE_INVALID,
            "archive and backup database versions do not match\n" .
            "HINT: has a stanza-upgrade been performed?");

        # Restore the info file
        $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE));

        # Mismatched system ID
        $oHostBackup->infoMunge(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
            {&INFO_ARCHIVE_SECTION_DB =>
                {&INFO_ARCHIVE_KEY_DB_SYSTEM_ID => 6999999999999999999},
             &INFO_ARCHIVE_SECTION_DB_HISTORY =>
                {'3' =>
                    {&INFO_ARCHIVE_KEY_DB_VERSION => PG_VERSION_95, &INFO_ARCHIVE_KEY_DB_ID => 6999999999999999999}}});

        $self->testException(sub {$oExpire->process()},
            ERROR_FILE_INVALID,
            "archive and backup database versions do not match\n" .
            "HINT: has a stanza-upgrade been performed?");

        # Restore the info file
        $oHostBackup->infoRestore(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE));
    }
}

1;
