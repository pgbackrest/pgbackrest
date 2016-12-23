####################################################################################################################################
# HelpHelpTest.pm - Tests for help command
####################################################################################################################################
package pgBackRestTest::Help::HelpHelpTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;

use pgBackRestTest::Common::Host::HostBaseTest;
use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# helpExecute
####################################################################################################################################
sub helpExecute
{
    my $self = shift;
    my $strCommand = shift;

    executeTest($self->backrestExe() . " $strCommand", {oLogTest => $self->expect()});
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Increment the run, log, and decide whether this unit test should be run
    if ($self->begin('base'))
    {
        $self->helpExecute(CMD_VERSION);
        $self->helpExecute(CMD_HELP);
        $self->helpExecute(CMD_HELP . ' version');
        $self->helpExecute(CMD_HELP . ' --output=json --stanza=main info');
        $self->helpExecute(CMD_HELP . ' --output=json --stanza=main info output');
    }
}

1;
