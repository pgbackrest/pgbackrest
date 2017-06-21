####################################################################################################################################
# JobTest.pm - Run a test job and monitor progress
####################################################################################################################################
package pgBackRestTest::Common::JobTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);
use POSIX qw(ceil);
use Time::HiRes qw(gettimeofday);

use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::ListTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{oStorageTest},
        $self->{strBackRestBase},
        $self->{strTestPath},
        $self->{strCoveragePath},
        $self->{oTest},
        $self->{bDryRun},
        $self->{bVmOut},
        $self->{iVmIdx},
        $self->{iVmMax},
        $self->{iTestIdx},
        $self->{iTestMax},
        $self->{strLogLevel},
        $self->{bLogForce},
        $self->{bShowOutputAsync},
        $self->{bNoCleanup},
        $self->{iRetry},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oStorageTest'},
            {name => 'strBackRestBase'},
            {name => 'strTestPath'},
            {name => 'strCoveragePath'},
            {name => 'oTest'},
            {name => 'bDryRun'},
            {name => 'bVmOut'},
            {name => 'iVmIdx'},
            {name => 'iVmMax'},
            {name => 'iTestIdx'},
            {name => 'iTestMax'},
            {name => 'strLogLevel'},
            {name => 'bLogForce'},
            {name => 'bShowOutputAsync'},
            {name => 'bNoCleanup'},
            {name => 'iRetry'},
        );

    # Set try to 0
    $self->{iTry} = 0;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    (my $strOperation) = logDebugParam (__PACKAGE__ . '->run', \@_,);

    # Was the job run?
    my $bRun = false;

    # Should the job be run?
    $self->{iTry}++;

    if ($self->{iTry} <= ($self->{iRetry} + 1))
    {
        if ($self->{iTry} != 1 && $self->{iTry} == ($self->{iRetry} + 1))
        {
            $self->{strLogLevel} = lc(DEBUG);
        }

        my $strTest = sprintf('P%0' . length($self->{iVmMax}) . 'd-T%0' . length($self->{iTestMax}) . 'd/%0' .
                              length($self->{iTestMax}) . "d - ", $self->{iVmIdx} + 1, $self->{iTestIdx} + 1, $self->{iTestMax}) .
                              'vm=' . $self->{oTest}->{&TEST_VM} .
                              ', module=' . $self->{oTest}->{&TEST_MODULE} .
                              ', test=' . $self->{oTest}->{&TEST_NAME} .
                              (defined($self->{oTest}->{&TEST_RUN}) ? ', run=' . join(',', @{$self->{oTest}->{&TEST_RUN}}) : '') .
                              (defined($self->{oTest}->{&TEST_DB}) ? ', db=' . $self->{oTest}->{&TEST_DB} : '') .
                              ($self->{iTry} > 1 ? ' (retry ' . ($self->{iTry} - 1) . ')' : '');

        my $strImage = 'test-' . $self->{iVmIdx};
        my $strDbVersion = (defined($self->{oTest}->{&TEST_DB}) ? $self->{oTest}->{&TEST_DB} : PG_VERSION_94);
        $strDbVersion =~ s/\.//;

        &log($self->{bDryRun} && !$self->{bVmOut} || $self->{bShowOutputAsync} ? INFO : DETAIL, "${strTest}" .
             (!($self->{bDryRun} || !$self->{bVmOut}) || $self->{bShowOutputAsync} ? "\n" : ''));

        my $strVmTestPath = '/home/' . TEST_USER . "/test/${strImage}";
        my $strHostTestPath = "$self->{strTestPath}/${strImage}";

        # Don't create the container if this is a dry run unless output from the VM is required.  Ouput can be requested
        # to get more information about the specific tests that will be run.
        if (!$self->{bDryRun} || $self->{bVmOut})
        {
            # Create host test directory
            $self->{oStorageTest}->pathCreate($strHostTestPath, {strMode => '0770'});

            if ($self->{oTest}->{&TEST_CONTAINER})
            {
                executeTest(
                    'docker run -itd -h ' . $self->{oTest}->{&TEST_VM} . "-test --name=${strImage}" .
                    " -v $self->{strCoveragePath}:$self->{strCoveragePath} " .
                    " -v ${strHostTestPath}:${strVmTestPath}" .
                    " -v $self->{strBackRestBase}:$self->{strBackRestBase} " .
                    containerRepo() . ':' . $self->{oTest}->{&TEST_VM} .
                    "-loop-test-pre",
                    {bSuppressStdErr => true});
            }
        }

        # Create run parameters
        my $strCommandRunParam = '';

        foreach my $iRunIdx (@{$self->{oTest}->{&TEST_RUN}})
        {
            $strCommandRunParam .= ' --run=' . $iRunIdx;
        }

        # Create command
        my $strCommand =
            ($self->{oTest}->{&TEST_CONTAINER} ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
            (vmCoverage($self->{oTest}->{&TEST_VM}) ? testRunExe(
                abs_path($0), dirname($self->{strCoveragePath}), $self->{strBackRestBase}, $self->{oTest}->{&TEST_MODULE},
                $self->{oTest}->{&TEST_NAME}, defined($self->{oTest}->{&TEST_RUN}) ? $self->{oTest}->{&TEST_RUN} : 'all') :
                abs_path($0)) .
            " --test-path=${strVmTestPath}" .
            " --vm=$self->{oTest}->{&TEST_VM}" .
            " --vm-id=$self->{iVmIdx}" .
            " --module=" . $self->{oTest}->{&TEST_MODULE} .
            ' --test=' . $self->{oTest}->{&TEST_NAME} .
            $strCommandRunParam .
            (defined($self->{oTest}->{&TEST_DB}) ? ' --db-version=' . $self->{oTest}->{&TEST_DB} : '') .
            ($self->{strLogLevel} ne lc(INFO) ? " --log-level=$self->{strLogLevel}" : '') .
            ' --pgsql-bin=' . $self->{oTest}->{&TEST_PGSQL_BIN} .
            ($self->{bLogForce} ? ' --log-force' : '') .
            ($self->{bDryRun} ? ' --dry-run' : '') .
            ($self->{bDryRun} ? ' --vm-out' : '') .
            ($self->{bNoCleanup} ? " --no-cleanup" : '');

        &log(DETAIL, $strCommand);

        if (!$self->{bDryRun} || $self->{bVmOut})
        {
            my $fTestStartTime = gettimeofday();

            # Set permissions on the Docker test directory.  This can be removed once users/groups are sync'd between
            # Docker and the host VM.
            if ($self->{oTest}->{&TEST_CONTAINER})
            {
                executeTest("docker exec ${strImage} chown " . TEST_USER . ':' . TEST_GROUP . " -R ${strVmTestPath}");
            }

            my $oExec = new pgBackRestTest::Common::ExecuteTest(
                $strCommand,
                {bSuppressError => true, bShowOutputAsync => $self->{bShowOutputAsync}});

            $oExec->begin();

            $self->{oProcess} =
            {
                exec => $oExec,
                test => $strTest,
                # idx => $self->{iTestIdx},
                # container => $self->{oTest}->{&TEST_CONTAINER},
                start_time => $fTestStartTime
            };

            $bRun = true;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bRun', value => $bRun, trace => true}
    );
}


####################################################################################################################################
# end
####################################################################################################################################
sub end
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    (my $strOperation) = logDebugParam (__PACKAGE__ . '->run', \@_,);

    # Is the job done?
    my $bDone = false;
    my $bFail = false;

    my $oExecDone = $self->{oProcess}{exec};
    my $strTestDone = $self->{oProcess}{test};
    my $iTestDoneIdx = $self->{oProcess}{idx};

    my $iExitStatus = $oExecDone->end(undef, $self->{iVmMax} == 1);

    if (defined($iExitStatus))
    {
        if ($self->{bShowOutputAsync})
        {
            syswrite(*STDOUT, "\n");
        }

        my $fTestElapsedTime = ceil((gettimeofday() - $self->{oProcess}{start_time}) * 100) / 100;

        if ($iExitStatus != 0)
        {
            &log(ERROR, "${strTestDone} (err${iExitStatus}-${fTestElapsedTime}s)" .
                 (defined($oExecDone->{strOutLog}) && !$self->{bShowOutputAsync} ?
                    ":\n\n" . trim($oExecDone->{strOutLog}) . "\n" : ''), undef, undef, 4);
            $bFail = true;
        }
        else
        {
            &log(INFO, "${strTestDone} (${fTestElapsedTime}s)".
                 ($self->{bVmOut} && !$self->{bShowOutputAsync} ?
                     ":\n\n" . trim($oExecDone->{strOutLog}) . "\n" : ''), undef, undef, 4);
        }

        if (!$self->{bNoCleanup})
        {
            my $strImage = 'test-' . $self->{iVmIdx};
            my $strHostTestPath = "$self->{strTestPath}/${strImage}";

            containerRemove("test-$self->{iVmIdx}");
            executeTest("sudo rm -rf ${strHostTestPath}");
        }

        $bDone = true;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bDone', value => $bDone, trace => true},
        {name => 'bFail', value => $bFail, trace => true}
    );
}

1;
