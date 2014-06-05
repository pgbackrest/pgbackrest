#!/usr/bin/perl
####################################################################################################################################
# test.pl - Unit Tests for Simple Postgres Backup and Restore
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

use lib dirname($0) . "/lib";
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

####################################################################################################################################
# Runs tests
####################################################################################################################################
if ($strModule eq 'all' || $strModule eq "file")
{
    BackRestFileTest($strModuleTest);
}

print "\nTEST COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)\n";
