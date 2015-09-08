####################################################################################################################################
# DOC CONFIG MODULE
####################################################################################################################################
package BackRestDoc::DocConfig;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Config::Config;
use BackRest::Config::Config::ConfigHelp;
use BackRest::FileCommon;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_DOC_CONFIG                                          => 'DocConfig';

use constant OP_DOC_CONFIG_NEW                                      => OP_DOC_CONFIG . '->new';
use constant OP_DOC_CONFIG_PROCESS                                  => OP_DOC_CONFIG . '->process';
use constant OP_DOC_CONFIG_HELP_DATA_WRITE                          => OP_DOC_CONFIG . '->helpDataWrite';

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oDoc},
        $self->{oDocRender}
    ) =
        logDebugParam
        (
            OP_DOC_CONFIG_NEW, \@_,
            {name => 'oDoc'},
            {name => 'oDocRender'}
        );

    $self->process();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# process
#
# Parse the xml doc into commands and options.
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(OP_DOC_CONFIG_PROCESS);

    # Iterate through all commands
    my $oDoc = $self->{oDoc};
    my $oConfigHash = {};

    foreach my $strCommand (sort(keys(commandHashGet())))
    {
        if ($strCommand eq CMD_REMOTE)
        {
            next;
        }

        my $oCommandDoc = $oDoc->nodeGet('operation')->nodeGet('command-list')->nodeGetById('command', $strCommand);

        $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand} = {};
        my $oCommand = $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand};

        $$oCommand{&CONFIG_HELP_SUMMARY} = $oCommandDoc->nodeGet('summary')->textGet();
        $$oCommand{&CONFIG_HELP_DESCRIPTION} = $oCommandDoc->textGet();
    }

    # Iterate through all options
    my $oOptionRule = optionRuleGet();

    foreach my $strOption (sort(keys($oOptionRule)))
    {
        if ($strOption =~ /^test/ || $strOption eq 'no-fork')
        {
            next;
        }

        # Iterate through all commands
        my @stryCommandList = sort(keys(defined($$oOptionRule{$strOption}{&OPTION_RULE_COMMAND}) ?
                              $$oOptionRule{$strOption}{&OPTION_RULE_COMMAND} : $$oConfigHash{&CONFIG_HELP_COMMAND}));

        foreach my $strCommand (@stryCommandList)
        {
            if (!defined($$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}))
            {
                next;
            }

            my $oCommandDoc = $oDoc->nodeGet('operation')->nodeGet('command-list')->nodeGetById('command', $strCommand);

            # First check if the option is documented in the command
            my $oOptionDoc;
            my $strOptionSource;
            my $oCommandOptionList = $oCommandDoc->nodeGet('option-list', false);

            if (defined($oCommandOptionList))
            {
                $oOptionDoc = $oCommandOptionList->nodeGetById('option', $strOption, false);

                $strOptionSource = CONFIG_HELP_SOURCE_COMMAND if (defined($oOptionDoc));
            }

            # If the option wasn't found keep looking
            my $strSection;

            if (!defined($oOptionDoc))
            {
                # Next see if it's documented in the section
                if (defined($$oOptionRule{$strOption}{&OPTION_RULE_SECTION}))
                {
                    $strSection = $$oOptionRule{$strOption}{&OPTION_RULE_SECTION};

                    if ($strSection eq '1')
                    {
                        if (defined($$oOptionRule{$strOption}{&OPTION_RULE_SECTION_INHERIT}))
                        {
                            $strSection = $$oOptionRule{$strOption}{&OPTION_RULE_SECTION_INHERIT};
                        }
                        else
                        {
                            $strSection = $strCommand;
                        }
                    }

                    # &log(INFO, "        trying section ${strSection}");

                    $oOptionDoc = $oDoc->nodeGet('config')->nodeGet('config-section-list')
                                       ->nodeGetById('config-section', $strSection)->nodeGetById('config-key-list')
                                       ->nodeGetById('config-key', $strOption, false);

                    $strOptionSource = CONFIG_HELP_SOURCE_SECTION if (defined($oOptionDoc));
                }
                # If no section is defined then look in the default command option list
                else
                {
                    $oOptionDoc = $oDoc->nodeGet('operation')->nodeGet('operation-general')->nodeGet('option-list')
                                       ->nodeGetById('option', $strOption, false);

                    $strOptionSource = CONFIG_HELP_SOURCE_DEFAULT if (defined($oOptionDoc));
                }
            }

            # If the option wasn't found then error
            if (!defined($oOptionDoc))
            {
                confess &log(ERROR, "unable to find option '${strOption}' for command '${strCommand}'")
            }

            # Store the option in the command
            $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption}{&CONFIG_HELP_SOURCE} = $strOptionSource;

            my $oCommandOption = $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption};

            $$oCommandOption{&CONFIG_HELP_SUMMARY} = $oOptionDoc->nodeGet('summary')->textGet();
            $$oCommandOption{&CONFIG_HELP_DESCRIPTION} = $oOptionDoc->textGet();

            # If the option did not come from the command also store in global option list.  This prevents duplication of commonly
            # used options.
            if ($strOptionSource ne CONFIG_HELP_SOURCE_COMMAND)
            {
                $$oConfigHash{&CONFIG_HELP_OPTION}{$strOption}{&CONFIG_HELP_SUMMARY} = $$oCommandOption{&CONFIG_HELP_SUMMARY};

                my $oOption = $$oConfigHash{&CONFIG_HELP_OPTION}{$strOption};

                if (defined($strSection))
                {
                    $$oOption{&CONFIG_HELP_SECTION} = $strSection;
                }

                $$oOption{&CONFIG_HELP_DESCRIPTION} = $$oCommandOption{&CONFIG_HELP_DESCRIPTION};
            }
        }
    }

    # Store the config hash
    $self->{oConfigHash} = $oConfigHash;

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

####################################################################################################################################
# helpDataWrite
#
# Write help data into a perl module so it can be accessed by backrest for command-line help.
####################################################################################################################################
sub helpDataWrite
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(OP_DOC_CONFIG_HELP_DATA_WRITE);

    # Internal function used to format text by quoting it and splitting lines so it looks good in the module.
    sub formatText
    {
        my $oDocRender = shift;
        my $oText = shift;
        my $iIndent = shift;
        my $iLength = shift;

        # Split the string into lines for processing
        my @stryText = split("\n", trim($oDocRender->processText($oText)));
        my $strText;
        my $iIndex = 0;

        foreach my $strLine (@stryText)
        {
            # Add a linefeed if this is not the first line
            if (defined($strText))
            {
                $strText .= " .\n";
            }

            # Escape perl special characters
            $strLine =~ s/\@/\\@/g;
            $strLine =~ s/\$/\\\$/g;
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
                    $strText .= "\" .\n";
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

    # Iterate options
    my $oConfigHash = $self->{oConfigHash};
    my $strOptionData;

    foreach my $strOption (sort(keys($$oConfigHash{&CONFIG_HELP_OPTION})))
    {
        my $oOptionHash = $$oConfigHash{&CONFIG_HELP_OPTION}{$strOption};

        if (defined($strOptionData))
        {
            $strOptionData .= ",\n\n";
        }

        # Format the option for output
        $strOptionData .=
            '        # ' . uc($strOption) . " Option Help\n" .
            '        #' . ('-' x 123) . "\n" .
            "        '${strOption}' =>\n" .
            "        {\n" .
            (defined($$oOptionHash{&CONFIG_HELP_SECTION}) ? '            ' . &CONFIG_HELP_SECTION .
                ' => \'' . $$oOptionHash{&CONFIG_HELP_SECTION} . "',\n" : '') .
            '            ' . &CONFIG_HELP_SUMMARY . " =>\n" .
            formatText($self->{oDocRender}, $$oOptionHash{&CONFIG_HELP_SUMMARY}, 16, 112) . ",\n" .
            '            ' . &CONFIG_HELP_DESCRIPTION . " =>\n" .
            formatText($self->{oDocRender}, $$oOptionHash{&CONFIG_HELP_DESCRIPTION}, 16, 112) . "\n" .
            "        }";
    }

    # Iterate commands
    my $strCommandData;

    foreach my $strCommand (sort(keys($$oConfigHash{&CONFIG_HELP_COMMAND})))
    {
        my $oCommandHash = $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand};

        if (defined($strCommandData))
        {
            $strCommandData .= ",\n\n";
        }

        # Format the command for output
        $strCommandData .=
            '        # ' . uc($strCommand) . " Command Help\n" .
            '        #' . ('-' x 123) . "\n" .
            "        '${strCommand}' =>\n" .
            "        {\n" .
            '            ' . &CONFIG_HELP_SUMMARY . " =>\n" .
            formatText($self->{oDocRender}, $$oCommandHash{&CONFIG_HELP_SUMMARY}, 16, 112) . ",\n" .
            '            ' . &CONFIG_HELP_DESCRIPTION . " =>\n" .
            formatText($self->{oDocRender}, $$oCommandHash{&CONFIG_HELP_DESCRIPTION}, 16, 112) . ",\n" .
            "\n";

        # Iterate options
        my $strOptionData;
        my $bExtraLinefeed = false;

        if (defined($$oCommandHash{&CONFIG_HELP_OPTION}))
        {
            $strCommandData .=
                '            ' . CONFIG_HELP_OPTION . " =>\n" .
                "            {\n";

            foreach my $strOption (sort(keys($$oCommandHash{&CONFIG_HELP_OPTION})))
            {
                my $oOptionHash = $$oCommandHash{&CONFIG_HELP_OPTION}{$strOption};

                if (defined($strOptionData))
                {
                    $strOptionData .= ",\n";
                }

                # If option came from the command then output details
                if ($$oOptionHash{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_COMMAND)
                {
                    if (defined($strOptionData))
                    {
                        $strOptionData .= "\n";
                    }

                    # Format the command option for output
                    $strOptionData .=
                        '                # ' . uc($strOption) . " Option Help\n" .
                        '                #' . ('-' x 115) . "\n" .
                        "                '${strOption}' =>\n" .
                        "                {\n" .
                        '                    ' . &CONFIG_HELP_SUMMARY . " =>\n" .
                        formatText($self->{oDocRender}, $$oOptionHash{&CONFIG_HELP_SUMMARY}, 24, 104) . ",\n" .
                        '                    ' . &CONFIG_HELP_DESCRIPTION . " =>\n" .
                        formatText($self->{oDocRender}, $$oOptionHash{&CONFIG_HELP_DESCRIPTION}, 24, 104) . "\n" .
                        "                }";

                    $bExtraLinefeed = true;
                }
                # Else output a reference to indicate the option is in the global option list
                else
                {
                    if ($bExtraLinefeed)
                    {
                        $strOptionData .= "\n";
                        $bExtraLinefeed = false;
                    }

                    $strOptionData .=
                        "                '${strOption}' => '" . $$oOptionHash{&CONFIG_HELP_SOURCE} . "'";
                }
            }

            $strCommandData .=
                $strOptionData . "\n" .
                "            }\n";
        }

        $strCommandData .=
            "        }";
    }

    # Format the perl module
    my $strHelpData =
        ('#' x 132) . "\n" .
        "# CONFIG HELP DATA MODULE\n" .
        "#\n" .
        "# This module is automatically generated by doc.pl and should never be manually edited.\n" .
        ('#' x 132) . "\n" .
        "package BackRest::Config::Config::ConfigHelpData;\n" .
        "\n" .
        "use strict;\n" .
        "use warnings FATAL => qw(all);\n" .
        "use Carp qw(confess);\n" .
        "\n" .
        "use Exporter qw(import);\n" .
        "    our \@EXPORT = qw();\n" .
        "\n" .
        ('#' x 132) . "\n" .
        "# Data used by the ConfigHelp module to generate command-line help\n" .
        ('#' x 132) . "\n" .
        "my \$oConfigHelpData =\n".
        "{\n" .
        "    # Option Help\n" .
        '    #' . ('-' x 127) . "\n" .
        "    " . CONFIG_HELP_OPTION . " =>\n" .
        "    {\n" .
        $strOptionData . "\n" .
        "    },\n" .
        "\n" .
        "    # Command Help\n" .
        '    #' . ('-' x 127) . "\n" .
        "    " . CONFIG_HELP_COMMAND . " =>\n" .
        "    {\n" .
        $strCommandData . "\n" .
        "    }\n" .
        "};\n" .
        "\n" .
        ('#' x 132) . "\n" .
        "# configHelpDataGet\n" .
        ('#' x 132) . "\n" .
        "sub configHelpDataGet\n" .
        "{\n" .
        "    return \$oConfigHelpData;\n" .
        "}\n" .
        "\n" .
        "push \@EXPORT, qw(configHelpDataGet);\n" .
        "\n" .
        "1;\n";

    # Write the perl module into the lib path
    fileStringWrite(dirname(dirname($0)) . '/lib/BackRest/Config/ConfigHelpData.pm', $strHelpData, false);

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
