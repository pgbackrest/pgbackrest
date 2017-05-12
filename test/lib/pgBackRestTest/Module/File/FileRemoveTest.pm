####################################################################################################################################
# FileRemoveTest.pm - Tests for File->remove()
####################################################################################################################################
package pgBackRestTest::Module::File::FileRemoveTest;
use parent 'pgBackRestTest::Module::File::FileCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Log;
use pgBackRest::File;

use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Loop through local/remote
    foreach my $bRemote (false, true)
    {
    # Loop through exists
    foreach my $bError (false, true)
    {
    # Loop through exists
    foreach my $bExists (false, true)
    {
    # Loop through temp
    foreach my $bTemp (false, true)
    {
    # Loop through ignore missing
    foreach my $bIgnoreMissing (false, true)
    {
        if (!$self->begin(
            "rmt ${bRemote}, err = ${bError}, exists ${bExists}, tmp ${bTemp}, ignmis ${bIgnoreMissing}")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        my $strFile = $self->testPath() . '/test.txt';

        if ($bError)
        {
            $strFile = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . '_private/test.txt';
        }
        elsif (!$bExists)
        {
            $strFile = $self->testPath() . '/private/error.txt';
        }
        else
        {
            executeTest("echo 'TESTDATA' > ${strFile}" . ($bTemp ? '.pgbackrest.tmp' : ''));
        }

        # Execute in eval in case of error
        my $bRemoved;

        eval
        {
            $bRemoved = $oFile->remove(PATH_BACKUP_ABSOLUTE, $strFile, $bTemp, $bIgnoreMissing);
            return true;
        }
        or do
        {
            if ($bError || $bRemote)
            {
                next;
            }

            if (!$bExists && !$bIgnoreMissing)
            {
                next;
            }

            confess $EVAL_ERROR;
        };

        if ($bError || $bRemote)
        {
            confess 'error should have been returned';
        }

        if (!$bRemoved)
        {
            if (!$bExists && $bIgnoreMissing)
            {
                next;
            }

            confess 'remove returned false, but something should have been removed';
        }

        if (-e ($strFile . ($bTemp ? '.pgbackrest.tmp' : '')))
        {
            confess 'file still exists';
        }
    }
    }
    }
    }
    }
}

1;
