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
use File::stat;
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

use pgBackRestTest::Common::BuildTest;
use pgBackRestTest::Common::CodeCountTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::JobTest;
use pgBackRestTest::Common::ListTest;
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
   --build-only         build the binary (and honor --build-package) but don't run tests
   --build-package      build the package
   --build-max          max processes to use for builds (default 4)
   --c-only             only run C tests
   --container-only     only run tests that must be run in a container
   --no-performance     do not run performance tests
   --gen-only           only run auto-generation
   --gen-check          check that auto-generated files are correct (used in CI to detect changes)
   --code-count         generate code counts
   --no-back-trace      don't run backrace on C unit tests (may be slow with valgrind)
   --no-valgrind        don't run valgrind on C unit tests (saves time)
   --no-coverage        don't run coverage on C unit tests (saves time)
   --no-coverage-report run coverage but don't generate coverage report (saves time)
   --no-optimize        don't do compile optimization for C (saves compile time)
   --profile            generate profile info
   --no-debug           don't generate a debug build
   --scale              scale performance tests
   --tz                 test with the specified timezone
   --debug-test-trace   test stack trace for low-level functions (slow, esp w/valgrind, may cause timeouts)

 Code Format Options:
   --code-format        format code to project standards -- this may overwrite files!
   --code-format-check  check that code is formatted to project standards

 Report Options:
   --coverage-summary   generate a coverage summary report for the documentation
   --coverage-only      only run coverage tests (as a subset of selected tests) for the documentation

 Configuration Options:
   --psql-bin           path to the psql executables (e.g. /usr/lib/postgresql/16/bin/)
   --test-path          path where tests are executed (defaults to ./test)
   --log-level          log level to use for test harness (and Perl tests) (defaults to INFO)
   --log-level-test     log level to use for C tests (defaults to OFF)
   --log-level-test-file log level to use for file logging in integration tests (defaults to TRACE)
   --no-log-timestamp   suppress timestamps, timings, etc. Used to generate documentation.
   --make-cmd           gnu-compatible make command (defaults to make)
   --quiet, -q          equivalent to --log-level=off

 VM Options:
   --vm                 docker container to build/test (e.g. rh8)
   --vm-arch            docker container architecture
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
my $strLogLevelTestFile = lc(DEBUG);
my $bNoLogTimestamp = false;
my $bVmOut = false;
my @stryModule;
my @stryModuleTest;
my @iyModuleTestRun;
my $iVmMax = 1;
my $bDryRun = false;
my $bCodeFormat = false;
my $bCodeFormatCheck = false;
my $bNoCleanup = false;
my $strPgSqlBin;
my $strTestPath;
my $strMakeCmd = 'make';
my $bVersion = false;
my $bHelp = false;
my $bQuiet = false;
my $strPgVersion = 'minimal';
my $strVm = VM_NONE;
my $strVmArch;
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
my $bGenCheck = false;
my $bCodeCount = false;
my $bProfile = false;
my $bNoBackTrace = false;
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
            'psql-bin=s' => \$strPgSqlBin,
            'test-path=s' => \$strTestPath,
            'make-cmd=s' => \$strMakeCmd,
            'log-level=s' => \$strLogLevel,
            'log-level-test=s' => \$strLogLevelTest,
            'log-level-test-file=s' => \$strLogLevelTestFile,
            'no-log-timestamp' => \$bNoLogTimestamp,
            'vm=s' => \$strVm,
            'vm-arch=s' => \$strVmArch,
            'vm-out' => \$bVmOut,
            'vm-build' => \$bVmBuild,
            'vm-force' => \$bVmForce,
            'module=s@' => \@stryModule,
            'test=s@' => \@stryModuleTest,
            'run=s@' => \@iyModuleTestRun,
            'vm-max=s' => \$iVmMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'pg-version=s' => \$strPgVersion,
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
            'gen-check' => \$bGenCheck,
            'code-count' => \$bCodeCount,
            'code-format' => \$bCodeFormat,
            'code-format-check' => \$bCodeFormatCheck,
            'profile' => \$bProfile,
            'no-back-trace' => \$bNoBackTrace,
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
    # Update options for --coverage-summary
    ################################################################################################################################
    if ($bCoverageSummary)
    {
        $bCoverageOnly = true;
        $bCOnly = true;
    }

    ################################################################################################################################
    # Update options for --profile
    ################################################################################################################################
    if ($bProfile)
    {
        $bNoBackTrace = true;
        $bNoValgrind = true;
        $bNoCoverage = true;
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
    &log(INFO, 'test begin on ' . hostArch() . " - log level ${strLogLevel}");

    if (@iyModuleTestRun > 1)
    {
        confess "Only one --run can be provided";
    }

    if (@stryModuleTest != 0 && @stryModule != 1)
    {
        confess "Only one --module can be provided when --test is specified";
    }

    if (@iyModuleTestRun != 0 && @stryModuleTest != 1)
    {
        confess "Only one --test can be provided when --run is specified";
    }

    # Check vm
    vmValid($strVm);

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
            confess &log(ERROR, "select a VM for coverage testing");
        }
        elsif ($strVm eq VM_ALL)
        {
            confess &log(ERROR, "select a single VM for coverage testing");
        }
    }

    # Get the base backrest path
    my $strBackRestBase = dirname(dirname(abs_path($0)));
    my $strVagrantPath = "${strBackRestBase}/test/.vagrant";

    my $oStorageBackRest = new pgBackRestTest::Common::Storage(
        $strBackRestBase, new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    # Check that the test path is not in the git repo path
    if (index("${strTestPath}/", "${strBackRestBase}/") != -1)
    {
        confess &log(
            ERROR,
            "test path '${strTestPath}' may not be in the repo path '${strBackRestBase}'\n" .
            "HINT: was test.pl run in '${strBackRestBase}'?\n" .
            "HINT: use --test-path to set a test path\n" .
            "HINT: run test.pl from outside the repo, e.g. 'pgbackrest/test/test.pl'");
    }

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
        containerBuild($oStorageBackRest, $strVm, $strVmArch, $bVmForce);
        exit 0;
    }

    ################################################################################################################################
    # Load test definition
    ################################################################################################################################
    testDefLoad(${$oStorageBackRest->get("test/define.yaml")});

    ################################################################################################################################
    # Start VM and run
    ################################################################################################################################

    # Clean up
    #-------------------------------------------------------------------------------------------------------------------------------
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
            "chmod 700 -R ${strTestPath}/test-* 2>&1 || true && rm -rf ${strTestPath}/temp ${strTestPath}/test-*" .
                " ${strTestPath}/data-*");
        $oStorageTest->pathCreate("${strTestPath}/temp", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

        # Overwrite the C coverage report so it will load but not show old coverage
        $oStorageTest->pathCreate(
            "${strBackRestBase}/test/result/coverage", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});
        $oStorageBackRest->put(
            "${strBackRestBase}/test/result/coverage/coverage.html",
            "<center>[ " . ($bNoCoverage ? "No Coverage Testing" : "Generating Coverage Report") . " ]</center>");

        # Copy C code for coverage tests
        if (vmCoverageC($strVm) && !$bDryRun)
        {
            executeTest("rm -rf ${strBackRestBase}/test/result/coverage/raw/*");
            $oStorageTest->pathCreate("${strCodePath}", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});
        }
    }

    # Auto-generate version for root meson.build script
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strMesonBuildOld = ${$oStorageTest->get("${strBackRestBase}/meson.build")};
    my $strMesonBuildNew;

    foreach my $strLine (split("\n", $strMesonBuildOld))
    {
        if ($strLine =~ /^    version\: '/)
        {
            $strLine = "    version: '" . PROJECT_VERSION . "',";
        }

        $strMesonBuildNew .= "${strLine}\n";
    }

    buildPutDiffers($oStorageBackRest, "${strBackRestBase}/meson.build", $strMesonBuildNew);

    # Auto-generate version for version.h
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strVersionCOld = ${$oStorageTest->get("${strBackRestBase}/src/version.h")};
    my $strVersionCNew;

    foreach my $strLine (split("\n", $strVersionCOld))
    {
        if ($strLine =~ /^#define PROJECT_VERSION /)
        {
            $strLine = "#define PROJECT_VERSION" . (' ' x 45) . '"' . PROJECT_VERSION . '"';
        }

        if ($strLine =~ /^#define PROJECT_VERSION_NUM /)
        {
            $strLine =
                "#define PROJECT_VERSION_NUM" . (' ' x 41) .
                sprintf('%d%03d%03d', PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
        }

        $strVersionCNew .= "${strLine}\n";
    }

    buildPutDiffers($oStorageBackRest, "${strBackRestBase}/src/version.h", $strVersionCNew);

    # Start build container if vm is not none
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strPlatform = defined($strVmArch) ? " --platform linux/${strVmArch}" : '';
    my $strImage = containerRepo() . ":${strVm}-test" . (defined($strVmArch) ? "-${strVmArch}" : '-' . hostArch());

    if ($strVm ne VM_NONE)
    {
        my $strCCachePath = "${strTestPath}/ccache-0/${strVm}";

        if (!$oStorageTest->pathExists($strCCachePath))
        {
            $oStorageTest->pathCreate($strCCachePath, {strMode => '0770', bCreateParent => true});
        }

        executeTest(
            "docker run${strPlatform} -itd -h test-build --name=test-build" .
                " -v ${strBackRestBase}:${strBackRestBase} -v ${strTestPath}:${strTestPath}" .
                " -v ${strCCachePath}:/home/${\TEST_USER}/.ccache" . " ${strImage}",
            {bSuppressStdErr => true});
    }

    # Create path for repo cache
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strRepoCachePath = "${strTestPath}/repo";

    # Create the repo path -- this should hopefully prevent obvious rsync errors below
    $oStorageTest->pathCreate($strRepoCachePath, {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

    # Auto-generate code files (if --dry-run specified then do minimum required)
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strBuildPath = "${strTestPath}/build/none";
    my $strBuildNinja = "${strBuildPath}/build.ninja";

    &log(INFO, (!-e $strBuildNinja ? 'clean ' : '') . 'autogenerate code');

    # Setup build if it does not exist
    my $strGenerateCommand =
        "ninja -C ${strBuildPath} src/build-code" .
        ($bDryRun ? '' : " && \\\n${strBuildPath}/src/build-code config ${strBackRestBase}/src") .
        ($bDryRun ? '' : " && \\\n${strBuildPath}/src/build-code error ${strBackRestBase}/src") .
        ($bDryRun ? '' : " && \\\n${strBuildPath}/src/build-code postgres-version ${strBackRestBase}/src") .
        " && \\\n${strBuildPath}/src/build-code postgres ${strBackRestBase}/src ${strRepoCachePath}" .
        " && \\\nninja -C ${strBuildPath} test/src/test-pgbackrest";

    if (!-e $strBuildNinja)
    {
        $strGenerateCommand =
            "meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug ${strBuildPath} ${strBackRestBase} && \\\n" .
            $strGenerateCommand;
    }

    # Build code
    executeTest($strGenerateCommand);

    if ($bGenOnly)
    {
        exit 0;
    }

    # Make a copy of the repo to track which files have been changed
    #-------------------------------------------------------------------------------------------------------------------------------
    executeTest(
        "git -C ${strBackRestBase} ls-files -c --others --exclude-standard |" .
            " rsync -rLtW --delete --files-from=- --exclude=test/result" .
            # This option is not supported on MacOS. The eventual plan is to remove the need for it.
            (trim(`uname`) ne 'Darwin' ? ' --ignore-missing-args' : '') .
            " ${strBackRestBase}/ ${strRepoCachePath}");

    # Format code with uncrustify and check permissions
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($bCodeFormat || $bCodeFormatCheck)
    {
        &log(INFO, 'code format' . ($bCodeFormatCheck ? ' check' : ''));

        my $hRepoManifest = $oStorageTest->manifest($strRepoCachePath);
        my $hManifest = $oStorageBackRest->manifest('');
        my $strCommand =
            "uncrustify -c ${strBackRestBase}/test/uncrustify.cfg" .
            ($bCodeFormatCheck ? ' --check' : ' --replace --no-backup');

        foreach my $strFile (sort(keys(%{$hManifest})))
        {
            # Skip non-C files
            next if $hManifest->{$strFile}{type} ne 'f' || ($strFile !~ /\.c$/ && $strFile !~ /\.h$/);

            # Skip files that do are not version controlled
            next if !defined($hRepoManifest->{$strFile});

            # Skip specific file
            next if
                # Does not format correctly because it is a template
                $strFile eq 'test/src/test.c' ||
                # Contains code copied directly from PostgreSQL
                $strFile eq 'src/postgres/interface/static.vendor.h' ||
                $strFile eq 'src/postgres/interface/version.vendor.h';

            $strCommand .= " ${strBackRestBase}/${strFile}";
        }

        executeTest($strCommand . " 2>&1");

        # Check execute permissions to make sure nothing got munged
        foreach my $strFile (sort(keys(%{$hManifest})))
        {
            next if ($strFile eq '.') || !defined($hRepoManifest->{$strFile});

            my $strExpectedMode = sprintf('%04o', oct($hManifest->{$strFile}{mode}) & 0666);

            if ($strFile eq 'doc/doc.pl' ||
                $strFile eq 'doc/release.pl' ||
                $strFile eq 'test/ci.pl' ||
                $strFile eq 'test/test.pl' ||
                $hManifest->{$strFile}{type} eq 'd')
            {
                $strExpectedMode = sprintf('%04o', oct($hManifest->{$strFile}{mode}) & 0777);
            }

            if ($hManifest->{$strFile}{mode} ne $strExpectedMode)
            {
                confess &log(
                    ERROR,
                    "expected mode for '${strExpectedMode}' for '${strFile}' but found '" . $hManifest->{$strFile}{mode} . "'");
            }
        }

        exit 0;
    }

    # Generate code counts
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($bCodeCount)
    {
        &log(INFO, "classify code files");

        codeCountScan($oStorageBackRest, $strBackRestBase);
        exit 0;
    }

    # Determine which tests to run
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oyTestRun;
    my $bBinRequired = $bBuildOnly;
    my $bUnitRequired = $bBuildOnly;

    # Only get the test list when they can run
    if (!$bBuildOnly)
    {
        # Get the test list
        $oyTestRun = testListGet(
            $strVm, \@stryModule, \@stryModuleTest, \@iyModuleTestRun, $strPgVersion, $bCoverageOnly, $bCOnly, $bContainerOnly,
            $bNoPerformance);

        # Determine if the C binary needs to be built
        foreach my $hTest (@{$oyTestRun})
        {
            # Unit build required for unit tests
            if ($hTest->{&TEST_C})
            {
                $bUnitRequired = true;
            }

            # Bin build required for integration tests and when specified
            if (!$hTest->{&TEST_C} || $hTest->{&TEST_BIN_REQ})
            {
                $bBinRequired = true;
            }
        }
    }

    # Build the binary and packages
    #-------------------------------------------------------------------------------------------------------------------------------
    if (!$bDryRun)
    {
        my $oVm = vmGet();

        # Build the binary
        #---------------------------------------------------------------------------------------------------------------------------
        if ($bBinRequired || $bUnitRequired)
        {
            # Loop through VMs to do the C bin/integration builds
            my $bLogDetail = $strLogLevel eq 'detail';
            my @stryBuildVm = $strVm eq VM_ALL ? VM_LIST : ($strVm);

            foreach my $strBuildVM (@stryBuildVm)
            {
                my $strBuildPath = "${strTestPath}/build/${strBuildVM}";
                my $strBuildNinja = "${strBuildPath}/build.ninja";

                foreach my $strBuildVM (@stryBuildVm)
                {
                    my $strBuildPath = "${strTestPath}/build/${strBuildVM}";
                    my $strBuildNinja = "${strBuildPath}/build.ninja";

                    &log(INFO, (!-e $strBuildNinja ? 'clean ' : '') . "build for ${strBuildVM} (${strBuildPath})");

                    # Setup build if it does not exist
                    my $strBuildCommand =
                        "ninja -C ${strBuildPath}" . ($bBinRequired ? ' src/pgbackrest' : '') .
                        ($bUnitRequired ? ' test/src/test-pgbackrest' : '');

                    if (!-e $strBuildNinja)
                    {
                        $strBuildCommand =
                            "meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug ${strBuildPath}" .
                                " ${strBackRestBase} && \\\n" .
                            $strBuildCommand;
                    }

                    # Build code
                    executeTest(
                        ($strBuildVM ne VM_NONE ? 'docker exec -i -u ' . TEST_USER . " test-build bash -c '" : '') .
                            $strBuildCommand . ($strBuildVM ne VM_NONE ? "'" : ''),
                        {bShowOutputAsync => $bLogDetail});
                }
            }
        }

        # Shut down the build vm
        #---------------------------------------------------------------------------------------------------------------------------
        if ($strVm ne VM_NONE)
        {
            executeTest("docker rm -f test-build");
        }

        # Build the package
        #---------------------------------------------------------------------------------------------------------------------------
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
                            "docker run${strPlatform} -itd -h test-build --name=test-build" .
                            " -v ${strBackRestBase}:${strBackRestBase} ${strImage}",
                            {bSuppressStdErr => true});
                    }

                    $oStorageBackRest->pathCreate($strBuildPath, {bIgnoreExists => true, bCreateParent => true});

                    # Clone a copy of the debian package repo
                    executeTest(
                        ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                        "bash -c 'git clone https://salsa.debian.org/postgresql/pgbackrest.git /root/package-src 2>&1'");

                    executeTest(
                        "rsync -rL --exclude=.vagrant --exclude=.git --exclude=test/result ${strBackRestBase}/" .
                            " ${strBuildPath}/");
                    executeTest(
                        ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                        "bash -c 'cp -r /root/package-src/debian ${strBuildPath} && sudo chown -R " . TEST_USER .
                        " ${strBuildPath}'");

                    # Patch files in debian package builds
                    #
                    # Use these commands to create a new patch (may need to modify first line):
                    # BRDIR=/home/vagrant/pgbackrest;BRVM=u22;BRPATCHFILE=${BRDIR?}/test/patch/debian-package.patch
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
                        "bash -c 'cd ${strBuildPath} && debuild -d -i -us -uc -b'");

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
                            "docker run${strPlatform} -itd -h test-build --name=test-build" .
                            " -v ${strBackRestBase}:${strBackRestBase} ${strImage}",
                            {bSuppressStdErr => true});
                    }

                    # Fetching specific files is fragile but even a shallow clone of the entire pgrpms repo is very expensive.
                    # Using 'git archive' does not seem to work: access denied or repository not exported: /git/pgrpms.git.
                    executeTest(
                        ($strVm ne VM_NONE ? "docker exec -i test-build " : '') .
                        "bash -c \"" .
                        "mkdir /root/package-src && " .
                        "wget -q -O /root/package-src/pgbackrest-conf.patch " .
                            "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;hb=refs/heads/mas"."ter;" .
                            "f=rpm/redhat/mas"."ter/common/pgbackrest/mas"."ter/pgbackrest-conf.patch' && " .
                        "wget -q -O /root/package-src/pgbackrest.logrotate " .
                            "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;hb=refs/heads/mas"."ter;" .
                            "f=rpm/redhat/mas"."ter/common/pgbackrest/mas"."ter/pgbackrest.logrotate' && " .
                        "wget -q -O /root/package-src/pgbackrest.spec " .
                            "'https://git.postgresql.org/gitweb/?p=pgrpms.git;a=blob_plain;hb=refs/heads/mas"."ter;" .
                            "f=rpm/redhat/mas"."ter/common/pgbackrest/mas"."ter/pgbackrest.spec'\"");

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
                    # BRDIR=/home/vagrant/pgbackrest;BRVM=rh8;BRPATCHFILE=${BRDIR?}/test/patch/rhel-package.patch
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

    # Perform static source code analysis
    #-------------------------------------------------------------------------------------------------------------------------------
    if (!$bDryRun)
    {
        logFileSet($oStorageTest, cwd() . "/test");
    }

    # Run the tests
    #-------------------------------------------------------------------------------------------------------------------------------
    if (@{$oyTestRun} == 0)
    {
        confess &log(ERROR, 'no tests were selected');
    }

    &log(INFO, @{$oyTestRun} . ' test' . (@{$oyTestRun} > 1 ? 's': '') . " selected\n");

    # Don't allow --no-cleanup when more than one test will run. How would the prior results be preserved?
    if ($bNoCleanup && @{$oyTestRun} > 1)
    {
        confess &log(ERROR, '--no-cleanup is not valid when more than one test will run')
    }

    # Disable file logging for integration tests when there is more than one test since it will be overwritten
    if (@{$oyTestRun} > 1)
    {
        $strLogLevelTestFile = lc(OFF);
    }

    # Don't allow --no-cleanup when more than one test will run. How would the prior results be preserved?

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

            # Only wait when all VMs are running or all tests have been assigned. Otherwise, there is something to do.
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
                    $oStorageTest, $strBackRestBase, $strTestPath, $$oyTestRun[$iTestIdx], $bDryRun, $bVmOut, $strPlatform,
                    $strImage, $iVmIdx, $iVmMax, $strMakeCmd, $iTestIdx, $iTestMax, $strLogLevel, $strLogLevelTest,
                    $strLogLevelTestFile, !$bNoLogTimestamp, $bShowOutputAsync, $bNoCleanup, $iRetry, !$bNoBackTrace, !$bNoValgrind,
                    !$bNoCoverage, $bCoverageSummary, !$bNoOptimize, $bProfile, $iScale, $strTimeZone, !$bNoDebug, $bDebugTestTrace,
                    $iBuildMax / $iVmMax < 1 ? 1 : int($iBuildMax / $iVmMax));
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
    #-------------------------------------------------------------------------------------------------------------------------------
    my $bUncoveredCodeModule = false;

    if (vmCoverageC($strVm) && !$bNoCoverage && !$bDryRun && $iTestFail == 0 && @iyModuleTestRun == 0)
    {
        my $strModuleList;

        foreach my $hTest (@{$oyTestRun})
        {
            $strModuleList .= ' ' . $hTest->{&TEST_MODULE} . '/' . $hTest->{&TEST_NAME};
        }

        my $oExec = new pgBackRestTest::Common::ExecuteTest(
            "${strBuildPath}/test/src/test-pgbackrest --log-level=warn --vm=${strVm} --repo-path=${strBackRestBase}" .
            " --test-path=${strTestPath}" . ($bCoverageSummary ? ' --coverage-summary' : '') . " test${strModuleList}",
            {bShowOutputAsync => true, bSuppressError => true});
        $oExec->begin();
        my $iResult = $oExec->end();

        if ($iResult <= 1)
        {
            $bUncoveredCodeModule = $iResult == 1;

            if (!$bUncoveredCodeModule)
            {
                &log(INFO, "tested modules have full coverage");
            }

        }
        else
        {
            &log(ERROR, "TEST SUBCOMMAND RETURNED ERROR CODE $iResult");
            exit $iResult;
        }
    }

    # Print test info and exit
    #-------------------------------------------------------------------------------------------------------------------------------
    &log(INFO,
        ($bDryRun ? 'DRY RUN COMPLETED' : 'TESTS COMPLETED') . ($iTestFail == 0 ? ' SUCCESSFULLY' .
            (!$bUncoveredCodeModule ? '' : " WITH MODULE(S) MISSING COVERAGE") :
        " WITH ${iTestFail} FAILURE(S)") . ($iTestRetry == 0 ? '' : ", ${iTestRetry} RETRY(IES)") .
            ($bNoLogTimestamp ? '' : ' (' . (time() - $lStartTime) . 's)'));

    exit 1 if ($iTestFail > 0 || ($bUncoveredCodeModule && !$bCoverageSummary));

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
