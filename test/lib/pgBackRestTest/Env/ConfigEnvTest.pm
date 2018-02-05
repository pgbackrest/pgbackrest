####################################################################################################################################
# ConfigCommonTest.pm - Common code for Config unit tests
####################################################################################################################################
package pgBackRestTest::Env::ConfigEnvTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Getopt::Long qw(GetOptions);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::LibC qw(:test);
use pgBackRest::Version;

use constant CONFIGENVTEST                                          => 'ConfigEnvTest';

sub optionTestSet
{
    my $self = shift;
    my $iOptionId = shift;
    my $strValue = shift;

    $self->{&CONFIGENVTEST}{option}{cfgOptionName($iOptionId)} = $strValue;
}

sub optionTestSetBool
{
    my $self = shift;
    my $iOptionId = shift;
    my $bValue = shift;

    $self->{&CONFIGENVTEST}{boolean}{cfgOptionName($iOptionId)} = defined($bValue) ? $bValue : true;
}

sub optionTestClear
{
    my $self = shift;
    my $iOptionId = shift;

    delete($self->{&CONFIGENVTEST}{option}{cfgOptionName($iOptionId)});
    delete($self->{&CONFIGENVTEST}{boolean}{cfgOptionName($iOptionId)});
}

sub configTestClear
{
    my $self = shift;

    my $rhConfig = $self->{&CONFIGENVTEST};

    delete($self->{&CONFIGENVTEST});

    return $rhConfig;
}

sub configTestSet
{
    my $self = shift;
    my $rhConfig = shift;

    $self->{&CONFIGENVTEST} = $rhConfig;
}

sub commandTestWrite
{
    my $self = shift;
    my $strCommand = shift;
    my $rhConfig = shift;

    my @szyParam = ();

    if (defined($rhConfig->{boolean}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{boolean}})))
        {
            if ($rhConfig->{boolean}{$strOption})
            {
                push(@szyParam, "--${strOption}");
            }
            else
            {
                push(@szyParam, "--no-${strOption}");
            }
        }
    }

    if (defined($rhConfig->{option}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{option}})))
        {
            push(@szyParam, "--${strOption}=$rhConfig->{option}{$strOption}");
        }
    }

    push(@szyParam, $strCommand);

    return @szyParam;
}

####################################################################################################################################
# Load the configuration
####################################################################################################################################
sub configTestLoad
{
    my $self = shift;
    my $iCommandId = shift;

    my @stryArg = $self->commandTestWrite(cfgCommandName($iCommandId), $self->{&CONFIGENVTEST});
    my $strConfigJson = cfgParseTest(backrestBin(), join('|', @stryArg));
    $self->testResult(
        sub {configLoad(false, backrestBin(), cfgCommandName($iCommandId), $strConfigJson)},
        true, 'config load: ' . join(" ", @stryArg));
}

1;
