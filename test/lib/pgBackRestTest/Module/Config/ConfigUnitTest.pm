####################################################################################################################################
# ConfigUnitTest.pm - Tests code paths
####################################################################################################################################
package pgBackRestTest::Module::Config::ConfigUnitTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::LibC qw(:config :configRule);

use pgBackRestBuild::Config::Data;

use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oConfig = {};
    my $strConfigFile = $self->testPath() . '/pgbackrest.conf';
    cfgCommandSet(CFGCMD_ARCHIVE_GET);
    cfgOptionSet(CFGOPT_CONFIG, $strConfigFile, true);

    if ($self->begin('Config::configFileValidate()'))
    {
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{&CFGBLDOPT_DB_PORT} = 1234;

        $self->testResult(sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, false,
            'valid option ' . &CFGBLDOPT_DB_PORT . ' under invalid section',
            {strLogExpect =>
                "WARN: $strConfigFile valid option '" . &CFGBLDOPT_DB_PORT . "' is a stanza section option and is not" .
                    " valid in section " . CFGDEF_SECTION_GLOBAL . "\n" .
                    "HINT: global options can be specified in global or stanza sections but not visa-versa"});

        #---------------------------------------------------------------------------------------------------------------------------
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . &CFGBLDCMD_BACKUP}{&CFGBLDOPT_DB_PORT} = 1234;

        $self->testResult(sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, false,
            'valid option ' . &CFGBLDOPT_DB_PORT . ' for command ' . &CFGBLDCMD_BACKUP . ' under invalid global section',
            {strLogExpect =>
                "WARN: $strConfigFile valid option '" . &CFGBLDOPT_DB_PORT . "' is a stanza section option and is not" .
                " valid in section " . CFGDEF_SECTION_GLOBAL . "\n" .
                "HINT: global options can be specified in global or stanza sections but not visa-versa"});

        #---------------------------------------------------------------------------------------------------------------------------
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . &CFGBLDCMD_ARCHIVE_PUSH}{&CFGBLDOPT_DB_PORT} = 1234;

        $self->testResult(sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, false,
            'valid option ' . &CFGBLDOPT_DB_PORT . ' under invalid stanza section command',
            {strLogExpect =>
                "WARN: $strConfigFile valid option '" . &CFGBLDOPT_DB_PORT . "' is not valid for command '" .
                &CFGBLDCMD_ARCHIVE_PUSH ."'"});

        #---------------------------------------------------------------------------------------------------------------------------
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{&BOGUS} = BOGUS;

        $self->testResult(
            sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, false,
            'invalid option ' . $$oConfig{&CFGDEF_SECTION_GLOBAL}{&BOGUS},
            {strLogExpect => "WARN: $strConfigFile file contains invalid option '" . BOGUS . "'"});

        #---------------------------------------------------------------------------------------------------------------------------
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{'thread-max'} = 3;

        $self->testResult(sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, true, 'valid alt name found');

        #---------------------------------------------------------------------------------------------------------------------------
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{&CFGBLDOPT_LOG_LEVEL_STDERR} =
            cfgOptionRuleDefault(CFGCMD_ARCHIVE_PUSH, CFGOPT_LOG_LEVEL_STDERR);
        $$oConfig{$self->stanza()}{&CFGBLDOPT_DB_PATH} = '/db';
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . &CFGBLDCMD_ARCHIVE_PUSH}{&CFGBLDOPT_PROCESS_MAX} = 2;

        $self->testResult(sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, true, 'valid config file');

        #---------------------------------------------------------------------------------------------------------------------------
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{&CFGBLDOPT_LOG_LEVEL_STDERR} =
            cfgOptionRuleDefault(CFGCMD_ARCHIVE_PUSH, CFGOPT_LOG_LEVEL_STDERR);
        $$oConfig{&CFGDEF_SECTION_GLOBAL . ':' . &CFGBLDCMD_ARCHIVE_PUSH}{&CFGBLDOPT_PROCESS_MAX} = 2;
        $$oConfig{'unusual-section^name!:' . &CFGBLDCMD_CHECK}{&CFGBLDOPT_DB_PATH} = '/db';

        $self->testResult(sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, true, 'valid unusual section name');

        #---------------------------------------------------------------------------------------------------------------------------
        $oConfig = {};
        $$oConfig{&CFGDEF_SECTION_GLOBAL}{&BOGUS} = BOGUS;

        # Change command to indicate remote
        cfgCommandSet(CFGCMD_REMOTE);

        $self->testResult(
            sub {pgBackRest::Config::Config::configFileValidate($oConfig)}, true,
            'invalid option but config file not validated on remote');
    }
}

1;
