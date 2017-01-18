####################################################################################################################################
# FileOwnerTest.pm - Tests for File->owner()
####################################################################################################################################
package pgBackRestTest::File::FileOwnerTest;
use parent 'pgBackRestTest::File::FileCommonTest';

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
use pgBackRestTest::Common::RunTest;

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
    foreach my $bExists (!$bRemote ? (false, true) : (false))
    {
    # Loop through exists
    foreach my $bError ($bExists && !$bRemote ? (false, true) : (false))
    {
    # Loop through users
    foreach my $strUser ($bExists && !$bError && !$bRemote ? ($self->pgUser(), BOGUS, undef) : ($self->pgUser()))
    {
    # Loop through group
    foreach my $strGroup (
        $bExists && !$bError && !$bRemote && defined($strUser) && $strUser ne BOGUS ? ($self->group(), BOGUS, undef) : ($self->group()))
    {
        if (!$self->begin(
            "rmt ${bRemote}, err ${bError}, exists ${bExists}, user " . (defined($strUser) ? $strUser : '[undef]') .
            ", group " . (defined($strGroup) ? $strGroup : '[undef]'))) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        # Create the test file
        my $strFile = $self->testPath() . '/test.txt';

        if ($bExists)
        {
            executeTest("echo 'TESTDATA' > ${strFile}");

            if ($bError)
            {
                executeTest("sudo chown root:root ${strFile}");
            }
        }

        my $strFunction = sub {$oFile->owner(PATH_BACKUP_ABSOLUTE, $strFile, $strUser, $strGroup)};

        # Remote operation not allowed
        if ($bRemote)
        {
            $self->testException($strFunction, ERROR_ASSERT, "pgBackRest::File->owner: remote operation not supported");
        }
        # Error on not exists
        elsif (!$bExists)
        {
            $self->testException($strFunction, ERROR_FILE_MISSING, "${strFile} does not exist");
        }
        # Else permission error
        elsif ($bError)
        {
            $self->testException(
                $strFunction, ERROR_FILE_OWNER, "unable to set ownership for '${strFile}': Operation not permitted");
        }
        # Else bogus user
        elsif (defined($strUser) && $strUser eq BOGUS)
        {
            $self->testException($strFunction, ERROR_USER_MISSING, "user '" . BOGUS . "' does not exist");
        }
        # Else bogus group
        elsif (defined($strGroup) && $strGroup eq BOGUS)
        {
            $self->testException($strFunction, ERROR_GROUP_MISSING, "group '" . BOGUS . "' does not exist");
        }
        # Else success
        else
        {
            $self->testResult($strFunction, "0");
        }
    }
    }
    }
    }
    }
}

1;
