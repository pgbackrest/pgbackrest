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
use IPC::Open3;

use lib dirname($0) . "/../lib";
use BackRest::Utility;
use BackRest::File;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestCommon_Setup BackRestTestCommon_Execute BackRestTestCommon_ExecuteBackRest
                 BackRestTestCommon_ConfigCreate
                 BackRestTestCommon_StanzaGet BackRestTestCommon_CommandMainGet BackRestTestCommon_CommandRemoteGet
                 BackRestTestCommon_HostGet BackRestTestCommon_UserGet BackRestTestCommon_GroupGet
                 BackRestTestCommon_UserBackRestGet BackRestTestCommon_TestPathGet BackRestTestCommon_BackupPathGet
                 BackRestTestCommon_ArchivePathGet BackRestTestCommon_DbPathGet BackRestTestCommon_DbCommonPathGet
                 BackRestTestCommon_DbPortGet);

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
my $strCommonArchivePath;
my $strCommonDbPath;
my $strCommonDbCommonPath;
my $iCommonDbPort;

####################################################################################################################################
# BackRestTestBackup_Execute
####################################################################################################################################
sub BackRestTestCommon_Execute
{
    my $strCommand = shift;
    my $bRemote = shift;
    my $bSuppressError = shift;

    # Set defaults
    $bRemote = defined($bRemote) ? $bRemote : false;
    $bSuppressError = defined($bSuppressError) ? $bSuppressError : false;

    if ($bRemote)
    {
        $strCommand = "ssh ${strCommonUserBackRest}\@${strCommonHost} '${strCommand}'";
    }

    my $strError;
    my $pId = open3(undef, undef, undef, $strCommand);

    # Wait for the process to finish and report any errors
    waitpid($pId, 0);
    my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

    if ($iExitStatus != 0 && !$bSuppressError)
    {
        confess &log(ERROR, "command '${strCommand}' returned " . $iExitStatus . ": " .
                            (defined($strError) ? $strError : "[unknown]"));
    }
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
    $strCommonArchivePath = "${strCommonTestPath}/archive";
    $strCommonDbPath = "${strCommonTestPath}/db";
    $strCommonDbCommonPath = "${strCommonTestPath}/db/common";
    $iCommonDbPort = 6543;
}

####################################################################################################################################
# BackRestTestCommon_ConfigCreate
####################################################################################################################################
sub BackRestTestCommon_ConfigCreate
{
    my $strLocal = shift;
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
    }

    if ($strLocal eq REMOTE_BACKUP)
    {
        $oParamHash{'db:command:option'}{'psql'} = "--port=${iCommonDbPort}";
    }
    elsif ($strLocal eq REMOTE_DB)
    {
    }
    else
    {
        confess "invalid local type ${strLocal}";
    }

    $oParamHash{$strCommonStanza}{'path'} = $strCommonDbCommonPath;
    $oParamHash{'global:backup'}{'path'} = $strCommonBackupPath;
    $oParamHash{'global:backup'}{'thread-max'} = '8';

    $oParamHash{'global:log'}{'level-console'} = 'error';
    $oParamHash{'global:log'}{'level-file'} = 'trace';

    foreach my $strSection (keys $oParamHashRef)
    {
        foreach my $strKey (keys ${$oParamHashRef}{$strSection})
        {
            $oParamHash{$strSection}{$strKey} = ${$oParamHashRef}{$strSection}{$strKey};
        }
    }

    # Write out the configuration file
    my $strFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf';

    tied(%oParamHash)->WriteConfig($strFile) or die "could not write config file ${strFile}";
    chmod(0660, $strFile) or die "unable to set permissions for ${strFile}";

    # Move the configuration file based on local
    if ($strLocal eq 'db')
    {
        rename($strFile, BackRestTestCommon_DbPathGet() . '/pg_backrest.conf')
            or die "unable to move ${strFile} to " . BackRestTestCommon_DbPathGet() . '/pg_backrest.conf path';
    }
    elsif ($strLocal eq 'backup' && !defined($strRemote))
    {
        rename($strFile, BackRestTestCommon_BackupPathGet() . '/pg_backrest.conf')
            or die "unable to move ${strFile} to " . BackRestTestCommon_BackupPathGet() . '/pg_backrest.conf path';
    }
    else
    {
        BackRestTestCommon_Execute("mv $strFile " . BackRestTestCommon_BackupPathGet() . '/pg_backrest.conf', true);
    }
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

sub BackRestTestCommon_ArchivePathGet
{
    return $strCommonArchivePath;
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
