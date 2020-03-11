####################################################################################################################################
# DOC CONFIG MODULE
####################################################################################################################################
package pgBackRestDoc::Common::DocConfig;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRestBuild::Config::Data;

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

####################################################################################################################################
# Help types
####################################################################################################################################
use constant CONFIG_HELP_COMMAND                                    => 'command';
    push @EXPORT, qw(CONFIG_HELP_COMMAND);
use constant CONFIG_HELP_CURRENT                                    => 'current';
use constant CONFIG_HELP_DEFAULT                                    => 'default';
use constant CONFIG_HELP_DESCRIPTION                                => 'description';
    push @EXPORT, qw(CONFIG_HELP_DESCRIPTION);
use constant CONFIG_HELP_EXAMPLE                                    => 'example';
use constant CONFIG_HELP_NAME                                       => 'name';
use constant CONFIG_HELP_NAME_ALT                                   => 'name-alt';
    push @EXPORT, qw(CONFIG_HELP_NAME_ALT);
use constant CONFIG_HELP_OPTION                                     => 'option';
    push @EXPORT, qw(CONFIG_HELP_OPTION);
use constant CONFIG_HELP_SECTION                                    => 'section';
    push @EXPORT, qw(CONFIG_HELP_SECTION);
use constant CONFIG_HELP_SUMMARY                                    => 'summary';
    push @EXPORT, qw(CONFIG_HELP_SUMMARY);

use constant CONFIG_HELP_SOURCE                                     => 'source';
    push @EXPORT, qw(CONFIG_HELP_SOURCE);
use constant CONFIG_HELP_SOURCE_DEFAULT                             => 'default';
use constant CONFIG_HELP_SOURCE_SECTION                             => CONFIG_HELP_SECTION;
use constant CONFIG_HELP_SOURCE_COMMAND                             => CONFIG_HELP_COMMAND;
    push @EXPORT, qw(CONFIG_HELP_SOURCE_COMMAND);

####################################################################################################################################
# Config Section Types
####################################################################################################################################
use constant CFGDEF_COMMAND                                 => 'command';
use constant CFGDEF_GENERAL                                 => 'general';
use constant CFGDEF_LOG                                     => 'log';
use constant CFGDEF_REPOSITORY                              => 'repository';

####################################################################################################################################
# Option define hash
####################################################################################################################################
my $rhConfigDefine = cfgDefine();

####################################################################################################################################
# Returns the option defines based on the command.
####################################################################################################################################
sub docConfigCommandDefine
{
    my $strOption = shift;
    my $strCommand = shift;

    if (defined($strCommand))
    {
        return defined($rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}) &&
               defined($rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}) &&
               ref($rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}) eq 'HASH' ?
               $rhConfigDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand} : undef;
    }

    return;
}

####################################################################################################################################
# Does the option have a default for this command?
####################################################################################################################################
sub docConfigOptionDefault
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command define
    my $oCommandDefine = docConfigCommandDefine($strOption, $strCommand);

    # Check for default in command
    my $strDefault = defined($oCommandDefine) ? $$oCommandDefine{&CFGDEF_DEFAULT} : undef;

    # If defined return, else try to grab the global default
    return defined($strDefault) ? $strDefault : $rhConfigDefine->{$strOption}{&CFGDEF_DEFAULT};
}

push @EXPORT, qw(docConfigOptionDefault);

####################################################################################################################################
# Get the allowed setting range for the option if it exists
####################################################################################################################################
sub docConfigOptionRange
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command define
    my $oCommandDefine = docConfigCommandDefine($strOption, $strCommand);

    # Check for default in command
    if (defined($oCommandDefine) && defined($$oCommandDefine{&CFGDEF_ALLOW_RANGE}))
    {
        return $$oCommandDefine{&CFGDEF_ALLOW_RANGE}[0], $$oCommandDefine{&CFGDEF_ALLOW_RANGE}[1];
    }

    # If defined return, else try to grab the global default
    return $rhConfigDefine->{$strOption}{&CFGDEF_ALLOW_RANGE}[0], $rhConfigDefine->{$strOption}{&CFGDEF_ALLOW_RANGE}[1];
}

push @EXPORT, qw(docConfigOptionRange);

####################################################################################################################################
# Get the option type
####################################################################################################################################
sub docConfigOptionType
{
    my $strOption = shift;

    return $rhConfigDefine->{$strOption}{&CFGDEF_TYPE};
}

push @EXPORT, qw(docConfigOptionType);

####################################################################################################################################
# Test the option type
####################################################################################################################################
sub docConfigOptionTypeTest
{
    my $strOption = shift;
    my $strType = shift;

    return docConfigOptionType($strOption) eq $strType;
}

push @EXPORT, qw(docConfigOptionTypeTest);

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
            {name => 'oDocRender', required => false}
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

    foreach my $strCommand (cfgDefineCommandList())
    {
        my $oCommandDoc = $oDoc->nodeGet('operation')->nodeGet('command-list')->nodeGetById('command', $strCommand);

        $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand} = {};
        my $oCommand = $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand};

        $$oCommand{&CONFIG_HELP_SUMMARY} = $oCommandDoc->nodeGet('summary')->textGet();
        $$oCommand{&CONFIG_HELP_DESCRIPTION} = $oCommandDoc->textGet();
    }

    # Iterate through all options
    my $oOptionDefine = cfgDefine();

    foreach my $strOption (sort(keys(%{$oOptionDefine})))
    {
        # Skip options that are internal only for all commands (test options)
        next if $oOptionDefine->{$strOption}{&CFGDEF_INTERNAL};

        # Iterate through all commands
        my @stryCommandList = sort(keys(%{defined($$oOptionDefine{$strOption}{&CFGDEF_COMMAND}) ?
                              $$oOptionDefine{$strOption}{&CFGDEF_COMMAND} : $$oConfigHash{&CONFIG_HELP_COMMAND}}));

        foreach my $strCommand (@stryCommandList)
        {
            if (!defined($$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}))
            {
                next;
            }

            if (ref(\$$oOptionDefine{$strOption}{&CFGDEF_COMMAND}{$strCommand}) eq 'SCALAR' &&
                $$oOptionDefine{$strOption}{&CFGDEF_COMMAND}{$strCommand} == false)
            {
                next;
            }

            # Skip options that are internal only for the current command
            next if $oOptionDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_INTERNAL};

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
                if (defined($$oOptionDefine{$strOption}{&CFGDEF_SECTION}))
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
                if (defined($$oOptionDefine{$strOption}{&CFGDEF_SECTION}))
                {
                    &log(ERROR,
                        "option ${strOption} defined in command ${strCommand} must not have " . CFGDEF_SECTION .
                        " defined");
                }
            }

            # Store the option in the command
            $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption}{&CONFIG_HELP_SOURCE} =
                $strOptionSource;

            my $oCommandOption = $$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption};

            $$oCommandOption{&CONFIG_HELP_SUMMARY} = $oOptionDoc->nodeGet('summary')->textGet();
            $$oCommandOption{&CONFIG_HELP_DESCRIPTION} = $oOptionDoc->textGet();
            $$oCommandOption{&CONFIG_HELP_EXAMPLE} = $oOptionDoc->fieldGet('example');

            $$oCommandOption{&CONFIG_HELP_NAME} = $oOptionDoc->paramGet('name');

            # Generate a list of alternate names
            if (defined($rhConfigDefine->{$strOption}{&CFGDEF_NAME_ALT}))
            {
                my $rhNameAlt = {};

                foreach my $strNameAlt (sort(keys(%{$rhConfigDefine->{$strOption}{&CFGDEF_NAME_ALT}})))
                {
                    $strNameAlt =~ s/\?//g;

                    if ($strNameAlt ne $strOption)
                    {
                        $rhNameAlt->{$strNameAlt} = true;
                    }
                }

                my @stryNameAlt = sort(keys(%{$rhNameAlt}));

                if (@stryNameAlt > 0)
                {
                    if (@stryNameAlt != 1)
                    {
                        confess &log(
                            ERROR, "multiple alt names are not supported for option '${strOption}':  " . join(', ', @stryNameAlt));
                    }

                    $oCommandOption->{&CONFIG_HELP_NAME_ALT} = \@stryNameAlt;
                }
            }

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
                $oOption->{&CONFIG_HELP_NAME_ALT} = $oCommandOption->{&CONFIG_HELP_NAME_ALT};
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
        '  ' . PROJECT_NAME . ' - ' . $oManifest->variableReplace($oIndexDoc->paramGet('subtitle')) . "\n\n" .
        "SYNOPSIS\n" .
        '  ' . PROJECT_EXE . ' [options] [command]';

    # Output the description (first two paragraphs of index.xml introduction)
    my $iParaTotal = 0;

    $strManPage .= "\n\n" .
        "DESCRIPTION";

    foreach my $oPara ($oIndexDoc->nodeGetById('section', 'introduction')->nodeList('p'))
    {
        $strManPage .= ($iParaTotal == 0 ? "\n" : "\n\n") . '  ' .
            manGetFormatText($oManifest->variableReplace($self->{oDocRender}->processText($oPara->textGet())), 80, 2);

        last;
    }

    # Build command and config hashes
    my $hConfigDefine = cfgDefine();
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
        my $strSection = defined($$hOption{&CONFIG_HELP_SECTION}) ? $$hOption{&CONFIG_HELP_SECTION} : CFGDEF_GENERAL;

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

            # Construct the default
            my $strCommand = grep(/$strSection/i, cfgDefineCommandList()) ? $strSection : undef;
            my $strDefault = docConfigOptionDefault($strOption, $strCommand);

            if (defined($strDefault))
            {
                if ($strOption eq CFGOPT_REPO_HOST_CMD || $strOption eq CFGOPT_PG_HOST_CMD)
                {
                    $strDefault = PROJECT_EXE;
                }
                elsif ($$hConfigDefine{$strOption}{&CFGDEF_TYPE} eq &CFGDEF_TYPE_BOOLEAN)
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
        '  ' . docConfigOptionDefault(CFGOPT_CONFIG) . "\n" .
        '  ' . docConfigOptionDefault(CFGOPT_REPO_PATH) . "\n" .
        '  ' . docConfigOptionDefault(CFGOPT_LOG_PATH) . "\n" .
        '  ' . docConfigOptionDefault(CFGOPT_SPOOL_PATH) . "\n" .
        '  ' . docConfigOptionDefault(CFGOPT_LOCK_PATH) . "\n" .
        "\n" .
        "EXAMPLES\n" .
        "\n" .
        "  * Create a backup of the PostgreSQL `main` cluster:\n" .
        "\n" .
        '    $ ' . PROJECT_EXE . ' --' . CFGOPT_STANZA . "=main backup\n" .
        "\n" .
        '    The `main` cluster should be configured in `' . docConfigOptionDefault(CFGOPT_CONFIG) . "`\n" .
        "\n" .
        "  * Show all available backups:\n" .
        "\n" .
        '    $ ' . PROJECT_EXE . ' ' . CFGCMD_INFO . "\n" .
        "\n" .
        "  * Show all available backups for a specific cluster:\n" .
        "\n" .
        '    $ ' . PROJECT_EXE . ' --' . CFGOPT_STANZA . '=main ' . CFGCMD_INFO . "\n" .
        "\n" .
        "  * Show backup specific options:\n" .
        "\n" .
        '    $ ' . PROJECT_EXE . ' ' . CFGCMD_HELP . ' ' . CFGCMD_BACKUP . "\n" .
        "\n" .
        "SEE ALSO\n" .
        "\n" .
        '  /usr/share/doc/' . PROJECT_EXE . "-doc/html/index.html\n" .
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

    my $oDoc = new pgBackRestDoc::Common::Doc();
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

        # Set the summary text for the section
        $oSectionElement->textSet($oSectionDoc->textGet());

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
    my $oOptionDefine = cfgDefine();

    my $oDoc = new pgBackRestDoc::Common::Doc();
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
                # Skip secure options that can't be defined on the command line
                next if ($rhConfigDefine->{$strOption}{&CFGDEF_SECURE});

                my ($oOption, $strCategory) = helpCommandDocGetOptionFind($oConfigHash, $oOptionDefine, $strCommand, $strOption);

                $$oCategory{$strCategory}{$strOption} = $oOption;
            }

            # Iterate sections
            foreach my $strCategory (sort(keys(%{$oCategory})))
            {
                my $oOptionListElement = $oSectionElement->nodeAdd(
                    'section', undef, {id => "category-${strCategory}", toc => 'n'});

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
    my $oOptionDefine = shift;
    my $strCommand = shift;
    my $strOption = shift;

    my $strSection = CONFIG_HELP_COMMAND;
    my $oOption = $$oConfigHelpData{&CONFIG_HELP_COMMAND}{$strCommand}{&CONFIG_HELP_OPTION}{$strOption};

    if ($$oOption{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_DEFAULT)
    {
        $strSection = CFGDEF_GENERAL;
    }
    elsif ($$oOption{&CONFIG_HELP_SOURCE} eq CONFIG_HELP_SOURCE_SECTION)
    {
        $oOption = $$oConfigHelpData{&CONFIG_HELP_OPTION}{$strOption};

        if (defined($$oOption{&CONFIG_HELP_SECTION}) && $strSection ne $strCommand)
        {
            $strSection = $$oOption{&CONFIG_HELP_SECTION};
        }

        if (($strSection ne CFGDEF_GENERAL && $strSection ne CFGDEF_LOG &&
             $strSection ne CFGDEF_REPOSITORY && $strSection ne CFGDEF_SECTION_STANZA) ||
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
    my $oOptionElement = $oParentElement->nodeAdd(
        'section', undef, {id => "option-${strOption}", toc => defined($strCommand) ? 'n' : 'y'});

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

    if (defined(docConfigOptionDefault($strOption, $strCommand)))
    {
        my $strDefault;

        if ($strOption eq CFGOPT_REPO_HOST_CMD || $strOption eq CFGOPT_PG_HOST_CMD)
        {
            $strDefault = '[INSTALL-PATH]/' . PROJECT_EXE;
        }
        else
        {
            if (docConfigOptionTypeTest($strOption, CFGDEF_TYPE_BOOLEAN))
            {
                $strDefault = docConfigOptionDefault($strOption, $strCommand) ? 'y' : 'n';
            }
            else
            {
                $strDefault = docConfigOptionDefault($strOption, $strCommand);
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
    my ($strRangeMin, $strRangeMax) = docConfigOptionRange($strOption, $strCommand);

    if (defined($strRangeMin))
    {
        $strCodeBlock .= (defined($strCodeBlock) ? "\n" : '') . "allowed: ${strRangeMin}-${strRangeMax}";
    }

    # Get the example
    my $strExample;

    my $strOptionPrefix = $rhConfigDefine->{$strOption}{&CFGDEF_PREFIX};
    my $strOptionIndex = defined($strOptionPrefix) ?
        "${strOptionPrefix}1-" . substr($strOption, length($strOptionPrefix) + 1) : $strOption;

    if (defined($strCommand))
    {
        if (docConfigOptionTypeTest($strOption, CFGDEF_TYPE_BOOLEAN))
        {
            if ($$oOptionHash{&CONFIG_HELP_EXAMPLE} ne 'n' && $$oOptionHash{&CONFIG_HELP_EXAMPLE} ne 'y')
            {
                confess &log(ERROR, "option ${strOption} example should be boolean but value is: " .
                             $$oOptionHash{&CONFIG_HELP_EXAMPLE});
            }

            $strExample = '--' . ($$oOptionHash{&CONFIG_HELP_EXAMPLE} eq 'n' ? 'no-' : '') . $strOptionIndex;
        }
        else
        {
            $strExample = "--${strOptionIndex}=" . $$oOptionHash{&CONFIG_HELP_EXAMPLE};
        }
    }
    else
    {
        $strExample = "${strOptionIndex}=" . $$oOptionHash{&CONFIG_HELP_EXAMPLE};
    }

    $strCodeBlock .= (defined($strCodeBlock) ? "\n" : '') . "example: ${strExample}";

    $oOptionElement->
        nodeAdd('code-block')->valueSet($strCodeBlock);

    # Output deprecated names
    if (defined($oOptionHash->{&CONFIG_HELP_NAME_ALT}))
    {
        my $strCaption = 'Deprecated Name' . (@{$oOptionHash->{&CONFIG_HELP_NAME_ALT}} > 1 ? 's' : '');

        $oOptionElement->
            nodeAdd('p')->textSet("${strCaption}: " . join(', ', @{$oOptionHash->{&CONFIG_HELP_NAME_ALT}}));
    }
}

1;
