#!/usr/bin/perl
####################################################################################################################################
# pg_backrest.pl - Simple Postgres Backup and Restore
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

$SIG{__DIE__} = sub { Carp::confess @_ };

use Cwd qw(abs_path);
use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions);
use Pod::Usage qw(pod2usage);
use XML::Checker::Parser;

use lib dirname($0) . '/lib';
use BackRestDoc::Doc;
use BackRestDoc::DocConfig;
use BackRestDoc::DocRender;

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Config::Config;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

doc.pl - Generate pgBackRest documentation

=head1 SYNOPSIS

doc.pl [options] [operation]

 General Options:
   --help           display usage and exit

=cut

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_MAIN                                                => 'Main';

use constant OP_MAIN_DOC_PROCESS                                    => OP_MAIN . '::docProcess';
use constant OP_MAIN_OPTION_LIST_PROCESS                            => OP_MAIN . '::optionListProcess';

####################################################################################################################################
# optionListProcess
####################################################################################################################################
sub optionListProcess
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oOptionListOut,
        $strCommand,
        $oOptionRuleRef
    ) =
        logDebugParam
        (
            OP_MAIN_OPTION_LIST_PROCESS, \@_,
            {name => 'oOptionListOut'},
            {name => 'strCommand', required => false},
            {name => 'oOptionRuleRef'}
        );

    foreach my $oOptionOut ($oOptionListOut->nodeList())
    {
        my $strOption = $oOptionOut->paramGet('id');

        if ($strOption eq 'help' || $strOption eq 'version')
        {
            next;
        }

        if (!defined($$oOptionRuleRef{$strOption}{&OPTION_RULE_TYPE}))
        {
            confess "unable to find option $strOption";
        }

        if (defined(optionDefault($strOption, $strCommand)))
        {
            $oOptionOut->fieldSet('required', false);

            if ($$oOptionRuleRef{$strOption}{&OPTION_RULE_TYPE} eq &OPTION_TYPE_BOOLEAN)
            {
                $oOptionOut->fieldSet('default', optionDefault($strOption, $strCommand) ? 'y' : 'n');
            }
            else
            {
                $oOptionOut->fieldSet('default', optionDefault($strOption, $strCommand));
            }
        }
        else
        {
            $oOptionOut->fieldSet('required', optionRequired($strOption, $strCommand));
        }

        if (defined($strCommand))
        {
            $oOptionOut->fieldSet('cmd', true);
        }

        if ($strOption eq 'cmd-remote')
        {
            $oOptionOut->fieldSet('default', 'same as local');
        }
    }
}

####################################################################################################################################
# Load command line parameters and config
####################################################################################################################################
my $bHelp = false;                                  # Display usage
my $bVersion = false;                               # Display version
my $bQuiet = false;                                 # Sets log level to ERROR
my $strLogLevel = 'info';                           # Log level for tests
my $strProjectName = 'pgBackRest';                  # Project name to use in docs
my $strExeName = 'pg_backrest';                     # Exe name to use in docs

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel)
    or pod2usage(2);

# Display version and exit if requested
if ($bHelp || $bVersion)
{
    print 'pg_backrest ' . version_get() . " doc builder\n";

    if ($bHelp)
    {
        print "\n";
        pod2usage();
    }

    exit 0;
}

# Set console log level
if ($bQuiet)
{
    $strLogLevel = 'off';
}

logLevelSet(undef, uc($strLogLevel));

# Get the base path
my $strBasePath = abs_path(dirname($0));

sub docProcess
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strXmlIn,
        $strMdOut,
        $bManual
    ) =
        logDebugParam
        (
            OP_MAIN_DOC_PROCESS, \@_,
            {name => 'strXmlIn'},
            {name => 'strMdOut'},
            {name => 'bManual', default => false}
        );

    # Build the document from xml
    my $oDoc = new BackRestDoc::Doc($strXmlIn);

    # Build commands pulled from the code
    if ($bManual)
    {
        # Get the option rules
        my $oOptionRule = optionRuleGet();

        # Ouput general options
        my $oOperationGeneralOptionListOut =
            $oDoc->nodeGet('operation')->nodeGet('operation-general')->nodeGet('option-list');

        optionListProcess($oOperationGeneralOptionListOut, undef, $oOptionRule);

        # Ouput commands
        my $oCommandListOut = $oDoc->nodeGet('operation')->nodeGet('command-list');

        foreach my $oCommandOut ($oCommandListOut->nodeList())
        {
            my $strCommand = $oCommandOut->paramGet('id');

            my $oOptionListOut = $oCommandOut->nodeGet('option-list', false);

            if (defined($oOptionListOut))
            {
                optionListProcess($oOptionListOut, $strCommand, $oOptionRule);
            }

            my $oExampleListOut = $oCommandOut->nodeGet('command-example-list');

            foreach my $oExampleOut ($oExampleListOut->nodeList())
            {
                if (defined($oExampleOut->paramGet('title', false)))
                {
                    $oExampleOut->paramSet('title', 'Example: ' . $oExampleOut->paramGet('title'));
                }
                else
                {
                    $oExampleOut->paramSet('title', 'Example');
                }
            }
        }

        # Ouput config section
        my $oConfigSectionListOut = $oDoc->nodeGet('config')->nodeGet('config-section-list');

        foreach my $oConfigSectionOut ($oConfigSectionListOut->nodeList())
        {
            my $oOptionListOut = $oConfigSectionOut->nodeGet('config-key-list', false);

            if (defined($oOptionListOut))
            {
                optionListProcess($oOptionListOut, undef, $oOptionRule);
            }
        }
    }

    # Write markdown
    my $oRender = new BackRestDoc::DocRender('markdown', $strProjectName, $strExeName);
    $oRender->save($strMdOut, $oRender->process($oDoc));
}

my $oRender = new BackRestDoc::DocRender('text', $strProjectName, $strExeName);
my $oDocConfig = new BackRestDoc::DocConfig(new BackRestDoc::Doc("${strBasePath}/xml/reference.xml"), $oRender);
$oDocConfig->helpDataWrite();

docProcess("${strBasePath}/xml/readme.xml", "${strBasePath}/../README.md");
docProcess("${strBasePath}/xml/reference.xml", "${strBasePath}/../USERGUIDE.md", true);
docProcess("${strBasePath}/xml/changelog.xml", "${strBasePath}/../CHANGELOG.md");
