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

use Cwd 'abs_path';
use Exporter qw(import);
use File::Basename;
use File::Copy qw(move);
use File::Path qw(remove_tree);
use IO::Select;
use IPC::Open3;
use POSIX ':sys_wait_h';

use lib dirname($0) . '/../lib';
use BackRest::Config;
use BackRest::Db;
use BackRest::File;
use BackRest::Ini;
use BackRest::Manifest;
use BackRest::Remote;
use BackRest::Utility;

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
                 BackRestTestCommon_manifestLoad BackRestTestCommon_manifestSave BackRestTestCommon_CommandMainAbsGet
                 BackRestTestCommon_TestLogAppendFile);

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
my $bExecuteRemote;
my $strErrorLog;
my $hError;
my $strOutLog;
my $strFullLog;
my $bFullLog = false;
my $hOut;
my $pId;
my $strCommand;
my $oReplaceHash = {};

# Test globals
my $strTestLog;
my $strModule;
my $strModuleTest;
my $iModuleTestRun;
my $bValidWalChecksum;

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
    my $bValidWalChecksumParam = shift;

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
    $oReplaceHash = {};

    $strModule = $strModuleParam;
    $strModuleTest = $strModuleTestParam;
    $iModuleTestRun = $iRun;
    $bValidWalChecksum = defined($bValidWalChecksumParam) ? $bValidWalChecksumParam : true;

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
# BackRestTestCommon_TestLogAppendFile
####################################################################################################################################
sub BackRestTestCommon_TestLogAppendFile
{
    my $strFileName = shift;
    my $bRemote = shift;

    if (defined($strModule))
    {
        my $hFile;

        if ($bRemote)
        {
            BackRestTestCommon_Execute("chmod g+x " . BackRestTestCommon_RepoPathGet(), $bRemote);
        }

        open($hFile, '<', $strFileName)
            or confess &log(ERROR, "unable to open ${strFileName} for appending to test log");

        my $strHeader = "+ supplemental file: " . BackRestTestCommon_ExecuteRegAll($strFileName);

        $strFullLog .= "\n${strHeader}\n" . ('-' x length($strHeader)) . "\n";

        while (my $strLine = readline($hFile))
        {
            $strLine = BackRestTestCommon_ExecuteRegAll($strLine);
            $strFullLog .= $strLine;
        }

        close($hFile);

        if ($bRemote)
        {
            BackRestTestCommon_Execute("chmod g-x " . BackRestTestCommon_RepoPathGet(), $bRemote);
        }
    }
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
    $bExecuteRemote = defined($bRemote) ? $bRemote : false;

    if ($bExecuteRemote)
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

    $bFullLog = false;

    if (defined($strModule) && $strCommandParam =~ /\/bin\/pg_backrest/)
    {
        $strCommandParam = BackRestTestCommon_ExecuteRegAll($strCommandParam);

        if (defined($strComment))
        {
            $strComment = BackRestTestCommon_ExecuteRegAll($strComment);
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
# BackRestTestCommon_ExecuteRegExp
####################################################################################################################################
sub BackRestTestCommon_ExecuteRegExp
{
    my $strLine = shift;
    my $strType = shift;
    my $strExpression = shift;
    my $strToken = shift;
    my $bIndex = shift;

    my @stryReplace = ($strLine =~ /$strExpression/g);

    foreach my $strReplace (@stryReplace)
    {
        my $iIndex;
        my $strTypeReplacement;
        my $strReplacement;

        if (!defined($bIndex) || $bIndex)
        {
            if (defined($strToken))
            {
                my @stryReplacement = ($strReplace =~ /$strToken/g);

                if (@stryReplacement != 1)
                {
                    confess &log(ASSERT, "'${strToken}' is not a sub-regexp of '${strExpression}' or matches multiple times on '${strReplace}'");
                }

                $strReplacement = $stryReplacement[0];
            }
            else
            {
                $strReplacement = $strReplace;
            }

            if (defined($$oReplaceHash{$strType}{$strReplacement}))
            {
                $iIndex = $$oReplaceHash{$strType}{$strReplacement}{index};
            }
            else
            {
                if (!defined($$oReplaceHash{$strType}{index}))
                {
                    $$oReplaceHash{$strType}{index} = 1;
                }

                $iIndex = $$oReplaceHash{$strType}{index}++;
                $$oReplaceHash{$strType}{$strReplacement}{index} = $iIndex;
            }
        }

        $strTypeReplacement = "[${strType}" . (defined($iIndex) ? "-${iIndex}" : '') . ']';

        if (defined($strToken))
        {
            $strReplacement = $strReplace;
            $strReplacement =~ s/$strToken/$strTypeReplacement/;
        }
        else
        {
            $strReplacement = $strTypeReplacement;
        }

        $strLine =~ s/$strReplace/$strReplacement/g;
    }

    return $strLine;
}

####################################################################################################################################
# BackRestTestCommon_ExecuteRegExpAll
####################################################################################################################################
sub BackRestTestCommon_ExecuteRegAll
{
    my $strLine = shift;

    my $strBinPath = dirname(dirname(abs_path($0))) . '/bin';

    $strLine =~ s/$strBinPath/[BACKREST_BIN_PATH]/g;
    $strLine =~ s/$strPgSqlBin/[PGSQL_BIN_PATH]/g;

    my $strTestPath = BackRestTestCommon_TestPathGet();

    if (defined($strTestPath))
    {
        $strLine =~ s/$strTestPath/[TEST_PATH]/g;
    }

    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'MODIFICATION-TIME', 'modification_time = [0-9]+', '[0-9]+$');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'TIMESTAMP', 'timestamp"[ ]{0,1}:[ ]{0,1}[0-9]+','[0-9]+$');

    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'BACKUP-INCR', '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}I');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'BACKUP-DIFF', '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}D');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'BACKUP-FULL', '[0-9]{8}\-[0-9]{6}F');

    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'GROUP', 'group = [^ \n,\[\]]+', '[^ \n,\[\]]+$');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'GROUP', 'group"[ ]{0,1}:[ ]{0,1}"[^"]+', '[^"]+$');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'USER', 'user = [^ \n,\[\]]+', '[^ \n,\[\]]+$');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'USER', 'user"[ ]{0,1}:[ ]{0,1}"[^"]+', '[^"]+$');

    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'VERSION', 'version[ ]{0,1}=[ ]{0,1}\"' . version_get(), version_get . '$');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'VERSION', '"version"[ ]{0,1}:[ ]{0,1}\"' . version_get(), version_get . '$');

    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'FORMAT', 'format=' . FORMAT, FORMAT . '$');
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'FORMAT', '"format"[ ]{0,1}:[ ]{0,1}' . FORMAT, FORMAT . '$');

    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'PORT', '--port=[0-9]+', '[0-9]+$');

    my $strTimestampRegExp = "[0-9]{4}-[0-1][0-9]-[0-3][0-9] [0-2][0-9]:[0-6][0-9]:[0-6][0-9]";

    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'TIMESTAMP',
        "timestamp-[a-z-]+[\"]{0,1}[ ]{0,1}[\:\=)]{1}[ ]{0,1}[\"]{0,1}[0-9]+", '[0-9]+$', false);
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'TIMESTAMP',
        "start\" : [0-9]{10}", '[0-9]{10}$', false);
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'TIMESTAMP',
        "stop\" : [0-9]{10}", '[0-9]{10}$', false);
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'TIMESTAMP-STR', "est backup timestamp: $strTimestampRegExp",
                                                "${strTimestampRegExp}\$", false);
    $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'CHECKSUM', 'checksum=[\"]{0,1}[0-f]{40}', '[0-f]{40}$', false);

    if (!$bValidWalChecksum)
    {
        $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'CHECKSUM', '[0-F]{24}-[0-f]{40}', '[0-f]{40}$', false);
        $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'DB-SYSTEM-ID',
            "db-system-id[\"]{0,1}[ ]{0,1}[\:\=)]{1}[ ]{0,1}[0-9]+", '[0-9]+$');
        $strLine = BackRestTestCommon_ExecuteRegExp($strLine, 'BACKUP-INFO',
            "backup-info-[a-z-]+[\"]{0,1}[ ]{0,1}[\:\=)]{1}[ ]{0,1}[0-9]+", '[0-9]+$', false);
    }

    return $strLine;
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

                if ($bFullLog)
                {
                    $strLine =~ s/^[0-9]{4}-[0-1][0-9]-[0-3][0-9] [0-2][0-9]:[0-6][0-9]:[0-6][0-9]\.[0-9]{3} T[0-9]{2} //;

                    if ($strLine !~ /^  TEST/ && $strLine !~ /\r$/)
                    {
                        $strLine =~ s/^                            //;
                        $strLine =~ s/^ //;

                        $strLine = BackRestTestCommon_ExecuteRegAll($strLine);
                        $strFullLog .= $strLine;
                    }
                }
            }
        }
    }

    # Check the exit status and output an error if needed
    my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

    if (defined($iExpectedExitStatus) && ($iExitStatus == $iExpectedExitStatus || $bExecuteRemote && $iExitStatus != 0))
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

    $strCommonCommandMain = "../bin/pg_backrest";
    $strCommonCommandRemote = "${strCommonBasePath}/bin/pg_backrest_remote";
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

    iniLoad($strFileName, $oIniRef);

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
    my $bChecksum = shift;

    # Defaults
    $bRemote = defined($bRemote) ? $bRemote : false;

    if ($bRemote)
    {
        BackRestTestCommon_Execute("chmod g+x " . BackRestTestCommon_RepoPathGet(), $bRemote);
        BackRestTestCommon_Execute("chmod g+w " . $strFileName, $bRemote);
    }

    # Calculate a new checksum if requested
    if (defined($bChecksum) && $bChecksum)
    {
        delete($$oIniRef{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});

        my $oSHA = Digest::SHA->new('sha1');
        my $oJSON = JSON::PP->new()->canonical()->allow_nonref();
        $oSHA->add($oJSON->encode($oIniRef));

        $$oIniRef{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} = $oSHA->hexdigest();
    }

    iniSave($strFileName, $oIniRef);

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
    iniLoad($strConfigFile, \%oConfig, true);

    # Load remote config file
    my %oRemoteConfig;
    my $strRemoteConfigFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf.remote';

    if ($bRemote)
    {
        BackRestTestCommon_Execute("mv " . BackRestTestCommon_RepoPathGet() . "/pg_backrest.conf ${strRemoteConfigFile}", true);
        iniLoad($strRemoteConfigFile, \%oRemoteConfig, true);
    }

    # Rewrite remap section
    delete($oConfig{"${strStanza}:restore:tablespace-map"});

    foreach my $strRemap (sort(keys $oRemapHashRef))
    {
        my $strRemapPath = ${$oRemapHashRef}{$strRemap};

        if ($strRemap eq 'base')
        {
            $oConfig{$strStanza}{'db-path'} = $strRemapPath;
            ${$oManifestRef}{'backup:path'}{base}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;

            if ($bRemote)
            {
                $oRemoteConfig{$strStanza}{'db-path'} = $strRemapPath;
            }
        }
        else
        {
            $oConfig{"${strStanza}:restore:tablespace-map"}{$strRemap} = $strRemapPath;

            ${$oManifestRef}{'backup:path'}{"tablespace/${strRemap}"}{&MANIFEST_SUBKEY_PATH} = $strRemapPath;
            ${$oManifestRef}{'base:link'}{"pg_tblspc/${strRemap}"}{destination} = $strRemapPath;
        }
    }

    # Resave the config file
    iniSave($strConfigFile, \%oConfig, true);

    # Load remote config file
    if ($bRemote)
    {
        iniSave($strRemoteConfigFile, \%oRemoteConfig, true);
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
    iniLoad($strConfigFile, \%oConfig, true);

    # Load remote config file
    my %oRemoteConfig;
    my $strRemoteConfigFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf.remote';

    if ($bRemote)
    {
        BackRestTestCommon_Execute("mv " . BackRestTestCommon_RepoPathGet() . "/pg_backrest.conf ${strRemoteConfigFile}", true);
        iniLoad($strRemoteConfigFile, \%oRemoteConfig, true);
    }

    # Rewrite remap section
    delete($oConfig{"${strStanza}:restore:recovery-setting"});

    foreach my $strOption (sort(keys $oRecoveryHashRef))
    {
        $oConfig{"${strStanza}:restore:recovery-setting"}{$strOption} = ${$oRecoveryHashRef}{$strOption};
    }

    # Resave the config file
    iniSave($strConfigFile, \%oConfig, true);

    # Load remote config file
    if ($bRemote)
    {
        iniSave($strRemoteConfigFile, \%oRemoteConfig, true);
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

    $oParamHash{'global:command'}{'[comment]'} = 'psql command and options';
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

    $oParamHash{'global:log'}{'[comment]'} = 'file and console log settings';
    $oParamHash{'global:log'}{'log-level-console'} = 'debug';
    $oParamHash{'global:log'}{'log-level-file'} = 'trace';

    $oParamHash{'global:general'}{'[comment]'} = 'general settings for all operations';

    if ($strLocal eq BACKUP)
    {
        $oParamHash{'global:general'}{'repo-path'} = $strCommonRepoPath;
    }
    elsif ($strLocal eq DB)
    {
        $oParamHash{'global:general'}{'repo-path'} = $strCommonLocalPath;

        if (defined($strRemote))
        {
            $oParamHash{'global:general'}{'repo-remote-path'} = $strCommonRepoPath;
        }
        else
        {
            $oParamHash{'global:general'}{'repo-path'} = $strCommonRepoPath;
        }

        if ($bArchiveAsync)
        {
            $oParamHash{'global:archive'}{'[comment]'} = 'WAL archive settings';
            $oParamHash{'global:archive'}{'archive-async'} = 'y';
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
        $oParamHash{"${strCommonStanza}:command"}{'[comment]'} = 'cluster-specific command options';
        $oParamHash{"${strCommonStanza}:command"}{'cmd-psql-option'} = "--port=${iCommonDbPort}";

        if (defined($bHardlink) && $bHardlink)
        {
            $oParamHash{'global:backup'}{'hardlink'} = 'y';
        }

        $oParamHash{'global:backup'}{'archive-copy'} = 'y';
        $oParamHash{'global:backup'}{'start-fast'} = 'y';
    }

    if (defined($bCompress) && !$bCompress)
    {
        $oParamHash{'global:general'}{'compress'} = 'n';
    }

    # Stanza settings
    $oParamHash{$strCommonStanza}{'[comment]'} = "cluster-specific settings";
    $oParamHash{$strCommonStanza}{'db-path'} = $strCommonDbCommonPath;

    # Comments
    if (defined($oParamHash{'global:backup'}))
    {
        $oParamHash{'global:backup'}{'[comment]'} = "backup settings";
    }

    # Write out the configuration file
    my $strFile = BackRestTestCommon_TestPathGet() . '/pg_backrest.conf';
    iniSave($strFile, \%oParamHash, true);

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
