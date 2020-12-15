####################################################################################################################################
# Auto-Generate Command and Option Configuration Definition Enums, Constants and Data
####################################################################################################################################
package pgBackRestBuild::Config::BuildHelp;

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
use constant BLDLCL_FILE_DEFINE                                     => 'help';

use constant BLDLCL_DATA_COMMAND                                    => '01-command';

####################################################################################################################################
# Definitions for constants and data to build
####################################################################################################################################
my $strSummary = 'Help Definition';

my $rhBuild =
{
    &BLD_FILE =>
    {
        &BLDLCL_FILE_DEFINE =>
        {
            &BLD_SUMMARY => $strSummary,

            &BLD_DATA =>
            {
                &BLDLCL_DATA_COMMAND =>
                {
                    &BLD_SUMMARY => 'Command help',
                },
            },
        },
    },
};

####################################################################################################################################
# Format pack tag
####################################################################################################################################
use constant PCK_TYPE_ARRAY                                         => 'pckTypeArray';
use constant PCK_TYPE_BOOL                                          => 'pckTypeBool';
use constant PCK_TYPE_OBJ                                           => 'pckTypeObj';
use constant PCK_TYPE_STR                                           => 'pckTypeStr';

sub packIntFormat
{
    my $iValue = shift;

    my $strResult = '';

    while ($iValue >= 0x80)
    {
        # Encode the lower order 7 bits, adding the continuation bit to indicate there is more data
        $strResult .= sprintf(" 0x%02X,", ($iValue & 0x7f) | 0x80);

        # Shift the value to remove bits that have been encoded
        $iValue >>= 7;
    }

    return $strResult . sprintf(" 0x%02X,", $iValue);
}

sub packTagFormat
{
    my $strName = shift;
    my $strType = shift;
    my $iDelta = shift;
    my $xData = shift;
    my $iIndent = shift;

    my $strIndent = ' ' x $iIndent;

    my $iValue = undef;
    my $iBits = undef;

    if ($strType eq PCK_TYPE_STR || $strType eq PCK_TYPE_BOOL)
    {
        $iBits = $iDelta & 0x3;
        $iDelta >>= 2;

        if ($iDelta != 0)
        {
            $iBits |= 0x4;
        }

        if ($strType eq PCK_TYPE_STR)
        {
            $iBits |= 0x8;
            $iValue = length($xData);
        }
        else
        {
            $iBits |= $xData ? 0x8 : 0;
            undef($xData);
        }
    }
    elsif ($strType eq PCK_TYPE_ARRAY || $strType eq PCK_TYPE_OBJ)
    {
        $iBits |= $iDelta & 0x7;
        $iDelta >>= 3;

        if ($iDelta != 0)
        {
            $iBits |= 0x8;
        }
    }

    my $strResult = "${strIndent}${strType} << 4";

    if ($iBits != 0)
    {
        $strResult .= sprintf(" | 0x%02X", $iBits);
    }

    $strResult .= ',';

    if ($iDelta > 0)
    {
        $strResult .= packIntFormat($iDelta);
    }

    if (defined($iValue))
    {
        $strResult .= packIntFormat($iValue);
    }

    $strResult .= " // ${strName}";

    if (defined($xData) && length($xData) > 0)
    {
        $strResult .= "\n${strIndent}    ";
        my $iLength = length($strIndent) + 4;
        my $bLastLF = false;
        my $bFirst = true;

        foreach my $iChar (unpack("W*", $xData))
        {
            my $strOut = sprintf("0x%02X,", $iChar);

            if ($bLastLF && $iChar != 0xA)
            {
                $strResult .= "\n${strIndent}    ";
                $iLength = length($strIndent) + 4;
                $bFirst = true;
            }

            $bLastLF = $iChar == 0xA;

            if ($iLength + length($strOut) + 1 > 132)
            {
                $strResult .= "\n${strIndent}    ${strOut}";
                $iLength = length($strIndent) + 4 + length($strOut);
            }
            else
            {
                $strResult .= ($bFirst ? '' : ' ') . "${strOut}";
                $iLength += length($strOut) + ($bFirst ? 0 : 1);
                $bFirst = false;
            }
        }
    }

    return $strResult . "\n";
}

####################################################################################################################################
# Build help data
####################################################################################################################################
sub buildConfigHelp
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

    # Build command help
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhCommandDefine = cfgDefineCommand();

    my $strBuildSource =
        "static const unsigned char helpDataPack[] =\n" .
        "{";

    # Build command data
    $strBuildSource .=
        "\n" .
        "    // Commands\n" .
        "    // " . (qw{-} x 125) . "\n";

    $strBuildSource .= packTagFormat("Commands begin", PCK_TYPE_ARRAY, 0, undef, 4);

    foreach my $strCommand (sort(keys(%{$rhCommandDefine})))
    {
        my $rhCommand = $rhCommandDefine->{$strCommand};
        my $iDelta = 0;

        # Get command help
        my $rhCommandHelp = $hConfigHelp->{&CONFIG_HELP_COMMAND}{$strCommand};

        if (!defined($rhCommandHelp))
        {
            confess "no help for command ${strCommand}"
        }

        # Build command data
        $strBuildSource .=
            "\n" .
            "        // ${strCommand} command\n" .
            "        // " . (qw{-} x 121) . "\n";

        if ($rhCommand->{&CFGDEF_INTERNAL})
        {
            $strBuildSource .= packTagFormat("Internal", PCK_TYPE_BOOL, 0, true, 8);
        }
        else
        {
            $iDelta++;
        }

        my $strSummary = trim($oManifest->variableReplace($oDocRender->processText($rhCommandHelp->{&CONFIG_HELP_SUMMARY})));

        if (length($strSummary) > 72)
        {
            confess("summary for command '${strCommand}' may not be greater than 72 characters");
        }

        $strBuildSource .= packTagFormat("Summary", PCK_TYPE_STR, $iDelta, $strSummary, 8);
        $iDelta = 0;

        $strBuildSource .= packTagFormat(
            "Description", PCK_TYPE_STR, 0,
            trim($oManifest->variableReplace($oDocRender->processText($rhCommandHelp->{&CONFIG_HELP_DESCRIPTION}))), 8);
    };

    $strBuildSource .=
        "\n" .
        "    0x00, // Commands end\n";

    # # Build option constants and data
    # #-------------------------------------------------------------------------------------------------------------------------------
    # my $rhConfigDefine = cfgDefine();
    #
    # $strBuildSource =
    #     "static const unsigned char helpOptionPack[] =\n" .
    #     "{";
    #
    # foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    # {
    #     # Build option data
    #     my $rhOption = $rhConfigDefine->{$strOption};
    #
    #     # Get option help
    #     my $rhOptionHelp = $hConfigHelp->{&CONFIG_HELP_OPTION}{$strOption};
    # #
    # #     my $strOptionPrefix = $rhOption->{&CFGDEF_PREFIX};
    # #
    #     $strBuildSource .=
    #         "\n" .
    #         "    // ${strOption} option\n" .
    #         "    // " . (qw{-} x 125) . "\n";
    #
    # #     my $bRequired = $rhOption->{&CFGDEF_REQUIRED};
    # #
    # #     $strBuildSource .=
    # #         "        CFGDEFDATA_OPTION_NAME(\"${strOption}\")\n" .
    # #         "        CFGDEFDATA_OPTION_REQUIRED(" . ($bRequired ? 'true' : 'false') . ")\n" .
    # #         "        CFGDEFDATA_OPTION_SECTION(cfgDefSection" .
    # #             (defined($rhOption->{&CFGDEF_SECTION}) ? ucfirst($rhOption->{&CFGDEF_SECTION}) : 'CommandLine') .
    # #             ")\n" .
    # #         "        CFGDEFDATA_OPTION_TYPE(" . buildConfigDefineOptionTypeEnum($rhOption->{&CFGDEF_TYPE}) . ")\n" .
    # #         "        CFGDEFDATA_OPTION_INTERNAL(" . ($rhOption->{&CFGDEF_INTERNAL} ? 'true' : 'false') . ")\n" .
    # #         "\n" .
    # #         "        CFGDEFDATA_OPTION_SECURE(" . ($rhOption->{&CFGDEF_SECURE} ? 'true' : 'false') . ")\n";
    # #
    # #     if (defined($hOptionHelp))
    # #     {
    # #         $strBuildSource .=
    # #             "\n";
    # #
    # #         # Output section
    # #         my $strSection =
    # #             defined($hOptionHelp->{&CONFIG_HELP_SECTION}) ? $hOptionHelp->{&CONFIG_HELP_SECTION} : 'general';
    # #
    # #         if (length($strSection) > 72)
    # #         {
    # #             confess("section for option '${strOption}' may not be greater than 72 characters");
    # #         }
    # #
    # #         $strBuildSource .=
    # #             "        CFGDEFDATA_OPTION_HELP_SECTION(\"${strSection}\")\n";
    # #
    # #         # Output summary
    # #         my $strSummary = helpFormatText($oManifest, $oDocRender, $hOptionHelp->{&CONFIG_HELP_SUMMARY}, 0, 72);
    # #
    # #         if (length($strSummary) > 74)
    # #         {
    # #             confess("summary for option '${strOption}' may not be greater than 72 characters");
    # #         }
    # #
    # #         $strBuildSource .=
    # #             "        CFGDEFDATA_OPTION_HELP_SUMMARY(${strSummary})\n";
    # #
    # #         # Output description
    # #         $strBuildSource .=
    # #             "        CFGDEFDATA_OPTION_HELP_DESCRIPTION\n" .
    # #             "        (\n" .
    # #                 helpFormatText($oManifest, $oDocRender, $hOptionHelp->{&CONFIG_HELP_DESCRIPTION}, 12, 119) . "\n" .
    # #             "        )\n";
    # #     }
    # #
    # #     $strBuildSource .=
    # #         "\n" .
    # #         "        CFGDEFDATA_OPTION_COMMAND_LIST\n" .
    # #         "        (\n";
    # #
    # #     foreach my $strCommand (cfgDefineCommandList())
    # #     {
    # #         if (defined($rhOption->{&CFGDEF_COMMAND}{$strCommand}))
    # #         {
    # #             $strBuildSource .=
    # #                 "            CFGDEFDATA_OPTION_COMMAND(" . buildConfigCommandEnum($strCommand) . ")\n";
    # #         }
    # #     }
    # #
    # #     $strBuildSource .=
    # #         "        )\n";
    # #
    # #     # Render optional data
    # #     my $strBuildSourceOptional = renderOptional($rhOption, false, $hOptionHelp, $oManifest, $oDocRender);
    # #
    # #     # Render command overrides
    # #     foreach my $strCommand (cfgDefineCommandList())
    # #     {
    # #         my $strBuildSourceOptionalCommand;
    # #         my $rhCommand = $rhOption->{&CFGDEF_COMMAND}{$strCommand};
    # #
    # #         if (defined($rhCommand))
    # #         {
    # #             $strBuildSourceOptionalCommand = renderOptional(
    # #                 $rhCommand, true, $hConfigHelp->{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption},
    # #                 $oManifest, $oDocRender, $strCommand, $strOption);
    # #
    # #             if (defined($strBuildSourceOptionalCommand))
    # #             {
    # #                 $strBuildSourceOptional .=
    # #                     (defined($strBuildSourceOptional) ? "\n" : '') .
    # #                     "            CFGDEFDATA_OPTION_OPTIONAL_COMMAND_OVERRIDE\n" .
    # #                     "            (\n" .
    # #                     "                CFGDEFDATA_OPTION_OPTIONAL_COMMAND(" . buildConfigCommandEnum($strCommand) . ")\n" .
    # #                     "\n" .
    # #                     $strBuildSourceOptionalCommand .
    # #                     "            )\n";
    # #             }
    # #         }
    # #
    # #     };
    # #
    # #     if (defined($strBuildSourceOptional))
    # #     {
    # #         $strBuildSource .=
    # #             "\n" .
    # #             "        CFGDEFDATA_OPTION_OPTIONAL_LIST\n" .
    # #             "        (\n" .
    # #             $strBuildSourceOptional .
    # #             "        )\n";
    # #     }
    # #
    # #     $strBuildSource .=
    # #         "    )\n";
    # }
    #
    # $strBuildSource .=
    #     "};\n";
    #
    # $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_OPTION}{&BLD_SOURCE} = $strBuildSource;

    $strBuildSource .=
        "\n" .
        "    0x00, // Pack end\n" .
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_COMMAND}{&BLD_SOURCE} = $strBuildSource;

    return $rhBuild;
}

push @EXPORT, qw(buildConfigHelp);

1;
