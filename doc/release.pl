#!/usr/bin/perl
####################################################################################################################################
# release.pl - PgBackRest Release Manager
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
use lib dirname(dirname($0)) . '/build/lib';
use lib dirname(dirname($0)) . '/lib';
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

release.pl - pgBackRest Release Manager

=head1 SYNOPSIS

release.pl [options]

 General Options:
   --help           Display usage and exit
   --version        Display pgBackRest version
   --quiet          Sets log level to ERROR
   --log-level      Log level for execution (e.g. ERROR, WARN, INFO, DEBUG)

 Release Options:
   --build          Build the cache before release (should be included in the release commit)
   --deploy         Deploy documentation to website (can be done as docs are updated)
=cut

####################################################################################################################################
# Load command line parameters and config (see usage above for details)
####################################################################################################################################
my $bHelp = false;
my $bVersion = false;
my $bQuiet = false;
my $strLogLevel = 'info';
my $bBuild = false;
my $bDeploy = false;

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel,
            'build' => \$bBuild,
            'deploy' => \$bDeploy)
    or pod2usage(2);

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
    # Display version and exit if requested
    if ($bHelp || $bVersion)
    {
        print BACKREST_NAME . ' ' . BACKREST_VERSION . " Release Manager\n";

        if ($bHelp)
        {
            print "\n";
            pod2usage();
        }

        exit 0;
    }

    # If neither build nor deploy is requested then error
    if (!$bBuild && !$bDeploy)
    {
        confess &log(ERROR, 'neither --build nor --deploy requested, nothing to do');
    }

    # Set console log level
    if ($bQuiet)
    {
        $strLogLevel = 'error';
    }

    logLevelSet(undef, uc($strLogLevel), OFF);

    # Set the paths
    my $strDocPath = dirname(abs_path($0));
    my $strDocHtml = "${strDocPath}/output/html";
    my $strDocExe = "${strDocPath}/doc.pl";

    my $oStorageDoc = new pgBackRest::Storage::Local(
        $strDocPath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

    # Determine if this is a dev release
    my $bDev = BACKREST_VERSION =~ /dev$/;
    my $strVersion = $bDev ? 'dev' : BACKREST_VERSION;

    if ($bBuild)
    {
        # Remove permanent cache file
        $oStorageDoc->remove("${strDocPath}/resource/exe.cache", {bIgnoreMissing => true});

        # Remove all docker containers to get consistent IP address assignments
        executeTest('docker rm -f $(docker ps -a -q)', {bSuppressError => true});

        # Generate deployment docs for RHEL/Centos 6
        &log(INFO, "Generate RHEL/CentOS 6 documentation");

        executeTest("${strDocExe} --deploy --keyword=co6 --out=pdf", {bShowOutputAsync => true});
        executeTest("${strDocExe} --deploy --cache-only --keyword=co6 --out=pdf --var=\"project-name=Crunchy BackRest\"");

        # Generate deployment docs for Debian
        &log(INFO, "Generate Debian/Ubuntu documentation");

        executeTest("${strDocExe} --deploy", {bShowOutputAsync => true});
        executeTest("${strDocExe} --deploy --cache-only --out=man --out=html --var=project-url-root=index.html");
    }

    if ($bDeploy)
    {
        my $strDeployPath = "${strDocPath}/site";

        # Generate deployment docs for the website history
        &log(INFO, 'Generate website ' . ($bDev ? 'dev' : 'history') . ' documentation');

        executeTest(
            $strDocExe . ($bDev ? '' : ' --deploy --cache-only') . ' --out=html --var=project-url-root=index.html' .
            ($bDev ? ' --keyword=default --keyword=dev --no-exe' :  ' --exclude=release'));

        # Deploy to server
        &log(INFO, '...Deploy to repository');
        executeTest("rm -rf ${strDeployPath}/${strVersion}");
        executeTest("mkdir ${strDeployPath}/${strVersion}");
        executeTest("cp ${strDocHtml}/* ${strDeployPath}/${strVersion}");

        # Generate deployment docs for the main website
        if (!$bDev)
        {
            &log(INFO, "Generate website documentation");

            executeTest("${strDocExe} --deploy --cache-only --out=html");

            &log(INFO, '...Deploy to repository');
            executeTest("rm -rf ${strDeployPath}/dev");
            executeTest("find ${strDeployPath} -maxdepth 1 -type f -exec rm {} +");
            executeTest("cp ${strDocHtml}/* ${strDeployPath}");
            executeTest("cp ${strDocPath}/../README.md ${strDeployPath}");
            executeTest("cp ${strDocPath}/../LICENSE ${strDeployPath}");
            executeTest("echo 'pgbackrest.org' > ${strDeployPath}/CNAME");
        }

        # Update permissions
        executeTest("find ${strDeployPath} -type d -exec chmod 550 {} +");
        executeTest("find ${strDeployPath} -type f -exec chmod 440 {} +");
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
