####################################################################################################################################
# FileCompressTest.pm - Tests for File->compress()
####################################################################################################################################
package pgBackRestTest::Module::File::FileCompressTest;
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

    # Loop through local/remote ??? enable remote tests and have them throw an error
    foreach my $bRemote (false)
    {
    # Loop through exists
    foreach my $bExists (false, true)
    {
    # Loop through error
    foreach my $bError (false, true)
    {
        if (!$self->begin("rmt ${bRemote}, exists ${bExists}, err ${bError}")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        my $strFile = $self->testPath() . '/test.txt';
        my $strSourceHash;
        my $iSourceSize;

        if ($bError)
        {
            $strFile = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . '_private/test.txt';
        }
        elsif ($bExists)
        {
            executeTest("echo 'TESTDATA' > ${strFile}");
            ($strSourceHash, $iSourceSize) = $oFile->hashSize(PATH_BACKUP_ABSOLUTE, $strFile);
        }

        # Execute in eval in case of error
        eval
        {
            $oFile->compress(PATH_BACKUP_ABSOLUTE, $strFile);
            return true;
        }
        or do
        {
            if (!$bExists || $bError)
            {
                next;
            }

            confess $EVAL_ERROR;
        };

        if (!$bExists || $bError)
        {
            confess 'expected error';
        }

        my $strDestinationFile = $strFile . '.gz';

        if (-e $strFile)
        {
            confess 'source file still exists';
        }

        unless (-e $strDestinationFile)
        {
            confess 'file was not compressed';
        }

        executeTest("gzip -d ${strDestinationFile}");

        my ($strDestinationHash, $iDestinationSize) = $oFile->hashSize(PATH_BACKUP_ABSOLUTE, $strFile);

        if ($strSourceHash ne $strDestinationHash)
        {
            confess "source ${strSourceHash} and destination ${strDestinationHash} file hashes do not match";
        }
    }
    }
    }
}

1;
