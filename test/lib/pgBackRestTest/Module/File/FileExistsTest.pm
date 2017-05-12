####################################################################################################################################
# FileExistsTest.pm - Tests for File->exists()
####################################################################################################################################
package pgBackRestTest::Module::File::FileExistsTest;
use parent 'pgBackRestTest::Module::File::FileCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
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
    foreach my $bExists (false, true)
    {
    # Loop through exists
    foreach my $bError ($bExists ? (false, true) : (false))
    {
        if (!$self->begin("rmt ${bRemote}, err ${bError}, exists ${bExists}")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        my $strFile = $self->testPath() . '/test.txt';

        if ($bError)
        {
            $strFile = $self->testPath() . '/private/test.txt';
        }
        elsif ($bExists)
        {
            executeTest("echo 'TESTDATA' > ${strFile}");
        }

        # Execute in eval in case of error
        eval
        {
            if ($oFile->exists(PATH_BACKUP_ABSOLUTE, $strFile) != $bExists)
            {
                confess "bExists is set to ${bExists}, but exists() returned " . !$bExists;
            }

            return true;
        }
        or do
        {
            my $oException = $@;
            my $iCode;
            my $strMessage;

            if (isException($oException))
            {
                $iCode = $oException->code();
                $strMessage = $oException->message();
            }
            else
            {
                $strMessage = $oException;
            }

            if ($bError)
            {
                next;
            }

            confess 'error raised: ' . $strMessage . "\n";
        };
    }
    }
    }
}

1;
