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

# Convert die to confess to capture the stack trace
$SIG{__DIE__} = sub { Carp::confess @_ };

use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions);
use Cwd qw(abs_path cwd);
use Pod::Usage qw(pod2usage);
use POSIX qw(ceil);
use Time::HiRes qw(gettimeofday);
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Db;
use pgBackRest::FileCommon;
use pgBackRest::Version;

use lib dirname($0) . '/../doc/lib';
use BackRestDoc::Custom::DocCustomRelease;

use lib dirname($0) . '/lib';
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
   --thread-max         max threads to run for backup/restore (default 4)
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
   --process-max        max VMs to run in parallel (default 1)

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
my $iThreadMax = undef;
my $iProcessMax = 1;
my $iProcessId = undef;
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
            'thread-max=s' => \$iThreadMax,
            'process-id=s' => \$iProcessId,
            'process-max=s' => \$iProcessMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'db-version=s' => \$strDbVersion,
            'log-force' => \$bLogForce,
            'no-lint' => \$bNoLint)
    or pod2usage(2);

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

####################################################################################################################################
# Setup
####################################################################################################################################
# Set a neutral umask so tests work as expected
umask(0);

# Set console log level
if ($bQuiet)
{
    $strLogLevel = 'off';
}

logLevelSet(uc($strLogLevel), uc($strLogLevel));

if ($strModuleTest ne 'all' && $strModule eq 'all')
{
    confess "--module must be provided for --test=\"${strModuleTest}\"";
}

if (defined($iModuleTestRun) && $strModuleTest eq 'all')
{
    confess "--test must be provided for --run=\"${iModuleTestRun}\"";
}

# Check thread total
if (defined($iThreadMax) && ($iThreadMax < 1 || $iThreadMax > 32))
{
    confess 'thread-max must be between 1 and 32';
}

# Set test path if not expicitly set
if (!defined($strTestPath))
{
    $strTestPath = cwd() . '/test';
}

# Get the base backrest path
my $strBackRestBase = dirname(dirname(abs_path($0)));

####################################################################################################################################
# Build Docker containers
####################################################################################################################################
if ($bVmBuild)
{
    containerBuild($strVm, $bVmForce, $strDbVersion);
    exit 0;
}

eval
{
    ################################################################################################################################
    # Start VM and run
    ################################################################################################################################
    if (!defined($iProcessId))
    {
        # Load the doc module dynamically since it is not supported on all systems
        use lib dirname(abs_path($0)) . '/../doc/lib';
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
        my $oyTestRun = testListGet($strVm, $strModule, $strModuleTest, $iModuleTestRun, $strDbVersion, $iThreadMax);

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

            for (my $iProcessIdx = 0; $iProcessIdx < 8; $iProcessIdx++)
            {
                push(@{$oyProcess}, undef);
            }

            executeTest("sudo rm -rf ${strTestPath}/*");
            filePathCreate($strTestPath);
        }

        if ($bDryRun)
        {
            $iProcessMax = 1;
        }

        my $iTestIdx = 0;
        my $iProcessTotal;
        my $iTestMax = @{$oyTestRun};
        my $lStartTime = time();
        my $bShowOutputAsync = $bVmOut && (@{$oyTestRun} == 1 || $iProcessMax == 1) && ! $bDryRun ? true : false;

        do
        {
            do
            {
                $iProcessTotal = 0;

                for (my $iProcessIdx = 0; $iProcessIdx < $iProcessMax; $iProcessIdx++)
                {
                    if (defined($$oyProcess[$iProcessIdx]))
                    {
                        my $oExecDone = $$oyProcess[$iProcessIdx]{exec};
                        my $strTestDone = $$oyProcess[$iProcessIdx]{test};
                        my $iTestDoneIdx = $$oyProcess[$iProcessIdx]{idx};

                        my $iExitStatus = $oExecDone->end(undef, $iProcessMax == 1);

                        if (defined($iExitStatus))
                        {
                            if ($bShowOutputAsync)
                            {
                                syswrite(*STDOUT, "\n");
                            }

                            my $fTestElapsedTime = ceil((gettimeofday() - $$oyProcess[$iProcessIdx]{start_time}) * 100) / 100;

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
                                my $strImage = 'test-' . $iProcessIdx;
                                my $strHostTestPath = "${strTestPath}/${strImage}";

                                containerRemove("test-${iProcessIdx}");
                                executeTest("sudo rm -rf ${strHostTestPath}");
                            }

                            $$oyProcess[$iProcessIdx] = undef;
                        }
                        else
                        {
                            $iProcessTotal++;
                        }
                    }
                }

                if ($iProcessTotal == $iProcessMax)
                {
                    waitHiRes(.1);
                }
            }
            while ($iProcessTotal == $iProcessMax);

            for (my $iProcessIdx = 0; $iProcessIdx < $iProcessMax; $iProcessIdx++)
            {
                if (!defined($$oyProcess[$iProcessIdx]) && $iTestIdx < @{$oyTestRun})
                {
                    my $oTest = $$oyTestRun[$iTestIdx];
                    $iTestIdx++;

                    my $strTest = sprintf('P%0' . length($iProcessMax) . 'd-T%0' . length($iTestMax) . 'd/%0' .
                                          length($iTestMax) . "d - ", $iProcessIdx, $iTestIdx, $iTestMax) .
                                          'vm=' . $$oTest{&TEST_VM} .
                                          ', module=' . $$oTest{&TEST_MODULE} .
                                          ', test=' . $$oTest{&TEST_NAME} .
                                          (defined($$oTest{&TEST_RUN}) ? ', run=' . $$oTest{&TEST_RUN} : '') .
                                          (defined($$oTest{&TEST_THREAD}) ? ', thread-max=' . $$oTest{&TEST_THREAD} : '') .
                                          (defined($$oTest{&TEST_DB}) ? ', db=' . $$oTest{&TEST_DB} : '');

                    my $strImage = 'test-' . $iProcessIdx;
                    my $strDbVersion = (defined($$oTest{&TEST_DB}) ? $$oTest{&TEST_DB} : PG_VERSION_94);
                    $strDbVersion =~ s/\.//;

                    &log($bDryRun && !$bVmOut || $bShowOutputAsync ? INFO : DETAIL, "${strTest}" .
                         ($bVmOut || $bShowOutputAsync ? "\n" : ''));

                    my $strVmTestPath = "/home/vagrant/test/${strImage}";
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
                                " -v ${strBackRestBase}:${strBackRestBase} backrest/" . $$oTest{&TEST_VM} . "-loop-test-pre");
                        }
                    }

                    my $strCommand =
                        ($$oTest{&TEST_CONTAINER} ? "docker exec -i -u vagrant ${strImage} " : '') . abs_path($0) .
                        " --test-path=${strVmTestPath}" .
                        " --vm=$$oTest{&TEST_VM}" .
                        " --process-id=${iProcessIdx}" .
                        " --module=" . $$oTest{&TEST_MODULE} .
                        ' --test=' . $$oTest{&TEST_NAME} .
                        (defined($$oTest{&TEST_RUN}) ? ' --run=' . $$oTest{&TEST_RUN} : '') .
                        (defined($$oTest{&TEST_DB}) ? ' --db-version=' . $$oTest{&TEST_DB} : '') .
                        (defined($$oTest{&TEST_THREAD}) ? ' --thread-max=' . $$oTest{&TEST_THREAD} : '') .
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
                            executeTest("docker exec ${strImage} chown vagrant:postgres -R ${strVmTestPath}");
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

                        $$oyProcess[$iProcessIdx] = $oProcess;
                    }

                    $iProcessTotal++;
                }
            }
        }
        while ($iProcessTotal > 0);

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
    $oHostGroup->paramSet(HOST_PARAM_PROCESS_ID, $iProcessId);
    $oHostGroup->paramSet(HOST_PARAM_TEST_PATH, $strTestPath);
    $oHostGroup->paramSet(HOST_PARAM_BACKREST_EXE, "${strBackRestBase}/bin/pgbackrest");
    $oHostGroup->paramSet(HOST_PARAM_THREAD_MAX, $iThreadMax);
    $oHostGroup->paramSet(HOST_DB_MASTER_USER, 'vagrant');
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
                helpTestRun($strModuleTest, undef, $bVmOut);
            }

            if ($strModule eq 'all' || $strModule eq 'config')
            {
                configTestRun($strModuleTest, undef, $bVmOut);
            }

        if ($strModule eq 'all' || $strModule eq 'file')
        {
            fileTestRun($strModuleTest, undef, $bVmOut);
        }

        if ($strModule eq 'all' || $strModule eq 'backup')
        {
            backupTestRun($strModuleTest, 1, $bVmOut);
        }
    }

    if (!$bNoCleanup)
    {
        if ($oHostGroup->removeAll() > 0)
        {
            executeTest("sudo rm -rf ${strTestPath}");
        }
    }
};

if ($@)
{
    my $oMessage = $@;

    # If a backrest exception then return the code - don't confess
    if (blessed($oMessage) && $oMessage->isa('pgBackRest::Common::Exception'))
    {
        syswrite(*STDOUT, $oMessage->trace());
        exit $oMessage->code();
    }

    syswrite(*STDOUT, $oMessage);
    exit 250;
}

if (!$bDryRun && !$bVmOut)
{
    &log(INFO, 'TESTS COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)');
}
