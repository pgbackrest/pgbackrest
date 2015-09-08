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
use Cwd qw(abs_path);
use Pod::Usage qw(pod2usage);
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Db;

use lib dirname($0) . '/lib';
use BackRestTest::BackupTest;
use BackRestTest::CommonTest;
use BackRestTest::CompareTest;
use BackRestTest::ConfigTest;
use BackRestTest::FileTest;
use BackRestTest::HelpTest;
# use BackRestTest::IniTest;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

test.pl - pgBackRest Unit Tests

=head1 SYNOPSIS

test.pl [options]

 Test Options:
   --module             test module to execute:
   --test               execute the specified test in a module
   --run                execute only the specified test run
   --thread-max         max threads to run for backup/restore (default 4)
   --dry-run            show only the tests that would be executed but don't execute them
   --no-cleanup         don't cleaup after the last test is complete - useful for debugging
   --infinite           repeat selected tests forever
   --db-version         version of postgres to test (or all)
   --log-force          force overwrite of current test log files

 Configuration Options:
   --exe                backup executable
   --psql-bin           path to the psql executables (e.g. /usr/lib/postgresql/9.3/bin/)
   --test-path          path where tests are executed (defaults to ./test)
   --log-level          log level to use for tests (defaults to INFO)
   --quiet, -q          equivalent to --log-level=off

 General Options:
   --version            display version and exit
   --help               display usage and exit
=cut

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $strLogLevel = 'info';
my $strModule = 'all';
my $strModuleTest = 'all';
my $iModuleTestRun = undef;
my $iThreadMax = 1;
my $bDryRun = false;
my $bNoCleanup = false;
my $strPgSqlBin;
my $strExe;
my $strTestPath;
my $bVersion = false;
my $bHelp = false;
my $bQuiet = false;
my $bInfinite = false;
my $strDbVersion = 'max';
my $bLogForce = false;

GetOptions ('q|quiet' => \$bQuiet,
            'version' => \$bVersion,
            'help' => \$bHelp,
            'pgsql-bin=s' => \$strPgSqlBin,
            'exes=s' => \$strExe,
            'test-path=s' => \$strTestPath,
            'log-level=s' => \$strLogLevel,
            'module=s' => \$strModule,
            'test=s' => \$strModuleTest,
            'run=s' => \$iModuleTestRun,
            'thread-max=s' => \$iThreadMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'infinite' => \$bInfinite,
            'db-version=s' => \$strDbVersion,
            'log-force' => \$bLogForce)
    or pod2usage(2);

# Display version and exit if requested
if ($bVersion || $bHelp)
{
    syswrite(*STDOUT, 'pgBackRest ' . BACKREST_VERSION . " Unit Tests\n");

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

logLevelSet(undef, uc($strLogLevel));

if ($strModuleTest ne 'all' && $strModule eq 'all')
{
    confess "--module must be provided for test \"${strModuleTest}\"";
}

if (defined($iModuleTestRun) && $strModuleTest eq 'all')
{
    confess "--module-test must be provided for run \"${iModuleTestRun}\"";
}

# Search for psql bin
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
            if ($strDbVersion eq 'all' || $strDbVersion eq 'max' && @stryTestVersion == 0 ||
                $strDbVersion eq $stryVersionSupport[$iVersionIdx])
            {
                my $strVersionPath = $strSearchPath;
                $strVersionPath =~ s/VERSION/$stryVersionSupport[$iVersionIdx]/g;

                if (-e "${strVersionPath}/initdb")
                {
                    &log(INFO, "FOUND pgsql-bin at ${strVersionPath}");
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

# Check thread total
if ($iThreadMax < 1 || $iThreadMax > 32)
{
    confess 'thread-max must be between 1 and 32';
}

####################################################################################################################################
# Make sure version number matches in the change log.
####################################################################################################################################
my $hReadMe;
my $strLine;
my $bMatch = false;
my $strChangeLogFile = abs_path(dirname($0) . '/../CHANGELOG.md');

if (!open($hReadMe, '<', $strChangeLogFile))
{
    confess "unable to open ${strChangeLogFile}";
}

while ($strLine = readline($hReadMe))
{
    if ($strLine =~ /^\#\# v/)
    {
        $bMatch = substr($strLine, 4, length(BACKREST_VERSION)) eq BACKREST_VERSION;
        last;
    }
}

if (!$bMatch)
{
    confess 'unable to find version ' . BACKREST_VERSION . " as last revision in ${strChangeLogFile}";
}

####################################################################################################################################
# Clean whitespace only if test.pl is being run from the test directory in the backrest repo
####################################################################################################################################
if (-e './test.pl' && -e '../bin/pg_backrest')
{
    BackRestTestCommon_Execute(
        "find .. -type f -not -path \"../.git/*\" -not -path \"*.DS_Store\" -not -path \"../test/test/*\" " .
        "-not -path \"../test/data/*\" " .
        "-exec sh -c 'for i;do echo \"\$i\" && sed 's/[[:space:]]*\$//' \"\$i\">/tmp/.\$\$ && cat /tmp/.\$\$ " .
        "> \"\$i\";done' arg0 {} + > /dev/null", false, true);
}

####################################################################################################################################
# Runs tests
####################################################################################################################################
# &log(INFO, "Testing with test_path = " . BackRestTestCommon_TestPathGet() . ", host = {strHost}, user = {strUser}, " .
#            "group = {strGroup}");

my $iRun = 0;

eval
{
    do
    {
        if (BackRestTestCommon_Setup($strExe, $strTestPath, $stryTestVersion[0], $iModuleTestRun,
                                     $bDryRun, $bNoCleanup, $bLogForce))
        {
            &log(INFO, "TESTING psql-bin = $stryTestVersion[0]\n");

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
                BackRestTestHelp_Test($strModuleTest);
            }

            if ($strModule eq 'all' || $strModule eq 'config')
            {
                BackRestTestConfig_Test($strModuleTest);
            }

            if ($strModule eq 'all' || $strModule eq 'file')
            {
                BackRestTestFile_Test($strModuleTest);
            }

            if ($strModule eq 'all' || $strModule eq 'backup')
            {
                BackRestTestBackup_Test($strModuleTest, $iThreadMax);

                if (@stryTestVersion > 1 && ($strModuleTest eq 'all' || $strModuleTest eq 'full'))
                {
                    for (my $iVersionIdx = 1; $iVersionIdx < @stryTestVersion; $iVersionIdx++)
                    {
                        BackRestTestCommon_Setup($strExe, $strTestPath, $stryTestVersion[$iVersionIdx],
                                                 $iModuleTestRun, $bDryRun, $bNoCleanup);
                        &log(INFO, "TESTING psql-bin = $stryTestVersion[$iVersionIdx] for backup/full\n");
                        BackRestTestBackup_Test('full', $iThreadMax);
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
    if (blessed($oMessage) && $oMessage->isa('BackRest::Common::Exception'))
    {
        syswrite(*STDOUT, $oMessage->trace());
        exit $oMessage->code();
    }

    syswrite(*STDOUT, $oMessage);
    exit 255;;
}


if (!$bDryRun)
{
    &log(INFO, 'TESTS COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)');
}
