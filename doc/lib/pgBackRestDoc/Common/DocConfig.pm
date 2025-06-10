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

use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::Custom::DocConfigData;
use pgBackRestDoc::ProjectInfo;

####################################################################################################################################
# Help types
####################################################################################################################################
use constant CONFIG_HELP_COMMAND                                    => 'command';
    push @EXPORT, qw(CONFIG_HELP_COMMAND);
use constant CONFIG_HELP_DESCRIPTION                                => 'description';
    push @EXPORT, qw(CONFIG_HELP_DESCRIPTION);
use constant CONFIG_HELP_INTERNAL                                   => 'internal';
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
        $oCommand->{&CONFIG_HELP_INTERNAL} = cfgDefineCommand()->{$strCommand}{&CFGDEF_INTERNAL};
    }

    # Iterate through all options
    my $oOptionDefine = cfgDefine();

    foreach my $strOption (sort(keys(%{$oOptionDefine})))
    {
        # Iterate through all commands
        my @stryCommandList = sort(keys(%{defined($$oOptionDefine{$strOption}{&CFGDEF_COMMAND}) ?
                              $$oOptionDefine{$strOption}{&CFGDEF_COMMAND} : $$oConfigHash{&CONFIG_HELP_COMMAND}}));

        foreach my $strCommand (@stryCommandList)
        {
            if (!defined($$oConfigHash{&CONFIG_HELP_COMMAND}{$strCommand}))
            {
                next;
            }

            # Skip the option if it is not valid for this command and the default role. Only options valid for the default role are
            # show in help because that is the only role available to a user.
            if (!defined($oOptionDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_COMMAND_ROLE}{&CFGCMD_ROLE_MAIN}))
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

                    # If a section is specified then use it, otherwise the option should be general since it is not for a specific
                    # command
                    if (defined($oOptionDoc))
                    {
                        $strSection = $oOptionDoc->paramGet('section', false);

                        if (!defined($strSection))
                        {
                            $strSection = "general";
                        }
                    }
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
            $oCommandOption->{&CONFIG_HELP_INTERNAL} =
                cfgDefineCommand()->{$strCommand}{&CFGDEF_INTERNAL} ? true : $oOptionDefine->{$strOption}{&CFGDEF_INTERNAL};

            # If internal is defined for the option/command it overrides everything else
            if (defined($oOptionDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_INTERNAL}))
            {
                $oCommandOption->{&CONFIG_HELP_INTERNAL} =
                    $oOptionDefine->{$strOption}{&CFGDEF_COMMAND}{$strCommand}{&CFGDEF_INTERNAL};
            }

            # If the option did not come from the command also store in global option list. This prevents duplication of commonly
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
                $oOption->{&CONFIG_HELP_INTERNAL} = $oOptionDefine->{$strOption}{&CFGDEF_INTERNAL};
            }
        }
    }

    # Store the config hash
    $self->{oConfigHash} = $oConfigHash;

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

1;
