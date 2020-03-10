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

use pgBackRestTest::Common::BuildTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::CoverageTest;
use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::ListTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;

####################################################################################################################################
# Has the C build directory been initialized yet?
####################################################################################################################################
my $rhBuildInit = undef;

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
        $self->{strVmHost},
        $self->{bVmOut},
        $self->{iVmIdx},
        $self->{iVmMax},
        $self->{iTestIdx},
        $self->{iTestMax},
        $self->{strLogLevel},
        $self->{strLogLevelTest},
        $self->{strLogLevelTestFile},
        $self->{bLogForce},
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
            {name => 'strVmHost'},
            {name => 'bVmOut'},
            {name => 'iVmIdx'},
            {name => 'iVmMax'},
            {name => 'iTestIdx'},
            {name => 'iTestMax'},
            {name => 'strLogLevel'},
            {name => 'strLogLevelTest'},
            {name => 'strLogLevelTestFile'},
            {name => 'bLogForce'},
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

    # Setup the path where gcc coverage will be performed
    $self->{strGCovPath} = "$self->{strTestPath}/gcov-$self->{oTest}->{&TEST_VM}-$self->{iVmIdx}";
    $self->{strDataPath} = "$self->{strTestPath}/data-$self->{iVmIdx}";

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

            # Create data directory
            if ($self->{oTest}->{&TEST_C} && !$self->{oStorageTest}->pathExists($self->{strDataPath}))
            {
                $self->{oStorageTest}->pathCreate($self->{strDataPath}, {strMode => '0770'});
            }

            if ($self->{oTest}->{&TEST_CONTAINER})
            {
                if ($self->{oTest}->{&TEST_VM} ne VM_NONE)
                {
                    executeTest(
                        'docker run -itd -h ' . $self->{oTest}->{&TEST_VM} . "-test --name=${strImage}" .
                        " -v ${strHostTestPath}:${strVmTestPath}" .
                        ($self->{oTest}->{&TEST_C} ? " -v $self->{strGCovPath}:$self->{strGCovPath}" : '') .
                        ($self->{oTest}->{&TEST_C} ? " -v $self->{strDataPath}:$self->{strDataPath}" : '') .
                        " -v $self->{strBackRestBase}:$self->{strBackRestBase} " .
                        containerRepo() . ':' . $self->{oTest}->{&TEST_VM} .
                        "-test",
                        {bSuppressStdErr => true});
                }

                # If testing C code copy source files to the test directory
                if ($self->{oTest}->{&TEST_C})
                {
                    # If this is the first build, then rsync files
                    if (!$rhBuildInit->{$self->{oTest}->{&TEST_VM}}{$self->{iVmIdx}})
                    {
                        executeTest(
                            'rsync -rt --delete --exclude=*.o --exclude=test.c --exclude=test.gcno ' .
                                ' --exclude=test --exclude=buildflags --exclude=testflags --exclude=harnessflags' .
                                ' --exclude=build.auto.h --exclude=build.auto.h.in --exclude=Makefile --exclude=Makefile.in' .
                                ' --exclude=configure --exclude=configure.ac' .
                                " $self->{strBackRestBase}/src/ $self->{strGCovPath} && " .
                            "rsync -rt --delete --exclude=*.o $self->{strBackRestBase}/test/src/ $self->{strGCovPath}/test");
                    }

                    # Build directory has been initialized
                    $rhBuildInit->{$self->{oTest}->{&TEST_VM}}{$self->{iVmIdx}} = true;
                }

                # If testing Perl code (or C code that calls the binary) install binary
                if ($self->{oTest}->{&TEST_VM} ne VM_NONE && (!$self->{oTest}->{&TEST_C} || $self->{oTest}->{&TEST_BIN_REQ}))
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
            $strCommand =
                ($self->{oTest}->{&TEST_VM} ne VM_NONE  ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                "bash -l -c '" .
                "cd $self->{strGCovPath} && " .
                "make -j $self->{iBuildMax} -s 2>&1 &&" .
                ($self->{oTest}->{&TEST_VM} ne VM_CO6 && $self->{bValgrindUnit} &&
                    $self->{oTest}->{&TEST_TYPE} ne TESTDEF_PERFORMANCE ?
                    " valgrind -q --gen-suppressions=all --suppressions=$self->{strGCovPath}/test/valgrind.suppress" .
                    " --leak-check=full --leak-resolution=high --error-exitcode=25" : '') .
                " ./test.bin 2>&1'";
        }
        else
        {
            $strCommand =
                ($self->{oTest}->{&TEST_CONTAINER} ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                abs_path($0) .
                " --test-path=${strVmTestPath}" .
                " --vm-host=$self->{strVmHost}" .
                " --vm=$self->{oTest}->{&TEST_VM}" .
                " --vm-id=$self->{iVmIdx}" .
                " --module=" . $self->{oTest}->{&TEST_MODULE} .
                ' --test=' . $self->{oTest}->{&TEST_NAME} .
                $strCommandRunParam .
                (defined($self->{oTest}->{&TEST_DB}) ? ' --pg-version=' . $self->{oTest}->{&TEST_DB} : '') .
                ($self->{strLogLevel} ne lc(INFO) ? " --log-level=$self->{strLogLevel}" : '') .
                ($self->{strLogLevelTestFile} ne lc(TRACE) ? " --log-level-test-file=$self->{strLogLevelTestFile}" : '') .
                ' --pgsql-bin=' . $self->{oTest}->{&TEST_PGSQL_BIN} .
                ($self->{strTimeZone} ? " --tz='$self->{strTimeZone}'" : '') .
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
                    my $strFileNoExt = substr($strFile, 0, length($strFile) - 2);

                    # Skip all files except .c files (including .auto.c)
                    next if $strFile !~ /(?<!\.auto)\.c$/;

                    # ??? Skip main for now until the function can be renamed to allow unit testing
                    next if $strFile =~ /main\.c$/;

                    # Skip test.c -- it will be added manually at the end
                    next if $strFile =~ /test\.c$/;

                    if (!defined($hTestCoverage->{$strFileNoExt}) && !grep(/^$strFileNoExt$/, @{$hTest->{&TESTDEF_INCLUDE}}) &&
                        $strFile !~ /^test\/module\/[^\/]*\/.*Test\.c$/)
                    {
                        push(@stryCFile, "${strFile}");
                    }
                }

                # Generate list of C files to include for testing
                my $strTestDepend = '';
                my $strTestFile =
                    "test/module/$self->{oTest}->{&TEST_MODULE}/" . testRunName($self->{oTest}->{&TEST_NAME}, false) . 'Test.c';
                my $strCInclude;

                foreach my $strFile (sort(keys(%{$hTestCoverage}), @{$hTest->{&TESTDEF_INCLUDE}}))
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
                    $strTestDepend .= " ${strCIncludeFile}";
                }

                # Update C test file with test module
                my $strTestC = ${$self->{oStorageTest}->get("$self->{strGCovPath}/test/test.c")};

                if (defined($strCInclude))
                {
                    $strTestC =~ s/\{\[C\_INCLUDE\]\}/$strCInclude/g;
                }
                else
                {
                    $strTestC =~ s/\{\[C\_INCLUDE\]\}//g;
                }

                $strTestC =~ s/\{\[C\_TEST\_INCLUDE\]\}/\#include \"$strTestFile\"/g;
                $strTestDepend .= " ${strTestFile}";

                # Build dependencies for the test file
                my $rhDependencyTree = {};
                buildDependencyTreeSub(
                    $self->{oStorageTest}, $rhDependencyTree, $strTestFile, false, $self->{strGCovPath}, ['', 'test']);

                foreach my $strDepend (@{$rhDependencyTree->{$strTestFile}{include}})
                {
                    $strTestDepend .=
                        ' ' . ($rhDependencyTree->{$strDepend}{path} ne '' ? $rhDependencyTree->{$strDepend}{path} . '/' : '') .
                        $strDepend;
                }

                # Determine where the project exe is located
                my $strProjectExePath = $self->{oTest}->{&TEST_VM} eq VM_NONE ?
                    "$self->{strBackRestBase}/test/.vagrant/bin/$self->{oTest}->{&TEST_VM}/src/" . PROJECT_EXE : PROJECT_EXE;

                # Is this test running in a container?
                my $strContainer = $self->{oTest}->{&TEST_VM} eq VM_NONE ? 'false' : 'true';

                # What test path should be passed to C?  Containers always have their test path at ~/test but when running with
                # vm=none it should be in a subdirectory of the current directory.
                my $strTestPathC = $self->{oTest}->{&TEST_VM} eq VM_NONE ? $strHostTestPath : $strVmTestPath;

                # Set globals
                $strTestC =~ s/\{\[C\_TEST\_CONTAINER\]\}/$strContainer/g;
                $strTestC =~ s/\{\[C\_TEST\_PROJECT\_EXE\]\}/$strProjectExePath/g;
                $strTestC =~ s/\{\[C\_TEST\_PATH\]\}/$strTestPathC/g;
                $strTestC =~ s/\{\[C\_TEST\_DATA_PATH\]\}/$self->{strDataPath}/g;
                $strTestC =~ s/\{\[C\_TEST\_IDX\]\}/$self->{iVmIdx}/g;
                $strTestC =~ s/\{\[C\_TEST\_REPO_PATH\]\}/$self->{strBackRestBase}/g;
                $strTestC =~ s/\{\[C\_TEST\_SCALE\]\}/$self->{iScale}/g;

                # Set timezone
                if (defined($self->{strTimeZone}))
                {
                    $strTestC =~ s/\{\[C\_TEST\_TZ\]\}/setenv\("TZ", "$self->{strTimeZone}", true\);/g;
                }
                else
                {
                    $strTestC =~ s/\{\[C\_TEST\_TZ\]\}/\/\/ No timezone specified/g;
                }

                # Set default log level
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
                        sprintf("hrnAdd(%3d, %8s);" , $iTestIdx, ($bSelected ? 'true' : 'false'));
                }

                $strTestC =~ s/\{\[C\_TEST\_LIST\]\}/$strTestInit/g;
                buildPutDiffers($self->{oStorageTest}, "$self->{strGCovPath}/test.c", $strTestC);

                # Create build.auto.h
                my $strBuildAutoH = "";

                buildPutDiffers($self->{oStorageTest}, "$self->{strGCovPath}/" . BUILD_AUTO_H, $strBuildAutoH);

                # Determine which warnings are available
                my $strWarningFlags =
                    '-Werror -Wfatal-errors -Wall -Wextra -Wwrite-strings -Wconversion -Wformat=2' .
                    ' -Wformat-nonliteral -Wstrict-prototypes -Wpointer-arith -Wvla' .
                    ($self->{oTest}->{&TEST_VM} eq VM_U16 || $self->{oTest}->{&TEST_VM} eq VM_U18 ?
                        ' -Wformat-signedness' : '') .
                    ($self->{oTest}->{&TEST_VM} eq VM_U18 ?
                        ' -Wduplicated-branches -Wduplicated-cond' : '') .
                    # This is theoretically a portability issue but a compiler that does not treat NULL and false as 0 is crazy
                        ' -Wno-missing-field-initializers';

                # Flags that are common to all builds
                my $strCommonFlags =
                    '-I. -Itest -std=c99 -fPIC -g -Wno-clobbered -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64' .
                        ' `pkg-config libxml-2.0 --cflags`' . ($self->{bProfile} ? " -pg" : '') .
                        ' -I`pg_config --includedir`' .
                    ($self->{oTest}->{&TEST_DEBUG_UNIT_SUPPRESS} ? '' : " -DDEBUG_UNIT") .
                    (vmWithBackTrace($self->{oTest}->{&TEST_VM}) && $self->{bBackTrace} ? ' -DWITH_BACKTRACE' : '') .
                    ($self->{oTest}->{&TEST_CDEF} ? " $self->{oTest}->{&TEST_CDEF}" : '') .
                    (vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit} ? ' -DDEBUG_COVERAGE' : '') .
                    ($self->{bDebug} && $self->{oTest}->{&TEST_TYPE} ne TESTDEF_PERFORMANCE ? '' : ' -DNDEBUG') .
                    ($self->{bDebugTestTrace} && $self->{bDebug} ? ' -DDEBUG_TEST_TRACE' : '') .
                    ($self->{oTest}->{&TEST_VM} eq VM_CO6 ? ' -DDEBUG_EXEC_TIME' : '');

                # Flags used to build harness files
                my $strHarnessFlags =
                    '-O2' . ($self->{oTest}->{&TEST_VM} ne VM_U12 ? ' -ftree-coalesce-vars' : '') .
                    ($self->{oTest}->{&TEST_CTESTDEF} ? " $self->{oTest}->{&TEST_CTESTDEF}" : '');

                buildPutDiffers(
                    $self->{oStorageTest}, "$self->{strGCovPath}/harnessflags",
                    "${strCommonFlags} ${strWarningFlags} ${strHarnessFlags}");

                # Flags used to build test.c
                my $strTestFlags =
                    ($self->{bDebug} ? '-DDEBUG_TEST_TRACE ' : '') .
                    ($self->{oTest}->{&TEST_VM} eq VM_F30 ? '-O2' : '-O0') .
                    ($self->{oTest}->{&TEST_VM} ne VM_U12 ? ' -ftree-coalesce-vars' : '') .
                    (vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit} ?
                        ' -fprofile-arcs -ftest-coverage' : '') .
                    ($self->{oTest}->{&TEST_CTESTDEF} ? " $self->{oTest}->{&TEST_CTESTDEF}" : '');

                buildPutDiffers(
                    $self->{oStorageTest}, "$self->{strGCovPath}/testflags",
                    "${strCommonFlags} ${strWarningFlags} ${strTestFlags}");

                # Flags used to build all other files
                my $strBuildFlags =
                    ($self->{bOptimize} || $self->{oTest}->{&TEST_VM} eq VM_F30 ?
                        '-O2' : '-O0' . ($self->{oTest}->{&TEST_VM} ne VM_U12 ? ' -ftree-coalesce-vars' : ''));

                buildPutDiffers(
                    $self->{oStorageTest}, "$self->{strGCovPath}/buildflags",
                    "${strCommonFlags} ${strWarningFlags} ${strBuildFlags}");

                # Build the Makefile
                my $strMakefile =
                    "CC=gcc\n" .
                    "COMMONFLAGS=${strCommonFlags}\n" .
                    "WARNINGFLAGS=${strWarningFlags}\n" .
                    "BUILDFLAGS=${strBuildFlags}\n" .
                    "HARNESSFLAGS=${strHarnessFlags}\n" .
                    "TESTFLAGS=${strTestFlags}\n" .
                    "LDFLAGS=-lcrypto -lssl -lxml2 -lz" .
                        (vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit} ? " -lgcov" : '') .
                        (vmWithBackTrace($self->{oTest}->{&TEST_VM}) && $self->{bBackTrace} ? ' -lbacktrace' : '') .
                    "\n" .
                    "SRCS=" . join(' ', @stryCFile) . "\n" .
                    "OBJS=\$(SRCS:.c=.o)\n" .
                    "\n" .
                    "test: \$(OBJS) test.o\n" .
                    "\t\$(CC) -o test.bin \$(OBJS) test.o"  . ($self->{bProfile} ? " -pg" : '') . " \$(LDFLAGS)\n" .
                    "\n" .
                    "test.o: testflags test.c${strTestDepend}\n" .
                    "\t\$(CC) \$(COMMONFLAGS) \$(WARNINGFLAGS) \$(TESTFLAGS) -c test.c\n";

                # Build C file dependencies
                foreach my $strCFile (@stryCFile)
                {
                    my $bHarnessFile = $strCFile =~ /^test\// ? true : false;

                    buildDependencyTreeSub(
                        $self->{oStorageTest}, $rhDependencyTree, $strCFile, !$bHarnessFile, $self->{strGCovPath}, ['', 'test']);

                    $strMakefile .=
                        "\n" . substr($strCFile, 0, length($strCFile) - 2) . ".o:" .
                        ($bHarnessFile ? " harnessflags" : " buildflags") . " $strCFile";

                    foreach my $strDepend (@{$rhDependencyTree->{$strCFile}{include}})
                    {
                        $strMakefile .=
                            ' ' .
                            ($rhDependencyTree->{$strDepend}{path} ne '' ? $rhDependencyTree->{$strDepend}{path} . '/' : '') .
                            $strDepend;
                    }

                    $strMakefile .=
                        "\n" .
                        "\t\$(CC) \$(COMMONFLAGS) \$(WARNINGFLAGS)" .
                            ($strCFile =~ /^test\// ? " \$(HARNESSFLAGS)" : " \$(BUILDFLAGS)") .
                            " -c $strCFile -o " . substr($strCFile, 0, length($strCFile) - 2) . ".o\n";
                }

                buildPutDiffers($self->{oStorageTest}, $self->{strGCovPath} . "/Makefile", $strMakefile);
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
                ($self->{oTest}->{&TEST_VM} ne VM_NONE  ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                    "gprof $self->{strGCovPath}/test.bin $self->{strGCovPath}/gmon.out > $self->{strGCovPath}/gprof.txt");

            $self->{oStorageTest}->pathCreate("$self->{strBackRestBase}/test/profile", {strMode => '0750', bIgnoreExists => true});
            $self->{oStorageTest}->copy(
                "$self->{strGCovPath}/gprof.txt", "$self->{strBackRestBase}/test/profile/gprof.txt");
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
            my $strLCovConf = $self->{strBackRestBase} . '/test/.vagrant/code/lcov.conf';
            coverageLCovConfigGenerate($self->{oStorageTest}, $strLCovConf, $self->{bCoverageSummary});

            my $strLCovExeBase = "lcov --config-file=${strLCovConf}";
            my $strLCovOut = $self->{strGCovPath} . '/test.lcov';
            my $strLCovOutTmp = $self->{strGCovPath} . '/test.tmp.lcov';

            executeTest(
                ($self->{oTest}->{&TEST_VM} ne VM_NONE  ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                "${strLCovExeBase} --capture --directory=$self->{strGCovPath} --o=${strLCovOut}");

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

                # Disable branch coverage for test files
                my $strLCovExe = $strLCovExeBase;

                if ($bTest)
                {
                    $strLCovExe .= ' --rc lcov_branch_coverage=0';
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

            if ($self->{oTest}->{&TEST_VM} ne VM_NONE)
            {
                containerRemove("test-$self->{iVmIdx}");
            }

            executeTest("rm -rf ${strHostTestPath}");
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
# Install C binary
####################################################################################################################################
sub jobInstallC
{
    my $strBasePath = shift;
    my $strVm = shift;
    my $strImage = shift;

    my $oVm = vmGet();
    my $strBuildPath = "${strBasePath}/test/.vagrant/bin/${strVm}";
    my $strBuildBinPath = "${strBuildPath}/src";

    executeTest(
        "docker exec -i -u root ${strImage} bash -c '" .
        "cp ${strBuildBinPath}/" . PROJECT_EXE . ' /usr/bin/' . PROJECT_EXE . ' && ' .
        'chmod 755 /usr/bin/' . PROJECT_EXE . "'");
}

push(@EXPORT, qw(jobInstallC));

1;
