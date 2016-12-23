####################################################################################################################################
# FileManifestTest.pm - Tests for File->manifest()
####################################################################################################################################
package pgBackRestTest::File::FileManifestTest;
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

    my $strManifestCompare =
        '.,d,' . $self->pgUser() . ',' . $self->group() . ",0770,,,,\n" .
        'sub1,d,' . $self->pgUser() . ',' . $self->group() . ",0750,,,,\n" .
        'sub1/sub2,d,' . $self->pgUser() . ',' . $self->group() . ",0750,,,,\n" .
        'sub1/sub2/test,l,' . $self->pgUser() . ',' . $self->group() . ",,,,,../..\n" .
        'sub1/sub2/test-hardlink.txt,f,' . $self->pgUser() . ',' . $self->group() . ",1640,1111111111,0,9,\n" .
        'sub1/sub2/test-sub2.txt,f,' . $self->pgUser() . ',' . $self->group() . ",0666,1111111113,0,11,\n" .
        'sub1/test,l,' . $self->pgUser() . ',' . $self->group() . ",,,,,..\n" .
        'sub1/test-hardlink.txt,f,' . $self->pgUser() . ',' . $self->group() . ",1640,1111111111,0,9,\n" .
        'sub1/test-sub1.txt,f,' . $self->pgUser() . ',' . $self->group() . ",0646,1111111112,0,10,\n" .
        'test.txt,f,' . $self->pgUser() . ',' . $self->group() . ",1640,1111111111,0,9,";

    # Loop through local/remote
    for (my $bRemote = 0; $bRemote <= 1; $bRemote++)
    {
    for (my $bError = 0; $bError <= 1; $bError++)
    {
    for (my $bExists = 0; $bExists <= 1; $bExists++)
    {
        if (!$self->begin("rmt ${bRemote}, exists ${bExists}, err ${bError}")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup($bRemote, $bError);

        # Setup test data
        executeTest('mkdir -m 750 ' . $self->testPath() . '/sub1');
        executeTest('mkdir -m 750 ' . $self->testPath() . '/sub1/sub2');

        executeTest("echo 'TESTDATA' > " . $self->testPath() . '/test.txt');
        utime(1111111111, 1111111111, $self->testPath() . '/test.txt');
        executeTest('chmod 1640 ' . $self->testPath() . '/test.txt');

        executeTest("echo 'TESTDATA_' > ". $self->testPath() . '/sub1/test-sub1.txt');
        utime(1111111112, 1111111112, $self->testPath() . '/sub1/test-sub1.txt');
        executeTest('chmod 0640 ' . $self->testPath() . '/sub1/test-sub1.txt');

        executeTest("echo 'TESTDATA__' > " . $self->testPath() . '/sub1/sub2/test-sub2.txt');
        utime(1111111113, 1111111113, $self->testPath() . '/sub1/sub2/test-sub2.txt');
        executeTest('chmod 0646 ' . $self->testPath() . '/sub1/test-sub1.txt');

        executeTest('ln ' . $self->testPath() . '/test.txt ' . $self->testPath() . '/sub1/test-hardlink.txt');
        executeTest('ln ' . $self->testPath() . '/test.txt ' . $self->testPath() . '/sub1/sub2/test-hardlink.txt');

        executeTest('ln -s .. ' . $self->testPath() . '/sub1/test');
        executeTest('chmod 0700 ' . $self->testPath() . '/sub1/test');
        executeTest('ln -s ../.. ' . $self->testPath() . '/sub1/sub2/test');
        executeTest('chmod 0750 ' . $self->testPath() . '/sub1/sub2/test');

        executeTest('chmod 0770 ' . $self->testPath());

        # Create path
        my $strPath = $self->testPath();

        if ($bError)
        {
            $strPath = $self->testPath() . '/' . ($bRemote ? 'user' : 'backrest') . '_private';
        }
        elsif (!$bExists)
        {
            $strPath = $self->testPath() . '/error';
        }

        # Execute in eval in case of error
        my $hManifest;
        my $bErrorExpected = !$bExists || $bError;

        eval
        {
            $hManifest = $oFile->manifest(PATH_BACKUP_ABSOLUTE, $strPath);
            return true;
        }
        # Check for an error
        or do
        {
            if ($bErrorExpected)
            {
                next;
            }

            confess $EVAL_ERROR;
        };

        # Check for an expected error
        if ($bErrorExpected)
        {
            confess 'error was expected';
        }

        my $strManifest;

        # Validate the manifest
        foreach my $strName (sort(keys(%{$hManifest})))
        {
            if (!defined($strManifest))
            {
                $strManifest = '';
            }
            else
            {
                $strManifest .= "\n";
            }

            if (defined($hManifest->{$strName}{inode}))
            {
                $hManifest->{$strName}{inode} = 0;
            }

            $strManifest .=
                "${strName}," .
                $hManifest->{$strName}{type} . ',' .
                (defined($hManifest->{$strName}{user}) ?
                    $hManifest->{$strName}{user} : '') . ',' .
                (defined($hManifest->{$strName}{group}) ?
                    $hManifest->{$strName}{group} : '') . ',' .
                (defined($hManifest->{$strName}{mode}) ?
                    $hManifest->{$strName}{mode} : '') . ',' .
                (defined($hManifest->{$strName}{modification_time}) ?
                    $hManifest->{$strName}{modification_time} : '') . ',' .
                (defined($hManifest->{$strName}{inode}) ?
                    $hManifest->{$strName}{inode} : '') . ',' .
                (defined($hManifest->{$strName}{size}) ?
                    $hManifest->{$strName}{size} : '') . ',' .
                (defined($hManifest->{$strName}{link_destination}) ?
                    $hManifest->{$strName}{link_destination} : '');
        }

        if ($strManifest ne $strManifestCompare)
        {
            confess "manifest is not equal:\n\n${strManifest}\n\ncompare:\n\n${strManifestCompare}\n\n";
        }
    }
    }
    }
}

1;
