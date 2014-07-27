#!/usr/bin/perl
####################################################################################################################################
# BackupTest.pl - Unit Tests for BackRest::File
####################################################################################################################################
package BackRestTest::UtilityTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use english;
use Carp;

use File::Basename;

use lib dirname($0) . "/../lib";
use BackRest::Utility;
use BackRest::File;

use BackRestTest::CommonTest;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestUtility_Test);

####################################################################################################################################
# BackRestTestUtility_Drop
####################################################################################################################################
sub BackRestTestUtility_Drop
{
    # Remove the test directory
    system('rm -rf ' . BackRestTestCommon_TestPathGet()) == 0
        or die 'unable to remove ' . BackRestTestCommon_TestPathGet() .  'path';
}

####################################################################################################################################
# BackRestTestUtility_Create
####################################################################################################################################
sub BackRestTestUtility_Create
{
    # Drop the old test directory
    BackRestTestUtility_Drop();

    # Create the test directory
    mkdir(BackRestTestCommon_TestPathGet(), oct('0770'))
        or confess 'Unable to create ' . BackRestTestCommon_TestPathGet() . ' path';
}

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
    &log(INFO, "UTILITY MODULE ******************************************************************");

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test config
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'config')
    {
        $iRun = 0;
        $bCreate = true;
        my $oFile = BackRest::File->new();

        &log(INFO, "Test config\n");

        # Increment the run, log, and decide whether this unit test should be run
        if (BackRestTestCommon_Run(++$iRun, "base"))
        {
            # Create the test directory
            if ($bCreate)
            {
                BackRestTestUtility_Create();

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
            config_save($strFile, \%oConfig);

            my $strConfigHash = $oFile->hash(PATH_ABSOLUTE, $strFile);

            # Reload the test config
            my %oConfigTest;

            config_load($strFile, \%oConfigTest);

            # Resave the test config and compare hashes
            my $strFileTest = "${strTestPath}/config-test.cfg";
            config_save($strFileTest, \%oConfigTest);

            my $strConfigTestHash = $oFile->hash(PATH_ABSOLUTE, $strFileTest);

            if ($strConfigHash ne $strConfigTestHash)
            {
                confess "config hash ${strConfigHash} != ${strConfigTestHash}";
            }

            if (BackRestTestCommon_Cleanup())
            {
                &log(INFO, 'cleanup');
                BackRestTestUtility_Drop();
            }
        }
    }
}

1;
