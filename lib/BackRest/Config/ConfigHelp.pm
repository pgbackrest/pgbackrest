####################################################################################################################################
# CONFIG HELP MODULE
####################################################################################################################################
package BackRest::Config::ConfigHelp;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Exception;
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Config::Config;

####################################################################################################################################
# Help types
####################################################################################################################################
use constant CONFIG_HELP_COMMAND                                         => 'command';
     push @EXPORT, qw(CONFIG_HELP_COMMAND);
use constant CONFIG_HELP_CURRENT                                         => 'current';
     push @EXPORT, qw(CONFIG_HELP_CURRENT);
use constant CONFIG_HELP_DEFAULT                                         => 'default';
     push @EXPORT, qw(CONFIG_HELP_DEFAULT);
use constant CONFIG_HELP_DESCRIPTION                                     => 'description';
     push @EXPORT, qw(CONFIG_HELP_DESCRIPTION);
use constant CONFIG_HELP_OPTION                                          => 'option';
     push @EXPORT, qw(CONFIG_HELP_OPTION);
use constant CONFIG_HELP_SECTION                                         => 'section';
     push @EXPORT, qw(CONFIG_HELP_SECTION);
use constant CONFIG_HELP_SUMMARY                                         => 'summary';
     push @EXPORT, qw(CONFIG_HELP_SUMMARY);

use constant CONFIG_HELP_SOURCE                                          => 'source';
     push @EXPORT, qw(CONFIG_HELP_SOURCE);
use constant CONFIG_HELP_SOURCE_DEFAULT                                  => 'default';
     push @EXPORT, qw(CONFIG_HELP_SOURCE_DEFAULT);
use constant CONFIG_HELP_SOURCE_SECTION                                  => CONFIG_HELP_SECTION;
     push @EXPORT, qw(CONFIG_HELP_SOURCE_SECTION);
use constant CONFIG_HELP_SOURCE_COMMAND                                  => CONFIG_HELP_COMMAND;
     push @EXPORT, qw(CONFIG_HELP_SOURCE_COMMAND);

####################################################################################################################################
# configHelp
#
# Display command-line help.
####################################################################################################################################
sub configHelp
{
    my $strCommand = shift;
    my $strOption = shift;

    # Load module dynamically
    require BackRest::Config::ConfigHelpData;
    BackRest::Config::ConfigHelpData->import();

    # Get config data
    my $oCommandHash = commandHashGet();
    my $oOptionRule = optionRuleGet();
    my $oConfigHelpData = configHelpDataGet();

    # Build the title
    my $strTitle;
    my $strHelp;

    # Since there's no command this will be general help
    if (!defined($strCommand))
    {
        $strTitle = "General"
    }
    # Else check the command
    else
    {
        $strCommand = lc($strCommand);

        if (defined($$oCommandHash{$strCommand}))
        {
            $strTitle = "'${strCommand}' command";

            # And check the option
            if (defined($strOption))
            {
                $strOption = lc($strOption);

                if (defined($$oConfigHelpData{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption}))
                {
                    $strTitle .= " - '${strOption}' option";
                }
                else
                {
                    $strHelp = "Invalid option '${strOption}' for command '${strCommand}'";
                }
            }
        }
        else
        {
            $strHelp = "Invalid command '${strCommand}'";
        }
    }

    # Internal text format function to make output look good on a console
    sub formatText
    {
        my $strTextIn = shift;
        my $iIndent = shift;
        my $bIndentFirst = shift;
        my $iLength = shift;

        my @stryText = split("\n", trim($strTextIn));
        my $strText;
        my $iIndex = 0;

        foreach my $strLine (@stryText)
        {
            if (defined($strText))
            {
                $strText .= "\n";
            }

            my $strPart;
            my $bFirst = true;

            do
            {
                ($strPart, $strLine) = stringSplit($strLine, ' ', $iLength - $iIndent);

                if (!$bFirst || $bIndentFirst)
                {
                    if (!$bFirst)
                    {
                        $strText .= "\n";
                    }

                    $strText .= ' ' x $iIndent;
                }

                $strText .= trim($strPart);

                $bFirst = false;
            }
            while (defined($strLine));

            $iIndex++;
        }

        return $strText;
    }

    # Internal option find function
    #
    # The option may be stored with the command or in the option list depending on whether it's generic or command-specific
    sub optionFind
    {
        my $oConfigHelpData = shift;
        my $oOptionRule = shift;
        my $strCommand = shift;
        my $strOption = shift;

        my $strSection = CONFIG_HELP_COMMAND;
        my $oOption = $$oConfigHelpData{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption};

        if (ref(\$oOption) eq 'SCALAR')
        {
            $oOption = $$oConfigHelpData{&CONFIG_HELP_OPTION}{$strOption};

            if (defined($$oOption{&CONFIG_HELP_SECTION}))
            {
                $strSection = $$oOption{&CONFIG_HELP_SECTION};

                if ($strSection eq CONFIG_SECTION_COMMAND)
                {
                    $strSection = CONFIG_SECTION_GENERAL;
                }
            }
            else
            {
                $strSection = CONFIG_SECTION_GENERAL;
            }

            if (($strSection ne CONFIG_SECTION_GENERAL && $strSection ne CONFIG_SECTION_LOG &&
                 $strSection ne CONFIG_SECTION_STANZA && $strSection ne CONFIG_SECTION_EXPIRE) ||
                 $strSection eq $strCommand)
            {
                $strSection = CONFIG_HELP_COMMAND;
            }
        }

        if (defined(optionDefault($strOption, $strCommand)))
        {
            if ($$oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq &OPTION_TYPE_BOOLEAN)
            {
                $$oOption{&CONFIG_HELP_DEFAULT} = optionDefault($strOption, $strCommand) ? 'y' : 'n';
            }
            else
            {
                $$oOption{&CONFIG_HELP_DEFAULT} = optionDefault($strOption, $strCommand);
            }
        }

        if (optionTest($strOption) && optionSource($strOption) ne CONFIG_HELP_SOURCE_DEFAULT)
        {
            if ($$oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq &OPTION_TYPE_BOOLEAN)
            {
                $$oOption{&CONFIG_HELP_CURRENT} = optionGet($strOption) ? 'y' : 'n';
            }
            else
            {
                $$oOption{&CONFIG_HELP_CURRENT} = optionGet($strOption);
            }
        }

        return $oOption, $strSection;
    }

    # Build the help
    my $strMore;

    if (!defined($strHelp))
    {
        my $iScreenWidth = 80;

        # General help
        if (!defined($strCommand))
        {
            $strHelp =
                "Usage:\n" .
                "    " . BACKREST_EXE . " [options] [command]\n\n" .
                "Commands:\n";

            # Find longest command length
            my $iCommandLength = 0;

            foreach my $strCommand (sort(keys(%{$$oConfigHelpData{&CONFIG_HELP_COMMAND}})))
            {
                if (length($strCommand) > $iCommandLength)
                {
                    $iCommandLength = length($strCommand);
                }
            }

            # Output commands
            foreach my $strCommand (sort(keys(%{$$oConfigHelpData{&CONFIG_HELP_COMMAND}})))
            {
                my $oCommand = $$oConfigHelpData{&CONFIG_HELP_COMMAND}{$strCommand};

                $strHelp .= "    ${strCommand}" . (' ' x ($iCommandLength - length($strCommand)));
                $strHelp .=
                    '  ' . formatText($$oCommand{&CONFIG_HELP_SUMMARY}, 4 + $iCommandLength + 2, false, $iScreenWidth + 1) .
                    "\n";
            }

            $strMore = '[command]';
        }
        # Else command help
        elsif (!defined($strOption))
        {
            my $oCommand = $$oConfigHelpData{&CONFIG_HELP_COMMAND}{$strCommand};

            $strHelp =
                formatText($$oCommand{&CONFIG_HELP_SUMMARY} . "\n\n" .
                           $$oCommand{&CONFIG_HELP_DESCRIPTION}, 0, true, $iScreenWidth + 1);

            # Find longest option length and unique list of sections
            my $iOptionLength = 0;
            my $oSection = {};

            if (defined($$oCommand{&CONFIG_HELP_OPTION}))
            {
                foreach my $strOption (sort(keys(%{$$oCommand{&CONFIG_HELP_OPTION}})))
                {
                    if (length($strOption) > $iOptionLength)
                    {
                        $iOptionLength = length($strOption);
                    }

                    my ($oOption, $strSection) = optionFind($oConfigHelpData, $oOptionRule, $strCommand, $strOption);

                    $$oSection{$strSection}{$strOption} = $oOption;
                }

                # Iterate sections
                foreach my $strSection (sort(keys(%{$oSection})))
                {
                    $strHelp .=
                        "\n\n" . ucfirst($strSection) . " Options:\n";

                    # Iterate options
                    foreach my $strOption (sort(keys(%{$$oSection{$strSection}})))
                    {
                        $strHelp .= "\n";

                        my $iIndent = 4 + $iOptionLength + 2;
                        my $oOption = $$oSection{$strSection}{$strOption};

                        # Set current and default values
                        my $strDefault = '';

                        if ($$oOption{&CONFIG_HELP_CURRENT} || $$oOption{&CONFIG_HELP_DEFAULT})
                        {
                            $strDefault = undef;

                            if ($$oOption{&CONFIG_HELP_CURRENT})
                            {
                                $strDefault .= 'current=' . $$oOption{&CONFIG_HELP_CURRENT};
                            }

                            if ($$oOption{&CONFIG_HELP_DEFAULT})
                            {
                                if (defined($strDefault))
                                {
                                    $strDefault .= ', ';
                                }

                                $strDefault .= 'default=' . $$oOption{&CONFIG_HELP_DEFAULT};
                            }

                            $strDefault = " [${strDefault}]";
                        }

                        # Output help
                        $strHelp .= "  --${strOption}" . (' ' x ($iOptionLength - length($strOption)));
                        $strHelp .= '  ' . formatText(lcfirst(substr($$oOption{&CONFIG_HELP_SUMMARY}, 0,
                                                                     length($$oOption{&CONFIG_HELP_SUMMARY}) - 1)) .
                                                      $strDefault, $iIndent, false, $iScreenWidth + 1);
                    }
                }

                $strMore = "${strCommand} [option]";
                $strHelp .= "\n";
            }
        }
        # Else option help
        else
        {
            my ($oOption) = optionFind($oConfigHelpData, $oOptionRule, $strCommand, $strOption);

            # Set current and default values
            my $strDefault = '';

            if ($$oOption{&CONFIG_HELP_CURRENT} || $$oOption{&CONFIG_HELP_DEFAULT})
            {
                $strDefault = undef;

                if ($$oOption{&CONFIG_HELP_CURRENT})
                {
                    $strDefault = 'current: ' . $$oOption{&CONFIG_HELP_CURRENT};
                }

                if ($$oOption{&CONFIG_HELP_DEFAULT})
                {
                    if (defined($strDefault))
                    {
                        $strDefault .= "\n";
                    }

                    $strDefault .= 'default: ' . $$oOption{&CONFIG_HELP_DEFAULT};
                }

                $strDefault = "\n\n${strDefault}";
            }

            # Output help
            $strHelp =
                formatText($$oOption{&CONFIG_HELP_SUMMARY} . "\n\n" . $$oOption{&CONFIG_HELP_DESCRIPTION} .
                           $strDefault, 0, true, $iScreenWidth + 1);
        }
    }

    # Output help
    syswrite(*STDOUT, ' -' . (defined($strTitle) ? " ${strTitle}" : '') . " help\n\n${strHelp}\n" .
             (defined($strMore) ? 'Use \'' . BACKREST_EXE . " help ${strMore}' for more information.\n" : ''));
}

push @EXPORT, qw(configHelp);

1;
