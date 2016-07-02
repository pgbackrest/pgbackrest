#!/usr/bin/perl
####################################################################################################################################
# doc.pl - PgBackRest Doc Builder
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
use Scalar::Util qw(blessed);
use Storable;

use lib dirname($0) . '/lib';
use BackRestDoc::Common::Doc;
use BackRestDoc::Common::DocConfig;
use BackRestDoc::Common::DocManifest;
use BackRestDoc::Common::DocRender;
use BackRestDoc::Html::DocHtmlSite;
use BackRestDoc::Latex::DocLatex;
use BackRestDoc::Markdown::DocMarkdown;

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;
use pgBackRest::FileCommon;
use pgBackRest::Version;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

doc.pl - Generate pgBackRest documentation

=head1 SYNOPSIS

doc.pl [options]

 General Options:
   --help           Display usage and exit
   --version        Display pgBackRest version
   --quiet          Sets log level to ERROR
   --log-level      Log level for execution (e.g. ERROR, WARN, INFO, DEBUG)
   --deploy         Write exe.cache into resource for persistence
   --no-exe         Should commands be executed when building help? (for testing only)
   --no-cache       Don't use execution cache
   --cache-only     Only use the execution cache - don't attempt to generate it
   --var            Override variables defined in the XML
   --doc-path       Document path to render (manifest.xml should be located here)
   --out            Output types (html, pdf, markdown)
   --keyword        Keyword used to filter output
   --require        Require only certain sections of the document (to speed testing)
   --exclude        Exclude source from generation (links will reference website)
=cut

####################################################################################################################################
# Load command line parameters and config (see usage above for details)
####################################################################################################################################
my $bHelp = false;
my $bVersion = false;
my $bQuiet = false;
my $strLogLevel = 'info';
my $bNoExe = false;
my $bNoCache = false;
my $bCacheOnly = false;
my $oVariableOverride = {};
my $strDocPath;
my @stryOutput;
my @stryKeyword;
my @stryRequire;
my @stryExclude;
my $bDeploy = false;

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel,
            'out=s@' => \@stryOutput,
            'keyword=s@' => \@stryKeyword,
            'require=s@' => \@stryRequire,
            'exclude=s@' => \@stryExclude,
            'no-exe', \$bNoExe,
            'deploy', \$bDeploy,
            'no-cache', \$bNoCache,
            'cache-only', \$bCacheOnly,
            'var=s%', $oVariableOverride,
            'doc-path=s', \$strDocPath)
    or pod2usage(2);

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
    # Display version and exit if requested
    if ($bHelp || $bVersion)
    {
        print BACKREST_NAME . ' ' . BACKREST_VERSION . " Documentation Builder\n";

        if ($bHelp)
        {
            print "\n";
            pod2usage();
        }

        exit 0;
    }

    # Disable cache when no exe
    if ($bNoExe)
    {
        $bNoCache = true;
    }

    # Make sure options are set correctly for deploy
    if ($bDeploy)
    {
        my $strError = 'cannot be specified for deploy';

        !$bNoExe
            or confess "--no-exe ${strError}";

        !@stryRequire
            or confess "--require ${strError}";
    }

    # Set console log level
    if ($bQuiet)
    {
        $strLogLevel = 'error';
    }

    # If no keyword was passed then use default
    if (@stryKeyword == 0)
    {
        @stryKeyword = ('default');
    }

    logLevelSet(undef, uc($strLogLevel));

    # Get the base path
    my $strBasePath = abs_path(dirname($0));

    if (!defined($strDocPath))
    {
        $strDocPath = $strBasePath;
    }

    my $strOutputPath = "${strDocPath}/output";

    # Create the out path if it does not exist
    if (!-e $strOutputPath)
    {
        mkdir($strOutputPath)
            or confess &log(ERROR, "unable to create path ${strOutputPath}");
    }

    # Load the manifest
    my $oManifest = new BackRestDoc::Common::DocManifest(
        \@stryKeyword, \@stryRequire, \@stryExclude, $oVariableOverride, $strDocPath, $bDeploy, $bCacheOnly);

    if (!$bNoCache)
    {
        $oManifest->cacheRead();
    }

    # If no outputs were given
    if (@stryOutput == 0)
    {
        @stryOutput = $oManifest->renderList();

        if ($oManifest->isBackRest())
        {
            push(@stryOutput, 'help');
            push(@stryOutput, 'man');
        }
    }

    for my $strOutput (@stryOutput)
    {
        if (!(($strOutput eq 'help' || $strOutput eq 'man') && $oManifest->isBackRest()))
        {
            $oManifest->renderGet($strOutput);
        }

        &log(INFO, "render ${strOutput} output");

        if ($strOutput eq 'markdown')
        {
            my $oMarkdown =
                new BackRestDoc::Markdown::DocMarkdown
                (
                    $oManifest,
                    "${strBasePath}/xml",
                    "${strOutputPath}/markdown",
                    !$bNoExe
                );

            $oMarkdown->process();
        }
        elsif (($strOutput eq 'help' || $strOutput eq 'man') && $oManifest->isBackRest())
        {
            # Generate the command-line help
            my $oRender = new BackRestDoc::Common::DocRender('text', $oManifest);
            my $oDocConfig =
                new BackRestDoc::Common::DocConfig(
                    new BackRestDoc::Common::Doc("${strBasePath}/xml/reference.xml"), $oRender);

            if ($strOutput eq 'help')
            {
                $oDocConfig->helpDataWrite($oManifest);
            }
            else
            {
                filePathCreate("${strBasePath}/output/man", '0770', true, true);
                fileStringWrite("${strBasePath}/output/man/" . lc(BACKREST_NAME) . '.1.txt', $oDocConfig->manGet($oManifest), false);
            }
        }
        elsif ($strOutput eq 'html')
        {
            my $oHtmlSite =
                new BackRestDoc::Html::DocHtmlSite
                (
                    $oManifest,
                    "${strBasePath}/xml",
                    "${strOutputPath}/html",
                    "${strBasePath}/resource/html/default.css",
                    defined($oManifest->variableGet('project-favicon')) ?
                        "${strBasePath}/resource/html/" . $oManifest->variableGet('project-favicon') : undef,
                    defined($oManifest->variableGet('project-logo')) ?
                        "${strBasePath}/resource/" . $oManifest->variableGet('project-logo') : undef,
                    !$bNoExe
                );

            $oHtmlSite->process();
        }
        elsif ($strOutput eq 'pdf')
        {
            my $oLatex =
                new BackRestDoc::Latex::DocLatex
                (
                    $oManifest,
                    "${strBasePath}/xml",
                    "${strOutputPath}/latex",
                    "${strBasePath}/resource/latex/preamble.tex",
                    !$bNoExe
                );

            $oLatex->process();
        }
    }

    # Cache the manifest (mostly useful for testing rendering changes in the code)
    if (!$bNoCache && !$bCacheOnly)
    {
        $oManifest->cacheWrite();
    }
};

####################################################################################################################################
# Check for errors
####################################################################################################################################
if ($@)
{
    my $oMessage = $@;

    # If a backrest exception then return the code - don't confess
    if (blessed($oMessage) && $oMessage->isa('pgBackRest::Common::Exception'))
    {
        exit $oMessage->code();
    }

    confess $oMessage;
}
