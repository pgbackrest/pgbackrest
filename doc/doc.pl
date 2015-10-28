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
use BackRestDoc::Common::Doc;
use BackRestDoc::Common::DocConfig;
use BackRestDoc::Html::DocHtmlSite;
use BackRestDoc::Common::DocRender;

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

####################################################################################################################################
# Load command line parameters and config
####################################################################################################################################
my $bHelp = false;                                  # Display usage
my $bVersion = false;                               # Display version
my $bQuiet = false;                                 # Sets log level to ERROR
my $strLogLevel = 'info';                           # Log level for tests
my $strProjectName = 'pgBackRest';                  # Project name to use in docs
my $strExeName = 'pg_backrest';                     # Exe name to use in docs
my $bHtml = false;                                  # Generate full html documentation
my $strHtmlRoot = '/';                              # Root html page
my $bNoExe = false;                                 # Should commands be executed when buildng help? (for testing only)

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel,
            'html' => \$bHtml,
            'html-root=s' => \$strHtmlRoot,
            'no-exe', \$bNoExe)
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
        $oHtmlSite
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
    my $oRender = new BackRestDoc::Common::DocRender('markdown', $strProjectName, $strExeName);
    $oRender->save($strMdOut, $oHtmlSite->variableReplace($oRender->process($oDoc)));
}

my $oRender = new BackRestDoc::Common::DocRender('text', $strProjectName, $strExeName);
my $oDocConfig = new BackRestDoc::Common::DocConfig(new BackRestDoc::Common::Doc("${strBasePath}/xml/reference.xml"), $oRender);
$oDocConfig->helpDataWrite();

# !!! Create Html Site Object to perform variable replacements on markdown and test
# !!! This should be replaced with a more generic site object in the future
my $oHtmlSite =
    new BackRestDoc::Html::DocHtmlSite
    (
        new BackRestDoc::Common::DocRender('html', $strProjectName, $strExeName),
        $oDocConfig,
        "${strBasePath}/xml",
        "${strBasePath}/html",
        "${strBasePath}/css/default.css",
        $strHtmlRoot,
        !$bNoExe
    );

docProcess("${strBasePath}/xml/index.xml", "${strBasePath}/../README.md", $oHtmlSite);
docProcess("${strBasePath}/xml/change-log.xml", "${strBasePath}/../CHANGELOG.md", $oHtmlSite);

# Only generate the HTML site when requested
if ($bHtml)
{
    $oHtmlSite->process();
}
