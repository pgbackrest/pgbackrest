#!/usr/bin/perl
####################################################################################################################################
# BackupCommonTest.pl - Common code for backup unit tests
####################################################################################################################################
package BackRestTest::BackupCommonTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use DBI;
use Exporter qw(import);
use Fcntl ':mode';
use File::Basename;
use File::Copy 'cp';
use File::stat;
use Time::HiRes qw(gettimeofday);

use lib dirname($0) . '/../lib';
use BackRest::Archive;
use BackRest::ArchiveInfo;
use BackRest::Config;
use BackRest::Exception;
use BackRest::File;
use BackRest::Ini;
use BackRest::Manifest;
use BackRest::Utility;

use BackRestTest::CommonTest;

my $hDb;

####################################################################################################################################
# BackRestTestBackup_PgHandleGet
####################################################################################################################################
our @EXPORT = qw(BackRestTestBackup_PgHandleGet);

sub BackRestTestBackup_PgHandleGet
{
    return $hDb;
}

####################################################################################################################################
# BackRestTestBackup_PgConnect
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PgConnect);

sub BackRestTestBackup_PgConnect
{
    my $iWaitSeconds = shift;

    # Disconnect user session
    BackRestTestBackup_PgDisconnect();

    # Setup the wait loop
    $iWaitSeconds = defined($iWaitSeconds) ? $iWaitSeconds : 30;
    my $oWait = waitInit($iWaitSeconds);

    do
    {
        # Connect to the db (whether it is local or remote)
        $hDb = DBI->connect('dbi:Pg:dbname=postgres;port=' . BackRestTestCommon_DbPortGet .
                            ';host=' . BackRestTestCommon_DbPathGet(),
                            BackRestTestCommon_UserGet(),
                            undef,
                            {AutoCommit => 0, RaiseError => 0, PrintError => 0});

        return if $hDb;
    }
    while (waitMore($oWait));

    confess &log(ERROR, "unable to connect to PostgreSQL after ${iWaitSeconds} second(s):\n" .
                 $DBI::errstr, ERROR_DB_CONNECT);
}

####################################################################################################################################
# BackRestTestBackup_PgDisconnect
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PgDisconnect);

sub BackRestTestBackup_PgDisconnect
{
    # Connect to the db (whether it is local or remote)
    if (defined($hDb))
    {
        $hDb->disconnect();
        undef($hDb);
    }
}

####################################################################################################################################
# BackRestTestBackup_PgExecuteNoTrans
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PgExecuteNoTrans);

sub BackRestTestBackup_PgExecuteNoTrans
{
    my $strSql = shift;

    # Connect to the db with autocommit on so we can runs statements that can't run in transaction blocks
    my $hDb = DBI->connect('dbi:Pg:dbname=postgres;port=' . BackRestTestCommon_DbPortGet .
                           ';host=' . BackRestTestCommon_DbPathGet(),
                           BackRestTestCommon_UserGet(),
                           undef,
                           {AutoCommit => 1, RaiseError => 1});

    # Log and execute the statement
    &log(DEBUG, "SQL: ${strSql}");
    my $hStatement = $hDb->prepare($strSql);

    $hStatement->execute() or
        confess &log(ERROR, "Unable to execute: ${strSql}");

    $hStatement->finish();

    # Close the connection
    $hDb->disconnect();
}

####################################################################################################################################
# BackRestTestBackup_PgExecute
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PgExecute);

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
push @EXPORT, qw(BackRestTestBackup_PgSwitchXlog);

sub BackRestTestBackup_PgSwitchXlog
{
    BackRestTestBackup_PgExecute('select pg_switch_xlog()', false, false);
    BackRestTestBackup_PgExecute('select pg_switch_xlog()', false, false);
}

####################################################################################################################################
# BackRestTestBackup_PgCommit
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PgCommit);

sub BackRestTestBackup_PgCommit
{
    my $bCheckpoint = shift;

    BackRestTestBackup_PgExecute('commit', $bCheckpoint, false);
}

####################################################################################################################################
# BackRestTestBackup_PgSelect
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PgSelect);

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
push @EXPORT, qw(BackRestTestBackup_PgSelectOne);

sub BackRestTestBackup_PgSelectOne
{
    my $strSql = shift;

    return (BackRestTestBackup_PgSelect($strSql))[0];
}

####################################################################################################################################
# BackRestTestBackup_PgSelectOneTest
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PgSelectOneTest);

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
push @EXPORT, qw(BackRestTestBackup_ClusterStop);

sub BackRestTestBackup_ClusterStop
{
    my $strPath = shift;
    my $bImmediate = shift;
    my $bNoError = shift;

    # Set default
    $strPath = defined($strPath) ? $strPath : BackRestTestCommon_DbCommonPathGet();
    $bImmediate = defined($bImmediate) ? $bImmediate : false;

    # Disconnect user session
    BackRestTestBackup_PgDisconnect();

    # Stop the cluster
    BackRestTestCommon_ClusterStop($strPath, $bImmediate);

    # Grep for errors in postgresql.log
    if ((!defined($bNoError) || !$bNoError) &&
        -e BackRestTestCommon_DbCommonPathGet() . '/postgresql.log')
    {
        BackRestTestCommon_Execute('grep ERROR ' . BackRestTestCommon_DbCommonPathGet() . '/postgresql.log',
                                   undef, undef, undef, 1);
    }
}

####################################################################################################################################
# BackRestTestBackup_ClusterStart
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_ClusterStart);

sub BackRestTestBackup_ClusterStart
{
    my $strPath = shift;
    my $iPort = shift;
    my $bHotStandby = shift;
    my $bArchive = shift;

    # Set default
    $iPort = defined($iPort) ? $iPort : BackRestTestCommon_DbPortGet();
    $strPath = defined($strPath) ? $strPath : BackRestTestCommon_DbCommonPathGet();
    $bHotStandby = defined($bHotStandby) ? $bHotStandby : false;
    $bArchive = defined($bArchive) ? $bArchive : true;

    # Make sure postgres is not running
    if (-e $strPath . '/postmaster.pid')
    {
        confess 'postmaster.pid exists';
    }

    # Create the archive command
    my $strArchive = BackRestTestCommon_CommandMainAbsGet() . ' --stanza=' . BackRestTestCommon_StanzaGet() .
                     ' --config=' . BackRestTestCommon_DbPathGet() . '/pg_backrest.conf archive-push %p';

    # Start the cluster
    my $strCommand = BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl start -o \"-c port=${iPort}" .
                     (BackRestTestCommon_DbVersion() < 9.5 ? ' -c checkpoint_segments=1' : '');

    if ($bArchive)
    {
        if (BackRestTestCommon_DbVersion() >= '8.3')
        {
            $strCommand .= " -c archive_mode=on";
        }

        $strCommand .= " -c archive_command='${strArchive}'";

        if (BackRestTestCommon_DbVersion() >= '9.0')
        {
            $strCommand .= " -c wal_level=hot_standby";

            if ($bHotStandby)
            {
                $strCommand .= ' -c hot_standby=on';
            }
        }
    }
    else
    {
        $strCommand .= " -c archive_mode=on -c wal_level=archive -c archive_command=true";
    }

    $strCommand .= " -c log_error_verbosity=verbose" .
                   " -c unix_socket_director" . (BackRestTestCommon_DbVersion() < '9.3' ? "y='" : "ies='") .
                   BackRestTestCommon_DbPathGet() . "'\" " .
                   "-D ${strPath} -l ${strPath}/postgresql.log -s";

    BackRestTestCommon_Execute($strCommand);

    # Connect user session
    BackRestTestBackup_PgConnect();
}

####################################################################################################################################
# BackRestTestBackup_ClusterRestart
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_ClusterRestart);

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
push @EXPORT, qw(BackRestTestBackup_ClusterCreate);

sub BackRestTestBackup_ClusterCreate
{
    my $strPath = shift;
    my $iPort = shift;
    my $bArchive = shift;

    # Defaults
    $strPath = defined($strPath) ? $strPath : BackRestTestCommon_DbCommonPathGet();

    BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/initdb -D ${strPath} -A trust");

    BackRestTestBackup_ClusterStart($strPath, $iPort, undef, $bArchive);

    # Connect user session
    BackRestTestBackup_PgConnect();
}

####################################################################################################################################
# BackRestTestBackup_Drop
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_Drop);

sub BackRestTestBackup_Drop
{
    my $bImmediate = shift;
    my $bNoError = shift;

    # Stop the cluster if one is running
    BackRestTestBackup_ClusterStop(BackRestTestCommon_DbCommonPathGet(), $bImmediate, $bNoError);

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
push @EXPORT, qw(BackRestTestBackup_Create);

sub BackRestTestBackup_Create
{
    my $bRemote = shift;
    my $bCluster = shift;
    my $bArchive = shift;

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

    BackRestTestCommon_CreateRepo($bRemote);

    # Create the cluster
    if ($bCluster)
    {
        BackRestTestBackup_ClusterCreate(undef, undef, $bArchive);
    }
}

####################################################################################################################################
# BackRestTestBackup_PathCreate
#
# Create a path specifying mode.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PathCreate);

sub BackRestTestBackup_PathCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;
    my $strMode = shift;

    # Create final file location
    my $strFinalPath = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strPath}{&MANIFEST_SUBKEY_PATH} .
                       (defined($strSubPath) ? "/${strSubPath}" : '');

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
push @EXPORT, qw(BackRestTestBackup_PathMode);

sub BackRestTestBackup_PathMode
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;
    my $strMode = shift;

    # Create final file location
    my $strFinalPath = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strPath}{&MANIFEST_SUBKEY_PATH} .
                       (defined($strSubPath) ? "/${strSubPath}" : '');

    BackRestTestCommon_PathMode($strFinalPath, $strMode);

    return $strFinalPath;
}

####################################################################################################################################
# BackRestTestBackup_ManifestPathCreate
#
# Create a path specifying mode and add it to the manifest.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_ManifestPathCreate);

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
    my $strSection = "${strPath}:" . MANIFEST_PATH;

    ${$oManifestRef}{$strSection}{$strManifestPath}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{$strSection}{$strManifestPath}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{$strSection}{$strManifestPath}{&MANIFEST_SUBKEY_MODE} = sprintf('%04o', S_IMODE($oStat->mode));
}

####################################################################################################################################
# BackRestTestBackup_PathRemove
#
# Remove a path.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_PathRemove);

sub BackRestTestBackup_PathRemove
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strSubPath = shift;

    # Create final file location
    my $strFinalPath = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strPath}{&MANIFEST_SUBKEY_PATH} .
                       (defined($strSubPath) ? "/${strSubPath}" : '');

    # Create the path
    BackRestTestCommon_PathRemove($strFinalPath);

    return $strFinalPath;
}

####################################################################################################################################
# BackRestTestBackup_ManifestTablespaceCreate
#
# Create a tablespace specifying mode and add it to the manifest.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_ManifestTablespaceCreate);

sub BackRestTestBackup_ManifestTablespaceCreate
{
    my $oManifestRef = shift;
    my $iOid = shift;
    my $strMode = shift;

    # Create final file location
    my $strPath = BackRestTestCommon_DbTablespacePathGet($iOid);

    # Stat the path
    my $oStat = lstat($strPath);

    # Check for errors in stat
    if (!defined($oStat))
    {
        confess 'unable to stat path ${strPath}';
    }

    # Load path into manifest
    my $strSection = MANIFEST_TABLESPACE . "/${iOid}:" . MANIFEST_PATH;

    ${$oManifestRef}{$strSection}{'.'}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{$strSection}{'.'}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{$strSection}{'.'}{&MANIFEST_SUBKEY_MODE} = sprintf('%04o', S_IMODE($oStat->mode));

    # Create the link in pg_tblspc
    my $strLink = BackRestTestCommon_DbCommonPathGet() . '/' . PATH_PG_TBLSPC . "/${iOid}";

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
    $strSection = MANIFEST_KEY_BASE . ':' . MANIFEST_LINK;

    ${$oManifestRef}{$strSection}{"pg_tblspc/${iOid}"}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{$strSection}{"pg_tblspc/${iOid}"}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{$strSection}{"pg_tblspc/${iOid}"}{&MANIFEST_SUBKEY_DESTINATION} = $strPath;

    # Load tablespace into the manifest
    $strSection = MANIFEST_TABLESPACE . "/${iOid}";

    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strSection}{&MANIFEST_SUBKEY_PATH} = $strPath;
    ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strSection}{&MANIFEST_SUBKEY_LINK} = $iOid;
}

####################################################################################################################################
# BackRestTestBackup_ManifestTablespaceDrop
#
# Drop a tablespace add remove it from the manifest.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_ManifestTablespaceDrop);

sub BackRestTestBackup_ManifestTablespaceDrop
{
    my $oManifestRef = shift;
    my $iOid = shift;
    my $iIndex = shift;

    # Remove tablespace path/file/link from manifest
    delete(${$oManifestRef}{MANIFEST_TABLESPACE . "/${iOid}:" . MANIFEST_PATH});
    delete(${$oManifestRef}{MANIFEST_TABLESPACE . "/${iOid}:" . MANIFEST_LINK});
    delete(${$oManifestRef}{MANIFEST_TABLESPACE . "/${iOid}:" . MANIFEST_FILE});

    # Drop the link in pg_tblspc
    BackRestTestCommon_FileRemove(BackRestTestCommon_DbCommonPathGet($iIndex) . '/' . PATH_PG_TBLSPC . "/${iOid}");

    # Remove tablespace rom manifest
    delete(${$oManifestRef}{MANIFEST_KEY_BASE . ':' . MANIFEST_LINK}{PATH_PG_TBLSPC . "/${iOid}"});
    delete(${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{MANIFEST_TABLESPACE . "/${iOid}"});
}

####################################################################################################################################
# BackRestTestBackup_FileCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_FileCreate);

sub BackRestTestBackup_FileCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $strContent = shift;
    my $lTime = shift;
    my $strMode = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strPath}{&MANIFEST_SUBKEY_PATH} . "/${strFile}";

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
push @EXPORT, qw(BackRestTestBackup_ManifestFileCreate);

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
    my $strSection = "${strPath}:" . MANIFEST_FILE;

    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_MODE} = sprintf('%04o', S_IMODE($oStat->mode));
    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_TIMESTAMP} = $oStat->mtime;
    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_SIZE} = $oStat->size;
    delete(${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_REFERENCE});

    if (defined($strChecksum))
    {
        ${$oManifestRef}{$strSection}{$strFile}{checksum} = $strChecksum;
    }
}

####################################################################################################################################
# BackRestTestBackup_FileRemove
#
# Remove a file from disk.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_FileRemove);

sub BackRestTestBackup_FileRemove
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $bIgnoreMissing = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strPath}{&MANIFEST_SUBKEY_PATH} . "/${strFile}";

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
push @EXPORT, qw(BackRestTestBackup_ManifestFileRemove);

sub BackRestTestBackup_ManifestFileRemove
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strPath}{&MANIFEST_SUBKEY_PATH} . "/${strFile}";

    # Remove the file
    BackRestTestBackup_FileRemove($oManifestRef, $strPath, $strFile, true);

    # Remove from manifest
    delete(${$oManifestRef}{"${strPath}:" . MANIFEST_FILE}{$strFile});
}

####################################################################################################################################
# BackRestTestBackup_ManifestReference
#
# Update all files that do not have a reference with the supplied reference.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_ManifestReference);

sub BackRestTestBackup_ManifestReference
{
    my $oManifestRef = shift;
    my $strReference = shift;
    my $bClear = shift;

    # Set prior backup
    if (defined($strReference))
    {
        ${$oManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR} = $strReference;
    }
    else
    {
        delete(${$oManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR});
    }

    # Find all file sections
    foreach my $strSectionFile (sort(keys(%$oManifestRef)))
    {
        # Skip non-file sections
        if ($strSectionFile !~ /\:file$/)
        {
            next;
        }

        foreach my $strFile (sort(keys(%{${$oManifestRef}{$strSectionFile}})))
        {
            if (!defined($strReference))
            {
                delete(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE});
            }
            elsif (defined($bClear) && $bClear)
            {
                if (defined(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE}) &&
                    ${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE} ne $strReference)
                {
                    delete(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE});
                }
            }
            elsif (!defined(${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE}))
            {
                ${$oManifestRef}{$strSectionFile}{$strFile}{&MANIFEST_SUBKEY_REFERENCE} = $strReference;
            }
        }
    }
}

####################################################################################################################################
# BackRestTestBackup_LinkCreate
#
# Create a file specifying content, mode, and time.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_LinkCreate);

sub BackRestTestBackup_LinkCreate
{
    my $oManifestRef = shift;
    my $strPath = shift;
    my $strFile = shift;
    my $strDestination = shift;

    # Create actual file location
    my $strPathFile = ${$oManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strPath}{&MANIFEST_SUBKEY_PATH} . "/${strFile}";

    # Create the file
    symlink($strDestination, $strPathFile)
        or confess "unable to link ${strPathFile} to ${strDestination}";

    # Return path to created file
    return $strPathFile;
}

####################################################################################################################################
# BackRestTestBackup_ManifestLinkCreate
#
# Create a link and add it to the manifest.
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_ManifestLinkCreate);

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
    my $strSection = "${strPath}:" . MANIFEST_LINK;

    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_GROUP} = getgrgid($oStat->gid);
    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_USER} = getpwuid($oStat->uid);
    ${$oManifestRef}{$strSection}{$strFile}{&MANIFEST_SUBKEY_DESTINATION} = $strDestination;
}

####################################################################################################################################
# BackRestTestBackup_LastBackup
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_LastBackup);

sub BackRestTestBackup_LastBackup
{
    my $oFile = shift;

    my @stryBackup = $oFile->list(PATH_BACKUP_CLUSTER, undef, undef, 'reverse');

    if (!defined($stryBackup[2]))
    {
        confess 'no backup was found';
    }

    return $stryBackup[2];
}

####################################################################################################################################
# BackRestTestBackup_BackupBegin
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_BackupBegin);

sub BackRestTestBackup_BackupBegin
{
    my $strType = shift;
    my $strStanza = shift;
    my $bRemote = shift;
    my $strComment = shift;
    my $bSynthetic = shift;
    my $bTestPoint = shift;
    my $fTestDelay = shift;
    my $strOptionalParam = shift;

    # Set defaults
    $bTestPoint = defined($bTestPoint) ? $bTestPoint : false;
    $fTestDelay = defined($fTestDelay) ? $fTestDelay : 0;

    $strComment = "${strType} backup" . (defined($strComment) ? " (${strComment})" : '');
    &log(INFO, "    $strComment");

    BackRestTestCommon_ExecuteBegin(($bRemote ? BackRestTestCommon_CommandMainAbsGet() : BackRestTestCommon_CommandMainGet()) .
                                    ' --config=' .
                                    ($bRemote ? BackRestTestCommon_RepoPathGet() : BackRestTestCommon_DbPathGet()) .
                                    "/pg_backrest.conf" . ($bSynthetic ? " --no-start-stop" : '') .
                                    (defined($strOptionalParam) ? " ${strOptionalParam}" : '') .
                                    ($strType ne 'incr' ? " --type=${strType}" : '') .
                                    " --stanza=${strStanza} backup" . ($bTestPoint ? " --test --test-delay=${fTestDelay}": ''),
                                    $bRemote, $strComment);
}

####################################################################################################################################
# BackRestTestBackup_BackupEnd
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_BackupEnd);

sub BackRestTestBackup_BackupEnd
{
    my $strType = shift;
    my $strStanza = shift;
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

    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE} = $strType;

    if (!defined($strBackup))
    {
        $strBackup = BackRestTestBackup_LastBackup($oFile);
    }

    if ($bSynthetic)
    {
        BackRestTestBackup_BackupCompare($oFile, $bRemote, $strBackup, $oExpectedManifestRef);
    }

    BackRestTestCommon_TestLogAppendFile(BackRestTestCommon_DbPathGet() . "/pg_backrest.conf", $bRemote);

    if ($bRemote)
    {
        BackRestTestCommon_TestLogAppendFile(BackRestTestCommon_RepoPathGet() . "/pg_backrest.conf", $bRemote);
    }

    BackRestTestCommon_TestLogAppendFile(BackRestTestCommon_RepoPathGet() .
                                         "/backup/${strStanza}/${strBackup}/backup.manifest", $bRemote);
    BackRestTestCommon_TestLogAppendFile(BackRestTestCommon_RepoPathGet() .
                                         "/backup/${strStanza}/backup.info", $bRemote);

    return $strBackup;
}

####################################################################################################################################
# BackRestTestBackup_BackupSynthetic
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_BackupSynthetic);

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
    my $strOptionalParam = shift;

    BackRestTestBackup_BackupBegin($strType, $strStanza, $bRemote, $strComment, true, defined($strTestPoint), $fTestDelay,
                                   $strOptionalParam);

    if (defined($strTestPoint))
    {
        BackRestTestCommon_ExecuteEnd($strTestPoint);
    }

    return BackRestTestBackup_BackupEnd($strType, $strStanza, $oFile, $bRemote, undef, $oExpectedManifestRef, true,
                                        $iExpectedExitStatus);
}

####################################################################################################################################
# BackRestTestBackup_Backup
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_Backup);

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

    return BackRestTestBackup_BackupEnd($strType, $strStanza, $oFile, $bRemote, undef, undef, false, $iExpectedExitStatus);
}

####################################################################################################################################
# BackRestTestBackup_Info
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_Info);

sub BackRestTestBackup_Info
{
    my $strStanza = shift;
    my $strOutput = shift;
    my $bRemote = shift;
    my $strComment = shift;

    $strComment = "info" . (defined($strStanza) ? " ${strStanza}" : '');
    &log(INFO, "    $strComment");

    BackRestTestCommon_Execute(($bRemote ? BackRestTestCommon_CommandMainAbsGet() : BackRestTestCommon_CommandMainGet()) .
                              ' --config=' .
                              ($bRemote ? BackRestTestCommon_RepoPathGet() : BackRestTestCommon_DbPathGet()) .
                              '/pg_backrest.conf' . (defined($strStanza) ? " --stanza=${strStanza}" : '') . ' info' .
                              (defined($strOutput) ? " --output=${strOutput}" : ''),
                              $bRemote, undef, undef, undef, $strComment);
}

####################################################################################################################################
# BackRestTestBackup_BackupCompare
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_BackupCompare);

sub BackRestTestBackup_BackupCompare
{
    my $oFile = shift;
    my $bRemote = shift;
    my $strBackup = shift;
    my $oExpectedManifestRef = shift;

    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LABEL} = $strBackup;

    # Change mode on the backup path so it can be read
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_RepoPathGet(), true);
    }

    my %oActualManifest;
    iniLoad($oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/' . FILE_MANIFEST, \%oActualManifest);

    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START} =
        $oActualManifest{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START};
    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP} =
        $oActualManifest{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP};
    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START} =
        $oActualManifest{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START};
    ${$oExpectedManifestRef}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} =
        $oActualManifest{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM};
    ${$oExpectedManifestRef}{&INI_SECTION_BACKREST}{&INI_KEY_FORMAT} = BACKREST_FORMAT + 0;

    my $strTestPath = BackRestTestCommon_TestPathGet();

    iniSave("${strTestPath}/actual.manifest", \%oActualManifest);
    iniSave("${strTestPath}/expected.manifest", $oExpectedManifestRef);

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
push @EXPORT, qw(BackRestTestBackup_ManifestMunge);

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
        BackRestTestCommon_Execute('chmod 770 ' . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/' . FILE_MANIFEST, true);
    }

    # Read the manifest
    my %oManifest;
    iniLoad($oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/' . FILE_MANIFEST, \%oManifest);

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
    delete($oManifest{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});

    my $oSHA = Digest::SHA->new('sha1');
    my $oJSON = JSON::PP->new()->canonical()->allow_nonref();
    $oSHA->add($oJSON->encode(\%oManifest));

    # Set the new checksum
    $oManifest{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM} = $oSHA->hexdigest();

    # Resave the manifest
    iniSave($oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/' . FILE_MANIFEST, \%oManifest);

    # Change mode on the backup path back before unit tests continue
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 750 ' . $oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/' . FILE_MANIFEST, true);
        BackRestTestCommon_Execute('chmod 700 ' . BackRestTestCommon_RepoPathGet(), true);
    }
}

####################################################################################################################################
# BackRestTestBackup_Restore
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_Restore);

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
    my $strOptionalParam = shift;
    my $bCompare = shift;

    # Set defaults
    $bDelta = defined($bDelta) ? $bDelta : false;
    $bForce = defined($bForce) ? $bForce : false;

    my $bSynthetic = defined($oExpectedManifestRef) ? true : false;

    $strComment = 'restore' .
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
                  (defined($strComment) ? " (${strComment})" : '');

    &log(INFO, "        ${strComment}");

    if (!defined($oExpectedManifestRef))
    {
        # Change mode on the backup path so it can be read
        if ($bRemote)
        {
            BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_RepoPathGet(), true);
        }

        my $oExpectedManifest = new BackRest::Manifest(BackRestTestCommon_RepoPathGet() .
                                                       "/backup/${strStanza}/${strBackup}/backup.manifest", true);

        $oExpectedManifestRef = $oExpectedManifest->{oContent};

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
    BackRestTestCommon_Execute(($bRemote ? BackRestTestCommon_CommandMainAbsGet() : BackRestTestCommon_CommandMainGet()) .
                               ' --config=' . BackRestTestCommon_DbPathGet() .
                               '/pg_backrest.conf'  . (defined($bDelta) && $bDelta ? ' --delta' : '') .
                               (defined($bForce) && $bForce ? ' --force' : '') .
                               ($strBackup ne 'latest' ? " --set=${strBackup}" : '') .
                               (defined($strOptionalParam) ? " ${strOptionalParam} " : '') .
                               (defined($strType) && $strType ne RECOVERY_TYPE_DEFAULT ? " --type=${strType}" : '') .
                               (defined($strTarget) ? " --target=\"${strTarget}\"" : '') .
                               (defined($strTargetTimeline) ? " --target-timeline=\"${strTargetTimeline}\"" : '') .
                               (defined($bTargetExclusive) && $bTargetExclusive ? " --target-exclusive" : '') .
                               (defined($bTargetResume) && $bTargetResume ? " --target-resume" : '') .
                               " --stanza=${strStanza} restore",
                               undef, undef, undef, $iExpectedExitStatus, $strComment);

    if (!defined($iExpectedExitStatus) && (!defined($bCompare) || $bCompare))
    {
        BackRestTestBackup_RestoreCompare($oFile, $strStanza, $bRemote, $strBackup, $bSynthetic, $oExpectedManifestRef);
        BackRestTestCommon_TestLogAppendFile(
            $$oExpectedManifestRef{&MANIFEST_SECTION_BACKUP_PATH}{&MANIFEST_KEY_BASE}{&MANIFEST_SUBKEY_PATH} .
            "/recovery.conf");
    }
}

####################################################################################################################################
# BackRestTestBackup_RestoreCompare
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_RestoreCompare);

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

    if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR}))
    {
        # Change mode on the backup path so it can be read
        if ($bRemote)
        {
            BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_RepoPathGet(), true);
        }

        my $oExpectedManifest = new BackRest::Manifest(BackRestTestCommon_RepoPathGet() .
                                                       "/backup/${strStanza}/${strBackup}/" . FILE_MANIFEST, true);

        $oLastManifest = new BackRest::Manifest(BackRestTestCommon_RepoPathGet() .
                                                "/backup/${strStanza}/" .
                                                ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_PRIOR} .
                                                '/' . FILE_MANIFEST, true);

        # Change mode on the backup path back before unit tests continue
        if ($bRemote)
        {
            BackRestTestCommon_Execute('chmod 700 ' . BackRestTestCommon_RepoPathGet(), true);
        }

    }

    # Generate the tablespace map for real backups
    my $oTablespaceMap = undef;

    if (!$bSynthetic)
    {
        # Tablespace_map file is not restored in versions >= 9.5 because it interferes with internal remapping features.
        if (${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION} >= 9.5)
        {
            delete(${$oExpectedManifestRef}{'base:file'}{'tablespace_map'});
        }

        foreach my $strTablespaceName (keys(%{${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}}))
        {
            if (defined(${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strTablespaceName}{&MANIFEST_SUBKEY_LINK}))
            {
                my $strTablespaceOid =
                    ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{$strTablespaceName}{&MANIFEST_SUBKEY_LINK};

                $$oTablespaceMap{oid}{$strTablespaceOid}{name} = (split('/', $strTablespaceName))[1];
            }
        }
    }

    # Generate the actual manifest
    my $oActualManifest = new BackRest::Manifest("${strTestPath}/" . FILE_MANIFEST, false);

    my $oTablespaceMapRef = undef;
    $oActualManifest->build($oFile,
        ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_PATH}{&MANIFEST_KEY_BASE}{&MANIFEST_SUBKEY_PATH},
        $oLastManifest, true, $oTablespaceMap);

    # Generate checksums for all files if required
    # Also fudge size if this is a synthetic test - sizes may change during backup.
    foreach my $strSectionPathKey ($oActualManifest->keys(MANIFEST_SECTION_BACKUP_PATH))
    {
        my $strSectionPath = $oActualManifest->get(MANIFEST_SECTION_BACKUP_PATH, $strSectionPathKey, MANIFEST_SUBKEY_PATH);

        # Create all paths in the manifest that do not already exist
        my $strSection = "${strSectionPathKey}:" . MANIFEST_FILE;

        if ($oActualManifest->test($strSection))
        {
            foreach my $strName ($oActualManifest->keys($strSection))
            {
                if (!$bSynthetic)
                {
                    $oActualManifest->set($strSection, $strName, MANIFEST_SUBKEY_SIZE,
                                          ${$oExpectedManifestRef}{$strSection}{$strName}{size});
                }

                if ($oActualManifest->get($strSection, $strName, MANIFEST_SUBKEY_SIZE) != 0)
                {
                    $oActualManifest->set($strSection, $strName, MANIFEST_SUBKEY_CHECKSUM,
                                          $oFile->hash(PATH_DB_ABSOLUTE, "${strSectionPath}/${strName}"));
                }
            }
        }
    }

    # Set actual to expected for settings that always change from backup to backup
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_CHECK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_CHECK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_ARCHIVE_COPY, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_ARCHIVE_COPY});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_COMPRESS});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_HARDLINK});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_START_STOP, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_OPTION}{&MANIFEST_KEY_START_STOP});

    $oActualManifest->set(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_DB_VERSION});
    $oActualManifest->setNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CONTROL});
    $oActualManifest->setNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_CATALOG});
    $oActualManifest->setNumeric(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef,
                                 ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP_DB}{&MANIFEST_KEY_SYSTEM_ID});

    $oActualManifest->set(INI_SECTION_BACKREST, INI_KEY_VERSION, undef,
                          ${$oExpectedManifestRef}{&INI_SECTION_BACKREST}{&INI_KEY_VERSION});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_COPY_START});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_START, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_START});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_STOP, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TIMESTAMP_STOP});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_LABEL});
    $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE, undef,
                          ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_TYPE});
    $oActualManifest->set(INI_SECTION_BACKREST, INI_KEY_CHECKSUM, undef,
                          ${$oExpectedManifestRef}{&INI_SECTION_BACKREST}{&INI_KEY_CHECKSUM});

    if (!$bSynthetic)
    {
        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef,
                              ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_START});
        $oActualManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP, undef,
                              ${$oExpectedManifestRef}{&MANIFEST_SECTION_BACKUP}{&MANIFEST_KEY_ARCHIVE_STOP});
    }

    iniSave("${strTestPath}/actual.manifest", $oActualManifest->{oContent});
    iniSave("${strTestPath}/expected.manifest", $oExpectedManifestRef);

    BackRestTestCommon_Execute("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/expected.manifest");
    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/actual.manifest");
}

####################################################################################################################################
# BackRestTestBackup_Expire
####################################################################################################################################
push @EXPORT, qw(BackRestTestBackup_Expire);

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
    my @stryArchiveActual = $oFile->list(PATH_BACKUP_ARCHIVE, BackRestTestCommon_DbVersion() . '-1/0000000100000000');

    if (join(",", @stryArchiveActual) ne join(",", @{$stryArchiveExpectedRef}))
    {
        confess "expected archive list:\n    " . join("\n    ", @{$stryArchiveExpectedRef}) .
                "\n\nbut actual was:\n    " . join("\n    ", @stryArchiveActual) . "\n";
    }

    BackRestTestCommon_TestLogAppendFile(BackRestTestCommon_RepoPathGet() .
                                         "/backup/${strStanza}/backup.info", false);
}

1;
