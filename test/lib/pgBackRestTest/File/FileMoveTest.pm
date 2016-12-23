####################################################################################################################################
# FileMoveTest.pm - Tests for File->move()
####################################################################################################################################
package pgBackRestTest::File::FileMoveTest;
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

    # Loop through local/remote ??? enable remote tests and have them throw an error
    for (my $bRemote = 0; $bRemote <= 0; $bRemote++)
    {
    # Loop through source exists
    for (my $bSourceExists = 0; $bSourceExists <= 1; $bSourceExists++)
    {
    # Loop through source errors
    for (my $bSourceError = 0; $bSourceError <= 1; $bSourceError++)
    {
    # Loop through destination exists
    for (my $bDestinationExists = 0; $bDestinationExists <= 1; $bDestinationExists++)
    {
    # Loop through source errors
    for (my $bDestinationError = 0; $bDestinationError <= 1; $bDestinationError++)
    {
    # Loop through create
    for (my $bCreate = 0; $bCreate <= $bDestinationExists; $bCreate++)
    {
        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin(
            "src_exists ${bSourceExists}, src_error ${bSourceError}, " .
            ", dst_exists ${bDestinationExists}, dst_error ${bDestinationError}, dst_create ${bCreate}")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bSourceError || $bDestinationError);

        my $strSourceFile = $self->testPath() . '/test.txt';
        my $strDestinationFile = $self->testPath() . '/test-dest.txt';

        if ($bSourceError)
        {
            $strSourceFile = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . "_private/test.txt";
        }
        elsif ($bSourceExists)
        {
            executeTest("echo 'TESTDATA' > ${strSourceFile}");
        }

        if ($bDestinationError)
        {
            $strDestinationFile = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . "_private/test.txt";
        }
        elsif (!$bDestinationExists)
        {
            $strDestinationFile = $self->testPath() . '/sub/test-dest.txt';
        }

        # Execute in eval in case of error
        eval
        {
            $oFile->move(PATH_BACKUP_ABSOLUTE, $strSourceFile, PATH_BACKUP_ABSOLUTE, $strDestinationFile, $bCreate);
            return true;
        }
        or do
        {
            if (!$bSourceExists || (!$bDestinationExists && !$bCreate) || $bSourceError || $bDestinationError)
            {
                next;
            }

            confess $EVAL_ERROR;
        };

        if (!$bSourceExists || (!$bDestinationExists && !$bCreate) || $bSourceError || $bDestinationError)
        {
            confess 'error should have been raised';
        }

        unless (-e $strDestinationFile)
        {
            confess 'file was not moved';
        }
    }
    }
    }
    }
    }
    }
}

1;
