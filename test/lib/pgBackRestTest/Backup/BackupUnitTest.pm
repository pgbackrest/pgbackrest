####################################################################################################################################
# BackupUnitTest.pm - Tests for Backup module
####################################################################################################################################
package pgBackRestTest::Backup::BackupUnitTest;
use parent 'pgBackRestTest::Backup::BackupCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::BackupCommon;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Increment the run, log, and decide whether this unit test should be run
    if (!$self->begin('unit')) {return}

    # Unit tests for backupLabelFormat()
    #-----------------------------------------------------------------------------------------------------------------------
    {
        # Test full backup label
        my $strBackupLabelFull = timestampFileFormat(undef, 1482000000) . 'F';
        $self->testResult(sub {backupLabelFormat(BACKUP_TYPE_FULL, undef, 1482000000)}, $strBackupLabelFull);

        # Make sure that an assertion is thrown if strBackupLabelLast is passed when formatting a full label
        $self->testException(
            sub {backupLabelFormat(BACKUP_TYPE_FULL, $strBackupLabelFull, 1482000000)},
            ERROR_ASSERT, "strBackupLabelLast must not be defined when strType = 'full'");

        # Test diff backup label
        my $strBackupLabelDiff = "${strBackupLabelFull}_" . timestampFileFormat(undef, 1482000400) . 'D';
        $self->testResult(sub {backupLabelFormat(BACKUP_TYPE_DIFF, $strBackupLabelFull, 1482000400)}, $strBackupLabelDiff);

        # Make sure that an assertion is thrown if strBackupLabelLast is not passed when formatting a diff label.  The same
        # check is used from incr labels so no need for a separate test.
        $self->testException(
            sub {backupLabelFormat(BACKUP_TYPE_DIFF, undef, 1482000400)},
            ERROR_ASSERT, "strBackupLabelLast must be defined when strType = 'diff'");

        # Test incr backup label
        $self->testResult(
            sub {backupLabelFormat(BACKUP_TYPE_INCR, $strBackupLabelDiff, 1482000800)},
            "${strBackupLabelFull}_" . timestampFileFormat(undef, 1482000800) . 'I');
    }

    # Unit tests for backupRegExpGet()
    #-----------------------------------------------------------------------------------------------------------------------
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
                    sub {backupRegExpGet($bFull, $bDiff, $bIncr, $bAnchor)},
                    $hExpected->{$bFull}{$bDiff}{$bIncr}{$bAnchor});
            }
        }
        }
        }
        }
    }
}

1;
