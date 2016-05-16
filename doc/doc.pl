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
use Storable;
use XML::Checker::Parser;

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
   --no-exe         Should commands be executed when building help? (for testing only)
   --use-cache      Use cached data to generate the docs (for testing textual changes only)
   --var            Override variables defined in the XML
   --doc-path       Document path to render (manifest.xml should be located here)
   --out            Output types (html, pdf, markdown)
   --keyword        Keyword used to filter output
   --require        Require only certain sections of the document (to speed testing)
=cut

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_MAIN                                                => 'Main';

use constant OP_MAIN_DOC_PROCESS                                    => OP_MAIN . '::docProcess';

####################################################################################################################################
# Load command line parameters and config (see usage above for details)
####################################################################################################################################
my $bHelp = false;
my $bVersion = false;
my $bQuiet = false;
my $strLogLevel = 'info';
my $bNoExe = false;
my $bUseCache = false;
my $oVariableOverride = {};
my $strDocPath;
my @stryOutput;
my @stryKeyword;
my @stryRequire;

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel,
            'out=s@' => \@stryOutput,
            'keyword=s@' => \@stryKeyword,
            'require=s@' => \@stryRequire,
            'no-exe', \$bNoExe,
            'use-cache', \$bUseCache,
            'var=s%', $oVariableOverride,
            'doc-path=s', \$strDocPath)
    or pod2usage(2);

# Display version and exit if requested
if ($bHelp || $bVersion)
{
    print BACKREST_NAME . ' ' . $VERSION . " Documentation Builder\n";

    if ($bHelp)
    {
        print "\n";
        pod2usage();
    }

    exit 0;
}

# Set no-exe if use-cache is set
if ($bUseCache)
{
    $bNoExe = true;
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

sub docProcess
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strXmlIn,
        $strMdOut,
        $oManifest
    ) =
        logDebugParam
        (
            OP_MAIN_DOC_PROCESS, \@_,
            {name => 'strXmlIn'},
            {name => 'strMdOut'},
            {name => 'oHtmlSite'}
        );

    # Build the document from xml
    my $oDoc = new BackRestDoc::Common::Doc($strXmlIn);

    # Write markdown
    my $oRender = new BackRestDoc::Common::DocRender('markdown', $oManifest);
    $oRender->save($strMdOut, $oManifest->variableReplace($oRender->process($oDoc)));
}

# Create the out path if it does not exist
if (!-e $strOutputPath)
{
    mkdir($strOutputPath)
        or confess &log(ERROR, "unable to create path ${strOutputPath}");
}

# Load the manifest
my $oManifest;
my $strManifestCache = "${strOutputPath}/manifest.cache";

if ($bUseCache && -e $strManifestCache)
{
    $oManifest = retrieve($strManifestCache);
}
else
{
    $oManifest = new BackRestDoc::Common::DocManifest(\@stryKeyword, \@stryRequire, $oVariableOverride, $strDocPath);
}

# If no outputs were given
if (@stryOutput == 0)
{
    @stryOutput = $oManifest->renderList();

    if ($oManifest->isBackRest())
    {
        push(@stryOutput, 'help');
    }
}

for my $strOutput (@stryOutput)
{
    if (!(($strOutput eq 'help' || $strOutput eq 'markdown') && $oManifest->isBackRest()))
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

        # Generate the change log using the old markdown code.
        if ($oManifest->isBackRest())
        {
            docProcess("${strBasePath}/xml/change-log.xml", "${strBasePath}/../CHANGELOG.md", $oManifest);
        }
    }
    elsif ($strOutput eq 'help' && $oManifest->isBackRest())
    {
        # Generate the command-line help
        my $oRender = new BackRestDoc::Common::DocRender('text', $oManifest);
        my $oDocConfig =
            new BackRestDoc::Common::DocConfig(
                new BackRestDoc::Common::Doc("${strBasePath}/xml/reference.xml"), $oRender);
        $oDocConfig->helpDataWrite($oManifest);
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
if (!$bUseCache)
{
    store($oManifest, $strManifestCache);
}
