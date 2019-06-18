####################################################################################################################################
# Tests for Storage::Local module
####################################################################################################################################
package pgBackRestTest::Module::Storage::StoragePerlTest;
use parent 'pgBackRestTest::Common::RunTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Config::Config;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::LibC qw(:crypto);
use pgBackRest::Storage::Base;

use pgBackRestTest::Common::ContainerTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Define test file
    my $strFile = $self->testPath() . '/file.txt';
    my $strFileCopy = $self->testPath() . '/file.txt.copy';
    my $strFileHash = 'bbbcf2c59433f68f22376cd2439d6cd309378df6';
    my $strFileContent = 'TESTDATA';
    my $iFileSize = length($strFileContent);

    # Create local storage
    $self->{oStorageLocal} = new pgBackRest::Storage::Storage('<LOCAL>');

    ################################################################################################################################
    if ($self->begin("pathGet()"))
    {
        $self->testResult(sub {$self->storageLocal()->pathGet('file')}, '/file', 'relative path');
        $self->testResult(sub {$self->storageLocal()->pathGet('/file2')}, '/file2', 'absolute path');
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
    if ($self->begin('get()'))
    {
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
            qw{(} . cryptoHashOne('sha1', $strFileContent) . ', ' . $iFileSize . qw{)}, '    check hash/size');
        $self->testResult(
            sub {$self->storageLocal()->hashSize(BOGUS, {bIgnoreMissing => true})}, "([undef], [undef])",
            '    check missing hash/size');
    }

    ################################################################################################################################
    if ($self->begin('copy()'))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->copy($self->storageLocal()->openRead($strFile), $strFileCopy)}, ERROR_FILE_MISSING,
            "unable to open missing file '${strFile}' for read");
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
            "unable to open missing file '${strFile}' for read");

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
    if ($self->begin('exists()'))
    {
        $self->storageLocal()->put($self->testPath() . "/test.file");

        $self->testResult(sub {$self->storageLocal()->exists($self->testPath() . "/test.file")}, true, 'existing file');
        $self->testResult(sub {$self->storageLocal()->exists($self->testPath() . "/test.missing")}, false, 'missing file');
        $self->testResult(sub {$self->storageLocal()->exists($self->testPath())}, false, 'path');
    }

    ################################################################################################################################
    if ($self->begin('info()'))
    {
        $self->testResult(
            sub {$self->storageLocal()->info($self->testPath())},
            "{group => " . $self->group() . ", mode => 0770, type => d, user => " . $self->pgUser() . "}",
            'stat dir successfully');

        $self->testException(sub {$self->storageLocal()->info(BOGUS)}, ERROR_FILE_OPEN,
            "unable to get info for missing path/file '/bogus'");
    }

    ################################################################################################################################
    if ($self->begin("manifest() and list()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->manifest($self->testPath() . '/missing')},
            ERROR_PATH_MISSING, "unable to list file info for missing path '" . $self->testPath() . "/missing'");

        #---------------------------------------------------------------------------------------------------------------------------
        # Setup test data
        executeTest('mkdir -m 750 ' . $self->testPath() . '/sub1');
        executeTest('mkdir -m 750 ' . $self->testPath() . '/sub1/sub2');
        executeTest('mkdir -m 750 ' . $self->testPath() . '/sub2');

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
            sub {$self->storageLocal()->manifest($self->testPath())},
            '{. => {group => ' . $self->group() . ', mode => 0770, type => d, user => ' . $self->pgUser() . '}, ' .
            'sub1 => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'sub1/sub2 => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'sub1/sub2/test => {group => ' . $self->group() . ', link_destination => ../.., type => l, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/sub2/test-hardlink.txt => ' .
                '{group => ' . $self->group() . ', mode => 0640, modification_time => 1111111111, size => 9, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/sub2/test-sub2.txt => ' .
                '{group => ' . $self->group() . ', mode => 0666, modification_time => 1111111113, size => 11, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/test => {group => ' . $self->group() . ', link_destination => .., type => l, user => ' . $self->pgUser() . '}, ' .
            'sub1/test-hardlink.txt => ' .
                '{group => ' . $self->group() . ', mode => 0640, modification_time => 1111111111, size => 9, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'sub1/test-sub1.txt => ' .
                '{group => ' . $self->group() . ', mode => 0646, modification_time => 1111111112, size => 10, type => f, user => ' .
                $self->pgUser() . '}, ' .
            'sub2 => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'test.txt => ' .
                '{group => ' . $self->group() . ', mode => 0640, modification_time => 1111111111, size => 9, type => f, user => ' .
                $self->pgUser() . '}}',
            'complete manifest');

        $self->testResult(sub {$self->storageLocal()->list($self->testPath())}, "(sub1, sub2, test.txt)", "list");
        $self->testResult(sub {$self->storageLocal()->list($self->testPath(), {strExpression => "2\$"})}, "sub2", "list");
        $self->testResult(
            sub {$self->storageLocal()->list($self->testPath(), {strSortOrder => 'reverse'})}, "(test.txt, sub2, sub1)",
            "list reverse");
        $self->testResult(sub {$self->storageLocal()->list($self->testPath() . "/sub2")}, "[undef]", "list empty");
        $self->testResult(
            sub {$self->storageLocal()->list($self->testPath() . "/sub99", {bIgnoreMissing => true})}, "[undef]", "list missing");
        $self->testException(
            sub {$self->storageLocal()->list($self->testPath() . "/sub99")}, ERROR_PATH_MISSING,
            "unable to list files for missing path '" . $self->testPath() . "/sub99'");
    }

    ################################################################################################################################
    if ($self->begin('move()'))
    {
        my $strFileCopy = "${strFile}.copy";
        my $strFileSub = $self->testPath() . '/sub/file.txt';

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {$self->storageLocal()->move($strFile, $strFileCopy)}, ERROR_FILE_MOVE,
            "unable to move '${strFile}' to '${strFile}.copy': No such file or directory");
    }

    ################################################################################################################################
    if ($self->begin('owner()'))
    {
        my $strFile = $self->testPath() . "/test.txt";

        $self->testException(
            sub {$self->storageLocal()->owner($strFile, 'root')}, ERROR_FILE_MISSING,
            "unable to stat '${strFile}': No such file or directory");

        executeTest("touch ${strFile}");

        $self->testException(
            sub {$self->storageLocal()->owner($strFile, BOGUS)}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}' because user 'bogus' does not exist");
        $self->testException(
            sub {$self->storageLocal()->owner($strFile, undef, BOGUS)}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}' because group 'bogus' does not exist");

        $self->testResult(sub {$self->storageLocal()->owner($strFile)}, undef, "no ownership changes");
        $self->testResult(sub {$self->storageLocal()->owner($strFile, TEST_USER)}, undef, "same user");
        $self->testResult(sub {$self->storageLocal()->owner($strFile, undef, TEST_GROUP)}, undef, "same group");
        $self->testResult(
            sub {$self->storageLocal()->owner($strFile, TEST_USER, TEST_GROUP)}, undef,
            "same user, group");

        $self->testException(
            sub {$self->storageLocal()->owner($strFile, 'root', undef)}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}': Operation not permitted");
        $self->testException(
            sub {$self->storageLocal()->owner($strFile, undef, 'root')}, ERROR_FILE_OWNER,
            "unable to set ownership for '${strFile}': Operation not permitted");

        executeTest("sudo chown :root ${strFile}");
        $self->testResult(
            sub {$self->storageLocal()->owner($strFile, undef, TEST_GROUP)}, undef, "change group back from root");
    }

    ################################################################################################################################
    if ($self->begin('pathCreate()'))
    {
        my $strTestPath = $self->testPath() . "/" . BOGUS;

        $self->testResult(sub {$self->storageLocal()->pathCreate($strTestPath)}, "[undef]",
            "test creation of path " . $strTestPath);

        $self->testException(sub {$self->storageLocal()->pathCreate($strTestPath)}, ERROR_PATH_CREATE,
            "unable to create path '". $strTestPath. "'");

        $self->testResult(sub {$self->storageLocal()->pathCreate($strTestPath, {bIgnoreExists => true})}, "[undef]",
            "ignore path exists");
    }

    ################################################################################################################################
    if ($self->begin('pathExists()'))
    {
        $self->storageLocal()->put($self->testPath() . "/test.file");

        $self->testResult(sub {$self->storageLocal()->pathExists($self->testPath() . "/test.file")}, false, 'existing file');
        $self->testResult(sub {$self->storageLocal()->pathExists($self->testPath() . "/test.missing")}, false, 'missing file');
        $self->testResult(sub {$self->storageLocal()->pathExists($self->testPath())}, true, 'path');
    }

    ################################################################################################################################
    if ($self->begin('pathSync()'))
    {
        $self->testResult(sub {$self->storageLocal()->pathSync($self->testPath())}, "[undef]", "test path sync");
    }
}

####################################################################################################################################
# Getters
####################################################################################################################################
# sub host {return '127.0.0.1'}
# sub pathLocal {return shift->{strPathLocal}};
# sub pathRemote {return shift->{strPathRemote}};
sub storageLocal {return shift->{oStorageLocal}};
# sub storageEncrypt {return shift->{oStorageEncrypt}};
# sub storageRemote {return shift->{oStorageRemote}};

1;
