#!/usr/bin/perl
####################################################################################################################################
# BackupTest.pl - Unit Tests for BackRest::Backup
####################################################################################################################################
use BackRestTest::BackupTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings;
use english;
use Carp;

use File::Basename;
use Cwd 'abs_path';
use File::stat;
use Fcntl ':mode';
use Scalar::Util 'blessed';

use lib dirname($0) . "/../lib";
use BackRest::Utility;
use BackRest::File;
use BackRest::Remote;

use Exporter qw(import);
our @EXPORT = qw(BackRestFileTest);

# my $strTestPath;
# my $strHost;
# my $strUserBackRest;

####################################################################################################################################
# BackRestFileBackupSetup
####################################################################################################################################
sub BackRestFileBackupSetup
{
    my $bPrivate = shift;
    my $bDropOnly = shift;

    my $strTestPath = BackRestCommonTestPathGet();

    # Remove the backrest private directory
    if (-e "${strTestPath}/private")
    {
        system("ssh ${strUserBackRest}\@${strHost} 'rm -rf ${strTestPath}/private'");
    }

    # Remove the test directory
    system("rm -rf ${strTestPath}") == 0 or die 'unable to drop test path';

    if (!defined($bDropOnly) || !$bDropOnly)
    {
        # Create the test directory
        mkdir($strTestPath, oct("0770")) or confess "Unable to create test directory";

        # Create the backrest private directory
        if (defined($bPrivate) && $bPrivate)
        {
            system("ssh backrest\@${strHost} 'mkdir -m 700 ${strTestPath}/private'") == 0 or die 'unable to create test/private path';
        }
    }
}

####################################################################################################################################
# BackRestBackupTest
####################################################################################################################################
sub BackRestBackupTest
{
    my $strStanza = shift;
    my $strCommand = shift;
    my $strHost = shift;
    my $strUser = shift;
    my $strGroup = shift;
    my $strUserBackRest = shift;
    my $strTestPath = shift;
    my $strTest = shift;

    # If no test was specified, then run them all
    if (!defined($strTest))
    {
        $strTest = 'all';
    }

    # Setup test variables
    my $iRun;
    my $strTestPath = BackRestCommonTestPathGet();
    my $strStanza = BackRestCommonStanzaGet();
    # my $strHost = "127.0.0.1";
    # my $strUser = getpwuid($<);
    # my $strGroup = getgrgid($();
    # $strUserBackRest = 'backrest';

    # Print test parameters
    &log(INFO, "Testing with test_path = ${strTestPath}, host = ${strHost}, user = ${strUser}, group = ${strGroup}");

    &log(INFO, "FILE MODULE ********************************************************************");

    system("ssh backrest\@${strHost} 'rm -rf ${strTestPath}/private'");

    #-------------------------------------------------------------------------------------------------------------------------------
    # Create remote
    #-------------------------------------------------------------------------------------------------------------------------------
    my $oRemote = BackRest::Remote->new
    (
        strHost => BackRestCommonHostGet(),
        strUser => BackRestCommonUserGet(),
        strCommand => $strCommand,
    );

    # #-------------------------------------------------------------------------------------------------------------------------------
    # # Test path_create()
    # #-------------------------------------------------------------------------------------------------------------------------------
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
    #             strStanza => "db",
    #             strBackupPath => ${strTestPath},
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
    #             &log(INFO, "run ${iRun} - " .
    #                        "remote ${bRemote}, exists ${bExists}, error ${bError}, permission ${bPermission}");
    #
    #             # Setup test directory
    #             BackRestFileTestSetup($bError);
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
    #
    #                 # # Make sure that we are not testing with the default permission
    #                 # if ($strPermission eq $oFile->{strDefaultPathPermission})
    #                 # {
    #                 #     confess 'cannot set test permission ${strPermission} equal to default permission' .
    #                 #             $oFile->{strDefaultPathPermission};
    #                 # }
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
    #
    # BackRestFileTestSetup(false, true);
}

1;
