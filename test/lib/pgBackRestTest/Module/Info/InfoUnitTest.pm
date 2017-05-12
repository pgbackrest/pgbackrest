####################################################################################################################################
# InfoUnitTest.pm - Unit tests for Info module
####################################################################################################################################
package pgBackRestTest::Module::Info::InfoUnitTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::BackupInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Info;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::ExpireEnvTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
    $self->{strDbPath} = $self->testPath() . '/db';

    # Create the local file object
    $self->{oFile} =
        new pgBackRest::File
        (
            $self->stanza(),
            $self->{strRepoPath},
            new pgBackRest::Protocol::Common
            (
                OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
                OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
                OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
                HOST_PROTOCOL_TIMEOUT                       # Protocol timeout
            )
        );
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create parent path for pg_control
    filePathCreate(($self->{strDbPath} . '/' . DB_PATH_GLOBAL), undef, false, true);

    # Create archive info path
    filePathCreate($self->{strArchivePath}, undef, true, true);

    # Create backup info path
    filePathCreate($self->{strBackupPath}, undef, true, true);

    # Create the test object
    $self->{oExpireTest} = new pgBackRestTest::Env::ExpireEnvTest(undef, $self->backrestExe(), $self->{oFile}, undef, $self);

    # Set options for stanzaCreate
    $self->optionStanzaCreate();

    $self->{oExpireTest}->stanzaCreate($self->stanza(), PG_VERSION_94);
}

sub optionStanzaCreate
{
    my $self = shift;

    # Set options for stanzaCreate
    my $oOption = {};
    $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->optionSetTest($oOption, OPTION_DB_PATH, $self->{strDbPath});
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $self->{strRepoPath});
    $self->optionSetTest($oOption, OPTION_LOG_PATH, $self->testPath());
    $self->optionBoolSetTest($oOption, OPTION_ONLINE, false);
    $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
    $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 6);

    $self->configLoadExpect(dclone($oOption), CMD_STANZA_CREATE);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oOption = {};

    # $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $self->{strRepoPath});

    # Used to create backups and WAL to test
    use constant SECONDS_PER_DAY => 86400;
    my $lBaseTime = 1486137448 - (60 * SECONDS_PER_DAY);

    ################################################################################################################################
    if ($self->begin("Info->formatTextStanza() && Info->formatTextBackup()"))
    {
        $self->configLoadExpect(dclone($oOption), CMD_INFO);

        my $oInfo = new pgBackRest::Info();

        #---------------------------------------------------------------------------------------------------------------------------
        my $hyStanza = $oInfo->stanzaList($self->{oFile}, $self->stanza());
        $self->testResult(sub {$oInfo->formatTextStanza(@{$hyStanza}[0])},
            "stanza: db\n    status: error (no valid backups)\n    wal archive min/max: none present", "stanza text output");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->{oExpireTest}->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, -1, -1);
        $hyStanza = $oInfo->stanzaList($self->{oFile}, $self->stanza());

        $self->testResult(sub {$oInfo->formatTextStanza(@{$hyStanza}[-1])},
            "stanza: db\n    status: ok\n    wal archive min/max: none present",
            "stanza text output");

        $self->testResult(sub {$oInfo->formatTextBackup(@{$hyStanza}[-1]->{&INFO_BACKUP_SECTION_BACKUP}[-1])},
            "    full backup: 20161206-155728F\n" .
            "        timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "        wal start/stop: n/a\n" .
            "        database size: 0B, backup size: 0B\n" .
            "        repository size: 0B, repository backup size: 0B",
            "full backup text output");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->{oExpireTest}->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY);
        $hyStanza = $oInfo->stanzaList($self->{oFile}, $self->stanza());

        $self->testResult(sub {$oInfo->formatTextStanza(@{$hyStanza}[-1])},
            "stanza: db\n    status: ok\n    wal archive min/max: 000000010000000000000000 / 000000010000000000000005",
            "stanza text output");

        $self->testResult(sub {$oInfo->formatTextBackup(@{$hyStanza}[-1]->{&INFO_BACKUP_SECTION_BACKUP}[-1])},
            "    diff backup: 20161206-155728F_20161207-155728D\n" .
            "        timestamp start/stop: 2016-12-07 15:57:28 / 2016-12-07 15:57:28\n" .
            "        wal start/stop: 000000010000000000000000 / 000000010000000000000002\n" .
            "        database size: 0B, backup size: 0B\n" .
            "        repository size: 0B, repository backup size: 0B",
            "diff backup text output");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->{oExpireTest}->backupCreate($self->stanza(), BACKUP_TYPE_INCR, $lBaseTime += SECONDS_PER_DAY, 256);
        $hyStanza = $oInfo->stanzaList($self->{oFile}, $self->stanza());

        $self->testResult(sub {$oInfo->formatTextStanza(@{$hyStanza}[-1])},
            "stanza: db\n    status: ok\n    wal archive min/max: 000000010000000000000000 / 000000010000000100000008",
            "stanza text output");

        $self->testResult(sub {$oInfo->formatTextBackup(@{$hyStanza}[-1]->{&INFO_BACKUP_SECTION_BACKUP}[-1])},
            "    incr backup: 20161206-155728F_20161208-155728I\n" .
            "        timestamp start/stop: 2016-12-08 15:57:28 / 2016-12-08 15:57:28\n" .
            "        wal start/stop: 000000010000000000000006 / 000000010000000100000005\n" .
            "        database size: 0B, backup size: 0B\n" .
            "        repository size: 0B, repository backup size: 0B",
            "incr backup text output");

        # Upgrade the stanza, generate archive and confirm the min/max archive is for the new (current) DB version
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionStanzaCreate();
        $self->{oExpireTest}->stanzaUpgrade($self->stanza(), PG_VERSION_95);
        $self->{oExpireTest}->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, 2);
        $hyStanza = $oInfo->stanzaList($self->{oFile}, $self->stanza());

        $self->testResult(sub {$oInfo->formatTextStanza(@{$hyStanza}[-1])},
            "stanza: db\n    status: ok\n    wal archive min/max: 000000010000000000000000 / 000000010000000000000004",
            "stanza text output");
    }
}

1;
