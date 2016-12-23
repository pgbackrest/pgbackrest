####################################################################################################################################
# FileWaitTest.pm - Tests for File->wait()
####################################################################################################################################
package pgBackRestTest::File::FileWaitTest;
use parent 'pgBackRestTest::File::FileCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use POSIX qw(ceil);
use Time::HiRes qw(gettimeofday usleep);

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
    for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
    {
        my $lTimeBegin = gettimeofday();

        if (!$self->begin("rmt ${bRemote}, begin ${lTimeBegin}")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote);

        # If there is not enough time to complete the test then sleep
        if (ceil($lTimeBegin) - $lTimeBegin < .250)
        {
            my $lSleepMs = ceil(((int($lTimeBegin) + 1) - $lTimeBegin) * 1000);

            usleep($lSleepMs * 1000);

            &log(DEBUG, "slept ${lSleepMs}ms: begin ${lTimeBegin}, end " . gettimeofday());

            $lTimeBegin = gettimeofday();
        }

        # Run the test
        my $lTimeBeginCheck = $oFile->wait(PATH_DB_ABSOLUTE);

        &log(DEBUG, "begin ${lTimeBegin}, check ${lTimeBeginCheck}, end " . time());

        # Current time should have advanced by 1 second
        if (int(time()) == int($lTimeBegin))
        {
            confess "time was not advanced by 1 second";
        }

        # lTimeBegin and lTimeBeginCheck should be equal
        if (int($lTimeBegin) != $lTimeBeginCheck)
        {
            confess 'time begin ' || int($lTimeBegin) || "and check ${lTimeBeginCheck} should be equal";
        }
    }
}

1;
