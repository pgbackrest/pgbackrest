#!/usr/bin/perl
####################################################################################################################################
# BackupTest.pl - Unit Tests for BackRest::File
####################################################################################################################################
package BackRestTest::BackupTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use Carp;

use File::Basename;
use File::Copy 'cp';
use File::stat;
use Fcntl ':mode';
use DBI;

use lib dirname($0) . '/../lib';
use BackRest::Utility;
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
                        {AutoCommit => 1, RaiseError => 1});
}

####################################################################################################################################
# BackRestTestBackup_Disconnect
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

    # Log and execute the statement
    &log(DEBUG, "SQL: ${strSql}");
    my $hStatement = $hDb->prepare($strSql);

    $hStatement->execute() or
        confess &log(ERROR, "Unable to execute: ${strSql}");

    $hStatement->finish();

    # Perform a checkpoint if requested
    if (defined($bCheckpoint) && $bCheckpoint)
    {
        BackRestTestBackup_PgExecute('checkpoint');
    }
}

####################################################################################################################################
# BackRestTestBackup_ClusterStop
####################################################################################################################################
sub BackRestTestBackup_ClusterStop
{
    my $strPath = shift;

    # Disconnect user session
    BackRestTestBackup_PgDisconnect();

    # If postmaster process is running them stop the cluster
    if (-e $strPath . '/postmaster.pid')
    {
        BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl stop -D ${strPath} -w -s -m immediate");
    }
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

    my $strArchive = BackRestTestCommon_CommandMainGet() . ' --stanza=' . BackRestTestCommon_StanzaGet() .
                     ' --config=' . BackRestTestCommon_DbPathGet() . '/pg_backrest.conf archive-push %p';

    BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/initdb -D ${strPath} -A trust");
    BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl start -o \"-c port=${iPort} -c " .
                               "checkpoint_segments=1 -c wal_level=archive -c archive_mode=on -c archive_command='${strArchive}' " .
                               "-c unix_socket_directories='" . BackRestTestCommon_DbPathGet() . "'\" " .
                               "-D ${strPath} -l ${strPath}/postgresql.log -w -s");

    # Connect user session
    BackRestTestBackup_PgConnect();
}

####################################################################################################################################
# BackRestTestBackup_Drop
####################################################################################################################################
sub BackRestTestBackup_Drop
{
    # Stop the cluster if one is running
    BackRestTestBackup_ClusterStop(BackRestTestCommon_DbCommonPathGet());

    # Remove the backrest private directory
    if (-e BackRestTestCommon_BackupPathGet())
    {
        BackRestTestCommon_PathRemove(BackRestTestCommon_BackupPathGet(), true, true);
    }

    # Remove the test directory
    BackRestTestCommon_PathRemove(BackRestTestCommon_TestPathGet());
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
    BackRestTestBackup_Drop();

    # Create the test directory
    BackRestTestCommon_PathCreate(BackRestTestCommon_TestPathGet(), '0770');

    # Create the db paths
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbPathGet());
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet());

    # Create tablespace paths
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet());
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet() . '/ts1');
    BackRestTestCommon_PathCreate(BackRestTestCommon_DbTablespacePathGet() . '/ts2');

    # Create the archive directory
    BackRestTestCommon_PathCreate(BackRestTestCommon_ArchivePathGet());

    # Create the backup directory
    if ($bRemote)
    {
        BackRestTestCommon_Execute('mkdir -m 700 ' . BackRestTestCommon_BackupPathGet(), true);
    }
    else
    {
        BackRestTestCommon_PathCreate(BackRestTestCommon_BackupPathGet());
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
    ${$oManifestRef}{"${strPath}:path"}{$strManifestPath}{permission} = sprintf('%04o', S_IMODE($oStat->mode));
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
    my $strPath = BackRestTestCommon_DbPathGet() . "/ts${iOid}";

    # Create the path
    if (!(-e $strPath))
    {
        BackRestTestCommon_PathCreate($strPath, $strMode);
    }

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
    ${$oManifestRef}{"tablespace:${iOid}:path"}{'.'}{permission} = sprintf('%04o', S_IMODE($oStat->mode));

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
    ${$oManifestRef}{"${strPath}:file"}{$strFile}{permission} = sprintf('%04o', S_IMODE($oStat->mode));
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
# BackRestTestBackup_CompareBackup
####################################################################################################################################
sub BackRestTestBackup_CompareBackup
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
                    if (${$oExpectedManifestRef}{backup}{reference} !~ /$strFileReference/)
                    {
                        ${$oExpectedManifestRef}{backup}{reference} .= ",${strFileReference}";
                    }
                }
            }
        }
    }

    # Change permissions on the backup path so it can be read
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 750 ' . BackRestTestCommon_BackupPathGet(), true);
    }

    my %oActualManifest;
    ini_load($oFile->path_get(PATH_BACKUP_CLUSTER, $strBackup) . '/backup.manifest', \%oActualManifest);

    ${$oExpectedManifestRef}{backup}{'timestamp-start'} = $oActualManifest{backup}{'timestamp-start'};
    ${$oExpectedManifestRef}{backup}{'timestamp-stop'} = $oActualManifest{backup}{'timestamp-stop'};

    my $strTestPath = BackRestTestCommon_TestPathGet();

    ini_save("${strTestPath}/actual.manifest", \%oActualManifest);
    ini_save("${strTestPath}/expected.manifest", $oExpectedManifestRef);

    BackRestTestCommon_Execute("diff ${strTestPath}/expected.manifest ${strTestPath}/actual.manifest");

    # Change permissions on the backup path back before unit tests continue
    if ($bRemote)
    {
        BackRestTestCommon_Execute('chmod 700 ' . BackRestTestCommon_BackupPathGet(), true);
    }

    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/expected.manifest");
    $oFile->remove(PATH_ABSOLUTE, "${strTestPath}/actual.manifest");
}


####################################################################################################################################
# BackRestTestBackup_CompareRestore
####################################################################################################################################
sub BackRestTestBackup_CompareRestore
{
    my $oFile = shift;
    my $strBackup = shift;
    my $strStanza = shift;
    my $oExpectedManifestRef = shift;
    my $bForce = shift;

    # Create the backup command
    BackRestTestCommon_Execute(BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                               '/pg_backrest.conf '  . (defined($bForce)? '--force ' : '') . "--stanza=${strStanza} restore");
}

####################################################################################################################################
# BackRestTestBackup_Test
####################################################################################################################################
sub BackRestTestBackup_Test
{
    my $strTest = shift;

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
    my $strArchiveTestFile = BackRestTestCommon_DataPathGet() . '/test.archive.bin';
    my $iThreadMax = 4;

    # Print test banner
    &log(INFO, 'BACKUP MODULE ******************************************************************');

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remote
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oRemote = BackRest::Remote->new
    (
        $strHost,
        $strUserBackRest,
        BackRestTestCommon_CommandRemoteGet()
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
            for (my $bChecksum = false; $bChecksum <= true; $bChecksum++)
            {
            for (my $bArchiveAsync = false; $bArchiveAsync <= $bRemote; $bArchiveAsync++)
            {
            for (my $bCompressAsync = false; $bCompressAsync <= true; $bCompressAsync++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, chk ${bChecksum}, " .
                                            "arc_async ${bArchiveAsync}, cmp_async ${bCompressAsync}")) {next}

                # Create the test directory
                if ($bCreate)
                {
                    # Create the file object
                    $oFile = (new BackRest::File
                    (
                        $strStanza,
                        BackRestTestCommon_BackupPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : undef
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);

                    $bCreate = false;
                }

                BackRestTestCommon_ConfigCreate('db',
                                                ($bRemote ? BACKUP : undef),
                                                $bCompress,
                                                $bChecksum,  # checksum
                                                undef,       # hardlink
                                                undef,       # thread-max
                                                $bArchiveAsync,
                                                $bCompressAsync);

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
                        my $strArchiveCheck = $strArchiveFile;

                        if ($bChecksum)
                        {
                            $strArchiveCheck .= "-${strArchiveChecksum}";
                        }

                        if ($bCompress)
                        {
                            $strArchiveCheck .= '.gz';
                        }

                        if (!$oFile->exists(PATH_BACKUP_ARCHIVE, $strArchiveCheck))
                        {
                            sleep(1);

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
            }
            }

            $bCreate = true;
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop();
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
            for (my $bChecksum = false; $bChecksum <= true; $bChecksum++)
            {
            for (my $bExists = false; $bExists <= true; $bExists++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, chk ${bChecksum}, exists ${bExists}")) {next}

                # Create the test directory
                if ($bCreate)
                {
                    # Create the file object
                    $oFile = (BackRest::File->new
                    (
                        $strStanza,
                        BackRestTestCommon_BackupPathGet(),
                        $bRemote ? 'backup' : undef,
                        $bRemote ? $oRemote : undef
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);

                    # Create the db/common/pg_xlog directory
                    BackRestTestCommon_PathCreate($strXlogPath);

                    $bCreate = false;
                }

                BackRestTestCommon_ConfigCreate('db',                           # local
                                                ($bRemote ? BACKUP : undef),    # remote
                                                $bCompress,                     # compress
                                                $bChecksum,                     # checksum
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

                        my $strSourceFile = $strArchiveFile;

                        if ($bChecksum)
                        {
                            $strSourceFile .= "-${strArchiveChecksum}";
                        }

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
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop();
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
        for (my $bChecksum = false; $bChecksum <= true; $bChecksum++)
        {
        for (my $bHardlink = false; $bHardlink <= true; $bHardlink++)
        {
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rmt ${bRemote}, cmp ${bCompress}, chk ${bChecksum}, " .
                                        "hardlink ${bHardlink}")) {next}

            # Get base time
            my $lTime = time() - 100000;

            # Build the manifest
            my %oManifest;

            $oManifest{backup}{version} = version_get();
            $oManifest{'backup:option'}{compress} = $bCompress ? 'y' : 'n';
            $oManifest{'backup:option'}{checksum} = $bChecksum ? 'y' : 'n';

            # Create the test directory
            BackRestTestBackup_Create($bRemote, false);

            $oManifest{'backup:path'}{base} = BackRestTestCommon_DbCommonPathGet();

            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base');

            # Create the file object
            my $oFile = new BackRest::File
            (
                $strStanza,
                BackRestTestCommon_BackupPathGet(),
                $bRemote ? 'backup' : undef,
                $bRemote ? $oRemote : undef
            );

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'PG_VERSION', '9.3',
                                                  $bChecksum ? 'e1f7a3a299f62225cba076fc6d3d6e677f303482' : undef, $lTime);

            # Create base path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'base');

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASE',
                                                  $bChecksum ? 'a3b357a3e395e43fcfb19bb13f3c1b5179279593' : undef, $lTime);

            # Create tablespace path
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'pg_tblspc');

            # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
            # {
            #     BackRestTestBackup_ManifestTablespaceCreate(\%oManifest, $iTablespaceIdx);
            #
            #     BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace1.txt', 'TBLSPC',
            #                                           $bChecksum ? '44ad0bf042936c576c75891d0e5ded8e2b60fb54' : undef, $lTime);
            # }

            # Create db config
            BackRestTestCommon_ConfigCreate('db',                           # local
                                            $bRemote ? BACKUP : undef,      # remote
                                            $bCompress,                     # compress
                                            $bChecksum,                     # checksum
                                            $bRemote ? undef : $bHardlink,  # hardlink
                                            $iThreadMax);                   # thread-max

            # Create backup config
            if ($bRemote)
            {
                BackRestTestCommon_ConfigCreate('backup',                   # local
                                                DB,                         # remote
                                                $bCompress,                 # compress
                                                $bChecksum,                 # checksum
                                                $bHardlink,                 # hardlink
                                                $iThreadMax);               # thread-max
            }

            # Create the backup command
            my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' .
                                       ($bRemote ? BackRestTestCommon_BackupPathGet() : BackRestTestCommon_DbPathGet()) .
                                       "/pg_backrest.conf --no-start-stop --stanza=${strStanza} backup";

            # Full backup
            #-----------------------------------------------------------------------------------------------------------------------
            my $strType = 'full';
            &log(INFO, "    ${strType} backup");

            BackRestTestBackup_ManifestLinkCreate(\%oManifest, 'base', 'link-test', '/test');
            BackRestTestBackup_ManifestPathCreate(\%oManifest, 'base', 'path-test');

            BackRestTestCommon_Execute("${strCommand} --type=${strType}", $bRemote);

            $oManifest{backup}{type} = $strType;
            my $strFullBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, $strFullBackup, \%oManifest);

            # Restore - tests various permissions, extra files/paths, missing files/paths
            #-----------------------------------------------------------------------------------------------------------------------
            my $bForce = true;
            &log(INFO, '        ' . ($bForce ? 'force ' : '') . 'restore');

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

            BackRestTestBackup_CompareRestore($oFile, $strFullBackup, $strStanza, \%oManifest, $bForce);

            # Incr backup - add a tablespace
            #-----------------------------------------------------------------------------------------------------------------------
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup);
            $oManifest{'backup:option'}{hardlink} = $bHardlink ? 'y' : 'n';

            $strType = 'incr';
            &log(INFO, "    ${strType} backup (add tablespace 1)");

            BackRestTestCommon_Execute("${strCommand} --type=${strType}", $bRemote);

            $oManifest{backup}{type} = $strType;
            my $strBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, BackRestTestBackup_LastBackup($oFile), \%oManifest);

            # Perform second incr backup
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            $strType = 'incr';
            &log(INFO, "    ${strType} backup (add files)");

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base2.txt', 'BASE2',
                                                  $bChecksum ? '09b5e31766be1dba1ec27de82f975c1b6eea2a92' : undef, $lTime);

            # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
            # {
            #     BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace2.txt', 'TBLSPC2',
            #                                           $bChecksum ? 'dc7f76e43c46101b47acc55ae4d593a9e6983578' : undef, $lTime);
            # }

            BackRestTestCommon_Execute("${strCommand} --type=${strType}", $bRemote);

            $oManifest{backup}{type} = $strType;
            $strBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, BackRestTestBackup_LastBackup($oFile), \%oManifest);

            # Perform third incr backup
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            $strType = 'incr';
            &log(INFO, "    ${strType} backup (update files)");

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASEUPDT',
                                                  $bChecksum ? '9a53d532e27785e681766c98516a5e93f096a501' : undef, $lTime);

            # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
            # {
            #     BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace1.txt', 'TBLSPCUPDT',
            #                                           $bChecksum ? 'ff21d59b07e8d9cfa7b1286202610550a71884b5' : undef, $lTime);
            # }

            BackRestTestCommon_Execute("${strCommand} --type=${strType}", $bRemote);

            $oManifest{backup}{type} = $strType;
            $strBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, BackRestTestBackup_LastBackup($oFile), \%oManifest);

            # Perform first diff backup
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strType = 'diff';
            &log(INFO, "    ${strType} backup (no updates)");

            BackRestTestCommon_Execute("${strCommand} --type=${strType}", $bRemote);

            $oManifest{backup}{type} = $strType;
            $strBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, BackRestTestBackup_LastBackup($oFile), \%oManifest);

            # Perform fourth incr backup
            BackRestTestBackup_ManifestReference(\%oManifest, $strBackup);

            $strType = 'incr';
            &log(INFO, "    ${strType} backup (remove files - but won't affect manifest)");

            BackRestTestCommon_ExecuteBegin("${strCommand} --type=${strType} --test --test-delay=1", $bRemote);

            if (BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD))
            {
                BackRestTestBackup_FileRemove(\%oManifest, 'base', 'base/base1.txt');

                # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
                # {
                #     BackRestTestBackup_FileRemove(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace1.txt');
                # }

                BackRestTestCommon_ExecuteEnd();
            }
            else
            {
                confess &log(ERROR, 'test point ' . TEST_MANIFEST_BUILD . ' was not found');
            }

            $oManifest{backup}{type} = $strType;
            $strBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, BackRestTestBackup_LastBackup($oFile), \%oManifest);

            # Perform second diff backup
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup, true);

            $strType = 'diff';
            &log(INFO, "    ${strType} backup (remove files)");

            BackRestTestBackup_ManifestFileRemove(\%oManifest, 'base', 'base/base1.txt');

            # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
            # {
            #     BackRestTestBackup_ManifestFileRemove(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace1.txt');
            # }

            BackRestTestCommon_ExecuteBegin("${strCommand} --type=${strType} --test --test-delay=1", $bRemote);

            if (BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD))
            {
                BackRestTestBackup_ManifestFileRemove(\%oManifest, 'base', 'base/base2.txt', true);

                # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
                # {
                #     BackRestTestBackup_ManifestFileRemove(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace2.txt', true);
                #     delete($oManifest{"tablespace:${iTablespaceIdx}:file"});
                # }

                BackRestTestCommon_ExecuteEnd();
            }
            else
            {
                confess &log(ERROR, 'test point ' . TEST_MANIFEST_BUILD . ' was not found');
            }

            $oManifest{backup}{type} = $strType;
            $strBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, BackRestTestBackup_LastBackup($oFile), \%oManifest);

            # Perform second full backup
            BackRestTestBackup_ManifestReference(\%oManifest);
            delete($oManifest{'backup:option'}{hardlink});

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base1.txt', 'BASEUPDT2',
                                                  $bChecksum ? '7579ada0808d7f98087a0a586d0df9de009cdc33' : undef, $lTime);

            # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
            # {
            #     BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace1.txt', 'TBLSPCUPDT2',
            #                                           $bChecksum ? '42f9bdebc34de4476f21688d00b37ea77d1a2ffc' : undef, $lTime);
            # }

            $strType = 'full';
            &log(INFO, "    ${strType} backup");

            BackRestTestCommon_Execute("${strCommand} --type=${strType}", $bRemote);

            $oManifest{backup}{type} = $strType;
            $strFullBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, $strFullBackup, \%oManifest);

            # Perform third diff backup
            BackRestTestBackup_ManifestReference(\%oManifest, $strFullBackup);
            $oManifest{'backup:option'}{hardlink} = $bHardlink ? 'y' : 'n';

            $strType = 'diff';
            &log(INFO, "    ${strType} backup (add files)");

            BackRestTestBackup_ManifestFileCreate(\%oManifest, 'base', 'base/base2.txt', 'BASE2UPDT',
                                                  $bChecksum ? 'cafac3c59553f2cfde41ce2e62e7662295f108c0' : undef, $lTime);

            # for (my $iTablespaceIdx = 1; $iTablespaceIdx <= $iTablespaceTotal; $iTablespaceIdx++)
            # {
            #     BackRestTestBackup_ManifestFileCreate(\%oManifest, "tablespace:${iTablespaceIdx}", 'tablespace2.txt', 'TBLSPC2UPDT',
            #                                           $bChecksum ? 'bee4bf711a7533db234eda606782af7e80a76cf2' : undef, $lTime);
            # }

            BackRestTestCommon_Execute("${strCommand} --type=${strType}", $bRemote);

            $oManifest{backup}{type} = $strType;
            $strBackup = BackRestTestBackup_LastBackup($oFile);

            BackRestTestBackup_CompareBackup($oFile, $bRemote, BackRestTestBackup_LastBackup($oFile), \%oManifest);
        }
        }
        }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop();
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test aborted
    #
    # Check the aborted backup functionality using synthetic data.
    #-------------------------------------------------------------------------------------------------------------------------------

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
            # Increment the run, log, and decide whether this unit test should be run
            if (!BackRestTestCommon_Run(++$iRun,
                                        "rmt ${bRemote}, arc_async ${bArchiveAsync}")) {next}

            # Create the test directory
            if ($bCreate)
            {
                BackRestTestBackup_Create($bRemote);
                $bCreate = false;
            }

            # Create db config
            BackRestTestCommon_ConfigCreate('db',                              # local
                                            $bRemote ? BACKUP : undef,         # remote
                                            false,                             # compress
                                            false,                             # checksum
                                            $bRemote ? undef : true,           # hardlink
                                            $iThreadMax,                       # thread-max
                                            $bArchiveAsync,                    # archive-async
                                            undef);                            # compress-async

            # Create backup config
            if ($bRemote)
            {
                BackRestTestCommon_ConfigCreate('backup',                      # local
                                                $bRemote ? DB : undef,         # remote
                                                false,                         # compress
                                                false,                         # checksum
                                                true,                          # hardlink
                                                $iThreadMax,                   # thread-max
                                                undef,                         # archive-async
                                                undef);                        # compress-async
            }

            # Create the backup command
            my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' .
                                       ($bRemote ? BackRestTestCommon_BackupPathGet() : BackRestTestCommon_DbPathGet()) .
                                       "/pg_backrest.conf --test --type=incr --stanza=${strStanza} backup";

            # Run the full/incremental tests
            for (my $iFull = 1; $iFull <= 1; $iFull++)
            {

                for (my $iIncr = 0; $iIncr <= 2; $iIncr++)
                {
                    &log(INFO, '    ' . ($iIncr == 0 ? ('full ' . sprintf('%02d', $iFull)) :
                                                       ('    incr ' . sprintf('%02d', $iIncr))));

                    # Create tablespace
                    if ($iIncr == 0)
                    {
                        BackRestTestBackup_PgExecute("create tablespace ts1 location '" .
                                                     BackRestTestCommon_DbTablespacePathGet() . "/ts1'", true);
                    }

                    # Create a table in each backup to check references
                    BackRestTestBackup_PgExecute("create table test_backup_${iIncr} (id int)", true);

                    # Create a table to be dropped to test missing file code
                    BackRestTestBackup_PgExecute('create table test_drop (id int)');

                    BackRestTestCommon_ExecuteBegin($strCommand, $bRemote);

                    if (BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD))
                    {
                        BackRestTestBackup_PgExecute('drop table test_drop', true);

                        BackRestTestCommon_ExecuteEnd();
                    }
                    else
                    {
                        confess &log(ERROR, 'test point ' . TEST_MANIFEST_BUILD . ' was not found');
                    }
                }
            }

            $bCreate = true;
        }
        }

        if (BackRestTestCommon_Cleanup())
        {
            &log(INFO, 'cleanup');
            BackRestTestBackup_Drop();
        }
    }
}

1;
