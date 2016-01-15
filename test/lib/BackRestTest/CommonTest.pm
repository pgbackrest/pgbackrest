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

use Cwd qw(abs_path cwd);
use Exporter qw(import);
use File::Basename;
use File::Copy qw(move);
use File::Path qw(remove_tree);
use IO::Select;
use IPC::Open3;
use POSIX ':sys_wait_h';
use Symbol 'gensym';

use lib dirname($0) . '/../lib';
use BackRest::Common::Ini;
use BackRest::Common::Log;
use BackRest::Common::String;
use BackRest::Common::Wait;
use BackRest::Config::Config;
use BackRest::Db;
use BackRest::File;
use BackRest::Manifest;

use BackRestTest::Common::ExecuteTest;
use BackRestTest::Common::LogTest;

our @EXPORT = qw(BackRestTestCommon_Create BackRestTestCommon_Drop BackRestTestCommon_Setup
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
                 BackRestTestCommon_CommandRemoteFullGet BackRestTestCommon_BasePathGet BackRestTestCommon_LinkCreate);

my $strPgSqlBin;
my $strCommonStanza;
my $strCommonCommandMain;
my $bCommandMainSet = false;
my $strCommonCommandRemote;
my $strCommonCommandRemoteFull;
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

use constant PROTOCOL_TIMEOUT_TEST                                       => 30;
    push @EXPORT, qw(PROTOCOL_TIMEOUT_TEST);

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
        executeTest(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl stop -D ${strPath} -w -s -m " .
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
        waitHiRes(.1);
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
        executeTest('mkdir -m 700 ' . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
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
    my $oLogTestRef = shift;

    # Save the previous test log
    if (defined($$oLogTestRef))
    {
        $$oLogTestRef->logWrite(BackRestTestCommon_BasePathGet(), BackRestTestCommon_TestPathGet());
        $$oLogTestRef = undef;
    }

    # Return if this test should not be run
    if (defined($iModuleTestRunOnly) && $iModuleTestRunOnly != $iRun)
    {
        return false;
    }

    # Output information about test to run
    &log(INFO, 'run ' . sprintf('%03d', $iRun) . ' - ' . $strLog);

    if ($bDryRun)
    {
        return false;
    }

    # If the module is defined then create a LogTest object
    if (defined($strModuleParam))
    {
        $$oLogTestRef = new BackRestTest::Common::LogTest($strModuleParam, $strModuleTestParam, $iRun, $bLogForce, $strLog,
                                                          BackRestTestCommon_CommandMainGet(),
                                                          BackRestTestCommon_CommandMainAbsGet(),
                                                          BackRestTestCommon_PgSqlBinPathGet(),
                                                          BackRestTestCommon_TestPathGet(),
                                                          BackRestTestCommon_RepoPathGet());
    }

    return true;
}

####################################################################################################################################
# BackRestTestCommon_Cleanup
####################################################################################################################################
sub BackRestTestCommon_Cleanup
{
    my $oLogTestRef = shift;

    # Save the previous test log
    if (defined($$oLogTestRef))
    {
        $$oLogTestRef->logWrite(BackRestTestCommon_BasePathGet(), BackRestTestCommon_TestPathGet());
        $$oLogTestRef = undef;
    }

    # Return false if there is no cleanup or if this was a test run (this prevents cleanup from being run)
    return !$bNoCleanup && !$bDryRun;
}

####################################################################################################################################
# BackRestTestCommon_LinkCreate
#
# Create a symlink
####################################################################################################################################
sub BackRestTestCommon_LinkCreate
{
    my $strLink = shift;
    my $strDestination = shift;

    # Create the file
    symlink($strDestination, $strLink)
        or confess "unable to link ${strLink} to ${strDestination}";
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
    my $bIgnoreExists = shift;

    # Create the path
    if (!mkdir($strPath))
    {
        if (!(defined($bIgnoreExists) && $bIgnoreExists && -e $strPath))
        {
            confess "unable to create ${strPath} path";
        }
    }

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

    executeTest('rm -rf ' . $strPath,
                {bRemote => $bRemote, bSuppressError => $bSuppressError});

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

    executeTest("cp -RpP ${strSourcePath} ${strDestinationPath}",
                {bRemote => $bRemote, bSuppressError => $bSuppressError});
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
    my $strExe = shift;
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
        $strCommonTestPath = cwd() . "/test";
    }

    $strCommonDataPath = "${strCommonBasePath}/test/data";
    $strCommonRepoPath = "${strCommonTestPath}/backrest";
    $strCommonLocalPath = "${strCommonTestPath}/local";
    $strCommonDbPath = "${strCommonTestPath}/db";
    $strCommonDbCommonPath = "${strCommonTestPath}/db/common";
    $strCommonDbTablespacePath = "${strCommonTestPath}/db/tablespace";

    $strCommonCommandMain = defined($strExe) ? $strExe : $strCommonBasePath . "/bin/../bin/pg_backrest";
    $bCommandMainSet = defined($strExe) ? true : false;
    $strCommonCommandRemote = defined($strExe) ? $strExe : "${strCommonBasePath}/bin/pg_backrest";
    $strCommonCommandRemoteFull = "${strCommonCommandRemote} --stanza=${strCommonStanza}" .
                                  " --repo-remote-path=${strCommonRepoPath} --no-config --command=test remote";
    $strCommonCommandPsql = "${strPgSqlBin}/psql -X %option% -h ${strCommonDbPath}";

    $iCommonDbPort = 6543;
    $iModuleTestRunOnly = $iModuleTestRunOnlyParam;
    $bDryRun = $bDryRunParam;
    $bNoCleanup = $bNoCleanupParam;
    $bLogForce = $bLogForceParam;

    # Check the exe for warnings
    my $strWarning = trim(executeTest("perl -cW ${strCommonCommandRemote} 2>&1"));

    if ($strWarning ne "${strCommonCommandRemote} syntax OK")
    {
        confess &log(ERROR, "${strCommonCommandRemote} failed syntax check:\n${strWarning}");
    }

    # Get the Postgres version
    my $strVersionRegExp = '(devel|((alpha|beta|rc)[0-9]+))$';
    my $strOutLog = executeTest($strPgSqlBin . '/postgres --version');

    my @stryVersionToken = split(/ /, $strOutLog);
    @stryVersionToken = split(/\./, $stryVersionToken[2]);
    $strCommonDbVersion = $stryVersionToken[0] . '.' . trim($stryVersionToken[1]);

    # Warn if this is a devel/alpha/beta version
    if ($strCommonDbVersion =~ /$strVersionRegExp/)
    {
        my $strDevVersion = $strCommonDbVersion;
        $strCommonDbVersion =~ s/$strVersionRegExp//;
        $strDevVersion = substr($strDevVersion, length($strCommonDbVersion));

        &log(WARN, "Testing against ${strCommonDbVersion} ${strDevVersion} version");
    }

    # Don't run unit tests for unsupported versions
    my @stryVersionSupport = versionSupport();

    if ($strCommonDbVersion < $stryVersionSupport[0])
    {
        confess "currently only version $stryVersionSupport[0] and up are supported";
    }

    return true;
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
        executeTest("chmod g+x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
    }

    my $oManifest = new BackRest::Manifest($strFileName);

    if ($bRemote)
    {
        executeTest("chmod g-x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});

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
        executeTest("chmod g+x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
        executeTest("chmod g+w " . $strFileName,
                    {bRemote => true});
    }

    $oManifest->save();

    if ($bRemote)
    {
        executeTest("chmod g-w " . $strFileName,
                    {bRemote => true});
        executeTest("chmod g-x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
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
        executeTest("chmod g+x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
    }

    iniLoad($strFileName, $oIniRef);

    if ($bRemote)
    {
        executeTest("chmod g-x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
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
        executeTest("chmod g+x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
        executeTest("chmod g+w " . $strFileName,
                    {bRemote => true});
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
        executeTest("chmod g-w " . $strFileName,
                    {bRemote => true});
        executeTest("chmod g-x " . BackRestTestCommon_RepoPathGet(),
                    {bRemote => true});
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
        executeTest("mv " . BackRestTestCommon_RepoPathGet() . "/pg_backrest.conf ${strRemoteConfigFile}",
                    {bRemote => true});
        iniLoad($strRemoteConfigFile, \%oRemoteConfig, true);
    }

    # Rewrite remap section
    delete($oConfig{"${strStanza}:restore:tablespace-map"});

    foreach my $strRemap (sort(keys(%$oRemapHashRef)))
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
        executeTest("mv ${strRemoteConfigFile} " . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf',
                    {bRemote => true});
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
        executeTest("mv " . BackRestTestCommon_RepoPathGet() . "/pg_backrest.conf ${strRemoteConfigFile}",
                    {bRemote => true});
        iniLoad($strRemoteConfigFile, \%oRemoteConfig, true);
    }

    # Rewrite remap section
    delete($oConfig{"${strStanza}:restore:recovery-setting"});

    foreach my $strOption (sort(keys(%$oRecoveryHashRef)))
    {
        $oConfig{"${strStanza}:restore:recovery-setting"}{$strOption} = ${$oRecoveryHashRef}{$strOption};
    }

    # Resave the config file
    iniSave($strConfigFile, \%oConfig, true);

    # Load remote config file
    if ($bRemote)
    {
        iniSave($strRemoteConfigFile, \%oRemoteConfig, true);
        executeTest("mv ${strRemoteConfigFile} " . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf',
                    {bRemote => true});
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
        $oParamHash{'global:command'}{'[comment]'} = 'backrest command';
        $oParamHash{'global:command'}{'cmd-remote'} = $strCommonCommandRemote;
    }

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
        $oParamHash{'global:general'}{'config-remote'} = "${strCommonDbPath}/pg_backrest.conf";
    }
    elsif ($strLocal eq DB)
    {
        $oParamHash{'global:general'}{'repo-path'} = $strCommonLocalPath;
        $oParamHash{'global:general'}{'config-remote'} = "${strCommonRepoPath}/pg_backrest.conf";

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
        # $oParamHash{"${strCommonStanza}:command"}{'[comment]'} = 'cluster-specific command options';
        # $oParamHash{"${strCommonStanza}:command"}{'cmd-psql-option'} = "--port=${iCommonDbPort}";

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
    $oParamHash{$strCommonStanza}{'db-port'} = $iCommonDbPort;
    $oParamHash{$strCommonStanza}{'db-socket-path'} = BackRestTestCommon_DbPathGet();

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
        executeTest("mv ${strFile} " . BackRestTestCommon_RepoPathGet() . '/pg_backrest.conf',
                    {bRemote => true});
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
    if ($bCommandMainSet)
    {
        return BackRestTestCommon_CommandMainGet()
    }

    return abs_path(BackRestTestCommon_CommandMainGet());
}

sub BackRestTestCommon_CommandRemoteGet
{
    return $strCommonCommandRemote;
}

sub BackRestTestCommon_CommandRemoteFullGet
{
    return $strCommonCommandRemoteFull;
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
