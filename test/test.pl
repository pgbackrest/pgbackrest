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

use pgBackRestTest::Backup::BackupTest;
use pgBackRestTest::Backup::Common::HostBackupTest;
use pgBackRestTest::Backup::Common::HostBaseTest;
use pgBackRestTest::Backup::Common::HostDbCommonTest;
use pgBackRestTest::Backup::Common::HostDbTest;
use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::ListTest;
use pgBackRestTest::CommonTest;
use pgBackRestTest::Config::ConfigTest;
use pgBackRestTest::File::FileTest;
use pgBackRestTest::Help::HelpTest;

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
   --no-lint            Disable static source code analysis

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
my $strModule = 'all';
my $strModuleTest = 'all';
my $iModuleTestRun = undef;
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
my $strVm = 'all';
my $bVmBuild = false;
my $bVmForce = false;
my $bNoLint = false;

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
            'module=s' => \$strModule,
            'test=s' => \$strModuleTest,
            'run=s' => \$iModuleTestRun,
            'process-max=s' => \$iProcessMax,
            'vm-id=s' => \$iVmId,
            'vm-max=s' => \$iVmMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'db-version=s' => \$strDbVersion,
            'log-force' => \$bLogForce,
            'no-lint' => \$bNoLint)
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

    if ($strModuleTest ne 'all' && $strModule eq 'all')
    {
        confess "--module must be provided for --test=\"${strModuleTest}\"";
    }

    if (defined($iModuleTestRun) && $strModuleTest eq 'all')
    {
        confess "--test must be provided for --run=\"${iModuleTestRun}\"";
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
            if (!$bNoLint)
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

        # Determine which tests to run
        #-----------------------------------------------------------------------------------------------------------------------
        my $oyTestRun = testListGet($strVm, $strModule, $strModuleTest, $iModuleTestRun, $strDbVersion, $iProcessMax);

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

        if (!$bDryRun || $bVmOut)
        {
            containerRemove('test-[0-9]+');

            for (my $iVmIdx = 0; $iVmIdx < 8; $iVmIdx++)
            {
                push(@{$oyProcess}, undef);
            }

            executeTest("sudo rm -rf ${strTestPath}/*");
            filePathCreate($strTestPath);
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

                            if (!($iExitStatus == 0 || $iExitStatus == 255))
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
                                          (defined($$oTest{&TEST_RUN}) ? ', run=' . $$oTest{&TEST_RUN} : '') .
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
                                " -v ${strHostTestPath}:${strVmTestPath}" .
                                " -v ${strBackRestBase}:${strBackRestBase} " . containerNamespace() . '/' . $$oTest{&TEST_VM} .
                                "-loop-test-pre");
                        }
                    }

                    my $strCommand =
                        ($$oTest{&TEST_CONTAINER} ? 'docker exec -i -u ' . TEST_USER . " ${strImage} " : '') . abs_path($0) .
                        " --test-path=${strVmTestPath}" .
                        " --vm=$$oTest{&TEST_VM}" .
                        " --vm-id=${iVmIdx}" .
                        " --module=" . $$oTest{&TEST_MODULE} .
                        ' --test=' . $$oTest{&TEST_NAME} .
                        (defined($$oTest{&TEST_RUN}) ? ' --run=' . $$oTest{&TEST_RUN} : '') .
                        (defined($$oTest{&TEST_DB}) ? ' --db-version=' . $$oTest{&TEST_DB} : '') .
                        (defined($$oTest{&TEST_PROCESS}) ? ' --process-max=' . $$oTest{&TEST_PROCESS} : '') .
                        ($strLogLevel ne lc(INFO) ? " --log-level=${strLogLevel}" : '') .
                        ' --pgsql-bin=' . $$oTest{&TEST_PGSQL_BIN} .
                        ($bLogForce ? ' --log-force' : '') .
                        ($bDryRun ? ' --dry-run' : '') .
                        ($bVmOut ? ' --vm-out' : '') .
                        ($bNoCleanup ? " --no-cleanup" : '');

                    &log(DETAIL, $strCommand);

                    if (!$bDryRun || $bVmOut)
                    {
                        my $fTestStartTime = gettimeofday();

                        filePathCreate($strVmTestPath, '0777', undef, true);

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

    # Set parameters in host group
    my $oHostGroup = hostGroupGet();
    $oHostGroup->paramSet(HOST_PARAM_VM, $strVm);
    $oHostGroup->paramSet(HOST_PARAM_VM_ID, $iVmId);
    $oHostGroup->paramSet(HOST_PARAM_TEST_PATH, $strTestPath);
    $oHostGroup->paramSet(HOST_PARAM_BACKREST_EXE, "${strBackRestBase}/bin/pgbackrest");
    $oHostGroup->paramSet(HOST_PARAM_PROCESS_MAX, $iProcessMax);
    $oHostGroup->paramSet(HOST_DB_USER, TEST_USER);
    $oHostGroup->paramSet(HOST_BACKUP_USER, 'backrest');

    if ($strDbVersion ne 'minimal')
    {
        $oHostGroup->paramSet(HOST_PARAM_DB_VERSION, $strDbVersion);
        $oHostGroup->paramSet(HOST_PARAM_DB_BIN_PATH, $strPgSqlBin);
    }

    if (testSetup($strTestPath, $strPgSqlBin, $iModuleTestRun,
                                 $bDryRun, $bNoCleanup, $bLogForce))
    {
        if (!$bVmOut &&
            ($strModule eq 'all' ||
             $strModule eq 'backup' && $strModuleTest eq 'all' ||
             $strModule eq 'backup' && $strModuleTest eq 'full'))
        {
            &log(INFO, "TESTING psql-bin = $strPgSqlBin\n");
        }

        if ($strModule eq 'all' || $strModule eq 'help')
        {
            helpTestRun($strModuleTest, $bVmOut);
        }

        if ($strModule eq 'all' || $strModule eq 'config')
        {
            configTestRun($strModuleTest, $bVmOut);
        }

        if ($strModule eq 'all' || $strModule eq 'file')
        {
            fileTestRun($strModuleTest, $bVmOut);
        }

        if ($strModule eq 'all' || $strModule eq 'backup')
        {
            backupTestRun($strModuleTest, $bVmOut);
        }
    }

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
