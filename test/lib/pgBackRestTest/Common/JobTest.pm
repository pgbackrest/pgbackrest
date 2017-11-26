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
use Time::HiRes qw(gettimeofday);

use pgBackRest::DbVersion;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Version;

use pgBackRestTest::Common::ContainerTest;
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

    # Setup the path where gcc coverage will be performed
    $self->{strGCovPath} = "$self->{strTestPath}/gcov-$self->{iVmIdx}";

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

            # Create gcov directory
            $self->{oStorageTest}->pathCreate($self->{strGCovPath}, {strMode => '0770'});

            if ($self->{oTest}->{&TEST_CONTAINER})
            {
                executeTest(
                    'docker run -itd -h ' . $self->{oTest}->{&TEST_VM} . "-test --name=${strImage}" .
                    " -v $self->{strCoveragePath}:$self->{strCoveragePath} " .
                    " -v ${strHostTestPath}:${strVmTestPath}" .
                    " -v $self->{strGCovPath}:$self->{strGCovPath}" .
                    " -v $self->{strBackRestBase}:$self->{strBackRestBase} " .
                    containerRepo() . ':' . $self->{oTest}->{&TEST_VM} .
                    "-test",
                    {bSuppressStdErr => true});

                # Install bin and Perl C Library
                if (!$self->{oTest}->{&TEST_C})
                {
                    jobInstallC($self->{strBackRestBase}, $self->{oTest}->{&TEST_VM}, $strImage);
                }
            }
        }

        # Create run parameters
        my $strCommandRunParam = '';

        foreach my $iRunIdx (@{$self->{oTest}->{&TEST_RUN}})
        {
            $strCommandRunParam .= ' --run=' . $iRunIdx;
        }

        # Create command
        my $strCommand;

        if ($self->{oTest}->{&TEST_C})
        {
            $strCommand = 'docker exec -i -u ' . TEST_USER . " ${strImage} $self->{strGCovPath}/test";
        }
        else
        {
            $strCommand =
                ($self->{oTest}->{&TEST_CONTAINER} ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                testRunExe(
                    vmCoverage($self->{oTest}->{&TEST_VM}), undef, abs_path($0),
                    dirname($self->{strCoveragePath}), $self->{strBackRestBase}, $self->{oTest}->{&TEST_MODULE},
                    $self->{oTest}->{&TEST_NAME}, defined($self->{oTest}->{&TEST_RUN}) ? $self->{oTest}->{&TEST_RUN} : 'all') .
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
        }

        &log(DETAIL, $strCommand);

        if (!$self->{bDryRun} || $self->{bVmOut})
        {
            # If testing C code
            if ($self->{oTest}->{&TEST_C})
            {
                my $strCSrcPath = "$self->{strBackRestBase}/src";
                my $hTest = (testDefModuleTest($self->{oTest}->{&TEST_MODULE}, $self->{oTest}->{&TEST_NAME}));
                my $hTestCoverage = $hTest->{&TESTDEF_COVERAGE};

                my @stryCFile;

                foreach my $strFile (sort(keys(%{$self->{oStorageTest}->manifest($strCSrcPath)})))
                {
                    # Skip all files except .c files (including .auto.c)
                    next if $strFile !~ /(?<!\.auto)\.c$/;

                    # !!! Skip main for now until it can be rewritten
                    next if $strFile =~ /main\.c$/;

                    if (!defined($hTestCoverage->{substr($strFile, 0, length($strFile) - 2)}))
                    {
                        push(@stryCFile, "${strCSrcPath}/${strFile}");
                    }
                }

                # Generate list of C files to include for testing
                my $strTestFile =
                    "module/$self->{oTest}->{&TEST_MODULE}/" . testRunName($self->{oTest}->{&TEST_NAME}, false) . 'Test.c';
                my $strCInclude;

                foreach my $strFile (sort(keys(%{$hTestCoverage})))
                {
                    # Don't include the test file as it is already included below
                    next if $strFile =~ /Test$/;

                    # Don't any auto files as they are included in their companion C files
                    next if $strFile =~ /auto$/;

                    # Include the C file if it exists
                    my $strCIncludeFile = "${strFile}.c";

                    # If the C file does not exist use the header file instead
                    if (!$self->{oStorageTest}->exists("${strCSrcPath}/${strCIncludeFile}"))
                    {
                        # Error if code was expected
                        if ($hTestCoverage->{$strFile} ne TESTDEF_COVERAGE_NOCODE)
                        {
                            confess &log(ERROR, "unable to file source file '${strCIncludeFile}'");
                        }

                        $strCIncludeFile = "${strFile}.h";
                    }

                    $strCInclude .= (defined($strCInclude) ? "\n" : '') . "#include \"${strCIncludeFile}\"";
                }

                # Update C test file with test module
                my $strTestC = ${$self->{oStorageTest}->get("$self->{strBackRestBase}/test/src/test.c")};
                $strTestC =~ s/\{\[C\_INCLUDE\]\}/$strCInclude/g;
                $strTestC =~ s/\{\[C\_TEST\_INCLUDE\]\}/\#include \"$strTestFile\"/g;

                # Initialize tests
                my $strTestInit;

                for (my $iTestIdx = 1; $iTestIdx <= $hTest->{&TESTDEF_TOTAL}; $iTestIdx++)
                {
                    my $bSelected = false;

                    if (!defined($self->{oTest}->{&TEST_RUN}) || @{$self->{oTest}->{&TEST_RUN}} == 0 ||
                        grep(/^$iTestIdx$/, @{$self->{oTest}->{&TEST_RUN}}))
                    {
                        $bSelected = true;
                    }

                    $strTestInit .=
                        (defined($strTestInit) ? "\n    " : '') .
                        sprintf("testAdd(%3d, %8s);" , $iTestIdx, ($bSelected ? 'true' : 'false'));
                }

                $strTestC =~ s/\{\[C\_TEST\_LIST\]\}/$strTestInit/g;

                # Save C test file
                $self->{oStorageTest}->put("$self->{strGCovPath}/test.c", $strTestC);

                my $strGccCommand =
                    'gcc -std=c99 -fPIC' .
                    (vmCoverage($self->{oTest}->{&TEST_VM}) ? ' -fprofile-arcs -ftest-coverage -O0' : ' -O2') .
                    ' -Werror -Wfatal-errors -Wall -Wextra -Wwrite-strings  -Wno-clobbered' .
                    ($self->{oTest}->{&TEST_VM} ne VM_CO6 && $self->{oTest}->{&TEST_VM} ne VM_U12 ? ' -Wpedantic ' : '') .
                    " -I/$self->{strBackRestBase}/src -I/$self->{strBackRestBase}/test/src test.c" .
                    " /$self->{strBackRestBase}/test/src/common/harnessTest.c " .
                    join(' ', @stryCFile) . " -l crypto -o test";

                executeTest(
                    'docker exec -i -u ' . TEST_USER . " ${strImage} bash -l -c '" .
                    "cd $self->{strGCovPath} && " .
                    "${strGccCommand}'");
            }

            my $oExec = new pgBackRestTest::Common::ExecuteTest(
                $strCommand,
                {bSuppressError => !$self->{oTest}->{&TEST_C}, bShowOutputAsync => $self->{bShowOutputAsync}});

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

    my $iExitStatus = $oExecDone->end(undef, $self->{iVmMax} == 1);

    if (defined($iExitStatus))
    {
        my $strImage = 'test-' . $self->{iVmIdx};

        if ($self->{bShowOutputAsync})
        {
            syswrite(*STDOUT, "\n");
        }

        # If C library generate coverage info
        if ($iExitStatus == 0 && $self->{oTest}->{&TEST_C} && vmCoverage($self->{oTest}->{&TEST_VM}))
        {
            # Generate a list of files to cover
            my $hTestCoverage =
                (testDefModuleTest($self->{oTest}->{&TEST_MODULE}, $self->{oTest}->{&TEST_NAME}))->{&TESTDEF_COVERAGE};

            my @stryCoveredModule;

            foreach my $strModule (sort(keys(%{$hTestCoverage})))
            {
                # Skip modules that have no code
                next if ($hTestCoverage->{$strModule} eq TESTDEF_COVERAGE_NOCODE);

                push (@stryCoveredModule, testRunName(basename($strModule), false) . ".c.gcov");
            }

            # Generate coverage reports for the modules
            executeTest(
                'docker exec -i -u ' . TEST_USER . " ${strImage} bash -l -c '" .
                "cd $self->{strGCovPath} && " .
                "gcov test.c'", {bSuppressStdErr => true});

            # Mark uncoverable statements as successful
            foreach my $strModule (sort(keys(%{$hTestCoverage})))
            {
                # File that contains coverage info for the module
                my $strCoverageFile = "$self->{strGCovPath}/" . testRunName(basename($strModule), false) . ".c.gcov";

                # If marked as no code then error if there is code
                if ($hTestCoverage->{$strModule} eq TESTDEF_COVERAGE_NOCODE)
                {
                    if ($self->{oStorageTest}->exists($strCoverageFile))
                    {
                        confess &log(ERROR, "module '${strModule}' is marked 'no code' but has code");
                    }

                    next;
                }

                # Load the coverage file
                my $strCoverage = ${$self->{oStorageTest}->get($strCoverageFile)};

                # Go line by line and update uncovered statements
                my $strUpdatedCoverage;

                foreach my $strLine (split("\n", trim($strCoverage)))
                {
                    # If the statement is marked uncoverable
                    if ($strLine =~ /\/\/ \{(uncoverable - [^\}]+|\+uncoverable|uncovered - [^\}]+|\+uncovered)\}\s*$/)
                    {
                        # Error if the statement is marked uncoverable but is in fact covered
                        if ($strLine !~ /^    \#\#\#\#\#/)
                        {
                            &log(ERROR, "line in ${strModule}.c is marked as uncoverable but is covered:\n${strLine}");
                        }

                        # Mark the statement as covered with 0 executions
                        $strLine =~ s/^    \#\#\#\#\#/^        0/;
                    }

                    # Add to updated file
                    $strUpdatedCoverage .= "${strLine}\n";
                }

                # Store the updated file
                $self->{oStorageTest}->put($strCoverageFile, $strUpdatedCoverage);
            }

            executeTest(
                'docker exec -i -u ' . TEST_USER . " ${strImage} bash -l -c '" .
                "cd $self->{strGCovPath} && " .
                # Only generate coverage files for VMs that support coverage
                (vmCoverage($self->{oTest}->{&TEST_VM}) ?
                    "gcov2perl -db ../cover_db " . join(' ', @stryCoveredModule) . " && " : '') .
                # If these aren't deleted then cover automagically finds them and includes them in the report
                "rm *.gcda *.c.gcov'");
        }

        # Record elapsed time
        my $fTestElapsedTime = ceil((gettimeofday() - $self->{oProcess}{start_time}) * 100) / 100;

        # Output error
        if ($iExitStatus != 0)
        {
            &log(ERROR, "${strTestDone} (err${iExitStatus}-${fTestElapsedTime}s)" .
                 (defined($oExecDone->{strOutLog}) && !$self->{bShowOutputAsync} ?
                    ":\n\n" . trim($oExecDone->{strOutLog}) . "\n" : ''), undef, undef, 4);
            $bFail = true;
        }
        # Output success
        else
        {
            &log(INFO, "${strTestDone} (${fTestElapsedTime}s)".
                 ($self->{bVmOut} && !$self->{bShowOutputAsync} ?
                     ":\n\n" . trim($oExecDone->{strOutLog}) . "\n" : ''), undef, undef, 4);
        }

        if (!$self->{bNoCleanup})
        {
            my $strHostTestPath = "$self->{strTestPath}/${strImage}";

            containerRemove("test-$self->{iVmIdx}");
            executeTest("sudo rm -rf ${strHostTestPath}");
            executeTest("sudo rm -rf $self->{strGCovPath}");
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

####################################################################################################################################
# Install C binary and library
####################################################################################################################################
sub jobInstallC
{
    my $strBasePath = shift;
    my $strVm = shift;
    my $strImage = shift;

    # Install Perl C Library
    my $oVm = vmGet();
    my $strBuildPath = "${strBasePath}/test/.vagrant";
    my $strBuildLibCPath = "$strBuildPath/libc/${strVm}/libc";
    my $strBuildBinPath = "$strBuildPath/bin/${strVm}/src";
    my $strPerlAutoPath = $oVm->{$strVm}{&VMDEF_PERL_ARCH_PATH} . '/auto/pgBackRest/LibC';
    my $strPerlModulePath = $oVm->{$strVm}{&VMDEF_PERL_ARCH_PATH} . '/pgBackRest';

    executeTest(
        "docker exec -i -u root ${strImage} bash -c '" .
        "mkdir -p -m 755 ${strPerlAutoPath} && " .
        # "cp ${strBuildLibCPath}/blib/arch/auto/pgBackRest/LibC/LibC.bs ${strPerlAutoPath} && " .
        "cp ${strBuildLibCPath}/blib/arch/auto/pgBackRest/LibC/LibC.so ${strPerlAutoPath} && " .
        "cp ${strBuildLibCPath}/blib/lib/auto/pgBackRest/LibC/autosplit.ix ${strPerlAutoPath} && " .
        "mkdir -p -m 755 ${strPerlModulePath} && " .
        "cp ${strBuildLibCPath}/blib/lib/pgBackRest/LibC.pm ${strPerlModulePath} && " .
        "cp ${strBuildLibCPath}/blib/lib/pgBackRest/LibCAuto.pm ${strPerlModulePath} && " .
        "cp ${strBuildBinPath}/" . BACKREST_EXE . ' /usr/bin/' . BACKREST_EXE . ' && ' .
        'chmod 755 /usr/bin/' . BACKREST_EXE . ' && ' .
        "ln -s ${strBasePath}/lib/pgBackRest /usr/share/perl5/pgBackRest'");
}

push(@EXPORT, qw(jobInstallC));

1;
