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

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::DbVersion;
use pgBackRest::Storage::Posix::Driver;
use pgBackRest::Storage::Local;
use pgBackRest::Version;

use pgBackRestBuild::Build;
use pgBackRestBuild::Build::Common;
use pgBackRestBuild::Config::Build;
use pgBackRestBuild::Config::BuildDefine;
use pgBackRestBuild::Config::BuildParse;
use pgBackRestBuild::Embed::Build;
use pgBackRestBuild::Error::Build;
use pgBackRestBuild::Error::Data;

use pgBackRestTest::Common::BuildTest;
use pgBackRestTest::Common::CodeCountTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::CiTest;
use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::JobTest;
use pgBackRestTest::Common::ListTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Common::VmTest;

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
   --no-cleanup         don't cleaup after the last test is complete - useful for debugging
   --pg-version         version of postgres to test (all, defaults to minimal)
   --log-force          force overwrite of current test log files
   --no-lint            disable static source code analysis
   --build-only         compile the test library / packages and run tests only
   --coverage-only      only run coverage tests (as a subset of selected tests)
   --c-only             only run C tests
   --gen-only           only run auto-generation
   --no-gen             do not run code generation
   --code-count         generate code counts
   --smart              perform libc/package builds only when source timestamps have changed
   --no-package         do not build packages
   --no-ci-config       don't overwrite the current continuous integration config
   --dev                --no-lint --smart --no-package --no-optimize
   --dev-test           --no-lint --no-package
   --expect             --no-lint --no-package --vm=co7 --db=9.6 --log-force
   --no-valgrind        don't run valgrind on C unit tests (saves time)
   --no-coverage        don't run coverage on C unit tests (saves time)
   --no-optimize        don't do compile optimization for C (saves compile time)
   --backtrace          enable backtrace when available (adds stack trace line numbers -- very slow)
   --profile            generate profile info
   --no-debug           don't generate a debug build

 Configuration Options:
   --psql-bin           path to the psql executables (e.g. /usr/lib/postgresql/9.3/bin/)
   --test-path          path where tests are executed (defaults to ./test)
   --log-level          log level to use for tests (defaults to INFO)
   --quiet, -q          equivalent to --log-level=off

 VM Options:
   --vm                 docker container to build/test (u12, u14, co6, co7)
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
my $strLogLevel = lc(INFO);
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
my $bNoLint = false;
my $bBuildOnly = false;
my $bCoverageOnly = false;
my $bNoCoverage = false;
my $bCOnly = false;
my $bGenOnly = false;
my $bNoGen = false;
my $bCodeCount = false;
my $bSmart = false;
my $bNoPackage = false;
my $bNoCiConfig = false;
my $bDev = false;
my $bDevTest = false;
my $bBackTrace = false;
my $bProfile = false;
my $bExpect = false;
my $bNoValgrind = false;
my $bNoOptimize = false;
my $bNoDebug = false;
my $iRetry = 0;

GetOptions ('q|quiet' => \$bQuiet,
            'version' => \$bVersion,
            'help' => \$bHelp,
            'pgsql-bin=s' => \$strPgSqlBin,
            'test-path=s' => \$strTestPath,
            'log-level=s' => \$strLogLevel,
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
            'no-lint' => \$bNoLint,
            'build-only' => \$bBuildOnly,
            'no-package' => \$bNoPackage,
            'no-ci-config' => \$bNoCiConfig,
            'coverage-only' => \$bCoverageOnly,
            'no-coverage' => \$bNoCoverage,
            'c-only' => \$bCOnly,
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
        syswrite(*STDOUT, BACKREST_NAME . ' ' . BACKREST_VERSION . " Test Engine\n");

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
    # Update options for --dev and --dev-fast and --dev-test
    ################################################################################################################################
    if ($bDev && $bDevTest)
    {
        confess "cannot combine --dev and --dev-test";
    }

    if ($bDev)
    {
        $bNoLint = true;
        $bSmart = true;
        $bNoPackage = true;
        $bNoOptimize = true;
    }

    if ($bDevTest)
    {
        $bNoPackage = true;
        $bNoLint = true;
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
        $bNoLint = true;
        $bNoPackage = true;
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

    logLevelSet(uc($strLogLevel), uc($strLogLevel), OFF);
    &log(INFO, "test begin - log level ${strLogLevel}");

    if (@stryModuleTest != 0 && @stryModule != 1)
    {
        confess "Only one --module can be provided when --test is specified";
    }

    if (@iyModuleTestRun != 0 && @stryModuleTest != 1)
    {
        confess "Only one --test can be provided when --run is specified";
    }

    # Set test path if not expicitly set
    if (!defined($strTestPath))
    {
        $strTestPath = cwd() . '/test';
    }

    my $oStorageTest = new pgBackRest::Storage::Local(
        $strTestPath, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

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
        elsif (!vmCoverage($strVm))
        {
            confess &log(ERROR, "only Debian-based VMs can be used for coverage testing");
        }
    }

    # If VM is not defined then set it to all
    if (!defined($strVm))
    {
        $strVm = VM_ALL;
    }

    # Get the base backrest path
    my $strBackRestBase = dirname(dirname(abs_path($0)));

    my $oStorageBackRest = new pgBackRest::Storage::Local(
        $strBackRestBase, new pgBackRest::Storage::Posix::Driver({bFileSync => false, bPathSync => false}));

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
        # Generate code counts
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
            &log(INFO, "check code autogenerate");

            # Auto-generate C files
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bSmart || buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['build']) >
                buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['src'], '\.auto\.c$'))
            {
                &log(INFO, "    autogenerate C code");

                errorDefineLoad(${$oStorageBackRest->get("build/error.yaml")});

                my $rhBuild =
                {
                    'config' =>
                    {
                        &BLD_DATA => buildConfig(),
                        &BLD_PATH => 'config',
                    },

                    'configDefine' =>
                    {
                        &BLD_DATA => buildConfigDefine(),
                        &BLD_PATH => 'config',
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

                buildAll("${strBackRestBase}/src", $rhBuild);
            }

            # Auto-generate Perl code
            #-----------------------------------------------------------------------------------------------------------------------
            use lib dirname(dirname($0)) . '/libc/build/lib';
            use pgBackRestLibC::Build;                                      ## no critic (Modules::ProhibitConditionalUseStatements)

            if (!$bSmart || buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['build', 'libc/build']) >
                buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['lib'], 'Auto\.pm$'))
            {
                &log(INFO, "    autogenerate Perl code");

                errorDefineLoad(${$oStorageBackRest->get("build/error.yaml")});

                buildXsAll("${strBackRestBase}/libc");
            }

            # Auto-generate C library code to embed in the binary
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bSmart || buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['libc']) >
                buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['src/perl'], '^libc\.auto\.c$'))
            {
                &log(INFO, "    autogenerate embedded C code");

                my $strLibC = executeTest(
                    "cd ${strBackRestBase}/libc && " .
                    "perl /usr/share/perl/5.22/ExtUtils/xsubpp -typemap /usr/share/perl/5.22/ExtUtils/typemap" .
                        " -typemap typemap LibC.xs");

                # Trim off any trailing LFs
                $strLibC = trim($strLibC) . "\n";

                # Strip out line numbers.  These are useful for the LibC build but only cause churn in the binary
                # build.
                $strLibC =~ s/^\#line .*\n//mg;

                # Save into the bin src dir
                $oStorageBackRest->put("${strBackRestBase}/src/perl/libc.auto.c", $strLibC);
            }

            # Auto-generate embedded Perl code
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bSmart || buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['lib']) >
                buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['src/perl'], 'embed\.auto\.c'))
            {
                &log(INFO, "    autogenerate embedded Perl code");

                my $rhBuild =
                {
                    'embed' =>
                    {
                        &BLD_DATA => buildEmbed($oStorageBackRest),
                        &BLD_PATH => 'perl',
                    },
                };

                buildAll("${strBackRestBase}/src", $rhBuild);
            }

            # Auto-generate C Makefile
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bSmart || buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['libc', 'src']) >
                buildLastModTime($oStorageBackRest, "${strBackRestBase}", ['src'], 'Makefile'))
            {
                &log(INFO, "    autogenerate C Makefile");

                $oStorageBackRest->put(
                    "src/Makefile",
                    buildMakefile(
                        $oStorageBackRest,
                        ${$oStorageBackRest->get("src/Makefile")},
                        {rhOption => {'postgres/pageChecksum.o' => '-funroll-loops -ftree-vectorize'}}));
            }

            if ($bGenOnly)
            {
                exit 0;
            }
        }

        # Build CI config
        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bNoCiConfig)
        {
            (new pgBackRestTest::Common::CiTest($oStorageBackRest))->process();
        }

        # Check Perl version against release notes and update version in C code if needed
        #---------------------------------------------------------------------------------------------------------------------------
        my $bVersionDev = true;
        my $strVersionBase;

        if (!$bDev)
        {
            # Make sure version number matches the latest release
            #-----------------------------------------------------------------------------------------------------------------------
            &log(INFO, "check version info");

            # Load the doc modules dynamically since they are not supported on all systems
            require BackRestDoc::Common::Doc;
            BackRestDoc::Common::Doc->import();
            require BackRestDoc::Custom::DocCustomRelease;
            BackRestDoc::Custom::DocCustomRelease->import();

            my $strReleaseFile = dirname(dirname(abs_path($0))) . '/doc/xml/release.xml';
            my $oRelease =
                (new BackRestDoc::Custom::DocCustomRelease(new BackRestDoc::Common::Doc($strReleaseFile)))->releaseLast();
            my $strVersion = $oRelease->paramGet('version');
            $bVersionDev = false;
            $strVersionBase = $strVersion;

            if ($strVersion =~ /dev$/)
            {
                $bVersionDev = true;
                $strVersionBase = substr($strVersion, 0, length($strVersion) - 3);

                if (BACKREST_VERSION !~ /dev$/ && $oRelease->nodeTest('release-core-list'))
                {
                    confess "dev release ${strVersion} must match the program version when core changes have been made";
                }
            }
            elsif ($strVersion ne BACKREST_VERSION)
            {
                confess 'unable to find version ' . BACKREST_VERSION . " as the most recent release in ${strReleaseFile}";
            }

            # Update version for the C code based on the current Perl version
            #-----------------------------------------------------------------------------------------------------------------------
            my $strCVersionFile = "${strBackRestBase}/src/version.h";
            my $strCVersionOld = ${$oStorageTest->get($strCVersionFile)};
            my $strCVersionNew;

            foreach my $strLine (split("\n", $strCVersionOld))
            {
                if ($strLine =~ /^#define PGBACKREST_VERSION/)
                {
                    $strLine = '#define PGBACKREST_VERSION' . (' ' x 42) . '"' . BACKREST_VERSION . '"';
                }

                $strCVersionNew .= "${strLine}\n";
            }

            if ($strCVersionNew ne $strCVersionOld)
            {
                $oStorageTest->put($strCVersionFile, $strCVersionNew);
            }
        }

        # Clean up
        #---------------------------------------------------------------------------------------------------------------------------
        my $iTestFail = 0;
        my $iTestRetry = 0;
        my $oyProcess = [];
        my $strCoveragePath = "${strTestPath}/cover_db";
        my $strCodePath = "${strBackRestBase}/test/.vagrant/code";

        if (!$bDryRun || $bVmOut)
        {
            &log(INFO, "cleanup old data and containers");
            containerRemove('test-([0-9]+|build)');

            for (my $iVmIdx = 0; $iVmIdx < 8; $iVmIdx++)
            {
                push(@{$oyProcess}, undef);
            }

            executeTest("sudo umount ${strTestPath}", {bSuppressError => true});
            executeTest("sudo rm -rf ${strTestPath}/*");
            $oStorageTest->pathCreate($strTestPath, {strMode => '0770', bIgnoreExists => true});
            executeTest("sudo mount -t tmpfs -o size=2560M test ${strTestPath}");
            $oStorageTest->pathCreate($strCoveragePath, {strMode => '0770', bIgnoreMissing => true, bCreateParent => true});

            # Remove old coverage dirs -- do it this way so the dirs stay open in finder/explorer, etc.
            executeTest("rm -rf ${strBackRestBase}/test/coverage/c/*");
            executeTest("rm -rf ${strBackRestBase}/test/coverage/perl/*");

            # Copy C code for coverage tests
            if (vmCoverage($strVm) && !$bDryRun)
            {
                $oStorageTest->pathCreate("${strCodePath}/test", {strMode => '0770', bIgnoreExists => true, bCreateParent => true});

                executeTest("rsync -rt --delete --exclude=test ${strBackRestBase}/src/ ${strCodePath}");
                executeTest("rsync -rt --delete ${strBackRestBase}/test/src/module/ ${strCodePath}/test");
            }
        }

        # Determine which tests to run
        #---------------------------------------------------------------------------------------------------------------------------
        my $oyTestRun;
        my $bBinRequired = $bBuildOnly;
        my $bLibCHostRequired = $bBuildOnly;
        my $bLibCVmRequired = $bBuildOnly;

        # Only get the test list when they can run
        if (!$bBuildOnly)
        {
            # Get the test list
            $oyTestRun = testListGet(
                $strVm, \@stryModule, \@stryModuleTest, \@iyModuleTestRun, $strPgVersion, $bCoverageOnly, $bCOnly);

            # Determine if the C binary and test library need to be built
            foreach my $hTest (@{$oyTestRun})
            {
                # Bin build required for all Perl tests or if a C unit test calls Perl
                if (!$hTest->{&TEST_C} || $hTest->{&TEST_PERL_REQ})
                {
                    $bBinRequired = true;
                }

                # Host LibC required if a Perl test
                if (!$hTest->{&TEST_C})
                {
                    $bLibCHostRequired = true;
                }

                # VM LibC required if Perl and not an integration test
                if (!$hTest->{&TEST_C} && !$hTest->{&TEST_INTEGRATION})
                {
                    $bLibCVmRequired = true;
                }
            }
        }

        my $strBuildRequired;

        if ($bBinRequired || $bLibCHostRequired || $bLibCVmRequired)
        {
            if ($bBinRequired)
            {
                $strBuildRequired = "bin";
            }

            if ($bLibCHostRequired)
            {
                $strBuildRequired .= ", libc host";
            }

            if ($bLibCVmRequired)
            {
                $strBuildRequired .= ", libc vm";
            }
        }
        else
        {
            $strBuildRequired = "none";
        }

        &log(INFO, "builds required: ${strBuildRequired}");

        # Build the binary, library and packages
        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDryRun)
        {
            my $oVm = vmGet();
            my $strVagrantPath = "${strBackRestBase}/test/.vagrant";
            my $lTimestampLast;

            # Build the C Library
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bLibCHostRequired || $bLibCVmRequired)
            {
                my $strLibCPath = "${strVagrantPath}/libc";
                my $strLibCSmart = "${strLibCPath}/build.timestamp";
                my $bRebuild = !$bSmart;
                my @stryLibCSrcPath = $bLibCHostRequired || $bLibCVmRequired ? ('libc', 'src') : ('libc');

                # Find the lastest modified time for dirs that affect the libc build
                $lTimestampLast = buildLastModTime($oStorageBackRest, $strBackRestBase, \@stryLibCSrcPath);

                # Rebuild if the modification time of the smart file does equal the last changes in source paths
                if ($bSmart)
                {
                    if (!$oStorageBackRest->exists($strLibCSmart) ||
                        $oStorageBackRest->info($strLibCSmart)->mtime < $lTimestampLast)
                    {
                        &log(INFO, '    libc dependencies have changed, rebuilding...');

                        $bRebuild = true;
                    }
                }
                else
                {
                    executeTest("sudo rm -rf ${strLibCPath}");
                }

                # Delete old libc files from the host
                if ($bRebuild)
                {
                    executeTest('sudo rm -rf ' . $oVm->{$strVmHost}{&VMDEF_PERL_ARCH_PATH} . '/auto/pgBackRest/LibC');
                    executeTest('sudo rm -rf ' . $oVm->{$strVmHost}{&VMDEF_PERL_ARCH_PATH} . '/pgBackRest');
                }

                # Loop through VMs to do the C Library builds
                my $bLogDetail = $strLogLevel eq 'detail';
                my @stryBuildVm = ();

                if ($strVm eq VM_ALL)
                {
                    @stryBuildVm  = $bLibCVmRequired ? VM_LIST : ($strVmHost);
                }
                else
                {
                    @stryBuildVm  = $bLibCVmRequired && $strVmHost ne $strVm ? ($strVmHost, $strVm) : ($strVmHost);
                }

                foreach my $strBuildVM (@stryBuildVm)
                {
                    my $strBuildPath = "${strLibCPath}/${strBuildVM}/libc";
                    my $bContainerExists = $strBuildVM ne $strVmHost;

                    if ($bRebuild)
                    {
                        &log(INFO, "    build test library for ${strBuildVM} (${strBuildPath})");

                        # It's very expensive to rebuild the Makefile so make sure it has actually changed
                        my $bMakeRebuild =
                            !$oStorageBackRest->exists("${strBuildPath}/Makefile") ||
                            ($oStorageBackRest->info("${strBackRestBase}/libc/Makefile.PL")->mtime >
                             $oStorageBackRest->info("${strBuildPath}/Makefile.PL")->mtime);

                        if ($bContainerExists)
                        {
                            executeTest(
                                "docker run -itd -h test-build --name=test-build" .
                                " -v ${strBackRestBase}:${strBackRestBase} " . containerRepo() . ":${strBuildVM}-build",
                                {bSuppressStdErr => true});
                        }

                        foreach my $strLibCSrcPath ('lib', 'libc', 'src')
                        {
                            $oStorageBackRest->pathCreate(
                                "${strLibCPath}/${strBuildVM}/${strLibCSrcPath}", {bIgnoreExists => true, bCreateParent => true});
                            executeTest(
                                "rsync -rt ${strBackRestBase}/${strLibCSrcPath}/* ${strLibCPath}/${strBuildVM}/${strLibCSrcPath}");
                        }

                        if ($bMakeRebuild)
                        {
                            executeTest(
                                ($bContainerExists ? "docker exec -i test-build bash -c '" : '') .
                                "cd ${strBuildPath} && perl Makefile.PL INSTALLMAN1DIR=none INSTALLMAN3DIR=none" .
                                ($bContainerExists ? "'" : ''),
                                {bSuppressStdErr => true, bShowOutputAsync => $bLogDetail});
                        }

                        executeTest(
                            ($bContainerExists ? 'docker exec -i test-build ' : '') .
                                "make --silent --directory ${strBuildPath}",
                            {bShowOutputAsync => $bLogDetail});
                        executeTest(
                            ($bContainerExists ? 'docker exec -i test-build ' : '') .
                                "make --silent --directory ${strBuildPath} test",
                            {bShowOutputAsync => $bLogDetail});
                        executeTest(
                            ($bContainerExists ? 'docker exec -i test-build ' : 'sudo ') .
                                "make --silent --directory ${strBuildPath} install",
                            {bShowOutputAsync => $bLogDetail});

                        if ($bContainerExists)
                        {
                            executeTest("docker rm -f test-build");
                        }

                        if ($strBuildVM eq $strVmHost)
                        {
                            executeTest("sudo make -C ${strBuildPath} install", {bSuppressStdErr => true});

                            # Load the module dynamically
                            require pgBackRest::LibC;
                            pgBackRest::LibC->import(qw(:debug));

                            require XSLoader;
                            XSLoader::load('pgBackRest::LibC', '999');

                            # Do a basic test to make sure it installed correctly
                            if (libcUvSize() != 8)
                            {
                                confess &log(ERROR, 'UVSIZE in test library does not equal 8');
                            }
                        }
                    }
                }

                # Write files to indicate the last time a build was successful
                $oStorageBackRest->put($strLibCSmart);
                utime($lTimestampLast, $lTimestampLast, $strLibCSmart) or
                    confess "unable to set time for ${strLibCSmart}" . (defined($!) ? ":$!" : '');
            }

            # Build the binary
            #-----------------------------------------------------------------------------------------------------------------------
            if ($bBinRequired)
            {
                my $strBinPath = "${strVagrantPath}/bin";
                my @stryBinSrcPath = ('src', 'libc');

                # Find the lastest modified time for dirs that affect the bin build
                $lTimestampLast = buildLastModTime($oStorageBackRest, $strBackRestBase, \@stryBinSrcPath);

                # Loop through VMs to do the C bin builds
                my $bLogDetail = $strLogLevel eq 'detail';
                my @stryBuildVm = $strVm eq VM_ALL ? VM_LIST : ($strVm);

                foreach my $strBuildVM (@stryBuildVm)
                {
                    my $strBuildPath = "${strBinPath}/${strBuildVM}/src";
                    my $bRebuild = !$bSmart;

                    # Rebuild if the modification time of the smart file does equal the last changes in source paths
                    if ($bSmart)
                    {
                        my $strBinSmart = "${strBuildPath}/pgbackrest";

                        if (!$oStorageBackRest->exists($strBinSmart) ||
                            $oStorageBackRest->info($strBinSmart)->mtime < $lTimestampLast)
                        {
                            &log(INFO, "    bin dependencies have changed for ${strBuildVM}, rebuilding...");

                            $bRebuild = true;
                        }
                    }

                    if ($bRebuild)
                    {
                        &log(INFO, "    build bin for ${strBuildVM} (${strBuildPath})");

                        executeTest(
                            "docker run -itd -h test-build --name=test-build" .
                            " -v ${strBackRestBase}:${strBackRestBase} " . containerRepo() . ":${strBuildVM}-build",
                            {bSuppressStdErr => true});

                        foreach my $strBinSrcPath (@stryBinSrcPath)
                        {
                            $oStorageBackRest->pathCreate(
                                "${strBinPath}/${strBuildVM}/${strBinSrcPath}", {bIgnoreExists => true, bCreateParent => true});
                        }

                        executeTest(
                            "rsync -rt" . (!$bSmart ? " --delete-excluded" : '') .
                            " --include=" . join('/*** --include=', @stryBinSrcPath) . '/*** --exclude=*' .
                            " ${strBackRestBase}/ ${strBinPath}/${strBuildVM}");

                        if (vmCoverage($strVm) && !$bNoLint)
                        {
                            &log(INFO, "    clang static analyzer ${strBuildVM} (${strBuildPath})");
                        }

                        my $strCExtra =
                            "'-g" . (vmWithBackTrace($strBuildVM) && $bNoLint && $bBackTrace ? ' -DWITH_BACKTRACE' : '') . "'";
                        my $strLdExtra = vmWithBackTrace($strBuildVM) && $bNoLint && $bBackTrace  ? '-lbacktrace' : '';
                        my $strCDebug = vmDebugIntegration($strBuildVM) ? 'CDEBUG=' : '';

                        executeTest(
                            'docker exec -i test-build' .
                            (vmCoverage($strVm) && !$bNoLint ? ' scan-build-5.0' : '') .
                            " make --silent --directory ${strBuildPath} CEXTRA=${strCExtra} LDEXTRA=${strLdExtra} ${strCDebug}",
                            {bShowOutputAsync => $bLogDetail});

                        executeTest("docker rm -f test-build");
                    }
                }
            }

            # Build the package
            #-----------------------------------------------------------------------------------------------------------------------
            if (!$bNoPackage)
            {
                my $strPackagePath = "${strVagrantPath}/package";
                my $strPackageSmart = "${strPackagePath}/build.timestamp";
                my @stryPackageSrcPath = ('lib');

                # Find the lastest modified time for additional dirs that affect the package build
                foreach my $strPackageSrcPath (@stryPackageSrcPath)
                {
                    my $hManifest = $oStorageBackRest->manifest($strPackageSrcPath);

                    foreach my $strFile (sort(keys(%{$hManifest})))
                    {
                        if ($hManifest->{$strFile}{type} eq 'f' && $hManifest->{$strFile}{modification_time} > $lTimestampLast)
                        {
                            $lTimestampLast = $hManifest->{$strFile}{modification_time};
                        }
                    }
                }

                # Rebuild if the modification time of the smart file does not equal the last changes in source paths
                if ((!$bSmart || !$oStorageBackRest->exists($strPackageSmart) ||
                     $oStorageBackRest->info($strPackageSmart)->mtime < $lTimestampLast))
                {
                    if ($bSmart)
                    {
                        &log(INFO, 'package dependencies have changed, rebuilding...');
                    }

                    executeTest("sudo rm -rf ${strPackagePath}");
                }

                # Loop through VMs to do the package builds
                my @stryBuildVm = $strVm eq VM_ALL ? VM_LIST : ($strVm);
                $oStorageBackRest->pathCreate($strPackagePath, {bIgnoreExists => true, bCreateParent => true});

                foreach my $strBuildVM (@stryBuildVm)
                {
                    my $strBuildPath = "${strPackagePath}/${strBuildVM}/src";

                    if (!$oStorageBackRest->pathExists($strBuildPath) && $oVm->{$strBuildVM}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
                    {
                        &log(INFO, "build package for ${strBuildVM} (${strBuildPath})");

                        executeTest(
                            "docker run -itd -h test-build --name=test-build" .
                            " -v ${strBackRestBase}:${strBackRestBase} " . containerRepo() . ":${strBuildVM}-build",
                            {bSuppressStdErr => true});

                        $oStorageBackRest->pathCreate($strBuildPath, {bIgnoreExists => true, bCreateParent => true});

                        executeTest("rsync -r --exclude .vagrant --exclude .git ${strBackRestBase}/ ${strBuildPath}/");
                        executeTest(
                            "docker exec -i test-build " .
                            "bash -c 'cp -r /root/package-src/debian ${strBuildPath}' && sudo chown -R " . TEST_USER .
                            " ${strBuildPath}");

                        # Patch files in debian package builds
                        #
                        # Use these commands to create a new patch (may need to modify first line):
                        # BRDIR=/backrest;BRVM=u16;BRPATCHFILE=${BRDIR?}/test/patch/debian-package.patch
                        # DBDIR=${BRDIR?}/test/.vagrant/package/${BRVM}/src/debian
                        # diff -Naur ${DBDIR?}.old ${DBDIR}.new > ${BRPATCHFILE?}
                        my $strDebianPackagePatch = "${strBackRestBase}/test/patch/debian-package.patch";

                        if ($oStorageBackRest->exists($strDebianPackagePatch))
                        {
                            executeTest("cp -r ${strBuildPath}/debian ${strBuildPath}/debian.old");
                            executeTest("patch -d ${strBuildPath}/debian < ${strDebianPackagePatch}");
                            executeTest("cp -r ${strBuildPath}/debian ${strBuildPath}/debian.new");
                        }

                        # If dev build then disable static release date used for reproducibility
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
                            "pgbackrest (${strVersionBase}-0." . ($bVersionDev ? 'D' : 'P') . strftime("%Y%m%d%H%M%S", gmtime) .
                                ") experimental; urgency=medium\n" .
                            "\n" .
                            '  * Automated experimental ' . ($bVersionDev ? 'development' : 'production') . " build.\n" .
                            "\n" .
                            ' -- David Steele <david@pgbackrest.org>  ' . strftime("%a, %e %b %Y %H:%M:%S %z", gmtime) . "\n\n" .
                            ${$oStorageBackRest->get("${strBuildPath}/debian/changelog")});

                        executeTest(
                            "docker exec -i test-build " .
                            "bash -c 'cd ${strBuildPath} && debuild -i -us -uc -b'");

                        executeTest(
                            "docker exec -i test-build " .
                            "bash -c 'rm -f ${strPackagePath}/${strBuildVM}/*.build ${strPackagePath}/${strBuildVM}/*.changes" .
                            " ${strPackagePath}/${strBuildVM}/pgbackrest-doc*'");

                        executeTest("docker rm -f test-build");
                    }

                    if (!$oStorageBackRest->pathExists($strBuildPath) && $oVm->{$strBuildVM}{&VM_OS_BASE} eq VM_OS_BASE_RHEL)
                    {
                        &log(INFO, "build package for ${strBuildVM} (${strBuildPath})");

                        # Create build container
                        executeTest(
                            "docker run -itd -h test-build --name=test-build" .
                            " -v ${strBackRestBase}:${strBackRestBase} " . containerRepo() . ":${strBuildVM}-build",
                            {bSuppressStdErr => true});

                        # Create build directories
                        $oStorageBackRest->pathCreate($strBuildPath, {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/SOURCES", {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/SPECS", {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/RPMS", {bIgnoreExists => true, bCreateParent => true});
                        $oStorageBackRest->pathCreate("${strBuildPath}/BUILD", {bIgnoreExists => true, bCreateParent => true});

                        # Copy source files
                        executeTest(
                            "tar --transform='s_^_pgbackrest-release-${strVersionBase}/_'" .
                                " -czf ${strBuildPath}/SOURCES/${strVersionBase}.tar.gz -C ${strBackRestBase}" .
                                " lib libc src LICENSE");

                        # Copy package files
                        executeTest(
                            "docker exec -i test-build bash -c '" .
                            "ln -s ${strBuildPath} /root/rpmbuild && " .
                            "cp /root/package-src/pgbackrest.spec ${strBuildPath}/SPECS && " .
                            "cp /root/package-src/*.patch ${strBuildPath}/SOURCES && " .
                            "sudo chown -R " . TEST_USER . " ${strBuildPath}'");

                        # Patch files in RHEL package builds
                        #
                        # Use these commands to create a new patch (may need to modify first line):
                        # BRDIR=/backrest;BRVM=co7;BRPATCHFILE=${BRDIR?}/test/patch/rhel-package.patch
                        # PKDIR=${BRDIR?}/test/.vagrant/package/${BRVM}/src/SPECS
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
                        $strSpec =~ s/^Version\:.*$/Version\:\t${strVersionBase}/gm;
                        $oStorageBackRest->put("${strBuildPath}/SPECS/pgbackrest.spec", $strSpec);

                        # Build package
                        executeTest(
                            "docker exec -i test-build rpmbuild -v -bb --clean root/rpmbuild/SPECS/pgbackrest.spec",
                            {bSuppressStdErr => true});

                        # Remove build container
                        executeTest("docker rm -f test-build");
                    }
                }

                # Write files to indicate the last time a build was successful
                if (!$bNoPackage)
                {
                    $oStorageBackRest->put($strPackageSmart);
                    utime($lTimestampLast, $lTimestampLast, $strPackageSmart) or
                        confess "unable to set time for ${strPackageSmart}" . (defined($!) ? ":$!" : '');
                }
            }

            # Exit if only testing builds
            exit 0 if $bBuildOnly;
        }

        # Perform static source code analysis
        #---------------------------------------------------------------------------------------------------------------------------
        if (!$bDryRun)
        {
            # Run Perl critic
            if (!$bNoLint && !$bBuildOnly)
            {
                my $strBasePath = dirname(dirname(abs_path($0)));

                &log(INFO, "Performing static code analysis using perlcritic");

                executeTest('perlcritic --quiet --verbose=8 --brutal --top=10' .
                            ' --verbose "[%p] %f: %m at line %l, column %c.  %e.  (Severity: %s)\n"' .
                            " \"--profile=${strBasePath}/test/lint/perlcritic.policy\"" .
                            " ${strBasePath}/lib/*" .
                            " ${strBasePath}/test/test.pl ${strBasePath}/test/lib/*" .
                            " ${strBasePath}/doc/doc.pl ${strBasePath}/doc/lib/*");
            }

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
                        $oStorageTest, $strBackRestBase, $strTestPath, $strCoveragePath, $$oyTestRun[$iTestIdx], $bDryRun, $bVmOut,
                        $iVmIdx, $iVmMax, $iTestIdx, $iTestMax, $strLogLevel, $bLogForce, $bShowOutputAsync, $bNoCleanup, $iRetry,
                        !$bNoValgrind, !$bNoCoverage, !$bNoOptimize, $bBackTrace, $bProfile, !$bNoDebug);
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

        if (vmCoverage($strVm) && !$bNoCoverage && !$bDryRun)
        {
            &log(INFO, 'writing coverage report');
            executeTest("cp -rp ${strCoveragePath} ${strCoveragePath}_temp");
            executeTest(
                "cd ${strCoveragePath}_temp && " .
                LIB_COVER_EXE . " -report json -outputdir ${strBackRestBase}/test/coverage/perl ${strCoveragePath}_temp",
                {bSuppressStdErr => true});
            executeTest("sudo rm -rf ${strCoveragePath}_temp");
            executeTest("sudo cp -rp ${strCoveragePath} ${strCoveragePath}_temp");
            executeTest(
                "cd ${strCoveragePath}_temp && " .
                LIB_COVER_EXE . " -outputdir ${strBackRestBase}/test/coverage/perl ${strCoveragePath}_temp",
                {bSuppressStdErr => true});
            executeTest("sudo rm -rf ${strCoveragePath}_temp");

            # Determine which modules were covered (only check coverage if all tests were successful)
            #-----------------------------------------------------------------------------------------------------------------------
            if ($iTestFail == 0)
            {
                my $hModuleTest;                                        # Everything that was run

                # Build a hash of all modules, tests, and runs that were executed
                foreach my $hTestRun (@{$oyTestRun})
                {
                    # Get coverage for the module
                    my $strModule = $hTestRun->{&TEST_MODULE};
                    my $hModule = testDefModule($strModule);

                    # Get coverage for the test
                    my $strTest = $hTestRun->{&TEST_NAME};
                    my $hTest = testDefModuleTest($strModule, $strTest);

                    # If no tests are listed it means all of them were run
                    if (@{$hTestRun->{&TEST_RUN}} == 0)
                    {
                        $hModuleTest->{$strModule}{$strTest} = true;
                    }
                }

                # Load the results of coverage testing from JSON
                my $oJSON = JSON::PP->new()->allow_nonref();
                my $hCoverageResult = $oJSON->decode(${$oStorageBackRest->get('test/coverage/perl/cover.json')});

                # Now compare against code modules that should have full coverage
                my $hCoverageList = testDefCoverageList();
                my $hCoverageType = testDefCoverageType();
                my $hCoverageActual;

                foreach my $strCodeModule (sort(keys(%{$hCoverageList})))
                {
                    if (@{$hCoverageList->{$strCodeModule}} > 0)
                    {
                        my $iCoverageTotal = 0;

                        foreach my $hTest (@{$hCoverageList->{$strCodeModule}})
                        {
                            if (!defined($hModuleTest->{$hTest->{strModule}}{$hTest->{strTest}}))
                            {
                                next;
                            }

                            $iCoverageTotal++;
                        }

                        if (@{$hCoverageList->{$strCodeModule}} == $iCoverageTotal)
                        {
                            $hCoverageActual->{testRunName($strCodeModule, false)} = $hCoverageType->{$strCodeModule};
                        }
                    }
                }

                if (keys(%{$hCoverageActual}) == 0)
                {
                    &log(INFO, 'no code modules had all tests run required for coverage');
                }

                foreach my $strCodeModule (sort(keys(%{$hCoverageActual})))
                {
                    # If the first char of the module is lower case then it's a c module
                    if (substr($strCodeModule, 0, 1) eq lc(substr($strCodeModule, 0, 1)))
                    {
                        next;
                    }

                    # Create code module path -- where the file is located on disk
                    my $strCodeModulePath = "${strBackRestBase}/lib/" . BACKREST_NAME . "/${strCodeModule}.pm";

                    # Get summary results
                    my $hCoverageResultAll = $hCoverageResult->{'summary'}{$strCodeModulePath}{total};

                    # Try an extra / if the module is not found
                    if (!defined($hCoverageResultAll))
                    {
                        $strCodeModulePath = "/${strCodeModulePath}";
                        $hCoverageResultAll = $hCoverageResult->{'summary'}{$strCodeModulePath}{total};
                    }

                    # If module is marked as having no code
                    if ($hCoverageActual->{$strCodeModule} eq TESTDEF_COVERAGE_NOCODE)
                    {
                        # Error if it really does have coverage
                        if ($hCoverageResultAll)
                        {
                            confess &log(ERROR, "perl module ${strCodeModule} is marked 'no code' but has code");
                        }

                        # Skip to next module
                        next;
                    }

                    if (!defined($hCoverageResultAll))
                    {
                        confess &log(ERROR, "unable to find coverage results for ${strCodeModule}");
                    }

                    # Check that all code has been covered
                    my $iCoverageTotal = $hCoverageResultAll->{total};
                    my $iCoverageUncoverable = coalesce($hCoverageResultAll->{uncoverable}, 0);
                    my $iCoverageCovered = coalesce($hCoverageResultAll->{covered}, 0);

                    if ($hCoverageActual->{$strCodeModule} eq TESTDEF_COVERAGE_FULL)
                    {
                        my $iUncoveredLines = $iCoverageTotal - $iCoverageCovered - $iCoverageUncoverable;

                        if ($iUncoveredLines != 0)
                        {
                            &log(ERROR, "perl module ${strCodeModule} is not fully covered");
                            $iUncoveredCodeModuleTotal++;

                            &log(ERROR, ('-' x 80));
                            executeTest(
                                "/usr/bin/cover -report text ${strCoveragePath} --select ${strBackRestBase}/lib/" .
                                BACKREST_NAME . "/${strCodeModule}.pm",
                                {bShowOutputAsync => true});
                            &log(ERROR, ('-' x 80));
                        }
                    }
                    # Else test how much partial coverage there was
                    elsif ($hCoverageActual->{$strCodeModule} eq TESTDEF_COVERAGE_PARTIAL)
                    {
                        my $iCoveragePercent = int(($iCoverageCovered + $iCoverageUncoverable) * 100 / $iCoverageTotal);

                        if ($iCoveragePercent == 100)
                        {
                            &log(ERROR, "perl module ${strCodeModule} has 100% coverage but is not marked fully covered");
                            $iUncoveredCodeModuleTotal++;
                        }
                    }
                }

                # Generate C coverage with lcov
                #---------------------------------------------------------------------------------------------------------------------------
                my $strLCovFile = "${strBackRestBase}/test/.vagrant/code/all.lcov";

                if ($oStorageBackRest->exists($strLCovFile))
                {
                    executeTest(
                        "genhtml ${strLCovFile} --config-file=${strBackRestBase}/test/src/lcov.conf" .
                            " --prefix=${strBackRestBase}/test/.vagrant/code" .
                            " --output-directory=${strBackRestBase}/test/coverage/c");

                    foreach my $strCodeModule (sort(keys(%{$hCoverageActual})))
                    {
                        # If the first char of the module is upper case then it's a Perl module
                        if (substr($strCodeModule, 0, 1) eq uc(substr($strCodeModule, 0, 1)))
                        {
                            next;
                        }

                        my $strCoverageFile = $strCodeModule;
                        $strCoverageFile =~ s/^module/test/mg;
                        $strCoverageFile = "${strBackRestBase}/test/.vagrant/code/${strCoverageFile}.lcov";

                        my $strCoverage = $oStorageBackRest->get(
                            $oStorageBackRest->openRead($strCoverageFile, {bIgnoreMissing => true}));

                        if (defined($strCoverage) && defined($$strCoverage))
                        {
                            my $iTotalLines = (split(':', ($$strCoverage =~ m/^LF\:.*$/mg)[0]))[1] + 0;
                            my $iCoveredLines = (split(':', ($$strCoverage =~ m/^LH\:.*$/mg)[0]))[1] + 0;

                            my $iTotalBranches = 0;
                            my $iCoveredBranches = 0;

                            if ($$strCoverage =~ /^BRF\:/mg && $$strCoverage =~ /^BRH\:/mg)
                            {
                                # If this isn't here the statements below fail -- huh?
                                my @match = $$strCoverage =~ m/^BRF\:.*$/mg;

                                $iTotalBranches = (split(':', ($$strCoverage =~ m/^BRF\:.*$/mg)[0]))[1] + 0;
                                $iCoveredBranches = (split(':', ($$strCoverage =~ m/^BRH\:.*$/mg)[0]))[1] + 0;
                            }

                            # Generate detail if there is missing coverage
                            my $strDetail = undef;

                            if ($iCoveredLines != $iTotalLines)
                            {
                                $strDetail .= "$iCoveredLines/$iTotalLines lines";
                            }

                            if ($iTotalBranches != $iCoveredBranches)
                            {
                                $strDetail .= (defined($strDetail) ? ', ' : '') . "$iCoveredBranches/$iTotalBranches branches";
                            }

                            if (defined($strDetail))
                            {
                                &log(ERROR, "c module ${strCodeModule} is not fully covered ($strDetail)");
                                $iUncoveredCodeModuleTotal++;
                            }
                        }
                    }
                }
                else
                {
                    executeTest("rm -rf ${strBackRestBase}/test/coverage/c");
                }
            }
        }

        # Print test info and exit
        #---------------------------------------------------------------------------------------------------------------------------
        &log(INFO,
            ($bDryRun ? 'DRY RUN COMPLETED' : 'TESTS COMPLETED') . ($iTestFail == 0 ? ' SUCCESSFULLY' .
                ($iUncoveredCodeModuleTotal == 0 ? '' : " WITH ${iUncoveredCodeModuleTotal} MODULE(S) MISSING COVERAGE") :
            " WITH ${iTestFail} FAILURE(S)") . ($iTestRetry == 0 ? '' : ", ${iTestRetry} RETRY(IES)") .
                ' (' . (time() - $lStartTime) . 's)');

        exit 1 if ($iTestFail > 0 || $iUncoveredCodeModuleTotal > 0);

        exit 0;
    }

    # Load the module dynamically
    require pgBackRest::LibCAuto;
    require pgBackRest::LibC;
    pgBackRest::LibC->import(qw(:debug));

    require XSLoader;
    XSLoader::load('pgBackRest::LibC', '999');

    ################################################################################################################################
    # Runs tests
    ################################################################################################################################
    my $iRun = 0;

    # Create host group for containers
    my $oHostGroup = hostGroupGet();

    # Run the test
    testRun($stryModule[0], $stryModuleTest[0])->process(
        $strVm, $iVmId,                                             # Vm info
        $strBackRestBase,                                           # Base backrest directory
        $strTestPath,                                               # Path where the tests will run
        '/usr/bin/' . BACKREST_EXE,                                 # Path to the backrest executable
        "${strBackRestBase}/bin/" . BACKREST_EXE,                   # Path to the backrest Perl helper
        $strPgVersion ne 'minimal' ? $strPgSqlBin: undef,           # Pg bin path
        $strPgVersion ne 'minimal' ? $strPgVersion: undef,          # Pg version
        $stryModule[0], $stryModuleTest[0], \@iyModuleTestRun,      # Module info
        $bVmOut, $bDryRun, $bNoCleanup, $bLogForce,                 # Test options
        TEST_USER, BACKREST_USER, TEST_GROUP);                      # User/group info

    if (!$bNoCleanup)
    {
        if ($oHostGroup->removeAll() > 0)
        {
            executeTest("sudo rm -rf ${strTestPath}");
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
        syswrite(*STDOUT, $EVAL_ERROR->message() . "\n" . $EVAL_ERROR->trace());
        exit $EVAL_ERROR->code();
    }

    # Else output the unhandled error
    syswrite(*STDOUT, $EVAL_ERROR);
    exit ERROR_UNHANDLED;
};

# It shouldn't be possible to get here
&log(ASSERT, 'execution reached invalid location in ' . __FILE__ . ', line ' . __LINE__);
exit ERROR_ASSERT;
