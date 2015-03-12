#!/usr/bin/perl
####################################################################################################################################
# test.pl - BackRest Unit Tests
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use Carp;

use File::Basename;
use Getopt::Long;
use Cwd 'abs_path';
use Pod::Usage;
#use Test::More;

use lib dirname($0) . '/../lib';
use BackRest::Utility;

use lib dirname($0) . '/lib';
use BackRestTest::CommonTest;
use BackRestTest::UtilityTest;
use BackRestTest::ConfigTest;
use BackRestTest::FileTest;
use BackRestTest::BackupTest;

####################################################################################################################################
# Usage
####################################################################################################################################

=head1 NAME

test.pl - Simple Postgres Backup and Restore Unit Tests

=head1 SYNOPSIS

test.pl [options]

 Test Options:
   --module             test module to execute:
   --module-test        execute the specified test in a module
   --module-test-run    execute only the specified test run
   --thread-max         max threads to run for backup/restore (default 4)
   --dry-run            show only the tests that would be executed but don't execute them
   --no-cleanup         don't cleaup after the last test is complete - useful for debugging
   --infinite           repeat selected tests forever

 Configuration Options:
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
my $strLogLevel = 'info';   # Log level for tests
my $strModule = 'all';
my $strModuleTest = 'all';
my $iModuleTestRun = undef;
my $iThreadMax = 1;
my $bDryRun = false;
my $bNoCleanup = false;
my $strPgSqlBin;
my $strTestPath;
my $bVersion = false;
my $bHelp = false;
my $bQuiet = false;
my $bInfinite = false;

GetOptions ('q|quiet' => \$bQuiet,
            'version' => \$bVersion,
            'help' => \$bHelp,
            'pgsql-bin=s' => \$strPgSqlBin,
            'test-path=s' => \$strTestPath,
            'log-level=s' => \$strLogLevel,
            'module=s' => \$strModule,
            'module-test=s' => \$strModuleTest,
            'module-test-run=s' => \$iModuleTestRun,
            'thread-max=s' => \$iThreadMax,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup,
            'infinite' => \$bInfinite)
    or pod2usage(2);

# Display version and exit if requested
if ($bVersion || $bHelp)
{
    print 'pg_backrest ' . version_get() . " unit test\n";

    if ($bHelp)
    {
        print "\n";
        pod2usage();
    }

    exit 0;
}

# Test::More->builder->output('/dev/null');

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

log_level_set(undef, uc($strLogLevel));

if ($strModuleTest ne 'all' && $strModule eq 'all')
{
    confess "--module must be provided for test \"${strModuleTest}\"";
}

if (defined($iModuleTestRun) && $strModuleTest eq 'all')
{
    confess "--module-test must be provided for run \"${iModuleTestRun}\"";
}

# Search for psql bin
if (!defined($strPgSqlBin))
{
    my @strySearchPath = ('/usr/lib/postgresql/VERSION/bin', '/Library/PostgreSQL/VERSION/bin');

    foreach my $strSearchPath (@strySearchPath)
    {
        for (my $fVersion = 9; $fVersion >= 0; $fVersion -= 1)
        {
            my $strVersionPath = $strSearchPath;
            $strVersionPath =~ s/VERSION/9\.$fVersion/g;

            if (-e "${strVersionPath}/initdb")
            {
                &log(INFO, "found pgsql-bin at ${strVersionPath}\n");
                $strPgSqlBin = ${strVersionPath};
            }
        }
    }

    if (!defined($strPgSqlBin))
    {
        confess 'pgsql-bin was not defined and could not be located';
    }
}

# Check thread total
if ($iThreadMax < 1 || $iThreadMax > 32)
{
    confess 'thread-max must be between 1 and 32';
}

####################################################################################################################################
# Make sure version number matches in README.md and VERSION
####################################################################################################################################
my $hReadMe;
my $strLine;
my $bMatch = false;
my $strVersion = version_get();

if (!open($hReadMe, '<', dirname($0) . '/../README.md'))
{
    confess 'unable to open README.md';
}

while ($strLine = readline($hReadMe))
{
    if ($strLine =~ /^\#\#\# v/)
    {
        $bMatch = substr($strLine, 5, length($strVersion)) eq $strVersion;
        last;
    }
}

if (!$bMatch)
{
    confess "unable to find version ${strVersion} as last revision in README.md";
}

####################################################################################################################################
# Clean whitespace only if test.pl is being run from the test directory in the backrest repo
####################################################################################################################################
my $hVersion;

if (-e './test.pl' && -e '../bin/pg_backrest.pl' && open($hVersion, '<', '../VERSION'))
{
    my $strTestVersion = readline($hVersion);

    if (defined($strTestVersion) && $strVersion eq trim($strTestVersion))
    {
        BackRestTestCommon_Execute(
            "find .. -type f -not -path \"../.git/*\" -not -path \"*.DS_Store\" -not -path \"../test/test/*\" " .
            "-not -path \"../test/data/*\" " .
            "-exec sh -c 'for i;do echo \"\$i\" && sed 's/[[:space:]]*\$//' \"\$i\">/tmp/.\$\$ && cat /tmp/.\$\$ " .
            "> \"\$i\";done' arg0 {} + > /dev/null", false, true);
    }

    close($hVersion);
}

####################################################################################################################################
# Runs tests
####################################################################################################################################
BackRestTestCommon_Setup($strTestPath, $strPgSqlBin, $iModuleTestRun, $bDryRun, $bNoCleanup);

# &log(INFO, "Testing with test_path = " . BackRestTestCommon_TestPathGet() . ", host = {strHost}, user = {strUser}, " .
#            "group = {strGroup}");

my $iRun = 0;

do
{
    if ($bInfinite)
    {
        $iRun++;
        &log(INFO, "INFINITE - RUN ${iRun}\n");
    }

    if ($strModule eq 'all' || $strModule eq 'utility')
    {
        BackRestTestUtility_Test($strModuleTest);
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
    }
}
while ($bInfinite);

if (!$bDryRun)
{
    &log(INFO, 'TESTS COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)');
}
