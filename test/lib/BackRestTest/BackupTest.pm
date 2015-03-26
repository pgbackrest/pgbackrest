#!/usr/bin/perl
####################################################################################################################################
# BackupTest.pl - Unit Tests for BackRest::Backup and BackRest::Restore
####################################################################################################################################
package BackRestTest::BackupTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename;
use File::Copy 'cp';
use File::stat;
use Fcntl ':mode';
use Time::HiRes qw(gettimeofday);
use DBI;

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;
use BackRest::Config;
use BackRest::Manifest;
use BackRest::File;
use BackRest::Remote;

use BackRestTest::CommonTest;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestBackup_Test);

my $strTestPath;
my $strHost;
my $strUser;
my $strGroup;
my $strUserBackRest;
my $hDb;

####################################################################################################################################
# BackRestTestBackup_PgConnect
####################################################################################################################################
sub BackRestTestBackup_PgConnect
{
    # Disconnect user session
    BackRestTestBackup_PgDisconnect();

    # Connect to the db (whether it is local or remote)
    $hDb = DBI->connect('dbi:Pg:dbname=postgres;port=' . BackRestTestCommon_DbPortGet .
                        ';host=' . BackRestTestCommon_DbPathGet(),
                        BackRestTestCommon_UserGet(),
                        undef,
                        {AutoCommit => 0, RaiseError => 1});
}

####################################################################################################################################
# BackRestTestBackup_PgDisconnect
####################################################################################################################################
sub BackRestTestBackup_PgDisconnect
{
    # Connect to the db (whether it is local or remote)
    if (defined($hDb))
    {
        $hDb->disconnect;
        undef($hDb);
    }
}

####################################################################################################################################
# BackRestTestBackup_PgExecute
####################################################################################################################################
sub BackRestTestBackup_PgExecute
{
    my $strSql = shift;
    my $bCheckpoint = shift;
    my $bCommit = shift;

    # Set defaults
    $bCommit = defined($bCommit) ? $bCommit : true;

    # Log and execute the statement
    &log(DEBUG, "SQL: ${strSql}");
    my $hStatement = $hDb->prepare($strSql);

    $hStatement->execute() or
        confess &log(ERROR, "Unable to execute: ${strSql}");

    $hStatement->finish();

    if ($bCommit)
    {
        BackRestTestBackup_PgExecute('commit', false, false);
    }

    # Perform a checkpoint if requested
    if (defined($bCheckpoint) && $bCheckpoint)
    {
        BackRestTestBackup_PgExecute('checkpoint');
    }
}

####################################################################################################################################
# BackRestTestBackup_PgSwitchXlog
####################################################################################################################################
sub BackRestTestBackup_PgSwitchXlog
{
    BackRestTestBackup_PgExecute('select pg_switch_xlog()', false, false);
    BackRestTestBackup_PgExecute('select pg_switch_xlog()', false, false);
}

####################################################################################################################################
# BackRestTestBackup_PgCommit
####################################################################################################################################
sub BackRestTestBackup_PgCommit
{
    my $bCheckpoint = shift;

    BackRestTestBackup_PgExecute('commit', $bCheckpoint, false);
}

####################################################################################################################################
# BackRestTestBackup_PgSelect
####################################################################################################################################
sub BackRestTestBackup_PgSelect
{
    my $strSql = shift;

    # Log and execute the statement
    &log(DEBUG, "SQL: ${strSql}");
    my $hStatement = $hDb->prepare($strSql);

    $hStatement = $hDb->prepare($strSql);

    $hStatement->execute() or
        confess &log(ERROR, "Unable to execute: ${strSql}");

    my @oyRow = $hStatement->fetchrow_array();

    $hStatement->finish();

    return @oyRow;
}

####################################################################################################################################
# BackRestTestBackup_PgSelectOne
####################################################################################################################################
sub BackRestTestBackup_PgSelectOne
{
    my $strSql = shift;

    return (BackRestTestBackup_PgSelect($strSql))[0];
}

####################################################################################################################################
# BackRestTestBackup_PgSelectOneTest
####################################################################################################################################
sub BackRestTestBackup_PgSelectOneTest
{
    my $strSql = shift;
    my $strExpectedValue = shift;
    my $iTimeout = shift;

    my $lStartTime = time();
    my $strActualValue;

    do
    {
        $strActualValue = BackRestTestBackup_PgSelectOne($strSql);

        if (defined($strActualValue) && $strActualValue eq $strExpectedValue)
        {
            return;
        }
    }
    while (defined($iTimeout) && (time() - $lStartTime) <= $iTimeout);

    confess "expected value '${strExpectedValue}' from '${strSql}' but actual was '" .
            (defined($strActualValue) ? $strActualValue : '[undef]') . "'";
}

####################################################################################################################################
# BackRestTestBackup_ClusterStop
####################################################################################################################################
sub BackRestTestBackup_ClusterStop
{
    my $strPath = shift;
    my $bImmediate = shift;

    # Set default
    $strPath = defined($strPath) ? $strPath : BackRestTestCommon_DbCommonPathGet();
    $bImmediate = defined($bImmediate) ? $bImmediate : false;

    # Disconnect user session
    BackRestTestBackup_PgDisconnect();

    # Drop the cluster
    BackRestTestCommon_ClusterStop
}

####################################################################################################################################
# BackRestTestBackup_ClusterStart
####################################################################################################################################
sub BackRestTestBackup_ClusterStart
{
    my $strPath = shift;
    my $iPort = shift;
    my $bHotStandby = shift;

    # Set default
    $iPort = defined($iPort) ? $iPort : BackRestTestCommon_DbPortGet();
    $strPath = defined($strPath) ? $strPath : BackRestTestCommon_DbCommonPathGet();
    $bHotStandby = defined($bHotStandby) ? $bHotStandby : false;

    # Make sure postgres is not running
    if (-e $strPath . '/postmaster.pid')
    {
        confess 'postmaster.pid exists';
    }

    # Creat the archive command
    my $strArchive = BackRestTestCommon_CommandMainGet() . ' --stanza=' . BackRestTestCommon_StanzaGet() .
                     ' --config=' . BackRestTestCommon_DbPathGet() . '/pg_backrest.conf archive-push %p';

    # Start the cluster
    BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl start -o \"-c port=${iPort}" .
                               ' -c checkpoint_segments=1' .
                               " -c wal_level=hot_standby -c archive_mode=on -c archive_command='${strArchive}'" .
                               ($bHotStandby ? ' -c hot_standby=on' : '') .
                               " -c unix_socket_directories='" . BackRestTestCommon_DbPathGet() . "'\" " .
                               "-D ${strPath} -l ${strPath}/postgresql.log -w -s");

    # Connect user session
    BackRestTestBackup_PgConnect();
}

####################################################################################################################################
# BackRestTestBackup_ClusterRestart
####################################################################################################################################
sub BackRestTestBackup_ClusterRestart
{
    my $strPath = BackRestTestCommon_DbCommonPathGet();

    # Disconnect user session
    BackRestTestBackup_PgDisconnect();

    # If postmaster process is running them stop the cluster
    if (-e $strPath . '/postmaster.pid')
    {
        BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl restart -D ${strPath} -w -s");
    }

    # Connect user session
    BackRestTestBackup_PgConnect();
}

####################################################################################################################################
# BackRestTestBackup_ClusterCreate
####################################################################################################################################
sub BackRestTestBackup_ClusterCreate
{
    my $strPath = shift;
    my $iPort = shift;

    BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/initdb -D ${strPath} -A trust");

    BackRestTestBackup_ClusterStart($strPath, $iPort);

    # Connect user session
    BackRestTestBackup_PgConnect();
}

####################################################################################################################################
# BackRestTestBackup_Drop
####################################################################################################################################
sub BackRestTestBackup_Drop
{
    my $bImmediate = shift;

    # Stop the cluster if one is running
    BackRestTestBackup_ClusterStop(BackRestTestCommon_DbCommonPathGet(), $bImmediate);

    # Drop the test path
    BackRestTestCommon_Drop();

    # # Remove the backrest private directory
    # while (-e BackRestTestCommon_RepoPathGet())
    # {
    #     BackRestTestCommon_PathRemove(BackRestTestCommon_RepoPathGet(), true, true);
    #     BackRestTestCommon_PathRemove(BackRestTestCommon_RepoPathGet(), false, true);
    #     hsleep(.1);
    # }
    #
    # # Remove the test directory
    # BackRestTestCommon_PathRemove(BackRestTestCommon_TestPathGet());
}

####################################################################################################################################
# BackRestTestBackup_Create
####################################################################################################################################
sub BackRestTestBackup_Create
{
    my $bRemote = shift;
    my $bCluster = shift;

    # Set defaults
    $bRemote = defined($bRemote) ? $bRemote : false;
    $bCluster = defined($bCluster) ? $bCluster : true;

    # Drop the old test directory
    BackRestTestBackup_Drop(true);

    # Create the test directory
    BackRestTestCommon_Create();

    # Create the db paths
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbPathGet());
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet());
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet(2));

    # Create tablespace paths
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet());
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet(1));
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet(1, 2));
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet(2));
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet(2, 2));

    # Create the archive directory
    if ($bRemote)
    {
        BackRestTestCommon_PathCreate(BackRestTestCommon_LocalPathGet());
    }

    # Create the backup directory
    if ($bRemote)
    {
        BackRestTestCommon_Execute('mkdir -m 700 ' . BackRestTestCommon_RepoPathGet(), true);
    }
    else
    {
        BackRestTestCommon_PathCreate(BackRestTestCommon_RepoPathGet());
    }

    # Create the cluster
    if ($bCluster)
    {
        BackRestTestBackup_ClusterCreate(BackRestTestCommon_DbCommonPathGet(), BackRestTestCommon_DbPortGet());
    }
}

####################################################################################################################################
# BackRestTestBackup_PathCreate
#
# Create a path specifying mode.
####################################################################################################################################
sub BackRestTestBackup_PathCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;
    my $strMode = shift;

    # Create final file location
    my $strFinalPath = ${$oManifestRef}{'backup:path'}{$strPath} . (defined($strSubPath) ? "/${strSubPath}" : '');

    # Create the path
    if (!(-e $strFinalPath))
    {
        BackRestTestCommon_PathCreate($strFinalPath, $strMode);
    }

    return $strFinalPath;
}

####################################################################################################################################
# BackRestTestBackup_PathMode
#
# Change the mode of a path.
####################################################################################################################################
sub BackRestTestBackup_PathMode
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;
    my $strMode = shift;

    # Create final file location
    my $strFinalPath = ${$oManifestRef}{'backup:path'}{$strPath} . (defined($strSubPath) ? "/${strSubPath}" : '');

    BackRestTestCommon_PathMode($strFinalPath, $strMode);

    return $strFinalPath;
}

####################################################################################################################################
# BackRestTestBackup_ManifestPathCreate
#
# Create a path specifying mode and add it to the manifest.
####################################################################################################################################
sub BackRestTestBackup_ManifestPathCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;
    my $strMode = shift;

    # Create final file location
    my $strFinalPath = BackRestTestBackup_PathCreate($oManifestRef, $strPath, $strSubPath, $strMode);

    # Stat the file
    my $oStat = lstat($strFinalPath);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat ${strSubPath}';
    }

    my $strManifestPath = defined($strSubPath) ? $strSubPath : '.';

    # Load file into manifest
    ${$oManifestRef}{"${strPath}:path"}{$strManifestPath}{group} = getgrgid($oStat->gid);
    ${$oManifestRef}{"${strPath}:path"}{$strManifestPath}{user} = getpwuid($oStat->uid);
    ${$oManifestRef}{"${strPath}:path"}{$strManifestPath}{mode} = sprintf('%04o', S_IMODE($oStat->mode));
}

####################################################################################################################################
# BackRestTestBackup_PathRemove
#
# Remove a path.
####################################################################################################################################
sub BackRestTestBackup_PathRemove
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;

    # Create final file location
    my $strFinalPath = ${$oManifestRef}{'backup:path'}{$strPath} . (defined($strSubPath) ? "/${strSubPath}" : '');

    # Create the path
    BackRestTestCommon_PathRemove($strFinalPath);

    return $strFinalPath;
}

####################################################################################################################################
# BackRestTestBackup_ManifestTablespaceCreate
#
# Create a tablespace specifying mode and add it to the manifest.
####################################################################################################################################
sub BackRestTestBackup_ManifestTablespaceCreate
{
    my $oManifestRef = shift;
    my $iOid = shift;
    my $strMode = shift;

    # Create final file location
    my $strPath = BackRestTestCommon_DbTablespacePathGet($iOid);

    # Create the path
    # if (!(-e $strPath))
    # {
    #     BackRestTestCommon_PathCreate($strPath, $strMode);
    # }

    # Stat the path
    my $oStat = lstat($strPath);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat path ${strPath}';
    }

    # Load path into manifest
    ${$oManifestRef}{"tablespace:${iOid}:path"}{'.'}{group} = getgrgid($oStat->gid);
    ${$oManifestRef}{"tablespace:${iOid}:path"}{'.'}{user} = getpwuid($oStat->uid);
    ${$oManifestRef}{"tablespace:${iOid}:path"}{'.'}{mode} = sprintf('%04o', S_IMODE($oStat->mode));

    # Create the link in pg_tblspc
    my $strLink = BackRestTestCommon_DbCommonPathGet() . "/pg_tblspc/${iOid}";

    symlink($strPath, $strLink)
        or confess "unable to link ${strLink} to ${strPath}";

    # Stat the link
    $oStat = lstat($strLink);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat link ${strLink}';
    }

    # Load link into the manifest
    ${$oManifestRef}{"base:link"}{"pg_tblspc/${iOid}"}{group} = getgrgid($oStat->gid);
    ${$oManifestRef}{"base:link"}{"pg_tblspc/${iOid}"}{user} = getpwuid($oStat->uid);
    ${$oManifestRef}{"base:link"}{"pg_tblspc/${iOid}"}{link_destination} = $strPath;

    # Load tablespace into the manifest
    ${$oManifestRef}{"backup:tablespace"}{$iOid}{link} = $iOid;
    ${$oManifestRef}{"backup:tablespace"}{$iOid}{path} = $strPath;

    ${$oManifestRef}{"backup:path"}{"tablespace:${iOid}"} = $strPath;
}

####################################################################################################################################
# BackRestTestBackup_ManifestTablespaceDrop
#
# Drop a tablespace add remove it from the manifest.
####################################################################################################################################
sub BackRestTestBackup_ManifestTablespaceDrop
{
    my $oManifestRef = shift;
    my $iOid = shift;
    my $iIndex = shift;

    # Remove tablespace path/file/link from manifest
    delete(${$oManifestRef}{"tablespace:${iOid}:path"});
    delete(${$oManifestRef}{"tablespace:${iOid}:link"});
    delete(${$oManifestRef}{"tablespace:${iOid}:file"});

    # Drop the link in pg_tblspc
    BackRestTestCommon_FileRemove(BackRestTestCommon_DbCommonPathGet($iIndex) . "/pg_tblspc/${iOid}");

    # Remove tablespace rom manifest
    delete(${$oManifestRef}{"base:link"}{"pg_tblspc/${iOid}"});
    delete(${$oManifestRef}{"backup:tablespace"}{$iOid});
    delete(${$oManifestRef}{"backup:path"}{"tablespace:${iOid}"});
}

####################################################################################################################################
# BackRestTestBackup_FileCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
sub BackRestTestBackup_FileCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $strContent = shift;
    my $lTime = shift;
    my $strMode = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{'backup:path'}{$strPath} . "/${strFile}";

    # Create the file
    BackRestTestCommon_FileCreate($strPathFile, $strContent, $lTime, $strMode);

    # Return path to created file
    return $strPathFile;
}

####################################################################################################################################
# BackRestTestBackup_ManifestFileCreate
#
# Create a file specifying content, mode, and time and add it to the manifest.
####################################################################################################################################
sub BackRestTestBackup_ManifestFileCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $strContent = shift;
    my $strChecksum = shift;
    my $lTime = shift;
    my $strMode = shift;

    # Create the file
    my $strPathFile = BackRestTestBackup_FileCreate($oManifestRef, $strPath, $strFile, $strContent, $lTime, $strMode);

    # Stat the file
    my $oStat = lstat($strPathFile);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat ${strFile}';
    }

    # Load file into manifest
    ${$oManifestRef}{"${strPath}:file"}{$strFile}{group} = getgrgid($oStat->gid);
    ${$oManifestRef}{"${strPath}:file"}{$strFile}{user} = getpwuid($oStat->uid);
    ${$oManifestRef}{"${strPath}:file"}{$strFile}{mode} = sprintf('%04o', S_IMODE($oStat->mode));
    ${$oManifestRef}{"${strPath}:file"}{$strFile}{modification_time} = $oStat->mtime;
    ${$oManifestRef}{"${strPath}:file"}{$strFile}{size} = $oStat->size;
    delete(${$oManifestRef}{"${strPath}:file"}{$strFile}{reference});

    if (defined($strChecksum))
    {
        ${$oManifestRef}{"${strPath}:file"}{$strFile}{checksum} = $strChecksum;
    }
}

####################################################################################################################################
# BackRestTestBackup_FileRemove
#
# Remove a file from disk.
####################################################################################################################################
sub BackRestTestBackup_FileRemove
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $bIgnoreMissing = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{'backup:path'}{$strPath} . "/${strFile}";

    # Remove the file
    if (!(defined($bIgnoreMissing) && $bIgnoreMissing && !(-e $strPathFile)))
    {
        BackRestTestCommon_FileRemove($strPathFile);
    }

    return $strPathFile;
}

####################################################################################################################################
# BackRestTestBackup_ManifestFileRemove
#
# Remove a file from disk and (optionally) the manifest.
####################################################################################################################################
sub BackRestTestBackup_ManifestFileRemove
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{'backup:path'}{$strPath} . "/${strFile}";

    # Remove the file
    BackRestTestBackup_FileRemove($oManifestRef, $strPath, $strFile, true);

    # Remove from manifest
    delete(${$oManifestRef}{"${strPath}:file"}{$strFile});
}

####################################################################################################################################
# BackRestTestBackup_ManifestReference
#
# Update all files that do not have a reference with the supplied reference.
####################################################################################################################################
sub BackRestTestBackup_ManifestReference
{
    my $oManifestRef = shift;
    my $strReference = shift;
    my $bClear = shift;

    # Set prior backup
    if (defined($strReference))
    {
        ${$oManifestRef}{backup}{prior} = $strReference;
    }
    else
    {
        delete(${$oManifestRef}{backup}{prior});
    }

    # Clear the reference list
    delete(${$oManifestRef}{backup}{reference});

    # Find all file sections
    foreach my $strSectionFile (sort(keys $oManifestRef))
    {
        # Skip non-file sections
        if ($strSectionFile !~ /\:file$/)
        {
            next;
        }

        foreach my $strFile (sort(keys ${$oManifestRef}{$strSectionFile}))
        {
            if (!defined($strReference))
            {
                delete(${$oManifestRef}{$strSectionFile}{$strFile}{reference});
            }
            elsif (defined($bClear) && $bClear)
            {
                if (defined(${$oManifestRef}{$strSectionFile}{$strFile}{reference}) &&
                    ${$oManifestRef}{$strSectionFile}{$strFile}{reference} ne $strReference)
                {
                    delete(${$oManifestRef}{$strSectionFile}{$strFile}{reference});
                }
            }
            elsif (!defined(${$oManifestRef}{$strSectionFile}{$strFile}{reference}))
            {
                ${$oManifestRef}{$strSectionFile}{$strFile}{reference} = $strReference;
            }
        }
    }
}

####################################################################################################################################
# BackRestTestBackup_LinkCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
sub BackRestTestBackup_LinkCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $strDestination = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{'backup:path'}{$strPath} . "/${strFile}";

    # Create the file
    symlink($strDestination, $strPathFile)
        or confess "unable to link ${strPathFile} to ${strDestination}";

    # Return path to created file
    return $strPathFile;
}

####################################################################################################################################
# BackRestTestBackup_LinkRemove
#
# Remove a link from disk.
####################################################################################################################################
# sub BackRestTestBackup_LinkRemove
# {
#     my $oManifestRef = shift;
#     my $strPath = shift;
#     my $strFile = shift;
#     my $bManifestRemove = shift;
#
#     # Create actual file location
#     my $strPathFile = ${$oManifestRef}{'backup:path'}{$strPath} . "/${strFile}";
#
#     # Remove the file
#     if (-e $strPathFile)
#     {
#         BackRestTestCommon_FileRemove($strPathFile);
#     }
#
#     # Remove from manifest
#     if (defined($bManifestRemove) && $bManifestRemove)
#     {
#         delete(${$oManifestRef}{"${strPath}:file"}{$strFile});
#     }
# }

####################################################################################################################################
# BackRestTestBackup_ManifestLinkCreate
#
# Create a link and add it to the manifest.
####################################################################################################################################
sub BackRestTestBackup_ManifestLinkCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $strDestination = shift;

    # Create the file
    my $strPathFile = BackRestTestBackup_LinkCreate($oManifestRef, $strPath, $strFile, $strDestination);

    # Stat the file
    my $oStat = lstat($strPathFile);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat ${strFile}';
    }

    # Load file into manifest
    ${$oManifestRef}{"${strPath}:link"}{$strFile}{group} = getgrgid($oStat->gid);
    ${$oManifestRef}{"${strPath}:link"}{$strFile}{user} = getpwuid($oStat->uid);
    ${$oManifestRef}{"${strPath}:link"}{$strFile}{link_destination} = $strDestination;
}

####################################################################################################################################
# BackRestTestBackup_LastBackup
####################################################################################################################################
sub BackRestTestBackup_LastBackup
{
    my $oFile = shift;

    my @stryBackup = $oFile->list(PATH_BACKUP_CLUSTER, undef, undef, 'reverse');

    if (!defined($stryBackup[1]))
    {
        confess 'no backup was found';
    }

    return $stryBackup[1];
}

####################################################################################################################################
# BackRestTestBackup_BackupBegin
####################################################################################################################################
sub BackRestTestBackup_BackupBegin
{
    my $strType = shift;
    my $strStanza = shift;
    my $bRemote = shift;
    my $strComment = shift;
    my $bSynthetic = shift;
    my $bTestPoint = shift;
    my $fTestDelay = shift;

    # Set defaults
    $bTestPoint = defined($bTestPoint) ? $bTestPoint : false;
    $fTestDelay = defined($fTestDelay) ? $fTestDelay : 0;

    &log(INFO, "    ${strType} backup" . (defined($strComment) ? " (${strComment})" : ''));

    BackRestTestCommon_ExecuteBegin(BackRestTestCommon_CommandMainGet() . ' --config=' .
                                    ($bRemote ? BackRestTestCommon_RepoPathGet() : BackRestTestCommon_DbPathGet()) .
                                    "/pg_backrest.conf" . ($bSynthetic ? " --no-start-stop" : '') .
                                    ($strType ne 'incr' ? " --type=${strType}" : '') .
                                    " --stanza=${strStanza} backup" . ($bTestPoint ? " --test --test-delay=${fTestDelay}": ''),
                                    $bRemote);
}

####################################################################################################################################
# BackRestTestBackup_BackupEnd
####################################################################################################################################
sub BackRestTestBackup_BackupEnd
{
    my $strType = shift;
    my $oFile = shift;
    my $bRemote = shift;
    my $strBackup = shift;
    my $oExpectedManifestRef = shift;
    my $bSynthetic = shift;
    my $iExpectedExitStatus = shift;

    my $iExitStatus = BackRestTestCommon_ExecuteEnd(undef, undef, undef, $iExpectedExitStatus);

    if (defined($iExpectedExitStatus))
    {
        return undef;
    }

    ${$oExpectedManifestRef}{backup}{type} = $strType;

    if (!defined($strBackup))
    {
        $strBackup = BackRestTestBackup_LastBackup($oFile);
    }

    if ($bSynthetic)
    {
        BackRestTestBackup_BackupCompare($oFile, $bRemote, $strBackup, $oExpectedManifestRef);
    }

    return $strBackup;
}

####################################################################################################################################
# BackRestTestBackup_BackupSynthetic
####################################################################################################################################
sub BackRestTestBackup_BackupSynthetic
{
    my $strType = shift;
    my $strStanza = shift;
    my $bRemote = shift;
    my $oFile = shift;
    my $oExpectedManifestRef = shift;
    my $strComment = shift;
    my $strTestPoint = shift;
    my $fTestDelay = shift;
    my $iExpectedExitStatus = shift;

    BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, $strComment, true, defined($strTestPoint), $fTestDelay);

    if (defined($strTestPoint))
    {
        BackRestTestCommon_ExecuteEnd($strTestPoint);
    }

    return BackRestTestBackup_BackupEnd($strType, $oFile, $bRemote, undef, $oExpectedManifestRef, true, $iExpectedExitStatus);
}

####################################################################################################################################
# BackRestTestBackup_Backup
####################################################################################################################################
sub BackRestTestBackup_Backup
{
    my $strType = shift;
    my $strStanza = shift;
    my $bRemote = shift;
    my $oFile = shift;
    my $strComment = shift;
    my $strTestPoint = shift;
    my $fTestDelay = shift;
    my $iExpectedExitStatus = shift;

    BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, $strComment, false, defined($strTestPoint), $fTestDelay);

    if (defined($strTestPoint))
    {
        BackRestTestCommon_ExecuteEnd($strTestPoint);
    }

    return BackRestTestBackup_BackupEnd($strType, $oFile, $bRemote, undef, undef, false, $iExpectedExitStatus);
}

####################################################################################################################################
# BackRestTestBackup_BackupCompare
####################################################################################################################################
sub BackRestTestBackup_BackupCompare
{
    my $oFile = shift;
    my $bRemote = shift;
    my $strBackup = shift;
    my $oExpectedManifestRef = shift;

    ${$oExpectedManifestRef}{backup}{label} = $strBackup;

    # Remove old reference list
    delete(${$oExpectedManifestRef}{backup}{reference});

    # Build the new reference list
    foreach my $strSectionFile (sort(keys $oExpectedManifestRef))
    {
        # Skip non-file sections
        if ($strSectionFile !~ /\:file$/)
        {
            next;
        }

        foreach my $strFile (sort(keys ${$oExpectedManifestRef}{$strSectionFile}))
        {
            if (defined(${$oExpectedManifestRef}{$strSectionFile}{$strFile}{reference}))
            {
                my $strFileReference = ${$oExpectedManifestRef}{$strSectionFile}{$strFile}{reference};

                if (!defined(${$oExpectedManifestRef}{backup}{reference}))
                {
                    ${$oExpectedManifestRef}{backup}{reference} = $strFileReference;
                }
                else
                {
                    if (${$oExpectedManifestRef}{backup}{reference} !~ /^$strFileReference|,$strFileReference/)
                    {
                        ${$oExpectedManifestRef}{backup}{reference} .= ",${strFileReference}";
                    }
                }
            }
        }
    }

    # Change mode on the backup path so it can be read
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_RepoPathGet(), true);
    }

    my %oActualManifest;
    ini_load($oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/backup.manifest', \%oActualManifest);

    ${$oExpectedManifestRef}{backup}{'timestamp-start'} = $oActualManifest{backup}{'timestamp-start'};
    ${$oExpectedManifestRef}{backup}{'timestamp-stop'} = $oActualManifest{backup}{'timestamp-stop'};
    ${$oExpectedManifestRef}{backup}{'timestamp-copy-start'} = $oActualManifest{backup}{'timestamp-copy-start'};
    ${$oExpectedManifestRef}{backup}{'checksum'} = $oActualManifest{backup}{'checksum'};
    ${$oExpectedManifestRef}{backup}{format} = FORMAT;

    my $strTestPath = BackRestTestCommon_TestPathGet();

    ini_save("${strTestPath}/actual.manifest", \%oActualManifest);
    ini_save("${strTestPath}/expected.manifest", $oExpectedManifestRef);

    BackRestTestCommon_Execute("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    # Change mode on the backup path back before unit tests continue
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 700 ' . BackRestTestCommon_RepoPathGet(), true);
    }

    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/expected.manifest");
    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/actual.manifest");
}

####################################################################################################################################
# BackRestTestBackup_ManifestMunge
#
# Allows for munging of the manifest while make it appear to be valid.  This is used to create various error conditions that should
# be caught by the unit tests.
####################################################################################################################################
sub BackRestTestBackup_ManifestMunge
{
    my $oFile = shift;
    my $bRemote = shift;
    my $strBackup = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $strValue = shift;

    # Make sure the new value is at least vaguely reasonable
    if (!defined($strSection) || !defined($strKey))
    {
        confess &log(ASSERT, 'strSection and strKey must be defined');
    }

    # Change mode on the backup path so it can be read/written
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_RepoPathGet(), true);
        BackRestTestCommon_Execute('chmod 770 ' . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/backup.manifest', true);
    }

    # Read the manifest
    my %oManifest;
    ini_load($oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/backup.manifest', \%oManifest);

    # Write in the munged value
    if (defined($strSubKey))
    {
        if (defined($strValue))
        {
            $oManifest{$strSection}{$strKey}{$strSubKey} = $strValue;
        }
        else
        {
            delete($oManifest{$strSection}{$strKey}{$strSubKey});
        }
    }
    else
    {
        if (defined($strValue))
        {
            $oManifest{$strSection}{$strKey} = $strValue;
        }
        else
        {
            delete($oManifest{$strSection}{$strKey});
        }
    }

    # Remove the old checksum
    delete($oManifest{backup}{checksum});

    my $oSHA = Digest::SHA->new('sha1');

    # Calculate the checksum from manifest values
    foreach my $strSection (sort(keys(%oManifest)))
    {
        $oSHA->add($strSection);

        foreach my $strKey (sort(keys($oManifest{$strSection})))
        {
            $oSHA->add($strKey);

            my $strValue = $oManifest{$strSection}{$strKey};

            if (!defined($strValue))
            {
                confess &log(ASSERT, "section ${strSection}, key ${$strKey} has undef value");
            }

            if (ref($strValue) eq "HASH")
            {
                foreach my $strSubKey (sort(keys($oManifest{$strSection}{$strKey})))
                {
                    my $strSubValue = $oManifest{$strSection}{$strKey}{$strSubKey};

                    if (!defined($strSubValue))
                    {
                        confess &log(ASSERT, "section ${strSection}, key ${strKey}, subkey ${strSubKey} has undef value");
                    }

                    $oSHA->add($strSubValue);
                }
            }
            else
            {
                $oSHA->add($strValue);
            }
        }
    }

    # Set the new checksum
    $oManifest{backup}{checksum} = $oSHA->hexdigest();

    # Resave the manifest
    ini_save($oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/backup.manifest', \%oManifest);

    # Change mode on the backup path back before unit tests continue
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 750 ' . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/backup.manifest', true);
        BackRestTestCommon_Execute('chmod 700 ' . BackRestTestCommon_RepoPathGet(), true);
    }
}

####################################################################################################################################
# BackRestTestBackup_Restore
####################################################################################################################################
sub BackRestTestBackup_Restore
{
    my $oFile = shift;
    my $strBackup = shift;
    my $strStanza = shift;
    my $bRemote = shift;
    my $oExpectedManifestRef = shift;
    my $oRemapHashRef = shift;
    my $bDelta = shift;
    my $bForce = shift;
    my $strType = shift;
    my $strTarget = shift;
    my $bTargetExclusive = shift;
    my $bTargetResume = shift;
    my $strTargetTimeline = shift;
    my $oRecoveryHashRef = shift;
    my $strComment = shift;
    my $iExpectedExitStatus = shift;

    # Set defaults
    $bDelta = defined($bDelta) ? $bDelta : false;
    $bForce = defined($bForce) ? $bForce : false;

    my $bSynthetic = defined($oExpectedManifestRef) ? true : false;

    &log(INFO, '        restore' .
               ($bDelta ? ' delta' : '') .
               ($bForce ? ', force' : '') .
               ($strBackup ne 'latest' ? ", backup '${strBackup}'" : '') .
               ($strType ? ", type '${strType}'" : '') .
               ($strTarget ? ", target '${strTarget}'" : '') .
               ($strTargetTimeline ? ", timeline '${strTargetTimeline}'" : '') .
               (defined($bTargetExclusive) && $bTargetExclusive ? ', exclusive' : '') .
               (defined($bTargetResume) && $bTargetResume ? ', resume' : '') .
               (defined($oRemapHashRef) ? ', remap' : '') .
               (defined($iExpectedExitStatus) ? ", expect exit ${iExpectedExitStatus}" : '') .
               (defined($strComment) ? " (${strComment})" : ''));

    if (!defined($oExpectedManifestRef))
    {
        # Change mode on the backup path so it can be read
        if ($bRemote)
        {
            BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_RepoPathGet(), true);
        }

        my $oExpectedManifest = new BackRest::Manifest(BackRestTestCommon_RepoPathGet() .
                                                       "/backup/${strStanza}/${strBackup}/backup.manifest", true);

        $oExpectedManifestRef = $oExpectedManifest->{oManifest};

        # Change mode on the backup path back before unit tests continue
        if ($bRemote)
        {
            BackRestTestCommon_Execute('chmod 700 ' . BackRestTestCommon_RepoPathGet(), true);
        }
    }

    if (defined($oRemapHashRef))
    {
        BackRestTestCommon_ConfigRemap($oRemapHashRef, $oExpectedManifestRef, $bRemote);
    }

    if (defined($oRecoveryHashRef))
    {
        BackRestTestCommon_ConfigRecovery($oRecoveryHashRef, $bRemote);
    }

    # Create the backup command
    BackRestTestCommon_Execute(BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                               '/pg_backrest.conf'  . (defined($bDelta) && $bDelta ? ' --delta' : '') .
                               (defined($bForce) && $bForce ? ' --force' : '') .
                               ($strBackup ne 'latest' ? " --set=${strBackup}" : '') .
                               (defined($strType) && $strType ne RECOVERY_TYPE_DEFAULT ? " --type=${strType}" : '') .
                               (defined($strTarget) ? " --target=\"${strTarget}\"" : '') .
                               (defined($strTargetTimeline) ? " --target-timeline=\"${strTargetTimeline}\"" : '') .
                               (defined($bTargetExclusive) && $bTargetExclusive ? " --target-exclusive" : '') .
                               (defined($bTargetResume) && $bTargetResume ? " --target-resume" : '') .
                               " --stanza=${strStanza} restore",
                               undef, undef, undef, $iExpectedExitStatus);

    if (!defined($iExpectedExitStatus))
    {
        BackRestTestBackup_RestoreCompare($oFile, $strStanza, $bRemote, $strBackup, $bSynthetic, $oExpectedManifestRef);
    }
}

####################################################################################################################################
# BackRestTestBackup_RestoreCompare
####################################################################################################################################
sub BackRestTestBackup_RestoreCompare
{
    my $oFile = shift;
    my $strStanza = shift;
    my $bRemote = shift;
    my $strBackup = shift;
    my $bSynthetic = shift;
    my $oExpectedManifestRef = shift;

    my $strTestPath = BackRestTestCommon_TestPathGet();

    # Load the last manifest if it exists
    my $oLastManifest = undef;

    if (defined(${$oExpectedManifestRef}{'backup'}{'prior'}))
    {
        # Change mode on the backup path so it can be read
        if ($bRemote)
        {
            BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_RepoPathGet(), true);
        }

        my $oExpectedManifest = new BackRest::Manifest(BackRestTestCommon_RepoPathGet() .
                                                       "/backup/${strStanza}/${strBackup}/backup.manifest", true);

        $oLastManifest = new BackRest::Manifest(BackRestTestCommon_RepoPathGet() .
                                                "/backup/${strStanza}/" . ${$oExpectedManifestRef}{'backup'}{'prior'} .
                                                '/backup.manifest', true);

        # Change mode on the backup path back before unit tests continue
        if ($bRemote)
        {
            BackRestTestCommon_Execute('chmod 700 ' . BackRestTestCommon_RepoPathGet(), true);
        }

    }

    # Generate the actual manifest
    my $oActualManifest = new BackRest::Manifest("${strTestPath}/actual.manifest", false);

    my $oTablespaceMapRef = undef;
    $oActualManifest->build($oFile, ${$oExpectedManifestRef}{'backup:path'}{'base'}, $oLastManifest, true, undef);

    # Generate checksums for all files if required
    # Also fudge size if this is a synthetic test - sizes may change during backup.
    foreach my $strSectionPathKey ($oActualManifest->keys('backup:path'))
    {
        my $strSectionPath = $oActualManifest->get('backup:path', $strSectionPathKey);

        # Create all paths in the manifest that do not already exist
        my $strSection = "${strSectionPathKey}:file";

        foreach my $strName ($oActualManifest->keys($strSection))
        {
            if (!$bSynthetic)
            {
                $oActualManifest->set($strSection, $strName, 'size', ${$oExpectedManifestRef}{$strSection}{$strName}{size});
            }

            if ($oActualManifest->get($strSection, $strName, 'size') != 0)
            {
                $oActualManifest->set($strSection, $strName, 'checksum',
                                      $oFile->hash(PATH_DB_ABSOLUTE, "${strSectionPath}/${strName}"));
            }
        }
    }

    # Set actual to expected for settings that always change from backup to backup
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef,
                          ${$oExpectedManifestRef}{'backup:option'}{compress});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef,
                          ${$oExpectedManifestRef}{'backup:option'}{hardlink});

    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_VERSION, undef,
                          ${$oExpectedManifestRef}{'backup'}{version});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
                          ${$oExpectedManifestRef}{'backup'}{'timestamp-copy-start'});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef,
                          ${$oExpectedManifestRef}{'backup'}{'timestamp-start'});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef,
                          ${$oExpectedManifestRef}{'backup'}{'timestamp-stop'});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef,
                          ${$oExpectedManifestRef}{'backup'}{'label'});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef,
                          ${$oExpectedManifestRef}{'backup'}{'type'});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_CHECKSUM, undef,
                          ${$oExpectedManifestRef}{'backup'}{'checksum'});

    if (!$bSynthetic)
    {
        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef,
                              ${$oExpectedManifestRef}{'backup'}{'archive-start'});
        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef,
                              ${$oExpectedManifestRef}{'backup'}{'archive-stop'});
    }

    ini_save("${strTestPath}/actual.manifest", $oActualManifest->{oManifest});
    ini_save("${strTestPath}/expected.manifest", $oExpectedManifestRef);

    BackRestTestCommon_Execute("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/expected.manifest");
    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/actual.manifest");
}

####################################################################################################################################
# BackRestTestBackup_Expire
####################################################################################################################################
sub BackRestTestBackup_Expire
{
    my $strStanza = shift;
    my $oFile = shift;
    my $stryBackupExpectedRef = shift;
    my $stryArchiveExpectedRef = shift;
    my $iExpireFull = shift;
    my $iExpireDiff = shift;
    my $strExpireArchiveType = shift;
    my $iExpireArchive = shift;

    my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                               "/pg_backrest.conf --stanza=${strStanza} expire";

    if (defined($iExpireFull))
    {
        $strCommand .= ' --retention-full=' . $iExpireFull;
    }

    if (defined($iExpireDiff))
    {
        $strCommand .= ' --retention-diff=' . $iExpireDiff;
    }

    if (defined($strExpireArchiveType))
    {
        $strCommand .= ' --retention-archive-type=' . $strExpireArchiveType .
                       ' --retention-archive=' . $iExpireArchive;
    }

    BackRestTestCommon_Execute($strCommand);

    # Check that the correct backups were expired
    my @stryBackupActual = $oFile->list(PATH_BACKUP_CLUSTER);

    if (join(",", @stryBackupActual) ne join(",", @{$stryBackupExpectedRef}))
    {
        confess "expected backup list:\n    " . join("\n    ", @{$stryBackupExpectedRef}) .
                "\n\nbut actual was:\n    " . join("\n    ", @stryBackupActual) . "\n";
    }

    # Check that the correct archive logs were expired
    my @stryArchiveActual = $oFile->list(PATH_BACKUP_ARCHIVE, '0000000100000000');

    if (join(",", @stryArchiveActual) ne join(",", @{$stryArchiveExpectedRef}))
    {
        confess "expected archive list:\n    " . join("\n    ", @{$stryArchiveExpectedRef}) .
                "\n\nbut actual was:\n    " . join("\n    ", @stryArchiveActual) . "\n";
    }
}

####################################################################################################################################
# BackRestTestBackup_Test
####################################################################################################################################
sub BackRestTestBackup_Test
{
    my $strTest = shift;
    my $iThreadMax = shift;

    # If no test was specified, then run them all
    if (!defined($strTest))
    {
        $strTest = 'all';
    }

    # Setup global variables
    $strTestPath = BackRestTestCommon_TestPathGet();
    $strHost = BackRestTestCommon_HostGet();
    $strUserBackRest = BackRestTestCommon_UserBackRestGet();
    $strUser = BackRestTestCommon_UserGet();
    $strGroup = BackRestTestCommon_GroupGet();

    # Setup test variables
    my $iRun;
    my $bCreate;
    my $strStanza = BackRestTestCommon_StanzaGet();

    my $strArchiveChecksum = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
    my $iArchiveMax = 3;
    my $strXlogPath = BackRestTestCommon_DbCommonPathGet() . '/pg_xlog';
    my $strArchiveTestFile = BackRestTestCommon_DataPathGet() . '/test.archive2.bin';

    # Print test banner
    &log(INFO, 'BACKUP MODULE ******************************************************************');

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remotes
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oRemote = BackRest::Remote->new
    (
        $strHost,                               # Host
        $strUserBackRest,                       # User
        BackRestTestCommon_CommandRemoteGet(),  # Command
        OPTION_DEFAULT_BUFFER_SIZE,             # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,          # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,  # Compress network level
    );

    my $oLocal = new BackRest::Remote
    (
        undef,                                  # Host
        undef,                                  # User
        undef,                                  # Command
        OPTION_DEFAULT_BUFFER_SIZE,             # Buffer size
        OPTION_DEFAULT_COMPRESS_LEVEL,          # Compress level
        OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,  # Compress network level
    );

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-push
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'archive-push')
    {
        $iRun = 0;
        $bCreate = true;
        my $oFile;

        &log(INFO, "Test archive-push\n");

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
            for (my $bCompress = false; $bCompress <= true; $bCompress++)
            {
            for (my $bArchiveAsync = false; $bArchiveAsync <= true; $bArchiveAsync++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, " .
                                            "arc_async ${bArchiveAsync}")) {next}

                # Create the test directory
                if ($bCreate)
                {
                    # Create the file object
                    $oFile = (new BackRest::File
                    (
                        $strStanza,
                        BackRestTestCommon_RepoPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : $oLocal
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);

                    $bCreate = false;
                }

                BackRestTestCommon_ConfigCreate('db',
                                                ($bRemote ? BACKUP : undef),
                                                $bCompress,
                                                undef,       # checksum
                                                undef,       # hardlink
                                                undef,       # thread-max
                                                $bArchiveAsync,
                                                undef);

                my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                                 '/pg_backrest.conf --stanza=db archive-push';

                # Loop through backups
                for (my $iBackup = 1; $iBackup <= 3; $iBackup++)
                {
                    my $strArchiveFile;

                    # Loop through archive files
                    for (my $iArchive = 1; $iArchive <= $iArchiveMax; $iArchive++)
                    {
                        # Construct the archive filename
                        my $iArchiveNo = (($iBackup - 1) * $iArchiveMax + ($iArchive - 1)) + 1;

                        if ($iArchiveNo > 255)
                        {
                            confess 'backup total * archive total cannot be greater than 255';
                        }

                        $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo));

                        &log(INFO, '    backup ' . sprintf('%02d', $iBackup) .
                                   ', archive ' .sprintf('%02x', $iArchive) .
                                   " - ${strArchiveFile}");

                        my $strSourceFile = "${strXlogPath}/${strArchiveFile}";

                        $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile, # Source file
                                     PATH_DB_ABSOLUTE, $strSourceFile,      # Destination file
                                     false,                                 # Source is not compressed
                                     false,                                 # Destination is not compressed
                                     undef, undef, undef,                   # Unused params
                                     true);                                 # Create path if it does not exist

                        BackRestTestCommon_Execute($strCommand . " ${strSourceFile}");

                        # Build the archive name to check for at the destination
                        my $strArchiveCheck = "${strArchiveFile}-${strArchiveChecksum}";

                        if ($bCompress)
                        {
                            $strArchiveCheck .= '.gz';
                        }

                        if (!$oFile->exists(PATH_BACKUP_ARCHIVE, $strArchiveCheck))
                        {
                            hsleep(1);

                            if (!$oFile->exists(PATH_BACKUP_ARCHIVE, $strArchiveCheck))
                            {
                                confess 'unable to find ' . $oFile->path_get(PATH_BACKUP_ARCHIVE, $strArchiveCheck);
                            }
                        }
                    }

                    # !!! Need to put in tests for .backup files here
                }
            }
            }

            $bCreate = true;
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive-get
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'archive-get')
    {
        $iRun = 0;
        $bCreate = true;
        my $oFile;

        &log(INFO, "Test archive-get\n");

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
            for (my $bCompress = false; $bCompress <= true; $bCompress++)
            {
            for (my $bExists = false; $bExists <= true; $bExists++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, exists ${bExists}")) {next}

                # Create the test directory
                if ($bCreate)
                {
                    # Create the file object
                    $oFile = (BackRest::File->new
                    (
                        $strStanza,
                        BackRestTestCommon_RepoPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : $oLocal
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);

                    # Create the db/common/pg_xlog directory
                    BackRestTestCommon_PathCreate($strXlogPath);

                    $bCreate = false;
                }

                BackRestTestCommon_ConfigCreate('db',                           # local
                                                ($bRemote ? BACKUP : undef),    # remote
                                                $bCompress,                     # compress
                                                undef,                          # checksum
                                                undef,                          # hardlink
                                                undef,                          # thread-max
                                                undef,                          # archive-async
                                                undef);                         # compress-async

                my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                                 '/pg_backrest.conf --stanza=db archive-get';

                if ($bExists)
                {
                    # Loop through archive files
                    my $strArchiveFile;

                    for (my $iArchiveNo = 1; $iArchiveNo <= $iArchiveMax; $iArchiveNo++)
                    {
                        # Construct the archive filename
                        if ($iArchiveNo > 255)
                        {
                            confess 'backup total * archive total cannot be greater than 255';
                        }

                        $strArchiveFile = uc(sprintf('0000000100000001%08x', $iArchiveNo));

                        &log(INFO, '    archive ' .sprintf('%02x', $iArchiveNo) .
                                   " - ${strArchiveFile}");

                        my $strSourceFile = "${strArchiveFile}-${strArchiveChecksum}";

                        if ($bCompress)
                        {
                            $strSourceFile .= '.gz';
                        }

                        $oFile->copy(PATH_DB_ABSOLUTE, $strArchiveTestFile,  # Source file
                                     PATH_BACKUP_ARCHIVE, $strSourceFile,    # Destination file
                                     false,                                  # Source is not compressed
                                     $bCompress,                             # Destination compress based on test
                                     undef, undef, undef,                    # Unused params
                                     true);                                  # Create path if it does not exist

                        my $strDestinationFile = "${strXlogPath}/${strArchiveFile}";

                        BackRestTestCommon_Execute($strCommand . " ${strArchiveFile} ${strDestinationFile}");

                        # Check that the destination file exists
                        if ($oFile->exists(PATH_DB_ABSOLUTE, $strDestinationFile))
                        {
                            if ($oFile->hash(PATH_DB_ABSOLUTE, $strDestinationFile) ne $strArchiveChecksum)
                            {
                                confess "archive file hash does not match ${strArchiveChecksum}";
                            }
                        }
                        else
                        {
                            confess 'archive file is not in destination';
                        }
                    }
                }
                else
                {
                    if (BackRestTestCommon_Execute($strCommand . " 000000090000000900000009 ${strXlogPath}/RECOVERYXLOG",
                                                   false, true) != 1)
                    {
                        confess 'archive-get should return 1 when archive log is not present';
                    }
                }

                $bCreate = true;
            }
            }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test expire
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'expire')
    {
        $iRun = 0;
        my $oFile;

        &log(INFO, "Test expire\n");

        # Create the file object
        $oFile = (BackRest::File->new
        (
            $strStanza,
            BackRestTestCommon_RepoPathGet(),
            'db',
            $oLocal
        ))->clone();

        # Create the database
        BackRestTestBackup_Create(false);

        # Create db config
        BackRestTestCommon_ConfigCreate('db',           # local
                                        undef,          # remote
                                        false,          # compress
                                        undef,          # checksum
                                        undef,          # hardlink
                                        $iThreadMax,    # thread-max
                                        undef,          # archive-async
                                        undef);         # compress-async

        # Create backup config
        BackRestTestCommon_ConfigCreate('backup',       # local
                                        undef,          # remote
                                        false,          # compress
                                        undef,          # checksum
                                        undef,          # hardlink
                                        $iThreadMax,    # thread-max
                                        undef,          # archive-async
                                        undef);         # compress-async

        # Backups
        my @stryBackupExpected;

        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_FULL, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_DIFF, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_INCR, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_FULL, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_DIFF, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_INCR, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_DIFF, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_DIFF, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_FULL, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_DIFF, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_INCR, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_INCR, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_INCR, $strStanza, false, $oFile);
        push @stryBackupExpected, BackRestTestBackup_Backup(BACKUP_TYPE_DIFF, $strStanza, false, $oFile);
        push @stryBackupExpected, 'latest';

        # Create an archive log path that will be removed as old on the first archive expire call
        $oFile->path_create(PATH_BACKUP_ARCHIVE, '0000000000000000');

        # Get the expected archive list
        my @stryArchiveExpected = $oFile->list(PATH_BACKUP_ARCHIVE, '0000000100000000');

        # Expire all but the last two fulls
        splice(@stryBackupExpected, 0, 3);
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 2);

        # Expire all but the last three diffs
        splice(@stryBackupExpected, 1, 3);
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, undef, 3);

        # Expire all but the last three diffs and last two fulls (should be no change)
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 2, 3);

        # Expire archive based on the last two fulls
        splice(@stryArchiveExpected, 0, 10);
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 2, 3, 'full', 2);

        if ($oFile->exists(PATH_BACKUP_ARCHIVE, '0000000000000000'))
        {
            confess 'archive log path 0000000000000000 should have been removed';
        }

        # Expire archive based on the last two diffs
        splice(@stryArchiveExpected, 0, 18);
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 2, 3, 'diff', 2);

        # Expire archive based on the last two incrs
        splice(@stryBackupExpected, 0, 2);
        splice(@stryArchiveExpected, 0, 9);
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 1, 2, 'incr', 2);

        # Expire archive based on the last two incrs (no change in archive)
        splice(@stryBackupExpected, 1, 4);
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 1, 1, 'incr', 2);

        # Expire archive based on the last two diffs (no change in archive)
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 1, 1, 'diff', 2);

        # Expire archive based on the last diff
        splice(@stryArchiveExpected, 0, 3);
        BackRestTestBackup_Expire($strStanza, $oFile, \@stryBackupExpected, \@stryArchiveExpected, 1, 1, 'diff', 1);

        # Cleanup
        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test backup
    #
    # Check the backup and restore functionality using synthetic data.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'backup')
    {
        $iRun = 0;

        &log(INFO, "Test Backup\n");

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
        for (my $bCompress = false; $bCompress <= true; $bCompress++)
        {
        for (my $bHardlink = false; $bHardlink <= true; $bHardlink++)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rmt ${bRemote}, cmp ${bCompress}, hardlink ${bHardlink}")) {next}

            # Get base time
            my $lTime = time() - 100000;

            # Build the manifest
            my %oManifest;

            $oManifest{backup}{version} = version_get();
            $oManifest{'backup:option'}{compress} = $bCompress ? 'y' : 'n';
            $oManifest{'backup:option'}{hardlink} = $bHardlink ? 'y' : 'n';

            # Create the test directory
            BackRestTestBackup_Create($bRemote, false);

            $oManifest{'backup:path'}{base} = BackRestTestCommon_DbCommonPathGet();

            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base');

            # Create the file object
            my $oFile = new BackRest::File
            (
                $strStanza,
                BackRestTestCommon_RepoPathGet(),
                $bRemote ? 'backup' : undef,
                $bRemote ? $oRemote : $oLocal
            );

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'PG_VERSION', '9.3',
                                                  'e1f7a3a299f62225cba076fc6d3d6e677f303482', $lTime);

            # Create base path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'base');

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASE',
                                                  'a3b357a3e395e43fcfb19bb13f3c1b5179279593', $lTime);

            # Create tablespace path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'pg_tblspc');

            # Create db config
            BackRestTestCommon_ConfigCreate('db',                           # local
                                            $bRemote ? BACKUP : undef,      # remote
                                            $bCompress,                     # compress
                                            true,                           # checksum
                                            $bRemote ? undef : $bHardlink,  # hardlink
                                            $iThreadMax);                   # thread-max

            # Create backup config
            if ($bRemote)
            {
                BackRestTestCommon_ConfigCreate('backup',                   # local
                                                DB,                         # remote
                                                $bCompress,                 # compress
                                                true,                       # checksum
                                                $bHardlink,                 # hardlink
                                                $iThreadMax);               # thread-max
            }

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            my $strType = 'full';

            BackRestTestBackup_ManifestLinkCreate(\%oManifest, 'base', 'link-test', '/test');
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'path-test');

            my $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest);

            # Resume Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'full';

            my $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strFullBackup}",
                                        $strTmpPath, $bRemote);

            $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                                'resume', TEST_BACKUP_RESUME);

            # Restore - tests various mode, extra files/paths, missing files/paths
            #-----------------------------------------------------------------------------------------------------------------------
            my $bDelta = true;
            my $bForce = false;

            # Create a path and file that are not in the manifest
            BackRestTestBackup_PathCreate(\%oManifest, 'base', 'deleteme');
            BackRestTestBackup_FileCreate(\%oManifest, 'base', 'deleteme/deleteme.txt', 'DELETEME');

            # Change path mode
            BackRestTestBackup_PathMode(\%oManifest, 'base', 'base', '0777');

            # Change an existing link to the wrong directory
            BackRestTestBackup_FileRemove(\%oManifest, 'base', 'link-test');
            BackRestTestBackup_LinkCreate(\%oManifest, 'base', 'link-test', '/wrong');

            # Remove an path
            BackRestTestBackup_PathRemove(\%oManifest, 'base', 'path-test');

            # Remove a file
            BackRestTestBackup_FileRemove(\%oManifest, 'base', 'PG_VERSION');
            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'add and delete files');

            # Incr backup - add a tablespace
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup);

            # Add tablespace 1
            BackRestTestBackup_ManifestTablespaceCreate(\%oManifest, 1);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:1", 'tablespace1.txt', 'TBLSPC1',
                                                  'd85de07d6421d90aa9191c11c889bfde43680f0f', $lTime);


            my $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                               'add tablespace 1');

            # Resume Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';

            # Move database from backup to temp
            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            # Add tablespace 2
            BackRestTestBackup_ManifestTablespaceCreate(\%oManifest, 2);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:2", 'tablespace2.txt', 'TBLSPC2',
                                                  'dc7f76e43c46101b47acc55ae4d593a9e6983578', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'resume and add tablespace 2', TEST_BACKUP_RESUME);

            # Resume Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';

            $strTmpPath = BackRestTestCommon_RepoPathGet() . "/temp/${strStanza}.tmp";

            BackRestTestCommon_PathMove(BackRestTestCommon_RepoPathGet() . "/backup/${strStanza}/${strBackup}",
                                        $strTmpPath, $bRemote);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'cannot resume - new diff', TEST_BACKUP_NORESUME);

            # Restore -
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;

            # Fail on used path
            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'fail on used path', ERROR_RESTORE_PATH_NOT_EMPTY);
            # Fail on undef format
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, 'backup', 'format', undef, undef);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                      undef, undef, undef, undef, undef, undef,
                                      'fail on undef format', ERROR_FORMAT);

            # Fail on mismatch format
            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, 'backup', 'format', undef, 0);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, undef, $bDelta, $bForce,
                                      undef, undef, undef, undef, undef, undef,
                                      'fail on mismatch format', ERROR_FORMAT);

            BackRestTestBackup_ManifestMunge($oFile, $bRemote, $strBackup, 'backup', 'format', undef, 3);

            # Remap the base path
            my %oRemapHash;
            $oRemapHash{base} = BackRestTestCommon_DbCommonPathGet(2);
            $oRemapHash{1} = BackRestTestCommon_DbTablespacePathGet(1, 2);
            $oRemapHash{2} = BackRestTestCommon_DbTablespacePathGet(2, 2);

            BackRestTestBackup_Restore($oFile, $strBackup, $strStanza, $bRemote, \%oManifest, \%oRemapHash, $bDelta, $bForce,
                                       undef, undef, undef, undef, undef, undef,
                                       'remap all paths');

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base2.txt', 'BASE2',
                                                  '09b5e31766be1dba1ec27de82f975c1b6eea2a92', $lTime);

            BackRestTestBackup_ManifestTablespaceDrop(\%oManifest, 1, 2);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:2", 'tablespace2b.txt', 'TBLSPC2B',
                                                  'e324463005236d83e6e54795dbddd20a74533bf3', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest,
                                                            'add files and remove tablespace 2');

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASEUPDT',
                                                  '9a53d532e27785e681766c98516a5e93f096a501', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest, 'update files');

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest, 'no updates');

            # Incr Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'incr';
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, "remove files - but won't affect manifest",
                                           true, true, 1);
            BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD);

            BackRestTestBackup_FileRemove(\%oManifest, 'base', 'base/base1.txt');

            $strBackup = BackRestTestBackup_BackupEnd($strType, $oFile, $bRemote, undef, \%oManifest, true);

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strType = 'diff';

            BackRestTestBackup_ManifestFileRemove(\%oManifest, 'base', 'base/base1.txt');

            BackRestTestBackup_ManifestFileRemove(\%oManifest, "tablespace:2", 'tablespace2b.txt', true);
            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:2", 'tablespace2c.txt', 'TBLSPC2C',
                                                  'ad7df329ab97a1e7d35f1ff0351c079319121836', $lTime);

            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, "remove files during backup", true, true, 1);
            BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:2", 'tablespace2c.txt', 'TBLSPCBIGGER',
                                                  'dfcb8679956b734706cf87259d50c88f83e80e66', $lTime);

            BackRestTestBackup_ManifestFileRemove(\%oManifest, 'base', 'base/base2.txt', true);

            $strBackup = BackRestTestBackup_BackupEnd($strType, $oFile, $bRemote, undef, \%oManifest, true);

            # Full Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'full';
            BackRestTestBackup_ManifestReference(\%oManifest);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASEUPDT2',
                                                  '7579ada0808d7f98087a0a586d0df9de009cdc33', $lTime);

            $strFullBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest);

            # Diff Backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = 'diff';
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup);

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base2.txt', 'BASE2UPDT',
                                                  'cafac3c59553f2cfde41ce2e62e7662295f108c0', $lTime);

            $strBackup = BackRestTestBackup_BackupSynthetic($strType, $strStanza, $bRemote, $oFile, \%oManifest, 'add files');
        }
        }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test full
    #
    # Check the entire backup mechanism using actual clusters.  Only the archive and start/stop mechanisms need to be tested since
    # everything else was tested in the backup test.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'full')
    {
        $iRun = 0;
        $bCreate = true;

        &log(INFO, "Test Full Backup\n");

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
        for (my $bArchiveAsync = false; $bArchiveAsync <= true; $bArchiveAsync++)
        {
        for (my $bCompress = false; $bCompress <= true; $bCompress++)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rmt ${bRemote}, arc_async ${bArchiveAsync}, cmp ${bCompress}")) {next}

            # Create the file object
            my $oFile = new BackRest::File
            (
                $strStanza,
                BackRestTestCommon_RepoPathGet(),
                $bRemote ? 'backup' : undef,
                $bRemote ? $oRemote : $oLocal
            );

            # Create the test directory
            if ($bCreate)
            {
                BackRestTestBackup_Create($bRemote);
                $bCreate = false;
            }

            # Create db config
            BackRestTestCommon_ConfigCreate('db',                              # local
                                            $bRemote ? BACKUP : undef,         # remote
                                            $bCompress,                        # compress
                                            undef,                             # checksum
                                            $bRemote ? undef : true,           # hardlink
                                            $iThreadMax,                       # thread-max
                                            $bArchiveAsync,                    # archive-async
                                            undef);                            # compress-async

            # Create backup config
            if ($bRemote)
            {
                BackRestTestCommon_ConfigCreate('backup',                      # local
                                                $bRemote ? DB : undef,         # remote
                                                $bCompress,                    # compress
                                                undef,                         # checksum
                                                true,                          # hardlink
                                                $iThreadMax,                   # thread-max
                                                undef,                         # archive-async
                                                undef);                        # compress-async
            }

            # Static backup parameters
            my $bSynthetic = false;
            my $fTestDelay = .1;

            # Variable backup parameters
            my $bDelta = true;
            my $bForce = false;
            my $strType = undef;
            my $strTarget = undef;
            my $bTargetExclusive = false;
            my $bTargetResume = false;
            my $strTargetTimeline = undef;
            my $oRecoveryHashRef = undef;
            my $strTestPoint = undef;
            my $strComment = undef;
            my $iExpectedExitStatus = undef;

            # Restore test string
            my $strDefaultMessage = 'default';
            my $strFullMessage = 'full';
            my $strIncrMessage = 'incr';
            my $strTimeMessage = 'time';
            my $strXidMessage = 'xid';
            my $strNameMessage = 'name';
            my $strTimelineMessage = 'timeline3';

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_FULL;
            $strTestPoint = TEST_MANIFEST_BUILD;
            $strComment = 'insert during backup';

            BackRestTestBackup_PgExecute("create table test (message text not null)");
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("insert into test values ('$strDefaultMessage')");

            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, $strComment, $bSynthetic,
                                           defined($strTestPoint), $fTestDelay);
            BackRestTestCommon_ExecuteEnd($strTestPoint);

            BackRestTestBackup_PgExecute("update test set message = '$strFullMessage'", false);

            my $strFullBackup = BackRestTestBackup_BackupEnd($strType, $oFile, $bRemote, undef, undef, $bSynthetic);

            # Setup the time target
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_PgExecute("update test set message = '$strTimeMessage'", false);
            BackRestTestBackup_PgSwitchXlog();
            my $strTimeTarget = BackRestTestBackup_PgSelectOne("select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ')");
            &log(INFO, "        time target is ${strTimeTarget}");

            # Incr backup
            #-----------------------------------------------------------------------------------------------------------------------
            $strType = BACKUP_TYPE_INCR;
            $strTestPoint = TEST_MANIFEST_BUILD;
            $strComment = 'update during backup';

            BackRestTestBackup_PgExecute("create table test_remove (id int)", false);
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("update test set message = '$strDefaultMessage'", false);
            BackRestTestBackup_PgSwitchXlog();

            BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, $strComment, $bSynthetic,
                                           defined($strTestPoint), $fTestDelay);
            BackRestTestCommon_ExecuteEnd($strTestPoint);

            BackRestTestBackup_PgExecute("drop table test_remove", false);
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("update test set message = '$strIncrMessage'", false);

            my $strIncrBackup = BackRestTestBackup_BackupEnd($strType, $oFile, $bRemote, undef, undef, $bSynthetic);

            # Setup the xid target
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_PgExecute("update test set message = '$strXidMessage'", false, false);
            BackRestTestBackup_PgSwitchXlog();
            my $strXidTarget = BackRestTestBackup_PgSelectOne("select txid_current()");
            BackRestTestBackup_PgCommit();
            &log(INFO, "        xid target is ${strXidTarget}");

            # Setup the name target
            #-----------------------------------------------------------------------------------------------------------------------
            my $strNameTarget = 'backrest';

            BackRestTestBackup_PgExecute("update test set message = '$strNameMessage'", false, true);
            BackRestTestBackup_PgSwitchXlog();
            BackRestTestBackup_PgExecute("select pg_create_restore_point('${strNameTarget}')", false, false);

            &log(INFO, "        name target is ${strNameTarget}");

            # Restore (type = default)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = false;
            $strType = RECOVERY_TYPE_DEFAULT;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            # Expect failure because postmaster.pid exists
            $strComment = 'postmaster running';
            $iExpectedExitStatus = ERROR_POSTMASTER_RUNNING;

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStop();

            # Expect failure because db path is not empty
            $strComment = 'path not empty';
            $iExpectedExitStatus = ERROR_RESTORE_PATH_NOT_EMPTY;

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            # Drop and recreate db path
            BackRestTestCommon_PathRemove(BackRestTestCommon_DbCommonPathGet());
            BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet());

            # Now the restore should work
            $strComment = undef;
            $iExpectedExitStatus = undef;

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strNameMessage);

            # Restore (restore type = xid, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = false;
            $bForce = true;
            $strType = RECOVERY_TYPE_XID;
            $strTarget = $strXidTarget;
            $bTargetExclusive = undef;
            $bTargetResume = true;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            # Save recovery file to test so we can use it in the next test
            $oFile->copy(PATH_ABSOLUTE, BackRestTestCommon_DbCommonPathGet() . '/recovery.conf',
                         PATH_ABSOLUTE, BackRestTestCommon_TestPathGet() . '/recovery.conf');

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strXidMessage);

            BackRestTestBackup_PgExecute("update test set message = '$strTimelineMessage'", false);

            # Restore (restore type = preserve, inclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_PRESERVE;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            # Restore recovery file that was save in last test
            $oFile->move(PATH_ABSOLUTE, BackRestTestCommon_TestPathGet() . '/recovery.conf',
                         PATH_ABSOLUTE, BackRestTestCommon_DbCommonPathGet() . '/recovery.conf');

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strXidMessage);

            BackRestTestBackup_PgExecute("update test set message = '$strTimelineMessage'", false);

            # Restore (restore type = time, inclusive) - there is no exclusive time test because I can't find a way to find the
            # exact commit time of a transaction.
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_TIME;
            $strTarget = $strTimeTarget;
            $bTargetExclusive = undef;
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strTimeMessage);

            # Restore (restore type = xid, exclusive)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_XID;
            $strTarget = $strXidTarget;
            $bTargetExclusive = true;
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strIncrMessage);

            # Restore (restore type = name)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = true;
            $strType = RECOVERY_TYPE_NAME;
            $strTarget = $strNameTarget;
            $bTargetExclusive = undef;
            $bTargetResume = undef;
            $strTargetTimeline = undef;
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart();
            BackRestTestBackup_PgSelectOneTest('select message from test', $strNameMessage);

            # Restore (restore type = default, timeline = 3)
            #-----------------------------------------------------------------------------------------------------------------------
            $bDelta = true;
            $bForce = false;
            $strType = RECOVERY_TYPE_DEFAULT;
            $strTarget = undef;
            $bTargetExclusive = undef;
            $bTargetResume = undef;
            $strTargetTimeline = 3;
            $oRecoveryHashRef = {'standy-mode' => 'on'};
            $oRecoveryHashRef = undef;
            $strComment = undef;
            $iExpectedExitStatus = undef;

            &log(INFO, "    testing recovery type = ${strType}");

            BackRestTestBackup_ClusterStop();

            BackRestTestBackup_Restore($oFile, $strFullBackup, $strStanza, $bRemote, undef, undef, $bDelta, $bForce,
                                       $strType, $strTarget, $bTargetExclusive, $bTargetResume, $strTargetTimeline,
                                       $oRecoveryHashRef, $strComment, $iExpectedExitStatus);

            BackRestTestBackup_ClusterStart(undef, undef, true);
            BackRestTestBackup_PgSelectOneTest('select message from test', $strTimelineMessage, 120);

            $bCreate = true;
        }
        }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test collision
    #
    # See if it is possible for a table to be written to, have stop backup run, and be written to again all in the same second.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'collision')
    {
        $iRun = 0;
        my $iRunMax = 1000;

        &log(INFO, "Test Backup Collision\n");

        # Create the file object
        my $oFile = (BackRest::File->new
        (
            $strStanza,
            BackRestTestCommon_RepoPathGet(),
            NONE,
            $oLocal
        ))->clone();

        # Create the test database
        BackRestTestBackup_Create(false);

        # Create the config file
        BackRestTestCommon_ConfigCreate('db',                              # local
                                        undef,                             # remote
                                        false,                             # compress
                                        false,                             # checksum
                                        false,                             # hardlink
                                        $iThreadMax,                       # thread-max
                                        false,                             # archive-async
                                        undef);                            # compress-async

        # Create the test table
        BackRestTestBackup_PgExecute("create table test_collision (id int)");

        # Construct filename to test
        my $strFile = BackRestTestCommon_DbCommonPathGet() . "/base";

        # Get the oid of the postgres db
        my $strSql = "select oid from pg_database where datname = 'postgres'";
        my $hStatement = $hDb->prepare($strSql);

        $hStatement->execute() or
            confess &log(ERROR, "Unable to execute: ${strSql}");

        my @oyRow = $hStatement->fetchrow_array();
        $strFile .= '/' . $oyRow[0];

        $hStatement->finish();

        # Get the oid of the new table so we can check the file on disk
        $strSql = "select oid from pg_class where relname = 'test_collision'";
        $hStatement = $hDb->prepare($strSql);

        $hStatement->execute() or
            confess &log(ERROR, "Unable to execute: ${strSql}");

        @oyRow = $hStatement->fetchrow_array();
        $strFile .= '/' . $oyRow[0];

        &log(INFO, 'table filename = ' . $strFile);

        $hStatement->finish();

        BackRestTestBackup_PgExecute("select pg_start_backup('test');");

        # File modified in the same second after the manifest is taken and file is copied
        while ($iRun < $iRunMax)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "mod after manifest")) {next}

            my $strTestChecksum = $oFile->hash(PATH_DB_ABSOLUTE, $strFile);

            # Insert a row and do a checkpoint
            BackRestTestBackup_PgExecute("insert into test_collision values (1)", true);

            # Stat the file to get size/modtime after the backup has started
            my $strBeginChecksum = $oFile->hash(PATH_DB_ABSOLUTE, $strFile);
            my $oStat = lstat($strFile);
            my $lBeginSize = $oStat->size;
            my $lBeginTime = $oStat->mtime;

            # Sleep .5 seconds to give a reasonable amount of time for the file to be copied after the manifest was generated
            # Sleep for a while to show there is a large window where this can happen
            &log(INFO, 'time ' . gettimeofday());
            hsleep(.5);
            &log(INFO, 'time ' . gettimeofday());

            # Insert another row
            BackRestTestBackup_PgExecute("insert into test_collision values (1)");

            # Stop backup, start a new backup
            BackRestTestBackup_PgExecute("select pg_stop_backup();");
            BackRestTestBackup_PgExecute("select pg_start_backup('test');");

            # Stat the file to get size/modtime after the backup has restarted
            my $strEndChecksum = $oFile->hash(PATH_DB_ABSOLUTE, $strFile);
            $oStat = lstat($strFile);
            my $lEndSize = $oStat->size;
            my $lEndTime = $oStat->mtime;

            # Error if the size/modtime are the same between the backups
            &log(INFO, "    begin size = ${lBeginSize}, time = ${lBeginTime}, hash ${strBeginChecksum} - " .
                       "end size = ${lEndSize}, time = ${lEndTime}, hash ${strEndChecksum} - test hash ${strTestChecksum}");

            if ($lBeginSize == $lEndSize && $lBeginTime == $lEndTime &&
                $strTestChecksum ne $strBeginChecksum && $strBeginChecksum ne $strEndChecksum)
            {
                &log(ERROR, "size and mod time are the same between backups");
                $iRun = $iRunMax;
                next;
            }
        }

        BackRestTestBackup_PgExecute("select pg_stop_backup();");

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # rsync-collision
    #
    # See if it is possible for a table to be written to, have stop backup run, and be written to again all in the same second.
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'rsync-collision')
    {
        $iRun = 0;
        my $iRunMax = 1000;

        &log(INFO, "Test Rsync Collision\n");

        # Create the file object
        my $oFile = (BackRest::File->new
        (
            $strStanza,
            BackRestTestCommon_RepoPathGet(),
            NONE,
            $oLocal
        ))->clone();

        # Create the test database
        BackRestTestBackup_Create(false, false);

        # Create test paths
        my $strPathRsync1 = BackRestTestCommon_TestPathGet() . "/rsync1";
        my $strPathRsync2 = BackRestTestCommon_TestPathGet() . "/rsync2";

        BackRestTestCommon_PathCreate($strPathRsync1);
        BackRestTestCommon_PathCreate($strPathRsync2);

        # Rsync command
        my $strCommand = "rsync -vvrt ${strPathRsync1}/ ${strPathRsync2}";

        # File modified in the same second after the manifest is taken and file is copied
        while ($iRun < $iRunMax)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rsync test")) {next}

            # Create test file
            &log(INFO, "create test file");
            BackRestTestCommon_FileCreate("${strPathRsync1}/test.txt", 'TEST1');

            # Stat the file to get size/modtime after the backup has started
            my $strBeginChecksum = $oFile->hash(PATH_DB_ABSOLUTE, "${strPathRsync1}/test.txt");

            # Rsync
            &log(INFO, "rsync 1st time");
            BackRestTestCommon_Execute($strCommand, false, false, true);

            # Sleep for a while to show there is a large window where this can happen
            &log(INFO, 'time ' . gettimeofday());
            hsleep(.5);
            &log(INFO, 'time ' . gettimeofday());

            # Modify the test file within the same second
            &log(INFO, "modify test file");
            BackRestTestCommon_FileCreate("${strPathRsync1}/test.txt", 'TEST2');

            my $strEndChecksum = $oFile->hash(PATH_DB_ABSOLUTE, "${strPathRsync1}/test.txt");

            # Rsync again
            &log(INFO, "rsync 2nd time");
            BackRestTestCommon_Execute($strCommand, false, false, true);

            my $strTestChecksum = $oFile->hash(PATH_DB_ABSOLUTE, "${strPathRsync2}/test.txt");

            # Error if checksums are not the same after rsync
            &log(INFO, "    begin hash ${strBeginChecksum} - end hash ${strEndChecksum} - test hash ${strTestChecksum}");

            if ($strTestChecksum ne $strEndChecksum)
            {
                &log(ERROR, "end and test checksums are not the same");
                $iRun = $iRunMax;
                next;
            }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop(true);
        }
    }
}

1;
