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
use IPC::Open3;
use POSIX ":sys_wait_h";
use IO::Select;

use lib dirname($0) . "/../lib";
use BackRest::Utility;
use BackRest::File;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestCommon_Setup BackRestTestCommon_Execute BackRestTestCommon_ExecuteBackRest
                 BackRestTestCommon_ConfigCreate BackRestTestCommon_Run BackRestTestCommon_Cleanup
                 BackRestTestCommon_StanzaGet BackRestTestCommon_CommandMainGet BackRestTestCommon_CommandRemoteGet
                 BackRestTestCommon_HostGet BackRestTestCommon_UserGet BackRestTestCommon_GroupGet
                 BackRestTestCommon_UserBackRestGet BackRestTestCommon_TestPathGet BackRestTestCommon_DataPathGet
                 BackRestTestCommon_BackupPathGet BackRestTestCommon_ArchivePathGet BackRestTestCommon_DbPathGet
                 BackRestTestCommon_DbCommonPathGet BackRestTestCommon_DbPortGet);

my $strCommonStanza;
my $strCommonCommandMain;
my $strCommonCommandRemote;
my $strCommonCommandPsql;
my $strCommonHost;
my $strCommonUser;
my $strCommonGroup;
my $strCommonUserBackRest;
my $strCommonTestPath;
my $strCommonDataPath;
my $strCommonBackupPath;
my $strCommonArchivePath;
my $strCommonDbPath;
my $strCommonDbCommonPath;
my $iCommonDbPort;
my $iModuleTestRun;
my $bDryRun;
my $bNoCleanup;

####################################################################################################################################
# BackRestTestBackup_Run
####################################################################################################################################
sub BackRestTestCommon_Run
{
    my $iRun = shift;
    my $strLog = shift;

    if (defined($iModuleTestRun) && $iModuleTestRun != $iRun)
    {
        return false;
    }

    &log(INFO, "run " . sprintf("%03d", $iRun) . " - " . $strLog);

    if ($bDryRun)
    {
        return false;
    }

    return true;
}

####################################################################################################################################
# BackRestTestBackup_Cleanup
####################################################################################################################################
sub BackRestTestCommon_Cleanup
{
    return !$bNoCleanup && !$bDryRun;
}

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

    # Create error and out file handles and buffers
    my $strErrorLog = '';
    my $hError;

    my $strOutLog = '';
    my $hOut;

    # Execute the command
    my $pId = open3(undef, $hOut, $hError, $strCommand);

    # Create select objects
    my $oErrorSelect = IO::Select->new();
    $oErrorSelect->add($hError);
    my $oOutSelect = IO::Select->new();
    $oOutSelect->add($hOut);

    # While the process is running drain the stdout and stderr streams
    while(waitpid($pId, WNOHANG) == 0)
    {
        # Drain the stderr stream
        if ($oErrorSelect->can_read(.1))
        {
            while (my $strLine = readline($hError))
            {
                $strErrorLog .= $strLine;
            }
        }

        # Drain the stdout stream
        if ($oOutSelect->can_read(.1))
        {
            while (my $strLine = readline($hOut))
            {
                $strOutLog .= $strLine;
            }
        }
    }

    # Check the exit status and output an error if needed
    my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

    if ($iExitStatus != 0 && !$bSuppressError)
    {
        confess &log(ERROR, "command '${strCommand}' returned " . $iExitStatus . "\n" .
                     ($strOutLog ne '' ? "STDOUT:\n${strOutLog}" : '') .
                     ($strErrorLog ne '' ? "STDERR:\n${strErrorLog}" : ''));
    }
}

####################################################################################################################################
# BackRestTestCommon_Setup
####################################################################################################################################
sub BackRestTestCommon_Setup
{
    my $iModuleTestRunParam = shift;
    my $bDryRunParam = shift;
    my $bNoCleanupParam = shift;

    my $strBasePath = dirname(dirname(abs_path($0)));

    $strCommonStanza = "db";
    $strCommonCommandMain = "${strBasePath}/bin/pg_backrest.pl";
    $strCommonCommandRemote = "${strBasePath}/bin/pg_backrest_remote.pl";
    $strCommonCommandPsql = '/Library/PostgreSQL/9.3/bin/psql -X %option%';
#    $strCommonCommandPsql = 'psql -X %option%';
    $strCommonHost = '127.0.0.1';
    $strCommonUser = getpwuid($<);
    $strCommonGroup = getgrgid($();
    $strCommonUserBackRest = 'backrest';
    $strCommonTestPath = "${strBasePath}/test/test";
    $strCommonDataPath = "${strBasePath}/test/data";
    $strCommonBackupPath = "${strCommonTestPath}/backrest";
    $strCommonArchivePath = "${strCommonTestPath}/archive";
    $strCommonDbPath = "${strCommonTestPath}/db";
    $strCommonDbCommonPath = "${strCommonTestPath}/db/common";
    $iCommonDbPort = 6543;
    $iModuleTestRun = $iModuleTestRunParam;
    $bDryRun = $bDryRunParam;
    $bNoCleanup = $bNoCleanupParam;
}

####################################################################################################################################
# BackRestTestCommon_ConfigCreate
####################################################################################################################################
sub BackRestTestCommon_ConfigCreate
{
    my $strLocal = shift;
    my $strRemote = shift;
    my $bCompress = shift;
    my $bChecksum = shift;
    my $bHardlink = shift;
    my $iThreadMax = shift;
    my $bArchiveLocal = shift;
    my $bCompressAsync = shift;

    my %oParamHash;

    if (defined($strRemote))
    {
        $oParamHash{'global:command'}{'remote'} = $strCommonCommandRemote;
    }

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

    $oParamHash{'global:log'}{'level-console'} = 'error';
    $oParamHash{'global:log'}{'level-file'} = 'trace';

    if ($strLocal eq REMOTE_BACKUP)
    {
        if (defined($bHardlink) && $bHardlink)
        {
            $oParamHash{'global:backup'}{'hardlink'} = 'y';
        }
    }
    elsif ($strLocal eq REMOTE_DB)
    {
        if (defined($strRemote))
        {
            $oParamHash{'global:log'}{'level-console'} = 'trace';
        }

        if ($bArchiveLocal)
        {
            $oParamHash{'global:archive'}{path} = BackRestTestCommon_ArchivePathGet();

            if (!$bCompressAsync)
            {
                $oParamHash{'global:archive'}{'compress_async'} = 'n';
            }
        }
    }
    else
    {
        confess "invalid local type ${strLocal}";
    }

    if (($strLocal eq REMOTE_BACKUP) || ($strLocal eq REMOTE_DB && !defined($strRemote)))
    {
        $oParamHash{'db:command:option'}{'psql'} = "--port=${iCommonDbPort}";
    }

    if (defined($bCompress) && !$bCompress)
    {
        $oParamHash{'global:backup'}{'compress'} = 'n';
    }

    if (defined($bChecksum) && !$bChecksum)
    {
        $oParamHash{'global:backup'}{'checksum'} = 'n';
    }

    $oParamHash{$strCommonStanza}{'path'} = $strCommonDbCommonPath;
    $oParamHash{'global:backup'}{'path'} = $strCommonBackupPath;

    if (defined($iThreadMax))
    {
        $oParamHash{'global:backup'}{'thread-max'} = $iThreadMax;
    }

    # Write out the configuration file
    my $strFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf';
    config_save($strFile, \%oParamHash);

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

sub BackRestTestCommon_DataPathGet
{
    return $strCommonDataPath;
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
