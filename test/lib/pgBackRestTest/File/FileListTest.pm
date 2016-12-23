####################################################################################################################################
# FileListTest.pm - Tests for File->list()
####################################################################################################################################
package pgBackRestTest::File::FileListTest;
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
    # Loop through sort
    foreach my $strSort ('reverse', undef)
    {
    # Loop through expression
    foreach my $strExpression (undef, "^test2\\..*\$", "^du\$")
    {
    # Loop through exists
    foreach my $bExists (false, true)
    {
    # Loop through ignore missing
    for (my $bIgnoreMissing = false; $bIgnoreMissing <= $bExists; $bIgnoreMissing++)
    {
    # Loop through error
    foreach my $bError (false, true)
    {
        if (!$self->begin(
            "rmt ${bRemote}, err ${bError}, exist ${bExists}, ignmis ${bIgnoreMissing}, " .
            'exp ' . (defined($strExpression) ? $strExpression : '[undef]') . ', ' .
            'srt ' . (defined($strSort) ? $strSort : '[undef]'))) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        my $strPath = $self->testPath();

        if ($bError)
        {
            $strPath = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . '_private';
        }
        elsif (!$bExists)
        {
            $strPath = $self->testPath() . '/error';
        }
        else
        {
            executeTest("echo 'TESTDATA' > ${strPath}/test.txt");
            executeTest("echo 'TESTDATA2' > ${strPath}/test2.txt");
        }

        my @stryFileCompare = split(/\n/, "test.txt\ntest2.txt");

        # Execute in eval in case of error
        my @stryFileList;
        my $bErrorExpected = (!$bExists && !$bIgnoreMissing) || $bError;

        eval
        {
            @stryFileList = $oFile->list(PATH_BACKUP_ABSOLUTE, $strPath, $strExpression, $strSort, $bIgnoreMissing);
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

        # Validate the list
        if (defined($strExpression))
        {
            @stryFileCompare = grep(/$strExpression/i, @stryFileCompare);
        }

        if (defined($strSort))
        {
            @stryFileCompare = sort {$b cmp $a} @stryFileCompare;
        }

        my $strFileList = sprintf("@stryFileList");
        my $strFileCompare = sprintf("@stryFileCompare");

        if ($strFileList ne $strFileCompare)
        {
            confess "list (${strFileList})[" . @stryFileList .
                    "] does not match compare (${strFileCompare})[" . @stryFileCompare . ']';
        }
    }
    }
    }
    }
    }
    }
}

1;
