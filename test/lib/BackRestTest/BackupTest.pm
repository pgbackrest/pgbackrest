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
use File::Copy "cp";
use DBI;

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
    my $bSwitchXlog = shift;

    &log(DEBUG, "SQL: ${strSql}");
    my $hStatement = $hDb->prepare($strSql);
    $hStatement->execute() or
        confess &log(ERROR, "Unable to execute: ${strSql}");
    $hStatement->finish();

    if (defined($bSwitchXlog) && $bSwitchXlog)
    {
        BackRestTestBackup_PgExecute('select pg_switch_xlog()');
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
    if (-e $strPath . "/postmaster.pid")
    {
        BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl stop -D $strPath -w -s -m fast");
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
    if (-e $strPath . "/postmaster.pid")
    {
        BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl restart -D $strPath -w -s");
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

    my $strArchive = BackRestTestCommon_CommandMainGet() . " --stanza=" . BackRestTestCommon_StanzaGet() .
                     " --config=" . BackRestTestCommon_DbPathGet() . "/pg_backrest.conf archive-push %p";

    BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/initdb -D ${strPath} -A trust");
    BackRestTestCommon_Execute(BackRestTestCommon_PgSqlBinPathGet() . "/pg_ctl start -o \"-c port=$iPort -c " .
                               "checkpoint_segments=1 -c wal_level=archive -c archive_mode=on -c archive_command='$strArchive' " .
                               "-c unix_socket_directories='" . BackRestTestCommon_DbPathGet() . "'\" " .
                               "-D $strPath -l $strPath/postgresql.log -w -s");

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

    # Setup global variables
    $strTestPath = BackRestTestCommon_TestPathGet();
    $strHost = BackRestTestCommon_HostGet();
    $strUserBackRest = BackRestTestCommon_UserBackRestGet();

    # Setup test variables
    my $iRun;
    my $bCreate;
    my $strStanza = BackRestTestCommon_StanzaGet();
    my $strGroup = BackRestTestCommon_GroupGet();

    my $strArchiveChecksum = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
    my $iArchiveMax = 3;
    my $strXlogPath = BackRestTestCommon_DbCommonPathGet() . '/pg_xlog';
    my $strArchiveTestFile = BackRestTestCommon_DataPathGet() . '/test.archive.bin';
    my $iThreadMax = 4; #8;

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
                    $oFile = (BackRest::File->new
                    (
                        strStanza => $strStanza,
                        strBackupPath => BackRestTestCommon_BackupPathGet(),
                        strRemote => $bRemote ? 'backup' : undef,
                        oRemote => $bRemote ? $oRemote : undef
                    ))->clone();

                    BackRestTestBackup_Create($bRemote, false);
                    #
                    # # Create the db/common/pg_xlog directory
                    # mkdir($strXlogPath)
                    #     or confess 'Unable to create ${strXlogPath} path';

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
                # Increment the run, log, and decide whether this unit test should be run
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, cmp ${bCompress}, chk ${bChecksum}")) {next}

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

                BackRestTestCommon_ConfigCreate('db',                               # local
                                                ($bRemote ? REMOTE_BACKUP : undef), # remote
                                                $bCompress,                         # compress
                                                $bChecksum,                         # checksum
                                                undef,                              # hardlink
                                                undef,                              # thread-max
                                                undef,                              # archive-async
                                                undef);                             # compress-async

                my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_DbPathGet() .
                                 "/pg_backrest.conf --stanza=db archive-get";

                # Loop through backups
                my $strArchiveFile;

                # Loop through archive files
                for (my $iArchiveNo = 1; $iArchiveNo <= $iArchiveMax; $iArchiveNo++)
                {
                    # Construct the archive filename
                    if ($iArchiveNo > 255)
                    {
                        confess "backup total * archive total cannot be greater than 255";
                    }

                    $strArchiveFile = uc(sprintf("0000000100000001%08x", $iArchiveNo));

                    &log(INFO, "    archive " .sprintf("%02x", $iArchiveNo) .
                               " - ${strArchiveFile}");

                    my $strSourceFile = $strArchiveFile;

                    if ($bChecksum)
                    {
                        $strSourceFile .= "-${strArchiveChecksum}";
                    }

                    if ($bCompress)
                    {
                        $strSourceFile .= ".gz";
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

                $bCreate = true;
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
            for (my $bCompress = false; $bCompress <= false; $bCompress++)
            {
            for (my $bChecksum = false; $bChecksum <= false; $bChecksum++)
            {
            for (my $bHardlink = false; $bHardlink <= true; $bHardlink++)
            {
            for (my $bArchiveAsync = false; $bArchiveAsync <= $bRemote; $bArchiveAsync++)
            {
                # Increment the run, log, and decide whether this unit test should be run
                if (!BackRestTestCommon_Run(++$iRun,
                                            "rmt ${bRemote}, lrg ${bLarge}, cmp ${bCompress}, chk ${bChecksum}, " .
                                            "hardlink ${bHardlink}, arc_async ${bArchiveAsync}")) {next}

                # Create the test directory
                if ($bCreate)
                {
                    BackRestTestBackup_Create($bRemote);
                    $bCreate = false;
                }

                # Create db config
                BackRestTestCommon_ConfigCreate('db',                                    # local
                                                $bRemote ? REMOTE_BACKUP : undef,        # remote
                                                $bCompress,                              # compress
                                                $bChecksum,                              # checksum
                                                defined($bRemote) ? undef : $bHardlink,  # hardlink
                                                defined($bRemote) ? undef : $iThreadMax, # thread-max
                                                $bArchiveAsync,                          # archive-async
                                                undef);                                  # compress-async

                # Create backup config
                if ($bRemote)
                {
                    BackRestTestCommon_ConfigCreate('backup',                      # local
                                                    $bRemote ? REMOTE_DB : undef,  # remote
                                                    $bCompress,                    # compress
                                                    $bChecksum,                    # checksum
                                                    $bHardlink,                    # hardlink
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

                    for (my $iIncr = 0; $iIncr <= 1; $iIncr++)
                    {
                        &log(INFO, "    " . ($iIncr == 0 ? ("full " . sprintf("%02d", $iFull)) :
                                                           ("    incr " . sprintf("%02d", $iIncr))));

                        BackRestTestBackup_PgExecute('create table test (id int)');

                        BackRestTestCommon_ExecuteBegin($strCommand, $bRemote);

                        if (BackRestTestCommon_ExecuteEnd(TEST_MANIFEST_BUILD))
                        {
                            BackRestTestBackup_PgExecute('drop table test', true);

                            BackRestTestCommon_ExecuteEnd();
                        }
                        else
                        {
                            confess &log(ERROR, 'test point ' . TEST_MANIFEST_BUILD . ' was not found');
                        }


#                        BackRestTestCommon_Execute($strCommand, $bRemote);
                    }
                }
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
