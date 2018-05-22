#!/usr/bin/perl
####################################################################################################################################
# Travis CI Test Wrapper
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

use lib dirname($0) . '/lib';
use lib dirname(dirname($0)) . '/lib';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

travis.pl - Travis CI Test Wrapper

=head1 SYNOPSIS

test.pl [options] doc|test

 VM Options:
   --vm                 docker container to build/test

 General Options:
   --help               display usage and exit
=cut

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $strVm;
my $bHelp;

GetOptions ('help' => \$bHelp,
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

sub processEnd
{
    &log(INFO, "End ${strProcessTitle} (" . (time() - $lProcessBegin) . 's)');
}

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
    # Display version and exit if requested
    if ($bHelp)
    {
        syswrite(*STDOUT, "Travis CI Test Wrapper\n");

        syswrite(*STDOUT, "\n");
        pod2usage();

        exit 0;
    }

    if (@ARGV != 1)
    {
        syswrite(*STDOUT, "test|doc required\n\n");
        pod2usage();
    }

    ################################################################################################################################
    # Paths
    ################################################################################################################################
    my $strBackRestBase = dirname(dirname(abs_path($0)));
    my $strReleaseExe = "${strBackRestBase}/doc/release.pl";
    my $strTestExe = "${strBackRestBase}/test/test.pl";

    logLevelSet(INFO, INFO, OFF);

    ################################################################################################################################
    # Build documentation
    ################################################################################################################################
    if ($ARGV[0] eq 'doc')
    {
        processBegin('LaTeX install');
        executeTest(
            'sudo apt-get install -y --no-install-recommends texlive-latex-base texlive-latex-extra texlive-fonts-recommended',
            {bSuppressStdErr => true});
        executeTest('sudo apt-get install -y texlive-font-utils latex-xcolor', {bSuppressStdErr => true});
        processEnd();

        processBegin(VM_CO6 . ' build');
        executeTest("${strTestExe} --vm-build --vm=" . VM_CO6, {bShowOutputAsync => true});
        processEnd();

        processBegin(VM_U16 . ' build');
        executeTest("${strTestExe} --vm-build --vm=" . VM_U16, {bShowOutputAsync => true});
        processEnd();

        processBegin('release documentation doc');
        executeTest("${strReleaseExe} --build", {bShowOutputAsync => true});
        processEnd();
    }

    ################################################################################################################################
    # Run test
    ################################################################################################################################
    elsif ($ARGV[0] eq 'test')
    {
        # VM must be defined
        if (!defined($strVm))
        {
            confess &log(ERROR, '--vm is required');
        }

        # Only lint on U16
        my $strParam = undef;

        if ($strVm ne VM_U16)
        {
            $strParam .= '--no-lint';
        }

        processBegin("${strVm} build");
        executeTest("${strTestExe} --vm-build --vm=${strVm}", {bShowOutputAsync => true});
        processEnd();

        processBegin("${strVm} test" . (defined($strParam) ? ": ${strParam}" : ''));
        executeTest(
            "${strTestExe} --no-gen --no-ci-config --vm-host=" . VM_U14 . " --vm-max=2 --vm=${strVm}" .
                (defined($strParam) ? " ${strParam}" : ''),
            {bShowOutputAsync => true});
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
