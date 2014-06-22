#!/usr/bin/perl
####################################################################################################################################
# CommonTest.pm - Common globals used for testing
####################################################################################################################################
package BackRestTest::CommonTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use english;
use Carp;

use File::Basename;
use Cwd 'abs_path';

use lib dirname($0) . "/../lib";
use BackRest::Utility;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestCommon_Setup BackRestTestCommon_Execute BackRestTestCommon_ExecuteBackRest
                 BackRestTestCommon_StanzaGet BackRestTestCommon_CommandRemoteGet
                 BackRestTestCommon_HostGet BackRestTestCommon_UserGet BackRestTestCommon_GroupGet
                 BackRestTestCommon_UserBackRestGet BackRestTestCommon_TestPathGet BackRestTestCommon_DbPortGet);

my $strCommonStanza;
my $strCommonCommandRemote;
my $strCommonHost;
my $strCommonUser;
my $strCommonGroup;
my $strCommonUserBackRest;
my $strCommonTestPath;
my $iCommonDbPort;

####################################################################################################################################
# BackRestTestBackup_Execute
####################################################################################################################################
sub BackRestTestCommon_Execute
{
    my $strCommand = shift;
    my $bSuppressError = shift;
    
    if (system($strCommand) != 0)
    {
        if (!defined($bSuppressError) || !$bSuppressError)
        {
            confess &log(ERROR, "unable to execute command: ${strCommand}");
        }
    }
}

####################################################################################################################################
# BackRestTestBackup_ExecuteBackRest
####################################################################################################################################
sub BackRestTestCommon_ExecuteBackRest
{
    my $strCommand = shift;
    my $bSuppressError = shift;
    
    $strCommand = "ssh ${strCommonUserBackRest}\@${strCommonHost} '${strCommand}'";
    
    BackRestTestCommon_Execute($strCommand, $bSuppressError);
}

####################################################################################################################################
# BackRestTestCommon_Setup
####################################################################################################################################
sub BackRestTestCommon_Setup
{
    $strCommonStanza = "db";
    $strCommonCommandRemote = "/Users/dsteele/pg_backrest/bin/pg_backrest_remote.pl";
    $strCommonHost = "127.0.0.1";
    $strCommonUser = getpwuid($<);
    $strCommonGroup = getgrgid($();
    $strCommonUserBackRest = 'backrest';
    $strCommonTestPath = dirname(abs_path($0)) . "/test";
    $iCommonDbPort = 6543;
}

####################################################################################################################################
# Get Methods
####################################################################################################################################
sub BackRestTestCommon_StanzaGet
{
    return $strCommonStanza;
}

sub BackRestTestCommon_CommandRemoteGet
{
    return $strCommonCommandRemote;
}

sub BackRestTestCommon_HostGet
{
    return $strCommonHost;
}

sub BackRestTestCommon_UserGet
{
    return $strCommonUser;
}

sub BackRestTestCommon_GroupGet
{
    return $strCommonGroup;
}

sub BackRestTestCommon_UserBackRestGet
{
    return $strCommonUserBackRest;
}

sub BackRestTestCommon_TestPathGet
{
    return $strCommonTestPath;
}

sub BackRestTestCommon_DbPortGet
{
    return $iCommonDbPort;
}

1;
