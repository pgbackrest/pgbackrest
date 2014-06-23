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
# use Cwd 'abs_path';
# use File::stat;
# use Fcntl ':mode';
# use Scalar::Util 'blessed';

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
# BackRestTestBackup_ClusterDrop
####################################################################################################################################
sub BackRestTestBackup_ClusterDrop
{
    my $strPath = shift;

    # If the db directory already exists, stop the cluster and remove the directory
    if (-e $strPath . "/postmaster.pid")
    {
        BackRestTestCommon_Execute("pg_ctl stop -D $strPath -w -s -m fast");
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
    BackRestTestCommon_Execute("/Library/PostgreSQL/9.3/bin/pg_ctl start -o \"-c port=$iPort -c checkpoint_segments=1 -c wal_level=archive " .
                               "-c archive_mode=on -c archive_command='$strArchive'\" " .
                               "-D $strPath -l $strPath/postgresql.log -w -s");
}

####################################################################################################################################
# BackRestTestBackup_Setup
####################################################################################################################################
sub BackRestTestBackup_Setup
{
    my $strRemote;
    my $bDropOnly = shift;

    BackRestTestBackup_ClusterDrop($strTestPath . "/db/common");

    # Remove the backrest private directory
    if (-e "${strTestPath}/backrest")
    {
        BackRestTestCommon_ExecuteBackRest("rm -rf ${strTestPath}/backrest", true);
    }

    # Remove the test directory
    system("rm -rf ${strTestPath}") == 0 or die 'unable to remove ${strTestPath} path';

    if (!defined($bDropOnly) || !$bDropOnly)
    {
        # Create the test directory
        mkdir($strTestPath, oct("0770")) or confess "Unable to create ${strTestPath} path";

        # Create the db directory
        mkdir($strTestPath . "/db", oct("0700")) or confess "Unable to create ${strTestPath}/db path";

        # Create the db/common directory
        mkdir($strTestPath . "/db/common") or confess "Unable to create ${strTestPath}/db/common path";

        # Create the cluster
        BackRestTestBackup_ClusterCreate($strTestPath . "/db/common", BackRestTestCommon_DbPortGet);

        # Create the backrest directory
        BackRestTestCommon_ExecuteBackRest("mkdir -m 770 ${strTestPath}/backrest")
    }
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

    BackRestTestBackup_Setup();

    BackRestTestCommon_ConfigCreate(BackRestTestCommon_DbPathGet() . '/pg_backrest.conf', REMOTE_DB);#, REMOTE_BACKUP);
    BackRestTestCommon_ConfigCreate(BackRestTestCommon_BackupPathGet() . '/pg_backrest.conf', REMOTE_BACKUP);#, REMOTE_DB);

    BackRestTestCommon_Execute(BackRestTestCommon_CommandMainGet() . ' --config=' . BackRestTestCommon_BackupPathGet() .
                                       "/pg_backrest.conf --type=incr --stanza=${strStanza} backup");

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
