####################################################################################################################################
# S3 File Read Tests
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageS3FileReadTest;
use parent 'pgBackRestTest::Env::S3EnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);

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
    if ($self->begin('openRead()'))
    {
        executeTest("$self->{strS3Command} cp ${strRandomFile} s3://pgbackrest-dev${strFile}");

        #---------------------------------------------------------------------------------------------------------------------------
        my $tBuffer;
        my $oFileRead = $self->testResult(sub {$oS3->openRead($strFile)}, '[object]', 'open read');
        $self->testResult(sub {$oFileRead->read(\$tBuffer, 524288)}, 524288, '    read half');
        $self->testResult(sub {$oFileRead->read(\$tBuffer, 524288)}, 524288, '    read half');
        $self->testResult(sub {$oFileRead->read(\$tBuffer, 512)}, 0, '    read 0');
        $self->testResult(length($tBuffer), 1048576, '    check length');
        $self->testResult(sha1_hex($tBuffer), sha1_hex($strRandom), '    check hash');
    }
}

1;
