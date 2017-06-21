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
use lib dirname(dirname($0)) . '/doc/lib';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Storage::Helper;
use pgBackRest::Storage::Posix::Driver;
use pgBackRest::Storage::Local;
use pgBackRest::Version;

use BackRestDoc::Custom::DocCustomRelease;

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
   --db-version         version of postgres to test (all, defaults to minimal)
   --log-force          force overwrite of current test log files
   --no-lint            disable static source code analysis
   --build-only         compile the C library / packages and run tests only
   --coverage-only      only run coverage tests (as a subset of selected tests)
   --smart              perform libc/package builds only when source timestamps have changed
   --no-package         do not build packages
   --no-ci-config       don't overwrite the current continuous integration config
   --dev                --no-lint --smart --no-package --process-max=1
   --expect             --no-lint --smart --no-package --process-max=1 --vm=co7 --db=9.6 --log-force

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
my $strDbVersion = 'minimal';
my $bLogForce = false;
my $strVm;
my $strVmHost = VM_HOST_DEFAULT;
my $bVmBuild = false;
my $bVmForce = false;
my $bNoLint = false;
my $bBuildOnly = false;
my $bCoverageOnly = false;
my $bSmart = false;
my $bNoPackage = false;
my $bNoCiConfig = false;
my $bDev = false;
my $bExpect = false;
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
            'db-version=s' => \$strDbVersion,
            'log-force' => \$bLogForce,
            'no-lint' => \$bNoLint,
            'build-only' => \$bBuildOnly,
            'no-package' => \$bNoPackage,
            'no-ci-config' => \$bNoCiConfig,
            'coverage-only' => \$bCoverageOnly,
            'smart' => \$bSmart,
            'dev' => \$bDev,
            'expect' => \$bExpect,
            'retry=s' => \$iRetry)
    or pod2usage(2);

####################################################################################################################################
# Run in eval block to catch errors
####################################################################################################################################
eval
{
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
    # Update options for --dev
    ################################################################################################################################
    if ($bDev)
    {
        $bNoLint = true;
        $bSmart = true;
        $bNoPackage = true;
    }

    ################################################################################################################################
    # Update options for --expect
    ################################################################################################################################
    if ($bExpect)
    {
        $bNoLint = true;
        $bSmart = true;
        $bNoPackage = true;
        $strVm = VM_CO7;
        $strDbVersion = '9.6';
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
        containerBuild($oStorageBackRest, $strVm, $bVmForce, $strDbVersion);
        exit 0;
    }

    ################################################################################################################################
    # Start VM and run
    ################################################################################################################################
    if (!defined($iVmId))
    {
        # Build CI configuration
        if (!$bNoCiConfig)
        {
            (new pgBackRestTest::Common::CiTest($oStorageBackRest))->process();
        }

        # Load the doc module dynamically since it is not supported on all systems
        require BackRestDoc::Common::Doc;
        BackRestDoc::Common::Doc->import();

        # Make sure version number matches the latest release
        my $strReleaseFile = dirname(dirname(abs_path($0))) . '/doc/xml/release.xml';
        my $oRelease = (new BackRestDoc::Custom::DocCustomRelease(new BackRestDoc::Common::Doc($strReleaseFile)))->releaseLast();
        my $strVersion = $oRelease->paramGet('version');
        my $bVersionDev = false;
        my $strVersionBase = $strVersion;

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

        if (!$bDryRun)
        {
            # Run Perl critic
            if (!$bNoLint && !$bBuildOnly)
            {
                my $strBasePath = dirname(dirname(abs_path($0)));

                &log(INFO, "Performing static code analysis using perl -cw");

                # Check the exe for warnings
                my $strWarning = trim(executeTest("perl -cw ${strBasePath}/bin/pgbackrest 2>&1"));

                if ($strWarning ne "${strBasePath}/bin/pgbackrest syntax OK")
                {
                    confess &log(ERROR, "${strBasePath}/bin/pgbackrest failed syntax check:\n${strWarning}");
                }

                &log(INFO, "Performing static code analysis using perlcritic");

                executeTest('perlcritic --quiet --verbose=8 --brutal --top=10' .
                            ' --verbose "[%p] %f: %m at line %l, column %c.  %e.  (Severity: %s)\n"' .
                            " \"--profile=${strBasePath}/test/lint/perlcritic.policy\"" .
                            " ${strBasePath}/bin/pgbackrest ${strBasePath}/lib/*" .
                            " ${strBasePath}/test/test.pl ${strBasePath}/test/lib/*" .
                            " ${strBasePath}/doc/doc.pl ${strBasePath}/doc/lib/*");
            }

            logFileSet(cwd() . "/test");
        }

        # Clean up
        #-----------------------------------------------------------------------------------------------------------------------
        my $iTestFail = 0;
        my $iTestRetry = 0;
        my $oyProcess = [];
        my $strCoveragePath = "${strTestPath}/cover_db";

        if (!$bDryRun || $bVmOut)
        {
            containerRemove('test-([0-9]+|build)');

            for (my $iVmIdx = 0; $iVmIdx < 8; $iVmIdx++)
            {
                push(@{$oyProcess}, undef);
            }

            executeTest("sudo rm -rf ${strTestPath}/*");
            $oStorageTest->pathCreate($strCoveragePath, {strMode => '0770', bIgnoreMissing => true, bCreateParent => true});
        }

        # Build the C Library and Packages
        #-----------------------------------------------------------------------------------------------------------------------
        if (!$bDryRun)
        {
            # Paths
            my $strVagrantPath = "${strBackRestBase}/test/.vagrant";
            my $strPackagePath = "${strVagrantPath}/package";
            my $strPackageSmart = "${strPackagePath}/build.timestamp";
            my $strLibCPath = "${strVagrantPath}/libc";
            my $strLibCSmart = "${strLibCPath}/build.timestamp";

            # VM Info
            my $oVm = vmGet();

            # Find the lastest modified time in the libc dir
            my $lTimestampLibCLast = 0;

            my $hManifest = $oStorageBackRest->manifest('libc');

            foreach my $strFile (sort(keys(%{$hManifest})))
            {
                if ($hManifest->{$strFile}{type} eq 'f' && $hManifest->{$strFile}{modification_time} > $lTimestampLibCLast)
                {
                    $lTimestampLibCLast = $hManifest->{$strFile}{modification_time};
                }
            }

            # Rebuild if the modification time of the makefile does not equal the latest file in libc
            if (!$bSmart || !$oStorageBackRest->exists($strLibCSmart) ||
                $oStorageBackRest->info($strLibCSmart)->mtime != $lTimestampLibCLast)
            {
                if ($bSmart)
                {
                    &log(INFO, 'libC code has changed, library will be rebuilt');
                }

                executeTest("sudo rm -rf ${strLibCPath}");
                executeTest('sudo rm -rf ' . $oVm->{$strVmHost}{&VMDEF_PERL_ARCH_PATH} . '/auto/pgBackRest/LibC');
                executeTest('sudo rm -rf ' . $oVm->{$strVmHost}{&VMDEF_PERL_ARCH_PATH} . '/pgBackRest');
            }

            # Find the lastest modified time in the bin, lib dirs
            my $lTimestampPackageLast = $lTimestampLibCLast;

            $hManifest = $oStorageBackRest->manifest('bin');

            foreach my $strFile (sort(keys(%{$hManifest})))
            {
                if ($hManifest->{$strFile}{type} eq 'f' && $hManifest->{$strFile}{modification_time} > $lTimestampPackageLast)
                {
                    $lTimestampPackageLast = $hManifest->{$strFile}{modification_time};
                }
            }

            $hManifest = $oStorageBackRest->manifest('lib');

            foreach my $strFile (sort(keys(%{$hManifest})))
            {
                if ($hManifest->{$strFile}{type} eq 'f' && $hManifest->{$strFile}{modification_time} > $lTimestampPackageLast)
                {
                    $lTimestampPackageLast = $hManifest->{$strFile}{modification_time};
                }
            }

            # Rebuild if the modification time of the makefile does not equal the latest file in libc
            if (!$bNoPackage &&
                (!$bSmart || !$oStorageBackRest->exists($strPackageSmart) ||
                 $oStorageBackRest->info($strPackageSmart)->mtime != $lTimestampPackageLast))
            {
                if ($bSmart)
                {
                    &log(INFO, 'libC or Perl code has changed, packages will be rebuilt');
                }

                executeTest("sudo rm -rf ${strPackagePath}");
            }

            # Loop through VMs to do the C Library builds
            my $bLogDetail = $strLogLevel eq 'detail';
            my @stryBuildVm = $strVm eq VM_ALL ? VM_LIST : ($strVm eq $strVmHost ? ($strVm) : ($strVm, $strVmHost));

            foreach my $strBuildVM (sort(@stryBuildVm))
            {
                my $strBuildPath = "${strLibCPath}/${strBuildVM}";
                my $bContainerExists = $strVm eq VM_ALL || $strBuildVM ne $strVmHost;

                if (!$oStorageBackRest->pathExists($strBuildPath))
                {
                    &log(INFO, "Build/test C library for ${strBuildVM} (${strBuildPath})");

                    if ($bContainerExists)
                    {
                        executeTest(
                            "docker run -itd -h test-build --name=test-build" .
                            " -v ${strBackRestBase}:${strBackRestBase} " . containerRepo() . ":${strBuildVM}-build",
                            {bSuppressStdErr => true});
                    }

                    $oStorageBackRest->pathCreate($strBuildPath, {bIgnoreExists => true, bCreateParent => true});
                    executeTest("cp -r ${strBackRestBase}/libc/* ${strBuildPath}");

                    executeTest(
                        ($bContainerExists ? "docker exec -i test-build bash -c '" : '') .
                        "cd ${strBuildPath} && perl Makefile.PL INSTALLMAN1DIR=none INSTALLMAN3DIR=none" .
                        ($bContainerExists ? "'" : ''),
                        {bSuppressStdErr => true, bShowOutputAsync => $bLogDetail});
                    executeTest(
                        ($bContainerExists ? 'docker exec -i test-build ' : '') .
                        "make -C ${strBuildPath}",
                        {bSuppressStdErr => true, bShowOutputAsync => $bLogDetail});
                    executeTest(
                        ($bContainerExists ? 'docker exec -i test-build ' : '') .
                        "make -C ${strBuildPath} test",
                        {bSuppressStdErr => true, bShowOutputAsync => $bLogDetail});
                    executeTest(
                        ($bContainerExists ? 'docker exec -i test-build ' : 'sudo ') .
                        "make -C ${strBuildPath} install",
                        {bSuppressStdErr => true, bShowOutputAsync => $bLogDetail});

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

                        # Do a basic test to make sure it installed correctly
                        if (&UVSIZE != 8)
                        {
                            confess &log(ERROR, 'UVSIZE in C library does not equal 8');
                        }

                        # Also check the version number
                        my $strLibCVersion =
                            BACKREST_VERSION =~ /dev$/ ?
                                substr(BACKREST_VERSION, 0, length(BACKREST_VERSION) - 3) . '.999' : BACKREST_VERSION;

                        if (libCVersion() ne $strLibCVersion)
                        {
                            confess &log(ERROR, $strLibCVersion . ' was expected for LibC version but found ' . libCVersion());
                        }
                    }
                }
            }

            # Write files to indicate the last time a build was successful
            $oStorageBackRest->put($strLibCSmart);
            utime($lTimestampLibCLast, $lTimestampLibCLast, $strLibCSmart) or
                confess "unable to set time for ${strLibCSmart}" . (defined($!) ? ":$!" : '');

            # Loop through VMs to do the package builds
            if (!$bNoPackage)
            {
                my @stryBuildVm = $strVm eq VM_ALL ? VM_LIST : ($strVm);
                $oStorageBackRest->pathCreate($strPackagePath, {bIgnoreExists => true, bCreateParent => true});

                foreach my $strBuildVM (sort(@stryBuildVm))
                {
                    my $strBuildPath = "${strPackagePath}/${strBuildVM}/src";

                    if (!$oStorageBackRest->pathExists($strBuildPath) && $oVm->{$strBuildVM}{&VM_OS_BASE} eq VM_OS_BASE_DEBIAN)
                    {
                        &log(INFO, "Build package for ${strBuildVM} (${strBuildPath})");

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

                        # If dev build then override then disable static release date used for reproducibility.
                        if ($bVersionDev)
                        {
                            my $strRules = ${$oStorageBackRest->get("${strBuildPath}/debian/rules")};

                            $strRules =~ s/\-\-var\=release-date-static\=y/\-\-var\=release-date-static\=n/g;
                            $strRules =~ s/\-\-out\=html \-\-cache\-only/\-\-out\=html \-\-no\-exe/g;

                            $oStorageBackRest->put("${strBuildPath}/debian/rules", $strRules);
                        }

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
                }

                # Write files to indicate the last time a build was successful
                if (!$bNoPackage)
                {
                    $oStorageBackRest->put($strPackageSmart);
                    utime($lTimestampPackageLast, $lTimestampPackageLast, $strPackageSmart) or
                        confess "unable to set time for ${strPackageSmart}" . (defined($!) ? ":$!" : '');
                }
            }

            # Exit if only testing builds
            exit 0 if $bBuildOnly;
        }

        # Determine which tests to run
        #-----------------------------------------------------------------------------------------------------------------------
        my $oyTestRun = testListGet(
            $strVm, \@stryModule, \@stryModuleTest, \@iyModuleTestRun, $strDbVersion, $bCoverageOnly);

        if (@{$oyTestRun} == 0)
        {
            confess &log(ERROR, 'no tests were selected');
        }

        &log(INFO, @{$oyTestRun} . ' test' . (@{$oyTestRun} > 1 ? 's': '') . " selected\n");

        if ($bNoCleanup && @{$oyTestRun} > 1)
        {
            confess &log(ERROR, '--no-cleanup is not valid when more than one test will run')
        }

        # Excecute tests
        if ($bDryRun)
        {
            $iVmMax = 1;
        }

        my $iTestIdx = 0;
        my $iVmTotal;
        my $iTestMax = @{$oyTestRun};
        my $lStartTime = time();
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

                if ($iVmTotal == $iVmMax)
                {
                    waitHiRes(.1);
                }
            }
            while ($iVmTotal == $iVmMax);

            for (my $iVmIdx = 0; $iVmIdx < $iVmMax; $iVmIdx++)
            {
                if (!defined($$oyProcess[$iVmIdx]) && $iTestIdx < @{$oyTestRun})
                {
                    my $oJob = new pgBackRestTest::Common::JobTest(
                        $oStorageTest, $strBackRestBase, $strTestPath, $strCoveragePath, $$oyTestRun[$iTestIdx], $bDryRun, $bVmOut,
                        $iVmIdx, $iVmMax, $iTestIdx, $iTestMax, $strLogLevel, $bLogForce, $bShowOutputAsync, $bNoCleanup, $iRetry);
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
        #-----------------------------------------------------------------------------------------------------------------------
        my $iUncoveredCodeModuleTotal = 0;

        if (vmCoverage($strVm) && !$bDryRun)
        {
            &log(INFO, 'writing coverage report');
            executeTest("rm -rf ${strBackRestBase}/test/coverage");
            executeTest("cp -rp ${strCoveragePath} ${strCoveragePath}_temp");
            executeTest(
                LIB_COVER_EXE . " -report json -outputdir ${strBackRestBase}/test/coverage ${strCoveragePath}_temp",
                {bSuppressStdErr => true});
            executeTest("sudo rm -rf ${strCoveragePath}_temp");
            executeTest("sudo cp -rp ${strCoveragePath} ${strCoveragePath}_temp");
            executeTest(
                LIB_COVER_EXE . " -outputdir ${strBackRestBase}/test/coverage ${strCoveragePath}_temp",
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
                my $hCoverageResult = $oJSON->decode(${$oStorageBackRest->get('test/coverage/cover.json')});

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
                            $hCoverageActual->{$strCodeModule} = $hCoverageType->{$strCodeModule};
                        }
                    }
                }

                if (keys(%{$hCoverageActual}) > 0)
                {
                    &log(INFO, 'test coverage for: ' . join(', ', sort(keys(%{$hCoverageActual}))));
                }
                else
                {
                    &log(INFO, 'no code modules had all tests run required for coverage');
                }

                foreach my $strCodeModule (sort(keys(%{$hCoverageActual})))
                {
                    # Get summary results (??? Need to fix this for coverage testing on bin/pgbackrest since .pm is required)
                    my $hCoverageResultAll =
                        $hCoverageResult->{'summary'}
                            {"${strBackRestBase}/lib/" . BACKREST_NAME . "/${strCodeModule}.pm"}{total};

                    if (!defined($hCoverageResultAll))
                    {
                        confess &log(ERROR, "unable to find coverage results for ${strCodeModule}");
                    }

                    # Check that all code has been covered
                    my $iCoverageTotal = $hCoverageResultAll->{total};
                    my $iCoverageUncoverable = coalesce($hCoverageResultAll->{uncoverable}, 0);
                    my $iCoverageCovered = $hCoverageResultAll->{covered};

                    if ($hCoverageActual->{$strCodeModule} == TESTDEF_COVERAGE_FULL)
                    {
                        my $iUncoveredLines = $iCoverageTotal - $iCoverageCovered - $iCoverageUncoverable;

                        if ($iUncoveredLines != 0)
                        {
                            &log(ERROR, "code module ${strCodeModule} is not fully covered");
                            $iUncoveredCodeModuleTotal++;
                        }
                    }
                    # Else test how much partial coverage where was
                    else
                    {
                        my $iCoveragePercent = int(($iCoverageCovered + $iCoverageUncoverable) * 100 / $iCoverageTotal);

                        if ($iCoveragePercent == 100)
                        {
                            &log(ERROR, "code module ${strCodeModule} has 100% coverage but is not marked fully covered");
                            $iUncoveredCodeModuleTotal++;
                        }
                        else
                        {
                            &log(INFO, "code module ${strCodeModule} has (expected) partial coverage of ${iCoveragePercent}%");
                        }
                    }
                }
            }
        }

        # Print test info and exit
        #-----------------------------------------------------------------------------------------------------------------------
        &log(INFO,
            ($bDryRun ? 'DRY RUN COMPLETED' : 'TESTS COMPLETED') . ($iTestFail == 0 ? ' SUCCESSFULLY' .
                ($iUncoveredCodeModuleTotal == 0 ? '' : " WITH ${iUncoveredCodeModuleTotal} MODULE(S) MISSING COVERAGE") :
            " WITH ${iTestFail} FAILURE(S)") . ($iTestRetry == 0 ? '' : ", ${iTestRetry} RETRY(IES)") .
                ' (' . (time() - $lStartTime) . 's)');

        exit 1 if ($iTestFail > 0 || $iUncoveredCodeModuleTotal > 0);

        exit 0;
    }

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
        "${strBackRestBase}/bin/" . BACKREST_EXE,                   # Path to the backrest executable
        $strDbVersion ne 'minimal' ? $strPgSqlBin: undef,           # Db bin path
        $strDbVersion ne 'minimal' ? $strDbVersion: undef,          # Db version
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
    exit $EVAL_ERROR->code() if (isException($EVAL_ERROR));

    # Else output the unhandled error
    print $EVAL_ERROR;
    exit ERROR_UNHANDLED;
};

# It shouldn't be possible to get here
&log(ASSERT, 'execution reached invalid location in ' . __FILE__ . ', line ' . __LINE__);
exit ERROR_ASSERT;
