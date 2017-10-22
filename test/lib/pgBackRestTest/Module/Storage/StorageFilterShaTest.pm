####################################################################################################################################
# StorageFilterShaTest.pm - Tests for StorageFilterSha module.
####################################################################################################################################
package pgBackRestTest::Module::Storage::StorageFilterShaTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Digest::SHA qw(sha1_hex);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Filter::Sha;
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
    my $strFileContent = 'TESTDATA';
    my $iFileLength = length($strFileContent);
    my $oDriver = new pgBackRest::Storage::Posix::Driver();

    ################################################################################################################################
    if ($self->begin('read()'))
    {
        my $tBuffer;

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        my $oFileIo = $self->testResult(sub {$oDriver->openRead($strFile)}, '[object]', 'open read');
        my $oShaIo = $self->testResult(sub {new pgBackRest::Storage::Filter::Sha($oFileIo)}, '[object]', 'new read');

        $self->testResult(sub {$oShaIo->read(\$tBuffer, 2, undef)}, 2, 'read 2 bytes');
        $self->testResult(sub {$oShaIo->read(\$tBuffer, 2, 2)}, 2, 'read 2 bytes');
        $self->testResult(sub {$oShaIo->read(\$tBuffer, 2, 4)}, 2, 'read 2 bytes');
        $self->testResult(sub {$oShaIo->read(\$tBuffer, 2, 6)}, 2, 'read 2 bytes');
        $self->testResult(sub {$oShaIo->read(\$tBuffer, 2, 8)}, 0, 'read 0 bytes');

        $self->testResult(sub {$oShaIo->close()}, true, 'close');
        my $strSha = $self->testResult(
            sub {$oShaIo->result(STORAGE_FILTER_SHA)}, sha1_hex($strFileContent), 'check hash against original content');
        $self->testResult($strSha, sha1_hex($tBuffer), 'check hash against buffer');
        $self->testResult(sub {${storageTest()->get($strFile)}}, $strFileContent, 'check content');

        #---------------------------------------------------------------------------------------------------------------------------
        $tBuffer = undef;

        $oFileIo = $self->testResult(
            sub {$oDriver->openRead($self->dataPath() . '/filecopy.archive2.bin')}, '[object]', 'open read');
        $oShaIo = $self->testResult(sub {new pgBackRest::Storage::Filter::Sha($oFileIo)}, '[object]', 'new read');

        $self->testResult(sub {$oShaIo->read(\$tBuffer, 8388608)}, 8388608, '    read 8388608 bytes');
        $self->testResult(sub {$oShaIo->read(\$tBuffer, 4194304)}, 4194304, '    read 4194304 bytes');
        $self->testResult(sub {$oShaIo->read(\$tBuffer, 4194304)}, 4194304, '    read 4194304 bytes');
        $self->testResult(sub {$oShaIo->read(\$tBuffer, 1)}, 0, '    read 0 bytes');

        $self->testResult(sub {$oShaIo->close()}, true, '    close');
        $self->testResult(sub {$oShaIo->close()}, false, '    close again to make sure nothing bad happens');
        $self->testResult($oShaIo->result(STORAGE_FILTER_SHA), '1c7e00fd09b9dd11fc2966590b3e3274645dd031', '    check hash');
        $self->testResult(sha1_hex($tBuffer), '1c7e00fd09b9dd11fc2966590b3e3274645dd031', '    check content');
    }

    ################################################################################################################################
    if ($self->begin('write()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oFileIo = $self->testResult(sub {$oDriver->openWrite($strFile, {bAtomic => true})}, '[object]', 'open write');
        my $oShaIo = $self->testResult(
            sub {new pgBackRest::Storage::Filter::Sha($oFileIo)}, '[object]', 'new');

        my $tBuffer = substr($strFileContent, 0, 2);
        $self->testResult(sub {$oShaIo->write(\$tBuffer)}, 2, 'write 2 bytes');
        $tBuffer = substr($strFileContent, 2, 2);
        $self->testResult(sub {$oShaIo->write(\$tBuffer)}, 2, 'write 2 bytes');
        $tBuffer = substr($strFileContent, 4, 2);
        $self->testResult(sub {$oShaIo->write(\$tBuffer)}, 2, 'write 2 bytes');
        $tBuffer = substr($strFileContent, 6, 2);
        $self->testResult(sub {$oShaIo->write(\$tBuffer)}, 2, 'write 2 bytes');
        $tBuffer = '';
        $self->testResult(sub {$oShaIo->write(\$tBuffer)}, 0, 'write 0 bytes');

        $self->testResult(sub {$oShaIo->close()}, true, 'close');
        my $strSha = $self->testResult(
            sub {$oShaIo->result(STORAGE_FILTER_SHA)}, sha1_hex($strFileContent), 'check hash against original content');
        $self->testResult(sub {${storageTest()->get($strFile)}}, $strFileContent, 'check content');
    }
}

1;
