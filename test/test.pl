#!/usr/bin/perl
####################################################################################################################################
# test.pl - pgBackRest Unit Tests
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

use Digest::SHA qw(sha1_hex);
use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions);
use Cwd qw(abs_path cwd);
use JSON::PP;
use Pod::Usage qw(pod2usage);
use POSIX qw(ceil strftime);
use Time::HiRes qw(gettimeofday);

use lib dirname($0) . '/lib';
use lib dirname(dirname($0)) . '/lib';
use lib dirname(dirname($0)) . '/build/lib';
use lib dirname(dirname($0)) . '/doc/lib';

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::Common::String;
use pgBackRestDoc::ProjectInfo;

use pgBackRestBuild::Build;
use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;
use pgBackRestBuild::Config::BuildHelp;
use pgBackRestBuild::Config::BuildParse;
use pgBackRestBuild::Error::Build;
use pgBackRestBuild::Error::Data;

use pgBackRestTest::Common::BuildTest;
use pgBackRestTest::Common::CodeCountTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::CoverageTest;
use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::JobTest;
use pgBackRestTest::Common::ListTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::Storage;
use pgBackRestTest::Common::StoragePosix;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

test.pl - pgBackRest Unit Tests

=head1 SYNOPSIS

test.pl [options]

 Test Options:
   --module             test module to execute
   --test               execute the specified test in a module
   --run                execute only the specified test run
   --dry-run            show only the tests that would be executed but don't execute them
   --no-cleanup         don't cleanup after the last test is complete - useful for debugging
   --clean              clean working and result paths for a completely fresh build
   --clean-only         execute --clean and exit
   --pg-version         version of postgres to test (all, defaults to minimal)
   --log-force          force overwrite of current test log files
   --build-only         build the binary (and honor --build-package) but don't run tests
   --build-package      build the package
   --build-max          max processes to use for builds (default 4)
   --coverage-only      only run coverage tests (as a subset of selected tests)
   --c-only             only run C tests
   --container-only     only run tests that must be run in a container
   --no-performance     do not run performance tests
   --gen-only           only run auto-generation
   --no-gen             do not run code generation
   --code-count         generate code counts
   --smart              perform bin/package builds only when source timestamps have changed
   --dev                --smart --no-optimize
   --dev-test           does nothing -- kept for backward compatibility
   --expect             --vm=co7 --pg-version=9.6 --log-force
   --no-valgrind        don't run valgrind on C unit tests (saves time)
   --no-coverage        don't run coverage on C unit tests (saves time)
   --no-coverage-report run coverage but don't generate coverage report (saves time)
   --no-optimize        don't do compile optimization for C (saves compile time)
   --backtrace          enable backtrace when available (adds stack trace line numbers -- very slow)
   --profile            generate profile info
   --no-debug           don't generate a debug build
   --scale              scale performance tests
   --tz                 test with the specified timezone
   --debug-test-trace   test stack trace for low-level functions (slow, esp w/valgrind, may cause timeouts)

 Report Options:
   --coverage-summary   generate a coverage summary report for the documentation

 Configuration Options:
   --psql-bin           path to the psql executables (e.g. /usr/lib/postgresql/9.3/bin/)
   --test-path          path where tests are executed (defaults to ./test)
   --log-level          log level to use for test harness (and Perl tests) (defaults to INFO)
   --log-level-test     log level to use for C tests (defaults to OFF)
   --log-level-test-file log level to use for file logging in integration tests (defaults to TRACE)
   --no-log-timestamp   suppress timestamps, timings, etc. Used to generate documentation.
   --quiet, -q          equivalent to --log-level=off

 VM Options:
   --vm                 docker container to build/test (e.g. co7)
   --vm-build           build Docker containers
   --vm-force           force a rebuild of Docker containers
   --vm-out             Show VM output (default false)
   --vm-max             max VMs to run in parallel (default 1)

 General Options:
   --version            display version and exit
   --help               display usage and exit
=cut

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $bClean;
my $bCleanOnly;
my $strLogLevel = lc(INFO);
my $strLogLevelTest = lc(OFF);
my $strLogLevelTestFile = lc(TRACE);
my $bNoLogTimestamp = false;
my $bVmOut = false;
my @stryModule;
my @stryModuleTest;
my @iyModuleTestRun;
my $iVmMax = 1;
my $iVmId = undef;
my $bDryRun = false;
my $bNoCleanup = false;
my $strPgSqlBin;
my $strTestPath;
my $bVersion = false;
my $bHelp = false;
my $bQuiet = false;
my $strPgVersion = 'minimal';
my $bLogForce = false;
my $strVm;
my $strVmHost = VM_HOST_DEFAULT;
my $bVmBuild = false;
my $bVmForce = false;
my $bBuildOnly = false;
my $bBuildPackage = false;
my $iBuildMax = 4;
my $bCoverageOnly = false;
my $bCoverageSummary = false;
my $bNoCoverage = false;
my $bNoCoverageReport = false;
my $bCOnly = false;
my $bContainerOnly = false;
my $bNoPerformance = false;
my $bGenOnly = false;
my $bNoGen = false;
my $bCodeCount = false;
my $bSmart = false;
my $bDev = false;
my $bDevTest = false;
my $bBackTrace = false;
my $bProfile = false;
my $bExpect = false;
my $bNoValgrind = false;
my $bNoOptimize = false;
my $bNoDebug = false;
my $iScale = 1;
my $bDebugTestTrace = false;
my $iRetry = 0;
my $strTimeZone = undef;

my @cmdOptions = @ARGV;

GetOptions ('q|quiet' => \$bQuiet,
            'version' => \$bVersion,
            'help' => \$bHelp,
            'clean' => \$bClean,
            'clean-only' => \$bCleanOnly,
            'pgsql-bin=s' => \$strPgSqlBin,
            'test-path=s' => \$strTestPath,
            'log-level=s' => \$strLogLevel,
            'log-level-test=s' => \$strLogLevelTest,
            'log-level-test-file=s' => \$strLogLevelTestFile,
            'no-log-timestamp' => \$bNoLogTimestamp,
            'vm=s' => \$strVm,
            'vm-host=s' => \$strVmHost,
            'vm-out' => \$bVmOut,
            'vm-build' => \$bVmBuild,
            'vm-force' => \$bVmForce,
            'module=s@' => \@stryModule,
            'test=s@' => \@stryModuleTest,
            'run=s@' => \@iyModuleTestRun,
            'vm-id=s' => \$iVmId,
            'vm-max=s' => \$iVmMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'pg-version=s' => \$strPgVersion,
            'log-force' => \$bLogForce,
            'build-only' => \$bBuildOnly,
            'build-package' => \$bBuildPackage,
            'build-max=s' => \$iBuildMax,
            'coverage-only' => \$bCoverageOnly,
            'coverage-summary' => \$bCoverageSummary,
            'no-coverage' => \$bNoCoverage,
            'no-coverage-report' => \$bNoCoverageReport,
            'c-only' => \$bCOnly,
            'container-only' => \$bContainerOnly,
            'no-performance' => \$bNoPerformance,
            'gen-only' => \$bGenOnly,
            'no-gen' => \$bNoGen,
            'code-count' => \$bCodeCount,
            'smart' => \$bSmart,
            'dev' => \$bDev,
            'dev-test' => \$bDevTest,
            'backtrace' => \$bBackTrace,
            'profile' => \$bProfile,
            'expect' => \$bExpect,
            'no-valgrind' => \$bNoValgrind,
            'no-optimize' => \$bNoOptimize,
            'no-debug', => \$bNoDebug,
            'scale=s' => \$iScale,
            'tz=s', => \$strTimeZone,
            'debug-test-trace', => \$bDebugTestTrace,
            'retry=s' => \$iRetry)
    or pod2usage(2);

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
    # Record the start time
    my $lStartTime = time();

    # Display version and exit if requested
    if ($bVersion || $bHelp)
    {
        syswrite(*STDOUT, PROJECT_NAME . ' ' . PROJECT_VERSION . " Test Engine\n");

        if ($bHelp)
        {
            syswrite(*STDOUT, "\n");
            pod2usage();
        }

        exit 0;
    }

    if (@ARGV > 0)
    {
        syswrite(*STDOUT, "invalid parameter\n\n");
        pod2usage();
    }

    ################################################################################################################################
    # Disable code generation on dry-run
    ################################################################################################################################
    if ($bDryRun)
    {
        $bNoGen = true;
    }

    ################################################################################################################################
    # Update options for --coverage-summary
    ################################################################################################################################
    if ($bCoverageSummary)
    {
        $bCoverageOnly = true;
        $bCOnly = true;
    }

    ################################################################################################################################
    # Update options for --dev and --dev-fast and --dev-test
    ################################################################################################################################
    if ($bDev && $bDevTest)
    {
        confess "cannot combine --dev and --dev-test";
    }

    if ($bDev)
    {
        $bSmart = true;
        $bNoOptimize = true;
    }

    ################################################################################################################################
    # Update options for --profile
    ################################################################################################################################
    if ($bProfile)
    {
        $bNoValgrind = true;
        $bNoCoverage = true;
    }

    ################################################################################################################################
    # Update options for --expect
    ################################################################################################################################
    if ($bExpect)
    {
        $strVm = VM_EXPECT;
        $strPgVersion = '9.6';
        $bLogForce = true;
    }

    ################################################################################################################################
    # Setup
    ################################################################################################################################
    # Set a neutral umask so tests work as expected
    umask(0);

    # Set console log level
    if ($bQuiet)
    {
        $strLogLevel = 'off';
    }

    logLevelSet(uc($strLogLevel), uc($strLogLevel), OFF, !$bNoLogTimestamp);
    &log(INFO, "test begin - log level ${strLogLevel}");

    if (@stryModuleTest != 0 && @stryModule != 1)
    {
        confess "Only one --module can be provided when --test is specified";
    }

    if (@iyModuleTestRun != 0 && @stryModuleTest != 1)
    {
        confess "Only one --test can be provided when --run is specified";
    }

    # Set test path if not explicitly set
    if (!defined($strTestPath))
    {
        $strTestPath = cwd() . '/test';
    }

    my $oStorageTest = new pgBackRestTest::Common::Storage(
        $strTestPath, new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    if ($bCoverageOnly)
    {
        if (!defined($strVm))
        {
            &log(INFO, "Set --vm=${strVmHost} for coverage testing");
            $strVm = $strVmHost;
        }
        elsif ($strVm eq VM_ALL)
        {
            confess &log(ERROR, "select a single Debian-based VM for coverage testing");
        }
    }

    # If VM is not defined then set it to all
    if (!defined($strVm))
    {
        $strVm = VM_ALL;
    }
    # Else make sure vm is valid
    elsif ($strVm ne VM_ALL)
    {
        vmValid($strVm);
    }

    # Get the base backrest path
    my $strBackRestBase = dirname(dirname(abs_path($0)));
    my $strVagrantPath = "${strBackRestBase}/test/.vagrant";

    my $oStorageBackRest = new pgBackRestTest::Common::Storage(
        $strBackRestBase, new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    ################################################################################################################################
    # Clean working and result paths
    ################################################################################################################################
    if ($bClean || $bCleanOnly)
    {
        &log(INFO, "clean working (${strTestPath}) and result (${strBackRestBase}/test/result) paths");

        if ($oStorageTest->pathExists($strTestPath))
        {
            executeTest("find ${strTestPath} -mindepth 1 -print0 | xargs -0 rm -rf");
        }

        if ($oStorageTest->pathExists("${strBackRestBase}/test/result"))
        {
            executeTest("find ${strBackRestBase}/test/result -mindepth 1 -print0 | xargs -0 rm -rf");
        }

        # Exit when clean-only
        exit 0 if $bCleanOnly;
    }

    ################################################################################################################################
    # Build Docker containers
    ################################################################################################################################
    if ($bVmBuild)
    {
        containerBuild($oStorageBackRest, $strVm, $bVmForce);
        exit 0;
    }

    ################################################################################################################################
    # Load test definition
    ################################################################################################################################
    testDefLoad(${$oStorageBackRest->get("test/define.yaml")});

    ################################################################################################################################
    # Start VM and run
    ################################################################################################################################
    if (!defined($iVmId))
    {
        # Make a copy of the repo to track which files have been changed.  Eventually all builds will be done from this directory.
        #---------------------------------------------------------------------------------------------------------------------------
        my $strRepoCachePath = "${strTestPath}/repo";
        my $strRepoCacheManifest = 'repo.manifest';

        # Create the repo path -- this should hopefully prevent obvious rsync errors below
        $oStorageTest->pathCreate("${strTestPath}/repo", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

        # Check if there are any files existing already.  If none, that means a full copy is happening and we shouldn't report
        # modified files
        my @stryExistingList = $oStorageTest->list($strRepoCachePath, {bIgnoreMissing => true});

        # First check if there is an old manifest that has not been cleared.  This indicates that an error happened before all new
        # files could be processed, which means they should be processed again.
        my @stryModifiedList;
        my $rstrModifiedList = $oStorageTest->get(
            $oStorageTest->openRead("${strRepoCachePath}/${strRepoCacheManifest}", {bIgnoreMissing => true}));

        if (defined($rstrModifiedList))
        {
            @stryModifiedList = split("\n", trim($$rstrModifiedList));
        }

        push(
            @stryModifiedList,
            split(
                "\n",
                trim(
                    executeTest(
                        "git -C ${strBackRestBase} ls-files -c --others --exclude-standard |" .
                            " rsync -rtW --out-format=\"\%n\" --delete --ignore-missing-args" .
                            " --exclude=test/result --exclude=repo.manifest" .
                            " ${strBackRestBase}/ --files-from=- ${strRepoCachePath}" .
                            " | grep -E -v '/\$' | cat"))));

        if (@stryModifiedList > 0)
        {
            $oStorageTest->put("${strRepoCachePath}/${strRepoCacheManifest}", join("\n", @stryModifiedList));

            if (@stryExistingList > 0)
            {
                &log(INFO, "modified since last run: " . join(', ', @stryModifiedList));
            }
        }

        # Generate code counts
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bCodeCount)
        {
            &log(INFO, "classify code files");

            codeCountScan($oStorageBackRest, $strBackRestBase);
            exit 0;
        }

        # Auto-generate files unless --no-gen specified
        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bNoGen)
        {
            my @stryBuiltAll;
            &log(INFO, "check code autogenerate");

            # Auto-generate version for configure.ac script
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bSmart || grep(/^src\/version\.h/, @stryModifiedList))
            {
                my $strConfigureAcOld = ${$oStorageTest->get("${strBackRestBase}/src/build/configure.ac")};
                my $strConfigureAcNew;

                foreach my $strLine (split("\n", $strConfigureAcOld))
                {
                    if ($strLine =~ /^AC_INIT\(/)
                    {
                        $strLine = 'AC_INIT([' . PROJECT_NAME . '], [' . PROJECT_VERSION . '])';
                    }

                    $strConfigureAcNew .= "${strLine}\n";
                }

                # Save into the src dir
                my @stryBuilt;
                my $strBuilt = 'src/build/configure.ac';

                if (buildPutDiffers($oStorageBackRest, "${strBackRestBase}/${strBuilt}", $strConfigureAcNew))
                {
                    push(@stryBuilt, $strBuilt);
                    push(@stryBuiltAll, @stryBuilt);
                    push(@stryModifiedList, @stryBuilt);
                }

                &log(INFO,
                    "    autogenerated version in configure.ac script: " . (@stryBuilt ? join(', ', @stryBuilt) : 'no changes'));
            }

            # Auto-generate configure script
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bSmart || grep(/^src\/build\/configure\.ac/, @stryModifiedList))
            {
                # Set build file
                my @stryBuilt;
                my $strBuilt = 'src/configure';

                # Get configure.ac and configure to see if anything has changed
                my $strConfigureAc = ${$oStorageBackRest->get('src/build/configure.ac')};
                my $strConfigureAcHash = sha1_hex($strConfigureAc);
                my $rstrConfigure = $oStorageBackRest->get($oStorageBackRest->openRead($strBuilt, {bIgnoreMissing => true}));

                # Check if configure needs to be regenerated
                if (!defined($rstrConfigure) || !defined($$rstrConfigure) ||
                    $strConfigureAcHash ne substr($$rstrConfigure, length($$rstrConfigure) - 41, 40))
                {
                    # Generate aclocal.m4
                    my $strAcLocal = executeTest("cd ${strBackRestBase}/src/build && aclocal --OUT=-");
                    $strAcLocal = trim($strAcLocal) . "\n";

                    buildPutDiffers($oStorageBackRest, "${strBackRestBase}/src/build/aclocal.m4", $strAcLocal);

                    # Generate configure
                    my $strConfigure = executeTest("cd ${strBackRestBase}/src/build && autoconf --output=-");
                    $strConfigure =
                        trim($strConfigure) . "\n\n# Generated from src/build/configure.ac sha1 ${strConfigureAcHash}\n";

                    # Remove unused options from help
                    $strConfigure =~ s/^  --((?!bin).)*dir=DIR.*\n//mg;
                    $strConfigure =~ s/^  --sbindir=DIR.*\n//mg;

                    # Save into the src dir
                    $oStorageBackRest->put(
                        $oStorageBackRest->openWrite("${strBackRestBase}/${strBuilt}", {strMode => '0755'}), $strConfigure);

                    # Add to built list
                    push(@stryBuilt, $strBuilt);
                    push(@stryBuiltAll, @stryBuilt);
                    push(@stryModifiedList, @stryBuilt);
                }

                &log(INFO, "    autogenerated configure script: " . (@stryBuilt ? join(', ', @stryBuilt) : 'no changes'));
            }

            # Auto-generate C files
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bSmart || grep(/^build\//, @stryModifiedList) || grep(/^doc\/xml\/reference\.xml/, @stryModifiedList))
            {
                errorDefineLoad(${$oStorageBackRest->get("build/error.yaml")});

                my $rhBuild =
                {
                    'config' =>
                    {
                        &BLD_DATA => buildConfig(),
                        &BLD_PATH => 'config',
                    },

                    'configHelp' =>
                    {
                        &BLD_DATA => buildConfigHelp(),
                        &BLD_PATH => 'command/help',
                    },

                    'configParse' =>
                    {
                        &BLD_DATA => buildConfigParse(),
                        &BLD_PATH => 'config',
                    },

                    'error' =>
                    {
                        &BLD_DATA => buildError(),
                        &BLD_PATH => 'common',
                    },
                };

                my @stryBuilt = buildAll("${strBackRestBase}/src", $rhBuild);
                &log(INFO, "    autogenerated C code: " . (@stryBuilt ? join(', ', @stryBuilt) : 'no changes'));

                if (@stryBuilt)
                {
                    push(@stryBuiltAll, @stryBuilt);
                    push(@stryModifiedList, @stryBuilt);
                }
            }

            # Copy the files that were auto-generated to the repo cache so they will be included in the current build
            #-----------------------------------------------------------------------------------------------------------------------
            foreach my $strBuilt (@stryBuiltAll)
            {
                executeTest("cp -p ${strBackRestBase}/${strBuilt} ${strRepoCachePath}/${strBuilt}");
            }

            if ($bGenOnly)
            {
                exit 0;
            }
        }

        # Clean up
        #---------------------------------------------------------------------------------------------------------------------------
        my $iTestFail = 0;
        my $iTestRetry = 0;
        my $oyProcess = [];
        my $strCodePath = "${strBackRestBase}/test/result/coverage/raw";

        if (!$bDryRun || $bVmOut)
        {
            &log(INFO, "cleanup old data" . ($strVm ne VM_NONE ? " and containers" : ''));

            if ($strVm ne VM_NONE)
            {
                containerRemove('test-([0-9]+|build)');
            }

            for (my $iVmIdx = 0; $iVmIdx < 8; $iVmIdx++)
            {
                push(@{$oyProcess}, undef);
            }

            executeTest(
                "rm -rf ${strTestPath}/temp ${strTestPath}/test-* ${strTestPath}/data-*" . ($bDev ? '' : " ${strTestPath}/gcov-*"));
            $oStorageTest->pathCreate("${strTestPath}/temp", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

            # Remove old lcov dirs -- do it this way so the dirs stay open in finder/explorer, etc.
            executeTest("rm -rf ${strBackRestBase}/test/result/coverage/lcov/*");

            # Overwrite the C coverage report so it will load but not show old coverage
            $oStorageTest->pathCreate(
                "${strBackRestBase}/test/result/coverage", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});
            $oStorageBackRest->put(
                "${strBackRestBase}/test/result/coverage/coverage.html", "<center>[ Generating New Report ]</center>");

            # Copy C code for coverage tests
            if (vmCoverageC($strVm) && !$bDryRun)
            {
                executeTest("rm -rf ${strBackRestBase}/test/result/coverage/raw/*");
                $oStorageTest->pathCreate("${strCodePath}", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});
            }
        }

        # Determine which tests to run
        #---------------------------------------------------------------------------------------------------------------------------
        my $oyTestRun;
        my $bBinRequired = $bBuildOnly;
        my $bHostBinRequired = $bBuildOnly;

        # Only get the test list when they can run
        if (!$bBuildOnly)
        {
            # Get the test list
            $oyTestRun = testListGet(
                $strVm, \@stryModule, \@stryModuleTest, \@iyModuleTestRun, $strPgVersion, $bCoverageOnly, $bCOnly, $bContainerOnly,
                $bNoPerformance);

            # Determine if the C binary and test library need to be built
            foreach my $hTest (@{$oyTestRun})
            {
                # Bin build required for all Perl tests or if a C unit test calls Perl
                if (!$hTest->{&TEST_C} || $hTest->{&TEST_BIN_REQ})
                {
                    $bBinRequired = true;
                }

                # Host bin required if a Perl test
                if (!$hTest->{&TEST_C})
                {
                    $bHostBinRequired = true;
                }
            }
        }

        my $strBuildRequired;

        if ($bBinRequired || $bHostBinRequired)
        {
            if ($bBinRequired)
            {
                $strBuildRequired = "bin";
            }

            if ($bHostBinRequired)
            {
                $strBuildRequired .= ", bin host";
            }
        }
        else
        {
            $strBuildRequired = "none";
        }

        &log(INFO, "builds required: ${strBuildRequired}");

        # Build the binary and packages
        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDryRun)
        {
            my $oVm = vmGet();
            my $lTimestampLast;
            my $strBinPath = "${strTestPath}/bin";
            my $rhBinBuild = {};

            # Build the binary
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bBinRequired)
            {
                # Find the lastest modified time for dirs that affect the bin build
                $lTimestampLast = buildLastModTime($oStorageBackRest, $strBackRestBase, ['src']);

                # Loop through VMs to do the C bin builds
                my $bLogDetail = $strLogLevel eq 'detail';
                my @stryBuildVm = $strVm eq VM_ALL ? VM_LIST : ($strVm);

                # Build binary for the host
                if ($bHostBinRequired)
                {
                    push(@stryBuildVm, VM_NONE);
                }

                foreach my $strBuildVM (@stryBuildVm)
                {
                    my $strBuildPath = "${strBinPath}/${strBuildVM}";
                    my $bRebuild = !$bSmart;
                    $rhBinBuild->{$strBuildVM} = true;

                    # Build configure/compile options and see if they have changed from the previous build
                    my $strCFlags =
                        "-Wfatal-errors -g" .
                        (vmWithBackTrace($strBuildVM) && $bBackTrace ? ' -DWITH_BACKTRACE' : '') .
                        ($bDebugTestTrace ? ' -DDEBUG_TEST_TRACE' : '');
                    my $strLdFlags = vmWithBackTrace($strBuildVM) && $bBackTrace  ? '-lbacktrace' : '';
                    my $strConfigOptions = (vmDebugIntegration($strBuildVM) ? ' --enable-test' : '');
                    my $strBuildFlags = "CFLAGS=${strCFlags}\nLDFLAGS=${strLdFlags}\nCONFIGURE=${strConfigOptions}";
                    my $strBuildFlagFile = "${strBinPath}/${strBuildVM}/build.flags";

                    my $bBuildOptionsDiffer = buildPutDiffers($oStorageBackRest, $strBuildFlagFile, $strBuildFlags);
                    $bBuildOptionsDiffer |= grep(/^src\/configure|src\/Makefile.in|src\/build\.auto\.h\.in$/, @stryModifiedList);

                    # Rebuild if the modification time of the bin file is less than the last changes in source paths
                    my $strBinSmart = "${strBuildPath}/pgbackrest";

                    if ($bBuildOptionsDiffer ||
                        ($bSmart &&
                         (!$oStorageBackRest->exists($strBinSmart) ||
                          $oStorageBackRest->info($strBinSmart)->mtime < $lTimestampLast)))
                    {
                        &log(
                            INFO, "    bin dependencies have changed for ${strBuildVM}, " . ($bBuildOptionsDiffer ? 're' : '') .
                            'building...');

                        $bRebuild = true;
                    }

                    if ($bRebuild)
                    {
                        &log(INFO, "    build bin for ${strBuildVM} (${strBuildPath})");

                        if ($strBuildVM ne VM_NONE)
                        {
                            executeTest(
                                "docker run -itd -h test-build --name=test-build" .
                                    " -v ${strBackRestBase}:${strBackRestBase} -v ${strTestPath}:${strTestPath} " .
                                    containerRepo() . ":${strBuildVM}-test",
                                {bSuppressStdErr => true});
                        }

                        if (!$bSmart || $bBuildOptionsDiffer || !$oStorageBackRest->exists("${strBuildPath}/Makefile"))
                        {
                            # Remove old path if it exists and save the build flags
                            executeTest("rm -rf ${strBuildPath}");
                            buildPutDiffers($oStorageBackRest, $strBuildFlagFile, $strBuildFlags);

                            executeTest(
                                ($strBuildVM ne VM_NONE ? 'docker exec -i -u ' . TEST_USER . ' test-build ' : '') .
                                "bash -c 'cd ${strBuildPath} && ${strBackRestBase}/src/configure${strConfigOptions}'",
                                {bShowOutputAsync => $bLogDetail});
                        }

                        executeTest(
                            ($strBuildVM ne VM_NONE ? 'docker exec -i -u ' . TEST_USER . ' test-build ' : '') .
                            "make -j ${iBuildMax}" . ($bLogDetail ? '' : ' --silent') .
                                " --directory ${strBuildPath} CFLAGS='${strCFlags}' LDFLAGS='${strLdFlags}'",
                            {bShowOutputAsync => $bLogDetail});

                        if ($strBuildVM ne VM_NONE)
                        {
                            executeTest("docker rm -f test-build");
                        }
                    }
                }
            }

            # Build the package
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bBuildPackage && $strVm ne VM_NONE)
            {
                my $strPackagePath = "${strBackRestBase}/test/result/package";

                # Loop through VMs to do the package builds
                my @stryBuildVm = $strVm eq VM_ALL ? VM_LIST : ($strVm);
                $oStorageBackRest->pathCreate($strPackagePath, {bIgnoreExists => true, bCreateParent => true});

                foreach my $strBuildVM (@stryBuildVm)
                {
                    my $strBuildPath = "${strPackagePath}/${strBuildVM}";

                    if ($oVm->{$strBuildVM}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
                    {
                        &log(INFO, "build package for ${strBuildVM} (${strBuildPath})");

                        if ($strVm ne VM_NONE)
                        {
                            executeTest(
                                "docker run -itd -h test-build --name=test-build" .
                                " -v ${strBackRestBase}:${strBackRestBase} " . containerRepo() . ":${strBuildVM}-test",
                                {bSuppressStdErr => true});
                        }

                        $oStorageBackRest->pathCreate($strBuildPath, {bIgnoreExists => true, bCreateParent => true});

                        # Clone a copy of the debian package repo
                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                            "bash -c 'git clone https://salsa.debian.org/postgresql/pgbackrest.git /root/package-src 2>&1'");

                        executeTest(
                            "rsync -r --exclude=.vagrant --exclude=.git --exclude=test/result ${strBackRestBase}/" .
                                " ${strBuildPath}/");
                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                            "bash -c 'cp -r /root/package-src/debian ${strBuildPath} && sudo chown -R " . TEST_USER .
                            " ${strBuildPath}'");

                        # Patch files in debian package builds
                        #
                        # Use these commands to create a new patch (may need to modify first line):
                        # BRDIR=/backrest;BRVM=u18;BRPATCHFILE=${BRDIR?}/test/patch/debian-package.patch
                        # DBDIR=${BRDIR?}/test/result/package/${BRVM}/debian
                        # diff -Naur ${DBDIR?}.old ${DBDIR}.new > ${BRPATCHFILE?}
                        my $strDebianPackagePatch = "${strBackRestBase}/test/patch/debian-package.patch";

                        if ($oStorageBackRest->exists($strDebianPackagePatch))
                        {
                            executeTest("cp -r ${strBuildPath}/debian ${strBuildPath}/debian.old");
                            executeTest("patch -d ${strBuildPath}/debian < ${strDebianPackagePatch}");
                            executeTest("cp -r ${strBuildPath}/debian ${strBuildPath}/debian.new");
                        }

                        # If dev build then disable static release date used for reproducibility
                        my $bVersionDev = PROJECT_VERSION =~ /dev$/;

                        if ($bVersionDev)
                        {
                            my $strRules = ${$oStorageBackRest->get("${strBuildPath}/debian/rules")};

                            $strRules =~ s/\-\-var\=release-date-static\=y/\-\-var\=release-date-static\=n/g;
                            $strRules =~ s/\-\-out\=html \-\-cache\-only/\-\-out\=html \-\-no\-exe/g;

                            $oStorageBackRest->put("${strBuildPath}/debian/rules", $strRules);
                        }

                        # Remove patches that should be applied to core code
                        $oStorageBackRest->remove("${strBuildPath}/debian/patches", {bRecurse => true, bIgnoreExists => true});

                        # Update changelog to add experimental version
                        $oStorageBackRest->put("${strBuildPath}/debian/changelog",
                            "pgbackrest (${\PROJECT_VERSION}-0." . ($bVersionDev ? 'D' : 'P') . strftime("%Y%m%d%H%M%S", gmtime) .
                                ") experimental; urgency=medium\n" .
                            "\n" .
                            '  * Automated experimental ' . ($bVersionDev ? 'development' : 'production') . " build.\n" .
                            "\n" .
                            ' -- David Steele <david@pgbackrest.org>  ' . strftime("%a, %e %b %Y %H:%M:%S %z", gmtime) . "\n\n" .
                            ${$oStorageBackRest->get("${strBuildPath}/debian/changelog")});

                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                            "bash -c 'cd ${strBuildPath} && debuild -i -us -uc -b'");

                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                            "bash -c 'rm -f ${strPackagePath}/${strBuildVM}/*.build ${strPackagePath}/${strBuildVM}/*.changes" .
                            " ${strPackagePath}/${strBuildVM}/pgbackrest-doc*'");

                        if ($strVm ne VM_NONE)
                        {
                            executeTest("docker rm -f test-build");
                        }
                    }

                    if ($oVm->{$strBuildVM}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
                    {
                        &log(INFO, "build package for ${strBuildVM} (${strBuildPath})");

                        # Create build container
                        if ($strVm ne VM_NONE)
                        {
                            executeTest(
                                "docker run -itd -h test-build --name=test-build" .
                                " -v ${strBackRestBase}:${strBackRestBase} " . containerRepo() . ":${strBuildVM}-test",
                                {bSuppressStdErr => true});
                        }

                        # Fetching specific files is fragile but even a shallow clone of the entire pgrpms repo is very expensive.
                        # Using 'git archive' does not seem to work: access denied or repository not exported: /git/pgrpms.git.
                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                            "bash -c \"" .
                            "mkdir /root/package-src && " .
                            "wget -q -O /root/package-src/pgbackrest-conf.patch " .
                                "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;hb=refs/heads/master;" .
                                "f=rpm/redhat/master/common/pgbackrest/master/pgbackrest-conf.patch' && " .
                            "wget -q -O /root/package-src/pgbackrest.logrotate " .
                                "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;hb=refs/heads/master;" .
                                "f=rpm/redhat/master/common/pgbackrest/master/pgbackrest.logrotate' && " .
                            "wget -q -O /root/package-src/pgbackrest.spec " .
                                "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;hb=refs/heads/master;" .
                                "f=rpm/redhat/master/common/pgbackrest/master/pgbackrest.spec'\"");

                        # Create build directories
                        $oStorageBackRest->pathCreate($strBuildPath, {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/SOURCES", {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/SPECS", {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/RPMS", {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/BUILD", {bIgnoreExists => true, bCreateParent => true});

                        # Install PostreSQL 11 development for package builds
                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                            "bash -c 'yum install -y postgresql11-devel 2>&1'");

                        # Copy source files
                        executeTest(
                            "tar --transform='s_^_pgbackrest-release-${\PROJECT_VERSION}/_'" .
                                " -czf ${strBuildPath}/SOURCES/${\PROJECT_VERSION}.tar.gz -C ${strBackRestBase}" .
                                " src LICENSE");

                        # Copy package files
                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') . "bash -c '" .
                            "ln -s ${strBuildPath} /root/rpmbuild && " .
                            "cp /root/package-src/pgbackrest.spec ${strBuildPath}/SPECS && " .
                            "cp /root/package-src/*.patch ${strBuildPath}/SOURCES && " .
                            "cp /root/package-src/pgbackrest.logrotate ${strBuildPath}/SOURCES && " .
                            "sudo chown -R " . TEST_USER . " ${strBuildPath}'");

                        # Patch files in RHEL package builds
                        #
                        # Use these commands to create a new patch (may need to modify first line):
                        # BRDIR=/backrest;BRVM=co7;BRPATCHFILE=${BRDIR?}/test/patch/rhel-package.patch
                        # PKDIR=${BRDIR?}/test/result/package/${BRVM}/SPECS
                        # diff -Naur ${PKDIR?}.old ${PKDIR}.new > ${BRPATCHFILE?}
                        my $strPackagePatch = "${strBackRestBase}/test/patch/rhel-package.patch";

                        if ($oStorageBackRest->exists($strPackagePatch))
                        {
                            executeTest("cp -r ${strBuildPath}/SPECS ${strBuildPath}/SPECS.old");
                            executeTest("patch -d ${strBuildPath}/SPECS < ${strPackagePatch}");
                            executeTest("cp -r ${strBuildPath}/SPECS ${strBuildPath}/SPECS.new");
                        }

                        # Update version number to match current version
                        my $strSpec = ${$oStorageBackRest->get("${strBuildPath}/SPECS/pgbackrest.spec")};
                        $strSpec =~ s/^Version\:.*$/Version\:\t${\PROJECT_VERSION}/gm;
                        $oStorageBackRest->put("${strBuildPath}/SPECS/pgbackrest.spec", $strSpec);

                        # Build package
                        executeTest(
                            ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                            "rpmbuild --define 'pgmajorversion %{nil}' -v -bb --clean root/rpmbuild/SPECS/pgbackrest.spec",
                            {bSuppressStdErr => true});

                        # Remove build container
                        if ($strVm ne VM_NONE)
                        {
                            executeTest("docker rm -f test-build");
                        }
                    }
                }
            }

            # Exit if only testing builds
            exit 0 if $bBuildOnly;
        }

        # Remove repo.manifest now that all processing that depends on modified files has been completed
        #---------------------------------------------------------------------------------------------------------------------------
        $oStorageTest->remove("${strRepoCachePath}/${strRepoCacheManifest}");

        # Perform static source code analysis
        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDryRun)
        {
            logFileSet($oStorageTest, cwd() . "/test");
        }

        # Run the tests
        #---------------------------------------------------------------------------------------------------------------------------
        if (@{$oyTestRun} == 0)
        {
            confess &log(ERROR, 'no tests were selected');
        }

        &log(INFO, @{$oyTestRun} . ' test' . (@{$oyTestRun} > 1 ? 's': '') . " selected\n");

        # Don't allow --no-cleanup when more than one test will run.  How would the prior results be preserved?
        if ($bNoCleanup && @{$oyTestRun} > 1)
        {
            confess &log(ERROR, '--no-cleanup is not valid when more than one test will run')
        }

        # Disable file logging for integration tests when there is more than one test since it will be overwritten
        if (@{$oyTestRun} > 1)
        {
            $strLogLevelTestFile = lc(OFF);
        }

        # Don't allow --no-cleanup when more than one test will run.  How would the prior results be preserved?

        # Only use one vm for dry run so results are printed in order
        if ($bDryRun)
        {
            $iVmMax = 1;
        }

        my $iTestIdx = 0;
        my $iVmTotal;
        my $iTestMax = @{$oyTestRun};
        my $bShowOutputAsync = $bVmOut && (@{$oyTestRun} == 1 || $iVmMax == 1) && ! $bDryRun ? true : false;

        do
        {
            do
            {
                $iVmTotal = 0;

                for (my $iVmIdx = 0; $iVmIdx < $iVmMax; $iVmIdx++)
                {
                    if (defined($$oyProcess[$iVmIdx]))
                    {
                        my ($bDone, $bFail) = $$oyProcess[$iVmIdx]->end();

                        if ($bDone)
                        {
                            if ($bFail)
                            {
                                if ($oyProcess->[$iVmIdx]->run())
                                {
                                    $iTestRetry++;
                                    $iVmTotal++;
                                }
                                else
                                {
                                    $iTestFail++;
                                    $$oyProcess[$iVmIdx] = undef;
                                }
                            }
                            else
                            {
                                $$oyProcess[$iVmIdx] = undef;
                            }
                        }
                        else
                        {
                            $iVmTotal++;
                        }
                    }
                }

                # Only wait when all VMs are running or all tests have been assigned.  Otherwise, there is something to do.
                if ($iVmTotal == $iVmMax || $iTestIdx == @{$oyTestRun})
                {
                    waitHiRes(.05);
                }
            }
            while ($iVmTotal == $iVmMax);

            for (my $iVmIdx = 0; $iVmIdx < $iVmMax; $iVmIdx++)
            {
                if (!defined($$oyProcess[$iVmIdx]) && $iTestIdx < @{$oyTestRun})
                {
                    my $oJob = new pgBackRestTest::Common::JobTest(
                        $oStorageTest, $strBackRestBase, $strTestPath, $$oyTestRun[$iTestIdx], $bDryRun, $strVmHost, $bVmOut,
                        $iVmIdx, $iVmMax, $iTestIdx, $iTestMax, $strLogLevel, $strLogLevelTest, $strLogLevelTestFile,
                        !$bNoLogTimestamp, $bLogForce, $bShowOutputAsync, $bNoCleanup, $iRetry, !$bNoValgrind, !$bNoCoverage,
                        $bCoverageSummary, !$bNoOptimize, $bBackTrace, $bProfile, $iScale, $strTimeZone, !$bNoDebug,
                        $bDebugTestTrace, $iBuildMax / $iVmMax < 1 ? 1 : int($iBuildMax / $iVmMax));
                    $iTestIdx++;

                    if ($oJob->run())
                    {
                        $$oyProcess[$iVmIdx] = $oJob;
                    }

                    $iVmTotal++;
                }
            }
        }
        while ($iVmTotal > 0);

        # Write out coverage info and test coverage
        #---------------------------------------------------------------------------------------------------------------------------
        my $iUncoveredCodeModuleTotal = 0;

        if (vmCoverageC($strVm) && !$bNoCoverage && !$bDryRun && $iTestFail == 0)
        {
            $iUncoveredCodeModuleTotal = coverageValidateAndGenerate(
                $oyTestRun, $oStorageBackRest, !$bNoCoverageReport, $bCoverageSummary, $strTestPath, "${strTestPath}/temp",
                "${strBackRestBase}/test/result", "${strBackRestBase}/doc/xml/auto");
        }

        # Print test info and exit
        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO,
            ($bDryRun ? 'DRY RUN COMPLETED' : 'TESTS COMPLETED') . ($iTestFail == 0 ? ' SUCCESSFULLY' .
                ($iUncoveredCodeModuleTotal == 0 ? '' : " WITH ${iUncoveredCodeModuleTotal} MODULE(S) MISSING COVERAGE") :
            " WITH ${iTestFail} FAILURE(S)") . ($iTestRetry == 0 ? '' : ", ${iTestRetry} RETRY(IES)") .
                ($bNoLogTimestamp ? '' : ' (' . (time() - $lStartTime) . 's)'));

        exit 1 if ($iTestFail > 0 || ($iUncoveredCodeModuleTotal > 0 && !$bCoverageSummary));

        exit 0;
    }

    ################################################################################################################################
    # Runs tests
    ################################################################################################################################
    my $iRun = 0;

    # Create host group for containers
    my $oHostGroup = hostGroupGet();

    # Set timezone
    if (defined($strTimeZone))
    {
        $ENV{TZ} = $strTimeZone;
    }

    # Run the test
    testRun($stryModule[0], $stryModuleTest[0])->process(
        $strVm, $iVmId,                                             # Vm info
        $strBackRestBase,                                           # Base backrest directory
        $strTestPath,                                               # Path where the tests will run
        dirname($strTestPath) . "/bin/${strVm}/" . PROJECT_EXE,     # Path to the pgbackrest binary
        dirname($strTestPath) . "/bin/" . VM_NONE . '/' . PROJECT_EXE,  # Path to the backrest Perl storage helper
        $strPgVersion ne 'minimal' ? $strPgSqlBin: undef,           # Pg bin path
        $strPgVersion ne 'minimal' ? $strPgVersion: undef,          # Pg version
        $stryModule[0], $stryModuleTest[0], \@iyModuleTestRun,      # Module info
        $bVmOut, $bDryRun, $bNoCleanup, $bLogForce,                 # Test options
        $strLogLevelTestFile,                                       # Log options
        TEST_USER, TEST_GROUP);                                     # User/group info

    if (!$bNoCleanup)
    {
        if ($oHostGroup->removeAll() > 0)
        {
            executeTest("rm -rf ${strTestPath}");
        }
    }

    if (!$bDryRun && !$bVmOut)
    {
        &log(INFO, 'TESTS COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)');
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
    if (isException(\$EVAL_ERROR))
    {
        if ($EVAL_ERROR->code() != ERROR_OPTION_INVALID_VALUE)
        {
            syswrite(*STDOUT, $EVAL_ERROR->message() . "\n" . $EVAL_ERROR->trace());
        }

        exit $EVAL_ERROR->code();
    }

    # Else output the unhandled error
    syswrite(*STDOUT, $EVAL_ERROR);
    exit ERROR_UNHANDLED;
};

# It shouldn't be possible to get here
&log(ASSERT, 'execution reached invalid location in ' . __FILE__ . ', line ' . __LINE__);
exit ERROR_ASSERT;
