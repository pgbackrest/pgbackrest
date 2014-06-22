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
use Config::IniFiles;

use lib dirname($0) . "/../lib";
use BackRest::Utility;
use BackRest::File;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestCommon_Setup BackRestTestCommon_Execute BackRestTestCommon_ExecuteBackRest
                 BackRestTestCommon_ConfigCreate
                 BackRestTestCommon_StanzaGet BackRestTestCommon_CommandMainGet BackRestTestCommon_CommandRemoteGet
                 BackRestTestCommon_HostGet BackRestTestCommon_UserGet BackRestTestCommon_GroupGet
                 BackRestTestCommon_UserBackRestGet BackRestTestCommon_TestPathGet BackRestTestCommon_BackupPathGet
                 BackRestTestCommon_DbPathGet BackRestTestCommon_DbCommonPathGet BackRestTestCommon_DbPortGet);

my $strCommonStanza;
my $strCommonCommandMain;
my $strCommonCommandRemote;
my $strCommonCommandPsql;
my $strCommonHost;
my $strCommonUser;
my $strCommonGroup;
my $strCommonUserBackRest;
my $strCommonTestPath;
my $strCommonBackupPath;
my $strCommonDbPath;
my $strCommonDbCommonPath;
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
    $strCommonCommandMain = '/Users/dsteele/pg_backrest/bin/pg_backrest.pl';
    $strCommonCommandRemote = '/Users/dsteele/pg_backrest/bin/pg_backrest_remote.pl';
    $strCommonCommandPsql = '/Library/PostgreSQL/9.3/bin/psql -X %option%';
    $strCommonHost = '127.0.0.1';
    $strCommonUser = getpwuid($<);
    $strCommonGroup = getgrgid($();
    $strCommonUserBackRest = 'backrest';
    $strCommonTestPath = dirname(abs_path($0)) . '/test';
    $strCommonBackupPath = "${strCommonTestPath}/backrest";
    $strCommonDbPath = "${strCommonTestPath}/db";
    $strCommonDbCommonPath = "${strCommonTestPath}/db/common";
    $iCommonDbPort = 6543;
}

####################################################################################################################################
# BackRestTestCommon_ConfigCreate
####################################################################################################################################
sub BackRestTestCommon_ConfigCreate
{
    my $strFile = shift;
    my $strRemote = shift;
    my $oParamHashRef = shift;
    
    my %oParamHash;
    tie %oParamHash, 'Config::IniFiles';

    $oParamHash{'global:command'}{'remote'} = $strCommonCommandRemote;
    $oParamHash{'global:command'}{'psql'} = $strCommonCommandPsql;

    if (defined($strRemote) && $strRemote eq REMOTE_BACKUP)
    {
        $oParamHash{'global:backup'}{'host'} = $strCommonHost;
        $oParamHash{'global:backup'}{'user'} = $strCommonUserBackRest;
    }
    elsif (defined($strRemote) && $strRemote eq REMOTE_DB)
    {
        $oParamHash{$strCommonStanza}{'host'} = $strCommonHost;
        $oParamHash{$strCommonStanza}{'user'} = $strCommonUser;
        $oParamHash{$strCommonStanza}{'path'} = $strCommonDbCommonPath;

        $oParamHash{'db:command:option'}{'psql'} = "--port=${iCommonDbPort}";
    }

    $oParamHash{'global:backup'}{'path'} = $strCommonBackupPath;

    tied(%oParamHash)->WriteConfig($strFile) or die "could not write config file ${strFile}";
    
    chmod(0770, $strFile) or die "unable to set permissions for ${strFile}";
}

####################################################################################################################################
# Get Methods
####################################################################################################################################
sub BackRestTestCommon_StanzaGet
{
    return $strCommonStanza;
}

sub BackRestTestCommon_CommandMainGet
{
    return $strCommonCommandMain;
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

sub BackRestTestCommon_BackupPathGet
{
    return $strCommonBackupPath;
}

sub BackRestTestCommon_DbPathGet
{
    return $strCommonDbPath;
}

sub BackRestTestCommon_DbCommonPathGet
{
    return $strCommonDbCommonPath;
}

sub BackRestTestCommon_DbPortGet
{
    return $iCommonDbPort;
}

1;
