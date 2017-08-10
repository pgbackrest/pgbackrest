####################################################################################################################################
# BackupUnitTest.pm - Tests for Backup module
####################################################################################################################################
package pgBackRestTest::Module::Backup::BackupUnitTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::Common;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::LibC qw(:config);
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin('backupRegExpGet()'))
    {
        # Expected results matrix
        my $hExpected = {};
        $hExpected->{&false}{&false}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}I';
        $hExpected->{&false}{&false}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}I$';
        $hExpected->{&false}{&true}{&false}{&false} = '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}D';
        $hExpected->{&false}{&true}{&false}{&true} = '^[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}D$';
        $hExpected->{&false}{&true}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}(D|I)';
        $hExpected->{&false}{&true}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F\_[0-9]{8}\-[0-9]{6}(D|I)$';
        $hExpected->{&true}{&false}{&false}{&false} = '[0-9]{8}\-[0-9]{6}F';
        $hExpected->{&true}{&false}{&false}{&true} = '^[0-9]{8}\-[0-9]{6}F$';
        $hExpected->{&true}{&false}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}I){0,1}';
        $hExpected->{&true}{&false}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}I){0,1}$';
        $hExpected->{&true}{&true}{&false}{&false} = '[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}D){0,1}';
        $hExpected->{&true}{&true}{&false}{&true} = '^[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}D){0,1}$';
        $hExpected->{&true}{&true}{&true}{&false} = '[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}(D|I)){0,1}';
        $hExpected->{&true}{&true}{&true}{&true} = '^[0-9]{8}\-[0-9]{6}F(\_[0-9]{8}\-[0-9]{6}(D|I)){0,1}$';

        # Iterate though all possible combinations
        for (my $bFull = false; $bFull <= true; $bFull++)
        {
        for (my $bDiff = false; $bDiff <= true; $bDiff++)
        {
        for (my $bIncr = false; $bIncr <= true; $bIncr++)
        {
        for (my $bAnchor = false; $bAnchor <= true; $bAnchor++)
        {
            # Make sure that an assertion is thrown if no types are requested
            if (!($bFull || $bDiff || $bIncr))
            {
                $self->testException(
                    sub {backupRegExpGet($bFull, $bDiff, $bIncr, $bAnchor)},
                    ERROR_ASSERT, 'at least one backup type must be selected');
            }
            # Else make sure the returned value is correct
            else
            {
                $self->testResult(
                    sub {backupRegExpGet($bFull, $bDiff, $bIncr, $bAnchor)}, $hExpected->{$bFull}{$bDiff}{$bIncr}{$bAnchor},
                    "expression full $bFull, diff $bDiff, incr $bIncr, anchor $bAnchor = " .
                        $hExpected->{$bFull}{$bDiff}{$bIncr}{$bAnchor});
            }
        }
        }
        }
        }
    }

    ################################################################################################################################
    if ($self->begin('backupLabelFormat()'))
    {
        my $strBackupLabelFull = timestampFileFormat(undef, 1482000000) . 'F';
        $self->testResult(sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, 1482000000)}, $strBackupLabelFull, 'full backup label');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, $strBackupLabelFull, 1482000000)},
            ERROR_ASSERT, "strBackupLabelLast must not be defined when strType = 'full'");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strBackupLabelDiff = "${strBackupLabelFull}_" . timestampFileFormat(undef, 1482000400) . 'D';
        $self->testResult(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, $strBackupLabelFull, 1482000400)}, $strBackupLabelDiff, 'diff backup label');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, undef, 1482000400)},
            ERROR_ASSERT, "strBackupLabelLast must be defined when strType = 'diff'");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_INCR, $strBackupLabelDiff, 1482000800)},
            "${strBackupLabelFull}_" . timestampFileFormat(undef, 1482000800) . 'I',
            'incremental backup label');
    }

    ################################################################################################################################
    if ($self->begin('backupLabel()'))
    {
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        #---------------------------------------------------------------------------------------------------------------------------
        my $lTime = time();

        my $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strFullLabel}", {bCreateParent => true});

        my $strNewFullLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $self->testResult(sub {$strFullLabel ne $strNewFullLabel}, true, 'new full label <> existing full backup dir');

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('rmdir ' . storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strFullLabel}"));

        storageRepo()->pathCreate(
            STORAGE_REPO_BACKUP . qw(/) . PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime), {bCreateParent => true});
        storageRepo()->put(
            STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime) .
                "/${strFullLabel}.manifest." . COMPRESS_EXT);

        $strNewFullLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $self->testResult(sub {$strFullLabel ne $strNewFullLabel}, true, 'new full label <> existing full history file');

        #---------------------------------------------------------------------------------------------------------------------------
        $lTime = time() + 1000;
        $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $strNewFullLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);

        $self->testResult(sub {$strFullLabel eq $strNewFullLabel}, true, 'new full label in future');

        #---------------------------------------------------------------------------------------------------------------------------
        $lTime = time();

        $strFullLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_FULL, undef, $lTime);
        my $strDiffLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);
        storageRepo()->pathCreate(STORAGE_REPO_BACKUP . "/${strDiffLabel}", {bCreateParent => true});

        my $strNewDiffLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $self->testResult(sub {$strDiffLabel ne $strNewDiffLabel}, true, 'new diff label <> existing diff backup dir');

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('rmdir ' . storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strDiffLabel}"));

        storageRepo()->pathCreate(
            STORAGE_REPO_BACKUP . qw(/) .  PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime),
            {bIgnoreExists => true, bCreateParent => true});
        storageRepo()->put(
            STORAGE_REPO_BACKUP . qw{/} . PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTime) .
                "/${strDiffLabel}.manifest." . COMPRESS_EXT);

        $strNewDiffLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $self->testResult(sub {$strDiffLabel ne $strNewDiffLabel}, true, 'new full label <> existing diff history file');

        #---------------------------------------------------------------------------------------------------------------------------
        $lTime = time() + 1000;
        $strDiffLabel = backupLabelFormat(CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $strNewDiffLabel = backupLabel(storageRepo(), CFGOPTVAL_BACKUP_TYPE_DIFF, $strFullLabel, $lTime);

        $self->testResult(sub {$strDiffLabel eq $strNewDiffLabel}, true, 'new diff label in future');
    }
}

1;
