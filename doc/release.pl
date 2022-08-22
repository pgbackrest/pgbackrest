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

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::Storage;
use pgBackRestTest::Common::StoragePosix;
use pgBackRestTest::Common::VmTest;

use pgBackRestDoc::Common::Doc;
use pgBackRestDoc::Common::DocConfig;
use pgBackRestDoc::Common::DocManifest;
use pgBackRestDoc::Common::DocRender;
use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::Custom::DocCustomRelease;
use pgBackRestDoc::Html::DocHtmlSite;
use pgBackRestDoc::Latex::DocLatex;
use pgBackRestDoc::Markdown::DocMarkdown;
use pgBackRestDoc::ProjectInfo;

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
   --no-gen         Don't auto-generate
   --vm             vm to build documentation for
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
my $bNoGen = false;
my $strVm = undef;

GetOptions ('help' => \$bHelp,
            'version' => \$bVersion,
            'quiet' => \$bQuiet,
            'log-level=s' => \$strLogLevel,
            'build' => \$bBuild,
            'deploy' => \$bDeploy,
            'no-gen' => \$bNoGen,
            'vm=s' => \$strVm)
    or pod2usage(2);

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
    # Display version and exit if requested
    if ($bHelp || $bVersion)
    {
        print PROJECT_NAME . ' ' . PROJECT_VERSION . " Release Manager\n";

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
    my $strTestExe = dirname($strDocPath) . "/test/test.pl";

    my $oStorageDoc = new pgBackRestTest::Common::Storage(
        $strDocPath, new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    # Determine if this is a dev release
    my $bDev = PROJECT_VERSION =~ /dev$/;
    my $strVersion = $bDev ? 'dev' : PROJECT_VERSION;

    # Make sure version number matches the latest release
    &log(INFO, "check version info");

    my $strReleaseFile = dirname(dirname(abs_path($0))) . '/doc/xml/release.xml';
    my $oRelease = (new pgBackRestDoc::Custom::DocCustomRelease(new pgBackRestDoc::Common::Doc($strReleaseFile)))->releaseLast();

    if ($oRelease->paramGet('version') ne PROJECT_VERSION)
    {
        confess 'unable to find version ' . PROJECT_VERSION . " as the most recent release in ${strReleaseFile}";
    }

    if ($bBuild)
    {
        if (!$bNoGen)
        {
            # Update git history
            my $strGitCommand =
                'git -C ' . $strDocPath .
                ' log --pretty=format:\'{^^^^commit^^^^:^^^^%H^^^^,^^^^date^^^^:^^^^%ci^^^^,^^^^subject^^^^:^^^^%s^^^^,^^^^body^^^^:^^^^%b^^^^},\'';
            my $strGitLog = qx($strGitCommand);
            $strGitLog =~ s/\^\^\^\^\}\,\n/\#\#\#\#/mg;
            $strGitLog =~ s/\\/\\\\/g;
            $strGitLog =~ s/\n/\\n/mg;
            $strGitLog =~ s/\r/\\r/mg;
            $strGitLog =~ s/\t/\\t/mg;
            $strGitLog =~ s/\"/\\\"/g;
            $strGitLog =~ s/\^\^\^\^/\"/g;
            $strGitLog =~ s/\#\#\#\#/\"\}\,\n/mg;
            $strGitLog = '[' . substr($strGitLog, 0, length($strGitLog) - 1) . ']';
            my @hyGitLog = @{(JSON::PP->new()->allow_nonref())->decode($strGitLog)};

            # Load prior history
            my @hyGitLogPrior = @{(JSON::PP->new()->allow_nonref())->decode(
                ${$oStorageDoc->get("${strDocPath}/resource/git-history.cache")})};

            # Add new commits
            for (my $iGitLogIdx = @hyGitLog - 1; $iGitLogIdx >= 0; $iGitLogIdx--)
            {
                my $rhGitLog = $hyGitLog[$iGitLogIdx];
                my $bFound = false;

                foreach my $rhGitLogPrior (@hyGitLogPrior)
                {
                    if ($rhGitLog->{commit} eq $rhGitLogPrior->{commit})
                    {
                        $bFound = true;
                    }
                }

                next if $bFound;

                $rhGitLog->{body} = trim($rhGitLog->{body});

                if ($rhGitLog->{body} eq '')
                {
                    delete($rhGitLog->{body});
                }

                unshift(@hyGitLogPrior, $rhGitLog);
            }

            # Write git log
            $strGitLog = undef;

            foreach my $rhGitLog (@hyGitLogPrior)
            {
                $strGitLog .=
                    (defined($strGitLog) ? ",\n" : '') .
                    "    {\n" .
                    '        "commit": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{commit})) . ",\n" .
                    '        "date": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{date})) . ",\n" .
                    '        "subject": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{subject}));

                # Skip the body if it is empty or a release (since we already have the release note content)
                if ($rhGitLog->{subject} !~ /^v[0-9]{1,2}\.[0-9]{1,2}\: /g && defined($rhGitLog->{body}))
                {
                    $strGitLog .=
                        ",\n" .
                        '        "body": ' . trim((JSON::PP->new()->allow_nonref()->pretty())->encode($rhGitLog->{body}));
                }

                $strGitLog .=
                    "\n" .
                    "    }";
            }

            $oStorageDoc->put("${strDocPath}/resource/git-history.cache", "[\n${strGitLog}\n]\n");

            # Generate coverage summary
            &log(INFO, "Generate Coverage Summary");
            executeTest("${strTestExe} --vm=u22 --no-valgrind --clean --coverage-summary", {bShowOutputAsync => true});
        }

        # Remove permanent cache file
        $oStorageDoc->remove("${strDocPath}/resource/exe.cache", {bIgnoreMissing => true});

        # Remove all docker containers to get consistent IP address assignments
        executeTest('docker rm -f $(docker ps -a -q)', {bSuppressError => true});

        # Generate deployment docs for RHEL
        if (!defined($strVm) || $strVm eq VM_RH8)
        {
            &log(INFO, "Generate RHEL documentation");

            executeTest("${strDocExe} --deploy --key-var=os-type=rhel --out=pdf", {bShowOutputAsync => true});

            if (!defined($strVm))
            {
                executeTest("${strDocExe} --deploy --cache-only --key-var=os-type=rhel --out=pdf");
            }
        }

        # Generate deployment docs for Debian
        if (!defined($strVm) || $strVm eq VM_U18)
        {
            &log(INFO, "Generate Debian/Ubuntu documentation");

            executeTest("${strDocExe} --deploy --out=man --out=html --out=markdown", {bShowOutputAsync => true});
        }

        # Generate a full copy of the docs for review
        if (!defined($strVm))
        {
            &log(INFO, "Generate full documentation for review");

            executeTest(
                "${strDocExe} --deploy --out-preserve --cache-only --key-var=os-type=rhel --out=html" .
                    " --var=project-url-root=index.html");
            $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-rhel.html");

            executeTest(
                "${strDocExe} --deploy --out-preserve --cache-only --out=man --out=html --var=project-url-root=index.html");
        }
    }

    if ($bDeploy)
    {
        my $strDeployPath = "${strDocPath}/site";

        # Generate docs for the website history
        &log(INFO, 'Generate website ' . ($bDev ? 'dev' : 'history') . ' documentation');

        my $strDocExeVersion =
            ${strDocExe} . ($bDev ? ' --dev' : ' --deploy --cache-only') . ' --var=project-url-root=index.html --out=html';

        executeTest("${strDocExeVersion} --out-preserve --key-var=os-type=rhel");
        $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-rhel.html");

        $oStorageDoc->remove("$strDocHtml/release.html");
        executeTest("${strDocExeVersion} --out-preserve --exclude=release");

        # Deploy to repository
        &log(INFO, '...Deploy to repository');
        executeTest("rm -rf ${strDeployPath}/prior/${strVersion}");
        executeTest("mkdir ${strDeployPath}/prior/${strVersion}");
        executeTest("cp ${strDocHtml}/* ${strDeployPath}/prior/${strVersion}");

        # Generate docs for the main website
        if (!$bDev)
        {
            &log(INFO, "Generate website documentation");

            executeTest("${strDocExe} --var=analytics=y --deploy --cache-only --key-var=os-type=rhel --out=html");
            $oStorageDoc->move("$strDocHtml/user-guide.html", "$strDocHtml/user-guide-rhel.html");
            executeTest("${strDocExe} --var=analytics=y --deploy --out-preserve --cache-only --out=html");

            # Deploy to repository
            &log(INFO, '...Deploy to repository');
            executeTest("rm -rf ${strDeployPath}/dev");
            executeTest("find ${strDeployPath} -maxdepth 1 -type f -exec rm {} +");
            executeTest("cp ${strDocHtml}/* ${strDeployPath}");
            executeTest("cp ${strDocPath}/../README.md ${strDeployPath}");
            executeTest("cp ${strDocPath}/../LICENSE ${strDeployPath}");
        }

        # Update permissions
        executeTest("find ${strDeployPath} -type d -exec chmod 750 {} +");
        executeTest("find ${strDeployPath} -type f -exec chmod 640 {} +");
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
