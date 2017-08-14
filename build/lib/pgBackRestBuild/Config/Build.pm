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
use File::Basename qw(dirname);
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
use constant VALUE_MAP                                              => 'value-map';

use constant CFGBLDDEF_RULE_ALLOW_LIST                              => CFGBLDDEF_RULE_ALLOW_LIST;
use constant CFGBLDDEF_RULE_ALLOW_LIST_VALUE                        => CFGBLDDEF_RULE_ALLOW_LIST . '-value';
use constant CFGBLDDEF_RULE_ALLOW_LIST_VALUE_TOTAL                  => CFGBLDDEF_RULE_ALLOW_LIST_VALUE . '-total';
use constant CFGBLDDEF_RULE_ALLOW_RANGE_MIN                         => CFGBLDDEF_RULE_ALLOW_RANGE . '-min';
use constant CFGBLDDEF_RULE_ALLOW_RANGE_MAX                         => CFGBLDDEF_RULE_ALLOW_RANGE . '-max';
use constant CFGBLDDEF_RULE_DEPEND                                  => CFGBLDDEF_RULE_DEPEND;
use constant CFGBLDDEF_RULE_DEPEND_VALUE                            => CFGBLDDEF_RULE_DEPEND . '-value';
use constant CFGBLDDEF_RULE_DEPEND_VALUE_TOTAL                      => CFGBLDDEF_RULE_DEPEND_VALUE . '-total';
use constant CFGBLDDEF_RULE_NAME                                    => 'name';
use constant CFGBLDDEF_RULE_VALID                                   => 'valid';
use constant PARAM_SUMMARY                                          => 'summary';

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
    &CFGBLDDEF_RULE_ALLOW_LIST =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowList',
        &PARAM_SUMMARY => 'is there an allow list for this option?',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },

    &CFGBLDDEF_RULE_ALLOW_LIST_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowListValue',
        &PARAM_SUMMARY => 'get value from allowed list',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID],
    },

    &CFGBLDDEF_RULE_ALLOW_LIST_VALUE_TOTAL =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowListValueTotal',
        &PARAM_SUMMARY => 'total number of values allowed',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_ALLOW_RANGE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowRange',
        &PARAM_SUMMARY => 'is the option constrained to a range?',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },

    &CFGBLDDEF_RULE_ALLOW_RANGE_MIN =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowRangeMin',
        &PARAM_SUMMARY => 'minimum value allowed (if the option is constrained to a range)',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_DOUBLE,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_ALLOW_RANGE_MAX =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'AllowRangeMax',
        &PARAM_SUMMARY => 'maximum value allowed (if the option is constrained to a range)',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_DOUBLE,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_DEFAULT =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Default',
        &PARAM_SUMMARY => 'default value',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => undef,
    },

    &CFGBLDDEF_RULE_INDEX =>
    {
        &PARAM_FILE => FILE_CONFIG,
        &CFGBLDDEF_RULE_NAME => 'IndexTotal',
        &PARAM_SUMMARY => 'total index values allowed',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => 1,
    },

    &CFGBLDDEF_RULE_DEPEND =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Depend',
        &PARAM_SUMMARY => 'does the option have a dependency on another option?',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },

    &CFGBLDDEF_RULE_DEPEND_OPTION =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'DependOption',
        &PARAM_SUMMARY => 'name of the option that this option depends in order to be set',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &VALUE_MAP => $rhOptionIdConstantMap,
    },

    &CFGBLDDEF_RULE_DEPEND_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'DependValue',
        &PARAM_SUMMARY => 'the depend option must have one of these values before this option is set',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID, PARAM_VALUE_ID],
    },

    &CFGBLDDEF_RULE_DEPEND_VALUE_TOTAL =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'DependValueTotal',
        &PARAM_SUMMARY => 'total depend values for this option',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
    },

    &CFGBLDDEF_RULE_HINT =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Hint',
        &PARAM_SUMMARY => 'some clue as to what value the user should provide when the option is missing but required',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => undef,
    },

    &CFGBLDDEF_RULE_ALT_NAME =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'NameAlt',
        &PARAM_SUMMARY => 'alternate name for the option primarily used for deprecated names',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => undef,
    },

    &CFGBLDDEF_RULE_NEGATE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Negate',
        &PARAM_SUMMARY => 'can the boolean option be negated?',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },

    &CFGBLDDEF_RULE_PREFIX =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Prefix',
        &PARAM_SUMMARY => 'prefix when the option has an index > 1 (e.g. "db")',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => undef,
    },

    &CFGBLDDEF_RULE_REQUIRED =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Required',
        &PARAM_SUMMARY => 'is the option required?',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },

    &CFGBLDDEF_RULE_SECTION =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Section',
        &PARAM_SUMMARY => 'section that the option belongs in, NULL means command-line only',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_CONSTCHAR,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => undef,
    },

    &CFGBLDDEF_RULE_SECURE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Secure',
        &PARAM_SUMMARY => 'secure options can never be passed on the commmand-line',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },

    &CFGBLDDEF_RULE_TYPE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Type',
        &PARAM_SUMMARY => 'secure options can never be passed on the commmand-line',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_INT32,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &VALUE_MAP => $rhOptionTypeIdConstantMap,
    },

    &CFGBLDDEF_RULE_VALID =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'Valid',
        &PARAM_SUMMARY => 'is the option valid for this command?',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_COMMAND_ID, PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },

    &CFGBLDDEF_RULE_HASH_VALUE =>
    {
        &PARAM_FILE => FILE_CONFIG_RULE,
        &CFGBLDDEF_RULE_NAME => 'ValueHash',
        &PARAM_SUMMARY => 'is the option a true hash or just a list of keys?',
        &CFGBLDDEF_RULE_TYPE => CGEN_DATATYPE_BOOL,
        &PARAM_ORDER => [PARAM_CFGBLDOPT_ID],
        &BLD_TRUTH_DEFAULT => false,
    },
};

####################################################################################################################################
# Option rule help functions
####################################################################################################################################
sub optionRule
{
    my $strRule = shift;
    my $strValue = shift;
    my $iCommandId = shift;
    my $iOptionId = shift;

    # return if defined($iCommandId) && !defined($strValue);

    push(
        @{$rhOptionRuleParam->{$strRule}{matrix}},
        [$iCommandId, $iOptionId, $strValue]);
}

sub optionRuleDepend
{
    my $rhDepend = shift;
    my $iCommandId = shift;
    my $iOptionId = shift;

    my $strDependOption = $rhDepend->{&CFGBLDDEF_RULE_DEPEND_OPTION};

    optionRule(CFGBLDDEF_RULE_DEPEND, defined($strDependOption) ? true : false, $iCommandId, $iOptionId);

    if (defined($strDependOption))
    {
        optionRule(CFGBLDDEF_RULE_DEPEND_OPTION, $rhOptionNameIdMap->{$strDependOption}, $iCommandId, $iOptionId);

        my $iValueTotal = 0;

        if (defined($rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}))
        {
            foreach my $strValue (@{$rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}})
            {
                push(
                    @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_DEPEND_VALUE}{matrix}},
                    [$iCommandId, $iOptionId, $iValueTotal, $strValue]);

                $iValueTotal++;
            }
        }

        optionRule(CFGBLDDEF_RULE_DEPEND_VALUE_TOTAL, $iValueTotal, $iCommandId, $iOptionId);
    }
}

sub optionRuleRange
{
    my $riyRange = shift;
    my $iCommandId = shift;
    my $iOptionId = shift;

    optionRule(
        CFGBLDDEF_RULE_ALLOW_RANGE, defined($riyRange) ? true : false, $iCommandId, $iOptionId);

    if (defined($riyRange))
    {
        optionRule(CFGBLDDEF_RULE_ALLOW_RANGE_MIN, $riyRange->[0], $iCommandId, $iOptionId);
        optionRule(CFGBLDDEF_RULE_ALLOW_RANGE_MAX, $riyRange->[1], $iCommandId, $iOptionId);
    }
}

sub optionRuleAllowList
{
    my $rhAllowList = shift;
    my $iCommandId = shift;
    my $iOptionId = shift;

    optionRule(
        CFGBLDDEF_RULE_ALLOW_LIST, defined($rhAllowList) ? true : false, $iCommandId, $iOptionId);

    if (defined($rhAllowList))
    {
        my $iValueTotal = 0;

        foreach my $strValue (@{$rhAllowList})
        {
            push(
                @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_ALLOW_LIST_VALUE}{matrix}},
                [$iCommandId, $iOptionId, $iValueTotal, $strValue]);

            $iValueTotal++;
        }

        optionRule(CFGBLDDEF_RULE_ALLOW_LIST_VALUE_TOTAL, $iValueTotal, $iCommandId, $iOptionId);
    }
}

####################################################################################################################################
# Build configuration functions and constants
####################################################################################################################################
sub buildConfig
{
    my $rhBuild;

    # Create C header with #defines for constants
    #-------------------------------------------------------------------------------------------------------------------------------
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

    # Build config rule maps used to create functions
    #-------------------------------------------------------------------------------------------------------------------------------
    foreach my $strOption (sort(keys(%{$rhOptionRule})))
    {
        my $rhOption = $rhOptionRule->{$strOption};
        my $iOptionId = $rhOptionNameIdMap->{$strOption};

        foreach my $strCommand (sort(keys(%{cfgbldCommandGet()})))
        {
            my $rhCommand = $rhOption->{&CFGBLDDEF_RULE_COMMAND}{$strCommand};
            my $iCommandId = $rhCommandNameIdMap->{$strCommand};

            optionRule(CFGBLDDEF_RULE_VALID, defined($rhCommand) ? true : false, $iCommandId, $iOptionId);

            if (defined($rhCommand))
            {
                optionRule(
                    CFGBLDDEF_RULE_REQUIRED,
                    coalesce($rhCommand->{&CFGBLDDEF_RULE_REQUIRED}, $rhOption->{&CFGBLDDEF_RULE_REQUIRED}, true), $iCommandId,
                    $iOptionId);

                optionRule(
                    CFGBLDDEF_RULE_DEFAULT,
                    defined($rhCommand->{&CFGBLDDEF_RULE_DEFAULT}) ?
                        $rhCommand->{&CFGBLDDEF_RULE_DEFAULT} : $rhOption->{&CFGBLDDEF_RULE_DEFAULT},
                    $iCommandId, $iOptionId);
                optionRule(
                    CFGBLDDEF_RULE_HINT,
                        defined($rhCommand->{&CFGBLDDEF_RULE_HINT}) ?
                            $rhCommand->{&CFGBLDDEF_RULE_HINT} : $rhOption->{&CFGBLDDEF_RULE_HINT},
                    $iCommandId, $iOptionId);
                optionRuleDepend(
                    defined($rhCommand->{&CFGBLDDEF_RULE_DEPEND}) ?
                        $rhCommand->{&CFGBLDDEF_RULE_DEPEND} : $rhOption->{&CFGBLDDEF_RULE_DEPEND},
                    $iCommandId, $iOptionId);
                optionRuleRange(
                    defined($rhCommand->{&CFGBLDDEF_RULE_ALLOW_RANGE}) ?
                        $rhCommand->{&CFGBLDDEF_RULE_ALLOW_RANGE} : $rhOption->{&CFGBLDDEF_RULE_ALLOW_RANGE},
                    $iCommandId, $iOptionId);
                optionRuleAllowList(
                    defined($rhCommand->{&CFGBLDDEF_RULE_ALLOW_LIST}) ?
                        $rhCommand->{&CFGBLDDEF_RULE_ALLOW_LIST} : $rhOption->{&CFGBLDDEF_RULE_ALLOW_LIST},
                    $iCommandId, $iOptionId);
            }
        }

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_INDEX}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_INDEX}]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_NEGATE}{matrix}},
            [$iOptionId, coalesce($rhOption->{&CFGBLDDEF_RULE_NEGATE}, false)]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_ALT_NAME}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_ALT_NAME}]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_PREFIX}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_PREFIX}]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_SECTION}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_SECTION}]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_SECURE}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_SECURE}]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_TYPE}{matrix}},
            [$iOptionId, $rhOptionTypeNameIdMap->{$rhOption->{&CFGBLDDEF_RULE_TYPE}}]);

        push(
            @{$rhOptionRuleParam->{&CFGBLDDEF_RULE_HASH_VALUE}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_HASH_VALUE}]);
    }

    my $rhLabelMap = {&PARAM_CFGBLDOPT_ID => $rhOptionIdConstantMap, &PARAM_COMMAND_ID => $rhCommandIdConstantMap};

    # Build truth table
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strTruthTable;

    foreach my $strOptionRule (sort(keys(%{$rhOptionRuleParam})))
    {
        my $rhOptionRule = $rhOptionRuleParam->{$strOptionRule};
        my $strPrefix = 'cfgOption' . ($rhOptionRule->{&PARAM_FILE} eq FILE_CONFIG_RULE ? 'Rule' : '');
        my $strFunctionName = $strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME};

        my $strSummary = ucfirst($rhOptionRule->{&PARAM_SUMMARY});
        $strSummary .= $strSummary =~ /\?$/ ? '' : '.';

        $strTruthTable .=
            (defined($strTruthTable) ? "\n" : '') .
            "## ${strFunctionName}\n\n" .
            "${strSummary}\n\n" .
            "### Truth Table:\n\n";

        my $strTruthDefault;

        if (exists($rhOptionRule->{&BLD_TRUTH_DEFAULT}))
        {
            $strTruthDefault =
                defined($rhOptionRule->{&BLD_TRUTH_DEFAULT}) ? $rhOptionRule->{&BLD_TRUTH_DEFAULT} : CGEN_DATAVAL_NULL;

            $strTruthTable .=
                'Functions that return `' .
                cgenTypeFormat(
                    $rhOptionRule->{&CFGBLDDEF_RULE_TYPE},
                    defined($rhOptionRule->{&VALUE_MAP}) && defined($rhOptionRule->{&VALUE_MAP}->{$strTruthDefault}) ?
                        $rhOptionRule->{&VALUE_MAP}->{$strTruthDefault} : $strTruthDefault) .
                    "` are not listed here for brevity.\n\n";
        }

        # Write table header
        $strTruthTable .= '| Suffix';

        for (my $iParamIdx = 0; $iParamIdx < @{$rhOptionRule->{&PARAM_ORDER}}; $iParamIdx++)
        {
            $strTruthTable .= ' | ' . $rhOptionRule->{&PARAM_ORDER}->[$iParamIdx];
        }

        $strTruthTable .=
            " | Result |\n" .
            '| ------';

        for (my $iParamIdx = 0; $iParamIdx < @{$rhOptionRule->{&PARAM_ORDER}}; $iParamIdx++)
        {
            $strTruthTable .= ' | ' . ('-' x length($rhOptionRule->{&PARAM_ORDER}->[$iParamIdx]));
        }

        $strTruthTable .=
            " | ------ |\n";

        # Format matrix data so that can be ordered (even by integer)
        my @stryOrderedMatrix;

        foreach my $rxyEntry (@{$rhOptionRule->{matrix}})
        {
            my $strEntry;

            for (my $iEntryIdx = 0; $iEntryIdx < @{$rxyEntry}; $iEntryIdx++)
            {
                my $strValue = defined($rxyEntry->[$iEntryIdx]) ? $rxyEntry->[$iEntryIdx] : CGEN_DATAVAL_NULL;

                if ($iEntryIdx != @{$rxyEntry} - 1)
                {
                    $strEntry .= (defined($strEntry) ? '~' : '') . sprintf('%016d', $strValue);
                }
                else
                {
                    $strEntry .= '~' . $strValue;
                }
            }

            push(@stryOrderedMatrix, $strEntry);
        }

        # Commands are frequently redundant, so if command is present (as first param) then remove it when redundant
        my $rhMatrixFilter;
        my @stryFilteredMatrix;

        if ($rhOptionRule->{&PARAM_ORDER}->[0] eq PARAM_COMMAND_ID)
        {
            foreach my $strEntry (sort(@stryOrderedMatrix))
            {
                my @xyEntry = split('\~', $strEntry);

                shift(@xyEntry);
                my $strValue = pop(@xyEntry);
                my $strKey = join('~', @xyEntry);

                push(@{$rhMatrixFilter->{$strKey}{entry}}, $strEntry);
                $rhMatrixFilter->{$strKey}{value}{$strValue} = true;
            }

            foreach my $strKey (sort(keys(%{$rhMatrixFilter})))
            {
                if (keys(%{$rhMatrixFilter->{$strKey}{value}}) == 1)
                {
                    my $strEntry =

                    push(@stryFilteredMatrix, CGEN_DATAVAL_NULL . "~${strKey}~" . (keys(%{$rhMatrixFilter->{$strKey}{value}}))[0]);
                }
                else
                {
                    push(@stryFilteredMatrix, @{$rhMatrixFilter->{$strKey}{entry}});
                }
            }
        }
        else
        {
            @stryFilteredMatrix = @stryOrderedMatrix;
        }

        # Output function entry
        foreach my $strEntry (sort(@stryFilteredMatrix))
        {
            my @xyEntry = split('\~', $strEntry);
            my $strRow = '| ' . $rhOptionRule->{&CFGBLDDEF_RULE_NAME};
            my $strValue;

            for (my $iEntryIdx = 0; $iEntryIdx < @xyEntry; $iEntryIdx++)
            {
                $strValue = $xyEntry[$iEntryIdx];
                $strRow .= ' | ';

                if ($iEntryIdx != @xyEntry - 1)
                {
                    my $strParam = $rhOptionRule->{&PARAM_ORDER}->[$iEntryIdx];

                    if ($strValue eq CGEN_DATAVAL_NULL)
                    {
                        $strRow .= '_\<ANY\>_';
                    }
                    else
                    {
                        $strRow .=
                            '`' .
                            (defined($rhLabelMap->{$strParam}) ? $rhLabelMap->{$strParam}{int($strValue)} : int($strValue)) .
                            '`';
                    }
                }
                else
                {
                    $strRow .=
                        '`' .
                        cgenTypeFormat(
                            $rhOptionRule->{&CFGBLDDEF_RULE_TYPE},
                            defined($rhOptionRule->{&VALUE_MAP}) && defined($rhOptionRule->{&VALUE_MAP}->{$strValue}) ?
                                $rhOptionRule->{&VALUE_MAP}->{$strValue} : $strValue) .
                        '`';

                }
            }

            if (defined($strTruthDefault) && $strTruthDefault eq $strValue)
            {
                next;
            }

            $strTruthTable .= "${strRow} |\n";
        }
    }

    $rhBuild->{&FILE_CONFIG}{&BLD_MD}{&BLD_SOURCE} = $strTruthTable;

    # Output config rule functions
    #-------------------------------------------------------------------------------------------------------------------------------
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
            cgenFunction($strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, $rhOptionRule->{&PARAM_SUMMARY}, undef,
                cgenSwitchBuild(
                    $strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, $rhOptionRule->{&CFGBLDDEF_RULE_TYPE},
                    $rhOptionRule->{matrix}, $rhOptionRule->{&PARAM_ORDER}, $rhLabelMap, $rhOptionRule->{&VALUE_MAP}));
    }

    $rhBuild->{&FILE_CONFIG_RULE}{&BLD_C}{&BLD_SOURCE} = $strConfigFunction;

    # Output config functions
    #-------------------------------------------------------------------------------------------------------------------------------
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
            cgenFunction($strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, $rhOptionRule->{&PARAM_SUMMARY}, undef,
                cgenSwitchBuild(
                    $strPrefix . $rhOptionRule->{&CFGBLDDEF_RULE_NAME}, $rhOptionRule->{&CFGBLDDEF_RULE_TYPE},
                    $rhOptionRule->{matrix}, $rhOptionRule->{&PARAM_ORDER}, $rhLabelMap, $rhOptionRule->{&VALUE_MAP}));
    }

    $rhBuild->{&FILE_CONFIG}{&BLD_C}{&BLD_SOURCE} = $strConfigFunction;

    return $rhBuild;
}

push @EXPORT, qw(buildConfig);

1;
