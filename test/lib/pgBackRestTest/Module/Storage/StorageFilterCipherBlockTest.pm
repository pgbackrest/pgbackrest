####################################################################################################################################
# Tests for Block Cipher
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageFilterCipherBlockTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Fcntl qw(O_RDONLY);
use Digest::SHA qw(sha1_hex);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::LibC qw(:random);
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::CipherBlock;
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
    my $strFileEncrypt = $self->testPath() . qw{/} . 'file.enc.txt';
    my $strFileDecrypt = $self->testPath() . qw{/} . 'file.dcr.txt';
    my $strFileBin = $self->testPath() . qw{/} . 'file.bin';
    my $strFileBinEncrypt = $self->testPath() . qw{/} . 'file.enc.bin';
    my $strFileContent = 'TESTDATA';
    my $iFileLength = length($strFileContent);
    my $oDriver = new pgBackRest::Storage::Posix::Driver();
    my $tCipherKey = 'areallybadkey';
    my $strCipherType = 'aes-256-cbc';
    my $tContent;

    ################################################################################################################################
    if ($self->begin('new()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        # Create an unencrypted file
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        $self->testException(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openRead($strFile), $strCipherType, $tCipherKey, {strMode => BOGUS})},
                ERROR_ASSERT, 'unknown cipher mode: ' . BOGUS);

        $self->testException(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openRead($strFile), $strCipherType, $tCipherKey, {strMode => BOGUS})},
                ERROR_ASSERT, 'unknown cipher mode: ' . BOGUS);

        $self->testException(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openRead($strFile), BOGUS, $tCipherKey)},
                ERROR_ASSERT, "unable to load cipher '" . BOGUS . "'");

        $self->testException(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openWrite($strFile), $strCipherType, $tCipherKey, {strMode => BOGUS})},
                ERROR_ASSERT, 'unknown cipher mode: ' . BOGUS);

        $self->testException(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openWrite($strFile), BOGUS, $tCipherKey)},
            ERROR_ASSERT, "unable to load cipher '" . BOGUS . "'");
    }

    ################################################################################################################################
    if ($self->begin('read() and write()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        # Create an plaintext file
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        # Instantiate the cipher object - default action encrypt
        my $oEncryptIo = $self->testResult(sub {new pgBackRest::Storage::Filter::CipherBlock($oDriver->openRead($strFile),
            $strCipherType, $tCipherKey)}, '[object]', 'new encrypt file');

        $self->testResult(sub {$oEncryptIo->read(\$tBuffer, 2)}, 16, '    read 16 bytes (header)');
        $self->testResult(sub {$oEncryptIo->read(\$tBuffer, 2)}, 16, '    read 16 bytes (data)');
        $self->testResult(sub {$oEncryptIo->read(\$tBuffer, 2)},  0, '    read 0 bytes');

        $self->testResult(sub {$tBuffer ne $strFileContent}, true, '    data read is encrypted');

        $self->testResult(sub {$oEncryptIo->close()}, true, '    close');
        $self->testResult(sub {$oEncryptIo->close()}, false, '    close again');

        # tBuffer is now encrypted - test write decrypts correctly
        my $oDecryptFileIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::CipherBlock($oDriver->openWrite($strFileDecrypt),
                $strCipherType, $tCipherKey, {strMode => STORAGE_DECRYPT})},
            '[object]', '    new decrypt file');

        $self->testResult(sub {$oDecryptFileIo->write(\$tBuffer)}, 32, '    write decrypted');
        $self->testResult(sub {$oDecryptFileIo->close()}, true, '    close');

        $self->testResult(sub {${$self->storageTest()->get($strFileDecrypt)}}, $strFileContent, '    data written is decrypted');

        #---------------------------------------------------------------------------------------------------------------------------
        $tBuffer = $strFileContent;
        my $oEncryptFileIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::CipherBlock($oDriver->openWrite($strFileEncrypt),
                $strCipherType, $tCipherKey)},
            '[object]', 'new write encrypt');

        $tContent = '';
        $self->testResult(sub {$oEncryptFileIo->write(\$tContent)}, 0, '    attempt empty buffer write');

        undef($tContent);
        $self->testException(
            sub {$oEncryptFileIo->write(\$tContent)}, ERROR_FILE_WRITE,
            "unable to write to '${strFileEncrypt}': Use of uninitialized value");

        # Encrypted length is not known so use tBuffer then test that tBuffer was encrypted
        my $iWritten = $self->testResult(sub {$oEncryptFileIo->write(\$tBuffer)}, length($tBuffer), '    write encrypted');
        $self->testResult(sub {$oEncryptFileIo->close()}, true, '    close');

        $tContent = $self->storageTest()->get($strFileDecrypt);
        $self->testResult(sub {defined($tContent) && $tContent ne $strFileContent}, true, '    data written is encrypted');

        #---------------------------------------------------------------------------------------------------------------------------
        undef($tBuffer);
        # Open encrypted file for decrypting
        $oEncryptFileIo =
            $self->testResult(
                sub {new pgBackRest::Storage::Filter::CipherBlock(
                    $oDriver->openRead($strFileEncrypt), $strCipherType, $tCipherKey,
                    {strMode => STORAGE_DECRYPT})},
                '[object]', 'new read encrypted file, decrypt');

        # Try to read more than the length of the data expected to be output from the decrypt and confirm the decrypted length is
        # the same as the original decrypted content.
        $self->testResult(sub {$oEncryptFileIo->read(\$tBuffer, $iFileLength+4)}, $iFileLength, '    read all bytes');

        # Just because length is the same does not mean content is so confirm
        $self->testResult($tBuffer, $strFileContent, '    data read is decrypted');
        $self->testResult(sub {$oEncryptFileIo->close()}, true, '    close');

        #---------------------------------------------------------------------------------------------------------------------------
        undef($tContent);
        undef($tBuffer);
        my $strFileBinHash = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';

        executeTest('cp ' . $self->dataPath() . "/filecopy.archive2.bin ${strFileBin}");
        $self->testResult(
            sub {sha1_hex(${storageTest()->get($strFileBin)})}, $strFileBinHash, 'bin test - check sha1');

        $tContent = ${storageTest()->get($strFileBin)};

        $oEncryptFileIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openWrite($strFileBinEncrypt), $strCipherType, $tCipherKey)},
            '[object]', '    new write encrypt');

        $self->testResult(sub {$oEncryptFileIo->write(\$tContent)}, length($tContent), '    write encrypted');
        $self->testResult(sub {$oEncryptFileIo->close()}, true, '    close');
        $self->testResult(
            sub {sha1_hex(${storageTest()->get($strFileBinEncrypt)}) ne $strFileBinHash}, true, '    check sha1 different');

        my $oEncryptBinFileIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openRead($strFileBinEncrypt), $strCipherType, $tCipherKey,
                {strMode => STORAGE_DECRYPT})},
            '[object]', 'new read encrypted bin file');

        $self->testResult(sub {$oEncryptBinFileIo->read(\$tBuffer, 16777216)}, 16777216, '    read 16777216 bytes');
        $self->testResult(sub {sha1_hex($tBuffer)}, $strFileBinHash, '    check sha1 same as original');
        $self->testResult(sub {$oEncryptBinFileIo->close()}, true, '    close');

        #---------------------------------------------------------------------------------------------------------------------------
        undef($tBuffer);

        $self->storageTest()->put($strFile, $strFileContent);

        executeTest(
            "openssl enc -k ${tCipherKey} -md sha1 -aes-256-cbc -in ${strFile} -out ${strFileEncrypt}");

        $oEncryptFileIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openRead($strFileEncrypt), $strCipherType, $tCipherKey,
                {strMode => STORAGE_DECRYPT})},
            '[object]', 'read file encrypted by openssl');

        $self->testResult(sub {$oEncryptFileIo->read(\$tBuffer, 16)}, 8, '    read 8 bytes');
        $self->testResult(sub {$oEncryptFileIo->close()}, true, '    close');
        $self->testResult(sub {$tBuffer}, $strFileContent, '    check content same as original');

        $self->storageTest()->remove($strFile);
        $self->storageTest()->remove($strFileEncrypt);

        $oEncryptFileIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::CipherBlock(
                $oDriver->openWrite($strFileEncrypt), $strCipherType, $tCipherKey)},
            '[object]', 'write file to be read by openssl');

        $self->testResult(sub {$oEncryptFileIo->write(\$tBuffer)}, 8, '    write 8 bytes');
        $self->testResult(sub {$oEncryptFileIo->close()}, true, '    close');

        executeTest(
            "openssl enc -d -k ${tCipherKey} -md sha1 -aes-256-cbc -in ${strFileEncrypt} -out ${strFile}");

        $self->testResult(sub {${$self->storageTest()->get($strFile)}}, $strFileContent, '    check content same as original');
    }
}

1;
