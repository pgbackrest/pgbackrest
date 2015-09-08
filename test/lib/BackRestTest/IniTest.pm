#!/usr/bin/perl
####################################################################################################################################
# IniTest.pm - Unit Tests for ini load and save
####################################################################################################################################
package BackRestTest::IniTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Config::Config;
use BackRest::File;

use BackRestTest::CommonTest;

####################################################################################################################################
# BackRestTestIni_Test
####################################################################################################################################
our @EXPORT = qw(BackRestTestIni_Test);

sub BackRestTestIni_Test
{
    my $strTest = shift;

    # Setup test variables
    my $iRun;
    my $bCreate;
    my $strTestPath = BackRestTestCommon_TestPathGet();

    # Print test banner
    &log(INFO, 'INI MODULE ******************************************************************');

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remote
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oLocal = new BackRest::Protocol::Common
    (
        OPTION_DEFAULT_BUFFER_SIZE,             # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,          # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK   # Compress network level
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
            iniSave($strFile, \%oConfig);

            my $strConfigHash = $oFile->hash(PATH_ABSOLUTE, $strFile);

            # Reload the test config
            my %oConfigTest;

            iniLoad($strFile, \%oConfigTest);

            # Resave the test config and compare hashes
            my $strFileTest = "${strTestPath}/config-test.cfg";
            iniSave($strFileTest, \%oConfigTest);

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
