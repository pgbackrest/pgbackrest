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
use english;
use Carp;

use File::Basename;
use File::Copy "cp";

use lib dirname($0) . "/../lib";
use BackRest::Utility;
use BackRest::File;
use BackRest::Remote;

use BackRestTest::CommonTest;

use Exporter qw(import);
our @EXPORT = qw(BackRestTestBackup_Test);

my $strTestPath;
my $strHost;
my $strUserBackRest;

####################################################################################################################################
# BackRestTestBackup_ClusterStop
####################################################################################################################################
sub BackRestTestBackup_ClusterStop
{
    my $strPath = shift;

    # If the db directory already exists, stop the cluster and remove the directory
    if (-e $strPath . "/postmaster.pid")
    {
        BackRestTestCommon_Execute("pg_ctl stop -D $strPath -w -s -m fast");
    }
}

####################################################################################################################################
# BackRestTestBackup_ClusterRestart
####################################################################################################################################
sub BackRestTestBackup_ClusterRestart
{
    my $strPath = BackRestTestCommon_DbCommonPathGet();

    # If the db directory already exists, stop the cluster and remove the directory
    if (-e $strPath . "/postmaster.pid")
    {
        BackRestTestCommon_Execute("pg_ctl restart -D $strPath -w -s");
    }
}

####################################################################################################################################
# BackRestTestBackup_ClusterCreate
####################################################################################################################################
sub BackRestTestBackup_ClusterCreate
{
    my $strPath = shift;
    my $iPort = shift;

    my $strArchive = BackRestTestCommon_CommandMainGet() . " --stanza=" . BackRestTestCommon_StanzaGet() .
                     " --config=" . BackRestTestCommon_DbPathGet() . "/pg_backrest.conf archive-push %p";

    BackRestTestCommon_Execute("initdb -D $strPath -A trust");
    BackRestTestCommon_Execute("/Library/PostgreSQL/9.3/bin/pg_ctl start -o \"-c port=$iPort -c checkpoint_segments=1 " .
                               "-c wal_level=archive -c archive_mode=on -c archive_command='$strArchive'\" " .
                               "-D $strPath -l $strPath/postgresql.log -w -s");
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
        BackRestTestCommon_Execute('rm -rf ' . BackRestTestCommon_BackupPathGet(), true, true);
    }

    # Remove the test directory
    system('rm -rf ' . BackRestTestCommon_TestPathGet()) == 0
        or die 'unable to remove ' . BackRestTestCommon_TestPathGet() .  'path';
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
    mkdir(BackRestTestCommon_TestPathGet(), oct('0770'))
        or confess 'Unable to create ' . BackRestTestCommon_TestPathGet() . ' path';

    # Create the db directory
    mkdir(BackRestTestCommon_DbPathGet(), oct('0700'))
        or confess 'Unable to create ' . BackRestTestCommon_DbPathGet() . ' path';

    # Create the db/common directory
    mkdir(BackRestTestCommon_DbCommonPathGet())
        or confess 'Unable to create ' . BackRestTestCommon_DbCommonPathGet() . ' path';

    # Create the archive directory
    mkdir(BackRestTestCommon_ArchivePathGet(), oct('0700'))
        or confess 'Unable to create ' . BackRestTestCommon_ArchivePathGet() . ' path';

    # Create the backup directory
    if ($bRemote)
    {
        BackRestTestCommon_Execute("mkdir -m 700 " . BackRestTestCommon_BackupPathGet(), true);
    }
    else
    {
        mkdir(BackRestTestCommon_BackupPathGet(), oct('0700'))
            or confess 'Unable to create ' . BackRestTestCommon_BackupPathGet() . ' path';
    }

    # Create the cluster
    if ($bCluster)
    {
        BackRestTestBackup_ClusterCreate(BackRestTestCommon_DbCommonPathGet(), BackRestTestCommon_DbPortGet());
    }
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

    # Setup test variables
    my $iRun;
    my $bCreate;
    $strTestPath = BackRestTestCommon_TestPathGet();
    my $strStanza = BackRestTestCommon_StanzaGet();
    my $strUserBackRest = BackRestTestCommon_UserBackRestGet();
    my $strGroup = BackRestTestCommon_GroupGet();
    $strHost = BackRestTestCommon_HostGet();
    $strUserBackRest = BackRestTestCommon_UserBackRestGet();

    # Print test banner
    &log(INFO, "BACKUP MODULE ******************************************************************");

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remote
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oRemote = BackRest::Remote->new
    (
        strHost => $strHost,
        strUser => $strUserBackRest,
        strCommand => BackRestTestCommon_CommandRemoteGet()
    );

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test archive
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'archive-push')
    {
        $iRun = 0;
        $bCreate = true;
        my $strXlogPath = BackRestTestCommon_DbCommonPathGet() . '/pg_xlog';
        my $strArchiveChecksum = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
        my $iArchiveMax = 3;
        my $oFile;

        &log(INFO, "Test Archiving\n");

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
                    $oFile = (BackRest::File->new
                    (
                        strStanza => $strStanza,
                        strBackupPath => BackRestTestCommon_BackupPathGet(),
                        strRemote => $bRemote ? 'backup' : undef,
                        oRemote => $bRemote ? $oRemote : undef
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);

                    # Create the db/common/pg_xlog directory
                    mkdir($strXlogPath)
                        or confess 'Unable to create ${strXlogPath} path';

                    $bCreate = false;
                }

                BackRestTestCommon_ConfigCreate('db',
                                                ($bRemote ? REMOTE_BACKUP : undef),
                                                $bCompress,
                                                $bChecksum,  # checksum
                                                undef,       # hardlink
                                                undef,       # thread-max
                                                $bArchiveAsync,
                                                $bCompressAsync);

                my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                                 "/pg_backrest.conf --stanza=db archive-push";


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
                            confess "backup total * archive total cannot be greater than 255";
                        }

                        $strArchiveFile = uc(sprintf("0000000100000001%08x", $iArchiveNo));

                        &log(INFO, "    backup " . sprintf("%02d", $iBackup) .
                                   ", archive " .sprintf("%02x", $iArchive) .
                                   " - ${strArchiveFile}");

                        cp(BackRestTestCommon_DataPathGet() . '/test.archive.bin', "${strXlogPath}/${strArchiveFile}");

                        BackRestTestCommon_Execute($strCommand . " ${strXlogPath}/${strArchiveFile}");

                        # Build the archive name to check for at the destination
                        my $strArchiveCheck = $strArchiveFile;

                        if ($bChecksum)
                        {
                            $strArchiveCheck .= "-${strArchiveChecksum}";
                        }

                        if ($bCompress)
                        {
                            $strArchiveCheck .= ".gz";
                        }

                        if (!$oFile->exists(PATH_BACKUP_ARCHIVE, $strArchiveCheck))
                        {
                            sleep(1);

                            if (!$oFile->exists(PATH_BACKUP_ARCHIVE, $strArchiveCheck))
                            {
                                confess "unable to find " . $oFile->path_get(PATH_BACKUP_ARCHIVE, $strArchiveCheck);
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
    # Test full
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'full')
    {
        $iRun = 0;
        $bCreate = true;

        &log(INFO, "Test Full Backup\n");

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
        for (my $bLarge = false; $bLarge <= false; $bLarge++)
        {
            for (my $bArchiveAsync = false; $bArchiveAsync <= $bRemote; $bArchiveAsync++)
            {
            for (my $bHardlink = false; $bHardlink <= true; $bHardlink++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, lrg ${bLarge}, arc_async ${bArchiveAsync}, " .
                                            "hardlink ${bHardlink}")) {next}

                # Create the test directory
                if ($bCreate)
                {
                    BackRestTestBackup_Create($bRemote);
                    $bCreate = false;
                }

                # Create db config
                BackRestTestCommon_ConfigCreate('db',
                                                ($bRemote ? REMOTE_BACKUP : undef),
                                                undef,          # compress
                                                undef,          # checksum
                                                undef,          # hardlink
                                                undef,          # thread-max
                                                $bArchiveAsync, # archive-async
                                                undef           # compressasync
                                               );

                # Create backup config
                BackRestTestCommon_ConfigCreate('backup',
                                                ($bRemote ? REMOTE_DB : undef),
                                                undef,           # compress
                                                undef,           # checksum
                                                $bHardlink,      # hardlink
                                                8,               # thread-max
                                                undef,           # archive-async
                                                undef,           # compress-async
                                               );

                for (my $iFull = 1; $iFull <= 1; $iFull++)
                {
                    &log(INFO, "    full " . sprintf("%02d", $iFull));

                    my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_BackupPathGet() .
                                               "/pg_backrest.conf --type=incr --stanza=${strStanza} backup";

                    BackRestTestCommon_Execute($strCommand, $bRemote);
                    # exit 0;

                    for (my $iIncr = 1; $iIncr <= 1; $iIncr++)
                    {
                        $iRun++;

                        &log(INFO, "        incr " . sprintf("%02d", $iIncr));

                        BackRestTestCommon_Execute($strCommand, $bRemote);
                    }
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
