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
use English '-no_match_vars';

$SIG{__DIE__} = sub { Carp::confess @_ };

use Cwd qw(abs_path);
use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions);
use Pod::Usage qw(pod2usage);
use Storable;

use lib dirname($0) . '/lib';
use lib dirname(dirname($0)) . '/lib';
use lib dirname(dirname($0)) . '/build/lib';
use lib dirname(dirname($0)) . '/test/lib';

use BackRestDoc::Common::Doc;
use BackRestDoc::Common::DocConfig;
use BackRestDoc::Common::DocManifest;
use BackRestDoc::Common::DocRender;
use BackRestDoc::Html::DocHtmlSite;
use BackRestDoc::Latex::DocLatex;
use BackRestDoc::Markdown::DocMarkdown;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Storage::Local;
use pgBackRest::Storage::Posix::Driver;
use pgBackRest::Version;

use pgBackRestTest::Common::ExecuteTest;

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
   --pre            Pre-build containers for execute elements marked pre
   --var            Override variables defined in the XML
   --doc-path       Document path to render (manifest.xml should be located here)
   --out            Output types (html, pdf, markdown)
   --require        Require only certain sections of the document (to speed testing)
   --include        Include source in generation (links will reference website)
   --exclude        Exclude source from generation (links will reference website)

 Keyword Options:
   --keyword        Keyword used to filter output
   --keyword-add    Add keyword without overriding 'default' keyword
   --dev            Add 'dev' keyword
   --debug          Add 'debug' keyword
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
my @stryKeywordAdd;
my @stryRequire;
my @stryInclude;
my @stryExclude;
my $bDeploy = false;
my $bDev = false;
my $bDebug = false;
my $bPre = false;

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel,
            'out=s@' => \@stryOutput,
            'keyword=s@' => \@stryKeyword,
            'keyword-add=s@' => \@stryKeywordAdd,
            'require=s@' => \@stryRequire,
            'include=s@' => \@stryInclude,
            'exclude=s@' => \@stryExclude,
            'no-exe', \$bNoExe,
            'deploy', \$bDeploy,
            'no-cache', \$bNoCache,
            'dev', \$bDev,
            'debug', \$bDebug,
            'pre', \$bPre,
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
        print PROJECT_NAME . ' ' . PROJECT_VERSION . " Documentation Builder\n";

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

    # one --include must be specified when --required is
    if (@stryRequire && @stryInclude != 1)
    {
        confess "one --include is required when --require is specified";
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

    # Push added keywords
    push(@stryKeyword, @stryKeywordAdd);

    # If --dev passed then add the dev keyword
    if ($bDev)
    {
        push(@stryKeyword, 'dev');
    }

    # If --debug passed then add the debug keyword
    if ($bDebug)
    {
        push(@stryKeyword, 'debug');
    }

    # Doesn't make sense to pass include and exclude
    if (@stryInclude > 0 && @stryExclude > 0)
    {
        confess "cannot specify both --include and --exclude";
    }

    logLevelSet(undef, uc($strLogLevel), OFF);

    # Get the base path
    my $strBasePath = abs_path(dirname($0));

    my $oStorageDoc = new pgBackRest::Storage::Local(
        $strBasePath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

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
        $oStorageDoc, \@stryKeyword, \@stryRequire, \@stryInclude, \@stryExclude, $oVariableOverride, $strDocPath, $bDeploy,
        $bCacheOnly, $bPre);

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
            push(@stryOutput, 'man');
        }
    }

    # Build host containers
    if (!$bCacheOnly && !$bNoExe)
    {
        foreach my $strSource ($oManifest->sourceList())
        {
            if ((@stryInclude == 0 || grep(/$strSource/, @stryInclude)) && !grep(/$strSource/, @stryExclude))
            {
                &log(INFO, "source $strSource");

                foreach my $oHostDefine ($oManifest->sourceGet($strSource)->{doc}->nodeList('host-define', false))
                {
                    if ($oManifest->keywordMatch($oHostDefine->paramGet('keyword', false)))
                    {
                        my $strImage = $oManifest->variableReplace($oHostDefine->paramGet('image'));
                        my $strFrom = $oManifest->variableReplace($oHostDefine->paramGet('from'));
                        my $strDockerfile = "${strOutputPath}/doc-host.dockerfile";

                        &log(INFO, "Build vm '${strImage}' from '${strFrom}'");

                        $oStorageDoc->put(
                            $strDockerfile,
                            "FROM ${strFrom}\n\n" . trim($oManifest->variableReplace($oHostDefine->valueGet())) . "\n");
                        executeTest("docker build -f ${strDockerfile} -t ${strImage} ${strBasePath}");
                    }
                }
            }
        }
    }

    # Render output
    for my $strOutput (@stryOutput)
    {
        if (!($strOutput eq 'man' && $oManifest->isBackRest()))
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
        elsif ($strOutput eq 'man' && $oManifest->isBackRest())
        {
            # Generate the command-line help
            my $oRender = new BackRestDoc::Common::DocRender('text', $oManifest, !$bNoExe);
            my $oDocConfig =
                new BackRestDoc::Common::DocConfig(
                    new BackRestDoc::Common::Doc("${strBasePath}/xml/reference.xml"), $oRender);

            $oStorageDoc->pathCreate(
                "${strBasePath}/output/man", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});
            $oStorageDoc->put("${strBasePath}/output/man/" . lc(PROJECT_NAME) . '.1.txt', $oDocConfig->manGet($oManifest));
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

    # Exit with success
    exit 0;
}

####################################################################################################################################
# Check for errors
####################################################################################################################################
or do
{
    # If a backrest exception then return the code
    exit $EVAL_ERROR->code() if (isException(\$EVAL_ERROR));

    # Else output the unhandled error
    print $EVAL_ERROR;
    exit ERROR_UNHANDLED;
};

# It shouldn't be possible to get here
&log(ASSERT, 'execution reached invalid location in ' . __FILE__ . ', line ' . __LINE__);
exit ERROR_ASSERT;
