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
use Pod::Usage qw(pod2usage);
use POSIX qw(ceil);
use Time::HiRes qw(gettimeofday);

use lib dirname($0) . '/lib';
use lib dirname($0) . '/../lib';
use lib dirname($0) . '/../doc/lib';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::FileCommon;
use pgBackRest::Version;

use BackRestDoc::Custom::DocCustomRelease;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
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
   --process-max        max processes to run for compression/transfer (default 1)
   --dry-run            show only the tests that would be executed but don't execute them
   --no-cleanup         don't cleaup after the last test is complete - useful for debugging
   --db-version         version of postgres to test (all, defaults to minimal)
   --log-force          force overwrite of current test log files
   --no-lint            disable static source code analysis
   --libc-only          compile the C library and run tests only
   --coverage           perform coverage analysis

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
my $strLogLevel = 'info';
my $bVmOut = false;
my @stryModule;
my @stryModuleTest;
my @iyModuleTestRun;
my $iProcessMax = undef;
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
my $strVm = VM_ALL;
my $bVmBuild = false;
my $bVmForce = false;
my $bNoLint = false;
my $bLibCOnly = false;
my $bCoverage = false;

GetOptions ('q|quiet' => \$bQuiet,
            'version' => \$bVersion,
            'help' => \$bHelp,
            'pgsql-bin=s' => \$strPgSqlBin,
            'test-path=s' => \$strTestPath,
            'log-level=s' => \$strLogLevel,
            'vm=s' => \$strVm,
            'vm-out' => \$bVmOut,
            'vm-build' => \$bVmBuild,
            'vm-force' => \$bVmForce,
            'module=s@' => \@stryModule,
            'test=s@' => \@stryModuleTest,
            'run=s@' => \@iyModuleTestRun,
            'process-max=s' => \$iProcessMax,
            'vm-id=s' => \$iVmId,
            'vm-max=s' => \$iVmMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'db-version=s' => \$strDbVersion,
            'log-force' => \$bLogForce,
            'no-lint' => \$bNoLint,
            'libc-only' => \$bLibCOnly,
            'coverage' => \$bCoverage)
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

    # Check process total
    if (defined($iProcessMax) && ($iProcessMax < 1 || $iProcessMax > OPTION_DEFAULT_PROCESS_MAX_MAX))
    {
        confess 'process-max must be between 1 and ' . OPTION_DEFAULT_PROCESS_MAX_MAX;
    }

    # Set test path if not expicitly set
    if (!defined($strTestPath))
    {
        $strTestPath = cwd() . '/test';
    }

    # Coverage can only be run with u16 containers due to version compatibility issues
    if ($bCoverage)
    {
        if ($strVm eq VM_ALL)
        {
            &log(INFO, 'Set --vm=' . VM_U16 . ' for coverage testing');
            $strVm = VM_U16;
        }
        elsif ($strVm ne VM_U16)
        {
            confess &log(ERROR, 'only --vm=' . VM_U16 . ' can be used for coverage testing');
        }
    }

    # Get the base backrest path
    my $strBackRestBase = dirname(dirname(abs_path($0)));

    ################################################################################################################################
    # Build Docker containers
    ################################################################################################################################
    if ($bVmBuild)
    {
        containerBuild($strVm, $bVmForce, $strDbVersion);
        exit 0;
    }

    ################################################################################################################################
    # Start VM and run
    ################################################################################################################################
    if (!defined($iVmId))
    {
        # Load the doc module dynamically since it is not supported on all systems
        require BackRestDoc::Common::Doc;
        BackRestDoc::Common::Doc->import();

        # Make sure version number matches the latest release
        my $strReleaseFile = dirname(dirname(abs_path($0))) . '/doc/xml/release.xml';
        my $oRelease = (new BackRestDoc::Custom::DocCustomRelease(new BackRestDoc::Common::Doc($strReleaseFile)))->releaseLast();
        my $strVersion = $oRelease->paramGet('version');

        if ($strVersion =~ /dev$/ && BACKREST_VERSION !~ /dev$/)
        {
            if ($oRelease->nodeTest('release-core-list'))
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
            if (!$bNoLint && !$bLibCOnly)
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

        # Build the C Library in host
        #-----------------------------------------------------------------------------------------------------------------------
        if (!$bDryRun)
        {
            my $bLogDetail = $strLogLevel eq 'detail';
            my $strBuildBasePath = "${strBackRestBase}/test/.vagrant/libc";
            my $strBuildPath = "${strBuildBasePath}/host";
            my $strMakeFile = "${strBuildPath}/Makefile.PL";
            my $strLibCPath = "${strBackRestBase}/libc";

            # Find the lastest modified time in the libc dir
            my $hManifest = fileManifest($strLibCPath);
            my $lTimestampLast = 0;

            foreach my $strFile (sort(keys(%{$hManifest})))
            {
                if ($hManifest->{$strFile}{type} eq 'f' && $hManifest->{$strFile}{modification_time} > $lTimestampLast)
                {
                    $lTimestampLast = $hManifest->{$strFile}{modification_time};
                }
            }

            # Rebuild if the modification time of the makefile does not equal the latest file in libc
            if (!fileExists($strMakeFile) || fileStat($strMakeFile)->mtime != $lTimestampLast)
            {
                &log(INFO, "Build/test/install C library for host (${strBuildPath})");

                executeTest("sudo rm -rf ${strBuildBasePath}");
                filePathCreate($strBuildPath, undef, true, true);
                executeTest("cp -rp ${strLibCPath}/* ${strBuildPath}");
                utime($lTimestampLast, $lTimestampLast, $strMakeFile) or
                    confess "unable to set time for ${strMakeFile}" . (defined($!) ? ":$!" : '');

                executeTest(
                    "cd ${strBuildPath} && perl Makefile.PL INSTALLMAN1DIR=none INSTALLMAN3DIR=none",
                    {bShowOutputAsync => $bLogDetail});
                executeTest("make -C ${strBuildPath}", {bSuppressStdErr => true, bShowOutputAsync => $bLogDetail});
                executeTest("sudo make -C ${strBuildPath} install", {bShowOutputAsync => $bLogDetail});
                executeTest("cd ${strBuildPath} && perl t/pgBackRest-LibC.t", {bShowOutputAsync => $bLogDetail});

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

            # Exit if only testing the C library
            exit 0 if $bLibCOnly;
        }

        # Determine which tests to run
        #-----------------------------------------------------------------------------------------------------------------------
        my $oyTestRun = testListGet(
            $strVm, \@stryModule, \@stryModuleTest, \@iyModuleTestRun, $strDbVersion, $iProcessMax, $bCoverage);

        if (@{$oyTestRun} == 0)
        {
            confess &log(ERROR, 'no tests were selected');
        }

        &log(INFO, @{$oyTestRun} . ' test' . (@{$oyTestRun} > 1 ? 's': '') . " selected\n");

        if ($bNoCleanup && @{$oyTestRun} > 1)
        {
            confess &log(ERROR, '--no-cleanup is not valid when more than one test will run')
        }

        # Execute tests
        #-----------------------------------------------------------------------------------------------------------------------
        my $iTestFail = 0;
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
            filePathCreate($strCoveragePath, '0770', true, true);
        }

        # Build the C Library in container
        #-----------------------------------------------------------------------------------------------------------------------
        if (!$bDryRun)
        {
            my $bLogDetail = $strLogLevel eq 'detail';
            my @stryBuildVm = $strVm eq VM_ALL ? (VM_CO6, VM_U16, VM_D8, VM_CO7, VM_U14, VM_U12) : ($strVm);

            foreach my $strBuildVM (sort(@stryBuildVm))
            {
                my $strBuildPath = "${strBackRestBase}/test/.vagrant/libc/${strBuildVM}";

                if (!fileExists($strBuildPath))
                {
                    executeTest(
                        "docker run -itd -h test-build --name=test-build" .
                        " -v ${strBackRestBase}:${strBackRestBase} " . containerNamespace() . "/${strBuildVM}-build");

                    &log(INFO, "Build/test C library for ${strBuildVM} (${strBuildPath})");

                    filePathCreate($strBuildPath, undef, true, true);
                    executeTest("cp -rp ${strBackRestBase}/libc/* ${strBuildPath}");

                    executeTest(
                        "docker exec -i test-build " .
                        "bash -c 'cd ${strBuildPath} && perl Makefile.PL INSTALLMAN1DIR=none INSTALLMAN3DIR=none'");
                    executeTest(
                        "docker exec -i test-build " .
                        "make -C ${strBuildPath}", {bSuppressStdErr => true});
                    executeTest(
                        "docker exec -i test-build " .
                        "make -C ${strBuildPath} test");
                    executeTest(
                        "docker exec -i test-build " .
                        "make -C ${strBuildPath} install", {bShowOutputAsync => $bLogDetail});

                    executeTest("docker rm -f test-build");
                }
            }
        }

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
                        my $oExecDone = $$oyProcess[$iVmIdx]{exec};
                        my $strTestDone = $$oyProcess[$iVmIdx]{test};
                        my $iTestDoneIdx = $$oyProcess[$iVmIdx]{idx};

                        my $iExitStatus = $oExecDone->end(undef, $iVmMax == 1);

                        if (defined($iExitStatus))
                        {
                            if ($bShowOutputAsync)
                            {
                                syswrite(*STDOUT, "\n");
                            }

                            my $fTestElapsedTime = ceil((gettimeofday() - $$oyProcess[$iVmIdx]{start_time}) * 100) / 100;

                            if ($iExitStatus != 0)
                            {
                                &log(ERROR, "${strTestDone} (err${iExitStatus}-${fTestElapsedTime}s)" .
                                     (defined($oExecDone->{strOutLog}) && !$bShowOutputAsync ?
                                        ":\n\n" . trim($oExecDone->{strOutLog}) . "\n" : ''), undef, undef, 4);
                                $iTestFail++;
                            }
                            else
                            {
                                &log(INFO, "${strTestDone} (${fTestElapsedTime}s)".
                                     ($bVmOut && !$bShowOutputAsync ?
                                         ":\n\n" . trim($oExecDone->{strOutLog}) . "\n" : ''), undef, undef, 4);
                            }

                            if (!$bNoCleanup)
                            {
                                my $strImage = 'test-' . $iVmIdx;
                                my $strHostTestPath = "${strTestPath}/${strImage}";

                                containerRemove("test-${iVmIdx}");
                                executeTest("sudo rm -rf ${strHostTestPath}");
                            }

                            $$oyProcess[$iVmIdx] = undef;
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
                    my $oTest = $$oyTestRun[$iTestIdx];
                    $iTestIdx++;

                    my $strTest = sprintf('P%0' . length($iVmMax) . 'd-T%0' . length($iTestMax) . 'd/%0' .
                                          length($iTestMax) . "d - ", $iVmIdx, $iTestIdx, $iTestMax) .
                                          'vm=' . $$oTest{&TEST_VM} .
                                          ', module=' . $$oTest{&TEST_MODULE} .
                                          ', test=' . $$oTest{&TEST_NAME} .
                                          (defined($$oTest{&TEST_RUN}) ? ', run=' . join(',', @{$$oTest{&TEST_RUN}}) : '') .
                                          (defined($$oTest{&TEST_PROCESS}) ? ', process-max=' . $$oTest{&TEST_PROCESS} : '') .
                                          (defined($$oTest{&TEST_DB}) ? ', db=' . $$oTest{&TEST_DB} : '');

                    my $strImage = 'test-' . $iVmIdx;
                    my $strDbVersion = (defined($$oTest{&TEST_DB}) ? $$oTest{&TEST_DB} : PG_VERSION_94);
                    $strDbVersion =~ s/\.//;

                    &log($bDryRun && !$bVmOut || $bShowOutputAsync ? INFO : DETAIL, "${strTest}" .
                         ($bVmOut || $bShowOutputAsync ? "\n" : ''));

                    my $strVmTestPath = '/home/' . TEST_USER . "/test/${strImage}";
                    my $strHostTestPath = "${strTestPath}/${strImage}";

                    # Don't create the container if this is a dry run unless output from the VM is required.  Ouput can be requested
                    # to get more information about the specific tests that will be run.
                    if (!$bDryRun || $bVmOut)
                    {
                        # Create host test directory
                        filePathCreate($strHostTestPath, '0770');

                        if ($$oTest{&TEST_CONTAINER})
                        {
                            executeTest(
                                'docker run -itd -h ' . $$oTest{&TEST_VM} . "-test --name=${strImage}" .
                                " -v ${strCoveragePath}:${strCoveragePath} " .
                                " -v ${strHostTestPath}:${strVmTestPath}" .
                                " -v ${strBackRestBase}:${strBackRestBase} " .
                                containerNamespace() . '/' . $$oTest{&TEST_VM} .
                                "-loop-test-pre");
                        }
                    }

                    # Create run parameters
                    my $strCommandRunParam = '';

                    foreach my $iRunIdx (@{$$oTest{&TEST_RUN}})
                    {
                        $strCommandRunParam .= ' --run=' . $iRunIdx;
                    }

                    # Create command
                    my $strCommand =
                        ($$oTest{&TEST_CONTAINER} ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') .
                        ($bCoverage ? testRunExe(
                            abs_path($0), dirname($strCoveragePath), $strBackRestBase, $$oTest{&TEST_MODULE},
                            $$oTest{&TEST_NAME}, defined($$oTest{&TEST_RUN}) ? $$oTest{&TEST_RUN} : 'all') :
                            abs_path($0)) .
                        " --test-path=${strVmTestPath}" .
                        " --vm=$$oTest{&TEST_VM}" .
                        " --vm-id=${iVmIdx}" .
                        " --module=" . $$oTest{&TEST_MODULE} .
                        ' --test=' . $$oTest{&TEST_NAME} .
                        $strCommandRunParam .
                        (defined($$oTest{&TEST_DB}) ? ' --db-version=' . $$oTest{&TEST_DB} : '') .
                        (defined($$oTest{&TEST_PROCESS}) ? ' --process-max=' . $$oTest{&TEST_PROCESS} : '') .
                        ($strLogLevel ne lc(INFO) ? " --log-level=${strLogLevel}" : '') .
                        ' --pgsql-bin=' . $$oTest{&TEST_PGSQL_BIN} .
                        ($bCoverage ? ' --coverage' : '') .
                        ($bLogForce ? ' --log-force' : '') .
                        ($bDryRun ? ' --dry-run' : '') .
                        ($bVmOut ? ' --vm-out' : '') .
                        ($bNoCleanup ? " --no-cleanup" : '');

                    &log(DETAIL, $strCommand);

                    if (!$bDryRun || $bVmOut)
                    {
                        my $fTestStartTime = gettimeofday();

                        # Set permissions on the Docker test directory.  This can be removed once users/groups are sync'd between
                        # Docker and the host VM.
                        if ($$oTest{&TEST_CONTAINER})
                        {
                            executeTest("docker exec ${strImage} chown " . TEST_USER . ":postgres -R ${strVmTestPath}");
                        }

                        my $oExec = new pgBackRestTest::Common::ExecuteTest(
                            $strCommand,
                            {bSuppressError => true, bShowOutputAsync => $bShowOutputAsync});

                        $oExec->begin();

                        my $oProcess =
                        {
                            exec => $oExec,
                            test => $strTest,
                            idx => $iTestIdx,
                            container => $$oTest{&TEST_CONTAINER},
                            start_time => $fTestStartTime
                        };

                        $$oyProcess[$iVmIdx] = $oProcess;
                    }

                    $iVmTotal++;
                }
            }
        }
        while ($iVmTotal > 0);

        # Write out coverage info
        #-----------------------------------------------------------------------------------------------------------------------
        if ($bCoverage)
        {
            &log(INFO, 'Writing coverage report');
            executeTest("rm -rf ${strBackRestBase}/test/coverage");
            executeTest("cp -rp ${strCoveragePath} ${strCoveragePath}_temp");
            executeTest("cover -report json -outputdir ${strBackRestBase}/test/coverage ${strCoveragePath}_temp");
            executeTest("rm -rf ${strCoveragePath}_temp");
            executeTest("cp -rp ${strCoveragePath} ${strCoveragePath}_temp");
            executeTest("cover -outputdir ${strBackRestBase}/test/coverage ${strCoveragePath}_temp");
            executeTest("rm -rf ${strCoveragePath}_temp");
        }

        # Print test info and exit
        #-----------------------------------------------------------------------------------------------------------------------
        if ($bDryRun)
        {
            &log(INFO, 'DRY RUN COMPLETED');
        }
        else
        {
            &log(INFO, 'TESTS COMPLETED ' . ($iTestFail == 0 ? 'SUCCESSFULLY' : "WITH ${iTestFail} FAILURE(S)") .
                       ' (' . (time() - $lStartTime) . 's)');
        }

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
        $iProcessMax, $bVmOut, $bDryRun, $bNoCleanup, $bLogForce,   # Test options
        $bCoverage,                                                 # Test options
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
