####################################################################################################################################
# DOC CONFIG MODULE
####################################################################################################################################
package BackRestDoc::Common::DocConfig;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::Config::ConfigHelp;
use pgBackRest::Version;

####################################################################################################################################
# Help types
####################################################################################################################################
use constant CONFIG_HELP_NAME                                       => 'name';
use constant CONFIG_HELP_EXAMPLE                                    => 'example';

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
            __PACKAGE__ . '->new', \@_,
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
    my $strOperation = logDebugParam(__PACKAGE__ . '->process');

    # Iterate through all commands
    my $oDoc = $self->{oDoc};
    my $oConfigHash = {};

    foreach my $strCommand (sort(keys(%{commandHashGet()})))
    {
        if ($strCommand eq CMD_REMOTE || $strCommand eq CMD_LOCAL)
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

    foreach my $strOption (sort(keys(%{$oOptionRule})))
    {
        if ($strOption =~ /^test/ || $strOption eq OPTION_ARCHIVE_MAX_MB)
        {
            next;
        }

        # Iterate through all commands
        my @stryCommandList = sort(keys(%{defined($$oOptionRule{$strOption}{&OPTION_RULE_COMMAND}) ?
                              $$oOptionRule{$strOption}{&OPTION_RULE_COMMAND} : $$oConfigHash{&CONFIG_HELP_COMMAND}}));

        foreach my $strCommand (@stryCommandList)
        {
            if (!defined($$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}))
            {
                next;
            }

            if (ref(\$$oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand}) eq 'SCALAR' &&
                $$oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand} == false)
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
                    # &log(INFO, "        trying section ${strSection}");
                    foreach my $oSectionNode ($oDoc->nodeGet('config')->nodeGet('config-section-list')->nodeList())
                    {
                        my $oOptionDocCheck = $oSectionNode->nodeGetById('config-key-list')
                                                           ->nodeGetById('config-key', $strOption, false);

                        if ($oOptionDocCheck)
                        {
                            if (defined($oOptionDoc))
                            {
                                confess 'option exists in more than one section';
                            }

                            $oOptionDoc = $oOptionDocCheck;
                            $strOptionSource = CONFIG_HELP_SOURCE_SECTION;
                            $strSection = $oSectionNode->paramGet('id');
                        }
                    }
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

            # if the option is documented in the command then it should be accessible from the command line only.
            if (!defined($strSection))
            {
                if (defined($$oOptionRule{$strOption}{&OPTION_RULE_SECTION}))
                {
                    &log(ERROR, "option ${strOption} defined in command ${strCommand} must not have " . OPTION_RULE_SECTION .
                                " defined");
                }
            }

            # Store the option in the command
            $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption}{&CONFIG_HELP_SOURCE} = $strOptionSource;

            my $oCommandOption = $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption};

            $$oCommandOption{&CONFIG_HELP_SUMMARY} = $oOptionDoc->nodeGet('summary')->textGet();
            $$oCommandOption{&CONFIG_HELP_DESCRIPTION} = $oOptionDoc->textGet();
            $$oCommandOption{&CONFIG_HELP_EXAMPLE} = $oOptionDoc->fieldGet('example');

            $$oCommandOption{&CONFIG_HELP_NAME} = $oOptionDoc->paramGet('name');

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

                $$oOption{&CONFIG_HELP_NAME} = $oOptionDoc->paramGet('name');
                $$oOption{&CONFIG_HELP_DESCRIPTION} = $$oCommandOption{&CONFIG_HELP_DESCRIPTION};
                $$oOption{&CONFIG_HELP_EXAMPLE} = $oOptionDoc->fieldGet('example');
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
    my
    (
        $strOperation,
        $oManifest
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->helpDataWrite', \@_,
            {name => 'oManifest'}
        );

    # Iterate options
    my $oConfigHash = $self->{oConfigHash};
    my $strOptionData;

    foreach my $strOption (sort(keys(%{$$oConfigHash{&CONFIG_HELP_OPTION}})))
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
            helpDataWriteFormatText($oManifest, $self->{oDocRender}, $$oOptionHash{&CONFIG_HELP_SUMMARY}, 16, 112) . ",\n" .
            '            ' . &CONFIG_HELP_DESCRIPTION . " =>\n" .
            helpDataWriteFormatText($oManifest, $self->{oDocRender}, $$oOptionHash{&CONFIG_HELP_DESCRIPTION}, 16, 112) . "\n" .
            "        }";
    }

    # Iterate commands
    my $strCommandData;

    foreach my $strCommand (sort(keys(%{$$oConfigHash{&CONFIG_HELP_COMMAND}})))
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
            helpDataWriteFormatText($oManifest, $self->{oDocRender}, $$oCommandHash{&CONFIG_HELP_SUMMARY}, 16, 112) . ",\n" .
            '            ' . &CONFIG_HELP_DESCRIPTION . " =>\n" .
            helpDataWriteFormatText($oManifest, $self->{oDocRender}, $$oCommandHash{&CONFIG_HELP_DESCRIPTION}, 16, 112) . ",\n" .
            "\n";

        # Iterate options
        my $strOptionData;
        my $bExtraLinefeed = false;

        if (defined($$oCommandHash{&CONFIG_HELP_OPTION}))
        {
            $strCommandData .=
                '            ' . CONFIG_HELP_OPTION . " =>\n" .
                "            {\n";

            foreach my $strOption (sort(keys(%{$$oCommandHash{&CONFIG_HELP_OPTION}})))
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
                        helpDataWriteFormatText($oManifest, $self->{oDocRender},
                                                $$oOptionHash{&CONFIG_HELP_SUMMARY}, 24, 104) . ",\n" .
                        '                    ' . &CONFIG_HELP_DESCRIPTION . " =>\n" .
                        helpDataWriteFormatText($oManifest, $self->{oDocRender},
                                                $$oOptionHash{&CONFIG_HELP_DESCRIPTION}, 24, 104) . "\n" .
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
        "package pgBackRest::Config::ConfigHelpData;\n" .
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
    $oManifest->storage()->put(dirname(dirname($0)) . '/lib/pgBackRest/Config/ConfigHelpData.pm', $strHelpData);

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

# Helper function for helpDataWrite() used to format text by quoting it and splitting lines so it looks good in the module.
sub helpDataWriteFormatText
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

####################################################################################################################################
# manGet
#
# Generate the man page.
####################################################################################################################################
sub manGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oManifest
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->manGet', \@_,
            {name => 'oManifest'}
        );

    # Get index.xml to pull various text from
    my $oIndexDoc = ${$oManifest->sourceGet('index')}{doc};

    # Write the header
    my $strManPage =
        "NAME\n" .
        '  ' . BACKREST_NAME . ' - ' . $oManifest->variableReplace($oIndexDoc->paramGet('subtitle')) . "\n\n" .
        "SYNOPSIS\n" .
        '  ' . BACKREST_EXE . ' [options] [command]';

    # Output the description (first two paragraphs of index.xml introduction)
    my $iParaTotal = 0;

    $strManPage .= "\n\n" .
        "DESCRIPTION";

    foreach my $oPara ($oIndexDoc->nodeGetById('section', 'introduction')->nodeList('p'))
    {
        if ($iParaTotal >= 2)
        {
            last;
        }

        $strManPage .= ($iParaTotal == 0 ? "\n" : "\n\n") . '  ' .
            manGetFormatText($oManifest->variableReplace($self->{oDocRender}->processText($oPara->textGet())), 80, 2);

        $iParaTotal++;
    }

    # Build command and config hashes
    my $hOptionRule = optionRuleGet();
    my $hConfig = $self->{oConfigHash};
    my $hCommandList = {};
    my $iCommandMaxLen = 0;
    my $hOptionList = {};
    my $iOptionMaxLen = 0;

    foreach my $strCommand (sort(keys(%{$$hConfig{&CONFIG_HELP_COMMAND}})))
    {
        my $hCommand = $$hConfig{&CONFIG_HELP_COMMAND}{$strCommand};
        $iCommandMaxLen = length($strCommand) > $iCommandMaxLen ? length($strCommand) : $iCommandMaxLen;

        $$hCommandList{$strCommand}{summary} = $$hCommand{&CONFIG_HELP_SUMMARY};

        if (defined($$hCommand{&CONFIG_HELP_OPTION}))
        {
            foreach my $strOption (sort(keys(%{$$hCommand{&CONFIG_HELP_OPTION}})))
            {
                my $hOption = $$hCommand{&CONFIG_HELP_OPTION}{$strOption};

                if ($$hOption{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_COMMAND)
                {
                    $iOptionMaxLen = length($strOption) > $iOptionMaxLen ? length($strOption) : $iOptionMaxLen;

                    $$hOptionList{$strCommand}{$strOption}{&CONFIG_HELP_SUMMARY} = $$hOption{&CONFIG_HELP_SUMMARY};
                }
            }
        }
    }

    foreach my $strOption (sort(keys(%{$$hConfig{&CONFIG_HELP_OPTION}})))
    {
        my $hOption = $$hConfig{&CONFIG_HELP_OPTION}{$strOption};
        $iOptionMaxLen = length($strOption) > $iOptionMaxLen ? length($strOption) : $iOptionMaxLen;
        my $strSection = defined($$hOption{&CONFIG_HELP_SECTION}) ? $$hOption{&CONFIG_HELP_SECTION} : CONFIG_SECTION_GENERAL;

        $$hOptionList{$strSection}{$strOption}{&CONFIG_HELP_SUMMARY} = $$hOption{&CONFIG_HELP_SUMMARY};
    }

    # Output Commands
    $strManPage .= "\n\n" .
        'COMMANDS';

    foreach my $strCommand (sort(keys(%{$hCommandList})))
    {
        # Construct the summary
        my $strSummary = $oManifest->variableReplace($self->{oDocRender}->processText($$hCommandList{$strCommand}{summary}));
        # $strSummary = lcfirst(substr($strSummary, 0, length($strSummary) - 1));

        # Output the summary
        $strManPage .=
            "\n  " . "${strCommand}" . (' ' x ($iCommandMaxLen - length($strCommand))) . '  ' .
            manGetFormatText($strSummary, 80, $iCommandMaxLen + 4);
    }

    # Output options
    my $bFirst = true;
    $strManPage .= "\n\n" .
        'OPTIONS';

    foreach my $strSection (sort(keys(%{$hOptionList})))
    {
        $strManPage .= ($bFirst ?'' : "\n") . "\n  " . ucfirst($strSection) . ' Options:';

        foreach my $strOption (sort(keys(%{$$hOptionList{$strSection}})))
        {
            my $hOption = $$hOptionList{$strSection}{$strOption};

            # Contruct the default
            my $strCommand = defined(${commandHashGet()}{$strSection}) ? $strSection : undef;
            my $strDefault = optionDefault($strOption, $strCommand);

            if (defined($strDefault))
            {
                if ($strOption eq OPTION_BACKUP_CMD || $strOption eq OPTION_DB_CMD)
                {
                    $strDefault = BACKREST_EXE;
                }
                elsif ($$hOptionRule{$strOption}{&OPTION_RULE_TYPE} eq &OPTION_TYPE_BOOLEAN)
                {
                    $strDefault = $strDefault ? 'y' : 'n';
                }
            }
            #
            # use Data::Dumper; confess Dumper($$hOption{&CONFIG_HELP_SUMMARY});

            # Construct the summary
            my $strSummary = $oManifest->variableReplace($self->{oDocRender}->processText($$hOption{&CONFIG_HELP_SUMMARY}));

            $strSummary = $strSummary . (defined($strDefault) ? " [default=${strDefault}]" : '');

            # Output the summary
            $strManPage .=
                "\n    " . "--${strOption}" . (' ' x ($iOptionMaxLen - length($strOption))) . '  ' .
                manGetFormatText($strSummary, 80, $iOptionMaxLen + 8);
        }

        $bFirst = false;
    }

    # Write files, examples, and references
    $strManPage .= "\n\n" .
        "FILES\n" .
        "\n" .
        '  ' . OPTION_DEFAULT_CONFIG . "\n" .
        '  ' . OPTION_DEFAULT_REPO_PATH . "\n" .
        '  ' . OPTION_DEFAULT_LOG_PATH . "\n" .
        '  ' . OPTION_DEFAULT_SPOOL_PATH . "\n" .
        '  ' . OPTION_DEFAULT_LOCK_PATH . "\n" .
        "\n" .
        "EXAMPLES\n" .
        "\n" .
        "  * Create a backup of the PostgreSQL `main` cluster:\n" .
        "\n" .
        '    $ ' . BACKREST_EXE . ' --' . OPTION_STANZA . "=main backup\n" .
        "\n" .
        '    The `main` cluster should be configured in `' . OPTION_DEFAULT_CONFIG . "`\n" .
        "\n" .
        "  * Show all available backups:\n" .
        "\n" .
        '    $ ' . BACKREST_EXE . ' ' . CMD_INFO . "\n" .
        "\n" .
        "  * Show all available backups for a specific cluster:\n" .
        "\n" .
        '    $ ' . BACKREST_EXE . ' --' . OPTION_STANZA . '=main ' . CMD_INFO . "\n" .
        "\n" .
        "  * Show backup specific options:\n" .
        "\n" .
        '    $ ' . BACKREST_EXE . ' ' . CMD_HELP . ' ' . CMD_BACKUP . "\n" .
        "\n" .
        "SEE ALSO\n" .
        "\n" .
	    '  /usr/share/doc/' . BACKREST_EXE . "-doc/html/index.html\n" .
        '  ' . $oManifest->variableReplace('{[backrest-url-base]}') . "\n";

    return $strManPage;
}

# Helper function for manGet() used to format text by indenting and splitting
sub manGetFormatText
{
    my $strLine = shift;
    my $iLength = shift;
    my $iIndentRest = shift;

    my $strPart;
    my $strResult;
    my $bFirst = true;

    do
    {
        my $iIndent = $bFirst ? 0 : $iIndentRest;

        ($strPart, $strLine) = stringSplit($strLine, ' ', $iLength - $iIndentRest);

        $strResult .= ($bFirst ? '' : "\n") . (' ' x $iIndent) . trim($strPart);

        $bFirst = false;
    }
    while (defined($strLine));

    return $strResult;
}

####################################################################################################################################
# helpConfigDocGet
#
# Get the xml for configuration help.
####################################################################################################################################
sub helpConfigDocGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->helpConfigDocGet');

    # Build a hash of the sections
    my $oConfigHash = $self->{oConfigHash};
    my $oConfigDoc = $self->{oDoc}->nodeGet('config');
    my $oSectionHash = {};

    foreach my $strOption (sort(keys(%{$$oConfigHash{&CONFIG_HELP_OPTION}})))
    {
        my $oOption = $$oConfigHash{&CONFIG_HELP_OPTION}{$strOption};

        if (defined($$oOption{&CONFIG_HELP_SECTION}))
        {
            $$oSectionHash{$$oOption{&CONFIG_HELP_SECTION}}{$strOption} = true;
        }
    }

    my $oDoc = new BackRestDoc::Common::Doc();
    $oDoc->paramSet('title', $oConfigDoc->paramGet('title'));

    # set the description for use as a meta tag
    $oDoc->fieldSet('description', $oConfigDoc->fieldGet('description'));

    # Output the introduction
    my $oIntroSectionDoc = $oDoc->nodeAdd('section', undef, {id => 'introduction'});
    $oIntroSectionDoc->nodeAdd('title')->textSet('Introduction');
    $oIntroSectionDoc->textSet($oConfigDoc->textGet());

    foreach my $strSection (sort(keys(%{$oSectionHash})))
    {
        my $oSectionElement = $oDoc->nodeAdd('section', undef, {id => "section-${strSection}"});

        my $oSectionDoc = $oConfigDoc->nodeGet('config-section-list')->nodeGetById('config-section', $strSection);

        $oSectionElement->
            nodeAdd('title')->textSet(
                {name => 'text',
                 children=> [$oSectionDoc->paramGet('name') . ' Options (', {name => 'id', value => $strSection}, ')']});

        foreach my $strOption (sort(keys(%{$$oSectionHash{$strSection}})))
        {
            $self->helpOptionGet(undef, $strOption, $oSectionElement, $$oConfigHash{&CONFIG_HELP_OPTION}{$strOption});
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDoc', value => $oDoc}
    );
}

####################################################################################################################################
# helpCommandDocGet
#
# Get the xml for command help.
####################################################################################################################################
sub helpCommandDocGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->helpCommandDocGet');

    # Working variables
    my $oConfigHash = $self->{oConfigHash};
    my $oOperationDoc = $self->{oDoc}->nodeGet('operation');
    my $oOptionRule = optionRuleGet();

    my $oDoc = new BackRestDoc::Common::Doc();
    $oDoc->paramSet('title', $oOperationDoc->paramGet('title'));

    # set the description for use as a meta tag
    $oDoc->fieldSet('description', $oOperationDoc->fieldGet('description'));

    # Output the introduction
    my $oIntroSectionDoc = $oDoc->nodeAdd('section', undef, {id => 'introduction'});
    $oIntroSectionDoc->nodeAdd('title')->textSet('Introduction');
    $oIntroSectionDoc->textSet($oOperationDoc->textGet());

    foreach my $strCommand (sort(keys(%{$$oConfigHash{&CONFIG_HELP_COMMAND}})))
    {
        my $oCommandHash = $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand};
        my $oSectionElement = $oDoc->nodeAdd('section', undef, {id => "command-${strCommand}"});

        my $oCommandDoc = $oOperationDoc->nodeGet('command-list')->nodeGetById('command', $strCommand);

        $oSectionElement->
            nodeAdd('title')->textSet(
                {name => 'text',
                 children=> [$oCommandDoc->paramGet('name') . ' Command (', {name => 'id', value => $strCommand}, ')']});

        $oSectionElement->textSet($$oCommandHash{&CONFIG_HELP_DESCRIPTION});

        # use Data::doc;
        # confess Dumper($oDoc->{oDoc});

        if (defined($$oCommandHash{&CONFIG_HELP_OPTION}))
        {
            my $oCategory = {};

            foreach my $strOption (sort(keys(%{$$oCommandHash{&CONFIG_HELP_OPTION}})))
            {
                my ($oOption, $strCategory) = helpCommandDocGetOptionFind($oConfigHash, $oOptionRule, $strCommand, $strOption);

                $$oCategory{$strCategory}{$strOption} = $oOption;
            }

            # Iterate sections
            foreach my $strCategory (sort(keys(%{$oCategory})))
            {
                my $oOptionListElement = $oSectionElement->nodeAdd('section', undef, {id => "category-${strCategory}"});

                $oOptionListElement->
                    nodeAdd('title')->textSet(ucfirst($strCategory) . ' Options');

                # Iterate options
                foreach my $strOption (sort(keys(%{$$oCategory{$strCategory}})))
                {
                    $self->helpOptionGet($strCommand, $strOption, $oOptionListElement,
                                         $$oCommandHash{&CONFIG_HELP_OPTION}{$strOption});
                }
            }
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oDoc', value => $oDoc}
    );
}

# Helper function for helpCommandDocGet() to find options. The option may be stored with the command or in the option list depending
# on whether it's generic or command-specific
sub helpCommandDocGetOptionFind
{
    my $oConfigHelpData = shift;
    my $oOptionRule = shift;
    my $strCommand = shift;
    my $strOption = shift;

    my $strSection = CONFIG_HELP_COMMAND;
    my $oOption = $$oConfigHelpData{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption};

    if ($$oOption{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_DEFAULT)
    {
        $strSection = CONFIG_SECTION_GENERAL;
    }
    elsif ($$oOption{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_SECTION)
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

    return $oOption, $strSection;
}

####################################################################################################################################
# helpOptionGet
#
# Get the xml for an option.
####################################################################################################################################
sub helpOptionGet
{
    my $self = shift;
    my $strCommand = shift;
    my $strOption = shift;
    my $oParentElement = shift;
    my $oOptionHash = shift;

    # Create the option section
    my $oOptionElement = $oParentElement->nodeAdd('section', undef, {id => "option-${strOption}"});

    # Set the option section title
    $oOptionElement->
        nodeAdd('title')->textSet(
            {name => 'text',
             children=> [$$oOptionHash{&CONFIG_HELP_NAME} . ' Option (', {name => 'id', value => "--${strOption}"}, ')']});

    # Add the option summary and description
    $oOptionElement->
        nodeAdd('p')->textSet($$oOptionHash{&CONFIG_HELP_SUMMARY});

    $oOptionElement->
        nodeAdd('p')->textSet($$oOptionHash{&CONFIG_HELP_DESCRIPTION});

    # Get the default value (or required=n if there is no default)
    my $strCodeBlock;

    if (defined(optionDefault($strOption, $strCommand)))
    {
        my $strDefault;

        if ($strOption eq OPTION_BACKUP_CMD || $strOption eq OPTION_DB_CMD)
        {
            $strDefault = '[INSTALL-PATH]/' . BACKREST_EXE;
        }
        else
        {
            if (optionTypeTest($strOption, OPTION_TYPE_BOOLEAN))
            {
                $strDefault = optionDefault($strOption, $strCommand) ? 'y' : 'n';
            }
            else
            {
                $strDefault = optionDefault($strOption, $strCommand);
            }
        }

        $strCodeBlock = "default: ${strDefault}";
    }
    # This won't work correctly until there is some notion of dependency
    # elsif (optionRequired($strOption, $strCommand))
    # {
    #     $strCodeBlock = 'required: y';
    # }

    # Get the allowed range if it exists
    my ($strRangeMin, $strRangeMax) = optionRange($strOption, $strCommand);

    if (defined($strRangeMin))
    {
        $strCodeBlock .= (defined($strCodeBlock) ? "\n" : '') . "allowed: ${strRangeMin}-${strRangeMax}";
    }

    # Get the example
    my $strExample;

    if (defined($strCommand))
    {
        if (optionTypeTest($strOption, OPTION_TYPE_BOOLEAN))
        {
            if ($$oOptionHash{&CONFIG_HELP_EXAMPLE} ne 'n' && $$oOptionHash{&CONFIG_HELP_EXAMPLE} ne 'y')
            {
                confess &log(ERROR, "option ${strOption} example should be boolean but value is: " .
                             $$oOptionHash{&CONFIG_HELP_EXAMPLE});
            }

            $strExample = '--' . ($$oOptionHash{&CONFIG_HELP_EXAMPLE} eq 'n' ? 'no-' : '') . $strOption;
        }
        else
        {
            $strExample = "--${strOption}=" . $$oOptionHash{&CONFIG_HELP_EXAMPLE};
        }
    }
    else
    {
        $strExample = "${strOption}=" . $$oOptionHash{&CONFIG_HELP_EXAMPLE};
    }

    $strCodeBlock .= (defined($strCodeBlock) ? "\n" : '') . "example: ${strExample}";

    $oOptionElement->
        nodeAdd('code-block')->valueSet($strCodeBlock);
}

1;
