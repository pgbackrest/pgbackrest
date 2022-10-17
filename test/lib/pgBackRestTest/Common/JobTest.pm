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
use File::Basename qw(dirname basename);
use POSIX qw(ceil);
use Time::HiRes qw(gettimeofday usleep);

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::BuildTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::CoverageTest;
use pgBackRestTest::Common::DbVersion;
use pgBackRestTest::Common::DefineTest;
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
        $self->{oTest},
        $self->{bDryRun},
        $self->{bVmOut},
        $self->{iVmIdx},
        $self->{iVmMax},
        $self->{strMakeCmd},
        $self->{iTestIdx},
        $self->{iTestMax},
        $self->{strLogLevel},
        $self->{strLogLevelTest},
        $self->{strLogLevelTestFile},
        $self->{bLogTimestamp},
        $self->{bShowOutputAsync},
        $self->{bNoCleanup},
        $self->{iRetry},
        $self->{bValgrindUnit},
        $self->{bCoverageUnit},
        $self->{bCoverageSummary},
        $self->{bOptimize},
        $self->{bBackTrace},
        $self->{bProfile},
        $self->{iScale},
        $self->{strTimeZone},
        $self->{bDebug},
        $self->{bDebugTestTrace},
        $self->{iBuildMax},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oStorageTest'},
            {name => 'strBackRestBase'},
            {name => 'strTestPath'},
            {name => 'oTest'},
            {name => 'bDryRun'},
            {name => 'bVmOut'},
            {name => 'iVmIdx'},
            {name => 'iVmMax'},
            {name => 'strMakeCmd'},
            {name => 'iTestIdx'},
            {name => 'iTestMax'},
            {name => 'strLogLevel'},
            {name => 'strLogLevelTest'},
            {name => 'strLogLevelTestFile'},
            {name => 'bLogTimestamp'},
            {name => 'bShowOutputAsync'},
            {name => 'bNoCleanup'},
            {name => 'iRetry'},
            {name => 'bValgrindUnit'},
            {name => 'bCoverageUnit'},
            {name => 'bCoverageSummary'},
            {name => 'bOptimize'},
            {name => 'bBackTrace'},
            {name => 'bProfile'},
            {name => 'iScale'},
            {name => 'strTimeZone', required => false},
            {name => 'bDebug'},
            {name => 'bDebugTestTrace'},
            {name => 'iBuildMax'},
        );

    # Set try to 0
    $self->{iTry} = 0;

    # Setup the path where unit test will be built
    $self->{strUnitPath} = "$self->{strTestPath}/unit-$self->{iVmIdx}/$self->{oTest}->{&TEST_VM}";
    $self->{strDataPath} = "$self->{strTestPath}/data-$self->{iVmIdx}";
    $self->{strRepoPath} = "$self->{strTestPath}/repo";

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

    # Start the test timer
    my $fTestStartTime = gettimeofday();

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

        my $strTest = sprintf(
            'P%0' . length($self->{iVmMax}) . 'd-T%0' . length($self->{iTestMax}) . 'd/%0' .
            length($self->{iTestMax}) . "d - ", $self->{iVmIdx} + 1, $self->{iTestIdx} + 1, $self->{iTestMax}) .
            'vm=' . $self->{oTest}->{&TEST_VM} .
            ', module=' . $self->{oTest}->{&TEST_MODULE} .
            ', test=' . $self->{oTest}->{&TEST_NAME} .
            (defined($self->{oTest}->{&TEST_RUN}) ? ', run=' . join(',', sort(@{$self->{oTest}->{&TEST_RUN}})) : '') .
            (defined($self->{oTest}->{&TEST_DB}) ? ', pg-version=' . $self->{oTest}->{&TEST_DB} : '') .
            ($self->{iTry} > 1 ? ' (retry ' . ($self->{iTry} - 1) . ')' : '');

        my $strImage = 'test-' . $self->{iVmIdx};
        my $strDbVersion = (defined($self->{oTest}->{&TEST_DB}) ? $self->{oTest}->{&TEST_DB} : PG_VERSION_94);
        $strDbVersion =~ s/\.//;

        &log($self->{bDryRun} && !$self->{bVmOut} || $self->{bShowOutputAsync} ? INFO : DETAIL, "${strTest}" .
             (!($self->{bDryRun} || !$self->{bVmOut}) || $self->{bShowOutputAsync} ? "\n" : ''));

        my $strHostTestPath = "$self->{strTestPath}/${strImage}";
        my $strVmTestPath = $strHostTestPath;

        # Don't create the container if this is a dry run unless output from the VM is required. Output can be requested
        # to get more information about the specific tests that will be run.
        if (!$self->{bDryRun} || $self->{bVmOut})
        {
            # Create host test directory
            $self->{oStorageTest}->pathCreate($strHostTestPath, {strMode => '0770'});

            # Create unit directory
            if ($self->{oTest}->{&TEST_C} && !$self->{oStorageTest}->pathExists($self->{strUnitPath}))
            {
                $self->{oStorageTest}->pathCreate($self->{strUnitPath}, {strMode => '0770', bCreateParent => true});
            }

            # Create data directory
            if ($self->{oTest}->{&TEST_C} && !$self->{oStorageTest}->pathExists($self->{strDataPath}))
            {
                $self->{oStorageTest}->pathCreate($self->{strDataPath}, {strMode => '0770'});
            }

            # Create ccache directory
            my $strCCachePath = "$self->{strTestPath}/ccache-$self->{iVmIdx}/$self->{oTest}->{&TEST_VM}";

            if ($self->{oTest}->{&TEST_C} && !$self->{oStorageTest}->pathExists($strCCachePath))
            {
                $self->{oStorageTest}->pathCreate($strCCachePath, {strMode => '0770', bCreateParent => true});
            }

            if ($self->{oTest}->{&TEST_CONTAINER})
            {
                if ($self->{oTest}->{&TEST_VM} ne VM_NONE)
                {
                    my $strBinPath = $self->{strTestPath} . '/bin/' . $self->{oTest}->{&TEST_VM} . '/' . PROJECT_EXE;
                    my $strBuildPath = $self->{strTestPath} . '/build/' . $self->{oTest}->{&TEST_VM};

                    executeTest(
                        'docker run -itd -h ' . $self->{oTest}->{&TEST_VM} . "-test --name=${strImage}" .
                        " -v ${strHostTestPath}:${strVmTestPath}" .
                        ($self->{oTest}->{&TEST_C} ? " -v $self->{strUnitPath}:$self->{strUnitPath}" : '') .
                        ($self->{oTest}->{&TEST_C} ? " -v $self->{strDataPath}:$self->{strDataPath}" : '') .
                        " -v $self->{strBackRestBase}:$self->{strBackRestBase}" .
                        " -v $self->{strRepoPath}:$self->{strRepoPath}" .
                        ($self->{oTest}->{&TEST_BIN_REQ} ? " -v ${strBinPath}:${strBinPath}:ro" : '') .
                        ($self->{oTest}->{&TEST_C} ? " -v ${strBuildPath}:${strBuildPath}:ro" : '') .
                        ($self->{oTest}->{&TEST_C} ? " -v ${strCCachePath}:/home/${\TEST_USER}/.ccache" : '') .
                        ' ' . containerRepo() . ':' . $self->{oTest}->{&TEST_VM} . '-test',
                        {bSuppressStdErr => true});
                }
            }
        }

        # Disable debug/coverage for performance and profile tests
        my $bPerformance = $self->{oTest}->{&TEST_TYPE} eq TESTDEF_PERFORMANCE;

        if ($bPerformance || $self->{bProfile})
        {
            $self->{bDebug} = false;
            $self->{bDebugTestTrace} = false;
            $self->{bCoverageUnit} = false;
        }

        # Is coverage being tested?
        my $bCoverage = vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit};

        # Create run parameters
        my $strCommandRunParam = '';

        foreach my $iRunIdx (@{$self->{oTest}->{&TEST_RUN}})
        {
            $strCommandRunParam .= ' --run=' . $iRunIdx;
        }

        if (!$self->{bDryRun} || $self->{bVmOut})
        {
            my $strCommand = undef;                                 # Command to run test

            # If testing with C harness
            if ($self->{oTest}->{&TEST_C})
            {
                # Create command
                # ------------------------------------------------------------------------------------------------------------------
                # Build filename for valgrind suppressions
                my $strValgrindSuppress = $self->{strRepoPath} . '/test/src/valgrind.suppress.' . $self->{oTest}->{&TEST_VM};

                $strCommand =
                    ($self->{oTest}->{&TEST_VM} ne VM_NONE ? "docker exec -i -u ${\TEST_USER} ${strImage} bash -l -c '" : '') .
                    " \\\n" .
                    $self->{strTestPath} . '/build/' . $self->{oTest}->{&TEST_VM} . '/test/src/test-pgbackrest' .
                        ' --repo-path=' . $self->{strTestPath} . '/repo' . ' --test-path=' . $self->{strTestPath} .
                        " --log-level=$self->{strLogLevel}" . ' --vm=' . $self->{oTest}->{&TEST_VM} .
                        ' --vm-id=' . $self->{iVmIdx} . ($self->{bProfile} ? ' --profile' : '') .
                        ($bCoverage ? '' : ' --no-coverage') . ' test ' .
                        $self->{oTest}->{&TEST_MODULE} . '/' . $self->{oTest}->{&TEST_NAME} . " && \\\n" .
                    # Allow stderr to be copied to stderr and stdout
                    "exec 3>&1 && \\\n" .
                    # Test with valgrind when requested
                    ($self->{bValgrindUnit} && $self->{oTest}->{&TEST_TYPE} ne TESTDEF_PERFORMANCE ?
                        'valgrind -q --gen-suppressions=all' .
                        ($self->{oStorageTest}->exists($strValgrindSuppress) ? " --suppressions=${strValgrindSuppress}" : '') .
                        " --exit-on-first-error=yes --leak-check=full --leak-resolution=high --error-exitcode=25" . ' ' : '') .
                        "$self->{strUnitPath}/build/test-unit 2>&1 1>&3 | tee /dev/stderr" .
                    ($self->{oTest}->{&TEST_VM} ne VM_NONE ? "'" : '');
            }
            else
            {
                $strCommand =
                    ($self->{oTest}->{&TEST_CONTAINER} ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                    abs_path($0) .
                    " --test-path=${strVmTestPath}" .
                    " --vm=$self->{oTest}->{&TEST_VM}" .
                    " --vm-id=$self->{iVmIdx}" .
                    " --module=" . $self->{oTest}->{&TEST_MODULE} .
                    ' --test=' . $self->{oTest}->{&TEST_NAME} .
                    $strCommandRunParam .
                    (defined($self->{oTest}->{&TEST_DB}) ? ' --pg-version=' . $self->{oTest}->{&TEST_DB} : '') .
                    ($self->{strLogLevel} ne lc(INFO) ? " --log-level=$self->{strLogLevel}" : '') .
                    ($self->{strLogLevelTestFile} ne lc(TRACE) ? " --log-level-test-file=$self->{strLogLevelTestFile}" : '') .
                    ($self->{bLogTimestamp} ? '' : ' --no-log-timestamp') .
                    ' --pgsql-bin=' . $self->{oTest}->{&TEST_PGSQL_BIN} .
                    ($self->{strTimeZone} ? " --tz='$self->{strTimeZone}'" : '') .
                    ($self->{bDryRun} ? ' --dry-run' : '') .
                    ($self->{bDryRun} ? ' --vm-out' : '') .
                    ($self->{bNoCleanup} ? " --no-cleanup" : '');
            }

            my $oExec = new pgBackRestTest::Common::ExecuteTest(
                $strCommand, {bSuppressError => true, bShowOutputAsync => $self->{bShowOutputAsync}});

            $oExec->begin();

            $self->{oProcess} =
            {
                exec => $oExec,
                test => $strTest,
                start_time => $fTestStartTime,
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

    my $iExitStatus = $oExecDone->end($self->{iVmMax} == 1);

    if (defined($iExitStatus))
    {
        my $strImage = 'test-' . $self->{iVmIdx};

        if ($self->{bShowOutputAsync})
        {
            syswrite(*STDOUT, "\n");
        }

        # If C code generate profile info
        if ($iExitStatus == 0 && $self->{oTest}->{&TEST_C} && $self->{bProfile})
        {
            executeTest(
                ($self->{oTest}->{&TEST_VM} ne VM_NONE ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                    "gprof $self->{strUnitPath}/build/test-unit $self->{strUnitPath}/build/gmon.out >" .
                    " $self->{strUnitPath}/gprof.txt");

            $self->{oStorageTest}->pathCreate(
                "$self->{strBackRestBase}/test/result/profile", {strMode => '0750', bIgnoreExists => true, bCreateParent => true});
            $self->{oStorageTest}->copy(
                "$self->{strUnitPath}/gprof.txt", "$self->{strBackRestBase}/test/result/profile/gprof.txt");
        }

        # If C code generate coverage info
        if ($iExitStatus == 0 && $self->{oTest}->{&TEST_C} && vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit})
        {
            coverageExtract(
                $self->{oStorageTest}, $self->{oTest}->{&TEST_MODULE}, $self->{oTest}->{&TEST_NAME},
                $self->{oTest}->{&TEST_VM} ne VM_NONE, $self->{bCoverageSummary},
                $self->{oTest}->{&TEST_VM} eq VM_NONE ? undef : $strImage, $self->{strTestPath}, "$self->{strTestPath}/temp",
                -e "$self->{strUnitPath}/build/test-unit.p" ?
                    "$self->{strUnitPath}/build/test-unit.p" : "$self->{strUnitPath}/build/test-unit\@exe",
                $self->{strBackRestBase} . '/test/result');
        }

        # Record elapsed time
        my $fTestElapsedTime = ceil((gettimeofday() - $self->{oProcess}{start_time}) * 100) / 100;

        # Output error
        if ($iExitStatus != 0 || (defined($oExecDone->{strErrorLog}) && $oExecDone->{strErrorLog} ne ''))
        {
            # Get stdout
            my $strOutput = trim($oExecDone->{strOutLog}) ? "STDOUT:\n" . trim($oExecDone->{strOutLog}) : '';

            # Get stderr
            if (defined($oExecDone->{strErrorLog}) && trim($oExecDone->{strErrorLog}) ne '')
            {
                if ($strOutput ne '')
                {
                    $strOutput .= "\n";
                }

                $strOutput .= "STDERR:\n" . trim($oExecDone->{strErrorLog});
            }

            # If no stdout or stderr output something rather than a blank line
            if ($strOutput eq '')
            {
                $strOutput = 'NO OUTPUT ON STDOUT OR STDERR';
            }

            &log(ERROR, "${strTestDone} (err${iExitStatus}" . ($self->{bLogTimestamp} ? "-${fTestElapsedTime}s)" : '') .
                 (defined($oExecDone->{strOutLog}) && !$self->{bShowOutputAsync} ? ":\n\n${strOutput}\n" : ''), undef, undef, 4);

            $bFail = true;
        }
        # Output success
        else
        {
            &log(INFO, "${strTestDone}" . ($self->{bLogTimestamp} ? " (${fTestElapsedTime}s)" : '').
                 ($self->{bVmOut} && !$self->{bShowOutputAsync} ?
                     ":\n\n" . trim($oExecDone->{strOutLog}) . "\n" : ''), undef, undef, 4);
        }

        if (!$self->{bNoCleanup})
        {
            my $strHostTestPath = "$self->{strTestPath}/${strImage}";

            if ($self->{oTest}->{&TEST_VM} ne VM_NONE)
            {
                containerRemove("test-$self->{iVmIdx}");
            }

            executeTest("chmod -R 700 ${strHostTestPath}/* 2>&1;rm -rf ${strHostTestPath}");
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
