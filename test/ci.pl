#!/usr/bin/perl
####################################################################################################################################
# CI Test Wrapper
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);
use English '-no_match_vars';

# Convert die to confess to capture the stack trace
$SIG{__DIE__} = sub { Carp::confess @_ };

use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions);
use Cwd qw(abs_path);
use Pod::Usage qw(pod2usage);

use lib dirname($0) . '/lib';
use lib dirname(dirname($0)) . '/lib';
use lib dirname(dirname($0)) . '/doc/lib';

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

ci.pl - CI Test Wrapper

=head1 SYNOPSIS

test.pl [options] doc|test

 VM Options:
   --vm                 docker container to build/test
   --param              parameters to pass to test.pl
   --sudo               test requires sudo

 General Options:
   --help               display usage and exit
=cut

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $strVm;
my @stryParam;
my $bSudo;
my $bHelp;

GetOptions ('help' => \$bHelp,
            'param=s@' => \@stryParam,
            'sudo' => \$bSudo,
            'vm=s' => \$strVm)
    or pod2usage(2);

####################################################################################################################################
# Begin/end functions to track timing
####################################################################################################################################
my $lProcessBegin;
my $strProcessTitle;

sub processBegin
{
    $strProcessTitle = shift;

    &log(INFO, "Begin ${strProcessTitle}");
    $lProcessBegin = time();
}

sub processExec
{
    my $strCommand = shift;
    my $rhParam = shift;

    &log(INFO, "    Exec ${strCommand}");
    executeTest($strCommand, $rhParam);
}

sub processEnd
{
    &log(INFO, "    End ${strProcessTitle} (" . (time() - $lProcessBegin) . 's)');
}

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
    # Display version and exit if requested
    if ($bHelp)
    {
        syswrite(*STDOUT, "CI Test Wrapper\n");

        syswrite(*STDOUT, "\n");
        pod2usage();

        exit 0;
    }

    if (@ARGV != 1)
    {
        syswrite(*STDOUT, "test|doc required\n\n");
        pod2usage();
    }

    # VM must be defined
    if (!defined($strVm))
    {
        confess &log(ERROR, '--vm is required');
    }

    ################################################################################################################################
    # Paths
    ################################################################################################################################
    my $strBackRestBase = dirname(dirname(abs_path($0)));
    my $strReleaseExe = "${strBackRestBase}/doc/release.pl";
    my $strTestExe = "${strBackRestBase}/test/test.pl";

    logLevelSet(INFO, INFO, OFF);

    processBegin('install common packages');
    processExec('sudo apt-get -qq update', {bSuppressStdErr => true, bSuppressError => true});
    processExec('sudo apt-get install -y libxml-checker-perl libyaml-perl', {bSuppressStdErr => true});
    processEnd();

    processBegin('mount tmpfs');
    processExec('mkdir -p -m 770 test');
    processExec('sudo mount -t tmpfs -o size=2048m tmpfs test');
    processExec('df -h test', {bShowOutputAsync => true});
    processEnd();

    ################################################################################################################################
    # Build documentation
    ################################################################################################################################
    my $strUser = getpwuid($UID);

    if ($ARGV[0] eq 'doc')
    {
        if ($strVm eq VM_CO7)
        {
            processBegin('LaTeX install');
            processExec(
                'sudo apt-get install -y --no-install-recommends texlive-latex-base texlive-latex-extra texlive-fonts-recommended',
                {bSuppressStdErr => true});
            processExec('sudo apt-get install -y texlive-font-utils texlive-latex-recommended', {bSuppressStdErr => true});
        }

        processBegin('remove sudo');
        processExec("sudo rm /etc/sudoers.d/${strUser}");
        processEnd();

        processBegin('create link from home to repo for contributing doc');
        processExec("ln -s ${strBackRestBase} \${HOME?}/" . PROJECT_EXE);
        processEnd();

        processBegin('release documentation');
        processExec("${strReleaseExe} --build --no-gen --vm=${strVm}", {bShowOutputAsync => true, bOutLogOnError => false});
        processEnd();
    }

    ################################################################################################################################
    # Run test
    ################################################################################################################################
    elsif ($ARGV[0] eq 'test')
    {
        # Build list of packages that need to be installed
        my $strPackage = "rsync zlib1g-dev libssl-dev libxml2-dev libpq-dev pkg-config";

        # Add lcov when testing coverage
        if (vmCoverageC($strVm))
        {
            $strPackage .= " lcov";
        }

        # Extra packages required when testing without containers
        if ($strVm eq VM_NONE)
        {
            $strPackage .= " valgrind liblz4-dev liblz4-tool zstd libzstd-dev bzip2 libbz2-dev";
        }
        # Else packages needed for integration tests on containers
        else
        {
            $strPackage .= " libdbd-pg-perl";
        }

        processBegin('/tmp/pgbackrest owned by root so tests cannot use it');
        processExec('sudo mkdir -p /tmp/pgbackrest && sudo chown root:root /tmp/pgbackrest && sudo chmod 700 /tmp/pgbackrest');
        processEnd();

        processBegin('install test packages');
        processExec("sudo apt-get install -y ${strPackage}", {bSuppressStdErr => true});
        processEnd();

        if (!$bSudo)
        {
            processBegin('remove sudo');
            processExec("sudo rm /etc/sudoers.d/${strUser}");
            processEnd();
        }

        # Build the container
        if ($strVm ne VM_NONE)
        {
            processBegin("${strVm} build");
            processExec("${strTestExe} --vm-build --vm=${strVm}", {bShowOutputAsync => true, bOutLogOnError => false});
            processEnd();
        }

        processBegin(($strVm eq VM_NONE ? "no container" : $strVm) . ' test');
        processExec(
            "${strTestExe} --gen-check --log-level-test-file=off --no-coverage-report --vm-host=none --vm-max=2 --vm=${strVm}" .
            (@stryParam != 0 ? " --" . join(" --", @stryParam) : ''),
            {bShowOutputAsync => true, bOutLogOnError => false});
        processEnd();
    }

    ################################################################################################################################
    # Catch error
    ################################################################################################################################
    else
    {
        confess &log(ERROR, 'invalid command ' . $ARGV[0]);
    }

    &log(INFO, "CI Complete");

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
