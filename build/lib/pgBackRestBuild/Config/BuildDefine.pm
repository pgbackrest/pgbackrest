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

use pgBackRest::Version;

use pgBackRestDoc::Common::DocConfig;
use pgBackRestDoc::Common::DocRender;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;

use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Data;

####################################################################################################################################
# Constants
####################################################################################################################################
use constant BLDLCL_FILE_DEFINE                                     => 'define';

use constant BLDLCL_DATA_COMMAND                                    => '01-dataCommand';
use constant BLDLCL_DATA_OPTION                                     => '02-dataOption';

use constant BLDLCL_ENUM_COMMAND                                    => '01-enumCommand';
use constant BLDLCL_ENUM_OPTION_TYPE                                => '02-enumOptionType';
use constant BLDLCL_ENUM_OPTION                                     => '03-enumOption';

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
                &BLDLCL_ENUM_COMMAND =>
                {
                    &BLD_SUMMARY => 'Command define',
                    &BLD_NAME => 'ConfigDefineCommand',
                },

                &BLDLCL_ENUM_OPTION_TYPE =>
                {
                    &BLD_SUMMARY => 'Option type define',
                    &BLD_NAME => 'ConfigDefineOptionType',
                },

                &BLDLCL_ENUM_OPTION =>
                {
                    &BLD_SUMMARY => 'Option define',
                    &BLD_NAME => 'ConfigDefineOption',
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
sub buildConfigDefineCommandEnum
{
    return bldEnum('cfgDefCmd', shift)
}

push @EXPORT, qw(buildConfigDefineCommandEnum);

sub buildConfigDefineOptionTypeEnum
{
    return bldEnum('cfgDefOptType', shift);
}

push @EXPORT, qw(buildConfigDefineOptionTypeEnum);

sub buildConfigDefineOptionEnum
{
    return bldEnum('cfgDefOpt', shift);
}

push @EXPORT, qw(buildConfigDefineOptionEnum);

####################################################################################################################################
# Helper function to format help text
####################################################################################################################################
sub helpFormatText
{
    my $oManifest = shift;
    my $oDocRender = shift;
    my $oText = shift;
    my $iIndent = shift;
    my $iLength = shift;

    # Split the string into lines for processing
    my @stryText = split("\n", trim($oManifest->variableReplace($oDocRender->processText($oText))));
    my $strText;
    my $iIndex = 0;

    foreach my $strLine (@stryText)
    {
        # Add a linefeed if this is not the first line
        if (defined($strText))
        {
            $strText .= "\n";
        }

        # Escape special characters
        $strLine =~ s/\"/\\"/g;

        my $strPart;
        my $bFirst = true;

        # Split the line for output if it's too long
        do
        {
            ($strPart, $strLine) = stringSplit($strLine, ' ', defined($strPart) ? $iLength - 4 : $iLength);

            $strText .= ' ' x $iIndent;

            if (!$bFirst)
            {
                $strText .= "    ";
            }

            $strText .= "\"${strPart}";

            if (defined($strLine))
            {
                $strText .= "\"\n";
            }
            else
            {
                $strText .= ($iIndex + 1 < @stryText ? '\n' : '') . '"';
            }

            $bFirst = false;
        }
        while (defined($strLine));

        $iIndex++;
    }

    return $strText;
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
            "${strIndent}                " . buildConfigDefineOptionEnum($strDependOption) . ",\n" .
            "${strIndent}                " . join(",\n${strIndent}                ", bldQuoteList($ryDependList)) .
            "\n" .
            "${strIndent}            )\n";
    }

    return
        "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_DEPEND(" . buildConfigDefineOptionEnum($strDependOption) . ")\n";
}

sub renderOptional
{
    my $rhOptional = shift;
    my $bCommand = shift;
    my $rhOptionHelp = shift;
    my $oManifest = shift;
    my $oDocRender = shift;
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

        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_ALLOW_RANGE(" . $fyRange[0] . ', ' . $fyRange[1] . ")\n";

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
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_DEFAULT(\"" . $rhOptional->{&CFGDEF_DEFAULT} . "\")\n";

        $bSingleLine = true;
    }

    if (defined($rhOptional->{&CFGDEF_PREFIX}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_PREFIX(\"" . $rhOptional->{&CFGDEF_PREFIX} . "\")\n";

        $bSingleLine = true;
    }

    # Output alternate name
    if (!$bCommand && defined($rhOptionHelp->{&CONFIG_HELP_NAME_ALT}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_HELP_NAME_ALT(" .
                join(', ', bldQuoteList($rhOptionHelp->{&CONFIG_HELP_NAME_ALT})) . ")\n";

        $bSingleLine = true;
    }


    if ($bCommand && defined($rhOptional->{&CFGDEF_INTERNAL}))
    {
        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) && !$bSingleLine ? "\n" : '') .
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_INTERNAL(" . ($rhOptional->{&CFGDEF_INTERNAL} ? 'true' : 'false') .
                ")\n";

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

    if ($bCommand && defined($rhOptionHelp) && defined($rhOptionHelp->{&CONFIG_HELP_SOURCE}) &&
        $rhOptionHelp->{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_COMMAND)
    {
        my $strSummary = helpFormatText($oManifest, $oDocRender, $rhOptionHelp->{&CONFIG_HELP_SUMMARY}, 0, 72);

        if (length($strSummary) > 74)
        {
            confess("summary for command '${strCommand}', option '${strOption}' may not be greater than 72 characters");
        }

        $strBuildSourceOptional .=
            (defined($strBuildSourceOptional) ? "\n" : '') .
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_HELP_SUMMARY(${strSummary})\n" .
            "${strIndent}            CFGDEFDATA_OPTION_OPTIONAL_HELP_DESCRIPTION\n" .
            "${strIndent}            (\n" .
                helpFormatText($oManifest, $oDocRender, $rhOptionHelp->{&CONFIG_HELP_DESCRIPTION}, 20, 111) . "\n" .
            "${strIndent}            )\n";

    }

    return $strBuildSourceOptional;
}

####################################################################################################################################
# Build configuration constants and data
####################################################################################################################################
sub buildConfigDefine
{
    # Load help data
    #-------------------------------------------------------------------------------------------------------------------------------
    require pgBackRestDoc::Common::Doc;
    require pgBackRestDoc::Common::DocManifest;

    my $strDocPath = abs_path(dirname($0) . '/../doc');

    my $oStorageDoc = new pgBackRestTest::Common::Storage(
        $strDocPath, new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    my @stryEmpty = [];
    my $oManifest = new pgBackRestDoc::Common::DocManifest(
        $oStorageDoc, \@stryEmpty, \@stryEmpty, \@stryEmpty, \@stryEmpty, undef, $strDocPath, false, false);

    my $oDocRender = new pgBackRestDoc::Common::DocRender('text', $oManifest, false);
    my $oDocConfig =
        new pgBackRestDoc::Common::DocConfig(
            new pgBackRestDoc::Common::Doc("${strDocPath}/xml/reference.xml"), $oDocRender);
    my $hConfigHelp = $oDocConfig->{oConfigHash};

    # Build command constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_ENUM}{&BLDLCL_ENUM_COMMAND};

    my $strBuildSource =
        "static ConfigDefineCommandData configDefineCommandData[] = CFGDEFDATA_COMMAND_LIST\n" .
        "(";

    foreach my $strCommand (cfgDefineCommandList())
    {
        # Get command help
        my $hCommandHelp = $hConfigHelp->{&CONFIG_HELP_COMMAND}{$strCommand};

        # Build C enum
        my $strCommandEnum = buildConfigDefineCommandEnum($strCommand);
        push(@{$rhEnum->{&BLD_LIST}}, $strCommandEnum);

        # Build command data
        $strBuildSource .=
            "\n" .
            "    CFGDEFDATA_COMMAND\n" .
            "    (\n" .
            "        CFGDEFDATA_COMMAND_NAME(\"${strCommand}\")\n";


        # Output help
        if (defined($hCommandHelp))
        {
            $strBuildSource .=
                "\n";

            # Output command summary
            my $strSummary = helpFormatText($oManifest, $oDocRender, $hCommandHelp->{&CONFIG_HELP_SUMMARY}, 0, 72);

            if (length($strSummary) > 74)
            {
                confess("summary for command '${strCommand}' may not be greater than 72 characters");
            }

            $strBuildSource .=
                "        CFGDEFDATA_COMMAND_HELP_SUMMARY(${strSummary})\n";

            # Output description
            $strBuildSource .=
                "        CFGDEFDATA_COMMAND_HELP_DESCRIPTION\n" .
                "        (\n" .
                    helpFormatText($oManifest, $oDocRender, $hCommandHelp->{&CONFIG_HELP_DESCRIPTION}, 12, 119) . "\n" .
                "        )\n";
        }

        $strBuildSource .=
            "    )\n";
    };

    $strBuildSource .=
        ")\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_COMMAND}{&BLD_SOURCE} = $strBuildSource;

    # Build option type constants
    #-------------------------------------------------------------------------------------------------------------------------------
    $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_ENUM}{&BLDLCL_ENUM_OPTION_TYPE};

    foreach my $strOptionType (cfgDefineOptionTypeList())
    {
        # Build C enum
        my $strOptionTypeEnum = buildConfigDefineOptionTypeEnum($strOptionType);
        push(@{$rhEnum->{&BLD_LIST}}, $strOptionTypeEnum);
    };

    # Build option constants and data
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhConfigDefine = cfgDefine();
    $rhEnum = $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_ENUM}{&BLDLCL_ENUM_OPTION};

    $strBuildSource =
        "static ConfigDefineOptionData configDefineOptionData[] = CFGDEFDATA_OPTION_LIST\n" .
        "(";

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        # Get option help
        my $hOptionHelp = $hConfigHelp->{&CONFIG_HELP_OPTION}{$strOption};

        # Build C enum
        my $strOptionEnum = buildConfigDefineOptionEnum($strOption);
        push(@{$rhEnum->{&BLD_LIST}}, $strOptionEnum);

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
            "        CFGDEFDATA_OPTION_INTERNAL(" . ($rhOption->{&CFGDEF_INTERNAL} ? 'true' : 'false') . ")\n" .
            "\n" .
            "        CFGDEFDATA_OPTION_INDEX_TOTAL(" . $rhOption->{&CFGDEF_INDEX_TOTAL} . ")\n" .
            "        CFGDEFDATA_OPTION_SECURE(" . ($rhOption->{&CFGDEF_SECURE} ? 'true' : 'false') . ")\n";

        if (defined($hOptionHelp))
        {
            $strBuildSource .=
                "\n";

            # Output section
            my $strSection =
                defined($hOptionHelp->{&CONFIG_HELP_SECTION}) ? $hOptionHelp->{&CONFIG_HELP_SECTION} : 'general';

            if (length($strSection) > 72)
            {
                confess("section for option '${strOption}' may not be greater than 72 characters");
            }

            $strBuildSource .=
                "        CFGDEFDATA_OPTION_HELP_SECTION(\"${strSection}\")\n";

            # Output summary
            my $strSummary = helpFormatText($oManifest, $oDocRender, $hOptionHelp->{&CONFIG_HELP_SUMMARY}, 0, 72);

            if (length($strSummary) > 74)
            {
                confess("summary for option '${strOption}' may not be greater than 72 characters");
            }

            $strBuildSource .=
                "        CFGDEFDATA_OPTION_HELP_SUMMARY(${strSummary})\n";

            # Output description
            $strBuildSource .=
                "        CFGDEFDATA_OPTION_HELP_DESCRIPTION\n" .
                "        (\n" .
                    helpFormatText($oManifest, $oDocRender, $hOptionHelp->{&CONFIG_HELP_DESCRIPTION}, 12, 119) . "\n" .
                "        )\n";
        }

        $strBuildSource .=
            "\n" .
            "        CFGDEFDATA_OPTION_COMMAND_LIST\n" .
            "        (\n";

        foreach my $strCommand (cfgDefineCommandList())
        {
            if (defined($rhOption->{&CFGDEF_COMMAND}{$strCommand}))
            {
                $strBuildSource .=
                    "            CFGDEFDATA_OPTION_COMMAND(" . buildConfigDefineCommandEnum($strCommand) . ")\n";
            }
        }

        $strBuildSource .=
            "        )\n";

        # Render optional data
        my $strBuildSourceOptional = renderOptional($rhOption, false, $hOptionHelp, $oManifest, $oDocRender);

        # Render command overrides
        foreach my $strCommand (cfgDefineCommandList())
        {
            my $strBuildSourceOptionalCommand;
            my $rhCommand = $rhOption->{&CFGDEF_COMMAND}{$strCommand};

            if (defined($rhCommand))
            {
                $strBuildSourceOptionalCommand = renderOptional(
                    $rhCommand, true, $hConfigHelp->{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption},
                    $oManifest, $oDocRender, $strCommand, $strOption);

                if (defined($strBuildSourceOptionalCommand))
                {
                    $strBuildSourceOptional .=
                        (defined($strBuildSourceOptional) ? "\n" : '') .
                        "            CFGDEFDATA_OPTION_OPTIONAL_COMMAND_OVERRIDE\n" .
                        "            (\n" .
                        "                CFGDEFDATA_OPTION_OPTIONAL_COMMAND(" . buildConfigDefineCommandEnum($strCommand) . ")\n" .
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
