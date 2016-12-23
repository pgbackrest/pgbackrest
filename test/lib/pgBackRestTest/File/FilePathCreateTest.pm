####################################################################################################################################
# FilePathCreateTest.pm - Tests for File->pathCreate()
####################################################################################################################################
package pgBackRestTest::File::FilePathCreateTest;
use parent 'pgBackRestTest::File::FileCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Fcntl qw(:mode);
use File::stat;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::File;
use pgBackRest::FileCommon;

use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Loop through local/remote
    for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
    {
    # Loop through error
    for (my $bError = 0; $bError <= 1; $bError++)
    {
    # Loop through mode (mode will be set on true)
    foreach my $strMode (undef, '0700')
    {
        my $strPathType = PATH_BACKUP_CLUSTER;

        # Increment the run, log, and decide whether this unit test should be run
        if (!$self->begin("rmt ${bRemote}, err ${bError}, mode " . (defined($strMode) ? $strMode : 'undef'))) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        filePathCreate($self->testPath() . '/backup', '770');
        filePathCreate($self->testPath() . '/backup/db', '770');

        my $strPath = 'path';

        # If not exists then set the path to something bogus
        if ($bError)
        {
            $strPath = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . "_private/path";

            $strPathType = PATH_BACKUP_ABSOLUTE;
        }

        # Execute in eval to catch errors
        my $bErrorExpected = $bError;

        eval
        {
            $oFile->pathCreate($strPathType, $strPath, $strMode);
            return true;
        }
        # Check for errors
        or do
        {
            # Ignore errors if the path did not exist
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

        # Make sure the path was actually created
        my $strPathCheck = $oFile->pathGet($strPathType, $strPath);

        unless (-e $strPathCheck)
        {
            confess 'path was not created';
        }

        # Check that the mode was set correctly
        my $oStat = lstat($strPathCheck);

        if (!defined($oStat))
        {
            confess "unable to stat ${strPathCheck}";
        }

        if (defined($strMode))
        {
            if ($strMode ne sprintf('%04o', S_IMODE($oStat->mode)))
            {
                confess "mode was not set to {$strMode}";
            }
        }
    }
    }
    }
}

1;
