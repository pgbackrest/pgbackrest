####################################################################################################################################
# Configuration Definition Interface
####################################################################################################################################
package pgBackRest::Config::Define;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Data;

####################################################################################################################################
# Generate indexed defines
####################################################################################################################################
my $rhConfigDefineIndex = cfgDefine();

foreach my $strKey (sort(keys(%{$rhConfigDefineIndex})))
{
    # Build options for all possible db configurations
    if (defined($rhConfigDefineIndex->{$strKey}{&CFGDEF_PREFIX}) &&
        $rhConfigDefineIndex->{$strKey}{&CFGDEF_PREFIX} eq CFGDEF_PREFIX_DB)
    {
        my $strPrefix = $rhConfigDefineIndex->{$strKey}{&CFGDEF_PREFIX};

        for (my $iIndex = 1; $iIndex <= CFGDEF_INDEX_DB; $iIndex++)
        {
            my $strKeyNew = "${strPrefix}${iIndex}" . substr($strKey, length($strPrefix));

            $rhConfigDefineIndex->{$strKeyNew} = dclone($rhConfigDefineIndex->{$strKey});

            $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_INDEX_TOTAL} = CFGDEF_INDEX_DB;
            $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_INDEX} = $iIndex - 1;

            # Create the alternate name for option index 1
            if ($iIndex == 1)
            {
                $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_ALT_NAME} = $strKey;
            }
            else
            {
                $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_REQUIRED} = false;
            }

            if (defined($rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}) &&
                defined($rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION}))
            {
                $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION} =
                    "${strPrefix}${iIndex}" .
                    substr(
                        $rhConfigDefineIndex->{$strKeyNew}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION},
                        length($strPrefix));
            }
        }

        delete($rhConfigDefineIndex->{$strKey});
    }
    else
    {
        $rhConfigDefineIndex->{$strKey}{&CFGDEF_INDEX} = 0;
    }
}

my $iOptionTotal = scalar(keys(%{$rhConfigDefineIndex}));

sub cfgDefineIndex
{
    return dclone($rhConfigDefineIndex);
}

push @EXPORT, qw(cfgDefineIndex);

####################################################################################################################################
# Create maps to convert option ids to names and vice versa
####################################################################################################################################
my $rhOptionNameId;
my $rhOptionIdName;
my $rhOptionNameAlt;

{
    my $iIndex = 0;

    foreach my $strOption (sort(keys(%{$rhConfigDefineIndex})))
    {
        $rhOptionNameId->{$strOption} = $iIndex;
        $rhOptionNameAlt->{$strOption} = $strOption;
        $rhOptionIdName->{$iIndex} = $strOption;

        if (defined(cfgDefOptionNameAlt($strOption)))
        {
            $rhOptionNameId->{cfgDefOptionNameAlt($strOption)} = $iIndex;
            $rhOptionNameAlt->{cfgDefOptionNameAlt($strOption)} = $strOption;
        }

        $iIndex++;
    }
}

####################################################################################################################################
# Get a define for the option from a command or default
####################################################################################################################################
sub cfgOptionDefine
{
    my $strCommand = shift;
    my $strOption = shift;
    my $strDefine = shift;

    return
        defined($rhConfigDefineIndex->{$strOption}{&CFGDEF_COMMAND}{$strCommand}) &&
        defined($rhConfigDefineIndex->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{$strDefine}) ?
            $rhConfigDefineIndex->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{$strDefine} :
            $rhConfigDefineIndex->{$strOption}{$strDefine};
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
# cfgDefOptionAllowList - does the option have a specific list of allowed values?
####################################################################################################################################
sub cfgDefOptionAllowList
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $bError = shift;

    my $rhAllowList = cfgOptionDefine($strCommand, $strOption, CFGDEF_ALLOW_LIST);

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

push @EXPORT, qw(cfgDefOptionAllowList);

####################################################################################################################################
# cfgDefOptionAllowListValue - get an allow list value
####################################################################################################################################
sub cfgDefOptionAllowListValue
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $iValueIdx = shift;

    # Index shouldn't be greater than the total number of values
    if ($iValueIdx >= cfgDefOptionAllowListValueTotal($strCommand, $strOption, $iValueIdx))
    {
        confess &log(ASSERT, "invalid allow list value index ${iValueIdx} for ${strCommand}, ${strOption}");
    }

    # Return value
    return cfgOptionDefine($strCommand, $strOption, CFGDEF_ALLOW_LIST)->[$iValueIdx];
}

push @EXPORT, qw(cfgDefOptionAllowListValue);

####################################################################################################################################
# cfgDefOptionAllowListValueTotal - how many values are allowed for the option?
####################################################################################################################################
sub cfgDefOptionAllowListValueTotal
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the allow list exists
    cfgDefOptionAllowList($strCommand, $strOption, true);

    # Return total elements in the list
    return scalar(@{cfgOptionDefine($strCommand, $strOption, CFGDEF_ALLOW_LIST)});
}

push @EXPORT, qw(cfgDefOptionAllowListValueTotal);

####################################################################################################################################
# cfgDefOptionAllowListValueValid - is the value valid for this option?
####################################################################################################################################
sub cfgDefOptionAllowListValueValid
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $strValue = shift;

    # Make sure the allow list exists
    cfgDefOptionAllowList($strCommand, $strOption, true);

    # Check if the value is valid
    foreach my $strValueMatch (@{cfgOptionDefine($strCommand, $strOption, CFGDEF_ALLOW_LIST)})
    {
        if ($strValue eq $strValueMatch)
        {
            return true;
        }
    }

    return false;
}

push @EXPORT, qw(cfgDefOptionAllowListValueValid);

####################################################################################################################################
# cfgDefOptionAllowRange - does the option have min/max values?
####################################################################################################################################
sub cfgDefOptionAllowRange
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $bError = shift;

    my $rhAllowRange = cfgOptionDefine($strCommand, $strOption, CFGDEF_ALLOW_RANGE);

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

push @EXPORT, qw(cfgDefOptionAllowRange);

####################################################################################################################################
# cfgDefOptionAllowRangeMax - get max value in allowed range
####################################################################################################################################
sub cfgDefOptionAllowRangeMax
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the allow range exists
    cfgDefOptionAllowRange($strCommand, $strOption);

    # Return value
    return cfgOptionDefine($strCommand, $strOption, CFGDEF_ALLOW_RANGE)->[1];
}

push @EXPORT, qw(cfgDefOptionAllowRangeMax);

####################################################################################################################################
# cfgDefOptionAllowRangeMin - get min value in allowed range
####################################################################################################################################
sub cfgDefOptionAllowRangeMin
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the allow range exists
    cfgDefOptionAllowRange($strCommand, $strOption);

    # Return value
    return cfgOptionDefine($strCommand, $strOption, CFGDEF_ALLOW_RANGE)->[0];
}

push @EXPORT, qw(cfgDefOptionAllowRangeMin);

####################################################################################################################################
# cfgDefOptionDefault - option default, if any
####################################################################################################################################
sub cfgDefOptionDefault
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    return cfgOptionDefine($strCommand, $strOption, CFGDEF_DEFAULT);
}

push @EXPORT, qw(cfgDefOptionDefault);

####################################################################################################################################
# cfgDefOptionDepend - does the option depend on another option being set or having a certain value?
####################################################################################################################################
sub cfgDefOptionDepend
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $bError = shift;

    my $rhDepend = cfgOptionDefine($strCommand, $strOption, CFGDEF_DEPEND);

    if (!defined($rhDepend) && defined($bError) && $bError)
    {
        confess &log(ASSERT, "depend define not set for ${strCommand}, ${strOption}");
    }

    return defined($rhDepend) ? true : false;
}

push @EXPORT, qw(cfgDefOptionDepend);

####################################################################################################################################
# cfgDefOptionDependOption - name of the option that this option depends on
####################################################################################################################################
sub cfgDefOptionDependOption
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the depend define exists
    cfgDefOptionDepend($strCommand, $strOption, true);

    # Error if the depend option does not exist
    my $rhDepend = cfgOptionDefine($strCommand, $strOption, CFGDEF_DEPEND);

    if (!defined($rhDepend->{&CFGDEF_DEPEND_OPTION}))
    {
        confess &log(ASSERT, "depend define option not set for ${strCommand}, ${strOption}");
    }

    return $rhDepend->{&CFGDEF_DEPEND_OPTION};
}

push @EXPORT, qw(cfgDefOptionDependOption);

####################################################################################################################################
# cfgDefOptionDependValue - get a depend option value
####################################################################################################################################
sub cfgDefOptionDependValue
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $iValueIdx = shift;

    # Index shouldn't be greater than the total number of values
    if ($iValueIdx >= cfgDefOptionDependValueTotal($strCommand, $strOption, $iValueIdx))
    {
        confess &log(ASSERT, "invalid depend value index ${iValueIdx} for ${strCommand}, ${strOption}");
    }

    # Return value
    return cfgOptionDefine($strCommand, $strOption, CFGDEF_DEPEND)->{&CFGDEF_DEPEND_LIST}->[$iValueIdx];
}

push @EXPORT, qw(cfgDefOptionDependValue);

####################################################################################################################################
# cfgDefOptionDependValueTotal - how many values are allowed for the depend option?
#
# 0 indicates that the value of the depend option doesn't matter, only that is is set.
####################################################################################################################################
sub cfgDefOptionDependValueTotal
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    # Make sure the depend define exists
    cfgDefOptionDepend($strCommand, $strOption, true);

    # It's OK for the list not to be defined
    my $rhDepend = cfgOptionDefine($strCommand, $strOption, CFGDEF_DEPEND);

    if (!defined($rhDepend->{&CFGDEF_DEPEND_LIST}))
    {
        return 0;
    }

    # Return total elements in the list
    return scalar(@{$rhDepend->{&CFGDEF_DEPEND_LIST}});
}

push @EXPORT, qw(cfgDefOptionDependValueTotal);

####################################################################################################################################
# cfgDefOptionDependValueValid - is the depend valid valid?
####################################################################################################################################
sub cfgDefOptionDependValueValid
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);
    my $strValue = shift;

    # Make sure the depend define exists
    cfgDefOptionDepend($strCommand, $strOption, true);

    # Check if the value is valid
    foreach my $strValueMatch (@{cfgOptionDefine($strCommand, $strOption, CFGDEF_DEPEND)->{&CFGDEF_DEPEND_LIST}})
    {
        if ($strValue eq $strValueMatch)
        {
            return true;
        }
    }

    return false;
}

push @EXPORT, qw(cfgDefOptionDependValueValid);

####################################################################################################################################
# cfgOptionIndex - index for option
####################################################################################################################################
sub cfgOptionIndex
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_INDEX};
}

push @EXPORT, qw(cfgOptionIndex);

####################################################################################################################################
# cfgOptionIndexTotal - max index for options that are indexed (e.g., db)
####################################################################################################################################
sub cfgOptionIndexTotal
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_INDEX_TOTAL};
}

push @EXPORT, qw(cfgOptionIndexTotal);

####################################################################################################################################
# cfgDefOptionNameAlt - alternative or deprecated option name
####################################################################################################################################
sub cfgDefOptionNameAlt
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_ALT_NAME};
}

push @EXPORT, qw(cfgDefOptionNameAlt);

####################################################################################################################################
# cfgDefOptionNegate - is the boolean option negatable?
####################################################################################################################################
sub cfgDefOptionNegate
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_NEGATE};
}

push @EXPORT, qw(cfgDefOptionNegate);

####################################################################################################################################
# cfgDefOptionPrefix - fixed prefix for indexed options
####################################################################################################################################
sub cfgDefOptionPrefix
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_PREFIX};
}

push @EXPORT, qw(cfgDefOptionPrefix);

####################################################################################################################################
# cfgDefOptionRequired - is the option required?
####################################################################################################################################
sub cfgDefOptionRequired
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    my $rxDefine = cfgOptionDefine($strCommand, $strOption, CFGDEF_REQUIRED);
    return defined($rxDefine) ? $rxDefine : true;
}

push @EXPORT, qw(cfgDefOptionRequired);

####################################################################################################################################
# cfgDefOptionSection - section to contain optio (global or stanza), all others are command-line only
####################################################################################################################################
sub cfgDefOptionSection
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_SECTION};
}

push @EXPORT, qw(cfgDefOptionSection);

####################################################################################################################################
# cfgDefOptionSecure - can the option be passed on the command-line?
####################################################################################################################################
sub cfgDefOptionSecure
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_SECURE};
}

push @EXPORT, qw(cfgDefOptionSecure);

####################################################################################################################################
# cfgDefOptionType - data type of the option (e.g. boolean, string)
####################################################################################################################################
sub cfgDefOptionType
{
    my $strOption = cfgOptionName(shift);

    return $rhConfigDefineIndex->{$strOption}{&CFGDEF_TYPE};
}

push @EXPORT, qw(cfgDefOptionType);

####################################################################################################################################
# cfgDefOptionValid - is the option valid for the command?
####################################################################################################################################
sub cfgDefOptionValid
{
    my $strCommand = shift;
    my $strOption = cfgOptionName(shift);

    return
        defined($rhConfigDefineIndex->{$strOption}{&CFGDEF_COMMAND}) &&
        defined($rhConfigDefineIndex->{$strOption}{&CFGDEF_COMMAND}{$strCommand}) ? true : false;
}

push @EXPORT, qw(cfgDefOptionValid);

1;
