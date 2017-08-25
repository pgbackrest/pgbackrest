####################################################################################################################################
# Legacy Rules Used Primarily by Documentation
#
# Most of these can be removed, but it will required some refactoring on DocConfig.pm.
####################################################################################################################################
package pgBackRestBuild::Config::Rule;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Data;

####################################################################################################################################
# Option rules hash
####################################################################################################################################
my $rhOptionRule = cfgdefRule();

####################################################################################################################################
# cfgbldCommandRule - returns the option rules based on the command.
####################################################################################################################################
sub cfgbldCommandRule
{
    my $strOption = shift;
    my $strCommand = shift;

    if (defined($strCommand))
    {
        return defined($rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_COMMAND}) &&
               defined($rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand}) &&
               ref($rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand}) eq 'HASH' ?
               $rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand} : undef;
    }

    return;
}

####################################################################################################################################
# cfgbldOptionDefault - does the option have a default for this command?
####################################################################################################################################
sub cfgbldOptionDefault
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command rule
    my $oCommandRule = cfgbldCommandRule($strOption, $strCommand);

    # Check for default in command
    my $strDefault = defined($oCommandRule) ? $$oCommandRule{&CFGBLDDEF_RULE_DEFAULT} : undef;

    # If defined return, else try to grab the global default
    return defined($strDefault) ? $strDefault : $rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_DEFAULT};
}

push @EXPORT, qw(cfgbldOptionDefault);

####################################################################################################################################
# cfgbldOptionRange - get the allowed setting range for the option if it exists
####################################################################################################################################
sub cfgbldOptionRange
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command rule
    my $oCommandRule = cfgbldCommandRule($strOption, $strCommand);

    # Check for default in command
    if (defined($oCommandRule) && defined($$oCommandRule{&CFGBLDDEF_RULE_ALLOW_RANGE}))
    {
        return $$oCommandRule{&CFGBLDDEF_RULE_ALLOW_RANGE}[0], $$oCommandRule{&CFGBLDDEF_RULE_ALLOW_RANGE}[1];
    }

    # If defined return, else try to grab the global default
    return $rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_ALLOW_RANGE}[0], $rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_ALLOW_RANGE}[1];
}

push @EXPORT, qw(cfgbldOptionRange);

####################################################################################################################################
# cfgbldOptionType - get the option type
####################################################################################################################################
sub cfgbldOptionType
{
    my $strOption = shift;

    return $rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_TYPE};
}

push @EXPORT, qw(cfgbldOptionType);

####################################################################################################################################
# cfgbldOptionTypeTest - test the option type
####################################################################################################################################
sub cfgbldOptionTypeTest
{
    my $strOption = shift;
    my $strType = shift;

    return cfgbldOptionType($strOption) eq $strType;
}

push @EXPORT, qw(cfgbldOptionTypeTest);

####################################################################################################################################
# cfgbldCommandGet - get the hash that contains all valid commands
####################################################################################################################################
sub cfgbldCommandGet
{
    my $rhCommand;

    # Get commands from the rule hash
    foreach my $strOption (sort(keys(%{$rhOptionRule})))
    {
        foreach my $strCommand (sort(keys(%{$rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_COMMAND}})))
        {
            $rhCommand->{$strCommand} = true;
        }
    }

    # Add special commands
    $rhCommand->{&CFGCMD_HELP} = true;
    $rhCommand->{&CFGCMD_VERSION} = true;

    return $rhCommand;
}

push @EXPORT, qw(cfgbldCommandGet);

1;
