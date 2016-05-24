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

use lib dirname($0) . '/lib';
use pgBackRestTest::BackupTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::CommonTest;
use pgBackRestTest::CompareTest;
use pgBackRestTest::ConfigTest;
use pgBackRestTest::Docker::ContainerTest;
use pgBackRestTest::FileTest;
use pgBackRestTest::HelpTest;

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
   --infinite           repeat selected tests forever
   --db-version         version of postgres to test (all, defaults to minimal)
   --log-force          force overwrite of current test log files
   --no-lint            Disable static source code analysis

 Configuration Options:
   --exe                pgBackRest executable
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
my $bDryRun = false;
my $bNoCleanup = false;
my $strPgSqlBin;
my $strExe;
my $strTestPath;
my $bVersion = false;
my $bHelp = false;
my $bQuiet = false;
my $bInfinite = false;
my $strDbVersion = 'minimal';
my $bLogForce = false;
my $strVm = 'all';
my $bVmBuild = false;
my $bVmForce = false;
my $bNoLint = false;

my $strCommandLine = join(' ', @ARGV);

GetOptions ('q|quiet' => \$bQuiet,
            'version' => \$bVersion,
            'help' => \$bHelp,
            'pgsql-bin=s' => \$strPgSqlBin,
            'exes=s' => \$strExe,
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
            'process-max=s' => \$iProcessMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'infinite' => \$bInfinite,
            'db-version=s' => \$strDbVersion,
            'log-force' => \$bLogForce,
            'no-lint' => \$bNoLint)
    or pod2usage(2);

# Display version and exit if requested
if ($bVersion || $bHelp)
{
    syswrite(*STDOUT, BACKREST_NAME . ' ' . $VERSION . " Test Engine\n");

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

if (defined($strExe) && !-e $strExe)
{
    confess '--exe must exist and be fully qualified'
}

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

####################################################################################################################################
# Build Docker containers
####################################################################################################################################
if ($bVmBuild)
{
    containerBuild($strVm, $bVmForce);
    exit 0;
}

eval
{
    ################################################################################################################################
    # Define tests
    ################################################################################################################################
    my $oTestDefinition =
    {
        module =>
        [
            # Help tests
            {
                name => 'help',
                test =>
                [
                    {
                        name => 'help'
                    }
                ]
            },
            # Config tests
            {
                name => 'config',
                test =>
                [
                    {
                        name => 'option'
                    },
                    {
                        name => 'config'
                    }
                ]
            },
            # File tests
            {
                name => 'file',
                test =>
                [
                    {
                        name => 'path_create'
                    },
                    {
                        name => 'move'
                    },
                    {
                        name => 'compress'
                    },
                    {
                        name => 'wait'
                    },
                    {
                        name => 'manifest'
                    },
                    {
                        name => 'list'
                    },
                    {
                        name => 'remove'
                    },
                    {
                        name => 'hash'
                    },
                    {
                        name => 'exists'
                    },
                    {
                        name => 'copy'
                    }
                ]
            },
            # Backup tests
            {
                name => 'backup',
                test =>
                [
                    {
                        name => 'archive-push',
                        total => 8
                    },
                    {
                        name => 'archive-stop',
                        total => 6
                    },
                    {
                        name => 'archive-get',
                        total => 8
                    },
                    {
                        name => 'expire',
                        total => 1
                    },
                    {
                        name => 'synthetic',
                        total => 8,
                        thread => true
                    },
                    {
                        name => 'full',
                        total => 8,
                        thread => true,
                        db => true
                    }
                ]
            }
        ]
    };

    my $oyTestRun = [];

    ################################################################################################################################
    # Start VM and run
    ################################################################################################################################
    if ($strVm ne 'none')
    {
        # Load the doc module dynamically since it is not supported on all systems
        use lib dirname($0) . '/../doc/lib';
        require BackRestDoc::Common::Doc;
        BackRestDoc::Common::Doc->import();

        # Make sure version number matches the latest release
        my $strReleaseFile = dirname(dirname($0)) . '/doc/xml/change-log.xml';
        my $oReleaseDoc = new BackRestDoc::Common::Doc($strReleaseFile);

        foreach my $oRelease ($oReleaseDoc->nodeGet('changelog')->nodeList('changelog-release'))
        {
            if ($oRelease->paramGet('version') ne BACKREST_VERSION)
            {
                confess 'unable to find version ' . BACKREST_VERSION . " as the most recent release in ${strReleaseFile}";
            }

            last;
        }

        if (!$bDryRun)
        {
            # Run Perl critic
            if (!$bNoLint)
            {
                my $strBasePath = dirname(dirname(abs_path($0)));

                &log(INFO, "Performing static code analysis using perl -cW");

                # Check the exe for warnings
                my $strWarning = trim(executeTest("perl -cW ${strBasePath}/bin/pgbackrest 2>&1"));

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

        my $oyVm = vmGet();

        if ($strVm ne 'all' && !defined($${oyVm}{$strVm}))
        {
            confess &log(ERROR, "${strVm} is not a valid VM");
        }

        # Determine which tests to run
        my $iTestsToRun = 0;

        my $stryTestOS = [];

        if ($strVm eq 'all')
        {
            $stryTestOS = ['co6', 'u12', 'co7', 'u14'];
        }
        else
        {
            $stryTestOS = [$strVm];
        }

        foreach my $strTestOS (@{$stryTestOS})
        {
            foreach my $oModule (@{$$oTestDefinition{module}})
            {
                if ($strModule eq $$oModule{name} || $strModule eq 'all')
                {
                    foreach my $oTest (@{$$oModule{test}})
                    {
                        if ($strModuleTest eq $$oTest{name} || $strModuleTest eq 'all')
                        {
                            my $iDbVersionMin = -1;
                            my $iDbVersionMax = -1;

                            # By default test every db version that is supported for each OS
                            my $strDbVersionKey = 'db';

                            # Run a reduced set of tests where each PG version is only tested on a single OS
                            if ($strDbVersion eq 'minimal')
                            {
                                $strDbVersionKey = 'db_minimal';
                            }

                            if (defined($$oTest{db}) && $$oTest{db})
                            {
                                $iDbVersionMin = 0;
                                $iDbVersionMax = @{$$oyVm{$strTestOS}{$strDbVersionKey}} - 1;
                            }

                            my $bFirstDbVersion = true;

                            for (my $iDbVersionIdx = $iDbVersionMax; $iDbVersionIdx >= $iDbVersionMin; $iDbVersionIdx--)
                            {
                                if ($iDbVersionIdx == -1 || $strDbVersion eq 'all' || $strDbVersion eq 'minimal' ||
                                    ($strDbVersion ne 'all' &&
                                        $strDbVersion eq ${$$oyVm{$strTestOS}{$strDbVersionKey}}[$iDbVersionIdx]))
                                {
                                    my $iTestRunMin = defined($iModuleTestRun) ?
                                                          $iModuleTestRun : (defined($$oTest{total}) ? 1 : -1);
                                    my $iTestRunMax = defined($iModuleTestRun) ?
                                                          $iModuleTestRun : (defined($$oTest{total}) ? $$oTest{total} : -1);

                                    if (defined($$oTest{total}) && $iTestRunMax > $$oTest{total})
                                    {
                                        confess &log(ERROR, "invalid run - must be >= 1 and <= $$oTest{total}")
                                    }

                                    for (my $iTestRunIdx = $iTestRunMin; $iTestRunIdx <= $iTestRunMax; $iTestRunIdx++)
                                    {
                                        my $iyThreadMax = [defined($iThreadMax) ? $iThreadMax : 1];

                                        if (defined($$oTest{thread}) && $$oTest{thread} &&
                                            !defined($iThreadMax) && $bFirstDbVersion)
                                        {
                                            $iyThreadMax = [1, 4];
                                        }

                                        foreach my $iThreadTestMax (@{$iyThreadMax})
                                        {
                                            my $oTestRun =
                                            {
                                                os => $strTestOS,
                                                module => $$oModule{name},
                                                test => $$oTest{name},
                                                run => $iTestRunIdx == -1 ? undef : $iTestRunIdx,
                                                thread => $iThreadTestMax,
                                                db => $iDbVersionIdx == -1 ? undef :
                                                    ${$$oyVm{$strTestOS}{$strDbVersionKey}}[$iDbVersionIdx]
                                            };

                                            push(@{$oyTestRun}, $oTestRun);
                                            $iTestsToRun++;
                                        }
                                    }

                                    $bFirstDbVersion = false;
                                }
                            }
                        }
                    }
                }
            }
        }

        if ($iTestsToRun == 0)
        {
            confess &log(ERROR, 'no tests were selected');
        }

        &log(INFO, $iTestsToRun . ' test' . ($iTestsToRun > 1 ? 's': '') . " selected\n");

        if ($bNoCleanup && $iTestsToRun > 1)
        {
            confess &log(ERROR, '--no-cleanup is not valid when more than one test will run')
        }

        my $iTestFail = 0;
        my $oyProcess = [];

        if (!$bDryRun || $bVmOut)
        {
            for (my $iProcessIdx = 0; $iProcessIdx < 8; $iProcessIdx++)
            {
                # &log(INFO, "stop test-${iProcessIdx}");
                push(@{$oyProcess}, undef);
                executeTest("docker rm -f test-${iProcessIdx}", {bSuppressError => true});
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

                                executeTest("docker rm -f ${strImage}");
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
                                          "vm=$$oTest{os}, module=$$oTest{module}, test=$$oTest{test}" .
                                          (defined($$oTest{run}) ? ", run=$$oTest{run}" : '') .
                                          (defined($$oTest{thread}) ? ", thread-max=$$oTest{thread}" : '') .
                                          (defined($$oTest{db}) ? ", db=$$oTest{db}" : '');

                    my $strImage = 'test-' . $iProcessIdx;
                    my $strDbVersion = (defined($$oTest{db}) ? $$oTest{db} : PG_VERSION_94);
                    $strDbVersion =~ s/\.//;

                    &log($bDryRun && !$bVmOut || $bShowOutputAsync ? INFO : DEBUG, "${strTest}" .
                         ($bVmOut || $bShowOutputAsync ? "\n" : ''));

                    my $strVmTestPath = "/home/vagrant/test/${strImage}";
                    my $strHostTestPath = "${strTestPath}/${strImage}";

                    # Don't create the container if this is a dry run unless output from the VM is required.  Ouput can be requested
                    # to get more information about the specific tests that will be run.
                    if (!$bDryRun || $bVmOut)
                    {
                        # Create host test directory
                        filePathCreate($strHostTestPath, '0770');

                        executeTest("docker run -itd -h $$oTest{os}-test --name=${strImage}" .
                                    " -v ${strHostTestPath}:${strVmTestPath}" .
                                    " -v /backrest:/backrest backrest/$$oTest{os}-test-${strDbVersion}");
                    }

                    # Build up command line for the individual test
                    $strCommandLine =~ s/\-\-os\=\S*//g;
                    $strCommandLine =~ s/\-\-test-path\=\S*//g;
                    $strCommandLine =~ s/\-\-module\=\S*//g;
                    $strCommandLine =~ s/\-\-test\=\S*//g;
                    $strCommandLine =~ s/\-\-run\=\S*//g;
                    $strCommandLine =~ s/\-\-db\-version\=\S*//g;

                    my $strCommand =
                        "docker exec -i -u vagrant ${strImage} $0 ${strCommandLine} --test-path=${strVmTestPath}" .
                        " --vm=none --module=$$oTest{module} --test=$$oTest{test}" .
                        (defined($$oTest{run}) ? " --run=$$oTest{run}" : '') .
                        (defined($$oTest{thread}) ? " --thread-max=$$oTest{thread}" : '') .
                        (defined($$oTest{db}) ? " --db-version=$$oTest{db}" : '') .
                        ($bDryRun ? " --dry-run" : '') .
                        " --no-cleanup --vm-out";

                    &log(DEBUG, $strCommand);

                    if (!$bDryRun || $bVmOut)
                    {
                        my $fTestStartTime = gettimeofday();

                        # Set permissions on the Docker test directory.  This can be removed once users/groups are sync'd between
                        # Docker and the host VM.
                        executeTest("docker exec ${strImage} chown vagrant:postgres -R ${strVmTestPath}");

                        my $oExec = new pgBackRestTest::Common::ExecuteTest(
                            $strCommand,
                            {bSuppressError => true, bShowOutputAsync => $bShowOutputAsync});

                        $oExec->begin();

                        my $oProcess =
                        {
                            exec => $oExec,
                            test => $strTest,
                            idx => $iTestIdx,
                            start_time => $fTestStartTime
                        };

                        $$oyProcess[$iProcessIdx] = $oProcess;
                    }

                    $iProcessTotal++;
                }
            }
        }
        while ($iProcessTotal > 0);

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
    # Search for psql
    ################################################################################################################################
    my @stryTestVersion;
    my @stryVersionSupport = versionSupport();

    if (!defined($strPgSqlBin))
    {
        # Distribution-specific paths where the PostgreSQL binaries may be located
        my @strySearchPath =
        (
            '/usr/lib/postgresql/VERSION/bin',  # Debian/Ubuntu
            '/usr/pgsql-VERSION/bin',           # CentOS/RHEL/Fedora
            '/Library/PostgreSQL/VERSION/bin',  # OSX
            '/usr/local/bin'                    # BSD
        );

        foreach my $strSearchPath (@strySearchPath)
        {
            for (my $iVersionIdx = @stryVersionSupport - 1; $iVersionIdx >= 0; $iVersionIdx--)
            {
                if ($strDbVersion eq 'all' || $strDbVersion eq 'minimal' || $strDbVersion eq 'max' && @stryTestVersion == 0 ||
                    $strDbVersion eq $stryVersionSupport[$iVersionIdx])
                {
                    my $strVersionPath = $strSearchPath;
                    $strVersionPath =~ s/VERSION/$stryVersionSupport[$iVersionIdx]/g;

                    if (-e "${strVersionPath}/initdb")
                    {
                        &log(DEBUG, "FOUND pgsql-bin at ${strVersionPath}");
                        push @stryTestVersion, $strVersionPath;
                    }
                }
            }
        }

        # Make sure at least one version of postgres was found
        @stryTestVersion > 0
            or confess 'pgsql-bin was not defined and postgres could not be located automatically';
    }
    else
    {
        push @stryTestVersion, $strPgSqlBin;
    }

    ################################################################################################################################
    # Clean whitespace only if test.pl is being run from the test directory in the backrest repo
    ################################################################################################################################
    # if (-e './test.pl' && -e '../bin/pgbackrest)
    # {
    #     BackRestTestCommon_Execute(
    #         "find .. -type f -not -path \"../.git/*\" -not -path \"*.DS_Store\" -not -path \"../test/test/*\" " .
    #         "-not -path \"../test/data/*\" " .
    #         "-exec sh -c 'for i;do echo \"\$i\" && sed 's/[[:space:]]*\$//' \"\$i\">/tmp/.\$\$ && cat /tmp/.\$\$ " .
    #         "> \"\$i\";done' arg0 {} + > /dev/null", false, true);
    # }

    ################################################################################################################################
    # Runs tests
    ################################################################################################################################
    # &log(INFO, "Testing with test_path = " . BackRestTestCommon_TestPathGet() . ", host = {strHost}, user = {strUser}, " .
    #            "group = {strGroup}");

    my $iRun = 0;

    do
    {
        if (BackRestTestCommon_Setup($strExe, $strTestPath, $stryTestVersion[0], $iModuleTestRun,
                                     $bDryRun, $bNoCleanup, $bLogForce))
        {
            if (!$bVmOut &&
                ($strModule eq 'all' ||
                 $strModule eq 'backup' && $strModuleTest eq 'all' ||
                 $strModule eq 'backup' && $strModuleTest eq 'full'))
            {
                &log(INFO, "TESTING psql-bin = $stryTestVersion[0]\n");
            }

            if ($bInfinite)
            {
                $iRun++;
                &log(INFO, "INFINITE - RUN ${iRun}\n");
            }

            # if ($strModule eq 'all' || $strModule eq 'ini')
            # {
            #     BackRestTestIni_Test($strModuleTest);
            # }

            if ($strModule eq 'all' || $strModule eq 'help')
            {
                BackRestTestHelp_Test($strModuleTest, undef, $bVmOut);
            }

            if ($strModule eq 'all' || $strModule eq 'config')
            {
                BackRestTestConfig_Test($strModuleTest, undef, $bVmOut);
            }

            if ($strModule eq 'all' || $strModule eq 'file')
            {
                BackRestTestFile_Test($strModuleTest, undef, $bVmOut);
            }

            if ($strModule eq 'all' || $strModule eq 'backup')
            {
                if (!defined($iThreadMax) || $iThreadMax == 1)
                {
                    BackRestTestBackup_Test($strModuleTest, 1, $bVmOut);
                }

                if (!defined($iThreadMax) || $iThreadMax > 1)
                {
                    BackRestTestBackup_Test($strModuleTest, defined($iThreadMax) ? $iThreadMax : 4, $bVmOut);
                }

                if (@stryTestVersion > 1 && ($strModuleTest eq 'all' || $strModuleTest eq 'full'))
                {
                    for (my $iVersionIdx = 1; $iVersionIdx < @stryTestVersion; $iVersionIdx++)
                    {
                        BackRestTestCommon_Setup($strExe, $strTestPath, $stryTestVersion[$iVersionIdx],
                                                 $iModuleTestRun, $bDryRun, $bNoCleanup);
                        &log(INFO, "TESTING psql-bin = $stryTestVersion[$iVersionIdx] for backup/full\n");
                        BackRestTestBackup_Test('full', defined($iThreadMax) ? $iThreadMax : 4, $bVmOut);
                    }
                }
            }

            if ($strModule eq 'compare')
            {
                BackRestTestCompare_Test($strModuleTest);
            }
        }
    }
    while ($bInfinite);
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
