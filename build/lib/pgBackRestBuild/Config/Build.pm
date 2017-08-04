####################################################################################################################################
# Build Makefile and Auto-Generate Files Required for Build
####################################################################################################################################
package pgBackRestBuild::Config::Build;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use Storable qw(dclone);

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Version;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::CodeGen::Common;
use pgBackRestBuild::CodeGen::Lookup;
use pgBackRestBuild::CodeGen::Switch;
use pgBackRestBuild::Config::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant PARAM_CFGBLDOPT_ID                                     => 'uiOptionId';
use constant PARAM_COMMAND_ID                                       => 'uiCommandId';
use constant PARAM_VALUE_ID                                         => 'uiValueId';

use constant PARAM_ORDER                                            => 'param-order';
use constant SWITCH_ORDER                                           => 'switch-order';
use constant VALUE_MAP                                              => 'value-map';

use constant CFGBLDOPT_RULE_ALLOW_LIST_VALUE                        => CFGBLDOPT_RULE_ALLOW_LIST . '-value';
use constant CFGBLDOPT_RULE_ALLOW_LIST_VALUE_TOTAL                  => CFGBLDOPT_RULE_ALLOW_LIST_VALUE . '-total';
use constant CFGBLDOPT_RULE_ALLOW_RANGE_MIN                         => CFGBLDOPT_RULE_ALLOW_RANGE . '-min';
use constant CFGBLDOPT_RULE_ALLOW_RANGE_MAX                         => CFGBLDOPT_RULE_ALLOW_RANGE . '-max';
use constant CFGBLDOPT_RULE_DEPEND_VALUE_TOTAL                      => CFGBLDOPT_RULE_DEPEND_VALUE . '-total';
use constant CFGBLDOPT_RULE_NAME                                    => 'name';
use constant CFGBLDOPT_RULE_VALID                                   => 'valid';

use constant PARAM_FILE                                             => 'file';

use constant FILE_CONFIG                                            => 'config';
use constant FILE_CONFIG_RULE                                       => FILE_CONFIG . 'Rule';

####################################################################################################################################
# Build command constant maps
####################################################################################################################################
my $rhCommandIdConstantMap;
my $rhCommandNameConstantMap;
my $rhCommandNameIdMap;
my $iCommandTotal = 0;

foreach my $strCommand (sort(keys(%{commandHashGet()})))
{
    my $strCommandConstant = "CFGCMD_" . uc($strCommand);
    $strCommandConstant =~ s/\-/\_/g;

    $rhCommandIdConstantMap->{$iCommandTotal} = $strCommandConstant;
    $rhCommandNameConstantMap->{$strCommand} = $strCommandConstant;
    $rhCommandNameIdMap->{$strCommand} = $iCommandTotal;

    $iCommandTotal++;
};

####################################################################################################################################
# Build option constant maps
####################################################################################################################################
my $rhOptionIdConstantMap;
my $rhOptionNameConstantMap;
my $rhOptionNameIdMap;
my $iOptionTotal = 0;
my $rhOptionRule = optionRuleGet();
my @stryOptionAlt;

foreach my $strOption (sort(keys(%{$rhOptionRule})))
{
    my $rhOption = $rhOptionRule->{$strOption};
    my $strOptionConstant = "CFGOPT_" . uc($strOption);
    $strOptionConstant =~ s/\-/\_/g;

    $rhOptionIdConstantMap->{$iOptionTotal} = $strOptionConstant;
    $rhOptionNameConstantMap->{$strOption} = $strOptionConstant;
    $rhOptionNameIdMap->{$strOption} = $iOptionTotal;

    # Create constants for the db- alt names
    my $strOptionAlt = $rhOption->{&CFGBLDOPT_RULE_ALT_NAME};

    if (defined($strOptionAlt) && $strOptionAlt =~ /^db-/)
    {
        $strOptionConstant = "CFGOPT_" . uc($strOptionAlt);
        $strOptionConstant =~ s/\-/\_/g;

        $rhOptionNameConstantMap->{$strOptionAlt} = $strOptionConstant;
        $rhOptionNameIdMap->{$strOptionAlt} = $iOptionTotal;

        push(@stryOptionAlt, $strOptionAlt);
    }

    $iOptionTotal++;
};

####################################################################################################################################
# Build option type constant maps
####################################################################################################################################
my $rhOptionTypeIdConstantMap;
my $rhOptionTypeNameConstantMap;
my $rhOptionTypeNameIdMap;
my $iOptionTypeTotal = 0;

foreach my $strOption (sort(keys(%{$rhOptionRule})))
{
    my $strOptionType = $rhOptionRule->{$strOption}{&CFGBLDOPT_RULE_TYPE};

    if (!defined($rhOptionTypeNameConstantMap->{$strOptionType}))
    {
        my $strOptionTypeConstant = "CFGOPTRULE_TYPE_" . uc($strOptionType);
        $strOptionTypeConstant =~ s/\-/\_/g;

        $rhOptionTypeIdConstantMap->{$iOptionTypeTotal} = $strOptionTypeConstant;
        $rhOptionTypeNameConstantMap->{$strOptionType} = $strOptionTypeConstant;
        $rhOptionTypeNameIdMap->{$strOptionType} = $iOptionTypeTotal;

        $iOptionTypeTotal++;
    }
};

####################################################################################################################################
# Definitions for functions to build
####################################################################################################################################
my $rhOptionRuleParam =
{
    &CFGBLDOPT_RULE_ALLOW_LIST_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'AllowListValue',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID],
    },

    &CFGBLDOPT_RULE_ALLOW_LIST_VALUE_TOTAL =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'AllowListValueTotal',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_ALLOW_RANGE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'AllowRange',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_ALLOW_RANGE_MIN =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'AllowRangeMin',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_DOUBLE,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_ALLOW_RANGE_MAX =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'AllowRangeMax',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_DOUBLE,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    # !!! NEED TO GUARANTEE THAT THIS RETURNS NULL
    &CFGBLDOPT_RULE_DEFAULT =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Default',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_INDEX =>
    {
        &PARAM_FILE => FILE_CONFIG,
        &CFGBLDOPT_RULE_NAME => 'IndexTotal',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_DEPEND_OPTION =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'DependOption',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &VALUE_MAP => $rhOptionIdConstantMap,
    },

    &CFGBLDOPT_RULE_DEPEND_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'DependValue',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID],
    },

    &CFGBLDOPT_RULE_DEPEND_VALUE_TOTAL =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'DependValueTotal',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_HINT =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Hint',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_ALT_NAME =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'NameAlt',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_NEGATE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Negate',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_PREFIX =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Prefix',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_REQUIRED =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Required',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_SECTION =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Section',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_SECURE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Secure',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_TYPE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Type',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &VALUE_MAP => $rhOptionTypeIdConstantMap,
    },

    &CFGBLDOPT_RULE_VALID =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'Valid',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDOPT_RULE_HASH_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDOPT_RULE_NAME => 'ValueHash',
        &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },
};

####################################################################################################################################
# Option rule help functions
####################################################################################################################################
sub optionRule
{
    my $strRule = shift;
    my $strValue = shift;
    my $iOptionId = shift;
    my $iCommandId = shift;

    return if defined($iCommandId) && !defined($strValue);

    push(
        @{$rhOptionRuleParam->{$strRule}{matrix}},
        [$strValue, $iOptionId, $iCommandId]);
}

sub optionRuleDepend
{
    my $rhDepend = shift;
    my $iOptionId = shift;
    my $iCommandId = shift;

    my $strDependOption = $rhDepend->{&CFGBLDOPT_RULE_DEPEND_OPTION};

    optionRule(
        CFGBLDOPT_RULE_DEPEND_OPTION,
        defined($strDependOption) ? $rhOptionNameIdMap->{$strDependOption} : (defined($iCommandId) ? undef : -1),
        $iOptionId, $iCommandId);

    my $iValueTotal = 0;

    if (defined($rhDepend->{&CFGBLDOPT_RULE_DEPEND_VALUE}))
    {
        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_DEPEND_VALUE}{matrix}},
            [$rhDepend->{&CFGBLDOPT_RULE_DEPEND_VALUE}, $iOptionId, $iValueTotal, $iCommandId]);

        $iValueTotal++;
    }
    elsif (defined($rhDepend->{&CFGBLDOPT_RULE_DEPEND_LIST}))
    {
        foreach my $strValue (sort(keys(%{$rhDepend->{&CFGBLDOPT_RULE_DEPEND_LIST}})))
        {
            push(
                @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_DEPEND_VALUE}{matrix}},
                [$strValue, $iOptionId, $iValueTotal, $iCommandId]);

            $iValueTotal++;
        }
    }
    else
    {
        push(@{$rhOptionRuleParam->{&CFGBLDOPT_RULE_DEPEND_VALUE}{matrix}}, [undef, $iOptionId, $iValueTotal, $iCommandId]);
    }

    optionRule(CFGBLDOPT_RULE_DEPEND_VALUE_TOTAL, $iValueTotal, $iOptionId, $iCommandId);
}

sub optionRuleRange
{
    my $riyRange = shift;
    my $iOptionId = shift;
    my $iCommandId = shift;

    optionRule(
        CFGBLDOPT_RULE_ALLOW_RANGE, defined($riyRange) ? true : (defined($iCommandId) ? undef : false), $iOptionId, $iCommandId);

    if (defined($riyRange))
    {
        optionRule(CFGBLDOPT_RULE_ALLOW_RANGE_MIN, $riyRange->[0], $iOptionId, $iCommandId);
        optionRule(CFGBLDOPT_RULE_ALLOW_RANGE_MAX, $riyRange->[1], $iOptionId, $iCommandId);
    }
}

sub optionRuleAllowList
{
    my $rhAllowList = shift;
    my $iOptionId = shift;
    my $iCommandId = shift;

    my $iValueTotal = 0;

    if (defined($rhAllowList))
    {
        foreach my $strValue (sort(keys(%{$rhAllowList})))
        {
            push(
                @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_ALLOW_LIST_VALUE}{matrix}},
                [$strValue, $iOptionId, $iValueTotal, $iCommandId]);

            $iValueTotal++;
        }
    }
    else
    {
        push(@{$rhOptionRuleParam->{&CFGBLDOPT_RULE_ALLOW_LIST_VALUE}{matrix}}, [undef, $iOptionId, $iValueTotal, $iCommandId]);
    }

    optionRule(CFGBLDOPT_RULE_ALLOW_LIST_VALUE_TOTAL, $iValueTotal, $iOptionId, $iCommandId);
}

sub buildConfig
{
    my $rhBuild;

    ################################################################################################################################
    # Create C header with #defines for constants
    ################################################################################################################################
    {
        my $strConstantHeader;

        # Add command constants
        $strConstantHeader .=  "#define CONFIG_COMMAND_TOTAL ${iCommandTotal}\n\n";

        foreach my $strCommand (sort(keys(%{$rhCommandNameConstantMap})))
        {
            my $strConstant = $rhCommandNameConstantMap->{$strCommand};
            my $strConstantValue = $rhCommandNameIdMap->{$strCommand};

            $strConstantHeader .= "#define ${strConstant} ${strConstantValue}\n";
            $rhBuild->{&FILE_CONFIG}{&BLD_HEADER}{&BLD_CONSTANT}{$strConstant} = $strConstantValue;
        }

        $strConstantHeader .=  "\n#define CFGOPTRULE_TYPE_TOTAL ${iOptionTypeTotal}\n\n";

        # Add option type constants
        foreach my $strOptionType (sort(keys(%{$rhOptionTypeNameConstantMap})))
        {
            my $strConstant = $rhOptionTypeNameConstantMap->{$strOptionType};
            my $strConstantValue = $rhOptionTypeNameIdMap->{$strOptionType};

            $strConstantHeader .= "#define ${strConstant} ${strConstantValue}\n";
            $rhBuild->{&FILE_CONFIG}{&BLD_HEADER}{&BLD_CONSTANT}{$strConstant} = $strConstantValue;
        }

        $strConstantHeader .=  "\n#define CONFIG_OPTION_TOTAL ${iOptionTotal}\n\n";

        # Add option constants
        foreach my $strOption (sort(keys(%{$rhOptionNameConstantMap})))
        {
            my $strConstant = $rhOptionNameConstantMap->{$strOption};
            my $strConstantValue = $rhOptionNameIdMap->{$strOption};

            $strConstantHeader .= "#define ${strConstant} ${strConstantValue}\n";
            $rhBuild->{&FILE_CONFIG}{&BLD_HEADER}{&BLD_CONSTANT}{$strConstant} = $strConstantValue;
        }

        $rhBuild->{&FILE_CONFIG}{&BLD_HEADER}{&BLD_SOURCE} =
            "/* AUTOGENERATED CONFIG CONSTANTS - DO NOT MODIFY THIS FILE */\n" .
            "#ifndef CONFIG_AUTO_H\n" .
            "#define CONFIG_AUTO_H\n\n" .
            "${strConstantHeader}\n" .
            "#endif";
    }

    foreach my $strOption (sort(keys(%{$rhOptionRule})))
    {
        my $rhOption = $rhOptionRule->{$strOption};
        my $iOptionId = $rhOptionNameIdMap->{$strOption};

        foreach my $strCommand (sort(keys(%{commandHashGet()})))
        {
            my $rhCommand = $rhOption->{&CFGBLDOPT_RULE_COMMAND}{$strCommand};
            my $iCommandId = $rhCommandNameIdMap->{$strCommand};

            if (defined($rhCommand))
            {
                optionRule(CFGBLDOPT_RULE_VALID, true, $iOptionId, $iCommandId);
                optionRule(CFGBLDOPT_RULE_REQUIRED, $rhCommand->{&CFGBLDOPT_RULE_REQUIRED}, $iOptionId, $iCommandId);
                optionRule(CFGBLDOPT_RULE_DEFAULT, $rhCommand->{&CFGBLDOPT_RULE_DEFAULT}, $iOptionId, $iCommandId);
                optionRule(CFGBLDOPT_RULE_HINT, $rhCommand->{&CFGBLDOPT_RULE_HINT}, $iOptionId, $iCommandId);
                optionRuleDepend($rhCommand->{&CFGBLDOPT_RULE_DEPEND}, $iOptionId, $iCommandId);
                optionRuleRange($rhCommand->{&CFGBLDOPT_RULE_ALLOW_RANGE}, $iOptionId, $iCommandId);
                optionRuleAllowList($rhCommand->{&CFGBLDOPT_RULE_ALLOW_LIST}, $iOptionId, $iCommandId);
            }
            else
            {
                optionRule(CFGBLDOPT_RULE_VALID, false, $iOptionId, $iCommandId);
            }
        }

        optionRule(CFGBLDOPT_RULE_REQUIRED, coalesce($rhOption->{&CFGBLDOPT_RULE_REQUIRED}, true), $iOptionId);
        optionRule(CFGBLDOPT_RULE_DEFAULT, $rhOption->{&CFGBLDOPT_RULE_DEFAULT}, $iOptionId);
        optionRule(CFGBLDOPT_RULE_HINT, $rhOption->{&CFGBLDOPT_RULE_HINT}, $iOptionId);
        optionRuleDepend($rhOption->{&CFGBLDOPT_RULE_DEPEND}, $iOptionId);
        optionRuleRange($rhOption->{&CFGBLDOPT_RULE_ALLOW_RANGE}, $iOptionId);
        optionRuleAllowList($rhOption->{&CFGBLDOPT_RULE_ALLOW_LIST}, $iOptionId);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_INDEX}{matrix}},
            [$rhOption->{&CFGBLDOPT_RULE_INDEX}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_NEGATE}{matrix}},
            [coalesce($rhOption->{&CFGBLDOPT_RULE_NEGATE}, false), $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_ALT_NAME}{matrix}},
            [$rhOption->{&CFGBLDOPT_RULE_ALT_NAME}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_PREFIX}{matrix}},
            [$rhOption->{&CFGBLDOPT_RULE_PREFIX}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_SECTION}{matrix}},
            [$rhOption->{&CFGBLDOPT_RULE_SECTION}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_SECURE}{matrix}},
            [$rhOption->{&CFGBLDOPT_RULE_SECURE}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_TYPE}{matrix}},
            [$rhOptionTypeNameIdMap->{$rhOption->{&CFGBLDOPT_RULE_TYPE}}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDOPT_RULE_HASH_VALUE}{matrix}},
            [$rhOption->{&CFGBLDOPT_RULE_HASH_VALUE}, $iOptionId]);
    }

    my $rhLabelMap = {&PARAM_CFGBLDOPT_ID => $rhOptionIdConstantMap, &PARAM_COMMAND_ID => $rhCommandIdConstantMap};

    my $strConfigFunction =
        "// AUTOGENERATED CODE - DO NOT MODIFY THIS FILE";

    $strConfigFunction .= "\n" . cgenFunction(
        'cfgCommandId', 'lookup command id using command name', undef, cgenLookupId('Command', 'CONFIG_COMMAND_TOTAL'));
    $strConfigFunction .= "\n" . cgenFunction(
        'cfgOptionId', 'lookup option id using option name', undef, cgenLookupId('Option', 'CONFIG_OPTION_TOTAL'));

    foreach my $strOptionRule (sort(keys(%{$rhOptionRuleParam})))
    {
        my $rhOptionRule = $rhOptionRuleParam->{$strOptionRule};

        next if $rhOptionRule->{&PARAM_FILE} ne FILE_CONFIG_RULE;

        my $strPrefix = 'cfgOptionRule';

        $strConfigFunction .= "\n" .
            cgenFunction($strPrefix . $rhOptionRule->{&CFGBLDOPT_RULE_NAME}, undef, undef,
                cgenSwitchBuild(
                    $strPrefix . $rhOptionRule->{&CFGBLDOPT_RULE_NAME}, $rhOptionRule->{&CFGBLDOPT_RULE_TYPE},
                    $rhOptionRule->{matrix}, $rhOptionRule->{&PARAM_ORDER}, $rhOptionRule->{&SWITCH_ORDER}, $rhLabelMap,
                    $rhOptionRule->{&VALUE_MAP}));
    }

    $rhBuild->{&FILE_CONFIG_RULE}{&BLD_C}{&BLD_SOURCE} = $strConfigFunction;

    $strConfigFunction =
        "// AUTOGENERATED CODE - DO NOT MODIFY THIS FILE";

    $strConfigFunction .= "\n" . cgenFunction(
        'cfgCommandName', 'lookup command name using command id', undef,
        cgenLookupString('Command', 'CONFIG_COMMAND_TOTAL', $rhCommandNameConstantMap));
    $strConfigFunction .= "\n" . cgenFunction(
        'cfgOptionName', 'lookup option name using option id', undef,
        cgenLookupString(
            'Option', 'CONFIG_OPTION_TOTAL', $rhOptionNameConstantMap, '^(' . join('|', @stryOptionAlt) . ')$'));

    foreach my $strOptionRule (sort(keys(%{$rhOptionRuleParam})))
    {
        my $rhOptionRule = $rhOptionRuleParam->{$strOptionRule};

        next if $rhOptionRule->{&PARAM_FILE} ne FILE_CONFIG;

        my $strPrefix = 'cfgOption';

        $strConfigFunction .= "\n" .
            cgenFunction($strPrefix . $rhOptionRule->{&CFGBLDOPT_RULE_NAME}, undef, undef,
                cgenSwitchBuild(
                    $strPrefix . $rhOptionRule->{&CFGBLDOPT_RULE_NAME}, $rhOptionRule->{&CFGBLDOPT_RULE_TYPE},
                    $rhOptionRule->{matrix}, $rhOptionRule->{&PARAM_ORDER}, $rhOptionRule->{&SWITCH_ORDER}, $rhLabelMap,
                    $rhOptionRule->{&VALUE_MAP}));
    }

    $rhBuild->{&FILE_CONFIG}{&BLD_C}{&BLD_SOURCE} = $strConfigFunction;

    ####################################################################################################################################
    # Create Help Functions
    ####################################################################################################################################
    # !!! THIS IS NOT WORKING BECAUSE DOCCONFIG BLOWS UP ON INDEXED OPTIONS.  NEED TO ADD INDEX_TOTAL AND CHANGE WHAT IS NOW INDEX TO
    # BE THE INDEX FOR THE CURRENT OPTION (RIGHT NOW IT IS TOTAL)
    # OR MAYBE JUST CREATE A DIFFERENT FUNCTION TO RETURN INDEXED OPTION HASH

    # use lib dirname($0) . '/../doc/lib';
    #
    # use BackRestDoc::Common::Doc;
    # use BackRestDoc::Common::DocConfig;
    #
    # my $oDocConfig = new BackRestDoc::Common::DocConfig(
    #     new BackRestDoc::Common::Doc(dirname($0) . '/../doc/xml/reference.xml', dirname($0) . '/../doc/xml/dtd'));

    # !!! IMPLEMENT THIS OR REMOVE IT !!!
    # use constant HELP_COMMAND                                           => 'helpCommand';
    # use constant HELP_COMMAND_DESCRIPTION                               => HELP_COMMAND . 'Description';
    # use constant HELP_COMMAND_SUMMARY                                   => HELP_COMMAND . 'Summary';
    #
    # use constant HELP_COMMAND_OPTION                                    => HELP_COMMAND . 'Option';
    # use constant HELP_COMMAND_CFGBLDOPT_DESCRIPTION                        => HELP_COMMAND_OPTION . 'Description';
    # use constant HELP_COMMAND_CFGBLDOPT_SUMMARY                            => HELP_COMMAND_OPTION . 'Summary';
    # use constant HELP_COMMAND_CFGBLDOPT_TOTAL                              => HELP_COMMAND_OPTION . 'Total';
    #
    # my $rhConfigHelpParam =
    # {
    #     &HELP_COMMAND_DESCRIPTION =>
    #     {
    #         &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID],
    #         &PARAM_ORDER => [PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_SUMMARY =>
    #     {
    #         &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID],
    #         &PARAM_ORDER => [PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_OPTION =>
    #     {
    #         &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    #         &PARAM_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_CFGBLDOPT_DESCRIPTION =>
    #     {
    #         &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    #         &PARAM_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_CFGBLDOPT_SUMMARY =>
    #     {
    #         &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    #         &PARAM_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_CFGBLDOPT_TOTAL =>
    #     {
    #         &CFGBLDOPT_RULE_TYPE => CGEN_DATATYPE_INT32,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID],
    #         &PARAM_ORDER => [PARAM_COMMAND_ID],
    #     },
    # };

    return $rhBuild;
}

push @EXPORT, qw(buildConfig);

1;
