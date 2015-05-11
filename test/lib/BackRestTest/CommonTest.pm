#!/usr/bin/perl
####################################################################################################################################
# CommonTest.pm - Common globals used for testing
####################################################################################################################################
package BackRestTest::CommonTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename;
use File::Path qw(remove_tree);
use Cwd 'abs_path';
use IPC::Open3;
use POSIX ':sys_wait_h';
use IO::Select;
use File::Copy qw(move);

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRest::Config;
use BackRest::Remote;
use BackRest::File;
use BackRest::Manifest;
use BackRest::Db;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestCommon_Create BackRestTestCommon_Drop BackRestTestCommon_Setup BackRestTestCommon_ExecuteBegin
                 BackRestTestCommon_ExecuteEnd BackRestTestCommon_Execute BackRestTestCommon_ExecuteBackRest
                 BackRestTestCommon_PathCreate BackRestTestCommon_PathMode BackRestTestCommon_PathRemove
                 BackRestTestCommon_FileCreate BackRestTestCommon_FileRemove BackRestTestCommon_PathCopy BackRestTestCommon_PathMove
                 BackRestTestCommon_ConfigCreate BackRestTestCommon_ConfigRemap BackRestTestCommon_ConfigRecovery
                 BackRestTestCommon_Run BackRestTestCommon_Cleanup BackRestTestCommon_PgSqlBinPathGet
                 BackRestTestCommon_StanzaGet BackRestTestCommon_CommandMainGet BackRestTestCommon_CommandRemoteGet
                 BackRestTestCommon_HostGet BackRestTestCommon_UserGet BackRestTestCommon_GroupGet
                 BackRestTestCommon_UserBackRestGet BackRestTestCommon_TestPathGet BackRestTestCommon_DataPathGet
                 BackRestTestCommon_RepoPathGet BackRestTestCommon_LocalPathGet BackRestTestCommon_DbPathGet
                 BackRestTestCommon_DbCommonPathGet BackRestTestCommon_ClusterStop BackRestTestCommon_DbTablespacePathGet
                 BackRestTestCommon_DbPortGet BackRestTestCommon_iniLoad BackRestTestCommon_iniSave BackRestTestCommon_DbVersion
                 BackRestTestCommon_CommandPsqlGet BackRestTestCommon_DropRepo BackRestTestCommon_CreateRepo
                 BackRestTestCommon_manifestLoad BackRestTestCommon_manifestSave BackRestTestCommon_CommandMainAbsGet);

my $strPgSqlBin;
my $strCommonStanza;
my $strCommonCommandMain;
my $strCommonCommandRemote;
my $strCommonCommandPsql;
my $strCommonHost;
my $strCommonUser;
my $strCommonGroup;
my $strCommonUserBackRest;
my $strCommonBasePath;
my $strCommonTestPath;
my $strCommonDataPath;
my $strCommonRepoPath;
my $strCommonLocalPath;
my $strCommonDbPath;
my $strCommonDbCommonPath;
my $strCommonDbTablespacePath;
my $iCommonDbPort;
my $strCommonDbVersion;
my $iModuleTestRunOnly;
my $bDryRun;
my $bNoCleanup;
my $bLogForce;

# Execution globals
my $strErrorLog;
my $hError;
my $strOutLog;
my $strFullLog;
my $bFullLog = false;
my $hOut;
my $pId;
my $strCommand;

# Test globals
my $strTestLog;
my $strModule;
my $strModuleTest;
my $iModuleTestRun;

####################################################################################################################################
# BackRestTestCommon_ClusterStop
####################################################################################################################################
sub BackRestTestCommon_ClusterStop
{
    my $strPath = shift;
    my $bImmediate = shift;

    # Set default
    $strPath = defined($strPath) ? $strPath : BackRestTestCommon_DbCommonPathGet();
    $bImmediate = defined($bImmediate) ? $bImmediate : false;

    # If postmaster process is running then stop the cluster
    if (-e $strPath . '/postmaster.pid')
    {
        BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl stop -D ${strPath} -w -s -m " .
                                   ($bImmediate ? 'immediate' : 'fast'));
    }
}

####################################################################################################################################
# BackRestTestCommon_DropRepo
####################################################################################################################################
sub BackRestTestCommon_DropRepo
{
    # Remove the backrest private directory
    while (-e BackRestTestCommon_RepoPathGet())
    {
        BackRestTestCommon_PathRemove(BackRestTestCommon_RepoPathGet(), true, true);
        BackRestTestCommon_PathRemove(BackRestTestCommon_RepoPathGet(), false, true);
        hsleep(.1);
    }
}


####################################################################################################################################
# BackRestTestCommon_CreateRepo
####################################################################################################################################
sub BackRestTestCommon_CreateRepo
{
    my $bRemote = shift;

    BackRestTestCommon_DropRepo();

    # Create the backup directory
    if ($bRemote)
    {
        BackRestTestCommon_Execute('mkdir -m 700 ' . BackRestTestCommon_RepoPathGet(), true);
    }
    else
    {
        BackRestTestCommon_PathCreate(BackRestTestCommon_RepoPathGet());
    }
}

####################################################################################################################################
# BackRestTestCommon_Drop
####################################################################################################################################
sub BackRestTestCommon_Drop
{
    # Drop the cluster if it exists
    BackRestTestCommon_ClusterStop(BackRestTestCommon_DbCommonPathGet(), true);

    # Remove the backrest private directory
    BackRestTestCommon_DropRepo();

    # Remove the test directory
    BackRestTestCommon_PathRemove(BackRestTestCommon_TestPathGet());
}

####################################################################################################################################
# BackRestTestCommon_Create
####################################################################################################################################
sub BackRestTestCommon_Create
{
    # Create the test directory
    BackRestTestCommon_PathCreate(BackRestTestCommon_TestPathGet(), '0770');
}

####################################################################################################################################
# BackRestTestCommon_Run
####################################################################################################################################
sub BackRestTestCommon_Run
{
    my $iRun = shift;
    my $strLog = shift;
    my $strModuleParam = shift;
    my $strModuleTestParam = shift;

    # &log(INFO, "module " . (defined($strModule) ? $strModule : ''));
    BackRestTestCommon_TestLog();

    if (defined($iModuleTestRunOnly) && $iModuleTestRunOnly != $iRun)
    {
        return false;
    }

    $strTestLog = 'run ' . sprintf('%03d', $iRun) . ' - ' . $strLog;

    &log(INFO, $strTestLog);

    if ($bDryRun)
    {
        return false;
    }

    $strFullLog = "${strTestLog}\n" . ('=' x length($strTestLog)) . "\n";

    $strModule = $strModuleParam;
    $strModuleTest = $strModuleTestParam;
    $iModuleTestRun = $iRun;

    return true;
}

####################################################################################################################################
# BackRestTestCommon_Cleanup
####################################################################################################################################
sub BackRestTestCommon_Cleanup
{
    BackRestTestCommon_TestLog();

    return !$bNoCleanup && !$bDryRun;
}

####################################################################################################################################
# BackRestTestCommon_TestLog
####################################################################################################################################
sub BackRestTestCommon_TestLog
{
    if (defined($strModule))
    {
        my $hFile;
        my $strLogFile = BackRestTestCommon_BasePathGet() .
                         sprintf("/test/log/${strModule}-${strModuleTest}-%03d.log", $iModuleTestRun);

        if (!$bLogForce && -e $strLogFile)
        {
            mkdir(BackRestTestCommon_TestPathGet() . '/log');

            my $strTestLogFile = BackRestTestCommon_TestPathGet() .
                                 sprintf("/log/${strModule}-${strModuleTest}-%03d.log", $iModuleTestRun);

            open($hFile, '>', $strTestLogFile)
                or die "Could not open file '${strTestLogFile}': $!";

            print $hFile $strFullLog;
            close $hFile;

            BackRestTestCommon_Execute("diff ${strLogFile} ${strTestLogFile}");
        }
        else
        {
            open($hFile, '>', $strLogFile)
                or die "Could not open file '${strLogFile}': $!";

            print $hFile $strFullLog;
            close $hFile;
        }

        undef($strModule);
    }
}

####################################################################################################################################
# BackRestTestCommon_ExecuteBegin
####################################################################################################################################
sub BackRestTestCommon_ExecuteBegin
{
    my $strCommandParam = shift;
    my $bRemote = shift;
    my $strComment = shift;

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

    my $strBinPath = dirname(dirname(abs_path($0))) . '/bin';
    $bFullLog = false;

    if ($strCommandParam =~ /\/bin\/pg_backrest\.pl/)
    {
        my $strTestPath = BackRestTestCommon_TestPathGet();

        $strCommandParam =~ s/$strBinPath/[BACKREST_BIN_PATH]/g;
        $strCommandParam =~ s/[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}(D|I)){0,1}/[BACKUP_LABEL]/;

        if (defined($strTestPath))
        {
            $strCommandParam =~ s/$strTestPath/[TEST_PATH]/g;
        }

        if (defined($strComment))
        {
            $strComment =~ s/[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}(D|I)){0,1}/[BACKUP_LABEL]/;
            $strFullLog .= "\n${strComment}";
        }

        $strFullLog .= "\n> ${strCommandParam}\n" . ('-' x '132') . "\n";
        $bFullLog = true;
    }

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

    # Get test path
    my $strTestPath = BackRestTestCommon_TestPathGet();
    my $strVersion = version_get();

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

                if ($bFullLog)
                {
                    $strLine =~ s/^[^ ]* [^ ]* [^ ]* //;
                    $strLine =~ s/[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}(D|I)){0,1}/[BACKUP_LABEL]/g;
                    $strLine =~ s/version = $strVersion/version = [VERSION]/g;
                    $strLine =~ s/modification_time = [0-9]+/modification_time = [MODIFICATION_TIME]/g;
                    $strLine =~ s/user = [^ \n,\[\]]+/user = [USER]/g;
                    $strLine =~ s/group = [^ \n,\[\]]+/group = [GROUP]/g;

                    if (defined($strTestPath))
                    {
                        $strLine =~ s/$strTestPath/[TEST_PATH]/g;
                    }

                    if ($strLine !~ /^  TEST/)
                    {
                        $strFullLog .= $strLine;
                    }
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

    if ($iExitStatus != 0 || (defined($iExpectedExitStatus) && $iExitStatus != $iExpectedExitStatus))
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
    my $strComment = shift;

    BackRestTestCommon_ExecuteBegin($strCommand, $bRemote, $strComment);
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

    # Set the mode
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

    # Set the mode
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

    BackRestTestCommon_Execute("cp -RpP ${strSourcePath} ${strDestinationPath}", $bRemote, $bSuppressError);
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

    # Set the mode
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
    my $iModuleTestRunOnlyParam = shift;
    my $bDryRunParam = shift;
    my $bNoCleanupParam = shift;
    my $bLogForceParam = shift;

    $strCommonBasePath = dirname(dirname(abs_path($0)));

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
        $strCommonTestPath = "${strCommonBasePath}/test/test";
    }

    $strCommonDataPath = "${strCommonBasePath}/test/data";
    $strCommonRepoPath = "${strCommonTestPath}/backrest";
    $strCommonLocalPath = "${strCommonTestPath}/local";
    $strCommonDbPath = "${strCommonTestPath}/db";
    $strCommonDbCommonPath = "${strCommonTestPath}/db/common";
    $strCommonDbTablespacePath = "${strCommonTestPath}/db/tablespace";

    $strCommonCommandMain = "../bin/pg_backrest.pl";
    $strCommonCommandRemote = "${strCommonBasePath}/bin/pg_backrest_remote.pl";
    $strCommonCommandPsql = "${strPgSqlBin}/psql -X %option% -h ${strCommonDbPath}";

    $iCommonDbPort = 6543;
    $iModuleTestRunOnly = $iModuleTestRunOnlyParam;
    $bDryRun = $bDryRunParam;
    $bNoCleanup = $bNoCleanupParam;
    $bLogForce = $bLogForceParam;

    BackRestTestCommon_Execute($strPgSqlBin . '/postgres --version');

    # Get the Postgres version
    my @stryVersionToken = split(/ /, $strOutLog);
    @stryVersionToken = split(/\./, $stryVersionToken[2]);
    $strCommonDbVersion = $stryVersionToken[0] . '.' . $stryVersionToken[1];

    # Don't run unit tests for unsupported versions
    my $strVersionSupport = versionSupport();

    if ($strCommonDbVersion < ${$strVersionSupport}[0])
    {
        confess "currently only version ${$strVersionSupport}[0] and up are supported";
    }
}

####################################################################################################################################
# BackRestTestCommon_manifestLoad
####################################################################################################################################
sub BackRestTestCommon_manifestLoad
{
    my $strFileName = shift;
    my $bRemote = shift;

    # Defaults
    $bRemote = defined($bRemote) ? $bRemote : false;

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g+x " . BackRestTestCommon_RepoPathGet(), $bRemote);
    }

    my $oManifest = new BackRest::Manifest($strFileName);

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g-x " . BackRestTestCommon_RepoPathGet(), $bRemote);
    }

    return $oManifest;
}

####################################################################################################################################
# BackRestTestCommon_manifestSave
####################################################################################################################################
sub BackRestTestCommon_manifestSave
{
    my $strFileName = shift;
    my $oManifest = shift;
    my $bRemote = shift;

    # Defaults
    $bRemote = defined($bRemote) ? $bRemote : false;

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g+x " . BackRestTestCommon_RepoPathGet(), $bRemote);
        BackRestTestCommon_Execute("chmod g+w " . $strFileName, $bRemote);
    }

    $oManifest->save();

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g-w " . $strFileName, $bRemote);
        BackRestTestCommon_Execute("chmod g-x " . BackRestTestCommon_RepoPathGet(), $bRemote);
    }
}

####################################################################################################################################
# BackRestTestCommon_iniLoad
####################################################################################################################################
sub BackRestTestCommon_iniLoad
{
    my $strFileName = shift;
    my $oIniRef = shift;
    my $bRemote = shift;

    # Defaults
    $bRemote = defined($bRemote) ? $bRemote : false;

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g+x " . BackRestTestCommon_RepoPathGet(), $bRemote);
    }

    ini_load($strFileName, $oIniRef);

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g-x " . BackRestTestCommon_RepoPathGet(), $bRemote);
    }
}

####################################################################################################################################
# BackRestTestCommon_iniSave
####################################################################################################################################
sub BackRestTestCommon_iniSave
{
    my $strFileName = shift;
    my $oIniRef = shift;
    my $bRemote = shift;

    # Defaults
    $bRemote = defined($bRemote) ? $bRemote : false;

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g+x " . BackRestTestCommon_RepoPathGet(), $bRemote);
        BackRestTestCommon_Execute("chmod g+w " . $strFileName, $bRemote);
    }

    ini_save($strFileName, $oIniRef);

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g-w " . $strFileName, $bRemote);
        BackRestTestCommon_Execute("chmod g-x " . BackRestTestCommon_RepoPathGet(), $bRemote);
    }
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
        BackRestTestCommon_Execute("mv " . BackRestTestCommon_RepoPathGet() . "/pg_backrest.conf ${strRemoteConfigFile}", true);
        ini_load($strRemoteConfigFile, \%oRemoteConfig);
    }

    # Rewrite remap section
    delete($oConfig{"${strStanza}:restore:tablespace-map"});

    foreach my $strRemap (sort(keys $oRemapHashRef))
    {
        my $strRemapPath = ${$oRemapHashRef}{$strRemap};

        if ($strRemap eq 'base')
        {
            $oConfig{$strStanza}{'db-path'} = $strRemapPath;
            ${$oManifestRef}{'backup:path'}{base} = $strRemapPath;

            if ($bRemote)
            {
                $oRemoteConfig{$strStanza}{'db-path'} = $strRemapPath;
            }
        }
        else
        {
            $oConfig{"${strStanza}:restore:tablespace-map"}{$strRemap} = $strRemapPath;

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
        BackRestTestCommon_Execute("mv ${strRemoteConfigFile} " . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf', true);
    }
}

####################################################################################################################################
# BackRestTestCommon_ConfigRecovery
####################################################################################################################################
sub BackRestTestCommon_ConfigRecovery
{
    my $oRecoveryHashRef = shift;
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
        BackRestTestCommon_Execute("mv " . BackRestTestCommon_RepoPathGet() . "/pg_backrest.conf ${strRemoteConfigFile}", true);
        ini_load($strRemoteConfigFile, \%oRemoteConfig);
    }

    # Rewrite remap section
    delete($oConfig{"${strStanza}:restore:recovery-setting"});

    foreach my $strOption (sort(keys $oRecoveryHashRef))
    {
        $oConfig{"${strStanza}:restore:recovery-setting"}{$strOption} = ${$oRecoveryHashRef}{$strOption};
    }

    # Resave the config file
    ini_save($strConfigFile, \%oConfig);

    # Load remote config file
    if ($bRemote)
    {
        ini_save($strRemoteConfigFile, \%oRemoteConfig);
        BackRestTestCommon_Execute("mv ${strRemoteConfigFile} " . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf', true);
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
    my $bArchiveAsync = shift;
    my $bCompressAsync = shift;

    my %oParamHash;

    if (defined($strRemote))
    {
        $oParamHash{'global:command'}{'cmd-remote'} = $strCommonCommandRemote;
    }

    $oParamHash{'global:command'}{'cmd-psql'} = $strCommonCommandPsql;

    if (defined($strRemote) && $strRemote eq BACKUP)
    {
        $oParamHash{'global:backup'}{'backup-host'} = $strCommonHost;
        $oParamHash{'global:backup'}{'backup-user'} = $strCommonUserBackRest;
    }
    elsif (defined($strRemote) && $strRemote eq DB)
    {
        $oParamHash{$strCommonStanza}{'db-host'} = $strCommonHost;
        $oParamHash{$strCommonStanza}{'db-user'} = $strCommonUser;
    }

    $oParamHash{'global:log'}{'log-level-console'} = 'debug';
    $oParamHash{'global:log'}{'log-level-file'} = 'trace';

    if ($strLocal eq BACKUP)
    {
        $oParamHash{'global:general'}{'repo-path'} = $strCommonRepoPath;
    }
    elsif ($strLocal eq DB)
    {
        $oParamHash{'global:general'}{'repo-path'} = $strCommonLocalPath;

        if (defined($strRemote))
        {
#            $oParamHash{'global:log'}{'log-level-console'} = 'trace';

            # if ($bArchiveAsync)
            # {
            #     $oParamHash{'global:archive'}{path} = BackRestTestCommon_LocalPathGet();
            # }

            $oParamHash{'global:general'}{'repo-remote-path'} = $strCommonRepoPath;
        }
        else
        {
            $oParamHash{'global:general'}{'repo-path'} = $strCommonRepoPath;
        }

        if ($bArchiveAsync)
        {
            $oParamHash{'global:archive'}{'archive-async'} = 'y';
            #
            # if (!$bCompressAsync)
            # {
            #     $oParamHash{'global:archive'}{'compress_async'} = 'n';
            # }
        }
    }
    else
    {
        confess "invalid local type ${strLocal}";
    }

    if (defined($iThreadMax) && $iThreadMax > 1)
    {
        $oParamHash{'global:general'}{'thread-max'} = $iThreadMax;
    }

    if (($strLocal eq BACKUP) || ($strLocal eq DB && !defined($strRemote)))
    {
        $oParamHash{'db:command'}{'cmd-psql-option'} = "--port=${iCommonDbPort}";
        $oParamHash{'global:backup'}{'thread-max'} = $iThreadMax;

        if (defined($bHardlink) && $bHardlink)
        {
            $oParamHash{'global:backup'}{'hardlink'} = 'y';
        }
    }

    if (defined($bCompress) && !$bCompress)
    {
        $oParamHash{'global:general'}{'compress'} = 'n';
    }

    # if (defined($bChecksum) && $bChecksum)
    # {
    #     $oParamHash{'global:backup'}{'checksum'} = 'y';
    # }

    $oParamHash{$strCommonStanza}{'db-path'} = $strCommonDbCommonPath;

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
        rename($strFile, BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf')
            or die "unable to move ${strFile} to " . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf path';
    }
    else
    {
        BackRestTestCommon_Execute("mv ${strFile} " . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf', true);
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

sub BackRestTestCommon_CommandPsqlGet
{
    return $strCommonCommandPsql;
}

sub BackRestTestCommon_CommandMainGet
{
    return $strCommonCommandMain;
}

sub BackRestTestCommon_CommandMainAbsGet
{
    return abs_path($strCommonCommandMain);
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

sub BackRestTestCommon_BasePathGet
{
    return $strCommonBasePath;
}

sub BackRestTestCommon_TestPathGet
{
    return $strCommonTestPath;
}

sub BackRestTestCommon_DataPathGet
{
    return $strCommonDataPath;
}

sub BackRestTestCommon_RepoPathGet
{
    return $strCommonRepoPath;
}

sub BackRestTestCommon_LocalPathGet
{
    return $strCommonLocalPath;
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

sub BackRestTestCommon_DbVersion
{
    return $strCommonDbVersion;
}

1;
