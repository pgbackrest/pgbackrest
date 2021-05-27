####################################################################################################################################
# Auto-Generate Option Definition for Parsing with getopt_long().
####################################################################################################################################
package pgBackRestBuild::Config::BuildParse;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;
use pgBackRestBuild::Config::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'parse';

use constant BLDLCL_ENUM_OPTION_TYPE                                => '01-enumOptionType';

use constant BLDLCL_DATA_COMMAND                                    => '01-dataCommand';
use constant BLDLCL_DATA_OPTION_GROUP                               => '01-dataOptionGroup';
use constant BLDLCL_DATA_OPTION                                     => '02-dataOption';
use constant BLDLCL_DATA_OPTION_GETOPT                              => '03-dataOptionGetOpt';
use constant BLDLCL_DATA_OPTION_RESOLVE                             => '04-dataOptionResolve';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $strSummary = 'Config Parse Rules';

my $rhBuild =
{
    &BLD_FILE =>
    {
        &BLDLCL_FILE_DEFINE =>
        {
            &BLD_SUMMARY => $strSummary,

            &BLD_ENUM =>
            {
                &BLDLCL_ENUM_OPTION_TYPE =>
                {
                    &BLD_SUMMARY => 'Option type',
                    &BLD_NAME => 'ConfigOptionType',
                },
            },

            &BLD_DATA =>
            {
                &BLDLCL_DATA_COMMAND =>
                {
                    &BLD_SUMMARY => 'Command parse data',
                },

                &BLDLCL_DATA_OPTION_GROUP =>
                {
                    &BLD_SUMMARY => 'Option group parse data',
                },

                &BLDLCL_DATA_OPTION =>
                {
                    &BLD_SUMMARY => 'Option parse data',
                },

                &BLDLCL_DATA_OPTION_GETOPT =>
                {
                    &BLD_SUMMARY => 'Option data for getopt_long()',
                },

                &BLDLCL_DATA_OPTION_RESOLVE =>
                {
                    &BLD_SUMMARY => 'Order for option parse resolution',
                },
            },
        },
    },
};

####################################################################################################################################
# Generate enum names
####################################################################################################################################
sub buildConfigDefineOptionTypeEnum
{
    return bldEnum('cfgOptType', shift);
}

push @EXPORT, qw(buildConfigDefineOptionTypeEnum);

sub buildConfigCommandRoleEnum
{
    return bldEnum('cfgCmdRole', shift);
}

####################################################################################################################################
# Helper functions for building optional option data
####################################################################################################################################
sub renderAllowList
{
    my $ryAllowList = shift;
    my $bCommandIndent = shift;

    my $strIndent = $bCommandIndent ? '    ' : '';

    return
        "${strIndent}            PARSE_RULE_OPTION_OPTIONAL_ALLOW_LIST\n" .
        "${strIndent}            (\n" .
        "${strIndent}                " . join(",\n${strIndent}                ", bldQuoteList($ryAllowList)) .
        "\n" .
        "${strIndent}            ),\n";
}

sub renderDepend
{
    my $rhDepend = shift;
    my $bCommandIndent = shift;

    my $strIndent = $bCommandIndent ? '    ' : '';

    my $strDependOption = $rhDepend->{&CFGDEF_DEPEND_OPTION};
    my $ryDependList = $rhDepend->{&CFGDEF_DEPEND_LIST};

    if (defined($ryDependList))
    {
        my @stryQuoteList;

        foreach my $strItem (@{$ryDependList})
        {
            push(@stryQuoteList, "\"${strItem}\"");
        }

        return
            "${strIndent}            PARSE_RULE_OPTION_OPTIONAL_DEPEND_LIST\n" .
            "${strIndent}            (\n" .
            "${strIndent}                " . buildConfigOptionEnum($strDependOption) . ",\n" .
            "${strIndent}                " . join(",\n${strIndent}                ", bldQuoteList($ryDependList)) .
            "\n" .
            "${strIndent}            ),\n";
    }

    return
        "${strIndent}            PARSE_RULE_OPTION_OPTIONAL_DEPEND(" . buildConfigOptionEnum($strDependOption) . "),\n";
}

sub renderOptional
{
    my $rhOptional = shift;
    my $bCommand = shift;
    my $strCommand = shift;
    my $strOption = shift;

    my $strIndent = $bCommand ? '    ' : '';
    my $strBuildSourceOptional;
    my $bSingleLine = false;

    if (defined($rhOptional->{&CFGDEF_ALLOW_LIST}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            renderAllowList($rhOptional->{&CFGDEF_ALLOW_LIST}, $bCommand);

        $bSingleLine = false;
    }

    if (defined($rhOptional->{&CFGDEF_ALLOW_RANGE}))
    {
        my @fyRange = @{$rhOptional->{&CFGDEF_ALLOW_RANGE}};

        my $iMultiplier = $rhOptional->{&CFGDEF_TYPE} eq CFGDEF_TYPE_TIME ? 1000 : 1;

        $strBuildSourceOptional =
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            PARSE_RULE_OPTION_OPTIONAL_ALLOW_RANGE(" . ($fyRange[0] * $iMultiplier) . ', ' .
            ($fyRange[1] * $iMultiplier) . "),\n";

        $bSingleLine = true;
    }

    if (defined($rhOptional->{&CFGDEF_DEPEND}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            renderDepend($rhOptional->{&CFGDEF_DEPEND}, $bCommand);

        $bSingleLine = defined($rhOptional->{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_LIST}) ? false : true;
    }

    if (defined($rhOptional->{&CFGDEF_DEFAULT}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            PARSE_RULE_OPTION_OPTIONAL_DEFAULT(" .
            ($rhOptional->{&CFGDEF_DEFAULT_LITERAL} ? '' : '"') .
            (defined($rhOptional->{&CFGDEF_TYPE}) && $rhOptional->{&CFGDEF_TYPE} eq CFGDEF_TYPE_TIME ?
                $rhOptional->{&CFGDEF_DEFAULT} * 1000 : $rhOptional->{&CFGDEF_DEFAULT}) .
            ($rhOptional->{&CFGDEF_DEFAULT_LITERAL} ? '' : '"') .
            "),\n";

        $bSingleLine = true;
    }

    if ($bCommand && defined($rhOptional->{&CFGDEF_REQUIRED}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            PARSE_RULE_OPTION_OPTIONAL_REQUIRED(" .
                ($rhOptional->{&CFGDEF_REQUIRED} ? 'true' : 'false') . "),\n";

        $bSingleLine = true;
    }

    return $strBuildSourceOptional;
}

####################################################################################################################################
# Build configuration constants and data
####################################################################################################################################
sub buildConfigParse
{
    # Build option type enum
    # ------------------------------------------------------------------------------------------------------------------------------
    my $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_ENUM}{&BLDLCL_ENUM_OPTION_TYPE};

    foreach my $strOptionType (cfgDefineOptionTypeList())
    {
        # Build C enum
        my $strOptionTypeEnum = buildConfigDefineOptionTypeEnum($strOptionType);
        push(@{$rhEnum->{&BLD_LIST}}, $strOptionTypeEnum);
    };

    # Build command parse data
    # ------------------------------------------------------------------------------------------------------------------------------
    my $rhCommandDefine = cfgDefineCommand();

    my $strBuildSource =
        "static const ParseRuleCommand parseRuleCommand[CFG_COMMAND_TOTAL] =\n" .
        "{";

    foreach my $strCommand (sort(keys(%{$rhCommandDefine})))
    {
        my $rhCommand = $rhCommandDefine->{$strCommand};

        # Build command data
        $strBuildSource .=
            "\n" .
            "    //" . (qw{-} x 126) . "\n" .
            "    PARSE_RULE_COMMAND\n" .
            "    (\n" .
            "        PARSE_RULE_COMMAND_NAME(\"${strCommand}\"),\n";

        if ($rhCommand->{&CFGDEF_PARAMETER_ALLOWED})
        {
            $strBuildSource .=
                "        PARSE_RULE_COMMAND_PARAMETER_ALLOWED(true),\n";
        }

        $strBuildSource .=
            "\n" .
            "        PARSE_RULE_COMMAND_ROLE_VALID_LIST\n" .
            "        (\n";

        foreach my $strCommandRole (sort(keys(%{$rhCommand->{&CFGDEF_COMMAND_ROLE}})))
        {
            $strBuildSource .=
                "            PARSE_RULE_COMMAND_ROLE(" . buildConfigCommandRoleEnum($strCommandRole) . ")\n";
        }

        $strBuildSource .=
            "        ),\n";

        $strBuildSource .=
            "    ),\n";
    };

    $strBuildSource .=
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_COMMAND}{&BLD_SOURCE} = $strBuildSource;

    # Build option group parse data
    # ------------------------------------------------------------------------------------------------------------------------------
    my $rhOptionGroupDefine = cfgDefineOptionGroup();

    $strBuildSource =
        "static const ParseRuleOptionGroup parseRuleOptionGroup[CFG_OPTION_GROUP_TOTAL] =\n" .
        "{";

    foreach my $strGroup (sort(keys(%{$rhOptionGroupDefine})))
    {
        $strBuildSource .=
            "\n" .
            "    //" . (qw{-} x 126) . "\n" .
            "    PARSE_RULE_OPTION_GROUP\n" .
            "    (\n" .
            "        PARSE_RULE_OPTION_GROUP_NAME(\"" . $strGroup . "\"),\n" .
            "    ),\n";
    }

    $strBuildSource .=
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION_GROUP}{&BLD_SOURCE} = $strBuildSource;

    # Build option parse data
    # ------------------------------------------------------------------------------------------------------------------------------
    my $rhConfigDefine = cfgDefine();

    $strBuildSource =
        "static const ParseRuleOption parseRuleOption[CFG_OPTION_TOTAL] =\n" .
        "{";

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $rhOption = $rhConfigDefine->{$strOption};

        $strBuildSource .=
            "\n" .
            "    // " . (qw{-} x 125) . "\n" .
            "    PARSE_RULE_OPTION\n" .
            "    (\n" .
            "        PARSE_RULE_OPTION_NAME(\"${strOption}\"),\n" .
            "        PARSE_RULE_OPTION_TYPE(" . buildConfigDefineOptionTypeEnum($rhOption->{&CFGDEF_TYPE}) . "),\n" .
            "        PARSE_RULE_OPTION_REQUIRED(" . ($rhOption->{&CFGDEF_REQUIRED} ? 'true' : 'false') . "),\n" .
            "        PARSE_RULE_OPTION_SECTION(cfgSection" .
                (defined($rhOption->{&CFGDEF_SECTION}) ? ucfirst($rhOption->{&CFGDEF_SECTION}) : 'CommandLine') .
                "),\n";

        if ($rhOption->{&CFGDEF_SECURE})
        {
            $strBuildSource .=
                "        PARSE_RULE_OPTION_SECURE(true),\n";
        }

        if ($rhOption->{&CFGDEF_TYPE} eq CFGDEF_TYPE_HASH || $rhOption->{&CFGDEF_TYPE} eq CFGDEF_TYPE_LIST)
        {
            $strBuildSource .=
                "        PARSE_RULE_OPTION_MULTI(true),\n";
        }

        # Build group info
        # --------------------------------------------------------------------------------------------------------------------------
        if ($rhOption->{&CFGDEF_GROUP})
        {
            $strBuildSource .=
                "        PARSE_RULE_OPTION_GROUP_MEMBER(true),\n" .
                "        PARSE_RULE_OPTION_GROUP_ID(" . buildConfigOptionGroupEnum($rhOption->{&CFGDEF_GROUP}) . "),\n";
        }

        # Build command role valid lists
        #---------------------------------------------------------------------------------------------------------------------------
        my $strBuildSourceSub = "";

        foreach my $strCommandRole (CFGCMD_ROLE_MAIN, CFGCMD_ROLE_ASYNC, CFGCMD_ROLE_LOCAL, CFGCMD_ROLE_REMOTE)
        {
            $strBuildSourceSub = "";

            foreach my $strCommand (cfgDefineCommandList())
            {
                if (defined($rhOption->{&CFGDEF_COMMAND}{$strCommand}))
                {
                    if (defined($rhOption->{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_COMMAND_ROLE}{$strCommandRole}))
                    {
                        $strBuildSourceSub .=
                            "            PARSE_RULE_OPTION_COMMAND(" . buildConfigCommandEnum($strCommand) . ")\n";
                    }
                }
            }

            if ($strBuildSourceSub ne "")
            {
                $strBuildSource .=
                    "\n" .
                    "        PARSE_RULE_OPTION_COMMAND_ROLE_" . uc($strCommandRole) . "_VALID_LIST\n" .
                    "        (\n" .
                    $strBuildSourceSub .
                    "        ),\n";
            }
        }

        # Render optional data and command overrides
        # --------------------------------------------------------------------------------------------------------------------------
        my $strBuildSourceOptional = renderOptional($rhOption, false);

        foreach my $strCommand (cfgDefineCommandList())
        {
            my $strBuildSourceOptionalCommand;
            my $rhCommand = $rhOption->{&CFGDEF_COMMAND}{$strCommand};

            if (defined($rhCommand))
            {
                $strBuildSourceOptionalCommand = renderOptional($rhCommand, true, $strCommand, $strOption);

                if (defined($strBuildSourceOptionalCommand))
                {
                    $strBuildSourceOptional .=
                        (defined($strBuildSourceOptional) ? "\n" : '') .
                        "            PARSE_RULE_OPTION_OPTIONAL_COMMAND_OVERRIDE\n" .
                        "            (\n" .
                        "                PARSE_RULE_OPTION_OPTIONAL_COMMAND(" . buildConfigCommandEnum($strCommand) . "),\n" .
                        "\n" .
                        $strBuildSourceOptionalCommand .
                        "            )\n";
                }
            }
        }

        if (defined($strBuildSourceOptional))
        {
            $strBuildSource .=
                "\n" .
                "        PARSE_RULE_OPTION_OPTIONAL_LIST\n" .
                "        (\n" .
                $strBuildSourceOptional .
                "        ),\n";
        }

        $strBuildSource .=
            "    ),\n";
    }

    $strBuildSource .=
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION}{&BLD_SOURCE} = $strBuildSource;

    # Build option list for getopt_long()
    #-------------------------------------------------------------------------------------------------------------------------------
    $strBuildSource =
        "static const struct option optionList[] =\n" .
        "{";

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $rhOption = $rhConfigDefine->{$strOption};
        my $strOptionEnum = buildConfigOptionEnum($strOption);
        my $strOptionArg = ($rhOption->{&CFGDEF_TYPE} ne CFGDEF_TYPE_BOOLEAN ? "        .has_arg = required_argument,\n" : '');
        my $strOptionPrefix = $rhConfigDefine->{$strOption}{&CFGDEF_PREFIX};

        my @stryOptionName = ($strOption);

        if (defined($rhOption->{&CFGDEF_DEPRECATE}))
        {
            foreach my $strOptionNameAlt (sort(keys(%{$rhOption->{&CFGDEF_DEPRECATE}})))
            {
                push(@stryOptionName, $strOptionNameAlt);
            }
        }

        $strBuildSource .=
            "\n" .
            "    // ${strOption} option" . (@stryOptionName > 1 ? ' and deprecations' : '') . "\n" .
            "    // " . (qw{-} x 125) . "\n";

        for (my $iOptionIdx = 1; $iOptionIdx <= $rhOption->{&CFGDEF_INDEX_TOTAL}; $iOptionIdx++)
        {
            for (my $iOptionNameIdx = 0; $iOptionNameIdx < @stryOptionName; $iOptionNameIdx++)
            {
                my $strOptionName = $stryOptionName[$iOptionNameIdx];
                my $rhNameAlt = $rhOption->{&CFGDEF_DEPRECATE}{$strOptionName};

                # Skip alt name if it is not valid for this option index
                if ($iOptionNameIdx > 0 && defined($rhNameAlt->{&CFGDEF_INDEX}) && $rhNameAlt->{&CFGDEF_INDEX} != $iOptionIdx)
                {
                    next;
                }

                # Generate output name
                my $strOptionNameOut = $strOptionName;
                my $strOptionConst;

                if (defined($strOptionPrefix))
                {
                    if ($iOptionNameIdx == 0)
                    {
                        $strOptionNameOut =
                            "${strOptionPrefix}${iOptionIdx}-" . substr($strOptionName, length($strOptionPrefix) + 1);
                    }
                    else
                    {
                        $strOptionNameOut =~ s/\?/$iOptionIdx/g;
                    }
                }

                # Generate option value used for parsing (offset is added so options don't conflict with getopt_long return values)
                my $strOptionFlag = 'PARSE_OPTION_FLAG |';

                # Build option constant name if this is the current name for the option
                if ($iOptionNameIdx == 0)
                {
                    if (!$rhConfigDefine->{$strOption}{&CFGDEF_GROUP})
                    {
                        $strOptionConst = "CFGOPT_" . uc($strOptionNameOut);
                        $strOptionConst =~ s/\-/_/g;
                    }
                }
                # Else use bare string and mark as deprecated
                else
                {
                    $strOptionFlag .= ' PARSE_DEPRECATE_FLAG |';
                }

                my $strOptionVal =
                    ($rhOption->{&CFGDEF_GROUP} ? "(" . ($iOptionIdx - 1) . " << PARSE_KEY_IDX_SHIFT) | " : '') . $strOptionEnum;

                # Add option
                $strBuildSource .=
                    "    {\n" .
                    "        .name = \"${strOptionNameOut}\",\n" .
                    $strOptionArg .
                    "        .val = ${strOptionFlag} ${strOptionVal},\n" .
                    "    },\n";

                # Add negation when defined
                if ($rhOption->{&CFGDEF_NEGATE} &&
                    !($iOptionNameIdx > 0 && defined($rhNameAlt->{&CFGDEF_NEGATE}) && !$rhNameAlt->{&CFGDEF_NEGATE}))
                {
                    $strBuildSource .=
                        "    {\n" .
                        "        .name = \"no-${strOptionNameOut}\",\n" .
                        "        .val = ${strOptionFlag} PARSE_NEGATE_FLAG | ${strOptionVal},\n" .
                        "    },\n";
                }

                # Add reset when defined
                if ($rhOption->{&CFGDEF_RESET} &&
                    !($iOptionNameIdx > 0 && defined($rhNameAlt->{&CFGDEF_RESET}) && !$rhNameAlt->{&CFGDEF_RESET}))
                {
                    $strBuildSource .=
                        "    {\n" .
                        "        .name = \"reset-${strOptionNameOut}\",\n" .
                        "        .val = ${strOptionFlag} PARSE_RESET_FLAG | ${strOptionVal},\n" .
                        "    },\n";
                }
            }
        }
    }

    # The option list needs to be terminated or getopt_long will just keep on reading
    $strBuildSource .=
        "    // Terminate option list\n" .
        "    {\n" .
        "        .name = NULL\n" .
        "    }\n" .
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION_GETOPT}{&BLD_SOURCE} = $strBuildSource;

    # Build option resolve order list.  This allows the option validation in C to take place in a single pass.
    #
    # Always process the stanza option first since confusing error message are produced if it is missing.
    #-------------------------------------------------------------------------------------------------------------------------------
    my @stryResolveOrder = (buildConfigOptionEnum(CFGOPT_STANZA));
    my $rhResolved = {&CFGOPT_STANZA => true};
    my $bAllResolved;

    do
    {
        # Assume that we will finish on this loop
        $bAllResolved = true;

        # Loop through all options
        foreach my $strOption (sort(keys(%{$rhConfigDefine})))
        {
            my $bSkip = false;

            # Check the default depend
            my $strOptionDepend =
                ref($rhConfigDefine->{$strOption}{&CFGDEF_DEPEND}) eq 'HASH' ?
                    $rhConfigDefine->{$strOption}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION} :
                    $rhConfigDefine->{$strOption}{&CFGDEF_DEPEND};

            if (defined($strOptionDepend) && !$rhResolved->{$strOptionDepend})
            {
                # &log(WARN, "$strOptionDepend is not resolved");
                $bSkip = true;
            }

            # Check the command depends
            foreach my $strCommand (sort(keys(%{$rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}})))
            {
                my $strOptionDepend =
                    ref($rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_DEPEND}) eq 'HASH' ?
                        $rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_DEPEND}{&CFGDEF_DEPEND_OPTION} :
                        $rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_DEPEND};

                if (defined($strOptionDepend) && !$rhResolved->{$strOptionDepend})
                {
                    $bSkip = true;
                }
            }

            # If dependency was not found try again on the next loop
            if ($bSkip)
            {
                $bAllResolved = false;
            }
            # Else add option to resolve order list
            elsif (!$rhResolved->{$strOption})
            {
                push(@stryResolveOrder, buildConfigOptionEnum($strOption));

                $rhResolved->{$strOption} = true;
            }
        }
    }
    while (!$bAllResolved);

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION_RESOLVE}{&BLD_SOURCE} =
        "static const ConfigOption optionResolveOrder[] =\n" .
        "{\n" .
        "    " . join(",\n    ", @stryResolveOrder) . ",\n" .
        "};\n";

    return $rhBuild;
}

push @EXPORT, qw(buildConfigParse);

1;
