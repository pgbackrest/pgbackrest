####################################################################################################################################
# Tests for Storage::Filter::Gzip module
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageFilterGzipPerlTest;
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
use pgBackRest::Storage::Posix::Driver;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

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
    my $oDriver = new pgBackRest::Storage::Posix::Driver();

    ################################################################################################################################
    if ($self->begin('errorCheck()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($oDriver->openWrite($strFileGz))}, '[object]', 'new write');

        $oGzipIo->{bWrite} = true;
        $self->testException(sub {$oGzipIo->errorCheck(Z_DATA_ERROR)}, ERROR_FILE_WRITE, "unable to deflate '${strFileGz}'");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oGzipIo->errorCheck(Z_OK)}, Z_OK, 'Z_OK');
        $self->testResult(sub {$oGzipIo->errorCheck(Z_BUF_ERROR)}, Z_OK, 'Z_BUF_ERROR');

        #---------------------------------------------------------------------------------------------------------------------------
        $oGzipIo->{bWrite} = false;
        $oGzipIo->{strCompressType} = STORAGE_DECOMPRESS;
        $self->testException(sub {$oGzipIo->errorCheck(Z_DATA_ERROR)}, ERROR_FILE_READ, "unable to inflate '${strFileGz}'");
    }

    ################################################################################################################################
    if ($self->begin('write()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($oDriver->openWrite($strFileGz), {lCompressBufferMax => 4})},
            '[object]', 'new write compress');

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

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("gzip ${strFile}");
        my $tFile = ${storageTest()->get($strFileGz)};

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip(
                $oDriver->openWrite($strFile), {strCompressType => STORAGE_DECOMPRESS})}, '[object]', 'new write decompress');

        $tBuffer = substr($tFile, 0, 10);
        $self->testResult(sub {$oGzipIo->write(\$tBuffer)}, 10, '     write bytes');
        $tBuffer = substr($tFile, 10);
        $self->testResult(sub {$oGzipIo->write(\$tBuffer)}, length($tFile) - 10, '     write bytes');
        $self->testResult(sub {$oGzipIo->close()}, true, '    close');

        $self->testResult(sub {${storageTest()->get($strFile)}}, $strFileContent, '    check content');

    }

    ################################################################################################################################
    if ($self->begin('read()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        my $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($oDriver->openWrite($strFileGz), {bWantGzip => false, iLevel => 3})},
            '[object]', 'new write compress');
        $self->testResult($oGzipIo->{iLevel}, 3, '    check level');
        $self->testResult(sub {$oGzipIo->write(\$strFileContent, $iFileLength)}, $iFileLength, '    write');
        $self->testResult(sub {$oGzipIo->close()}, true, '    close');

        #---------------------------------------------------------------------------------------------------------------------------
        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip(
                $oDriver->openRead($strFileGz), {bWantGzip => false, strCompressType => STORAGE_DECOMPRESS})},
            '[object]', 'new read decompress');

        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 4)}, 4, '    read 4 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 2, '    read 2 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 2, '    read 2 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 0, '    read 0 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 0, '    read 0 bytes');

        $self->testResult(sub {$oGzipIo->close()}, true, '    close');
        $self->testResult(sub {$oGzipIo->close()}, false, '    close again');
        $self->testResult($tBuffer, $strFileContent, '    check content');

        storageTest()->remove($strFileGz);

        #---------------------------------------------------------------------------------------------------------------------------
        $tBuffer = 'AA';
        storageTest()->put($strFile, $strFileContent);

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($oDriver->openRead($strFile))},
            '[object]', 'new read compress');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 10, '    read 10 bytes (request 2)');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 18, '    read 18 bytes (request 2)');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2)}, 0, '    read 0 bytes (request 2)');
        $self->testResult(sub {$oGzipIo->close()}, true, '    close');

        $self->testResult(sub {storageTest()->put($strFileGz, substr($tBuffer, 2))}, 28, '    put content');
        executeTest("gzip -df ${strFileGz}");
        $self->testResult(sub {${storageTest()->get($strFile)}}, $strFileContent, '    check content');

        #---------------------------------------------------------------------------------------------------------------------------
        $tBuffer = undef;

        executeTest('cat ' . $self->dataPath() . "/filecopy.archive2.bin | gzip -c > ${strFileGz}");

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip(
                $oDriver->openRead($strFileGz), {lCompressBufferMax => 4096, strCompressType => STORAGE_DECOMPRESS})},
            '[object]', 'new read decompress');

        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 8388608)}, 8388608, '    read 8388608 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 4194304)}, 4194304, '    read 4194304 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 4194304)}, 4194304, '    read 4194304 bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 1)}, 0, '    read 0 bytes');

        $self->testResult(sub {$oGzipIo->close()}, true, '    close');
        $self->testResult(sha1_hex($tBuffer), '1c7e00fd09b9dd11fc2966590b3e3274645dd031', '    check content');

        storageTest()->remove($strFileGz);

        #---------------------------------------------------------------------------------------------------------------------------
        $tBuffer = undef;

        executeTest('cp ' . $self->dataPath() . "/filecopy.archive2.bin ${strFile}");

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($oDriver->openRead($strFile), {strCompressType => STORAGE_COMPRESS})},
            '[object]', 'new read compress');

        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2000000) > 0}, true, '    read bytes');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2000000) > 0}, true, '    read bytes');
        $self->testResult(sub {$oGzipIo->close()}, true, '    close');

        $self->testResult(sub {storageTest()->put($strFileGz, $tBuffer) > 0}, true, '    put content');
        executeTest("gzip -df ${strFileGz}");
        $self->testResult(
            sub {sha1_hex(${storageTest()->get($strFile)})}, '1c7e00fd09b9dd11fc2966590b3e3274645dd031', '    check content');

        #---------------------------------------------------------------------------------------------------------------------------
        $tBuffer = undef;

        my $oFile = $self->testResult(
            sub {storageTest()->openWrite($strFile)}, '[object]', 'open file to extend during compression');

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($oDriver->openRead($strFile), {lCompressBufferMax => 4194304})},
            '[object]', '    new read compress');

        $self->testResult(sub {$oFile->write(\$strFileContent)}, length($strFileContent), '   write first block');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2000000) > 0}, true, '    read compressed first block (compression done)');

        $self->testResult(sub {$oFile->write(\$strFileContent)}, length($strFileContent), '   write second block');
        $self->testResult(sub {$oGzipIo->read(\$tBuffer, 2000000)}, 0, '    read compressed  = 0');

        $self->testResult(sub {storageTest()->put($strFileGz, $tBuffer) > 0}, true, '    put content');
        executeTest("gzip -df ${strFileGz}");
        $self->testResult(
            sub {${storageTest()->get($strFile)}}, $strFileContent, '    check content');

        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->put($strFileGz, $strFileContent);

        $oGzipIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Gzip($oDriver->openRead($strFileGz), {strCompressType => STORAGE_DECOMPRESS})},
            '[object]', 'new read decompress');

        $self->testException(
            sub {$oGzipIo->read(\$tBuffer, 1)}, ERROR_FILE_READ, "unable to inflate '${strFileGz}': incorrect header check");
    }
}

1;
