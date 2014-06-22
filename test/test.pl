#!/usr/bin/perl
####################################################################################################################################
# test.pl - BackRest Unit Tests
####################################################################################################################################

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use english;

use File::Basename;
use Getopt::Long;
use Carp;

use lib dirname($0) . "/../lib";
use BackRest::Utility;

use lib dirname($0) . "/lib";
use BackRestTest::CommonTest;
use BackRestTest::FileTest;

####################################################################################################################################
# Command line parameters
####################################################################################################################################
my $strLogLevel = 'off';   # Log level for tests
my $strModule = 'all';
my $strModuleTest = 'all';

GetOptions ("log-level=s" => \$strLogLevel,
            "module=s" => \$strModule,
            "module-test=s" => \$strModuleTest)
    or die("Error in command line arguments\n");

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

BackRestCommonTestSetup();

####################################################################################################################################
# Clean whitespace
####################################################################################################################################
# find .. -not \( -path ../.git -prune \) -not \( -path ../test/test -prune \) -not \( -iname ".DS_Store" \)

####################################################################################################################################
# Make sure version number matches in README.md and VERSION
####################################################################################################################################
my $hReadMe;
my $strLine;
my $bMatch = false;
my $strVersion = version_get();

if (!open($hReadMe, '<', dirname($0) . "/../README.md"))
{
    confess "unable to open README.md";
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
# Runs tests
####################################################################################################################################
#&log(INFO, "Testing with test_path = ${strTestPath}, host = ${strHost}, user = ${strUser}, group = ${strGroup}");

if ($strModule eq 'all' || $strModule eq "file")
{
    BackRestFileTest($strModuleTest);
}

&log(ASSERT, "TESTS COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)");
