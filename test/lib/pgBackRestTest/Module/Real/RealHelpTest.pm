####################################################################################################################################
# Tests for help command
####################################################################################################################################
package pgBackRestTest::Module::Real::RealHelpTest;
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

use pgBackRestTest::Env::Host::HostBaseTest;
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
        $self->helpExecute(cfgCommandName(CFGCMD_VERSION));
        $self->helpExecute(cfgCommandName(CFGCMD_HELP));
        $self->helpExecute(cfgCommandName(CFGCMD_HELP) . ' version');
        $self->helpExecute(cfgCommandName(CFGCMD_HELP) . ' --output=json --stanza=main --backup-host=backup info');
        $self->helpExecute(cfgCommandName(CFGCMD_HELP) . ' --output=json --stanza=main info output');
        $self->helpExecute(cfgCommandName(CFGCMD_HELP) . ' check');
    }
}

1;
