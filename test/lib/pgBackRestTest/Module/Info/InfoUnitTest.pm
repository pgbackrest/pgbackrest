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

use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Info;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::ExpireEnvTest;
use pgBackRestTest::Common::FileTest;
# use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Env::HostEnvTest;

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
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create parent path for pg_control
    storageTest()->pathCreate(($self->{strDbPath} . '/' . DB_PATH_GLOBAL), {bCreateParent => true});

    # Create archive info path
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});

    # Create backup info path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});
}

####################################################################################################################################
# initStanzaCreate - initialize options and create the stanza object
####################################################################################################################################
sub initStanzaCreate
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

    # Create the test object
    $self->{oExpireTest} = new pgBackRestTest::Env::ExpireEnvTest(undef, $self->backrestExe(), storageRepo(), undef, $self);

    $self->{oExpireTest}->stanzaCreate($self->stanza(), PG_VERSION_94);
}

####################################################################################################################################
# initStanzaUpgrade - initialize options and create the stanza object
####################################################################################################################################
sub initStanzaUpgrade
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

    $self->configLoadExpect(dclone($oOption), CMD_STANZA_UPGRADE);

    # Create the test object
    $self->{oExpireTest} = new pgBackRestTest::Env::ExpireEnvTest(undef, $self->backrestExe(), storageRepo(), undef, $self);

    $self->{oExpireTest}->stanzaUpgrade($self->stanza(), PG_VERSION_95);
}


####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oOption = {};

    # Used to create backups and WAL to test
    use constant SECONDS_PER_DAY => 86400;
    my $lBaseTime = 1486137448 - (60 * SECONDS_PER_DAY);

    ################################################################################################################################
    if ($self->begin("Info"))
    {
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_INFO); logEnable();

        my $oInfo = new pgBackRest::Info();

        # Output No stanzas exist in default text option
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oInfo->process()}, 0, 'No stanzas exist and default text option');

        # Invalid option
        #---------------------------------------------------------------------------------------------------------------------------
        optionSet(OPTION_OUTPUT, BOGUS);

        $self->testException(sub {$oInfo->process()}, ERROR_ASSERT, "invalid info output option '" . BOGUS . "'");

        # Output json option with no stanza defined
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionSetTest($oOption, OPTION_OUTPUT, INFO_OUTPUT_JSON);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_INFO); logEnable();

        $self->testResult(sub {$oInfo->process()}, 0, 'json option');

        # Add linefeed to JSON
        #---------------------------------------------------------------------------------------------------------------------------
        my $strJson = '[{"archive" : 1}]';
        $self->testResult(sub {$oInfo->outputJSON($strJson)}, 1, 'add linefeed to json');

        # Missing stanza path
        #---------------------------------------------------------------------------------------------------------------------------
        my $hyStanza = $oInfo->stanzaList(BOGUS);

        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: bogus\n    status: error (missing stanza path)\n",
            'missing stanza path');

        # Create more than one stanza but no data
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionSetTest($oOption, OPTION_REPO_PATH, $self->{strRepoPath});
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_INFO); logEnable();

        # Create archive info paths but no archive info
        storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});
        storageTest()->pathCreate("$self->{strRepoPath}/archive/" . BOGUS, {bIgnoreExists => true, bCreateParent => true});

        # Create backup info paths but no backup info
        storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});
        storageTest()->pathCreate("$self->{strRepoPath}/backup/" . BOGUS, {bIgnoreExists => true, bCreateParent => true});

        # Get a list of all stanzas in the repo
        $hyStanza = $oInfo->stanzaList();

        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: bogus\n    status: error (no valid backups)\n\n" .
            "stanza: db\n    status: error (no valid backups)\n",
            'fomatText() multiple stanzas');

        # Define the stanza option
        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
        $self->optionReset($oOption, OPTION_OUTPUT);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_INFO); logEnable();

        $self->testResult(sub {$oInfo->process()}, 0, 'stanza set');

        # Create the stanza - no WAL or backups
        #---------------------------------------------------------------------------------------------------------------------------
        $self->initStanzaCreate();
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_INFO); logEnable();

        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: error (no valid backups)\n\n" .
            "    db (current)\n        wal archive min/max (9.4-1): none present\n",
            "formatText() one stanza");

        # Create a backup and list backup for just one stanza
        #---------------------------------------------------------------------------------------------------------------------------
        $self->{oExpireTest}->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, -1, -1);

        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: ok\n\n    db (current)\n        wal archive min/max (9.4-1): none present\n\n" .
            "        full backup: 20161206-155728F\n" .
            "            timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "            wal start/stop: n/a\n            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() one stanza");

        # Coverage for major WAL paths with no WAL
        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($self->{strArchivePath} . "/9.4-1/0000000100000000", {bIgnoreExists => true});
        storageTest()->pathCreate($self->{strArchivePath} . "/9.4-1/0000000200000000", {bIgnoreExists => true});
        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: ok\n\n    db (current)\n        wal archive min/max (9.4-1): none present\n\n" .
            "        full backup: 20161206-155728F\n" .
            "            timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "            wal start/stop: n/a\n            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() major WAL paths with no WAL");

        # Upgrade postgres version and backup with WAL
        #---------------------------------------------------------------------------------------------------------------------------
        undef($self->{oExpireTest});
        $self->initStanzaUpgrade();
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_INFO); logEnable();

        $self->{oExpireTest}->backupCreate($self->stanza(), BACKUP_TYPE_FULL, $lBaseTime += SECONDS_PER_DAY, 1, 1);
        $self->{oExpireTest}->backupCreate($self->stanza(), BACKUP_TYPE_DIFF, $lBaseTime += SECONDS_PER_DAY, 1, 1);

        # Remove the 9.4-1 path for condition test coverage
        storageTest()->remove($self->{strArchivePath} . "/9.4-1/", {bRecurse => true});

        $hyStanza = $oInfo->stanzaList($self->stanza());
        $self->testResult(sub {$oInfo->formatText($hyStanza)},
            "stanza: db\n    status: ok\n" .
            "\n    db (current)\n" .
            "        wal archive min/max (9.5-2): 000000010000000000000000 / 000000010000000000000003\n\n" .
            "        full backup: 20161207-155728F\n" .
            "            timestamp start/stop: 2016-12-07 15:57:28 / 2016-12-07 15:57:28\n" .
            "            wal start/stop: 000000010000000000000000 / 000000010000000000000000\n" .
            "            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n\n" .
            "        diff backup: 20161207-155728F_20161208-155728D\n" .
            "            timestamp start/stop: 2016-12-08 15:57:28 / 2016-12-08 15:57:28\n" .
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000002\n" .
            "            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n" .
            "\n    db (prior)\n" .
            "        full backup: 20161206-155728F\n" .
            "            timestamp start/stop: 2016-12-06 15:57:28 / 2016-12-06 15:57:28\n" .
            "            wal start/stop: n/a\n" .
            "            database size: 0B, backup size: 0B\n" .
            "            repository size: 0B, repository backup size: 0B\n",
            "formatText() multiple DB versions");
    }
}

1;
