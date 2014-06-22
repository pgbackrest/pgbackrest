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

use Exporter qw(import);
our @EXPORT = qw(BackRestTestCommon_Setup BackRestTestCommon_StanzaGet BackRestTestCommon_CommandRemoteGet
                 BackRestTestCommon_HostGet BackRestTestCommon_UserGet BackRestTestCommon_GroupGet
                 BackRestTestCommon_UserBackRestGet BackRestTestCommon_TestPathGet);

my $strCommonStanza;
my $strCommonCommandRemote;
my $strCommonHost;
my $strCommonUser;
my $strCommonGroup;
my $strCommonUserBackRest;
my $strCommonTestPath;

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

1;
