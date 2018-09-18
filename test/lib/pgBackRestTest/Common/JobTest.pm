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
# Build flags from the last build.  When the build flags change test files must be rebuilt
####################################################################################################################################
my $rhBuildFlags = undef;

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
        $self->{strLogLevelTest},
        $self->{bLogForce},
        $self->{bShowOutputAsync},
        $self->{bNoCleanup},
        $self->{iRetry},
        $self->{bValgrindUnit},
        $self->{bCoverageUnit},
        $self->{bOptimize},
        $self->{bBackTrace},
        $self->{bProfile},
        $self->{bDebug},
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
            {name => 'strLogLevelTest'},
            {name => 'bLogForce'},
            {name => 'bShowOutputAsync'},
            {name => 'bNoCleanup'},
            {name => 'iRetry'},
            {name => 'bValgrindUnit'},
            {name => 'bCoverageUnit'},
            {name => 'bOptimize'},
            {name => 'bBackTrace'},
            {name => 'bProfile'},
            {name => 'bDebug'},
        );

    # Set try to 0
    $self->{iTry} = 0;

    # Setup the path where gcc coverage will be performed
    $self->{strGCovPath} = "$self->{strTestPath}/gcov-$self->{oTest}->{&TEST_VM}-$self->{iVmIdx}";
    $self->{strExpectPath} = "$self->{strTestPath}/expect-$self->{iVmIdx}";

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
            my $bGCovExists = true;

            if ($self->{oTest}->{&TEST_C} && !$self->{oStorageTest}->pathExists($self->{strGCovPath}))
            {
                $self->{oStorageTest}->pathCreate($self->{strGCovPath}, {strMode => '0770'});
                $bGCovExists = false;
            }

            # Create expect directory
            if ($self->{oTest}->{&TEST_C} && !$self->{oStorageTest}->pathExists($self->{strExpectPath}))
            {
                $self->{oStorageTest}->pathCreate($self->{strExpectPath}, {strMode => '0770'});
            }

            if ($self->{oTest}->{&TEST_CONTAINER})
            {
                executeTest(
                    'docker run -itd -h ' . $self->{oTest}->{&TEST_VM} . "-test --name=${strImage}" .
                    " -v $self->{strCoveragePath}:$self->{strCoveragePath} " .
                    " -v ${strHostTestPath}:${strVmTestPath}" .
                    ($self->{oTest}->{&TEST_C} ? " -v $self->{strGCovPath}:$self->{strGCovPath}" : '') .
                    ($self->{oTest}->{&TEST_C} ? " -v $self->{strExpectPath}:$self->{strExpectPath}" : '') .
                    " -v $self->{strBackRestBase}:$self->{strBackRestBase} " .
                    containerRepo() . ':' . $self->{oTest}->{&TEST_VM} .
                    "-test",
                    {bSuppressStdErr => true});

                # If testing C code copy source files to the test directory
                if ($self->{oTest}->{&TEST_C})
                {
                    # If any of the build flags have changed then we'll need to rebuild from scratch
                    my $bFlagsChanged =
                        defined($rhBuildFlags->{$self->{iVmIdx}}) != defined($self->{oTest}->{&TEST_CDEF}) ||
                        defined($rhBuildFlags->{$self->{iVmIdx}}) &&
                        $rhBuildFlags->{$self->{iVmIdx}} ne $self->{oTest}->{&TEST_CDEF};

                    if (!$bGCovExists || $bFlagsChanged)
                    {
                        executeTest("rsync -rt --delete $self->{strBackRestBase}/src/ $self->{strGCovPath}");
                        executeTest("rsync -t $self->{strBackRestBase}/libc/LibC.h $self->{strGCovPath}");
                        executeTest("rsync -rt $self->{strBackRestBase}/libc/xs/ $self->{strGCovPath}/xs");
                        executeTest("rsync -rt $self->{strBackRestBase}/test/src/ $self->{strGCovPath}");
                    }

                    $rhBuildFlags->{$self->{iVmIdx}} = $self->{oTest}->{&TEST_CDEF};
                }

                # If testing Perl code (or C code that calls Perl code) install bin and Perl C Library
                if (!$self->{oTest}->{&TEST_C} || $self->{oTest}->{&TEST_PERL_REQ})
                {
                    jobInstallC(
                        $self->{strBackRestBase}, $self->{oTest}->{&TEST_VM}, $strImage,
                        !$self->{oTest}->{&TEST_C} && !$self->{oTest}->{&TEST_INTEGRATION});
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
            $strCommand =
                'docker exec -i -u ' . TEST_USER . " ${strImage} bash -l -c '" .
                "cd $self->{strGCovPath} && " .
                "make -s 2>&1 &&" .
                ($self->{oTest}->{&TEST_VM} ne VM_CO6 && $self->{bValgrindUnit}?
                    " valgrind -q --gen-suppressions=all --suppressions=$self->{strBackRestBase}/test/src/valgrind.suppress" .
                    " --leak-check=full --leak-resolution=high --error-exitcode=25" : '') .
                " ./test 2>&1'";
        }
        else
        {
            $strCommand =
                ($self->{oTest}->{&TEST_CONTAINER} ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                testRunExe(
                    vmCoverageC($self->{oTest}->{&TEST_VM}), undef, abs_path($0), dirname($self->{strCoveragePath}),
                    $self->{strBackRestBase}, $self->{oTest}->{&TEST_MODULE}, $self->{oTest}->{&TEST_NAME}) .
                " --test-path=${strVmTestPath}" .
                " --vm=$self->{oTest}->{&TEST_VM}" .
                " --vm-id=$self->{iVmIdx}" .
                " --module=" . $self->{oTest}->{&TEST_MODULE} .
                ' --test=' . $self->{oTest}->{&TEST_NAME} .
                $strCommandRunParam .
                (defined($self->{oTest}->{&TEST_DB}) ? ' --pg-version=' . $self->{oTest}->{&TEST_DB} : '') .
                ($self->{strLogLevel} ne lc(INFO) ? " --log-level=$self->{strLogLevel}" : '') .
                ' --pgsql-bin=' . $self->{oTest}->{&TEST_PGSQL_BIN} .
                ($self->{bLogForce} ? ' --log-force' : '') .
                ($self->{bDryRun} ? ' --dry-run' : '') .
                ($self->{bDryRun} ? ' --vm-out' : '') .
                ($self->{bNoCleanup} ? " --no-cleanup" : '');
        }

        if (!$self->{bDryRun} || $self->{bVmOut})
        {
            # If testing C code
            if ($self->{oTest}->{&TEST_C})
            {
                my $hTest = (testDefModuleTest($self->{oTest}->{&TEST_MODULE}, $self->{oTest}->{&TEST_NAME}));
                my $hTestCoverage = $hTest->{&TESTDEF_COVERAGE};

                my @stryCFile;

                foreach my $strFile (sort(keys(%{$self->{oStorageTest}->manifest($self->{strGCovPath})})))
                {
                    # Skip all files except .c files (including .auto.c)
                    next if $strFile !~ /(?<!\.auto)\.c$/;

                    # !!! Skip main for now until it can be rewritten
                    next if $strFile =~ /main\.c$/;

                    # Skip test.c -- it will be added manually at the end
                    next if $strFile =~ /test\.c$/;

                    if (!defined($hTestCoverage->{substr($strFile, 0, length($strFile) - 2)}) &&
                        $strFile !~ /^module\/[^\/]*\/.*Test\.c$/)
                    {
                        push(@stryCFile, "${strFile}");
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
                    if (!$self->{oStorageTest}->exists("$self->{strGCovPath}/${strCIncludeFile}"))
                    {
                        # Error if code was expected
                        if ($hTestCoverage->{$strFile} ne TESTDEF_COVERAGE_NOCODE)
                        {
                            confess &log(ERROR, "unable to find source file '$self->{strGCovPath}/${strCIncludeFile}'");
                        }

                        $strCIncludeFile = "${strFile}.h";
                    }

                    $strCInclude .= (defined($strCInclude) ? "\n" : '') . "#include \"${strCIncludeFile}\"";
                }

                # Update C test file with test module
                my $strTestC = ${$self->{oStorageTest}->get("$self->{strBackRestBase}/test/src/test.c")};
                $strTestC =~ s/\{\[C\_INCLUDE\]\}/$strCInclude/g;
                $strTestC =~ s/\{\[C\_TEST\_INCLUDE\]\}/\#include \"$strTestFile\"/g;

                # Set globals
                $strTestC =~ s/\{\[C\_TEST\_PATH\]\}/$strVmTestPath/g;
                $strTestC =~ s/\{\[C\_TEST\_EXPECT_PATH\]\}/$self->{strExpectPath}/g;

                # Set defalt log level
                my $strLogLevelTestC = "logLevel" . ucfirst($self->{strLogLevelTest});
                $strTestC =~ s/\{\[C\_LOG\_LEVEL\_TEST\]\}/$strLogLevelTestC/g;

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

                # Build the Makefile
                my $strMakefile =
                    "CC=gcc\n" .
                    "CFLAGS=-I. -std=c99 -fPIC -g" . ($self->{bProfile} ? " -pg" : '') . "\\\n" .
                    "       -Werror -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wno-clobbered -Wswitch-enum -Wconversion \\\n" .
                    ($self->{oTest}->{&TEST_VM} eq VM_U16 || $self->{oTest}->{&TEST_VM} eq VM_U18 ?
                        "       -Wformat-signedness \\\n" : '') .
                    ($self->{oTest}->{&TEST_VM} eq VM_U18 ?
                        "       -Wduplicated-branches -Wduplicated-cond \\\n" : '') .
                    # This warning appears to be broken on U12 even though the functionality is fine
                    ($self->{oTest}->{&TEST_VM} eq VM_U12 || $self->{oTest}->{&TEST_VM} eq VM_CO6 ?
                        "       -Wno-missing-field-initializers \\\n" : '') .
                    # ($self->{oTest}->{&TEST_VM} ne VM_CO6 && $self->{oTest}->{&TEST_VM} ne VM_U12 &&
                    #     $self->{oTest}->{&TEST_MODULE} ne 'perl' && $self->{oTest}->{&TEST_NAME} ne 'exec' ?
                    #         "       -Wpedantic \\\n" : '') .
                    "       -Wformat=2 -Wformat-nonliteral -Wstrict-prototypes -Wpointer-arith -Wvla \\\n" .
                    "       `perl -MExtUtils::Embed -e ccopts`\n" .
                    "LDFLAGS=-lcrypto -lz" . (vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit} ? " -lgcov" : '') .
                        (vmWithBackTrace($self->{oTest}->{&TEST_VM}) && $self->{bBackTrace} ? ' -lbacktrace' : '') .
                        " `perl -MExtUtils::Embed -e ldopts`\n" .
                    'TESTFLAGS=' . ($self->{oTest}->{&TEST_DEBUG_UNIT_SUPPRESS} ? '' : "-DDEBUG_UNIT") .
                        (vmWithBackTrace($self->{oTest}->{&TEST_VM}) && $self->{bBackTrace} ? ' -DWITH_BACKTRACE' : '') .
                        ($self->{oTest}->{&TEST_CDEF} ? " $self->{oTest}->{&TEST_CDEF}" : '') .
                        ($self->{bDebug} ? '' : " -DNDEBUG") .
                    "\n" .
                    "\nSRCS=" . join(' ', @stryCFile) . "\n" .
                    "OBJS=\$(SRCS:.c=.o)\n" .
                    "\n" .
                    "test: \$(OBJS) test.o\n" .
                    "\t\$(CC) -o test \$(OBJS) test.o"  . ($self->{bProfile} ? " -pg" : '') . " \$(LDFLAGS)\n" .
                    "\n" .
                    "test.o: test.c\n" .
	                "\t\$(CC) \$(CFLAGS) \$(TESTFLAGS) -O0" .
                        ($self->{oTest}->{&TEST_VM} ne VM_U12 ? ' -ftree-coalesce-vars' : '') .
                        (vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit} ?
                            ' -fprofile-arcs -ftest-coverage' : '') .
                        " -c test.c\n" .
                    "\n" .
                    ".c.o:\n" .
	                "\t\$(CC) \$(CFLAGS) \$(TESTFLAGS) " . ($self->{bOptimize} ? '-O2' : '-O0') . " -c \$< -o \$@\n";

                $self->{oStorageTest}->put($self->{strGCovPath} . "/Makefile", $strMakefile);
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

    my $iExitStatus = $oExecDone->end(undef, $self->{iVmMax} == 1);

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
                'docker exec -i -u ' . TEST_USER . " ${strImage} " .
                "gprof --flat-profile $self->{strGCovPath}/test $self->{strGCovPath}/gmon.out > $self->{strGCovPath}/gprof.flat.txt");

            $self->{oStorageTest}->pathCreate("$self->{strBackRestBase}/test/profile", {strMode => '0750', bIgnoreExists => true});
            $self->{oStorageTest}->copy(
                "$self->{strGCovPath}/gprof.flat.txt", "$self->{strBackRestBase}/test/profile/gprof.flat.txt");
        }

        # If C code generate coverage info
        if ($iExitStatus == 0 && $self->{oTest}->{&TEST_C} && vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit})
        {
            # Generate a list of files to cover
            my $hTestCoverage =
                (testDefModuleTest($self->{oTest}->{&TEST_MODULE}, $self->{oTest}->{&TEST_NAME}))->{&TESTDEF_COVERAGE};

            my @stryCoveredModule;

            foreach my $strModule (sort(keys(%{$hTestCoverage})))
            {
                push (@stryCoveredModule, $strModule);
            }

            push(
                @stryCoveredModule,
                "module/$self->{oTest}->{&TEST_MODULE}/" . testRunName($self->{oTest}->{&TEST_NAME}, false) . 'Test');

            # Generate coverage reports for the modules
            my $strLCovExe = "lcov --config-file=$self->{strBackRestBase}/test/src/lcov.conf";
            my $strLCovOut = $self->{strGCovPath} . '/test.lcov';
            my $strLCovOutTmp = $self->{strGCovPath} . '/test.tmp.lcov';

            executeTest(
                'docker exec -i -u ' . TEST_USER . " ${strImage} " .
                "${strLCovExe} --capture --directory=$self->{strGCovPath} --o=${strLCovOut}");

            # Generate coverage report for each module
            foreach my $strModule (@stryCoveredModule)
            {
                my $strModuleName = testRunName($strModule, false);
                my $strModuleOutName = $strModuleName;
                my $bTest = false;

                if ($strModuleOutName =~ /^module/mg)
                {
                    $strModuleOutName =~ s/^module/test/mg;
                    $bTest = true;
                }

                # Generate lcov reports
                my $strModulePath = $self->{strBackRestBase} . "/test/.vagrant/code/${strModuleOutName}";
                my $strLCovFile = "${strModulePath}.lcov";
                my $strLCovTotal = $self->{strBackRestBase} . "/test/.vagrant/code/all.lcov";

                executeTest(
                    "${strLCovExe} --extract=${strLCovOut} */${strModuleName}.c --o=${strLCovOutTmp}");

                # Combine with prior run if there was one
                if ($self->{oStorageTest}->exists($strLCovFile))
                {
                    my $strCoverage = ${$self->{oStorageTest}->get($strLCovOutTmp)};
                    $strCoverage =~ s/^SF\:.*$/SF:$strModulePath\.c/mg;
                    $self->{oStorageTest}->put($strLCovOutTmp, $strCoverage);

                    executeTest(
                        "${strLCovExe} --add-tracefile=${strLCovOutTmp} --add-tracefile=${strLCovFile} --o=${strLCovOutTmp}");
                }

                # Update source file
                my $strCoverage = ${$self->{oStorageTest}->get($strLCovOutTmp)};

                if (defined($strCoverage))
                {
                    if (!$bTest && $hTestCoverage->{$strModule} eq TESTDEF_COVERAGE_NOCODE)
                    {
                        confess &log(ERROR, "module '${strModule}' is marked 'no code' but has code");
                    }

                    # Get coverage info
                    my $iTotalLines = (split(':', ($strCoverage =~ m/^LF:.*$/mg)[0]))[1] + 0;
                    my $iCoveredLines = (split(':', ($strCoverage =~ m/^LH:.*$/mg)[0]))[1] + 0;

                    my $iTotalBranches = 0;
                    my $iCoveredBranches = 0;

                    if ($strCoverage =~ /^BRF\:/mg && $strCoverage =~ /^BRH\:/mg)
                    {
                        # If this isn't here the statements below fail -- huh?
                        my @match = $strCoverage =~ m/^BRF\:.*$/mg;

                        $iTotalBranches = (split(':', ($strCoverage =~ m/^BRF:.*$/mg)[0]))[1] + 0;
                        $iCoveredBranches = (split(':', ($strCoverage =~ m/^BRH:.*$/mg)[0]))[1] + 0;
                    }

                    # Report coverage if this is not a test or if the test does not have complete coverage
                    if (!$bTest || $iTotalLines != $iCoveredLines || $iTotalBranches != $iCoveredBranches)
                    {
                        # Fix source file name
                        $strCoverage =~ s/^SF\:.*$/SF:$strModulePath\.c/mg;

                        $self->{oStorageTest}->put($strLCovFile, $strCoverage);

                        if ($self->{oStorageTest}->exists($strLCovTotal))
                        {
                            executeTest(
                                "${strLCovExe} --add-tracefile=${strLCovFile} --add-tracefile=${strLCovTotal} --o=${strLCovTotal}");
                        }
                        else
                        {
                            $self->{oStorageTest}->copy($strLCovFile, $strLCovTotal)
                        }
                    }
                    else
                    {
                        $self->{oStorageTest}->remove($strLCovFile);
                    }
                }
                else
                {
                    if ($hTestCoverage->{$strModule} ne TESTDEF_COVERAGE_NOCODE)
                    {
                        confess &log(ERROR, "module '${strModule}' is marked 'code' but has no code");
                    }
                }
            }
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
    my $bCopyLibC = shift;

    # Install Perl C Library
    my $oVm = vmGet();
    my $strBuildPath = "${strBasePath}/test/.vagrant/bin/${strVm}";
    my $strBuildLibCPath = "${strBuildPath}/libc";
    my $strBuildBinPath = "${strBuildPath}/src";
    my $strPerlAutoPath = $oVm->{$strVm}{&VMDEF_PERL_ARCH_PATH} . '/auto/pgBackRest/LibC';

    executeTest(
        "docker exec -i -u root ${strImage} bash -c '" .
        (defined($bCopyLibC) && $bCopyLibC ?
            "mkdir -p -m 755 ${strPerlAutoPath} && " .
            "cp ${strBuildLibCPath}/blib/arch/auto/pgBackRest/LibC/LibC.so ${strPerlAutoPath} && " : '') .
        "cp ${strBuildBinPath}/" . BACKREST_EXE . ' /usr/bin/' . BACKREST_EXE . ' && ' .
        'chmod 755 /usr/bin/' . BACKREST_EXE . "'");
}

push(@EXPORT, qw(jobInstallC));

1;
