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
use Cwd;

use lib dirname($0) . '/../lib';
use BackRest::Utility;

use lib dirname($0) . '/lib';
use BackRestTest::CommonTest;
use BackRestTest::UtilityTest;
use BackRestTest::FileTest;
use BackRestTest::BackupTest;

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $strLogLevel = 'off';   # Log level for tests
my $strModule = 'all';
my $strModuleTest = 'all';
my $iModuleTestRun = undef;
my $bDryRun = false;
my $bNoCleanup = false;
my $strPgSqlBin;
my $strTestPath;

GetOptions ('pgsql-bin=s' => \$strPgSqlBin,
            'test-path=s' => \$strTestPath,
            'log-level=s' => \$strLogLevel,
            'module=s' => \$strModule,
            'module-test=s' => \$strModuleTest,
            'module-test-run=s' => \$iModuleTestRun,
            'dry-run' => \$bDryRun,
            'no-cleanup' => \$bNoCleanup)
    or die 'error in command line arguments';

####################################################################################################################################
# Setup
####################################################################################################################################
# Set a neutral umask so tests work as expected
umask(0);

# Set console log level to trace for testing
log_level_set(undef, uc($strLogLevel));

if ($strModuleTest ne 'all' && $strModule eq 'all')
{
    confess "--module must be provided for test \"${strModuleTest}\"";
}

if (defined($iModuleTestRun) && $strModuleTest eq 'all')
{
    confess "--module-test must be provided for run \"${iModuleTestRun}\"";
}

# Make sure PG bin has been defined
if (!defined($strPgSqlBin))
{
    confess 'pgsql-bin was not defined';
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

if ($strModule eq 'all' || $strModule eq 'utility')
{
    BackRestTestUtility_Test($strModuleTest);
}

if ($strModule eq 'all' || $strModule eq 'file')
{
    BackRestTestFile_Test($strModuleTest);
}

if ($strModule eq 'all' || $strModule eq 'backup')
{
    BackRestTestBackup_Test($strModuleTest);
}

if (!$bDryRun)
{
    &log(ASSERT, 'TESTS COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)');
}
