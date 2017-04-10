####################################################################################################################################
# ConfigUnitTest.pm - Tests code paths
####################################################################################################################################
package pgBackRestTest::Config::ConfigUnitTest;
use parent 'pgBackRestTest::Config::ConfigEnvTest';

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

use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oConfig = {};
    my $strConfigFile = $self->testPath() . '/pgbackrest.conf';
    optionSet(OPTION_CONFIG, $strConfigFile, true);

    if ($self->begin('valid option ' . OPTION_DB_PORT . ' under invalid section'))
    {
        my $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_DB_PORT} = 1234;

        $self->testResult(sub {configFileValidate($oConfig)}, false,
            'valid option ' . OPTION_DB_PORT . ' under invalid section',
            {strLogExpect =>
                "WARN: $strConfigFile valid option '" . OPTION_DB_PORT . "' is a stanza section option and is not valid in" .
                " section " . CONFIG_SECTION_GLOBAL . "\nHINT: global options can be specified in global or stanza sections but" .
                " not visa-versa"});
    }

    if ($self->begin('valid option ' . OPTION_DB_PORT . ' for command ' . CMD_BACKUP . ' under invalid global section'))
    {
        my $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_BACKUP}{&OPTION_DB_PORT} = 1234;

        $self->testResult(sub {configFileValidate($oConfig)}, false,
            'valid option ' . OPTION_DB_PORT . ' for command ' . CMD_BACKUP . ' under invalid global section',
            {strLogExpect =>
                "WARN: $strConfigFile valid option '" . OPTION_DB_PORT . "' is a stanza section option and is not valid in" .
                " section " . CONFIG_SECTION_GLOBAL ."\nHINT: global options can be specified in global or stanza sections but" .
                " not visa-versa"});
    }

    if ($self->begin('valid option ' . OPTION_DB_PORT . ' under invalid stanza section command'))
    {
        my $oConfig = {};
        $$oConfig{$self->stanza() . ':' . &CMD_ARCHIVE_PUSH}{&OPTION_DB_PORT} = 1234;

        $self->testResult(sub {configFileValidate($oConfig)}, false,
            'valid option ' . OPTION_DB_PORT . ' under invalid stanza section command',
            {strLogExpect =>
                "WARN: $strConfigFile valid option '" . OPTION_DB_PORT . "' is not valid for command '" . CMD_ARCHIVE_PUSH ."'"});
    }

    if ($self->begin('invalid option'))
    {
        my $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&BOGUS} = BOGUS;

        $self->testResult(sub {configFileValidate($oConfig)}, false, 'invalid option ' . $$oConfig{&CONFIG_SECTION_GLOBAL}{&BOGUS},
            {strLogExpect => "WARN: $strConfigFile file contains invalid option '" . BOGUS . "'"});
    }

    if ($self->begin('valid alt name'))
    {
        my $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{'thread-max'} = 3;

        $self->testResult(sub {configFileValidate($oConfig)}, true, 'valid alt name found');
    }

    if ($self->begin('valid config file'))
    {
        my $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_STDERR} = OPTION_DEFAULT_LOG_LEVEL_STDERR;
        $$oConfig{$self->stanza()}{&OPTION_DB_PATH} = '/db';
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_ARCHIVE_PUSH}{&OPTION_PROCESS_MAX} = 2;

        $self->testResult(sub {configFileValidate($oConfig)}, true, 'valid config file');
    }

    if ($self->begin('valid unusual section name'))
    {
        my $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_STDERR} = OPTION_DEFAULT_LOG_LEVEL_STDERR;
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_ARCHIVE_PUSH}{&OPTION_PROCESS_MAX} = 2;
        $$oConfig{'unusual-section^name!:' . &CMD_CHECK}{&OPTION_DB_PATH} = '/db';

        $self->testResult(sub {configFileValidate($oConfig)}, true, 'valid unusual section name');
    }
}

1;
