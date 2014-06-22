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
our @EXPORT = qw(BackRestCommonTestSetup BackRestCommonStanzaGet BackRestCommonCommandRemoteGet BackRestCommonHostGet
                 BackRestCommonUserGet BackRestCommonGroupGet BackRestCommonUserBackRestGet BackRestCommonTestPathGet);

my $strCommonStanza;
my $strCommonCommandRemote;
my $strCommonHost;
my $strCommonUser;
my $strCommonGroup;
my $strCommonUserBackRest;
my $strCommonTestPath;

####################################################################################################################################
# BackRestCommonTestSetup
####################################################################################################################################
sub BackRestCommonTestSetup
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
sub BackRestCommonStanzaGet
{
    return $strCommonStanza;
}

sub BackRestCommonCommandRemoteGet
{
    return $strCommonCommandRemote;
}

sub BackRestCommonHostGet
{
    return $strCommonHost;
}

sub BackRestCommonUserGet
{
    return $strCommonUser;
}

sub BackRestCommonGroupGet
{
    return $strCommonGroup;
}

sub BackRestCommonUserBackRestGet
{
    return $strCommonUserBackRest;
}

sub BackRestCommonTestPathGet
{
    return $strCommonTestPath;
}

1;
