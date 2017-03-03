####################################################################################################################################
# ExpireExpireTest.pm - Tests for expire command
####################################################################################################################################
package pgBackRestTest::Expire::ExpireExpireTest;
use parent 'pgBackRestTest::Common::Env::EnvHostTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::BackupInfo;
use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;

use pgBackRestTest::Common::Env::EnvHostTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Expire::ExpireEnvTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    if ($self->begin("local"))
    {


        # Create hosts, file object, and config
        my ($oHostDbMaster, $oHostDbStandby, $oHostBackup, $oFile) = $self->setup(true, $self->expect());

        # Set up the configuration needed for stanza object
        my $oOption = {};

        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_DB_PATH, $oHostDbMaster->dbBasePath());
        $self->optionSetTest($oOption, OPTION_REPO_PATH, $oHostBackup->{strRepoPath});
        $self->optionSetTest($oOption, OPTION_LOG_PATH, $self->testPath());

        $self->optionBoolSetTest($oOption, OPTION_ONLINE, false);

        $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
        $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 6);

        $self->configLoadExpect(dclone($oOption), CMD_STANZA_CREATE);

        # Create the test object
        my $oExpireTest = new pgBackRestTest::Expire::ExpireEnvTest($oHostBackup, $self->backrestExe(), $oFile, $self->expect(),
            $self);

        $oExpireTest->stanzaCreate($self->stanza(), PG_VERSION_92);
        use constant SECONDS_PER_DAY => 86400;
        my $lBaseTime = time() - (SECONDS_PER_DAY * 56);

        #-----------------------------------------------------------------------------------------------------------------------
        my $strDescription = 'Nothing to expire';

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
    }
}

1;
