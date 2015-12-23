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
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Config::Config;
use BackRest::FileCommon;

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

####################################################################################################################################
# Load command line parameters and config
####################################################################################################################################
my $bHelp = false;                                  # Display usage
my $bVersion = false;                               # Display version
my $bQuiet = false;                                 # Sets log level to ERROR
my $strLogLevel = 'info';                           # Log level for tests
my $bNoExe = false;                                 # Should commands be executed when buildng help? (for testing only)
my $bUseCache = false;                              # Use cached data to generate the docs (for testing code changes only)
my $oVariableOverride = {};                         # Override variables
my $strDocPath;                                     # Document path to render
my @stryOutput;                                     # Output types
my @stryKeyword;                                    # Keyword used to filter output
my @stryRequire;                                    # Required sections of the document (to speed testing)

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
    print 'pg_backrest ' . version_get() . " doc builder\n";

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

# If no keyword we passed then use default
if (@stryKeyword == 0)
{
    @stryKeyword = ('default')
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
        push(@stryOutput, 'markdown');
    }
}

for my $strOutput (@stryOutput)
{
    if (!(($strOutput eq 'help' || $strOutput eq 'markdown') && $oManifest->isBackRest()))
    {
        $oManifest->renderGet($strOutput);
    }

    &log(INFO, "render ${strOutput} output");

    if ($strOutput eq 'markdown' && $oManifest->isBackRest())
    {
        # Generate the markdown
        docProcess("${strBasePath}/xml/index.xml", "${strBasePath}/../README.md", $oManifest);
        docProcess("${strBasePath}/xml/change-log.xml", "${strBasePath}/../CHANGELOG.md", $oManifest);
    }
    elsif ($strOutput eq 'markdown')
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
