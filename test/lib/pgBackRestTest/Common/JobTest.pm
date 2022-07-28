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

    # Use the new C test harness?
    $self->{bTestC} = $self->{oTest}->{&TEST_C} && !$self->{bProfile} && $self->{oTest}->{&TEST_TYPE} ne TESTDEF_PERFORMANCE;

    # Setup the path where unit test will be built
    $self->{strUnitPath} =
        "$self->{strTestPath}/" . ($self->{bTestC} ? 'unit' : 'gcov') . "-$self->{iVmIdx}/$self->{oTest}->{&TEST_VM}";
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
                        ($self->{bTestC} ? " -v ${strBuildPath}:${strBuildPath}:ro" : '') .
                        ($self->{bTestC} ? " -v ${strCCachePath}:/home/${\TEST_USER}/.ccache" : '') .
                        ' ' . containerRepo() . ':' . $self->{oTest}->{&TEST_VM} . '-test',
                        {bSuppressStdErr => true});
                }
            }
        }

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
            if ($self->{bTestC})
            {
                # Create command
                # ------------------------------------------------------------------------------------------------------------------
                # Build filename for valgrind suppressions
                my $strValgrindSuppress = $self->{strRepoPath} . '/test/src/valgrind.suppress.' . $self->{oTest}->{&TEST_VM};

                # Is coverage enabled?
                my $bCoverage = vmCoverageC($self->{oTest}->{&TEST_VM}) && $self->{bCoverageUnit};

                $strCommand =
                    ($self->{oTest}->{&TEST_VM} ne VM_NONE ? "docker exec -i -u ${\TEST_USER} ${strImage} bash -l -c '" : '') .
                    " \\\n" .
                    $self->{strTestPath} . '/build/' . $self->{oTest}->{&TEST_VM} . '/test/src/test-pgbackrest --no-run' .
                        ' --no-repo-copy --repo-path=' . $self->{strTestPath} . '/repo' . '  --test-path=' . $self->{strTestPath} .
                        ' --vm=' . $self->{oTest}->{&TEST_VM} . ' --vm-id=' . $self->{iVmIdx} .
                        ($bCoverage ? '' : ' --no-coverage') . ' test ' . $self->{oTest}->{&TEST_MODULE} . '/' .
                        $self->{oTest}->{&TEST_NAME} . " && \\\n" .
                    # Allow stderr to be copied to stderr and stdout
                    "exec 3>&1 && \\\n" .
                    # Test with valgrind when requested
                    ($self->{bValgrindUnit} && $self->{oTest}->{&TEST_TYPE} ne TESTDEF_PERFORMANCE ?
                        'valgrind -q --gen-suppressions=all' .
                        ($self->{oStorageTest}->exists($strValgrindSuppress) ? " --suppressions=${strValgrindSuppress}" : '') .
                        " --exit-on-first-error=yes --leak-check=full --leak-resolution=high --error-exitcode=25" . ' ' : '') .
                        $self->{strUnitPath} . '/build/test-unit 2>&1 1>&3 | tee /dev/stderr' .
                    ($self->{oTest}->{&TEST_VM} ne VM_NONE ? "'" : '');
            }
            # Else still testing with Perl harness
            elsif ($self->{oTest}->{&TEST_C})
            {
                my $strRepoCopyPath = $self->{strTestPath} . '/repo';           # Path to repo copy
                my $strRepoCopySrcPath = $strRepoCopyPath . '/src';             # Path to repo copy src
                my $strRepoCopyTestSrcPath = $strRepoCopyPath . '/test/src';    # Path to repo copy test src
                my $strShimSrcPath = $self->{strUnitPath} . '/src';             # Path to shim src
                my $strShimTestSrcPath = $self->{strUnitPath} . '/test/src';    # Path to shim test src

                my $bCleanAll = false;                              # Do all object files need to be cleaned?
                my $bConfigure = false;                             # Does configure need to be run?

                # If the build.processing file exists then wipe the path to start clean
                # ------------------------------------------------------------------------------------------------------------------
                my $strBuildProcessingFile = $self->{strUnitPath} . "/build.processing";

                # If the file exists then processing terminated before test.bin was run in the last test and the path might be in a
                # bad state.
                if ($self->{oStorageTest}->exists($strBuildProcessingFile))
                {
                    executeTest("find $self->{strUnitPath} -mindepth 1 -print0 | xargs -0 rm -rf");
                }

                # Write build.processing to track processing of this test
                $self->{oStorageTest}->put($strBuildProcessingFile);

                # Create Makefile.in
                # ------------------------------------------------------------------------------------------------------------------
                my $strMakefileIn =
                    "CC_CONFIG = \@CC\@\n" .
                    "CFLAGS_CONFIG = \@CFLAGS\@\n" .
                    "CPPFLAGS_CONFIG = \@CPPFLAGS\@\n" .
                    "LDFLAGS_CONFIG = \@LDFLAGS\@\n" .
                    "LIBS_CONFIG = \@LIBS\@ \@LIBS_BUILD\@\n";

                # If Makefile.in has changed then configure needs to be run and all files cleaned
                if (buildPutDiffers($self->{oStorageTest}, $self->{strUnitPath} . "/Makefile.in", $strMakefileIn))
                {
                    $bConfigure = true;
                    $bCleanAll = true;
                }

                # Create Makefile.param
                # ------------------------------------------------------------------------------------------------------------------
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

                # Generate Makefile.param
                my $strMakefileParam =
                    "CFLAGS =" .
                        " \\\n\t-DERROR_MESSAGE_BUFFER_SIZE=131072" .
                        ($self->{bProfile} ? " \\\n\t-pg" : '') .
                        (vmArchBits($self->{oTest}->{&TEST_VM}) == 32 ? " \\\n\t-D_FILE_OFFSET_BITS=64" : '') .
                        ($self->{bDebug} ? '' : " \\\n\t-DNDEBUG") .
                        ($self->{oTest}->{&TEST_VM} eq VM_RH7 ? " \\\n\t-DDEBUG_EXEC_TIME" : '') .
                        ($bCoverage ? " \\\n\t-DDEBUG_COVERAGE" : '') .
                        ($self->{bDebugTestTrace} && $self->{bDebug} ? " \\\n\t-DDEBUG_TEST_TRACE" : '') .
                        (vmWithBackTrace($self->{oTest}->{&TEST_VM}) && $self->{bBackTrace} ? " \\\n\t-DWITH_BACKTRACE" : '') .
                        ($self->{oTest}->{&TEST_CDEF} ? " \\\n\t$self->{oTest}->{&TEST_CDEF}" : '') .
                        "\n" .
                    "\n" .
                    "CFLAGS_TEST =" .
                        " \\\n\t" . (($self->{bOptimize} && ($self->{bProfile} || $bPerformance)) ? '-O2' : '-O0') .
                        (!$self->{bDebugTestTrace} && $self->{bDebug} ? " \\\n\t-DDEBUG_TEST_TRACE" : '') .
                        ($bCoverage ? " \\\n\t-fprofile-arcs -ftest-coverage" : '') .
                        ($self->{oTest}->{&TEST_VM} eq VM_NONE ? '' : " \\\n\t-DTEST_CONTAINER_REQUIRED") .
                        ($self->{oTest}->{&TEST_CTESTDEF} ? " \\\n\t$self->{oTest}->{&TEST_CTESTDEF}" : '') .
                        "\n" .
                    "\n" .
                    "CFLAGS_HARNESS =" .
                        " \\\n\t" . ($self->{bOptimize} ? '-O2' : '-O0') .
                        ($self->{oTest}->{&TEST_CTESTDEF} ? " \\\n\t$self->{oTest}->{&TEST_CTESTDEF}" : '') .
                        "\n" .
                    "\n" .
                    "CFLAGS_CORE =" .
                        " \\\n\t" . ($self->{bOptimize} ? '-O2' : '-O0') .
                        "\n" .
                    "\n" .
                    "LDFLAGS =" .
                        ($self->{bProfile} ? " \\\n\t-pg" : '') .
                        "\n" .
                    "\n" .
                    "LIBS =" .
                        ($bCoverage ? " \\\n\t-lgcov" : '') .
                        (vmWithBackTrace($self->{oTest}->{&TEST_VM}) && $self->{bBackTrace} ? " \\\n\t-lbacktrace" : '') .
                        "\n" .
                    "\n" .
                    "INCLUDE =" .
                        " \\\n\t-I\"${strShimSrcPath}\"" .
                        " \\\n\t-I\"${strShimTestSrcPath}\"" .
                        " \\\n\t-I\"${strRepoCopySrcPath}\"" .
                        " \\\n\t-I\"${strRepoCopyTestSrcPath}\"" .
                        "\n" .
                    "\n" .
                    "vpath \%.c ${strShimSrcPath}:${strShimTestSrcPath}:${strRepoCopySrcPath}:${strRepoCopyTestSrcPath}\n";

                # If Makefile.param has changed then clean all files
                if (buildPutDiffers($self->{oStorageTest}, $self->{strUnitPath} . "/Makefile.param", $strMakefileParam))
                {
                    $bCleanAll = true;
                }

                # Generate list of harness files
                # ------------------------------------------------------------------------------------------------------------------
                my $hTest = (testDefModuleTest($self->{oTest}->{&TEST_MODULE}, $self->{oTest}->{&TEST_NAME}));
                my $strRepoCopyTestSrcHarnessPath = $strRepoCopyTestSrcPath . '/common';

                # C modules included in harness files that should not be added to the make list
                my $rhHarnessCModule = {};

                # List of harness files to include in make
                my @stryHarnessFile = ('common/harnessTest');

                foreach my $rhHarness (@{$hTest->{&TESTDEF_HARNESS}})
                {
                    my $bFound = false;
                    my $strFile = "common/harness" . ucfirst($rhHarness->{&TESTDEF_HARNESS_NAME});

                    # Include harness file if present
                    my $strHarnessSrcFile = "${strRepoCopyTestSrcPath}/${strFile}.c";

                    if ($self->{oStorageTest}->exists($strHarnessSrcFile))
                    {
                        $bFound = true;

                        if (!defined($hTest->{&TESTDEF_HARNESS_SHIM_DEF}{$rhHarness->{&TESTDEF_HARNESS_NAME}}))
                        {
                            push(@stryHarnessFile, $strFile);
                        }

                        # Install shim
                        my $rhShim = $rhHarness->{&TESTDEF_HARNESS_SHIM};

                        if (defined($rhShim))
                        {
                            my $strHarnessSrc = ${$self->{oStorageTest}->get($strHarnessSrcFile)};

                            # Error if there is no placeholder for the shimmed modules
                            if ($strHarnessSrc !~ /\{\[SHIM\_MODULE\]\}/)
                            {
                                confess &log(ERROR, "{[SHIM_MODULE]} tag not found in '${strFile}' harness with shims");
                            }

                            # Build list of shimmed C modules
                            my $strShimModuleList = undef;

                            foreach my $strShimModule (sort(keys(%{$rhShim})))
                            {
                                # If there are shimmed elements the C module will need to be updated and saved to the test path
                                if (defined($rhShim->{$strShimModule}))
                                {
                                    my $strShimModuleSrc = ${$self->{oStorageTest}->get(
                                        "${strRepoCopySrcPath}/${strShimModule}.c")};
                                    my @stryShimModuleSrcRenamed;
                                    my $strFunctionDeclaration = undef;
                                    my $strFunctionShim = undef;

                                    foreach my $strLine (split("\n", $strShimModuleSrc))
                                    {
                                        # If shimmed function declaration construction is in progress
                                        if (defined($strFunctionShim))
                                        {
                                            # When the beginning of the function block is found, output both the constructed
                                            # declaration and the renamed implementation.
                                            if ($strLine =~ /^{/)
                                            {
                                                push(
                                                    @stryShimModuleSrcRenamed,
                                                    trim($strFunctionDeclaration) . "; " . $strFunctionShim);
                                                push(@stryShimModuleSrcRenamed, $strLine);

                                                $strFunctionShim = undef;
                                            }
                                            # Else keep constructing the declaration and implementation
                                            else
                                            {
                                                $strFunctionDeclaration .= trim($strLine);
                                                $strFunctionShim .= "${strLine}\n";
                                            }
                                        }
                                        # Else search for shimmed functions
                                        else
                                        {
                                            # Rename shimmed functions
                                            my $bFound = false;

                                            foreach my $strFunction (@{$rhShim->{$strShimModule}{&TESTDEF_HARNESS_SHIM_FUNCTION}})
                                            {
                                                # If the function to shim is static then we need to create a declaration with the
                                                # original name so references to the original name in the C module will compile.
                                                # This is not necessary for externed functions since they should already have a
                                                # declaration in the header file.
                                                if ($strLine =~ /^${strFunction}\(/ && $stryShimModuleSrcRenamed[-1] =~ /^static /)
                                                {
                                                    my $strLineLast = pop(@stryShimModuleSrcRenamed);

                                                    $strFunctionDeclaration = "${strLineLast} ${strLine}";

                                                    $strLine =~ s/^${strFunction}\(/${strFunction}_SHIMMED\(/;
                                                    $strFunctionShim = "${strLineLast}\n${strLine}";

                                                    $bFound = true;
                                                    last;
                                                }
                                            }

                                            # If the function was not found then just append the line
                                            if (!$bFound)
                                            {
                                                push(@stryShimModuleSrcRenamed, $strLine);
                                            }
                                        }
                                    }

                                    buildPutDiffers(
                                        $self->{oStorageTest}, "${strShimSrcPath}/${strShimModule}.c",
                                        join("\n", @stryShimModuleSrcRenamed));
                                }

                                # Build list to include in the harness
                                if (defined($strShimModuleList))
                                {
                                    $strShimModuleList .= "\n";
                                }

                                $rhHarnessCModule->{$strShimModule} = true;
                                $strShimModuleList .= "#include \"${strShimModule}.c\"";
                            }

                            # Replace modules and save
                            $strHarnessSrc =~ s/\{\[SHIM\_MODULE\]\}/${strShimModuleList}/g;
                            buildPutDiffers($self->{oStorageTest}, "${strShimTestSrcPath}/${strFile}.c", $strHarnessSrc);
                        }
                    }

                    # Include files in the harness directory if present
                    for my $strFileSub (
                        $self->{oStorageTest}->list("${strRepoCopyTestSrcPath}/${strFile}",
                        {bIgnoreMissing => true, strExpression => '\.c$'}))
                    {
                        push(@stryHarnessFile, "${strFile}/" . substr($strFileSub, 0, length($strFileSub) - 2));
                        $bFound = true;
                    }

                    # Error when no harness files were found
                    if (!$bFound)
                    {
                        confess &log(ERROR, "no files found for harness '$rhHarness->{&TESTDEF_HARNESS_NAME}'");
                    }
                }

                # Generate list of core files (files to be tested/included in this unit will be excluded)
                # ------------------------------------------------------------------------------------------------------------------
                my $hTestCoverage = $hTest->{&TESTDEF_COVERAGE};

                my @stryCoreFile;

                foreach my $strFile (@{$hTest->{&TESTDEF_CORE}})
                {
                    # Skip all files except .c files (including .auto.c and .vendor.c)
                    next if $strFile !~ /(?<!\.auto)$/ || $strFile !~ /(?<!\.vendor)$/;

                    # Skip if no C file exists
                    next if !$self->{oStorageTest}->exists("${strRepoCopySrcPath}/${strFile}.c");

                    # Skip if the C file is included in the harness
                    next if defined($rhHarnessCModule->{$strFile});

                    if (!defined($hTestCoverage->{$strFile}) && !grep(/^$strFile$/, @{$hTest->{&TESTDEF_INCLUDE}}))
                    {
                        push(@stryCoreFile, $strFile);
                    }
                }

                # Create Makefile
                # ------------------------------------------------------------------------------------------------------------------
                my $strMakefile =
                    "include Makefile.config\n" .
                    "include Makefile.param\n" .
                    "\n" .
                    "SRCS = test.c \\\n" .
                    "\t" . join('.c ', @stryHarnessFile) . ".c \\\n" .
                    (@stryCoreFile > 0 ? "\t" . join('.c ', @stryCoreFile) . ".c\n" : '').
                    "\n" .
                    ".build/test.o: CFLAGS += \$(CFLAGS_TEST)\n" .
                    "\n" .
                    ".build/" . join(".o: CFLAGS += \$(CFLAGS_HARNESS)\n.build/", @stryHarnessFile) .
                        ".o: CFLAGS += \$(CFLAGS_HARNESS)\n" .
                    "\n" .
                    ".build/" . join(".o: CFLAGS += \$(CFLAGS_CORE)\n.build/", @stryCoreFile) .
                        ".o: CFLAGS += \$(CFLAGS_CORE)\n" .
                    "\n" .
                    ".build/\%.o : \%.c\n" .
                    "	\@if test ! -d \$(\@D); then mkdir -p \$(\@D); fi\n" .
                    "	\$(CC_CONFIG) \$(INCLUDE) \$(CFLAGS_CONFIG) \$(CPPFLAGS_CONFIG) \$(CFLAGS)" .
                        " -c -o \$\@ \$< -MMD -MP -MF .build/\$*.dep\n" .
                    "\n" .
                    "OBJS = \$(patsubst \%.c,.build/\%.o,\$(SRCS))\n" .
                    "\n" .
                    "test: \$(OBJS)\n" .
                    "	\$(CC_CONFIG) -o test.bin \$(OBJS) \$(LDFLAGS_CONFIG) \$(LDFLAGS) \$(LIBS_CONFIG) \$(LIBS)\n" .
                    "\n" .
                    "rwildcard = \$(wildcard \$1\$2) \$(foreach d,\$(wildcard \$1*),\$(call rwildcard,\$d/,\$2))\n" .
                    "DEP_FILES = \$(call rwildcard,.build,*.dep)\n" .
                    "include \$(DEP_FILES)\n";

                buildPutDiffers($self->{oStorageTest}, $self->{strUnitPath} . "/Makefile", $strMakefile);

                # Create test.c
                # ------------------------------------------------------------------------------------------------------------------
                # Generate list of C files to include for testing
                my $strTestDepend = '';
                my $strTestFile =
                    "module/$self->{oTest}->{&TEST_MODULE}/" . testRunName($self->{oTest}->{&TEST_NAME}, false) . 'Test.c';
                my $strCInclude;

                foreach my $strFile (sort(keys(%{$hTestCoverage}), @{$hTest->{&TESTDEF_INCLUDE}}))
                {
                    # Don't include the test file as it is already included below
                    next if $strFile =~ /Test$/;

                    # Don't include auto files as they are included in their companion C files
                    next if $strFile =~ /auto$/;

                    # Don't include vendor files as they are included in regular C files
                    next if $strFile =~ /vendor$/;

                    # Skip if the C file is included in the harness
                    next if defined($rhHarnessCModule->{$strFile});

                    # Include the C file if it exists
                    my $strCIncludeFile = "${strFile}.c";

                    # If the C file does not exist use the header file instead
                    if (!$self->{oStorageTest}->exists("${strRepoCopySrcPath}/${strCIncludeFile}"))
                    {
                        # Error if code was expected
                        if ($hTestCoverage->{$strFile} ne TESTDEF_COVERAGE_NOCODE)
                        {
                            confess &log(ERROR, "unable to find source file '${strRepoCopySrcPath}/${strCIncludeFile}'");
                        }

                        $strCIncludeFile = "${strFile}.h";
                    }

                    $strCInclude .= (defined($strCInclude) ? "\n" : '') . "#include \"${strCIncludeFile}\"";
                    $strTestDepend .= " ${strCIncludeFile}";
                }

                # Add harnesses with shims that are first defined in this module
                foreach my $strHarness (sort(keys(%{$hTest->{&TESTDEF_HARNESS_SHIM_DEF}})))
                {
                    $strCInclude .=
                        (defined($strCInclude) ? "\n" : '') . "#include \"common/harness" . ucfirst($strHarness) . ".c\"";
                }

                # Update C test file with test module
                my $strTestC = ${$self->{oStorageTest}->get("${strRepoCopyTestSrcPath}/test.c")};

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

                # Determine where the project exe is located
                my $strProjectExePath =
                    "$self->{strTestPath}/bin/$self->{oTest}->{&TEST_VM}/" . ($self->{oTest}->{&TEST_VM} eq VM_NONE ? 'src/' : '') .
                    PROJECT_EXE;

                # Is this test running in a container?
                my $strContainer = $self->{oTest}->{&TEST_VM} eq VM_NONE ? 'false' : 'true';

                # What test path should be passed to C?  Containers always have their test path at ~/test but when running with
                # vm=none it should be in a subdirectory of the current directory.
                my $strTestPathC = $self->{oTest}->{&TEST_VM} eq VM_NONE ? $strHostTestPath : $strVmTestPath;

                # Set globals
                $strTestC =~ s/\{\[C\_TEST\_CONTAINER\]\}/$strContainer/g;
                $strTestC =~ s/\{\[C\_TEST\_PROJECT\_EXE\]\}/$strProjectExePath/g;
                $strTestC =~ s/\{\[C\_TEST\_PATH\]\}/$strTestPathC/g;
                $strTestC =~ s/\{\[C\_TEST\_USER\]\}/${\TEST_USER}/g;
                $strTestC =~ s/\{\[C\_TEST\_USER\_ID\]\}/${\TEST_USER_ID}/g;
                $strTestC =~ s/\{\[C\_TEST\_GROUP\]\}/${\TEST_GROUP}/g;
                $strTestC =~ s/\{\[C\_TEST\_GROUP\_ID\]\}/${\TEST_GROUP_ID}/g;
                $strTestC =~ s/\{\[C\_TEST\_PGB\_PATH\]\}/$strRepoCopyPath/g;
                $strTestC =~ s/\{\[C\_HRN\_PATH\]\}/$self->{strDataPath}/g;
                $strTestC =~ s/\{\[C\_TEST\_IDX\]\}/$self->{iVmIdx}/g;
                $strTestC =~ s/\{\[C\_HRN\_PATH\_REPO\]\}/$self->{strBackRestBase}/g;
                $strTestC =~ s/\{\[C\_TEST\_SCALE\]\}/$self->{iScale}/g;

                my $strLogTimestampC = $self->{bLogTimestamp} ? 'true' : 'false';
                $strTestC =~ s/\{\[C\_TEST\_TIMING\]\}/$strLogTimestampC/g;

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

                # Save test.c and make sure it gets a new timestamp
                # ------------------------------------------------------------------------------------------------------------------
                my $strTestCFile = "$self->{strUnitPath}/test.c";

                if (buildPutDiffers($self->{oStorageTest}, "$self->{strUnitPath}/test.c", $strTestC))
                {
                    # Get timestamp for test.bin if it existss
                    my $oTestBinInfo = $self->{oStorageTest}->info("$self->{strUnitPath}/test.bin", {bIgnoreMissing => true});
                    my $iTestBinOriginalTime = defined($oTestBinInfo) ? $oTestBinInfo->mtime : 0;

                    # Get timestamp for test.c
                    my $iTestCNewTime = $self->{oStorageTest}->info($strTestCFile)->mtime;

                    # Is the timestamp for test.c newer than test.bin?
                    while ($iTestCNewTime <= $iTestBinOriginalTime)
                    {
                        # If not then sleep until the next second
                        my $iTimeToSleep = ($iTestBinOriginalTime + 1) - gettimeofday();

                        if ($iTimeToSleep > 0)
                        {
                            usleep($iTimeToSleep * 1000000);
                        }

                        # Save the file again
                        $self->{oStorageTest}->put($self->{oStorageTest}->openWrite($strTestCFile), $strTestC);
                        $iTestCNewTime = $self->{oStorageTest}->info($strTestCFile)->mtime;
                    }
                }

                # Create command
                # ------------------------------------------------------------------------------------------------------------------
                # Build filename for valgrind suppressions
                my $strValgrindSuppress = $self->{strRepoPath} . '/test/src/valgrind.suppress.' . $self->{oTest}->{&TEST_VM};

                $strCommand =
                    ($self->{oTest}->{&TEST_VM} ne VM_NONE ? "docker exec -i -u ${\TEST_USER} ${strImage} bash -l -c '" : '') .
                    " \\\n" .
                    "cd $self->{strUnitPath} && \\\n" .
                    # Clean build
                    ($bCleanAll ? "rm -rf .build && \\\n" : '') .
                    # Remove coverage data
                    (!$bCleanAll && $bCoverage ? "rm -rf .build/test.gcda && \\\n" : '') .
                    # Configure when required
                    ($bConfigure ?
                        "mv Makefile Makefile.tmp && ${strRepoCopySrcPath}/configure -q --enable-test" .
                            " && mv Makefile Makefile.config && mv Makefile.tmp Makefile && \\\n" :
                        '') .
                    $self->{strMakeCmd} . " -j $self->{iBuildMax} -s 2>&1 && \\\n" .
                    "rm ${strBuildProcessingFile} && \\\n" .
                    # Allow stderr to be copied to stderr and stdout
                    "exec 3>&1 && \\\n" .
                    # Test with valgrind when requested
                    ($self->{bValgrindUnit} && $self->{oTest}->{&TEST_TYPE} ne TESTDEF_PERFORMANCE ?
                        'valgrind -q --gen-suppressions=all' .
                        ($self->{oStorageTest}->exists($strValgrindSuppress) ? " --suppressions=${strValgrindSuppress}" : '') .
                        " --exit-on-first-error=yes --leak-check=full --leak-resolution=high --error-exitcode=25" . ' ' : '') .
                        "./test.bin 2>&1 1>&3 | tee /dev/stderr" .
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
                    "gprof $self->{strUnitPath}/test.bin $self->{strUnitPath}/gmon.out > $self->{strUnitPath}/gprof.txt");

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
