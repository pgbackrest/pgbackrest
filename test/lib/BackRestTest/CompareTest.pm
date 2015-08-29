#!/usr/bin/perl
####################################################################################################################################
# CompareTest.pl - Performance comparison tests between rsync and backrest
####################################################################################################################################
package BackRestTest::CompareTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname);
use File::stat;
use Time::HiRes qw(gettimeofday);

use lib dirname($0) . '/../lib';
use BackRest::Common::Log;

use BackRestTest::BackupTest;
use BackRestTest::CommonTest;

####################################################################################################################################
# Exports
####################################################################################################################################
our @EXPORT = qw(BackRestTestCompare_Test);

####################################################################################################################################
# BackRestTestCompare_BuildDb
####################################################################################################################################
sub BackRestTestCompare_BuildDb
{
    my $iTableTotal = shift;
    my $iTableSize = shift;

    &log(INFO, "build database: " . fileSizeFormat($iTableTotal * $iTableSize * 1024 * 1024));

    for (my $iTableIdx = 0; $iTableIdx < $iTableTotal; $iTableIdx++)
    {
        my $strSourceFile = BackRestTestCommon_DataPathGet() . "/test.table.bin";
        my $strTableFile = BackRestTestCommon_DbCommonPathGet() . "/test-${iTableIdx}";

        for (my $iTableSizeIdx = 0; $iTableSizeIdx < $iTableSize; $iTableSizeIdx++)
        {
            BackRestTestCommon_Execute("cat ${strSourceFile} >> ${strTableFile}");
        }
    }
}

####################################################################################################################################
# BackRestTestCompare_Test
####################################################################################################################################
sub BackRestTestCompare_Test
{
    my $strTest = shift;

    #-------------------------------------------------------------------------------------------------------------------------------
    # Test rsync
    #-------------------------------------------------------------------------------------------------------------------------------
    if ($strTest eq 'all' || $strTest eq 'rsync')
    {
        my $iRun = 0;
        my $bRemote = false;

        &log(INFO, "Test rsync\n");

        # Increment the run, log, and decide whether this unit test should be run
        if (!BackRestTestCommon_Run(++$iRun,
                                    "rmt ${bRemote}")) {next}

        # Create the cluster and paths
        BackRestTestBackup_Create($bRemote, false);
        BackRestTestCommon_PathCreate(BackRestTestCommon_DbCommonPathGet() . '/pg_tblspc');

        BackRestTestCompare_BuildDb(48, 10);
        BackRestTestCommon_Execute('sync');

        for (my $bRemote = true; $bRemote <= true; $bRemote++)
        {
        for (my $bRsync = true; $bRsync >= false; $bRsync--)
        {
            my $strCommand;
            BackRestTestCommon_CreateRepo($bRemote);

            &log(INFO, ($bRsync ? 'rsync' : 'backrest') . " test");

            if ($bRsync)
            {
                $strCommand = 'rsync --compress-level=6 -zvlhprtogHS --delete ' .
                              ($bRemote ? BackRestTestCommon_UserGet . '@' . BackRestTestCommon_HostGet . ':' : '') .
                              BackRestTestCommon_DbCommonPathGet() . '/ ' . BackRestTestCommon_RepoPathGet() . ';' .
                              'gzip -r "' . BackRestTestCommon_RepoPathGet() . '"';
            }
            else
            {
                $strCommand = BackRestTestCommon_CommandMainGet() .
                                 ' --stanza=main' .
                                 ($bRemote ? ' "--db-host=' . BackRestTestCommon_HostGet . '"' .
                                             ' "--db-user=' . BackRestTestCommon_UserGet . '"' : '') .
#                                 ' --log-level-file=debug' .
                                 ' --no-start-stop' .
#                                ' --no-compress' .
                                ' --thread-max=4' .
                                 ' "--db-path=' . BackRestTestCommon_DbCommonPathGet() . '"' .
                                 ' "--repo-path=' . BackRestTestCommon_RepoPathGet() . '"' .
                                 ' --type=full backup';
            }

            my $fTimeBegin = gettimeofday();
            BackRestTestCommon_Execute($strCommand, $bRemote);
            BackRestTestCommon_Execute('sync');
            my $fTimeEnd = gettimeofday();

            &log(INFO, "    time = " . (int(($fTimeEnd - $fTimeBegin) * 100) / 100));
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
