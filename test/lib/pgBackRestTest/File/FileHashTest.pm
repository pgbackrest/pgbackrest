####################################################################################################################################
# FileHashTest.pm - Tests for File->hash()
####################################################################################################################################
package pgBackRestTest::File::FileHashTest;
use parent 'pgBackRestTest::File::FileCommonTest';

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
    # Loop through error
    foreach my $bError (false, true)
    {
    # Loop through exists
    foreach my $bExists (false, true)
    {
    # Loop through exists
    foreach my $bCompressed (false, true)
    {
        if (!$self->begin("rmt ${bRemote}, err ${bError}, exists ${bExists}, cmp ${bCompressed}")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        my $strFile = $self->testPath() . '/test.txt';

        if ($bError)
        {
            $strFile = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . '_private/test.txt';
        }
        elsif (!$bExists)
        {
            $strFile = $self->testPath() . '/error.txt';
        }
        else
        {
            executeTest("echo 'TESTDATA' > ${strFile}");

            if ($bCompressed && !$bRemote)
            {
                $oFile->compress(PATH_BACKUP_ABSOLUTE, $strFile);
                $strFile = $strFile . '.gz';
            }
        }

        # Execute in eval in case of error
        my $strHash;
        my $iSize;
        my $bErrorExpected = !$bExists || $bError || $bRemote;

        eval
        {
            ($strHash, $iSize) = $oFile->hashSize(PATH_BACKUP_ABSOLUTE, $strFile, $bCompressed);
            return true;
        }
        or do
        {
            if ($bErrorExpected)
            {
                next;
            }

            confess $EVAL_ERROR;
        };

        if ($bErrorExpected)
        {
            confess 'error was expected';
        }

        if ($strHash ne '06364afe79d801433188262478a76d19777ef351')
        {
            confess 'hashes do not match';
        }
    }
    }
    }
    }
}

1;
