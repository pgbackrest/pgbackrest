####################################################################################################################################
# HelpTest.pm - Unit Tests for help
####################################################################################################################################
package pgBackRestTest::Help::HelpTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Log;
use pgBackRest::Config::Config;

use pgBackRestTest::Backup::Common::HostBackupTest;
use pgBackRestTest::Common::HostGroupTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::CommonTest;

####################################################################################################################################
# helpExecute
####################################################################################################################################
sub helpExecute
{
    my $strCommand = shift;
    my $oLogTest = shift;

    my $oHostGroup = hostGroupGet();

    executeTest($oHostGroup->paramGet(HOST_PARAM_BACKREST_EXE) . " $strCommand", {oLogTest => $oLogTest});
}

####################################################################################################################################
# helpTestRun
####################################################################################################################################
sub helpTestRun
{
    my $strTest = shift;
    my $bVmOut = shift;

    # Setup test variables
    my $iRun;
    my $strModule = 'help';
    my $oLogTest = undef;

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

        # Increment the run, log, and decide whether this unit test should be run
        if (testRun(++$iRun, 'base', $strModule, $strThisTest, \$oLogTest))
        {
            helpExecute(CMD_VERSION, $oLogTest);
            helpExecute(CMD_HELP, $oLogTest);
            helpExecute(CMD_HELP . ' version', $oLogTest);
            helpExecute(CMD_HELP . ' --output=json --stanza=main info', $oLogTest);
            helpExecute(CMD_HELP . ' --output=json --stanza=main info output', $oLogTest);
        }

        # Cleanup
        testCleanup(\$oLogTest);
    }
}

push @EXPORT, qw(helpTestRun);

1;
