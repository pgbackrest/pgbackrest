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
use Carp;

use File::Basename;
use File::Path qw(remove_tree);
use Cwd 'abs_path';
use IPC::Open3;
use POSIX ':sys_wait_h';
use IO::Select;
use File::Copy qw(move);

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRest::Remote;
use BackRest::File;
use BackRest::Manifest;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestCommon_Setup BackRestTestCommon_ExecuteBegin BackRestTestCommon_ExecuteEnd
                 BackRestTestCommon_Execute BackRestTestCommon_ExecuteBackRest
                 BackRestTestCommon_PathCreate BackRestTestCommon_PathMode BackRestTestCommon_PathRemove
                 BackRestTestCommon_FileCreate BackRestTestCommon_FileRemove BackRestTestCommon_PathCopy BackRestTestCommon_PathMove
                 BackRestTestCommon_ConfigCreate BackRestTestCommon_ConfigRemap BackRestTestCommon_Run BackRestTestCommon_Cleanup
                 BackRestTestCommon_PgSqlBinPathGet BackRestTestCommon_StanzaGet BackRestTestCommon_CommandMainGet
                 BackRestTestCommon_CommandRemoteGet BackRestTestCommon_HostGet BackRestTestCommon_UserGet
                 BackRestTestCommon_GroupGet BackRestTestCommon_UserBackRestGet BackRestTestCommon_TestPathGet
                 BackRestTestCommon_DataPathGet BackRestTestCommon_BackupPathGet BackRestTestCommon_ArchivePathGet
                 BackRestTestCommon_DbPathGet BackRestTestCommon_DbCommonPathGet BackRestTestCommon_DbTablespacePathGet
                 BackRestTestCommon_DbPortGet);

my $strPgSqlBin;
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
my $strCommonDbTablespacePath;
my $iCommonDbPort;
my $iModuleTestRun;
my $bDryRun;
my $bNoCleanup;

# Execution globals
my $strErrorLog;
my $hError;
my $strOutLog;
my $hOut;
my $pId;
my $strCommand;

####################################################################################################################################
# BackRestTestCommon_Run
####################################################################################################################################
sub BackRestTestCommon_Run
{
    my $iRun = shift;
    my $strLog = shift;

    if (defined($iModuleTestRun) && $iModuleTestRun != $iRun)
    {
        return false;
    }

    &log(INFO, 'run ' . sprintf('%03d', $iRun) . ' - ' . $strLog);

    if ($bDryRun)
    {
        return false;
    }

    return true;
}

####################################################################################################################################
# BackRestTestCommon_Cleanup
####################################################################################################################################
sub BackRestTestCommon_Cleanup
{
    return !$bNoCleanup && !$bDryRun;
}

####################################################################################################################################
# BackRestTestCommon_ExecuteBegin
####################################################################################################################################
sub BackRestTestCommon_ExecuteBegin
{
    my $strCommandParam = shift;
    my $bRemote = shift;

    # Set defaults
    $bRemote = defined($bRemote) ? $bRemote : false;

    if ($bRemote)
    {
        $strCommand = "ssh ${strCommonUserBackRest}\@${strCommonHost} '${strCommandParam}'";
    }
    else
    {
        $strCommand = $strCommandParam;
    }

    $strErrorLog = '';
    $hError = undef;
    $strOutLog = '';
    $hOut = undef;

    &log(DEBUG, "executing command: ${strCommand}");

    # Execute the command
    $pId = open3(undef, $hOut, $hError, $strCommand);
}

####################################################################################################################################
# BackRestTestCommon_ExecuteEnd
####################################################################################################################################
sub BackRestTestCommon_ExecuteEnd
{
    my $strTest = shift;
    my $bSuppressError = shift;
    my $bShowOutput = shift;
    my $iExpectedExitStatus = shift;

    # Set defaults
    $bSuppressError = defined($bSuppressError) ? $bSuppressError : false;
    $bShowOutput = defined($bShowOutput) ? $bShowOutput : false;

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

                if (defined($strTest) && test_check($strLine, $strTest))
                {
                    &log(DEBUG, "Found test ${strTest}");
                    return true;
                }
            }
        }
    }

    # Check the exit status and output an error if needed
    my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

    if (defined($iExpectedExitStatus) && $iExitStatus == $iExpectedExitStatus)
    {
        return $iExitStatus;
    }

    if ($iExitStatus != 0)
    {
        if ($bSuppressError)
        {
            &log(DEBUG, "suppressed error was ${iExitStatus}");
        }
        else
        {
            confess &log(ERROR, "command '${strCommand}' returned " . $iExitStatus .
                         (defined($iExpectedExitStatus) ? ", but ${iExpectedExitStatus} was expected" : '') . "\n" .
                         ($strOutLog ne '' ? "STDOUT:\n${strOutLog}" : '') .
                         ($strErrorLog ne '' ? "STDERR:\n${strErrorLog}" : ''));
        }
    }

    if ($bShowOutput)
    {
        print "output:\n${strOutLog}\n";
    }

    if (defined($strTest))
    {
        confess &log(ASSERT, "test point ${strTest} was not found");
    }

    $hError = undef;
    $hOut = undef;

    return $iExitStatus;
}

####################################################################################################################################
# BackRestTestCommon_Execute
####################################################################################################################################
sub BackRestTestCommon_Execute
{
    my $strCommand = shift;
    my $bRemote = shift;
    my $bSuppressError = shift;
    my $bShowOutput = shift;
    my $iExpectedExitStatus = shift;

    BackRestTestCommon_ExecuteBegin($strCommand, $bRemote);
    return BackRestTestCommon_ExecuteEnd(undef, $bSuppressError, $bShowOutput, $iExpectedExitStatus);
}

####################################################################################################################################
# BackRestTestCommon_PathCreate
#
# Create a path and set mode.
####################################################################################################################################
sub BackRestTestCommon_PathCreate
{
    my $strPath = shift;
    my $strMode = shift;

    # Create the path
    mkdir($strPath)
        or confess "unable to create ${strPath} path";

    # Set the permissions
    chmod(oct(defined($strMode) ? $strMode : '0700'), $strPath)
        or confess 'unable to set mode ${strMode} for ${strPath}';
}

####################################################################################################################################
# BackRestTestCommon_PathMode
#
# Set mode of an existing path.
####################################################################################################################################
sub BackRestTestCommon_PathMode
{
    my $strPath = shift;
    my $strMode = shift;

    # Set the permissions
    chmod(oct($strMode), $strPath)
        or confess 'unable to set mode ${strMode} for ${strPath}';
}

####################################################################################################################################
# BackRestTestCommon_PathRemove
#
# Remove a path and all subpaths.
####################################################################################################################################
sub BackRestTestCommon_PathRemove
{
    my $strPath = shift;
    my $bRemote = shift;
    my $bSuppressError = shift;

    BackRestTestCommon_Execute('rm -rf ' . $strPath, $bRemote, $bSuppressError);

    # remove_tree($strPath, {result => \my $oError});
    #
    # if (@$oError)
    # {
    #     my $strMessage = "error(s) occurred while removing ${strPath}:";
    #
    #     for my $strFile (@$oError)
    #     {
    #         $strMessage .= "\nunable to remove: " . $strFile;
    #     }
    #
    #     confess $strMessage;
    # }
}

####################################################################################################################################
# BackRestTestCommon_PathCopy
#
# Copy a path.
####################################################################################################################################
sub BackRestTestCommon_PathCopy
{
    my $strSourcePath = shift;
    my $strDestinationPath = shift;
    my $bRemote = shift;
    my $bSuppressError = shift;

    BackRestTestCommon_Execute("cp -rp ${strSourcePath} ${strDestinationPath}", $bRemote, $bSuppressError);
}

####################################################################################################################################
# BackRestTestCommon_PathMove
#
# Copy a path.
####################################################################################################################################
sub BackRestTestCommon_PathMove
{
    my $strSourcePath = shift;
    my $strDestinationPath = shift;
    my $bRemote = shift;
    my $bSuppressError = shift;

    BackRestTestCommon_PathCopy($strSourcePath, $strDestinationPath, $bRemote, $bSuppressError);
    BackRestTestCommon_PathRemove($strSourcePath, $bRemote, $bSuppressError);
}

####################################################################################################################################
# BackRestTestCommon_FileCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
sub BackRestTestCommon_FileCreate
{
    my $strFile = shift;
    my $strContent = shift;
    my $lTime = shift;
    my $strMode = shift;

    # Open the file and save strContent to it
    my $hFile = shift;

    open($hFile, '>', $strFile)
        or confess "unable to open ${strFile} for writing";

    syswrite($hFile, $strContent)
        or confess "unable to write to ${strFile}: $!";

    close($hFile);

    # Set the time
    if (defined($lTime))
    {
        utime($lTime, $lTime, $strFile)
            or confess 'unable to set time ${lTime} for ${strPath}';
    }

    # Set the permissions
    chmod(oct(defined($strMode) ? $strMode : '0600'), $strFile)
        or confess 'unable to set mode ${strMode} for ${strFile}';
}

####################################################################################################################################
# BackRestTestCommon_FileRemove
#
# Remove a file.
####################################################################################################################################
sub BackRestTestCommon_FileRemove
{
    my $strFile = shift;

    unlink($strFile)
        or confess "unable to remove ${strFile}: $!";
}

####################################################################################################################################
# BackRestTestCommon_Setup
####################################################################################################################################
sub BackRestTestCommon_Setup
{
    my $strTestPathParam = shift;
    my $strPgSqlBinParam = shift;
    my $iModuleTestRunParam = shift;
    my $bDryRunParam = shift;
    my $bNoCleanupParam = shift;

    my $strBasePath = dirname(dirname(abs_path($0)));

    $strPgSqlBin = $strPgSqlBinParam;

    $strCommonStanza = 'db';
    $strCommonHost = '127.0.0.1';
    $strCommonUser = getpwuid($<);
    $strCommonGroup = getgrgid($();
    $strCommonUserBackRest = 'backrest';

    if (defined($strTestPathParam))
    {
        $strCommonTestPath = $strTestPathParam;
    }
    else
    {
        $strCommonTestPath = "${strBasePath}/test/test";
    }

    $strCommonDataPath = "${strBasePath}/test/data";
    $strCommonBackupPath = "${strCommonTestPath}/backrest";
    $strCommonArchivePath = "${strCommonTestPath}/archive";
    $strCommonDbPath = "${strCommonTestPath}/db";
    $strCommonDbCommonPath = "${strCommonTestPath}/db/common";
    $strCommonDbTablespacePath = "${strCommonTestPath}/db/tablespace";

    $strCommonCommandMain = "${strBasePath}/bin/pg_backrest.pl";
    $strCommonCommandRemote = "${strBasePath}/bin/pg_backrest_remote.pl";
    $strCommonCommandPsql = "${strPgSqlBin}/psql -X %option% -h ${strCommonDbPath}";

    $iCommonDbPort = 6543;
    $iModuleTestRun = $iModuleTestRunParam;
    $bDryRun = $bDryRunParam;
    $bNoCleanup = $bNoCleanupParam;
}

####################################################################################################################################
# BackRestTestCommon_ConfigRemap
####################################################################################################################################
sub BackRestTestCommon_ConfigRemap
{
    my $oRemapHashRef = shift;
    my $oManifestRef = shift;
    my $bRemote = shift;

    # Create config filename
    my $strConfigFile = BackRestTestCommon_DbPathGet() . '/pg_backrest.conf';
    my $strStanza = BackRestTestCommon_StanzaGet();

    # Load Config file
    my %oConfig;
    ini_load($strConfigFile, \%oConfig);

    # Load remote config file
    my %oRemoteConfig;
    my $strRemoteConfigFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf.remote';

    if ($bRemote)
    {
        BackRestTestCommon_Execute("mv " . BackRestTestCommon_BackupPathGet() . "/pg_backrest.conf ${strRemoteConfigFile}", true);
        ini_load($strRemoteConfigFile, \%oRemoteConfig);
    }

    # Rewrite remap section
    delete($oConfig{"${strStanza}:tablespace:map"});

    foreach my $strRemap (sort(keys $oRemapHashRef))
    {
        my $strRemapPath = ${$oRemapHashRef}{$strRemap};

        if ($strRemap eq 'base')
        {
            $oConfig{$strStanza}{path} = $strRemapPath;
            ${$oManifestRef}{'backup:path'}{base} = $strRemapPath;

            if ($bRemote)
            {
                $oRemoteConfig{$strStanza}{path} = $strRemapPath;
            }
        }
        else
        {
            $oConfig{"${strStanza}:tablespace:map"}{$strRemap} = $strRemapPath;

            ${$oManifestRef}{'backup:path'}{"tablespace:${strRemap}"} = $strRemapPath;
            ${$oManifestRef}{'backup:tablespace'}{$strRemap}{'path'} = $strRemapPath;
            ${$oManifestRef}{'base:link'}{"pg_tblspc/${strRemap}"}{'link_destination'} = $strRemapPath;
        }
    }

    # Resave the config file
    ini_save($strConfigFile, \%oConfig);

    # Load remote config file
    if ($bRemote)
    {
        ini_save($strRemoteConfigFile, \%oRemoteConfig);
        BackRestTestCommon_Execute("mv ${strRemoteConfigFile} " . BackRestTestCommon_BackupPathGet() . '/pg_backrest.conf', true);
    }
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

    if (defined($strRemote) && $strRemote eq BACKUP)
    {
        $oParamHash{'global:backup'}{'host'} = $strCommonHost;
        $oParamHash{'global:backup'}{'user'} = $strCommonUserBackRest;
    }
    elsif (defined($strRemote) && $strRemote eq DB)
    {
        $oParamHash{$strCommonStanza}{'host'} = $strCommonHost;
        $oParamHash{$strCommonStanza}{'user'} = $strCommonUser;
    }

    $oParamHash{'global:log'}{'level-console'} = 'error';
    $oParamHash{'global:log'}{'level-file'} = 'trace';

    if ($strLocal eq BACKUP)
    {
    }
    elsif ($strLocal eq DB)
    {
        if (defined($strRemote))
        {
            $oParamHash{'global:log'}{'level-console'} = 'trace';

            if (!$bArchiveLocal)
            {
                $oParamHash{'global:restore'}{path} = BackRestTestCommon_ArchivePathGet();
            }

            $oParamHash{'global:restore'}{'thread-max'} = $iThreadMax;
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

    if (($strLocal eq BACKUP) || ($strLocal eq DB && !defined($strRemote)))
    {
        $oParamHash{'db:command:option'}{'psql'} = "--port=${iCommonDbPort}";
        $oParamHash{'global:backup'}{'thread-max'} = $iThreadMax;

        if (defined($bHardlink) && !$bHardlink)
        {
            $oParamHash{'global:backup'}{'hardlink'} = 'n';
        }
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

    # Write out the configuration file
    my $strFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf';
    ini_save($strFile, \%oParamHash);

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
        BackRestTestCommon_Execute("mv ${strFile} " . BackRestTestCommon_BackupPathGet() . '/pg_backrest.conf', true);
    }
}

####################################################################################################################################
# Get Methods
####################################################################################################################################
sub BackRestTestCommon_PgSqlBinPathGet
{
    return $strPgSqlBin;
}

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
    my $iIndex = shift;

    return $strCommonDbCommonPath . (defined($iIndex) ? "-${iIndex}" : '');
}

sub BackRestTestCommon_DbTablespacePathGet
{
    my $iTablespace = shift;
    my $iIndex = shift;

    return $strCommonDbTablespacePath . (defined($iTablespace) ? "/ts${iTablespace}" . (defined($iIndex) ? "-${iIndex}" : '') : '');
}

sub BackRestTestCommon_DbPortGet
{
    return $iCommonDbPort;
}

1;
