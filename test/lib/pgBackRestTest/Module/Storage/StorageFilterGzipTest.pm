####################################################################################################################################
# StorageFilterGzipTest.pm - Tests for Storage::Filter::Gzip module.
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageFilterGzipTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Compress::Raw::Zlib qw(Z_OK Z_BUF_ERROR Z_DATA_ERROR);
use Digest::SHA qw(sha1_hex);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::Gzip;
use pgBackRest::Storage::Posix::File;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# posixIo - get an instance of the posix io object
####################################################################################################################################
sub posixIo
{
    my $self = shift;
    my $strFile = shift;
    my $strType = shift;

    return new pgBackRest::Storage::Posix::File(new pgBackRest::Storage::Posix::Driver(), $strFile, $strType);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Test data
    my $strFile = $self->testPath() . qw{/} . 'file.txt';
    my $strFileGz = "${strFile}.gz";
    my $strFileContent = 'TESTDATA';
    my $iFileLength = length($strFileContent);

    ################################################################################################################################
    if ($self->begin('errorCheck()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($self->posixIo($strFileGz, STORAGE_FILE_WRITE))}, '[object]', 'new write');

        $self->testException(sub {$oGzipIo->errorCheck(Z_DATA_ERROR)}, ERROR_FILE_WRITE, "unable to deflate '${strFileGz}'");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oGzipIo->errorCheck(Z_OK)}, Z_OK, 'Z_OK');
        $self->testResult(sub {$oGzipIo->errorCheck(Z_BUF_ERROR)}, Z_OK, 'Z_BUF_ERROR');

        #---------------------------------------------------------------------------------------------------------------------------
        $oGzipIo->{strType} = STORAGE_FILE_READ;

        $self->testException(sub {$oGzipIo->errorCheck(Z_DATA_ERROR)}, ERROR_FILE_READ, "unable to inflate '${strFileGz}'");
    }

    ################################################################################################################################
    if ($self->begin('write()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($self->posixIo($strFileGz, STORAGE_FILE_WRITE))}, '[object]', 'new write');

        my $tBuffer = substr($strFileContent, 0, 2);
        $self->testResult(sub {$oGzipIo->write(\$tBuffer)}, 2, '    write 2 bytes');
        $tBuffer = substr($strFileContent, 2, 2);
        $self->testResult(sub {$oGzipIo->write(\$tBuffer)}, 2, '     write 2 bytes');
        $tBuffer = substr($strFileContent, 4, 2);
        $self->testResult(sub {$oGzipIo->write(\$tBuffer)}, 2, '     write 2 bytes');
        $tBuffer = substr($strFileContent, 6, 2);
        $self->testResult(sub {$oGzipIo->write(\$tBuffer)}, 2, '     write 2 bytes');
        $tBuffer = '';
        $self->testResult(sub {$oGzipIo->write(\$tBuffer)}, 0, '     write 0 bytes');

        $self->testResult(sub {$oGzipIo->close()}, true, '    close');

        executeTest("gzip -d ${strFileGz}");
        $self->testResult(sub {${storageTest()->get($strFile)}}, $strFileContent, '    check content');
    }

    ################################################################################################################################
    if ($self->begin('read()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        my $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($self->posixIo($strFileGz, STORAGE_FILE_WRITE), {bWantGzip => false})},
            '[object]', 'new write');
        $self->testResult(sub {$oGzipIo->write(\$strFileContent, $iFileLength)}, $iFileLength, '    write');
        $self->testResult(sub {$oGzipIo->close()}, true, '    close');

        #---------------------------------------------------------------------------------------------------------------------------
        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($self->posixIo($strFileGz, STORAGE_FILE_READ), {bWantGzip => false})},
            '[object]', 'new read');

        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 4)}, 4, '    read 4 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 2, '    read 2 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 2, '    read 2 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 0, '    read 0 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 0, '    read 0 bytes');

        $self->testResult(sub {$oGzipIo->close()}, true, '    close');
        $self->testResult($tBuffer, $strFileContent, '    check content');

        storageTest()->remove($strFileGz);

        #---------------------------------------------------------------------------------------------------------------------------
        $tBuffer = undef;

        executeTest('cat ' . $self->dataPath() . "/filecopy.archive2.bin | gzip -c > ${strFileGz}");

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($self->posixIo($strFileGz, STORAGE_FILE_READ), {lBufferMax => 4096})},
            '[object]', 'new read');

        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 8388608)}, 8388608, '    read 8388608 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 4194304)}, 4194304, '    read 4194304 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 4194304)}, 4194304, '    read 4194304 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 1)}, 0, '    read 0 bytes');

        $self->testResult(sub {$oGzipIo->close()}, true, '    close');
        $self->testResult(sha1_hex($tBuffer), '1c7e00fd09b9dd11fc2966590b3e3274645dd031', '    check content');

        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->put($strFileGz, $strFileContent);

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($self->posixIo($strFileGz, STORAGE_FILE_READ))},
            '[object]', 'new read');

        $self->testException(
            sub {$oGzipIo->read(\$tBuffer, 1)}, ERROR_FILE_READ, "unable to inflate '${strFileGz}': incorrect header check");
    }
}

1;
