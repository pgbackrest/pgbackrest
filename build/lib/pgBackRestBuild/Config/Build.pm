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

use constant CFGBLDDEF_RULE_ALLOW_LIST_VALUE                           => CFGBLDDEF_RULE_ALLOW_LIST . '-value';
use constant CFGBLDDEF_RULE_ALLOW_LIST_VALUE_TOTAL                     => CFGBLDDEF_RULE_ALLOW_LIST_VALUE . '-total';
use constant CFGBLDDEF_RULE_ALLOW_RANGE_MIN                            => CFGBLDDEF_RULE_ALLOW_RANGE . '-min';
use constant CFGBLDDEF_RULE_ALLOW_RANGE_MAX                            => CFGBLDDEF_RULE_ALLOW_RANGE . '-max';
use constant CFGBLDDEF_RULE_DEPEND_VALUE_TOTAL                         => CFGBLDDEF_RULE_DEPEND_VALUE . '-total';
use constant CFGBLDDEF_RULE_NAME                                       => 'name';
use constant CFGBLDDEF_RULE_VALID                                      => 'valid';

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

foreach my $strCommand (sort(keys(%{cfgbldCommandGet()})))
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
my $rhOptionRule = cfgbldOptionRuleGet();
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
    my $strOptionAlt = $rhOption->{&CFGBLDDEF_RULE_ALT_NAME};

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
    my $strOptionType = $rhOptionRule->{$strOption}{&CFGBLDDEF_RULE_TYPE};

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
    &CFGBLDDEF_RULE_ALLOW_LIST_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowListValue',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID],
    },

    &CFGBLDDEF_RULE_ALLOW_LIST_VALUE_TOTAL =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowListValueTotal',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_ALLOW_RANGE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowRange',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_ALLOW_RANGE_MIN =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowRangeMin',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_DOUBLE,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_ALLOW_RANGE_MAX =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowRangeMax',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_DOUBLE,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    # !!! NEED TO GUARANTEE THAT THIS RETURNS NULL
    &CFGBLDDEF_RULE_DEFAULT =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Default',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_INDEX =>
    {
        &PARAM_FILE => FILE_CONFIG,
        &CFGBLDDEF_RULE_NAME => 'IndexTotal',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_DEPEND_OPTION =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'DependOption',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &VALUE_MAP => $rhOptionIdConstantMap,
    },

    &CFGBLDDEF_RULE_DEPEND_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'DependValue',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID],
    },

    &CFGBLDDEF_RULE_DEPEND_VALUE_TOTAL =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'DependValueTotal',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_HINT =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Hint',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_ALT_NAME =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'NameAlt',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_NEGATE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Negate',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_PREFIX =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Prefix',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_REQUIRED =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Required',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_SECTION =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Section',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_SECURE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Secure',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_TYPE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Type',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID],
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &VALUE_MAP => $rhOptionTypeIdConstantMap,
    },

    &CFGBLDDEF_RULE_VALID =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Valid',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &SWITCH_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_HASH_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'ValueHash',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
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

    my $strDependOption = $rhDepend->{&CFGBLDDEF_RULE_DEPEND_OPTION};

    optionRule(
        CFGBLDDEF_RULE_DEPEND_OPTION,
        defined($strDependOption) ? $rhOptionNameIdMap->{$strDependOption} : (defined($iCommandId) ? undef : -1),
        $iOptionId, $iCommandId);

    my $iValueTotal = 0;

    if (defined($rhDepend->{&CFGBLDDEF_RULE_DEPEND_VALUE}))
    {
        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_DEPEND_VALUE}{matrix}},
            [$rhDepend->{&CFGBLDDEF_RULE_DEPEND_VALUE}, $iOptionId, $iValueTotal, $iCommandId]);

        $iValueTotal++;
    }
    elsif (defined($rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}))
    {
        foreach my $strValue (sort(keys(%{$rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}})))
        {
            push(
                @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_DEPEND_VALUE}{matrix}},
                [$strValue, $iOptionId, $iValueTotal, $iCommandId]);

            $iValueTotal++;
        }
    }
    else
    {
        push(@{$rhOptionRuleParam->{&CFGBLDDEF_RULE_DEPEND_VALUE}{matrix}}, [undef, $iOptionId, $iValueTotal, $iCommandId]);
    }

    optionRule(CFGBLDDEF_RULE_DEPEND_VALUE_TOTAL, $iValueTotal, $iOptionId, $iCommandId);
}

sub optionRuleRange
{
    my $riyRange = shift;
    my $iOptionId = shift;
    my $iCommandId = shift;

    optionRule(
        CFGBLDDEF_RULE_ALLOW_RANGE, defined($riyRange) ? true : (defined($iCommandId) ? undef : false), $iOptionId, $iCommandId);

    if (defined($riyRange))
    {
        optionRule(CFGBLDDEF_RULE_ALLOW_RANGE_MIN, $riyRange->[0], $iOptionId, $iCommandId);
        optionRule(CFGBLDDEF_RULE_ALLOW_RANGE_MAX, $riyRange->[1], $iOptionId, $iCommandId);
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
                @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_ALLOW_LIST_VALUE}{matrix}},
                [$strValue, $iOptionId, $iValueTotal, $iCommandId]);

            $iValueTotal++;
        }
    }
    else
    {
        push(@{$rhOptionRuleParam->{&CFGBLDDEF_RULE_ALLOW_LIST_VALUE}{matrix}}, [undef, $iOptionId, $iValueTotal, $iCommandId]);
    }

    optionRule(CFGBLDDEF_RULE_ALLOW_LIST_VALUE_TOTAL, $iValueTotal, $iOptionId, $iCommandId);
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

        foreach my $strCommand (sort(keys(%{cfgbldCommandGet()})))
        {
            my $rhCommand = $rhOption->{&CFGBLDDEF_RULE_COMMAND}{$strCommand};
            my $iCommandId = $rhCommandNameIdMap->{$strCommand};

            if (defined($rhCommand))
            {
                optionRule(CFGBLDDEF_RULE_VALID, true, $iOptionId, $iCommandId);
                optionRule(CFGBLDDEF_RULE_REQUIRED, $rhCommand->{&CFGBLDDEF_RULE_REQUIRED}, $iOptionId, $iCommandId);
                optionRule(CFGBLDDEF_RULE_DEFAULT, $rhCommand->{&CFGBLDDEF_RULE_DEFAULT}, $iOptionId, $iCommandId);
                optionRule(CFGBLDDEF_RULE_HINT, $rhCommand->{&CFGBLDDEF_RULE_HINT}, $iOptionId, $iCommandId);
                optionRuleDepend($rhCommand->{&CFGBLDDEF_RULE_DEPEND}, $iOptionId, $iCommandId);
                optionRuleRange($rhCommand->{&CFGBLDDEF_RULE_ALLOW_RANGE}, $iOptionId, $iCommandId);
                optionRuleAllowList($rhCommand->{&CFGBLDDEF_RULE_ALLOW_LIST}, $iOptionId, $iCommandId);
            }
            else
            {
                optionRule(CFGBLDDEF_RULE_VALID, false, $iOptionId, $iCommandId);
            }
        }

        optionRule(CFGBLDDEF_RULE_REQUIRED, coalesce($rhOption->{&CFGBLDDEF_RULE_REQUIRED}, true), $iOptionId);
        optionRule(CFGBLDDEF_RULE_DEFAULT, $rhOption->{&CFGBLDDEF_RULE_DEFAULT}, $iOptionId);
        optionRule(CFGBLDDEF_RULE_HINT, $rhOption->{&CFGBLDDEF_RULE_HINT}, $iOptionId);
        optionRuleDepend($rhOption->{&CFGBLDDEF_RULE_DEPEND}, $iOptionId);
        optionRuleRange($rhOption->{&CFGBLDDEF_RULE_ALLOW_RANGE}, $iOptionId);
        optionRuleAllowList($rhOption->{&CFGBLDDEF_RULE_ALLOW_LIST}, $iOptionId);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_INDEX}{matrix}},
            [$rhOption->{&CFGBLDDEF_RULE_INDEX}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_NEGATE}{matrix}},
            [coalesce($rhOption->{&CFGBLDDEF_RULE_NEGATE}, false), $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_ALT_NAME}{matrix}},
            [$rhOption->{&CFGBLDDEF_RULE_ALT_NAME}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_PREFIX}{matrix}},
            [$rhOption->{&CFGBLDDEF_RULE_PREFIX}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_SECTION}{matrix}},
            [$rhOption->{&CFGBLDDEF_RULE_SECTION}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_SECURE}{matrix}},
            [$rhOption->{&CFGBLDDEF_RULE_SECURE}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_TYPE}{matrix}},
            [$rhOptionTypeNameIdMap->{$rhOption->{&CFGBLDDEF_RULE_TYPE}}, $iOptionId]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_HASH_VALUE}{matrix}},
            [$rhOption->{&CFGBLDDEF_RULE_HASH_VALUE}, $iOptionId]);
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
            cgenFunction($strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, undef, undef,
                cgenSwitchBuild(
                    $strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, $rhOptionRule->{&CFGBLDDEF_RULE_TYPE},
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
            cgenFunction($strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, undef, undef,
                cgenSwitchBuild(
                    $strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, $rhOptionRule->{&CFGBLDDEF_RULE_TYPE},
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
    #         &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID],
    #         &PARAM_ORDER => [PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_SUMMARY =>
    #     {
    #         &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID],
    #         &PARAM_ORDER => [PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_OPTION =>
    #     {
    #         &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    #         &PARAM_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_CFGBLDOPT_DESCRIPTION =>
    #     {
    #         &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    #         &PARAM_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_CFGBLDOPT_SUMMARY =>
    #     {
    #         &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    #         &PARAM_ORDER => [PARAM_CFGBLDOPT_ID, PARAM_COMMAND_ID],
    #     },
    #
    #     &HELP_COMMAND_CFGBLDOPT_TOTAL =>
    #     {
    #         &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
    #         &SWITCH_ORDER => [PARAM_COMMAND_ID],
    #         &PARAM_ORDER => [PARAM_COMMAND_ID],
    #     },
    # };

    return $rhBuild;
}

push @EXPORT, qw(buildConfig);

1;
