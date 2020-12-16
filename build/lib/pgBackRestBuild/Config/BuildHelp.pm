####################################################################################################################################
# Auto-Generate Command and Option Help Pack
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
# Definitions for data to build
####################################################################################################################################
my $strSummary = 'Help Definition Pack';

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

# Pack an unsigned 64-bit integer to base-128 varint encoding and output to hex. This is a simplified version of
# pckWriteUInt64Internal() so see that function for more information.
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

# Write pack field tag and data. This is a cut down version of pckWriteTag() so see that function for more information.
sub packTagFormat
{
    my $strName = shift;
    my $strType = shift;
    my $iDelta = shift;
    my $xData = shift;
    my $iIndent = shift;

    my $strIndent = ' ' x $iIndent;

    # Pack delta bits and determine value for various pack. See pckWriteTag() for more detailed information.
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

    # Output pack type and bits
    my $strResult = "${strIndent}${strType} << 4";

    if ($iBits != 0)
    {
        $strResult .= sprintf(" | 0x%02X", $iBits);
    }

    $strResult .= ',';

    # Output additional id delta when present
    if ($iDelta > 0)
    {
        $strResult .= packIntFormat($iDelta);
    }

    # Output value when present
    if (defined($iValue))
    {
        $strResult .= packIntFormat($iValue);
    }

    # Output pack name
    $strResult .= " // ${strName}";

    # Output data in hex format
    if (defined($xData) && length($xData) > 0)
    {
        $strResult .= "\n${strIndent}    ";
        my $iLength = length($strIndent) + 4;
        my $bLastLF = false;
        my $bFirst = true;

        # Loop through all chars
        foreach my $iChar (unpack("W*", $xData))
        {
            # Encode char to hex
            my $strOut = sprintf("0x%02X,", $iChar);

            # Break on linefeeds to prevent diffs within a paragraph of text from cascading through all the data
            if ($bLastLF && $iChar != 0xA)
            {
                $strResult .= "\n${strIndent}    ";
                $iLength = length($strIndent) + 4;
                $bFirst = true;
            }

            $bLastLF = $iChar == 0xA;

            # If this hex would exceed the line length then break and write on the next line
            if ($iLength + length($strOut) + 1 > 132)
            {
                $strResult .= "\n${strIndent}    ${strOut}";
                $iLength = length($strIndent) + 4 + length($strOut);
            }
            # Else append the hex
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
        "{\n" .
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

    # Build option help
    #-------------------------------------------------------------------------------------------------------------------------------
    my $rhConfigDefine = cfgDefine();

    $strBuildSource .=
        "\n" .
        "    // Options\n" .
        "    // " . (qw{-} x 125) . "\n";

    $strBuildSource .= packTagFormat("Options begin", PCK_TYPE_ARRAY, 0, undef, 4);

    my $iDelta = 0;

    foreach my $strOption (sort(keys(%{$rhConfigDefine})))
    {
        my $bFirst = true;
        my $bInternal = false;

        # Build option data
        my $rhOption = $rhConfigDefine->{$strOption};

        # Get option help
        my $rhOptionHelp = $hConfigHelp->{&CONFIG_HELP_OPTION}{$strOption};

        # Build command data
        $strBuildSource .=
            "\n" .
            "        // ${strOption} option\n" .
            "        // " . (qw{-} x 121) . "\n";

        # Internal
        if ($rhOption->{&CFGDEF_INTERNAL})
        {
            $strBuildSource .= packTagFormat("Internal", PCK_TYPE_BOOL, $iDelta, true, 8);
            $iDelta = 0;
            $bInternal = true;
        }
        else
        {
            $iDelta++;
        }

        if (defined($rhOptionHelp))
        {
            # Section
            my $strSection = $rhOptionHelp->{&CONFIG_HELP_SECTION};

            if (defined($strSection))
            {
                if (length($strSection) > 72)
                {
                    confess("section for option '${strOption}' may not be greater than 72 characters");
                }

                $strBuildSource .= packTagFormat("Section", PCK_TYPE_STR, $iDelta, $strSection, 8);
                $iDelta = 0;
            }
            else
            {
                $iDelta++;
            }

            # Summary
            my $strSummary = trim($oManifest->variableReplace($oDocRender->processText($rhOptionHelp->{&CONFIG_HELP_SUMMARY})));

            if (length($strSummary) > 72)
            {
                confess("summary for option '${strOption}' may not be greater than 72 characters");
            }

            $strBuildSource .= packTagFormat("Summary", PCK_TYPE_STR, $iDelta, $strSummary, 8);
            $iDelta = 0;

            # Description
            $strBuildSource .= packTagFormat(
                "Description", PCK_TYPE_STR, $iDelta,
                trim($oManifest->variableReplace($oDocRender->processText($rhOptionHelp->{&CONFIG_HELP_DESCRIPTION}))), 8);

            $bFirst = false;
        }
        else
        {
            $iDelta += 3;
        }

        # Output deprecated names
        my $stryDeprecatedName = $rhOptionHelp->{&CONFIG_HELP_NAME_ALT};

        if (defined($stryDeprecatedName))
        {
            $strBuildSource .=
                ($bFirst ? '' : "\n") .
                packTagFormat("Deprecated names begin", PCK_TYPE_ARRAY, $iDelta, undef, 8);
            $iDelta = 0;

            foreach my $strDeprecatedName (@{$stryDeprecatedName})
            {
                $strBuildSource .= packTagFormat($strDeprecatedName, PCK_TYPE_STR, 0, $strDeprecatedName, 12);
            }

            $strBuildSource .=
                "        0x00, // Deprecated names end\n";

            $bFirst = false;
        }
        else
        {
            $iDelta++;
        }

        # Command overrides
        my $strBuildSourceCommands;
        my $iCommandId = 0;
        my $iLastCommandId = 0;

        foreach my $strCommand (sort(keys(%{$rhCommandDefine})))
        {
            my $rhCommand = $rhOption->{&CFGDEF_COMMAND}{$strCommand};
            my $iDeltaCommand = 0;
            my $strBuildSourceCommand;

            if (defined($rhCommand))
            {
                if ($bInternal && defined($rhCommand->{&CFGDEF_INTERNAL}) && !$rhCommand->{&CFGDEF_INTERNAL})
                {
                    confess("option '${strOption}' is internal but command '${strCommand}' override is not");
                }

                # Internal
                if (defined($rhCommand->{&CFGDEF_INTERNAL}) && $bInternal != $rhCommand->{&CFGDEF_INTERNAL})
                {
                    $strBuildSourceCommand .=
                        packTagFormat("Internal", PCK_TYPE_BOOL, $iDeltaCommand, true, 16);
                    $iDeltaCommand = 0;
                }
                else
                {
                    $iDeltaCommand++;
                }

                my $rhCommandHelp = $hConfigHelp->{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption};

                if (defined($rhCommandHelp->{&CONFIG_HELP_SOURCE}) &&
                    $rhCommandHelp->{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_COMMAND)
                {
                    # Summary
                    my $strSummary = trim(
                        $oManifest->variableReplace($oDocRender->processText($rhCommandHelp->{&CONFIG_HELP_SUMMARY})));

                    if (length($strSummary) > 72)
                    {
                        confess("summary for command '${strCommand}' option '${strOption}' may not be greater than 72 characters");
                    }

                    $strBuildSourceCommand .=
                        packTagFormat("Summary", PCK_TYPE_STR, $iDeltaCommand, $strSummary, 16);
                    $iDeltaCommand = 0;

                    # Description
                    $strBuildSourceCommand .= packTagFormat(
                        "Description", PCK_TYPE_STR, $iDeltaCommand,
                        trim($oManifest->variableReplace($oDocRender->processText($rhCommandHelp->{&CONFIG_HELP_DESCRIPTION}))),
                        16);
                }

                if (defined($strBuildSourceCommand))
                {
                    $strBuildSourceCommands .=
                        "\n" .
                        packTagFormat(
                        "Command ${strCommand} override begin", PCK_TYPE_OBJ, $iCommandId - $iLastCommandId, undef, 12) .
                        $strBuildSourceCommand .
                        "            0x00, // Command ${strCommand} override end\n";

                    $iLastCommandId = $iCommandId + 1;
                }
            }

            $iCommandId++;
        }

        if (defined($strBuildSourceCommands))
        {
            $strBuildSource .=
                ($bFirst ? '' : "\n") .
                packTagFormat("Command overrides begin", PCK_TYPE_ARRAY, $iDelta, undef, 8) .
                $strBuildSourceCommands . "\n" .
                "        0x00, // Command overrides end\n";
            $iDelta = 0;

            $bFirst = false;
        }
        else
        {
            $iDelta++;
        }
    }

    $strBuildSource .=
        "\n" .
        "    0x00, // Options end\n";

    $strBuildSource .=
        "\n" .
        "    0x00, // Pack end\n" .
        "};\n";

    $rhBuild->{&BLD_FILE}{&BLDLCL_FILE_DEFINE}{&BLD_DATA}{&BLDLCL_DATA_COMMAND}{&BLD_SOURCE} = $strBuildSource;

    return $rhBuild;
}

push @EXPORT, qw(buildConfigHelp);

1;
