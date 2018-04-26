####################################################################################################################################
# Tests for Storage::Local module
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageLocalPerlTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);

use pgBackRest::Config::Config;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Filter::Sha;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Local;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule - common objects and variables used by all tests.
####################################################################################################################################
sub initModule
{
    my $self = shift;

    # Local path
    $self->{strPathLocal} = $self->testPath() . '/local';

    # Create the dynamic rule
    my $fnRule = sub
    {
        my $strRule = shift;
        my $strFile = shift;
        my $xData = shift;

        if ($strRule eq '<fn-rule-1>')
        {
            return "fn-rule-1/${xData}" . (defined($strFile) ? "/${strFile}" : '');
        }
        else
        {
            return 'fn-rule-2/' . (defined($strFile) ? "${strFile}/${strFile}" : 'no-file');
        }
    };

    # Create the rule hash
    my $hRule =
    {
        '<static-rule>' => 'static-rule-path',
        '<fn-rule-1>' =>
        {
            fnRule => $fnRule,
            xData => 'test',
        },
        '<fn-rule-2>' =>
        {
            fnRule => $fnRule,
        },
    };

    # Create local storage
    $self->{oStorageLocal} = new pgBackRest::Storage::Local(
        $self->pathLocal(), new pgBackRest::Storage::Posix::Driver(), {hRule => $hRule, bAllowTemp => false});

    # Create encrypted storage
    $self->{oStorageEncrypt} = new pgBackRest::Storage::Local(
        $self->testPath(), new pgBackRest::Storage::Posix::Driver(),
        {hRule => $hRule, bAllowTemp => false, strCipherType => CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC});

    # Remote path
    $self->{strPathRemote} = $self->testPath() . '/remote';

    # Create the repo path so the remote won't complain that it's missing
    mkdir($self->pathRemote())
        or confess &log(ERROR, "unable to create repo directory '" . $self->pathRemote() . qw{'});

    # Remove repo path now that the remote is created
    rmdir($self->{strPathRemote})
        or confess &log(ERROR, "unable to remove repo directory '" . $self->pathRemote() . qw{'});

    # Create remote storage
    $self->{oStorageRemote} = new pgBackRest::Storage::Local(
        $self->pathRemote(), new pgBackRest::Storage::Posix::Driver(), {hRule => $hRule});
}

####################################################################################################################################
# initTest - initialization before each test
####################################################################################################################################
sub initTest
{
    my $self = shift;

    executeTest(
        'ssh ' . $self->backrestUser() . '\@' . $self->host() . ' mkdir -m 700 ' . $self->pathRemote(), {bSuppressStdErr => true});

    executeTest('mkdir -m 700 ' . $self->pathLocal());
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Define test file
    my $strFile = 'file.txt';
    my $strFileCopy = 'file.txt.copy';
    my $strFileHash = 'bbbcf2c59433f68f22376cd2439d6cd309378df6';
    my $strFileContent = 'TESTDATA';
    my $iFileSize = length($strFileContent);

    ################################################################################################################################
    if ($self->begin("pathGet()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->pathGet('<static-rule>/test', {bTemp => true})},
            ERROR_ASSERT, "temp file not supported for storage '" . $self->storageLocal()->pathBase() . "'");
        $self->testException(
            sub {$self->storageRemote()->pathGet('<static-rule>', {bTemp => true})},
            ERROR_ASSERT, 'file part must be defined when temp file specified');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageRemote()->pathGet('/file', {bTemp => true})}, "/file.tmp", 'absolute path temp');
        $self->testResult(sub {$self->storageRemote()->pathGet('/file')}, "/file", 'absolute path file');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->pathGet('file')}, $self->storageLocal()->pathBase() . '/file', 'relative path');
        $self->testResult(
            sub {$self->storageRemote()->pathGet('file', {bTemp => true})},
                $self->storageRemote()->pathBase() . '/file.tmp', 'relative path temp');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->pathGet('<static-rule/file')}, ERROR_ASSERT, "found < but not > in '<static-rule/file'");

        $self->testException(
            sub {$self->storageLocal()->pathGet('<bogus-rule>')}, ERROR_ASSERT, "storage rule '<bogus-rule>' does not exist");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<static-rule>/file')},
            $self->storageLocal()->pathBase() . '/static-rule-path/file', 'static rule file');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<static-rule>')},
            $self->storageLocal()->pathBase() . '/static-rule-path', 'static rule path');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-1>/file')},
            $self->storageLocal()->pathBase() . '/fn-rule-1/test/file', 'function rule 1 file');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-2>/file')},
            $self->storageLocal()->pathBase() . '/fn-rule-2/file/file', 'function rule 2 file');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-1>')},
            $self->storageLocal()->pathBase() . '/fn-rule-1/test', 'function rule 1 path');
        $self->testResult(
            sub {$self->storageLocal()->pathGet('<fn-rule-2>')},
            $self->storageLocal()->pathBase() . '/fn-rule-2/no-file', 'function rule 2 no file');
    }

    ################################################################################################################################
    if ($self->begin('openWrite()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oFileIo = $self->testResult(sub {$self->storageLocal()->openWrite($strFile)}, '[object]', 'open write');

        $self->testResult(sub {$oFileIo->write(\$strFileContent)}, $iFileSize, "write $iFileSize bytes");
        $self->testResult(sub {$oFileIo->close()}, true, 'close');

        # Check that it is not encrypted
        $self->testResult(sub {$self->storageLocal()->encrypted($strFile)}, false, 'test storage not encrypted');
    }

    ################################################################################################################################
    if ($self->begin('put()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($self->storageLocal()->openWrite($strFile))}, 0, 'put empty');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($strFile)}, 0, 'put empty (all defaults)');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($self->storageLocal()->openWrite($strFile), $strFileContent)}, $iFileSize, 'put');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($self->storageLocal()->openWrite($strFile), \$strFileContent)}, $iFileSize,
            'put reference');
    }

    ################################################################################################################################
    if ($self->begin('openRead()'))
    {
        my $tContent;

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->openRead($strFile, {bIgnoreMissing => true})}, undef, 'ignore missing');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->openRead($strFile)}, ERROR_FILE_MISSING,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('sudo touch ' . $self->pathLocal() . "/${strFile} && sudo chmod 700 " . $self->pathLocal() . "/${strFile}");

        $self->testException(
            sub {$self->storageLocal()->openRead($strFile)}, ERROR_FILE_OPEN,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': Permission denied");

        executeTest('sudo rm ' . $self->pathLocal() . "/${strFile}");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($self->storageLocal()->openWrite($strFile), $strFileContent);

        my $oFileIo = $self->testResult(sub {$self->storageLocal()->openRead($strFile)}, '[object]', 'open read');

        $self->testResult(sub {$oFileIo->read(\$tContent, $iFileSize)}, $iFileSize, "read $iFileSize bytes");
        $self->testResult($tContent, $strFileContent, '    check read');

        #---------------------------------------------------------------------------------------------------------------------------
        $oFileIo = $self->testResult(
            sub {$self->storageLocal()->openRead($strFile, {rhyFilter => [{strClass => STORAGE_FILTER_SHA}]})}, '[object]',
            'open read + checksum');

        undef($tContent);
        $self->testResult(sub {$oFileIo->read(\$tContent, $iFileSize)}, $iFileSize, "read $iFileSize bytes");
        $self->testResult(sub {$oFileIo->close()}, true, 'close');
        $self->testResult($tContent, $strFileContent, '    check read');
        $self->testResult($oFileIo->result(STORAGE_FILTER_SHA), sha1_hex($strFileContent), '    check hash');
    }

    ################################################################################################################################
    if ($self->begin('get()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->get($self->storageLocal()->openRead($strFile, {bIgnoreMissing => true}))}, undef,
            'get missing');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($strFile);
        $self->testResult(sub {${$self->storageLocal()->get($strFile)}}, undef, 'get empty');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($strFile, $strFileContent);
        $self->testResult(sub {${$self->storageLocal()->get($strFile)}}, $strFileContent, 'get');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {${$self->storageLocal()->get($self->storageLocal()->openRead($strFile))}}, $strFileContent, 'get from io');
    }

    ################################################################################################################################
    if ($self->begin('hashSize()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->put($strFile, $strFileContent)}, 8, 'put');

        $self->testResult(
            sub {$self->storageLocal()->hashSize($strFile)},
            qw{(} . sha1_hex($strFileContent) . ', ' . $iFileSize . qw{)}, '    check hash/size');
    }

    ################################################################################################################################
    if ($self->begin('copy()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->copy($self->storageLocal()->openRead($strFile), $strFileCopy)}, ERROR_FILE_MISSING,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': No such file or directory");
        $self->testResult(
            sub {$self->storageLocal()->exists($strFileCopy)}, false, '   destination does not exist');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageLocal()->copy(
                $self->storageLocal()->openRead($strFile, {bIgnoreMissing => true}),
                $self->storageLocal()->openWrite($strFileCopy))},
            false, 'missing source io');
        $self->testResult(
            sub {$self->storageLocal()->exists($strFileCopy)}, false, '   destination does not exist');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->copy($self->storageLocal()->openRead($strFile), $strFileCopy)}, ERROR_FILE_MISSING,
            "unable to open '" . $self->storageLocal()->pathBase() . "/${strFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->put($strFile, $strFileContent);

        $self->testResult(sub {$self->storageLocal()->copy($strFile, $strFileCopy)}, true, 'copy filename->filename');
        $self->testResult(sub {${$self->storageLocal()->get($strFileCopy)}}, $strFileContent, '    check copy');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->remove($strFileCopy);

        $self->testResult(
            sub {$self->storageLocal()->copy($self->storageLocal()->openRead($strFile), $strFileCopy)}, true, 'copy io->filename');
        $self->testResult(sub {${$self->storageLocal()->get($strFileCopy)}}, $strFileContent, '    check copy');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->storageLocal()->remove($strFileCopy);

        $self->testResult(
            sub {$self->storageLocal()->copy(
                $self->storageLocal()->openRead($strFile), $self->storageLocal()->openWrite($strFileCopy))},
            true, 'copy io->io');
        $self->testResult(sub {${$self->storageLocal()->get($strFileCopy)}}, $strFileContent, '    check copy');
    }

    ################################################################################################################################
    if ($self->begin('info()'))
    {
        $self->testResult(sub {$self->storageLocal()->info($self->{strPathLocal})}, "[object]", 'stat dir successfully');

        $self->testException(sub {$self->storageLocal()->info($strFile)}, ERROR_FILE_MISSING,
            "unable to stat '". $self->{strPathLocal} . "/" . $strFile ."': No such file or directory");
    }

    ################################################################################################################################
    if ($self->begin('pathCreate()'))
    {
        my $strTestPath = $self->{strPathLocal} . "/" . BOGUS;

        $self->testResult(sub {$self->storageLocal()->pathCreate($strTestPath)}, "[undef]",
            "test creation of path " . $strTestPath);

        $self->testException(sub {$self->storageLocal()->pathCreate($strTestPath)}, ERROR_PATH_EXISTS,
            "unable to create path '". $strTestPath. "' because it already exists");

        $self->testResult(sub {$self->storageLocal()->pathCreate($strTestPath, {bIgnoreExists => true})}, "[undef]",
            "ignore path exists");
    }

    ################################################################################################################################
    if ($self->begin('encryption'))
    {
        my $strCipherPass = 'x';
        $self->testResult(sub {sha1_hex($strFileContent)}, $strFileHash, 'hash check contents to be written');

        # Error when passphrase not passed
        #---------------------------------------------------------------------------------------------------------------------------
        my $oFileIo = $self->testException(sub {$self->storageEncrypt()->openWrite($strFile)},
            ERROR_ASSERT, 'tCipherPass is required in Storage::Filter::CipherBlock->new');

        # Write an encrypted file
        #---------------------------------------------------------------------------------------------------------------------------
        $oFileIo = $self->testResult(sub {$self->storageEncrypt()->openWrite($strFile, {strCipherPass => $strCipherPass})},
            '[object]', 'open write');

        my $iWritten = $oFileIo->write(\$strFileContent);
        $self->testResult(sub {$oFileIo->close()}, true, '    close');

        # Check that it is encrypted and valid for the repo encryption type
        $self->testResult(sub {$self->storageEncrypt()->encryptionValid($self->storageEncrypt()->encrypted($strFile))}, true,
            '    test storage encrypted and valid');

        $self->testResult(
            sub {sha1_hex(${storageTest()->get($strFile)}) ne $strFileHash}, true, '    check written sha1 different');

        # Error when passphrase not passed
        #---------------------------------------------------------------------------------------------------------------------------
        $oFileIo = $self->testException(sub {$self->storageEncrypt()->openRead($strFile)},
            ERROR_ASSERT, 'tCipherPass is required in Storage::Filter::CipherBlock->new');

        # Read it and confirm it decrypts and is same as original content
        #---------------------------------------------------------------------------------------------------------------------------
        $oFileIo = $self->testResult(sub {$self->storageEncrypt()->openRead($strFile, {strCipherPass => $strCipherPass})},
            '[object]', 'open read and decrypt');
        my $strContent;
        $oFileIo->read(\$strContent, $iWritten);
        $self->testResult(sub {$oFileIo->close()}, true, '    close');
        $self->testResult($strContent, $strFileContent, '    decrypt read equal orginal contents');

        # Copy
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$self->storageEncrypt()->copy(
                $self->storageEncrypt()->openRead($strFile, {strCipherPass => $strCipherPass}),
                $self->storageEncrypt()->openWrite($strFileCopy, {strCipherPass => $strCipherPass}))},
            true, 'copy - decrypt/encrypt');

        $self->testResult(
            sub {sha1_hex(${$self->storageEncrypt()->get($strFileCopy, {strCipherPass => $strCipherPass})})}, $strFileHash,
                '    check decrypted copy file sha1 same as original plaintext file');

        # Write an empty encrypted file
        #---------------------------------------------------------------------------------------------------------------------------
        my $strFileZero = 'file-0.txt';
        my $strZeroContent = '';
        $oFileIo = $self->testResult(
            sub {$self->storageEncrypt()->openWrite($strFileZero, {strCipherPass => $strCipherPass})}, '[object]',
            'open write for zero');

        $self->testResult(sub {$oFileIo->write(\$strZeroContent)}, 0, '    zero written');
        $self->testResult(sub {$oFileIo->close()}, true, '    close');

        $self->testResult(sub {$self->storageEncrypt()->encrypted($strFile)}, true, '    test empty file encrypted');

        # Write an unencrypted file to the encrypted storage and check if the file is valid for that storage
        #---------------------------------------------------------------------------------------------------------------------------
        my $strFileTest = $self->testPath() . qw{/} . 'test.file.txt';

        # Create empty file
        executeTest("touch ${strFileTest}");
        $self->testResult(sub {$self->storageEncrypt()->encrypted($strFileTest)}, false, 'empty file so not encrypted');

        # Add unencrypted content to the file
        executeTest("echo -n '${strFileContent}' | tee ${strFileTest}");
        $self->testResult(sub {$self->storageEncrypt()->encryptionValid($self->storageEncrypt()->encrypted($strFileTest))}, false,
            'storage encryption and unencrypted file format do not match');

        # Unencrypted file valid in unencrypted storage
        $self->testResult(sub {$self->storageLocal()->encryptionValid($self->storageLocal()->encrypted($strFileTest))}, true,
            'unencrypted file valid in unencrypted storage');

        # Prepend encryption Magic Signature and test encrypted file in unencrypted storage not valid
        executeTest('echo "' . CIPHER_MAGIC . '$(cat ' . $strFileTest . ')" > ' . $strFileTest);
        $self->testResult(sub {$self->storageLocal()->encryptionValid($self->storageLocal()->encrypted($strFileTest))}, false,
            'storage unencrypted and encrypted file format do not match');

        # Test a file that does not exist
        #---------------------------------------------------------------------------------------------------------------------------
        $strFileTest = $self->testPath() . qw{/} . 'testfile';
        $self->testException(sub {$self->storageEncrypt()->encrypted($strFileTest)}, ERROR_FILE_MISSING,
            "unable to open '" . $strFileTest . "': No such file or directory");

        $self->testResult(sub {$self->storageEncrypt()->encrypted($strFileTest, {bIgnoreMissing => true})}, true,
            'encryption for ignore missing file returns encrypted for encrypted storage');

        $self->testResult(sub {$self->storageLocal()->encrypted($strFileTest, {bIgnoreMissing => true})}, false,
            'encryption for ignore missing file returns unencrypted for unencrypted storage');
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
sub host {return '127.0.0.1'}
sub pathLocal {return shift->{strPathLocal}};
sub pathRemote {return shift->{strPathRemote}};
sub protocolLocal {return shift->{oProtocolLocal}};
sub protocolRemote {return shift->{oProtocolRemote}};
sub storageLocal {return shift->{oStorageLocal}};
sub storageEncrypt {return shift->{oStorageEncrypt}};
sub storageRemote {return shift->{oStorageRemote}};

1;
