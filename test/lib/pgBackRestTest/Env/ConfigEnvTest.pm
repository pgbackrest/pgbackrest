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

use pgBackRestTest::Common::RunTest;

use constant CONFIGENVTEST                                          => 'ConfigEnvTest';

####################################################################################################################################
# Is the option secure?
####################################################################################################################################
sub optionTestSecure
{
    my $self = shift;
    my $strOption = shift;

    return (cfgDefOptionSecure(cfgOptionId($strOption)) ? true : false);
}

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

####################################################################################################################################
# Write all secure options to a config file
####################################################################################################################################
sub configFileWrite
{
    my $self = shift;
    my $strConfigFile = shift;
    my $rhConfig = shift;

    my $strConfig = "[global]\n";

    if (defined($rhConfig->{boolean}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{boolean}})))
        {
            if ($self->optionTestSecure($strOption))
            {
                $strConfig .= "${strOption}=" . ($rhConfig->{boolean}{$strOption} ? 'y' : 'n') . "\n";
            }
        }
    }

    if (defined($rhConfig->{option}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{option}})))
        {
            if ($self->optionTestSecure($strOption))
            {
                $strConfig .= "${strOption}=$rhConfig->{option}{$strOption}\n";
            }
        }
    }

    storageTest()->put($strConfigFile, $strConfig);
}

####################################################################################################################################
# Write all non-secure options to the command line
####################################################################################################################################
sub commandTestWrite
{
    my $self = shift;
    my $strCommand = shift;
    my $strConfigFile = shift;
    my $rhConfig = shift;

    my @szyParam = ();

    # Add boolean options
    if (defined($rhConfig->{boolean}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{boolean}})))
        {
            if (!$self->optionTestSecure($strOption))
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
    }

    # Add non-boolean options
    if (defined($rhConfig->{option}))
    {
        foreach my $strOption (sort(keys(%{$rhConfig->{option}})))
        {
            if (!$self->optionTestSecure($strOption))
            {
                push(@szyParam, "--${strOption}=$rhConfig->{option}{$strOption}");
            }
        }
    }

    # Add config file
    push(@szyParam, '--' . cfgOptionName(CFGOPT_CONFIG) . "=${strConfigFile}");

    # Add command
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

    # A config file is required to store secure options before they can be parsed
    my $strConfigFile = $self->testPath() . '/pgbackrest.test.conf';
    $self->configFileWrite($strConfigFile, $self->{&CONFIGENVTEST});

    my @stryArg = $self->commandTestWrite(cfgCommandName($iCommandId), $strConfigFile, $self->{&CONFIGENVTEST});
    my $strConfigJson = cfgParseTest(projectBin(), join('|', @stryArg));
    $self->testResult(
        sub {configLoad(false, projectBin(), cfgCommandName($iCommandId), \$strConfigJson)},
        true, 'config load: ' . join(" ", @stryArg));
}

1;
