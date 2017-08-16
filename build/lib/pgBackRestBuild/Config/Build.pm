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
use constant BLDLCL_FILE_CONFIG                                     => 'config';
use constant BLDLCL_FILE_CONFIG_RULE                                => BLDLCL_FILE_CONFIG . 'Rule';

use constant BLDLCL_PARAM_OPTIONID                                  => 'uiOptionId';
use constant BLDLCL_PARAM_COMMANDID                                 => 'uiCommandId';
use constant BLDLCL_PARAM_VALUEID                                   => 'uiValueId';

use constant BLDLCL_PREFIX_OPTION                                   => 'cfgOption';

use constant BLDLCL_FUNCTION_INDEX_TOTAL                            => BLDLCL_PREFIX_OPTION . 'IndexTotal';

use constant BLDLCL_PREFIX_OPTION_RULE                              => 'cfgOptionRule';

use constant BLDLCL_FUNCTION_ALLOW_LIST                             => BLDLCL_PREFIX_OPTION_RULE . 'AllowList';
use constant BLDLCL_FUNCTION_ALLOW_LIST_VALUE                       => BLDLCL_FUNCTION_ALLOW_LIST . 'Value';
use constant BLDLCL_FUNCTION_ALLOW_LIST_VALUE_TOTAL                 => BLDLCL_FUNCTION_ALLOW_LIST_VALUE . 'Total';

use constant BLDLCL_FUNCTION_ALLOW_RANGE                            => BLDLCL_PREFIX_OPTION_RULE . 'AllowRange';
use constant BLDLCL_FUNCTION_ALLOW_RANGE_MAX                        => BLDLCL_FUNCTION_ALLOW_RANGE . 'Max';
use constant BLDLCL_FUNCTION_ALLOW_RANGE_MIN                        => BLDLCL_FUNCTION_ALLOW_RANGE . 'Min';

use constant BLDLCL_FUNCTION_DEFAULT                                => BLDLCL_PREFIX_OPTION_RULE . "Default";

use constant BLDLCL_FUNCTION_DEPEND                                 => BLDLCL_PREFIX_OPTION_RULE . 'Depend';
use constant BLDLCL_FUNCTION_DEPEND_OPTION                          => BLDLCL_FUNCTION_DEPEND . 'Option';
use constant BLDLCL_FUNCTION_DEPEND_VALUE                           => BLDLCL_FUNCTION_DEPEND . 'Value';
use constant BLDLCL_FUNCTION_DEPEND_VALUE_TOTAL                     => BLDLCL_FUNCTION_DEPEND_VALUE . 'Total';

use constant BLDLCL_FUNCTION_HINT                                   => BLDLCL_PREFIX_OPTION_RULE . 'Hint';
use constant BLDLCL_FUNCTION_NAME_ALT                               => BLDLCL_PREFIX_OPTION_RULE . 'NameAlt';
use constant BLDLCL_FUNCTION_NEGATE                                 => BLDLCL_PREFIX_OPTION_RULE . 'Negate';
use constant BLDLCL_FUNCTION_PREFIX                                 => BLDLCL_PREFIX_OPTION_RULE . 'Prefix';
use constant BLDLCL_FUNCTION_REQUIRED                               => BLDLCL_PREFIX_OPTION_RULE . 'Required';
use constant BLDLCL_FUNCTION_SECTION                                => BLDLCL_PREFIX_OPTION_RULE . 'Section';
use constant BLDLCL_FUNCTION_SECURE                                 => BLDLCL_PREFIX_OPTION_RULE . 'Secure';
use constant BLDLCL_FUNCTION_TYPE                                   => BLDLCL_PREFIX_OPTION_RULE . 'Type';
use constant BLDLCL_FUNCTION_VALID                                  => BLDLCL_PREFIX_OPTION_RULE . 'Valid';
use constant BLDLCL_FUNCTION_VALUE_HASH                             => BLDLCL_PREFIX_OPTION_RULE . 'ValueHash';

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
my $rhCodeGen =
{
    &BLDLCL_FILE_CONFIG =>
    {
        &BLD_FUNCTION =>
        {
            &BLDLCL_FUNCTION_INDEX_TOTAL =>
            {
                &BLD_SUMMARY => 'total index values allowed',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_INT32,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => 1,
            },
        },
    },

    &BLDLCL_FILE_CONFIG_RULE =>
    {
        &BLD_FUNCTION =>
        {
            &BLDLCL_FUNCTION_ALLOW_LIST =>
            {
                &BLD_SUMMARY => 'is there an allow list for this option?',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },

            &BLDLCL_FUNCTION_ALLOW_LIST_VALUE =>
            {
                &BLD_SUMMARY => 'get value from allowed list',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_CONSTCHAR,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID, BLDLCL_PARAM_VALUEID],
            },

            &BLDLCL_FUNCTION_ALLOW_LIST_VALUE_TOTAL =>
            {
                &BLD_SUMMARY => 'total number of values allowed',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_INT32,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
            },

            &BLDLCL_FUNCTION_ALLOW_RANGE =>
            {
                &BLD_SUMMARY => 'is the option constrained to a range?',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },

            &BLDLCL_FUNCTION_ALLOW_RANGE_MIN =>
            {
                &BLD_SUMMARY => 'minimum value allowed (if the option is constrained to a range)',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_DOUBLE,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
            },

            &BLDLCL_FUNCTION_ALLOW_RANGE_MAX =>
            {
                &BLD_SUMMARY => 'maximum value allowed (if the option is constrained to a range)',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_DOUBLE,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
            },

            &BLDLCL_FUNCTION_DEFAULT =>
            {
                &BLD_SUMMARY => 'default value',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_CONSTCHAR,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => undef,
            },

            &BLDLCL_FUNCTION_DEPEND =>
            {
                &BLD_SUMMARY => 'does the option have a dependency on another option?',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },

            &BLDLCL_FUNCTION_DEPEND_OPTION =>
            {
                &BLD_SUMMARY => 'name of the option that this option depends in order to be set',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_INT32,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_RETURN_VALUE_MAP => $rhOptionIdConstantMap,
            },

            &BLDLCL_FUNCTION_DEPEND_VALUE =>
            {
                &BLD_SUMMARY => 'the depend option must have one of these values before this option is set',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_CONSTCHAR,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID, BLDLCL_PARAM_VALUEID],
            },

            &BLDLCL_FUNCTION_DEPEND_VALUE_TOTAL =>
            {
                &BLD_SUMMARY => 'total depend values for this option',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_INT32,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
            },

            &BLDLCL_FUNCTION_HINT =>
            {
                &BLD_SUMMARY => 'some clue as to what value the user should provide when the option is missing but required',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_CONSTCHAR,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => undef,
            },

            &BLDLCL_FUNCTION_NAME_ALT =>
            {
                &BLD_SUMMARY => 'alternate name for the option primarily used for deprecated names',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_CONSTCHAR,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => undef,
            },

            &BLDLCL_FUNCTION_NEGATE =>
            {
                &BLD_SUMMARY => 'can the boolean option be negated?',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },

            &BLDLCL_FUNCTION_PREFIX =>
            {
                &BLD_SUMMARY => 'prefix when the option has an index > 1 (e.g. "db")',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_CONSTCHAR,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => undef,
            },

            &BLDLCL_FUNCTION_REQUIRED =>
            {
                &BLD_SUMMARY => 'is the option required?',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },

            &BLDLCL_FUNCTION_SECTION =>
            {
                &BLD_SUMMARY => 'section that the option belongs in, NULL means command-line only',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_CONSTCHAR,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => undef,
            },

            &BLDLCL_FUNCTION_SECURE =>
            {
                &BLD_SUMMARY => 'secure options can never be passed on the commmand-line',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },

            &BLDLCL_FUNCTION_TYPE =>
            {
                &BLD_SUMMARY => 'secure options can never be passed on the commmand-line',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_INT32,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_RETURN_VALUE_MAP => $rhOptionTypeIdConstantMap,
            },

            &BLDLCL_FUNCTION_VALID =>
            {
                &BLD_SUMMARY => 'is the option valid for this command?',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_COMMANDID, BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },

            &BLDLCL_FUNCTION_VALUE_HASH =>
            {
                &BLD_SUMMARY => 'is the option a true hash or just a list of keys?',
                &BLD_RETURN_TYPE => CGEN_DATATYPE_BOOL,
                &BLD_PARAM => [BLDLCL_PARAM_OPTIONID],
                &BLD_TRUTH_DEFAULT => false,
            },
        },
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
        @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{$strRule}{matrix}},
        [$iCommandId, $iOptionId, $strValue]);
}

sub optionRuleDepend
{
    my $rhDepend = shift;
    my $iCommandId = shift;
    my $iOptionId = shift;

    my $strDependOption = $rhDepend->{&CFGBLDDEF_RULE_DEPEND_OPTION};

    optionRule(BLDLCL_FUNCTION_DEPEND, defined($strDependOption) ? true : false, $iCommandId, $iOptionId);

    if (defined($strDependOption))
    {
        optionRule(BLDLCL_FUNCTION_DEPEND_OPTION, $rhOptionNameIdMap->{$strDependOption}, $iCommandId, $iOptionId);

        my $iValueTotal = 0;

        if (defined($rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}))
        {
            foreach my $strValue (@{$rhDepend->{&CFGBLDDEF_RULE_DEPEND_LIST}})
            {
                push(
                    @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_DEPEND_VALUE}{matrix}},
                    [$iCommandId, $iOptionId, $iValueTotal, $strValue]);

                $iValueTotal++;
            }
        }

        optionRule(BLDLCL_FUNCTION_DEPEND_VALUE_TOTAL, $iValueTotal, $iCommandId, $iOptionId);
    }
}

sub optionRuleRange
{
    my $riyRange = shift;
    my $iCommandId = shift;
    my $iOptionId = shift;

    optionRule(
        BLDLCL_FUNCTION_ALLOW_RANGE, defined($riyRange) ? true : false, $iCommandId, $iOptionId);

    if (defined($riyRange))
    {
        optionRule(BLDLCL_FUNCTION_ALLOW_RANGE_MIN, $riyRange->[0], $iCommandId, $iOptionId);
        optionRule(BLDLCL_FUNCTION_ALLOW_RANGE_MAX, $riyRange->[1], $iCommandId, $iOptionId);
    }
}

sub optionRuleAllowList
{
    my $rhAllowList = shift;
    my $iCommandId = shift;
    my $iOptionId = shift;

    optionRule(
        BLDLCL_FUNCTION_ALLOW_LIST, defined($rhAllowList) ? true : false, $iCommandId, $iOptionId);

    if (defined($rhAllowList))
    {
        my $iValueTotal = 0;

        foreach my $strValue (@{$rhAllowList})
        {
            push(
                @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_ALLOW_LIST_VALUE}{matrix}},
                [$iCommandId, $iOptionId, $iValueTotal, $strValue]);

            $iValueTotal++;
        }

        optionRule(BLDLCL_FUNCTION_ALLOW_LIST_VALUE_TOTAL, $iValueTotal, $iCommandId, $iOptionId);
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
            $rhBuild->{&BLDLCL_FILE_CONFIG}{&BLD_HEADER}{&BLD_CONSTANT}{$strConstant} = $strConstantValue;
        }

        $strConstantHeader .=  "\n#define CFGOPTRULE_TYPE_TOTAL ${iOptionTypeTotal}\n\n";

        # Add option type constants
        foreach my $strOptionType (sort(keys(%{$rhOptionTypeNameConstantMap})))
        {
            my $strConstant = $rhOptionTypeNameConstantMap->{$strOptionType};
            my $strConstantValue = $rhOptionTypeNameIdMap->{$strOptionType};

            $strConstantHeader .= "#define ${strConstant} ${strConstantValue}\n";
            $rhBuild->{&BLDLCL_FILE_CONFIG}{&BLD_HEADER}{&BLD_CONSTANT}{$strConstant} = $strConstantValue;
        }

        $strConstantHeader .=  "\n#define CONFIG_OPTION_TOTAL ${iOptionTotal}\n\n";

        # Add option constants
        foreach my $strOption (sort(keys(%{$rhOptionNameConstantMap})))
        {
            my $strConstant = $rhOptionNameConstantMap->{$strOption};
            my $strConstantValue = $rhOptionNameIdMap->{$strOption};

            $strConstantHeader .= "#define ${strConstant} ${strConstantValue}\n";
            $rhBuild->{&BLDLCL_FILE_CONFIG}{&BLD_HEADER}{&BLD_CONSTANT}{$strConstant} = $strConstantValue;
        }

        $rhBuild->{&BLDLCL_FILE_CONFIG}{&BLD_HEADER}{&BLD_SOURCE} =
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

            optionRule(BLDLCL_FUNCTION_VALID, defined($rhCommand) ? true : false, $iCommandId, $iOptionId);

            if (defined($rhCommand))
            {
                optionRule(
                    BLDLCL_FUNCTION_REQUIRED,
                    coalesce($rhCommand->{&CFGBLDDEF_RULE_REQUIRED}, $rhOption->{&CFGBLDDEF_RULE_REQUIRED}, true), $iCommandId,
                    $iOptionId);

                optionRule(
                    BLDLCL_FUNCTION_DEFAULT,
                    defined($rhCommand->{&CFGBLDDEF_RULE_DEFAULT}) ?
                        $rhCommand->{&CFGBLDDEF_RULE_DEFAULT} : $rhOption->{&CFGBLDDEF_RULE_DEFAULT},
                    $iCommandId, $iOptionId);
                optionRule(
                    BLDLCL_FUNCTION_HINT,
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
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_INDEX_TOTAL}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_INDEX}]);

        push(
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_NEGATE}{matrix}},
            [$iOptionId, coalesce($rhOption->{&CFGBLDDEF_RULE_NEGATE}, false)]);

        push(
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_NAME_ALT}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_ALT_NAME}]);

        push(
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_PREFIX}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_PREFIX}]);

        push(
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_SECTION}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_SECTION}]);

        push(
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_SECURE}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_SECURE}]);

        push(
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_TYPE}{matrix}},
            [$iOptionId, $rhOptionTypeNameIdMap->{$rhOption->{&CFGBLDDEF_RULE_TYPE}}]);

        push(
            @{$rhCodeGen->{&BLDLCL_FILE_CONFIG_RULE}{&BLD_FUNCTION}{&BLDLCL_FUNCTION_VALUE_HASH}{matrix}},
            [$iOptionId, $rhOption->{&CFGBLDDEF_RULE_HASH_VALUE}]);
    }

    my $rhLabelMap = {&BLDLCL_PARAM_OPTIONID => $rhOptionIdConstantMap, &BLDLCL_PARAM_COMMANDID => $rhCommandIdConstantMap};

    # Iterate all files
    #-------------------------------------------------------------------------------------------------------------------------------
    foreach my $strFile (sort(keys(%{$rhCodeGen})))
    {
        # Build truth table
        #-------------------------------------------------------------------------------------------------------------------------------
        my $strTruthTable;

        foreach my $strFunction (sort(keys(%{$rhCodeGen->{$strFile}{&BLD_FUNCTION}})))
        {
            my $rhOptionRule = $rhCodeGen->{$strFile}{&BLD_FUNCTION}{$strFunction};

            my $strSummary = ucfirst($rhOptionRule->{&BLD_SUMMARY});
            $strSummary .= $strSummary =~ /\?$/ ? '' : '.';

            $strTruthTable .=
                (defined($strTruthTable) ? "\n" : '') .
                "## ${strFunction}\n\n" .
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
                        $rhOptionRule->{&BLD_RETURN_TYPE},
                        defined($rhOptionRule->{&BLD_RETURN_VALUE_MAP}) && defined($rhOptionRule->{&BLD_RETURN_VALUE_MAP}->{$strTruthDefault}) ?
                            $rhOptionRule->{&BLD_RETURN_VALUE_MAP}->{$strTruthDefault} : $strTruthDefault) .
                        "` are not listed here for brevity.\n\n";
            }

            # Write table header
            $strTruthTable .= '| Function';

            for (my $iParamIdx = 0; $iParamIdx < @{$rhOptionRule->{&BLD_PARAM}}; $iParamIdx++)
            {
                $strTruthTable .= ' | ' . $rhOptionRule->{&BLD_PARAM}->[$iParamIdx];
            }

            $strTruthTable .=
                " | Result |\n" .
                '| --------';

            for (my $iParamIdx = 0; $iParamIdx < @{$rhOptionRule->{&BLD_PARAM}}; $iParamIdx++)
            {
                $strTruthTable .= ' | ' . ('-' x length($rhOptionRule->{&BLD_PARAM}->[$iParamIdx]));
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

            if ($rhOptionRule->{&BLD_PARAM}->[0] eq BLDLCL_PARAM_COMMANDID)
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

                        push(
                            @stryFilteredMatrix,
                            CGEN_DATAVAL_NULL . "~${strKey}~" . (keys(%{$rhMatrixFilter->{$strKey}{value}}))[0]);
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
                my $strRow = '| ' . $strFunction;
                my $strValue;

                for (my $iEntryIdx = 0; $iEntryIdx < @xyEntry; $iEntryIdx++)
                {
                    $strValue = $xyEntry[$iEntryIdx];
                    $strRow .= ' | ';

                    if ($iEntryIdx != @xyEntry - 1)
                    {
                        my $strParam = $rhOptionRule->{&BLD_PARAM}->[$iEntryIdx];

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
                                $rhOptionRule->{&BLD_RETURN_TYPE},
                                defined($rhOptionRule->{&BLD_RETURN_VALUE_MAP}) && defined($rhOptionRule->{&BLD_RETURN_VALUE_MAP}->{$strValue}) ?
                                    $rhOptionRule->{&BLD_RETURN_VALUE_MAP}->{$strValue} : $strValue) .
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

        $rhBuild->{$strFile}{&BLD_MD}{&BLD_SOURCE} = $strTruthTable;

        # Output config rule functions
        #-------------------------------------------------------------------------------------------------------------------------------
        my $strConfigFunction =
            "// AUTOGENERATED CODE - DO NOT MODIFY THIS FILE";

        if ($strFile eq BLDLCL_FILE_CONFIG)
        {
            $strConfigFunction .= "\n" . cgenFunction(
                'cfgCommandName', 'lookup command name using command id', undef,
                cgenLookupString('Command', 'CONFIG_COMMAND_TOTAL', $rhCommandNameConstantMap));
            $strConfigFunction .= "\n" . cgenFunction(
                'cfgOptionName', 'lookup option name using option id', undef,
                cgenLookupString(
                    'Option', 'CONFIG_OPTION_TOTAL', $rhOptionNameConstantMap, '^(' . join('|', @stryOptionAlt) . ')$'));
        }
        else
        {
            $strConfigFunction .= "\n" . cgenFunction(
                'cfgCommandId', 'lookup command id using command name', undef, cgenLookupId('Command', 'CONFIG_COMMAND_TOTAL'));
            $strConfigFunction .= "\n" . cgenFunction(
                'cfgOptionId', 'lookup option id using option name', undef, cgenLookupId('Option', 'CONFIG_OPTION_TOTAL'));
        }

        foreach my $strFunction (sort(keys(%{$rhCodeGen->{$strFile}{&BLD_FUNCTION}})))
        {
            my $rhOptionRule = $rhCodeGen->{$strFile}{&BLD_FUNCTION}{$strFunction};

            $strConfigFunction .= "\n" .
                cgenFunction($strFunction, $rhOptionRule->{&BLD_SUMMARY}, undef,
                    cgenSwitchBuild(
                        $strFunction, $rhOptionRule->{&BLD_RETURN_TYPE}, $rhOptionRule->{matrix},
                        $rhOptionRule->{&BLD_PARAM}, $rhLabelMap, $rhOptionRule->{&BLD_RETURN_VALUE_MAP}));
        }

        $rhBuild->{$strFile}{&BLD_C}{&BLD_SOURCE} = $strConfigFunction;
    }

    return $rhBuild;
}

push @EXPORT, qw(buildConfig);

1;
