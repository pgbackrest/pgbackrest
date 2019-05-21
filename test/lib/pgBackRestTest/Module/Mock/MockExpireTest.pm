####################################################################################################################################
# Tests for expire command
####################################################################################################################################
package pgBackRestTest::Module::Mock::MockExpireTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Archive::Info;
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
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Env::ExpireEnvTest;
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Env::HostEnvTest;

####################################################################################################################################
# initStanzaOption
####################################################################################################################################
sub initStanzaOption
{
    my $self = shift;
    my $strDbBasePath = shift;
    my $strRepoPath = shift;
    my $oHostS3 = shift;

    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_PG_PATH, $strDbBasePath);
    $self->optionTestSet(CFGOPT_REPO_PATH, $strRepoPath);
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());

    $self->optionTestSetBool(CFGOPT_ONLINE, false);

    $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
    $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 6);

    if (defined($oHostS3))
    {
        $self->optionTestSet(CFGOPT_REPO_TYPE, CFGOPTVAL_REPO_TYPE_S3);
        $self->optionTestSet(CFGOPT_REPO_S3_KEY, HOST_S3_ACCESS_KEY);
        $self->optionTestSet(CFGOPT_REPO_S3_KEY_SECRET, HOST_S3_ACCESS_SECRET_KEY);
        $self->optionTestSet(CFGOPT_REPO_S3_BUCKET, HOST_S3_BUCKET);
        $self->optionTestSet(CFGOPT_REPO_S3_ENDPOINT, HOST_S3_ENDPOINT);
        $self->optionTestSet(CFGOPT_REPO_S3_REGION, HOST_S3_REGION);
        $self->optionTestSet(CFGOPT_REPO_S3_HOST, $oHostS3->ipGet());
        $self->optionTestSetBool(CFGOPT_REPO_S3_VERIFY_TLS, false);
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

    foreach my $rhRun
    (
        {vm => VM1, s3 =>  true, encrypt => false},
        {vm => VM2, s3 => false, encrypt =>  true},
        {vm => VM3, s3 => false, encrypt => false},
        {vm => VM4, s3 =>  true, encrypt =>  true},
    )
    {
        # Only run tests for this vm
        next if ($rhRun->{vm} ne $self->vm());

        # Increment the run, log, and decide whether this unit test should be run
        my $bS3 = $rhRun->{s3};
        my $bEncrypt = $rhRun->{encrypt};

        if ($bS3 && ($self->vm() eq VM3))
        {
            confess &log("cannot configure s3 for expect log tests");
        }

        ############################################################################################################################
        if ($self->begin("simple, enc ${bEncrypt}, s3 ${bS3}"))
        {
            # Create hosts, file object, and config
            my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3) = $self->setup(
                true, $self->expect(), {bS3 => $bS3, bRepoEncrypt => $bEncrypt});

            $self->initStanzaOption($oHostDbMaster->dbBasePath(), $oHostBackup->{strRepoPath}, $oHostS3);
            $self->configTestLoad(CFGCMD_STANZA_CREATE);

            # Create the test object
            my $oExpireTest = new pgBackRestTest::Env::ExpireEnvTest(
                $oHostBackup, $self->backrestExe(), storageRepo(), $self->expect(), $self);

            $oExpireTest->stanzaCreate($self->stanza(), PG_VERSION_92);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Nothing to expire';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, 246);

            $oExpireTest->process($self->stanza(), 1, 1, CFGOPTVAL_BACKUP_TYPE_FULL, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest full backup, archive expire falls on segment major boundary';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 1, 1, CFGOPTVAL_BACKUP_TYPE_FULL, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest full backup';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, 256);
            $oExpireTest->process($self->stanza(), 1, 1, CFGOPTVAL_BACKUP_TYPE_FULL, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest diff backup, archive expire does not fall on major segment boundary';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, undef, 0);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, undef, 0);
            $oExpireTest->process($self->stanza(), 1, 1, CFGOPTVAL_BACKUP_TYPE_DIFF, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire oldest diff backup (cascade to incr)';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 1, 1, CFGOPTVAL_BACKUP_TYPE_DIFF, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire archive based on newest incr backup';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 1, 1, CFGOPTVAL_BACKUP_TYPE_INCR, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire diff treating full as diff';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 2, 1, CFGOPTVAL_BACKUP_TYPE_DIFF, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire diff with repo-retention-archive with warning repo-retention-diff not set';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), undef, undef, CFGOPTVAL_BACKUP_TYPE_DIFF, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire full with repo-retention-archive with warning repo-retention-full not set';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), undef, undef, CFGOPTVAL_BACKUP_TYPE_FULL, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire no archive with warning since repo-retention-archive not set for INCR';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 1, 1, CFGOPTVAL_BACKUP_TYPE_INCR, undef, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire no archive with warning since neither repo-retention-archive nor repo-retention-diff is set';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), undef, undef, CFGOPTVAL_BACKUP_TYPE_DIFF, undef, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Use oldest full backup for archive retention';
            $oExpireTest->process($self->stanza(), 10, 10, CFGOPTVAL_BACKUP_TYPE_FULL, 10, $strDescription);
        }

        ############################################################################################################################
        if ($self->begin("stanzaUpgrade, enc ${bEncrypt}, s3 ${bS3}"))
        {
            # Create hosts, file object, and config
            my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oHostS3) = $self->setup(
                true, $self->expect(), {bS3 => $bS3, bRepoEncrypt => $bEncrypt});

            $self->initStanzaOption($oHostDbMaster->dbBasePath(), $oHostBackup->{strRepoPath}, $oHostS3);
            $self->configTestLoad(CFGCMD_STANZA_CREATE);

            # Create the test object
            my $oExpireTest = new pgBackRestTest::Env::ExpireEnvTest(
                $oHostBackup, $self->backrestExe(), storageRepo(), $self->expect(), $self);

            $oExpireTest->stanzaCreate($self->stanza(), PG_VERSION_92);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Create backups in current db version';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), undef, undef, CFGOPTVAL_BACKUP_TYPE_DIFF, undef, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Upgrade stanza and expire only earliest db backup and archive';

            $oExpireTest->stanzaUpgrade($self->stanza(), PG_VERSION_93);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, 246);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 3, undef, CFGOPTVAL_BACKUP_TYPE_FULL, undef, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Upgrade the stanza, create full back - earliest db orphaned archive removed and earliest full backup ' .
                'and archive in previous db version removed';

            $oExpireTest->stanzaUpgrade($self->stanza(), PG_VERSION_10);
            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 2, undef, CFGOPTVAL_BACKUP_TYPE_FULL, undef, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire all archive last full backup through pitr';

            $oExpireTest->backupCreate($self->stanza(), CFGOPTVAL_BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY);
            $oExpireTest->process($self->stanza(), 3, 1, CFGOPTVAL_BACKUP_TYPE_DIFF, 1, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $strDescription = 'Expire all archive except for the current database';

            $oExpireTest->process($self->stanza(), 2, undef, CFGOPTVAL_BACKUP_TYPE_FULL, undef, $strDescription);

            #-----------------------------------------------------------------------------------------------------------------------
            $self->optionTestClear(CFGOPT_DB_TIMEOUT);
            $self->optionTestClear(CFGOPT_PG_PATH);
            $self->optionTestClear(CFGOPT_ONLINE);
            $self->optionTestClear(CFGOPT_PROTOCOL_TIMEOUT);
            $self->optionTestSet(CFGOPT_REPO_RETENTION_FULL, 1);
            $self->optionTestSet(CFGOPT_REPO_RETENTION_DIFF, 1);
            $self->optionTestSet(CFGOPT_REPO_RETENTION_ARCHIVE_TYPE, CFGOPTVAL_BACKUP_TYPE_FULL);
            $self->optionTestSet(CFGOPT_REPO_RETENTION_ARCHIVE, 1);
            $self->configTestLoad(CFGCMD_EXPIRE);

            $strDescription = 'Expiration cannot occur due to info file db mismatch';
            my $oExpire = new pgBackRest::Expire();

            # Mismatched version
            $oHostBackup->infoMunge(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . qw{/} . ARCHIVE_INFO_FILE),
                {&INFO_ARCHIVE_SECTION_DB =>
                    {&INFO_ARCHIVE_KEY_DB_VERSION => PG_VERSION_93,
                        &INFO_ARCHIVE_KEY_DB_SYSTEM_ID => $self->dbSysId(PG_VERSION_95)},
                 &INFO_ARCHIVE_SECTION_DB_HISTORY =>
                    {'3' =>
                        {&INFO_ARCHIVE_KEY_DB_VERSION => PG_VERSION_93,
                            &INFO_ARCHIVE_KEY_DB_ID => $self->dbSysId(PG_VERSION_95)}}});

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
}

1;
