####################################################################################################################################
# FileStatTest.pm - Tests for FileCommon::fileStat
####################################################################################################################################
package pgBackRestTest::Module::File::FileStatTest;
use parent 'pgBackRestTest::Module::File::FileCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Fcntl qw(:mode);
use File::stat;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::FileCommon;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin("FileCommon::fileStat()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strFile = $self->testPath() . '/test.txt';

        $self->testException(sub {fileStat($strFile)}, ERROR_FILE_MISSING, "unable to stat ${strFile}: No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {fileStat($strFile, true)}, '[undef]', 'ignore missing file');

        #---------------------------------------------------------------------------------------------------------------------------
        fileStringWrite($strFile);

        $self->testResult(sub {fileStat($strFile)}, '[object]', 'stat file');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {S_ISREG(fileStat($strFile)->mode)}, true, 'check stat file result');

        #---------------------------------------------------------------------------------------------------------------------------
        my $strPrivateDir = $self->testPath() . '/private_dir';
        my $strPrivateFile = "${strPrivateDir}/test.txt";
        executeTest("sudo mkdir -m 700 ${strPrivateDir}");

        $self->testException(
            sub {fileStat($strPrivateFile, true)}, ERROR_FILE_OPEN, "unable to stat ${strPrivateFile}: Permission denied");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {S_ISDIR(fileStat($strPrivateDir)->mode)}, true, 'check stat directory result');
    }
}

1;
