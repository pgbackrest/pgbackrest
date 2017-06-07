####################################################################################################################################
# StorageS3FileWriteTest.pm - S3 Storage Tests
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageS3FileWriteTest;
use parent 'pgBackRestTest::Env::S3EnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Storage::S3::Driver;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Initialize the driver
    my $oS3 = $self->initS3();

    # Test variables
    my $strFile = '/path/to/file.txt';
    my $strFileContent = 'TESTDATA';
    my $iFileLength = length($strFileContent);

    # Create a random 1mb file
    my $strRandomFile = $self->testPath() . '/random1mb.bin';
    executeTest("dd if=/dev/urandom of=${strRandomFile} bs=1024k count=1", {bSuppressStdErr => true});
    my $strRandom = ${storageTest()->get($strRandomFile)};

    ################################################################################################################################
    if ($self->begin('openWrite()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oFileWrite = $self->testResult(sub {$oS3->openWrite($strFile)}, '[object]', 'open write');
        $self->testResult(sub {$oFileWrite->close()}, true, '    close without writing');

        #---------------------------------------------------------------------------------------------------------------------------
        $oFileWrite = $self->testResult(sub {$oS3->openWrite($strFile)}, '[object]', 'open write');
        $self->testResult(sub {$oFileWrite->write()}, 0, '    write undef');
        $self->testResult(sub {$oFileWrite->write(\$strFileContent)}, $iFileLength, '    write');
        $self->testResult(sub {$oFileWrite->close()}, true, '    close');

        #---------------------------------------------------------------------------------------------------------------------------
        $oFileWrite = $self->testResult(sub {$oS3->openWrite($strFile)}, '[object]', 'open write');

        for (my $iIndex = 1; $iIndex <= 17; $iIndex++)
        {
            $self->testResult(sub {$oFileWrite->write(\$strRandom)}, 1024 * 1024, '    write 1mb');
        }

        $self->testResult(sub {$oFileWrite->close()}, true, '    close');
    }
}

1;
