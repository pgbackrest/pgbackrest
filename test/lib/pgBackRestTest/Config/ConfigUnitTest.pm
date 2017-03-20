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

    my $oOption = {};
    my $oConfig = {};
    my $strConfigFile = $self->testPath() . '/pgbackrest.conf';

    if ($self->begin('invalid section ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . &CMD_BACKUP}{&OPTION_PROCESS_MAX} = 2;

        $self->testResult(sub {configFileValidate($oConfig)}, false, 'valid option under invalid section');
    }

    if ($self->begin('invalid section command ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{$self->stanza() . ':' . &CMD_ARCHIVE_PUSH}{&OPTION_DB_PORT} = 1234;

        $self->testResult(sub {configFileValidate($oConfig)}, false, 'valid option under invalid command section');
    }

    if ($self->begin('invalid option ' . OPTION_PROCESS_MAX))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{bogus} = 'bogus';

        $self->testResult(sub {configFileValidate($oConfig)}, false, 'invalid option');
    }

    if ($self->begin('valid config file'))
    {
        $oConfig = {};
        $$oConfig{&CONFIG_SECTION_GLOBAL}{&OPTION_LOG_LEVEL_STDERR} = OPTION_DEFAULT_LOG_LEVEL_STDERR;
        $$oConfig{$self->stanza()}{&OPTION_DB_PATH} = '/db';
        $$oConfig{&CONFIG_SECTION_GLOBAL . ':' . &CMD_ARCHIVE_PUSH}{&OPTION_PROCESS_MAX} = 2;

        $self->testResult(sub {configFileValidate($oConfig)}, true, 'valid config file');
    }

}

####################################################################################################################################
# Getters
####################################################################################################################################
# Change this from the default so the same stanza is not used in all tests.
sub stanza {return 'main'};

1;
