#!/usr/bin/perl
####################################################################################################################################
# UtilityTest.pl - Unit Tests for BackRest::Utility
####################################################################################################################################
package BackRestTest::UtilityTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename;

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRest::Config;
use BackRest::File;

use BackRestTest::CommonTest;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestUtility_Test);

####################################################################################################################################
# BackRestTestUtility_Test
####################################################################################################################################
sub BackRestTestUtility_Test
{
    my $strTest = shift;

    # Setup test variables
    my $iRun;
    my $bCreate;
    my $strTestPath = BackRestTestCommon_TestPathGet();

    # Print test banner
    &log(INFO, 'UTILITY MODULE ******************************************************************');

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remote
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oLocal = new BackRest::Remote
    (
        undef,                                  # Host
        undef,                                  # User
        undef,                                  # Command
        OPTION_DEFAULT_BUFFER_SIZE,             # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,          # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,  # Compress network level
    );

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test config
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'config')
    {
        $iRun = 0;
        $bCreate = true;

        my $oFile = new BackRest::File
        (
            undef,
            undef,
            undef,
            $oLocal
        );

        &log(INFO, "Test config\n");

        # Increment the run, log, and decide whether this unit test should be run
        if (BackRestTestCommon_Run(++$iRun, 'base'))
        {
            # Create the test directory
            if ($bCreate)
            {
                BackRestTestCommon_Drop();
                BackRestTestCommon_Create();

                $bCreate = false;
            }

            # Generate a test config
            my %oConfig;

            $oConfig{test1}{key1} = 'value';
            $oConfig{test1}{key2} = 'value';

            $oConfig{test2}{key1} = 'value';

            $oConfig{test3}{key1} = 'value';
            $oConfig{test3}{key2}{sub1} = 'value';
            $oConfig{test3}{key2}{sub2} = 'value';

            # Save the test config
            my $strFile = "${strTestPath}/config.cfg";
            ini_save($strFile, \%oConfig);

            my $strConfigHash = $oFile->hash(PATH_ABSOLUTE, $strFile);

            # Reload the test config
            my %oConfigTest;

            ini_load($strFile, \%oConfigTest);

            # Resave the test config and compare hashes
            my $strFileTest = "${strTestPath}/config-test.cfg";
            ini_save($strFileTest, \%oConfigTest);

            my $strConfigTestHash = $oFile->hash(PATH_ABSOLUTE, $strFileTest);

            if ($strConfigHash ne $strConfigTestHash)
            {
                confess "config hash ${strConfigHash} != ${strConfigTestHash}";
            }

            if (BackRestTestCommon_Cleanup())
            {
                &log(INFO, 'cleanup');
                BackRestTestCommon_Drop();
            }
        }
    }
}

1;
