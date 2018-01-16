####################################################################################################################################
# CommonLogTest.pm - Unit tests for Log module
####################################################################################################################################
package pgBackRestTest::Module::Common::CommonLogPerlTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin("log()"))
    {
        logWarnOnErrorEnable();
        $self->testResult(sub {&log(ERROR, "my test log", 27)}, "[object]", 'log error as warning',
            {strLogExpect => "WARN: [027]: my test log"});

        $self->testResult(sub {&log(INFO, "my test log")}, "my test log", 'log info as info',
            {strLogLevel => INFO, strLogExpect => "INFO: my test log"});

        logWarnOnErrorDisable();
        $self->testResult(sub {&log(ERROR, "my test log", 27)}, "[object]", 'log error',
            {strLogExpect => "ERROR: [027]: my test log"});
    }
}

1;
