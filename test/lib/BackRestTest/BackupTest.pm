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
        BackRestTestCommon_Execute(['pg_ctl', 'stop', "-D $strPath", '-w', '-s', '-m fast']);
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
        BackRestTestCommon_Execute(['pg_ctl', 'restart', "-D $strPath", '-w', '-s']);
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

    BackRestTestCommon_Execute(['initdb', '-D', $strPath, '-A',  'trust']);
    BackRestTestCommon_Execute(['pg_ctl', 'start', '-o', '"-c', "port=$iPort", '-c', 'checkpoint_segments=1', '-c',
                                'wal_level=archive', '-c', 'archive_mode=on', '-c', "archive_command='$strArchive'\"",
                                '-D', $strPath, '-l', "$strPath/postgresql.log", '-w', '-s']);
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
        BackRestTestCommon_ExecuteOld('rm -rf ' . BackRestTestCommon_BackupPathGet(), true, true);
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

    # Set defaults
    $bRemote = defined($bRemote) ? $bRemote : false;

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
        BackRestTestCommon_ExecuteOld("mkdir -m 700 " . BackRestTestCommon_BackupPathGet(), true);
    }
    else
    {
        mkdir(BackRestTestCommon_BackupPathGet(), oct('0700'))
            or confess 'Unable to create ' . BackRestTestCommon_BackupPathGet() . ' path';
    }

    # Create the cluster
    BackRestTestBackup_ClusterCreate(BackRestTestCommon_DbCommonPathGet(), BackRestTestCommon_DbPortGet());
}

####################################################################################################################################
# BackRestTestBackup_Test
####################################################################################################################################
sub BackRestTestBackup_Test
{
    my $strTest = shift;
    my $iTestRun = shift;

    # If no test was specified, then run them all
    if (!defined($strTest))
    {
        $strTest = 'all';
    }

    # Setup test variables
    my $iRun;
    $strTestPath = BackRestTestCommon_TestPathGet();
    my $strStanza = BackRestTestCommon_StanzaGet();
    my $strUser = BackRestTestCommon_UserGet();
    my $strGroup = BackRestTestCommon_GroupGet();
    $strHost = BackRestTestCommon_HostGet();
    $strUserBackRest = BackRestTestCommon_UserBackRestGet();

    # Print test banner
    &log(INFO, "BACKUP MODULE ******************************************************************");

    if ($strTest eq 'all' || $strTest eq 'full')
    {
        $iRun = 0;

        &log(INFO, "Test Full Backup\n");

        for (my $bRemote = false; $bRemote <= true; $bRemote++)
        {
            BackRestTestBackup_Create($bRemote);

            for (my $bHardlink = false; $bHardlink <= true; $bHardlink++)
            {
                my %oDbConfigHash;
                my %oBackupConfigHash;

                # Confgure hard-linking
                if ($bHardlink)
                {
                    $oBackupConfigHash{'global:backup'}{hardlink} = 'y';
                }

                # for (my $bArchiveLocal = false; $bArchiveLocal <= true; $bArchiveLocal++)
                # {
                    BackRestTestCommon_ConfigCreate('db',
                                                    ($bRemote ? REMOTE_BACKUP : undef), \%oDbConfigHash);
                    BackRestTestCommon_ConfigCreate('backup',
                                                    ($bRemote ? REMOTE_DB : undef), \%oBackupConfigHash);

                    for (my $iFull = 1; $iFull <= 1; $iFull++)
                    {
                        $iRun++;

                        &log(INFO, "run ${iRun} - " .
                                   "remote ${bRemote}, full ${iFull}");

                        my $strCommand = BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_BackupPathGet() .
                                                   "/pg_backrest.conf --type=incr --stanza=${strStanza} backup";

                        BackRestTestCommon_ExecuteOld($strCommand, $bRemote);

                        for (my $iIncr = 1; $iIncr <= 1; $iIncr++)
                        {
                            $iRun++;

                            &log(INFO, "run ${iRun} - " .
                                       "remote ${bRemote}, full ${iFull}, hardlink ${bHardlink}, incr ${iIncr}");

                            BackRestTestCommon_ExecuteOld($strCommand, $bRemote);
                        }
                    }
                # }
            }

            BackRestTestBackup_Drop();
        }
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test path_create()
    #-------------------------------------------------------------------------------------------------------------------------------
    # if ($strTest eq 'all' || $strTest eq 'path_create')
    # {
    #     $iRun = 0;
    #
    #     &log(INFO, "Test File->path_create()\n");
    #
    #     # Loop through local/remote
    #     for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
    #     {
    #         # Create the file object
    #         my $oFile = (BackRest::File->new
    #         (
    #             strStanza => $strStanza,
    #             strBackupPath => $strTestPath,
    #             strRemote => $bRemote ? 'backup' : undef,
    #             oRemote => $bRemote ? $oRemote : undef
    #         ))->clone();
    #
    #         # Loop through exists (does the paren path exist?)
    #         for (my $bExists = 0; $bExists <= 1; $bExists++)
    #         {
    #         # Loop through exists (does the paren path exist?)
    #         for (my $bError = 0; $bError <= 1; $bError++)
    #         {
    #         # Loop through permission (permission will be set on true)
    #         for (my $bPermission = 0; $bPermission <= $bExists; $bPermission++)
    #         {
    #             my $strPathType = PATH_BACKUP_CLUSTER;
    #
    #             $iRun++;
    #
    #             if (defined($iTestRun) && $iTestRun != $iRun)
    #             {
    #                 next;
    #             }
    #
    #             &log(INFO, "run ${iRun} - " .
    #                        "remote ${bRemote}, exists ${bExists}, error ${bError}, permission ${bPermission}");
    #
    #             # Setup test directory
    #             BackRestTestFile_Setup($bError);
    #
    #             mkdir("$strTestPath/backup") or confess "Unable to create test/backup directory";
    #             mkdir("$strTestPath/backup/db") or confess "Unable to create test/backup/db directory";
    #
    #             my $strPath = "path";
    #             my $strPermission;
    #
    #             # If permission then set one (other than the default)
    #             if ($bPermission)
    #             {
    #                 $strPermission = "0700";
    #             }
    #
    #             # If not exists then set the path to something bogus
    #             if ($bError)
    #             {
    #                 $strPath = "${strTestPath}/private/path";
    #                 $strPathType = PATH_BACKUP_ABSOLUTE;
    #             }
    #             elsif (!$bExists)
    #             {
    #                 $strPath = "error/path";
    #             }
    #
    #             # Execute in eval to catch errors
    #             my $bErrorExpected = !$bExists || $bError;
    #
    #             eval
    #             {
    #                 $oFile->path_create($strPathType, $strPath, $strPermission);
    #             };
    #
    #             # Check for errors
    #             if ($@)
    #             {
    #                 # Ignore errors if the path did not exist
    #                 if ($bErrorExpected)
    #                 {
    #                     next;
    #                 }
    #
    #                 confess "error raised: " . $@ . "\n";
    #             }
    #
    #             if ($bErrorExpected)
    #             {
    #                 confess 'error was expected';
    #             }
    #
    #             # Make sure the path was actually created
    #             my $strPathCheck = $oFile->path_get($strPathType, $strPath);
    #
    #             unless (-e $strPathCheck)
    #             {
    #                 confess "path was not created";
    #             }
    #
    #             # Check that the permissions were set correctly
    #             my $oStat = lstat($strPathCheck);
    #
    #             if (!defined($oStat))
    #             {
    #                 confess "unable to stat ${strPathCheck}";
    #             }
    #
    #             if ($bPermission)
    #             {
    #                 if ($strPermission ne sprintf("%04o", S_IMODE($oStat->mode)))
    #                 {
    #                     confess "permissions were not set to {$strPermission}";
    #                 }
    #             }
    #         }
    #         }
    #         }
    #     }
    # }

#    BackRestTestBackup_Setup(true);
}

1;
