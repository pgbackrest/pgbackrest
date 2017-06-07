####################################################################################################################################
# StoragePosixFileTest.pm - tests for Storage::Posix::File module
####################################################################################################################################
package pgBackRestTest::Module::Storage::StoragePosixFileTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Posix::FileRead;
use pgBackRest::Storage::Posix::FileWrite;

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
    my $iFileLengthHalf = int($iFileLength / 2);

    ################################################################################################################################
    if ($self->begin('new()'))
    {
        my $oDriver = new pgBackRest::Storage::Posix::Driver();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {new pgBackRest::Storage::Posix::FileRead($oDriver, $strFile)}, ERROR_FILE_MISSING,
            "unable to open '${strFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileRead($oDriver, $strFile)}, '[object]', 'open read');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite($oDriver, $strFile)}, '[object]', 'open write');

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("chmod 600 ${strFile} && sudo chown root:root ${strFile}");

        $self->testException(
            sub {new pgBackRest::Storage::Posix::FileRead($oDriver, $strFile)}, ERROR_FILE_OPEN,
            "unable to open '${strFile}': Permission denied");
    }

    ################################################################################################################################
    if ($self->begin('write()'))
    {
        my $oDriver = new pgBackRest::Storage::Posix::Driver();
        my $tContent = $strFileContent;

        #---------------------------------------------------------------------------------------------------------------------------
        my $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite($oDriver, $strFile)}, '[object]', 'open');

        $tContent = substr($strFileContent, 0, $iFileLengthHalf);
        $self->testResult(
            sub {$oPosixIo->write(\$tContent)}, $iFileLengthHalf, 'write part 1');

        $tContent = substr($strFileContent, $iFileLengthHalf);
        $self->testResult(
            sub {$oPosixIo->write(\$tContent)}, $iFileLength - $iFileLengthHalf,
            'write part 2');
        $oPosixIo->close();

        $tContent = undef;
        $self->testResult(
            sub {(new pgBackRest::Storage::Posix::FileRead($oDriver, $strFile))->read(\$tContent, $iFileLength)},
            $iFileLength, 'check write content length');
        $self->testResult($tContent, $strFileContent, 'check write content');

        #---------------------------------------------------------------------------------------------------------------------------
        $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite(
                $oDriver, "${strFile}.atomic", {bAtomic => true, strMode => '0666', lTimestamp => time(), bSync => false})},
            '[object]', 'open');

        $self->testResult(sub {$oPosixIo->write(\$tContent, $iFileLength)}, $iFileLength, 'write');
        $self->testResult(sub {$oPosixIo->close()}, true, 'close');

        $self->testResult(sub {${storageTest()->get("${strFile}.atomic")}}, $strFileContent, 'check content');
    }

    ################################################################################################################################
    if ($self->begin('close()'))
    {
        my $oDriver = new pgBackRest::Storage::Posix::Driver();

        #---------------------------------------------------------------------------------------------------------------------------
        my $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite($oDriver, $strFile)}, '[object]', 'open');

        $self->testResult(sub {$oPosixIo->close()}, true, 'close');

        undef($oPosixIo);

        #---------------------------------------------------------------------------------------------------------------------------
        $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite($oDriver, $strFile, {lTimestamp => time()})}, '[object]', 'open');
        $self->testResult(sub {$oPosixIo->write(\$strFileContent, $iFileLength)}, $iFileLength, 'write');
        executeTest("rm -f $strFile");

        $self->testException(
            sub {$oPosixIo->close()}, ERROR_FILE_WRITE, "unable to set time for '${strFile}': No such file or directory");
    }
}

1;
