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

use File::Basename qw(dirname);
use Time::HiRes qw(gettimeofday);
use File::stat;
use Exporter qw(import);

use lib dirname($0) . '/../lib';
use BackRest::Utility;
use BackRestTest::CommonTest;
use BackRestTest::BackupTest;

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

    my $iDbOid = BackRestTestBackup_PgSelectOne("select oid from pg_database where datname = 'postgres'");

    for (my $iTableIdx = 0; $iTableIdx < $iTableTotal; $iTableIdx++)
    {
        my $strTableName = "test_${iTableIdx}";

        BackRestTestBackup_PgExecute("create table ${strTableName} (id int)");
        my $iTableOid = BackRestTestBackup_PgSelectOne("select oid from pg_class where relname = '${strTableName}'");
        my $strTableFile = BackRestTestCommon_DbCommonPathGet() . "/base/${iDbOid}/${iTableOid}";

        my $lSize;

        do
        {
            BackRestTestBackup_PgExecute("do \$\$ declare iIndex int; begin for iIndex in 1..289000 loop insert into ${strTableName} values (1); end loop; end \$\$;");
            BackRestTestBackup_PgExecute("checkpoint");
            my $oStat = stat($strTableFile);

            # Evaluate error
            if (!defined($oStat))
            {
                confess "cannot stat ${strTableFile}";
            }

            $lSize = $oStat->size;
        }
        while ($lSize < $iTableSize * 1024 * 1024);
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
        BackRestTestBackup_Create($bRemote, undef, false);
        BackRestTestCompare_BuildDb(48, 10);
        BackRestTestBackup_ClusterStop();

        for (my $bRemote = true; $bRemote <= true; $bRemote++)
        {
        for (my $bRsync = true; $bRsync >= false; $bRsync--)
        {
            my $strCommand;
            BackRestTestCommon_CreateRepo($bRemote);

            if ($bRsync)
            {
                $strCommand = 'rsync -zvlhprtogHS --delete ' . BackRestTestCommon_DbCommonPathGet() . '/ ' .
                              ($bRemote ? BackRestTestCommon_UserBackRestGet . '@' . BackRestTestCommon_HostGet . ':' : '') .
                                          BackRestTestCommon_RepoPathGet();
            }
            else
            {
                $strCommand = BackRestTestCommon_CommandMainGet() .
                                 ' --stanza=main' .
                                 # ' "--cmd-psql=' . BackRestTestCommon_CommandPsqlGet() . '"' .
                                 # ' "--cmd-psql-option= -p ' . BackRestTestCommon_DbPortGet . '"' .
                                 ($bRemote ? ' "--db-host=' . BackRestTestCommon_HostGet . '"' .
                                             ' "--db-user=' . BackRestTestCommon_UserGet . '"' : '') .
                                 ' --log-level-file=debug' .
                                 ' --no-start-stop' .
#                                ' --no-compress' .
                                 ' --thread-max=4' .
                                 ' "--db-path=' . BackRestTestCommon_DbCommonPathGet() . '"' .
                                 ' "--repo-path=' . BackRestTestCommon_RepoPathGet() . '"' .
                                 ' --type=full backup';
            }

            my $fTimeBegin = gettimeofday();
            BackRestTestCommon_Execute($strCommand, !$bRsync && $bRemote);
            my $fTimeEnd = gettimeofday();

            &log(INFO, ($bRsync ? 'rsync' : 'backrest') . " time = " . (int(($fTimeEnd - $fTimeBegin) * 100) / 100));
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
