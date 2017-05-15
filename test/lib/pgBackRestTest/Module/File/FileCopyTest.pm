####################################################################################################################################
# FileCopyTest.pm - Tests for File->copy()
####################################################################################################################################
package pgBackRestTest::Module::File::FileCopyTest;
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

    # Loop through small/large
    for (my $iLarge = 0; $iLarge <= 3; $iLarge++)
    {
    # Loop through possible remotes
    foreach my $strRemote ($iLarge ? (undef, 'db') : (undef, 'backup', 'db'))
    {
    # Loop through source path types
    foreach my $strSourcePathType ($iLarge ? (PATH_BACKUP_ABSOLUTE) : (PATH_BACKUP_ABSOLUTE, PATH_DB_ABSOLUTE))
    {
    # Loop through destination path types
    foreach my $strDestinationPathType ($iLarge ? (PATH_DB_ABSOLUTE) : (PATH_BACKUP_ABSOLUTE, PATH_DB_ABSOLUTE))
    {
    # Loop through source missing/present
    foreach my $bSourceMissing ($iLarge ? (false) : (false, true))
    {
    # Loop through source ignore/require
    foreach my $bSourceIgnoreMissing ($bSourceMissing ? (false, true) : (false))
    {
    # Loop through checksum append
    foreach my $bChecksumAppend ($bSourceMissing || $iLarge ? (false) : (false, true))
    {
    # Loop through source compression
    foreach my $bSourceCompressed ($bSourceMissing ? (false) : (false, true))
    {
    # Loop through destination compression
    foreach my $bDestinationCompress ($bSourceMissing ? (false) : (false, true))
    {
        my $strSourcePath = $strSourcePathType eq PATH_DB_ABSOLUTE ? 'db' : 'backup';
        my $strDestinationPath = $strDestinationPathType eq PATH_DB_ABSOLUTE ? 'db' : 'backup';

        if (!$self->begin(
            "lrg ${iLarge}, rmt " .
            (defined($strRemote) && ($strRemote eq $strSourcePath || $strRemote eq $strDestinationPath) ? 1 : 0) .
            ', srcpth ' . (defined($strRemote) && $strRemote eq $strSourcePath ? 'rmt' : 'lcl') .
            ":${strSourcePath}, srcmiss ${bSourceMissing}, srcignmiss ${bSourceIgnoreMissing}, srccmp $bSourceCompressed, " .
            'dstpth ' . (defined($strRemote) && $strRemote eq $strDestinationPath ? 'rmt' : 'lcl') .
            ":${strDestinationPath}, chkapp ${bChecksumAppend}, dstcmp $bDestinationCompress")) {next}

        # Setup test directory and get file object
        my $oFile = $self->setup(defined($strRemote), false);
        executeTest('mkdir ' . $self->testPath() . '/backup');
        executeTest('mkdir ' . $self->testPath() . '/db');

        my $strSourceFile = $self->testPath() . "/${strSourcePath}/test-source";
        my $strDestinationFile = $self->testPath() . "/${strDestinationPath}/test-destination";

        my $strCopyHash;
        my $iCopySize;

        # Create the compressed or uncompressed test file
        my $strSourceHash;
        my $iSourceSize;

        if (!$bSourceMissing)
        {
            if ($iLarge)
            {
                $strSourceFile .= '.bin';
                $strDestinationFile .= '.bin';

                if ($iLarge < 3)
                {
                    executeTest('cp ' . $self->dataPath() . "/filecopy.archive${iLarge}.bin ${strSourceFile}");
                }
                else
                {
                    for (my $iTableSizeIdx = 0; $iTableSizeIdx < 25; $iTableSizeIdx++)
                    {
                        executeTest('cat ' . $self->dataPath() . "/filecopy.table.bin >> ${strSourceFile}");
                    }
                }
            }
            else
            {
                $strSourceFile .= '.txt';
                $strDestinationFile .= '.txt';

                system("echo 'TESTDATA' > ${strSourceFile}");
            }

            if ($iLarge == 1)
            {
                $strSourceHash = '4518a0fdf41d796760b384a358270d4682589820';
                $iSourceSize = 16777216;
            }
            elsif ($iLarge == 2)
            {
                $strSourceHash = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
                $iSourceSize = 16777216;
            }
            elsif ($iLarge == 3)
            {
                $strSourceHash = 'b2055a6ba15bf44359c18fbbf23c68b50a670eb0';
                $iSourceSize = 26214400;
            }
            else
            {
                $strSourceHash = '06364afe79d801433188262478a76d19777ef351';
                $iSourceSize = 9;
            }

            if ($bSourceCompressed)
            {
                system("gzip ${strSourceFile}");
                $strSourceFile .= '.gz';
            }
        }

        if ($bDestinationCompress)
        {
            $strDestinationFile .= '.gz';
        }

        # Run file copy in an eval block because some errors are expected
        my $bReturn;

        eval
        {
            ($bReturn, $strCopyHash, $iCopySize) =
                $oFile->copy($strSourcePathType, $strSourceFile,
                             $strDestinationPathType, $strDestinationFile,
                             $bSourceCompressed, $bDestinationCompress,
                             $bSourceIgnoreMissing, undef, '0770', false, undef, undef,
                             $bChecksumAppend);

            return true;
        }
        # Check for errors after copy
        or do
        {
            my $oException = $EVAL_ERROR;

            if (isException($oException))
            {
                if ($bSourceMissing && !$bSourceIgnoreMissing)
                {
                    next;
                }

                confess $oException;
            }

            confess $oException;
        };

        if ($bSourceMissing)
        {
            if ($bSourceIgnoreMissing)
            {
                 if ($bReturn)
                 {
                     confess 'copy() returned ' . $bReturn .  ' when ignore missing set';
                 }

                 next;
            }

            confess 'expected source file missing error';
        }

        if (!defined($strCopyHash))
        {
            confess 'copy hash must be defined';
        }

        if ($bChecksumAppend)
        {
            if ($bDestinationCompress)
            {
                $strDestinationFile =
                    substr($strDestinationFile, 0, length($strDestinationFile) -3) . "-${strSourceHash}.gz";
            }
            else
            {
                $strDestinationFile .= '-' . $strSourceHash;
            }
        }

        unless (-e $strDestinationFile)
        {
            confess "could not find destination file ${strDestinationFile}";
        }

        my $strDestinationTest = $strDestinationFile;

        if ($bDestinationCompress)
        {
            $strDestinationTest = substr($strDestinationFile, 0, length($strDestinationFile) - 3) . '.test';

            system("gzip -dc ${strDestinationFile} > ${strDestinationTest}") == 0
                or die "could not decompress ${strDestinationFile}";
        }

        my ($strDestinationHash, $iDestinationSize) = $oFile->hashSize(PATH_ABSOLUTE, $strDestinationTest);

        if ($strSourceHash ne $strDestinationHash || $strSourceHash ne $strCopyHash)
        {
            confess
                "source ${strSourceHash}, copy ${strCopyHash} and destination ${strDestinationHash} file hashes do not match";
        }

        if ($iSourceSize != $iDestinationSize || $iSourceSize != $iCopySize)
        {
            confess "source ${iSourceSize}, copy ${iCopySize} and destination ${iDestinationSize} sizes do not match";
        }
    }
    }
    }
    }
    }
    }
    }
    }
    }
}

1;
