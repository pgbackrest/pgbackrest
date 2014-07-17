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
use JSON; # remove when config functions go to utility

use lib dirname($0) . "/../lib";
use BackRest::Utility;

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
        my $oFile;

        &log(INFO, "Test config\n");

        # Increment the run, log, and decide whether this unit test should be run
        if (BackRestTestCommon_Run(++$iRun, "base"))
        {
            # Create the test directory
            if ($bCreate)
            {
                # # Create the file object
                # $oFile = (BackRest::File->new
                # (
                #     # strStanza => $strStanza,
                #     # strBackupPath => BackRestTestCommon_BackupPathGet(),
                #     # strRemote => $bRemote ? 'backup' : undef,
                #     # oRemote => $bRemote ? $oRemote : undef
                # ))->clone();

                BackRestTestUtility_Create();

                $bCreate = false;
            }

            my %oConfig;

            $oConfig{test1}{key1} = 'value';
            $oConfig{test1}{key2} = 'value';

            $oConfig{test2}{key1} = 'value';

            $oConfig{test3}{key1} = 'value';
            $oConfig{test3}{key2}{sub1} = 'value';
            $oConfig{test3}{key2}{sub2} = 'value';

            my $strFile = "$strTestPath/test.cfg";
            my $hFile;
            my $bFirst = true;

            open($hFile, '>', $strFile)
                or confess "unable to open ${strFile}";

            foreach my $strSection (sort(keys %oConfig))
            {
                if (!$bFirst)
                {
                    syswrite($hFile, "\n")
                        or confess "unable to write lf: $!";
                }

                syswrite($hFile, "[${strSection}]\n")
                    or confess "unable to write section ${strSection}: $!";

                foreach my $strKey (sort(keys $oConfig{$strSection}))
                {
                    my $strValue = $oConfig{"${strSection}"}{"${strKey}"};

                    if (defined($strValue))
                    {
                        if (ref($strValue) eq "HASH")
                        {
                            syswrite($hFile, "${strKey}=" . encode_json($strValue) . "\n")
                                or confess "unable to write key ${strKey}: $!";
                        }
                        else
                        {
                            syswrite($hFile, "${strKey}=${strValue}\n")
                                or confess "unable to write key ${strKey}: $!";
                        }
                    }
                }

                $bFirst = false;
            }

            close($hFile);

            if (BackRestTestCommon_Cleanup())
            {
                &log(INFO, 'cleanup');
                BackRestTestUtility_Drop();
            }
        }
    }
}

1;
