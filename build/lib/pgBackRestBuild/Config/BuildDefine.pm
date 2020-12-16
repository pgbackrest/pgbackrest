####################################################################################################################################
# Auto-Generate Command and Option Configuration Definition Enums, Constants and Data
####################################################################################################################################
package pgBackRestBuild::Config::BuildDefine;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRestDoc::Common::DocConfig;
use pgBackRestDoc::Common::DocRender;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;
use pgBackRestBuild::Config::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'define';

use constant BLDLCL_DATA_COMMAND                                    => '01-dataCommand';
use constant BLDLCL_DATA_OPTION                                     => '02-dataOption';

use constant BLDLCL_ENUM_OPTION_TYPE                                => '02-enumOptionType';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $strSummary = 'Command and Option Definition';

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
                    &BLD_SUMMARY => 'Option type define',
                    &BLD_NAME => 'ConfigDefineOptionType',
                },
            },

            &BLD_DATA =>
            {
                &BLDLCL_DATA_COMMAND =>
                {
                    &BLD_SUMMARY => 'Command define data',
                },

                &BLDLCL_DATA_OPTION =>
                {
                    &BLD_SUMMARY => 'Option define data',
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
    return bldEnum('cfgDefOptType', shift);
}

push @EXPORT, qw(buildConfigDefineOptionTypeEnum);

####################################################################################################################################
# Helper functions for building optional option data
####################################################################################################################################
sub renderAllowList
{
    my $ryAllowList = shift;
    my $bCommandIndent = shift;

    my $strIndent = $bCommandIndent ? '    ' : '';

    return
        "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_ALLOW_LIST\n" .
        "${strIndent}            (\n" .
        "${strIndent}                " . join(",\n${strIndent}                ", bldQuoteList($ryAllowList)) .
        "\n" .
        "${strIndent}            )\n";
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
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_DEPEND_LIST\n" .
            "${strIndent}            (\n" .
            "${strIndent}                " . buildConfigOptionEnum($strDependOption) . ",\n" .
            "${strIndent}                " . join(",\n${strIndent}                ", bldQuoteList($ryDependList)) .
            "\n" .
            "${strIndent}            )\n";
    }

    return
        "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_DEPEND(" . buildConfigOptionEnum($strDependOption) . ")\n";
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
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_ALLOW_RANGE(" . ($fyRange[0] * $iMultiplier) . ', ' .
            ($fyRange[1] * $iMultiplier) . ")\n";

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
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_DEFAULT(\"" .
            (defined($rhOptional->{&CFGDEF_TYPE}) && $rhOptional->{&CFGDEF_TYPE} eq CFGDEF_TYPE_TIME ?
                $rhOptional->{&CFGDEF_DEFAULT} * 1000 : $rhOptional->{&CFGDEF_DEFAULT}) .
            "\")\n";

        $bSingleLine = true;
    }

    if ($bCommand && defined($rhOptional->{&CFGDEF_REQUIRED}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_REQUIRED(" .
                ($rhOptional->{&CFGDEF_REQUIRED} ? 'true' : 'false') . ")\n";

        $bSingleLine = true;
    }

    return $strBuildSourceOptional;
}

####################################################################################################################################
# Build configuration constants and data
####################################################################################################################################
sub buildConfigDefine
{
    # Build command constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strBuildSource =
        "static ConfigDefineCommandData configDefineCommandData[] = CFGDEFDATA_COMMAND_LIST\n" .
        "(";

    foreach my $strCommand (cfgDefineCommandList())
    {
        # Build command data
        $strBuildSource .=
            "\n" .
            "    CFGDEFDATA_COMMAND\n" .
            "    (\n" .
            "        CFGDEFDATA_COMMAND_NAME(\"${strCommand}\")\n" .
            "    )\n";
    };

    $strBuildSource .=
        ")\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_COMMAND}{&BLD_SOURCE} = $strBuildSource;

    # Build option type constants
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_ENUM}{&BLDLCL_ENUM_OPTION_TYPE};

    foreach my $strOptionType (cfgDefineOptionTypeList())
    {
        # Build C enum
        my $strOptionTypeEnum = buildConfigDefineOptionTypeEnum($strOptionType);
        push(@{$rhEnum->{&BLD_LIST}}, $strOptionTypeEnum);
    };

    # Build option constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhConfigDefine = cfgDefine();

    $strBuildSource =
        "static ConfigDefineOptionData configDefineOptionData[] = CFGDEFDATA_OPTION_LIST\n" .
        "(";

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        # Build option data
        my $rhOption = $rhConfigDefine->{$strOption};

        my $strOptionPrefix = $rhOption->{&CFGDEF_PREFIX};

        $strBuildSource .=
            "\n" .
            "    // " . (qw{-} x 125) . "\n" .
            "    CFGDEFDATA_OPTION\n" .
            "    (\n";

        my $bRequired = $rhOption->{&CFGDEF_REQUIRED};

        $strBuildSource .=
            "        CFGDEFDATA_OPTION_NAME(\"${strOption}\")\n" .
            "        CFGDEFDATA_OPTION_REQUIRED(" . ($bRequired ? 'true' : 'false') . ")\n" .
            "        CFGDEFDATA_OPTION_SECTION(cfgDefSection" .
                (defined($rhOption->{&CFGDEF_SECTION}) ? ucfirst($rhOption->{&CFGDEF_SECTION}) : 'CommandLine') .
                ")\n" .
            "        CFGDEFDATA_OPTION_TYPE(" . buildConfigDefineOptionTypeEnum($rhOption->{&CFGDEF_TYPE}) . ")\n" .
            "\n" .
            "        CFGDEFDATA_OPTION_SECURE(" . ($rhOption->{&CFGDEF_SECURE} ? 'true' : 'false') . ")\n";

        $strBuildSource .=
            "\n" .
            "        CFGDEFDATA_OPTION_COMMAND_LIST\n" .
            "        (\n";

        foreach my $strCommand (cfgDefineCommandList())
        {
            if (defined($rhOption->{&CFGDEF_COMMAND}{$strCommand}))
            {
                $strBuildSource .=
                    "            CFGDEFDATA_OPTION_COMMAND(" . buildConfigCommandEnum($strCommand) . ")\n";
            }
        }

        $strBuildSource .=
            "        )\n";

        # Render optional data
        my $strBuildSourceOptional = renderOptional($rhOption, false);

        # Render command overrides
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
                        "            CFGDEFDATA_OPTION_OPTIONAL_COMMAND_OVERRIDE\n" .
                        "            (\n" .
                        "                CFGDEFDATA_OPTION_OPTIONAL_COMMAND(" . buildConfigCommandEnum($strCommand) . ")\n" .
                        "\n" .
                        $strBuildSourceOptionalCommand .
                        "            )\n";
                }
            }

        };

        if (defined($strBuildSourceOptional))
        {
            $strBuildSource .=
                "\n" .
                "        CFGDEFDATA_OPTION_OPTIONAL_LIST\n" .
                "        (\n" .
                $strBuildSourceOptional .
                "        )\n";
        }

        $strBuildSource .=
            "    )\n";
    }

    $strBuildSource .=
        ")\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION}{&BLD_SOURCE} = $strBuildSource;

    return $rhBuild;
}

push @EXPORT, qw(buildConfigDefine);

1;
