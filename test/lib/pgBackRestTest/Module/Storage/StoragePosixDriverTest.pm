####################################################################################################################################
# StoragePosixDriverTest.pm - Tests for Storage/Posix/Driver module.
####################################################################################################################################
package pgBackRestTest::Module::Storage::StoragePosixDriverTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
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
    my $strFile = $self->testPath() . '/file.txt';
    my $strFileContent = 'TESTDATA';
    my $iFileLength = length($strFileContent);

    ################################################################################################################################
    if ($self->begin('new()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {new pgBackRest::Storage::Posix::Driver()}, '[object]', 'new');
    }

    ################################################################################################################################
    if ($self->begin('openRead()'))
    {
        my $oPosix = new pgBackRest::Storage::Posix::Driver();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$oPosix->openRead($strFile)}, ERROR_FILE_MISSING, "unable to open '${strFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        $self->testResult(
            sub {$oPosix->openRead($strFile)}, '[object]', 'open read');
    }

    ################################################################################################################################
    if ($self->begin('openWrite()'))
    {
        my $oPosix = new pgBackRest::Storage::Posix::Driver();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oPosix->openWrite($strFile)}, '[object]', 'open write');
    }

    ################################################################################################################################
    if ($self->begin('exists()'))
    {
        my $oPosix = new pgBackRest::Storage::Posix::Driver();
        my $strPathSub = $self->testPath() . '/sub';

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oPosix->exists($strFile)}, false, 'file');

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("sudo mkdir ${strPathSub} && sudo chmod 700 ${strPathSub}");

        $self->testResult(
            sub {$oPosix->pathExists($strPathSub)}, true, 'path');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$oPosix->exists("${strPathSub}/file")}, ERROR_FILE_EXISTS,
            "unable to test if file '${strPathSub}/file' exists: Permission denied");
    }

    ################################################################################################################################
    if ($self->begin('pathCreate()'))
    {
        my $oPosix = new pgBackRest::Storage::Posix::Driver();
        my $strPathParent = $self->testPath() . '/parent';
        my $strPathSub = "${strPathParent}/sub1/sub2";

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oPosix->pathCreate($strPathParent)}, undef, 'parent path');
        $self->testResult(
            sub {$oPosix->pathExists($strPathParent)}, true, '    check path');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$oPosix->pathCreate($strPathParent)}, ERROR_PATH_EXISTS,
            "unable to create path '${strPathParent}' because it already exists");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oPosix->pathCreate($strPathParent, {bIgnoreExists => true})}, undef, 'path already exists');

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("sudo chown root:root ${strPathParent} && sudo chmod 700 ${strPathParent}");

        $self->testException(
            sub {$oPosix->pathCreate($strPathSub)}, ERROR_PATH_CREATE,
            "unable to create path '${strPathSub}': Permission denied");

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("rmdir ${strPathParent}");

        $self->testException(
            sub {$oPosix->pathCreate($strPathSub)}, ERROR_PATH_MISSING,
            "unable to create path '${strPathSub}' because parent does not exist");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oPosix->pathCreate($strPathSub, {bCreateParent => true})}, undef, 'path with parents');
        $self->testResult(
            sub {$oPosix->pathExists($strPathSub)}, true, '    check path');
    }

    ################################################################################################################################
    if ($self->begin('move()'))
    {
        my $oPosix = new pgBackRest::Storage::Posix::Driver();
        my $strFileCopy = "${strFile}.copy";
        my $strFileSub = $self->testPath() . '/sub/file.txt';

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$oPosix->move($strFile, $strFileCopy)}, ERROR_FILE_MISSING,
            "unable to move '${strFile}' because it is missing");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {storageTest()->put($strFile, $strFileContent)}, $iFileLength, 'put');
        $self->testResult(
            sub {$oPosix->move($strFile, $strFileCopy)}, undef, 'simple move');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$oPosix->move($strFileCopy, $strFileSub)}, ERROR_PATH_MISSING,
            "unable to move '${strFileCopy}' to missing path '" . dirname($strFileSub) . "'");

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest('sudo mkdir ' . dirname($strFileSub) . ' && sudo chmod 700 ' . dirname($strFileSub));

        $self->testException(
            sub {$oPosix->move($strFileCopy, $strFileSub)}, ERROR_FILE_MOVE,
            "unable to move '${strFileCopy}' to '${strFileSub}': Permission denied");

        executeTest('sudo rmdir ' . dirname($strFileSub));

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oPosix->move($strFileCopy, $strFileSub, {bCreatePath => true})}, undef, 'create parent path');
    }
}

1;
