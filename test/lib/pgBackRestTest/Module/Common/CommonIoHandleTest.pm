####################################################################################################################################
# CommonIoHandleTest.pm - tests for Common::Io::Handle module
####################################################################################################################################
package pgBackRestTest::Module::Common::CommonIoHandleTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Fcntl qw(O_RDONLY O_WRONLY O_CREAT O_TRUNC O_NONBLOCK);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Base;
use pgBackRest::Common::Io::Handle;
use pgBackRest::Common::Log;

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
    if ($self->begin('new() & handleRead() & handleReadSet() & handleWrite() & handleWriteSet()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle('test', undef, undef)}, '[object]', 'new - no handles');

        $self->testResult(sub {$oIoHandle->id()}, 'test', '    check error msg');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oIoHandle->handleReadSet(1)}, 1, '    set read handle');
        $self->testResult(sub {$oIoHandle->handleRead()}, 1, '    check read handle');
        $self->testResult(sub {$oIoHandle->handleWrite()}, undef, '    check write handle');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oIoHandle->handleWriteSet(2)}, 2, '  set write handle');
        $self->testResult(sub {$oIoHandle->handleRead()}, 1, '    check read handle');
        $self->testResult(sub {$oIoHandle->handleWrite()}, 2, '    check write handle');

        #---------------------------------------------------------------------------------------------------------------------------
        $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle('test', 1, undef)}, '[object]', 'new - read handle');

        $self->testResult(sub {$oIoHandle->handleRead()}, 1, '    check read handle');
        $self->testResult(sub {$oIoHandle->handleWrite()}, undef, '    check write handle');

        #---------------------------------------------------------------------------------------------------------------------------
        $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle('test', undef, 2)}, '[object]', 'new - write handle');

        $self->testResult(sub {$oIoHandle->handleRead()}, undef, '    check read handle');
        $self->testResult(sub {$oIoHandle->handleWrite()}, 2, '    check write handle');

        #---------------------------------------------------------------------------------------------------------------------------
        $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle('test', 1, 2)}, '[object]', 'new - read/write handle');

        $self->testResult(sub {$oIoHandle->handleRead()}, 1, '    check read handle');
        $self->testResult(sub {$oIoHandle->handleWrite()}, 2, '    check write handle');
    }

    ################################################################################################################################
    if ($self->begin('read()'))
    {
        my $tContent;
        my $fhRead;

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        sysopen($fhRead, $strFile, O_RDONLY)
            or confess &log(ERROR, "unable to open '${strFile}'");

        my $oIoHandle = $self->testResult(sub {new pgBackRest::Common::Io::Handle("'$strFile'", $fhRead)}, '[object]', 'open');
        $self->testException(
            sub {$oIoHandle->read(\$tContent, -1)}, ERROR_FILE_READ, "unable to read from '${strFile}': Negative length");

        #---------------------------------------------------------------------------------------------------------------------------
        $tContent = undef;

        sysopen($fhRead, $strFile, O_RDONLY)
            or confess &log(ERROR, "unable to open '${strFile}'");

        $oIoHandle = $self->testResult(sub {new pgBackRest::Common::Io::Handle("'$strFile'", $fhRead)}, '[object]', 'open');

        $self->testResult(sub {$oIoHandle->read(\$tContent, $iFileLengthHalf)}, $iFileLengthHalf, '    read part 1');
        $self->testResult($tContent, substr($strFileContent, 0, $iFileLengthHalf), '    check read');

        $self->testResult(sub {$oIoHandle->eof()}, false, '    not eof');

        $self->testResult(
            sub {$oIoHandle->read(
                \$tContent, $iFileLength - $iFileLengthHalf)}, $iFileLength - $iFileLengthHalf, '    read part 2');
        $self->testResult($tContent, $strFileContent, '    check read');

        $self->testResult(sub {$oIoHandle->read(\$tContent, 1)}, 0, '    zero read');
        $self->testResult(sub {$oIoHandle->eof()}, true, '    eof');
    }

    ################################################################################################################################
    if ($self->begin('write()'))
    {
        my $tContent;
        my $fhRead;
        my $fhWrite;

        #---------------------------------------------------------------------------------------------------------------------------
        sysopen($fhWrite, $strFile, O_WRONLY | O_CREAT | O_TRUNC)
            or confess &log(ERROR, "unable to open '${strFile}'");

        my $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle("'$strFile'", undef, $fhWrite)}, '[object]', 'open write');

        $self->testException(
            sub {$oIoHandle->write(undef)}, ERROR_FILE_WRITE,
            "unable to write to '${strFile}': Can't use an undefined value as a SCALAR reference");

        #---------------------------------------------------------------------------------------------------------------------------
        $tContent = substr($strFileContent, 0, $iFileLengthHalf);
        $self->testResult(sub {$oIoHandle->write(\$tContent)}, $iFileLengthHalf, '    write part 1');
        $self->testResult(sub {$oIoHandle->size()}, $iFileLengthHalf, '    check part 1 size');

        $tContent = substr($strFileContent, $iFileLengthHalf);
        $self->testResult(sub {$oIoHandle->write(\$tContent)}, $iFileLength - $iFileLengthHalf, '    write part 2');
        $self->testResult(sub {$oIoHandle->size()}, $iFileLength, '    check part 2 size');
        $self->testResult(sub {$oIoHandle->close()}, true, '   close');

        sysopen($fhRead, $strFile, O_RDONLY)
            or confess &log(ERROR, "unable to open '${strFile}'");

        $oIoHandle = $self->testResult(sub {new pgBackRest::Common::Io::Handle("'$strFile'", $fhRead)}, '[object]', 'open read');

        $tContent = undef;
        $self->testResult(sub {$oIoHandle->read(\$tContent, $iFileLength)}, $iFileLength, '    read');
        $self->testResult($tContent, $strFileContent, '    check write content');
    }

    ################################################################################################################################
    if ($self->begin('result() & resultSet()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle('test', undef, undef)}, '[object]', 'new - no handles');

        $self->testResult(sub {$oIoHandle->resultSet('Module::1', 1)}, 1, '    set int result');
        $self->testResult(sub {$oIoHandle->resultSet('Module::2', {value => 2})}, '{value => 2}', '    set hash result');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oIoHandle->result('Module::1')}, 1, '    check int result');
        $self->testResult(sub {$oIoHandle->result('Module::2')}, '{value => 2}', '    check hash result');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oIoHandle->{rhResult}}, '{Module::1 => 1, Module::2 => {value => 2}}', '    check all results');
    }

    ################################################################################################################################
    if ($self->begin('isA()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle('test', undef, undef)}, '[object]', 'new - no handles');

        $self->testResult(
            sub {$oIoHandle->isA()}, '(' . COMMON_IO_HANDLE . ', ' . COMMON_IO_BASE . ')', '    check isA');
    }

    ################################################################################################################################
    if ($self->begin('className()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $oIoHandle = $self->testResult(
            sub {new pgBackRest::Common::Io::Handle('test', undef, undef)}, '[object]', 'new - no handles');

        $self->testResult(sub {$oIoHandle->className()}, COMMON_IO_HANDLE, '    check class name');
    }

    ################################################################################################################################
    if ($self->begin('close()'))
    {
        my $tContent;
        my $fhRead;

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        sysopen($fhRead, $strFile, O_RDONLY)
            or confess &log(ERROR, "unable to open '${strFile}'");

        my $oIoHandle = $self->testResult(sub {new pgBackRest::Common::Io::Handle("'$strFile'", $fhRead)}, '[object]', 'open read');
        $self->testResult(sub {$oIoHandle->read(\$tContent, $iFileLengthHalf)}, $iFileLengthHalf, '    read');
        $self->testResult(sub {$oIoHandle->close()}, true, '    close');

        $self->testResult(sub {$oIoHandle->result(COMMON_IO_HANDLE)}, $iFileLengthHalf, '    check result');

        # Destroy and to make sure close runs again
        undef($oIoHandle);
    }
}

1;
