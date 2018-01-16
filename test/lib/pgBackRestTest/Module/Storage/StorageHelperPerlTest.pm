####################################################################################################################################
# StorageHelperTest.pm - Tests for Storage::Helper module.
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageHelperPerlTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Storable qw(dclone);

use pgBackRest::Config::Config;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Env::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initTest - initialization before each test
####################################################################################################################################
sub initTest
{
    my $self = shift;

    storageTest()->pathCreate('db');
    storageTest()->pathCreate('repo');
    storageTest()->pathCreate('spool');
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
    my $strFileContent = 'TESTDATA';
    my $iFileSize = length($strFileContent);

    # Setup parameters
    $self->optionTestSet(CFGOPT_DB_PATH, $self->testPath() . '/db');
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
    $self->optionTestSet(CFGOPT_SPOOL_PATH, $self->testPath() . '/spool');
    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSetBool(CFGOPT_ARCHIVE_ASYNC, true);

    $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

    #-------------------------------------------------------------------------------------------------------------------------------
    if ($self->begin("storageLocal()"))
    {
        $self->testResult(sub {storageLocal($self->testPath())->put($strFile, $strFileContent)}, $iFileSize, 'put');
        $self->testResult(sub {${storageTest()->get($strFile)}}, $strFileContent, '    check put');

        $self->testResult(sub {storageLocal($self->testPath())->put($strFile, $strFileContent)}, $iFileSize, 'put cache storage');
        $self->testResult(sub {${storageTest()->get($strFile)}}, $strFileContent, '    check put');
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    if ($self->begin("storageDb()"))
    {
        $self->testResult(sub {storageDb()->put($strFile, $strFileContent)}, $iFileSize, 'put');
        $self->testResult(sub {${storageTest()->get("db/${strFile}")}}, $strFileContent, '    check put');

        $self->testResult(sub {storageDb()->put($strFileCopy, $strFileContent)}, $iFileSize, 'put cached storage');
        $self->testResult(sub {${storageTest()->get("db/${strFileCopy}")}}, $strFileContent, '    check put');
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    if ($self->begin("storageRepo()"))
    {
        $self->testResult(sub {storageRepo()->put($strFile, $strFileContent)}, $iFileSize, 'put');
        $self->testResult(sub {${storageTest()->get("repo/${strFile}")}}, $strFileContent, '    check put');

        $self->testResult(sub {storageRepo()->put($strFileCopy, $strFileContent)}, $iFileSize, 'put cached storage');
        $self->testResult(sub {${storageTest()->get("repo/${strFileCopy}")}}, $strFileContent, '    check put');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageRepo()->pathGet(STORAGE_REPO_ARCHIVE)}, $self->testPath() . '/repo/archive/db', 'check archive path');
        $self->testResult(
            sub {storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . '/9.3-1/000000010000000100000001')},
            $self->testPath() . '/repo/archive/db/9.3-1/0000000100000001/000000010000000100000001', 'check repo WAL file');
        $self->testResult(
            sub {storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . '/9.3-1/0000000100000001')},
            $self->testPath() . '/repo/archive/db/9.3-1/0000000100000001', 'check repo WAL path');
        $self->testResult(
            sub {storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . '/9.3-1/0000000100000001')},
            $self->testPath() . '/repo/archive/db/9.3-1/0000000100000001', 'check repo WAL major path');
        $self->testResult(
            sub {storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . '/9.3-1')},
            $self->testPath() . '/repo/archive/db/9.3-1', 'check repo archive id path');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageRepo()->pathGet(STORAGE_REPO_BACKUP)}, $self->testPath() . '/repo/backup/db', 'check backup path');
        $self->testResult(
            sub {storageRepo()->pathGet(STORAGE_REPO_BACKUP . '/file')}, $self->testPath() . '/repo/backup/db/file',
            'check backup file');

        #---------------------------------------------------------------------------------------------------------------------------
        # Insert a bogus rule to generate an error
        storageRepo()->{hRule}{'<BOGUS>'} =
        {
            fnRule => storageRepo()->{hRule}{&STORAGE_REPO_ARCHIVE}{fnRule},
        };

        $self->testException(sub {storageRepo()->pathGet('<BOGUS>')}, ERROR_ASSERT, 'invalid <REPO> storage rule <BOGUS>');
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    if ($self->begin("storageSpool()"))
    {
        $self->testResult(sub {storageSpool()->put($strFile, $strFileContent)}, $iFileSize, 'put');
        $self->testResult(sub {${storageTest()->get("spool/${strFile}")}}, $strFileContent, '    check put');

        $self->testResult(sub {storageSpool()->put($strFileCopy, $strFileContent)}, $iFileSize, 'put cached storage');
        $self->testResult(sub {${storageTest()->get("spool/${strFileCopy}")}}, $strFileContent, '    check put');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {storageSpool()->pathGet(STORAGE_SPOOL_ARCHIVE_OUT)}, $self->testPath() . '/spool/archive/db/out',
            'check archive out path');
    }

    #-------------------------------------------------------------------------------------------------------------------------------
    if ($self->begin("storageRepo() encryption"))
    {
        my $strStanzaEncrypt = 'test-encrypt';
        $self->optionTestSet(CFGOPT_REPO_CIPHER_TYPE, CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        # Encryption passphrase required when encryption type not 'none' (default)
        $self->testException(sub {storageRepo({strStanza => $strStanzaEncrypt})}, ERROR_ASSERT, 'option ' .
            cfgOptionName(CFGOPT_REPO_CIPHER_PASS) . ' is required');

        # Set the encryption passphrase and confirm passphrase and type have been set in the storage object
        $self->optionTestSet(CFGOPT_REPO_CIPHER_PASS, 'x');
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        $self->testResult(sub {storageRepo({strStanza => $strStanzaEncrypt})->cipherType() eq
            CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC}, true, 'encryption type set');
        $self->testResult(sub {storageRepo({strStanza => $strStanzaEncrypt})->cipherPassUser() eq 'x'}, true,
            'encryption passphrase set');

        # Cannot change encryption after it has been set (cached values not reset)
        $self->optionTestClear(CFGOPT_REPO_CIPHER_TYPE);
        $self->optionTestClear(CFGOPT_REPO_CIPHER_PASS);
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        $self->testResult(sub {storageRepo({strStanza => $strStanzaEncrypt})->cipherType() eq
            CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC}, true, 'encryption type not reset');
        $self->testResult(sub {storageRepo({strStanza => $strStanzaEncrypt})->cipherPassUser() eq 'x'}, true,
            'encryption passphrase not reset');
    }
}

1;
