####################################################################################################################################
# HelpTest.pm - Unit Tests for help
####################################################################################################################################
package BackRestTest::HelpTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;
use BackRest::Config::Config;

use BackRestTest::Common::ExecuteTest;
use BackRestTest::CommonTest;

####################################################################################################################################
# BackRestTestHelp_ExecuteHelp
####################################################################################################################################
sub BackRestTestHelp_ExecuteHelp
{
    my $strCommand = shift;

    executeTest(BackRestTestCommon_CommandMainAbsGet() . ' --no-config ' . $strCommand);
}

####################################################################################################################################
# BackRestTestHelp_Test
####################################################################################################################################
our @EXPORT = qw(BackRestTestHelp_Test);

sub BackRestTestHelp_Test
{
    my $strTest = shift;
    my $iThreadMax = shift;
    my $bVmOut = shift;

    # Setup test variables
    my $iRun;
    my $strModule = 'help';

    # Print test banner
    if (!$bVmOut)
    {
        &log(INFO, 'HELP MODULE *****************************************************************');
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test config
    #-------------------------------------------------------------------------------------------------------------------------------
    my $strThisTest = 'help';

    if ($strTest eq 'all' || $strTest eq $strThisTest)
    {
        $iRun = 0;

        if (!$bVmOut)
        {
            &log(INFO, "Test help\n");
        }

        BackRestTestCommon_Drop(true);
        BackRestTestCommon_Create();

        # Increment the run, log, and decide whether this unit test should be run
        if (BackRestTestCommon_Run(++$iRun, 'base', $strModule, $strThisTest, undef, false))
        {
            BackRestTestHelp_ExecuteHelp('version');
            BackRestTestHelp_ExecuteHelp('help');
            BackRestTestHelp_ExecuteHelp('help version');
            BackRestTestHelp_ExecuteHelp('help --output=json --stanza=main info');
            BackRestTestHelp_ExecuteHelp('help --output=json --stanza=main info output');
        }

        # Cleanup
        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestCommon_Drop(true);
        }
    }
}

1;
