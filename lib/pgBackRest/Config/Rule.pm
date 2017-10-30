####################################################################################################################################
# Configuration Rule Interface
####################################################################################################################################
package pgBackRest::Config::Rule;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Data;

####################################################################################################################################
# Copy indexed rules locally
####################################################################################################################################
my $rhOptionRuleIndex = cfgdefRuleIndex();
my $iOptionTotal = scalar(keys(%{$rhOptionRuleIndex}));

####################################################################################################################################
# Create maps to convert option ids to names and vice versa
####################################################################################################################################
my $rhOptionNameId;
my $rhOptionIdName;
my $rhOptionNameAlt;

{
    my $iIndex = 0;

    foreach my $strOption (sort(keys(%{$rhOptionRuleIndex})))
    {
        $rhOptionNameId->{$strOption} = $iIndex;
        $rhOptionNameAlt->{$strOption} = $strOption;
        $rhOptionIdName->{$iIndex} = $strOption;

        if (defined(cfgRuleOptionNameAlt($strOption)))
        {
            $rhOptionNameId->{cfgRuleOptionNameAlt($strOption)} = $iIndex;
            $rhOptionNameAlt->{cfgRuleOptionNameAlt($strOption)} = $strOption;
        }

        $iIndex++;
    }
}

####################################################################################################################################
# cfgOptionRule - get a rule for the option from a command or default
####################################################################################################################################
sub cfgOptionRule
{
    my $strCommand = shift;
    my $strOption = shift;
    my $strRule = shift;

    return
        defined($rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand}{$strRule}) ?
            $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand}{$strRule} :
            $rhOptionRuleIndex->{$strOption}{$strRule};
}

####################################################################################################################################
# Functions that are noops in the Perl code since commands/options are always treated as strings
####################################################################################################################################
sub cfgCommandId {return shift()}
sub cfgCommandName {return shift()}

sub cfgOptionId
{
    my $strOptionName = shift;

    if (!defined($rhOptionNameId->{$strOptionName}))
    {
        return -1;
    }

    return $rhOptionNameAlt->{$strOptionName};
}

sub cfgOptionName
{
    my $strOptionId = shift;

    if (defined($rhOptionIdName->{$strOptionId}))
    {
        return $rhOptionIdName->{$strOptionId};
    }

    return $rhOptionNameAlt->{$strOptionId};
}

push @EXPORT, qw(cfgCommandId cfgCommandName cfgOptionId cfgOptionName);

####################################################################################################################################
# cfgOptionTotal - total number of options
####################################################################################################################################
sub cfgOptionTotal
{
    return $iOptionTotal;
}

push @EXPORT, qw(cfgOptionTotal);

####################################################################################################################################
# cfgRuleOptionAllowList - does the option have a specific list of allowed values?
####################################################################################################################################
sub cfgRuleOptionAllowList
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $bError = shift;

    my $rhAllowList = cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_ALLOW_LIST);

    if (!defined($rhAllowList) && defined($bError) && $bError)
    {
        confess &log(ASSERT, "allow list not set for ${strCommand}, ${strOption}");
    }

    # The allow list must have values
    if (defined($rhAllowList) && @{$rhAllowList} == 0)
    {
        confess &log(ASSERT, "allow list must have values for ${strCommand}, ${strOption}");
    }

    return defined($rhAllowList) ? true : false;
}

push @EXPORT, qw(cfgRuleOptionAllowList);

####################################################################################################################################
# cfgRuleOptionAllowListValue - get an allow list value
####################################################################################################################################
sub cfgRuleOptionAllowListValue
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $iValueIdx = shift;

    # Index shouldn't be greater than the total number of values
    if ($iValueIdx >= cfgRuleOptionAllowListValueTotal($strCommand, $strOption, $iValueIdx))
    {
        confess &log(ASSERT, "invalid allow list value index ${iValueIdx} for ${strCommand}, ${strOption}");
    }

    # Return value
    return cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_ALLOW_LIST)->[$iValueIdx];
}

push @EXPORT, qw(cfgRuleOptionAllowListValue);

####################################################################################################################################
# cfgRuleOptionAllowListValueTotal - how many values are allowed for the option?
####################################################################################################################################
sub cfgRuleOptionAllowListValueTotal
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the allow list exists
    cfgRuleOptionAllowList($strCommand, $strOption, true);

    # Return total elements in the list
    return scalar(@{cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_ALLOW_LIST)});
}

push @EXPORT, qw(cfgRuleOptionAllowListValueTotal);

####################################################################################################################################
# cfgRuleOptionAllowListValueValid - is the value valid for this option?
####################################################################################################################################
sub cfgRuleOptionAllowListValueValid
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $strValue = shift;

    # Make sure the allow list exists
    cfgRuleOptionAllowList($strCommand, $strOption, true);

    # Check if the value is valid
    foreach my $strValueMatch (@{cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_ALLOW_LIST)})
    {
        if ($strValue eq $strValueMatch)
        {
            return true;
        }
    }

    return false;
}

push @EXPORT, qw(cfgRuleOptionAllowListValueValid);

####################################################################################################################################
# cfgRuleOptionAllowRange - does the option have min/max values?
####################################################################################################################################
sub cfgRuleOptionAllowRange
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $bError = shift;

    my $rhAllowRange = cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_ALLOW_RANGE);

    if (!defined($rhAllowRange) && defined($bError) && $bError)
    {
        confess &log(ASSERT, "allow range not set for ${strCommand}, ${strOption}");
    }

    # The allow range must have two values
    if (defined($rhAllowRange) && @{$rhAllowRange} != 2)
    {
        confess &log(ASSERT, "allow range must have two values for ${strCommand}, ${strOption}");
    }

    return defined($rhAllowRange) ? true : false;
}

push @EXPORT, qw(cfgRuleOptionAllowRange);

####################################################################################################################################
# cfgRuleOptionAllowRangeMax - get max value in allowed range
####################################################################################################################################
sub cfgRuleOptionAllowRangeMax
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the allow range exists
    cfgRuleOptionAllowRange($strCommand, $strOption);

    # Return value
    return cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_ALLOW_RANGE)->[1];
}

push @EXPORT, qw(cfgRuleOptionAllowRangeMax);

####################################################################################################################################
# cfgRuleOptionAllowRangeMin - get min value in allowed range
####################################################################################################################################
sub cfgRuleOptionAllowRangeMin
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the allow range exists
    cfgRuleOptionAllowRange($strCommand, $strOption);

    # Return value
    return cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_ALLOW_RANGE)->[0];
}

push @EXPORT, qw(cfgRuleOptionAllowRangeMin);

####################################################################################################################################
# cfgRuleOptionDefault - option default, if any
####################################################################################################################################
sub cfgRuleOptionDefault
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    return cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_DEFAULT);
}

push @EXPORT, qw(cfgRuleOptionDefault);

####################################################################################################################################
# cfgRuleOptionDepend - does the option depend on another option being set or having a certain value?
####################################################################################################################################
sub cfgRuleOptionDepend
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $bError = shift;

    my $rhDepend = cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_DEPEND);

    if (!defined($rhDepend) && defined($bError) && $bError)
    {
        confess &log(ASSERT, "depend rule not set for ${strCommand}, ${strOption}");
    }

    return defined($rhDepend) ? true : false;
}

push @EXPORT, qw(cfgRuleOptionDepend);

####################################################################################################################################
# cfgRuleOptionDependOption - name of the option that this option depends on
####################################################################################################################################
sub cfgRuleOptionDependOption
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the depend rule exists
    cfgRuleOptionDepend($strCommand, $strOption, true);

    # Error if the depend option does not exist
    my $rhDepend = cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_DEPEND);

    if (!defined($rhDepend->{&CFGBLDDEF_RULE_DEPEND_OPTION}))
    {
        confess &log(ASSERT, "depend rule option not set for ${strCommand}, ${strOption}");
    }

    return $rhDepend->{&CFGBLDDEF_RULE_DEPEND_OPTION};
}

push @EXPORT, qw(cfgRuleOptionDependOption);

####################################################################################################################################
# cfgRuleOptionDependValue - get a depend option value
####################################################################################################################################
sub cfgRuleOptionDependValue
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $iValueIdx = shift;

    # Index shouldn't be greater than the total number of values
    if ($iValueIdx >= cfgRuleOptionDependValueTotal($strCommand, $strOption, $iValueIdx))
    {
        confess &log(ASSERT, "invalid depend value index ${iValueIdx} for ${strCommand}, ${strOption}");
    }

    # Return value
    return cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_DEPEND)->{&CFGBLDDEF_RULE_DEPEND_LIST}->[$iValueIdx];
}

push @EXPORT, qw(cfgRuleOptionDependValue);

####################################################################################################################################
# cfgRuleOptionDependValueTotal - how many values are allowed for the depend option?
#
# 0 indicates that the value of the depend option doesn't matter, only that is is set.
####################################################################################################################################
sub cfgRuleOptionDependValueTotal
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the depend rule exists
    cfgRuleOptionDepend($strCommand, $strOption, true);

    # It's OK for the list not to be defined
    my $rhDepend = cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_DEPEND);

    if (!defined($rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}))
    {
        return 0;
    }

    # Return total elements in the list
    return scalar(@{$rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}});
}

push @EXPORT, qw(cfgRuleOptionDependValueTotal);

####################################################################################################################################
# cfgRuleOptionDependValueValid - is the depend valid valid?
####################################################################################################################################
sub cfgRuleOptionDependValueValid
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $strValue = shift;

    # Make sure the depend rule exists
    cfgRuleOptionDepend($strCommand, $strOption, true);

    # Check if the value is valid
    foreach my $strValueMatch (@{cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_DEPEND)->{&CFGBLDDEF_RULE_DEPEND_LIST}})
    {
        if ($strValue eq $strValueMatch)
        {
            return true;
        }
    }

    return false;
}

push @EXPORT, qw(cfgRuleOptionDependValueValid);

####################################################################################################################################
# cfgOptionIndexTotal - max index for options that are indexed (e.g., db)
####################################################################################################################################
sub cfgOptionIndexTotal
{
    my $strOption = cfgOptionName(shift);

    return $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_INDEX};
}

push @EXPORT, qw(cfgOptionIndexTotal);

####################################################################################################################################
# cfgRuleOptionNameAlt - alternative or deprecated option name
####################################################################################################################################
sub cfgRuleOptionNameAlt
{
    my $strOption = cfgOptionName(shift);

    return $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_ALT_NAME};
}

push @EXPORT, qw(cfgRuleOptionNameAlt);

####################################################################################################################################
# cfgRuleOptionNegate - is the boolean option negatable?
####################################################################################################################################
sub cfgRuleOptionNegate
{
    my $strOption = cfgOptionName(shift);

    return $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_NEGATE};
}

push @EXPORT, qw(cfgRuleOptionNegate);

####################################################################################################################################
# cfgRuleOptionPrefix - fixed prefix for indexed options
####################################################################################################################################
sub cfgRuleOptionPrefix
{
    my $strOption = cfgOptionName(shift);

    return $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_PREFIX};
}

push @EXPORT, qw(cfgRuleOptionPrefix);

####################################################################################################################################
# cfgRuleOptionRequired - is the option required?
####################################################################################################################################
sub cfgRuleOptionRequired
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    my $rxRule = cfgOptionRule($strCommand, $strOption, CFGBLDDEF_RULE_REQUIRED);
    return defined($rxRule) ? $rxRule : true;
}

push @EXPORT, qw(cfgRuleOptionRequired);

####################################################################################################################################
# cfgRuleOptionSection - section to contain optio (global or stanza), all others are command-line only
####################################################################################################################################
sub cfgRuleOptionSection
{
    my $strOption = cfgOptionName(shift);

    return $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_SECTION};
}

push @EXPORT, qw(cfgRuleOptionSection);

####################################################################################################################################
# cfgRuleOptionSecure - can the option be passed on the command-line?
####################################################################################################################################
sub cfgRuleOptionSecure
{
    my $strOption = cfgOptionName(shift);

    return $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_SECURE};
}

push @EXPORT, qw(cfgRuleOptionSecure);

####################################################################################################################################
# cfgRuleOptionType - data type of the option (e.g. boolean, string)
####################################################################################################################################
sub cfgRuleOptionType
{
    my $strOption = cfgOptionName(shift);

    return $rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_TYPE};
}

push @EXPORT, qw(cfgRuleOptionType);

####################################################################################################################################
# cfgRuleOptionValid - is the option valid for the command?
####################################################################################################################################
sub cfgRuleOptionValid
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    return defined($rhOptionRuleIndex->{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand}) ? true : false;
}

push @EXPORT, qw(cfgRuleOptionValid);

1;
