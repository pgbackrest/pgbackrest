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

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Ini;
use pgBackRestDoc::Common::Log;

use pgBackRestTest::Env::ArchiveInfo;
use pgBackRestTest::Env::BackupInfo;
use pgBackRestTest::Env::ExpireEnvTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::Host::HostS3Test;
use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Env::Manifest;
use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::StorageRepo;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Wait;

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
        {vm => VM1, storage => POSIX, encrypt => false},
        {vm => VM2, storage =>    S3, encrypt =>  true},
        {vm => VM3, storage => POSIX, encrypt => false},
        {vm => VM4, storage => AZURE, encrypt =>  true},
    )
    {
        # Only run tests for this vm
        next if ($rhRun->{vm} ne vmTest($self->vm()));

        # Increment the run, log, and decide whether this unit test should be run
        my $strStorage = $rhRun->{storage};
        my $bEncrypt = $rhRun->{encrypt};

        if ($strStorage ne POSIX && ($self->vm() eq VM3))
        {
            confess &log("cannot configure non-posix storage for expect log tests");
        }

        ############################################################################################################################
        if ($self->begin("simple, enc ${bEncrypt}, storage ${strStorage}"))
        {
            # Create hosts, file object, and config
            my ($oHostDbPrimary, $oHostDbStandby, $oHostBackup) = $self->setup(
                true, $self->expect(), {strStorage => $strStorage, bRepoEncrypt => $bEncrypt});

            # Create the test object
            my $oExpireTest = new pgBackRestTest::Env::ExpireEnvTest(
                $oHostBackup, $self->backrestExe(), storageRepo(), $oHostDbPrimary->dbPath(), $self->expect(), $self);

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
        }

        ############################################################################################################################
        if ($self->begin("stanzaUpgrade, enc ${bEncrypt}, storage ${strStorage}"))
        {
            # Create hosts, file object, and config
            my ($oHostDbPrimary, $oHostDbStandby, $oHostBackup) = $self->setup(
                true, $self->expect(), {strStorage => $strStorage, bRepoEncrypt => $bEncrypt});

            # Create the test object
            my $oExpireTest = new pgBackRestTest::Env::ExpireEnvTest(
                $oHostBackup, $self->backrestExe(), storageRepo(), $oHostDbPrimary->dbPath(), $self->expect(), $self);

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
        }
    }
}

1;
