####################################################################################################################################
# Posix Driver Tests
####################################################################################################################################
package pgBackRestTest::Module::Storage::StoragePosixPerlTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(basename dirname);
use IO::Socket::UNIX;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Posix::Driver;

use pgBackRestTest::Common::ContainerTest;
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
    my $iFileLengthHalf = int($iFileLength / 2);

    # Test driver
    my $oPosix = new pgBackRest::Storage::Posix::Driver();

    ################################################################################################################################
    if ($self->begin('exists()'))
    {
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
    if ($self->begin("manifestList()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my @stryFile = ('.', 'test.txt');

        $self->testResult(
            sub {$oPosix->manifestList($self->testPath(), \@stryFile)},
            '{. => {group => ' . $self->group() . ', mode => 0770, type => d, user => ' . $self->pgUser() . '}}',
            'skip missing file');
    }

    ################################################################################################################################
    if ($self->begin("manifestStat()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strFile = $self->testPath() . '/test.txt';

        $self->testResult(sub {$oPosix->manifestStat($strFile)}, '[undef]', 'ignore missing file');

        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->put($strFile, "TEST");
        utime(1111111111, 1111111111, $strFile);
        executeTest('chmod 1640 ' . $strFile);

        $self->testResult(
            sub {$oPosix->manifestStat($strFile)},
            '{group => ' . $self->group() .
                ', mode => 1640, modification_time => 1111111111, size => 4, type => f, user => ' . $self->pgUser() . '}',
            'stat file');

        #---------------------------------------------------------------------------------------------------------------------------
        my $strSocketFile = $self->testPath() . '/test.socket';

        # Create a socket to test invalid files
        my $oSocket = IO::Socket::UNIX->new(Type => SOCK_STREAM(), Local => $strSocketFile, Listen => 1);

        $self->testException(
            sub {$oPosix->manifestStat($strSocketFile)}, ERROR_FILE_INVALID,
            "${strSocketFile} is not of type directory, file, or link");

        # Cleanup socket
        $oSocket->close();
        storageTest()->remove($strSocketFile);

        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestPath = $self->testPath() . '/public_dir';
        storageTest()->pathCreate($strTestPath, {strMode => '0750'});

        $self->testResult(
            sub {$oPosix->manifestStat($strTestPath)},
            '{group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}',
            'stat directory');

        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestLink = $self->testPath() . '/public_dir_link';

        symlink($strTestPath, $strTestLink)
            or confess &log(ERROR, "unable to create symlink from ${strTestPath} to ${strTestLink}");

        $self->testResult(
            sub {$oPosix->manifestStat($strTestLink)},
            '{group => ' . $self->group() . ", link_destination => ${strTestPath}, type => l, user => " . $self->pgUser() . '}',
            'stat link');
    }

    ################################################################################################################################
    if ($self->begin("manifestRecurse()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestPath = $self->testPath() . '/public_dir';
        my $strTestFile = "${strTestPath}/test.txt";

        $self->testException(
            sub {my $hManifest = {}; $oPosix->manifestRecurse($strTestFile, undef, 0, $hManifest); $hManifest},
            ERROR_FILE_MISSING, "unable to stat '${strTestFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->pathCreate($strTestPath, {strMode => '0750'});

        $self->testResult(
            sub {my $hManifest = {}; $oPosix->manifestRecurse($strTestPath, undef, 0, $hManifest); $hManifest},
            '{. => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}}',
            'empty directory manifest');

        #---------------------------------------------------------------------------------------------------------------------------
        storageTest()->put($strTestFile, "TEST");
        utime(1111111111, 1111111111, $strTestFile);
        executeTest('chmod 0750 ' . $strTestFile);

        storageTest()->pathCreate("${strTestPath}/sub", {strMode => '0750'});

        $self->testResult(
            sub {my $hManifest = {}; $oPosix->manifestRecurse(
                $self->testPath(), basename($strTestPath), 1, $hManifest); $hManifest},
            '{public_dir => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'public_dir/sub => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'public_dir/' . basename($strTestFile) . ' => {group => ' . $self->group() .
                ', mode => 0750, modification_time => 1111111111, size => 4, type => f, user => ' . $self->pgUser() . '}}',
            'directory and file manifest');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {my $hManifest = {}; $oPosix->manifestRecurse($strTestFile, undef, 0, $hManifest); $hManifest},
            '{' . basename($strTestFile) . ' => {group => ' . $self->group() .
                ', mode => 0750, modification_time => 1111111111, size => 4, type => f, user => ' . $self->pgUser() . '}}',
            'single file manifest');
    }

    ################################################################################################################################
    if ($self->begin("manifest()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strMissingFile = $self->testPath() . '/missing';

        $self->testException(
            sub {$oPosix->manifest($strMissingFile)},
            ERROR_FILE_MISSING, "unable to stat '${strMissingFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        # Setup test data
        executeTest('mkdir -m 750 ' . $self->testPath() . '/sub1');
        executeTest('mkdir -m 750 ' . $self->testPath() . '/sub1/sub2');

        executeTest("echo 'TESTDATA' > " . $self->testPath() . '/test.txt');
        utime(1111111111, 1111111111, $self->testPath() . '/test.txt');
        executeTest('chmod 1640 ' . $self->testPath() . '/test.txt');

        executeTest("echo 'TESTDATA_' > ". $self->testPath() . '/sub1/test-sub1.txt');
        utime(1111111112, 1111111112, $self->testPath() . '/sub1/test-sub1.txt');
        executeTest('chmod 0640 ' . $self->testPath() . '/sub1/test-sub1.txt');

        executeTest("echo 'TESTDATA__' > " . $self->testPath() . '/sub1/sub2/test-sub2.txt');
        utime(1111111113, 1111111113, $self->testPath() . '/sub1/sub2/test-sub2.txt');
        executeTest('chmod 0646 ' . $self->testPath() . '/sub1/test-sub1.txt');

        executeTest('ln ' . $self->testPath() . '/test.txt ' . $self->testPath() . '/sub1/test-hardlink.txt');
        executeTest('ln ' . $self->testPath() . '/test.txt ' . $self->testPath() . '/sub1/sub2/test-hardlink.txt');

        executeTest('ln -s .. ' . $self->testPath() . '/sub1/test');
        executeTest('chmod 0700 ' . $self->testPath() . '/sub1/test');
        executeTest('ln -s ../.. ' . $self->testPath() . '/sub1/sub2/test');
        executeTest('chmod 0750 ' . $self->testPath() . '/sub1/sub2/test');

        executeTest('chmod 0770 ' . $self->testPath());

        $self->testResult(
            sub {$oPosix->manifest($self->testPath())},
            '{. => {group => ' . $self->group() . ', mode => 0770, type => d, user => ' . $self->pgUser() . '}, ' .
            'sub1 => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'sub1/sub2 => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'sub1/sub2/test => {group => ' . $self->group() . ', link_destination => ../.., type => l, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/sub2/test-hardlink.txt => ' .
                '{group => ' . $self->group() . ', mode => 1640, modification_time => 1111111111, size => 9, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/sub2/test-sub2.txt => ' .
                '{group => ' . $self->group() . ', mode => 0666, modification_time => 1111111113, size => 11, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/test => {group => ' . $self->group() . ', link_destination => .., type => l, user => ' . $self->pgUser() . '}, ' .
            'sub1/test-hardlink.txt => ' .
                '{group => ' . $self->group() . ', mode => 1640, modification_time => 1111111111, size => 9, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/test-sub1.txt => ' .
                '{group => ' . $self->group() . ', mode => 0646, modification_time => 1111111112, size => 10, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'test.txt => ' .
                '{group => ' . $self->group() . ', mode => 1640, modification_time => 1111111111, size => 9, type => f, user => ' .
                $self->pgUser() . '}}',
            'complete manifest');
    }

    ################################################################################################################################
    if ($self->begin('openRead() & Posix::FileRead'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$oPosix->openRead($strFile)}, ERROR_FILE_MISSING, "unable to open '${strFile}': No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");

        $self->testResult(
            sub {$oPosix->openRead($strFile)}, '[object]', 'open read');
    }

    ################################################################################################################################
    if ($self->begin('openWrite() & Posix::FileWrite'))
    {
        my $tContent = $strFileContent;

        #---------------------------------------------------------------------------------------------------------------------------
        executeTest("echo -n '${strFileContent}' | tee ${strFile}");
        executeTest("chmod 600 ${strFile} && sudo chown root:root ${strFile}");

        $self->testException(
            sub {new pgBackRest::Storage::Posix::FileRead($oPosix, $strFile)}, ERROR_FILE_OPEN,
            "unable to open '${strFile}': Permission denied");

        executeTest("sudo rm -rf ${strFile}");

        #---------------------------------------------------------------------------------------------------------------------------
        my $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite($oPosix, $strFile)}, '[object]', 'open');

        $tContent = undef;
        $self->testException(
            sub {$oPosixIo->write(\$tContent)}, ERROR_FILE_WRITE, "unable to write to '${strFile}': Use of uninitialized value");

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
            sub {(new pgBackRest::Storage::Posix::FileRead($oPosix, $strFile))->read(\$tContent, $iFileLength)},
            $iFileLength, 'check write content length');
        $self->testResult($tContent, $strFileContent, 'check write content');

        #---------------------------------------------------------------------------------------------------------------------------
        $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite(
                $oPosix, "${strFile}.atomic", {bAtomic => true, strMode => '0666', lTimestamp => time(), bSync => false})},
            '[object]', 'open');

        $self->testResult(sub {$oPosixIo->write(\$tContent, $iFileLength)}, $iFileLength, 'write');
        $self->testResult(sub {$oPosixIo->close()}, true, 'close');

        $self->testResult(sub {${storageTest()->get("${strFile}.atomic")}}, $strFileContent, 'check content');

        #---------------------------------------------------------------------------------------------------------------------------
        $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite($oPosix, $strFile)}, '[object]', 'open');

        $self->testResult(sub {$oPosixIo->close()}, true, 'close');

        undef($oPosixIo);

        #---------------------------------------------------------------------------------------------------------------------------
        $oPosixIo = $self->testResult(
            sub {new pgBackRest::Storage::Posix::FileWrite($oPosix, $strFile, {lTimestamp => time()})}, '[object]', 'open');
        $self->testResult(sub {$oPosixIo->write(\$strFileContent, $iFileLength)}, $iFileLength, 'write');
        executeTest("rm -f $strFile");

        $self->testException(
            sub {$oPosixIo->close()}, ERROR_FILE_WRITE, "unable to set time for '${strFile}': No such file or directory");
    }

    ################################################################################################################################
    if ($self->begin('owner()'))
    {
        my $strFile = $self->testPath() . "/test.txt";

        $self->testException(
            sub {$oPosix->owner($strFile, {strUser => 'root'})}, ERROR_FILE_MISSING,
            "unable to stat '${strFile}': No such file or directory");

        executeTest("touch ${strFile}");

        $self->testException(
            sub {$oPosix->owner($strFile, {strUser => BOGUS})}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}' because user 'bogus' does not exist");
        $self->testException(
            sub {$oPosix->owner($strFile, {strGroup => BOGUS})}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}' because group 'bogus' does not exist");

        $self->testResult(sub {$oPosix->owner($strFile)}, undef, "no ownership changes");
        $self->testResult(sub {$oPosix->owner($strFile, {strUser => TEST_USER})}, undef, "same user");
        $self->testResult(sub {$oPosix->owner($strFile, {strGroup => TEST_GROUP})}, undef, "same group");
        $self->testResult(
            sub {$oPosix->owner($strFile, {strUser => TEST_USER, strGroup => TEST_GROUP})}, undef, "same user, group");

        $self->testException(
            sub {$oPosix->owner($strFile, {strUser => 'root'})}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}': Operation not permitted");
        $self->testException(
            sub {$oPosix->owner($strFile, {strGroup => 'root'})}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}': Operation not permitted");

        executeTest("sudo chown :root ${strFile}");
        $self->testResult(
            sub {$oPosix->owner($strFile, {strGroup => TEST_GROUP})}, undef, "change group back from root");
    }

    ################################################################################################################################
    if ($self->begin('pathCreate()'))
    {
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
