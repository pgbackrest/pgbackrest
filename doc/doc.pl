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
use BackRestDoc::DocRender;

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Config;

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
        $oOptionFoundRef,
        $oOptionRuleRef
    ) =
        logDebugParam
        (
            OP_MAIN_OPTION_LIST_PROCESS, \@_,
            {name => 'oOptionListOut'},
            {name => 'strCommand', required => false},
            {name => 'oOptionFoundRef'},
            {name => 'oOptionRuleRef'}
        );

    foreach my $oOptionOut (@{$$oOptionListOut{children}})
    {
        my $strOption = $$oOptionOut{param}{id};

        if ($strOption eq 'help' || $strOption eq 'version')
        {
            next;
        }

        $$oOptionFoundRef{$strOption} = true;

        if (!defined($$oOptionRuleRef{$strOption}{&OPTION_RULE_TYPE}))
        {
            confess "unable to find option $strOption";
        }

        $$oOptionOut{field}{default} = optionDefault($strOption, $strCommand);

        if (defined($$oOptionOut{field}{default}))
        {
            $$oOptionOut{field}{required} = false;

            if ($$oOptionRuleRef{$strOption}{&OPTION_RULE_TYPE} eq &OPTION_TYPE_BOOLEAN)
            {
                $$oOptionOut{field}{default} = $$oOptionOut{field}{default} ? 'y' : 'n';
            }
        }
        else
        {
            $$oOptionOut{field}{required} = optionRequired($strOption, $strCommand);
        }

        if (defined($strCommand))
        {
            $$oOptionOut{field}{cmd} = true;
        }

        if ($strOption eq 'cmd-remote')
        {
            $$oOptionOut{field}{default} = 'same as local';
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
    my %oOptionFound;

    # Ouput general options
    my $oOperationGeneralOptionListOut =
        $oDoc->nodeGet(
            $oDoc->nodeGet(
                $oDoc->nodeGet(undef, 'operation'), 'operation-general'), 'option-list');

    optionListProcess($oOperationGeneralOptionListOut, undef, \%oOptionFound, $oOptionRule);

    # Ouput commands
    my $oCommandListOut = $oDoc->nodeGet($oDoc->nodeGet(undef, 'operation'), 'command-list');

    foreach my $oCommandOut (@{$$oCommandListOut{children}})
    {
        my $strCommand = $$oCommandOut{param}{id};

        my $oOptionListOut = $oDoc->nodeGet($oCommandOut, 'option-list', false);

        if (defined($oOptionListOut))
        {
            optionListProcess($oOptionListOut, $strCommand, \%oOptionFound, $oOptionRule);
        }

        my $oExampleListOut = $oDoc->nodeGet($oCommandOut, 'command-example-list');

        foreach my $oExampleOut (@{$$oExampleListOut{children}})
        {
            if (defined($$oExampleOut{param}{title}))
            {
                $$oExampleOut{param}{title} = 'Example: ' . $$oExampleOut{param}{title};
            }
            else
            {
                $$oExampleOut{param}{title} = 'Example';
            }
        }

        # $$oExampleListOut{param}{title} = 'Examples';
    }

    # Ouput config section
    my $oConfigSectionListOut = $oDoc->nodeGet($oDoc->nodeGet(undef, 'config'), 'config-section-list');

    foreach my $oConfigSectionOut (@{$$oConfigSectionListOut{children}})
    {
        my $oOptionListOut = $oDoc->nodeGet($oConfigSectionOut, 'config-key-list', false);

        if (defined($oOptionListOut))
        {
            optionListProcess($oOptionListOut, undef, \%oOptionFound, $oOptionRule);
        }
    }

    # Mark undocumented features as processed
    $oOptionFound{'no-fork'} = true;
    $oOptionFound{'test'} = true;
    $oOptionFound{'test-delay'} = true;

    # Make sure all options were processed
    foreach my $strOption (sort(keys($oOptionRule)))
    {
        if (!defined($oOptionFound{$strOption}))
        {
            confess "option ${strOption} was not found";
        }
    }
    }

    # Write markdown
    my $oRender = new BackRestDoc::DocRender($oDoc, 'markdown', $strProjectName, $strExeName);
    $oRender->save($strMdOut, $oRender->process());
}

docProcess("${strBasePath}/xml/readme.xml", "${strBasePath}/../README.md");
docProcess("${strBasePath}/xml/userguide.xml", "${strBasePath}/../USERGUIDE.md", true);
docProcess("${strBasePath}/xml/changelog.xml", "${strBasePath}/../CHANGELOG.md");
