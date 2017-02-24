####################################################################################################################################
# FileManifestTest.pm - Tests for File->manifest()
####################################################################################################################################
package pgBackRestTest::File::FileManifestTest;
use parent 'pgBackRestTest::File::FileCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(basename dirname);
use IO::Socket::UNIX;

use pgBackRest::Common::Log;
use pgBackRest::Common::Exception;
use pgBackRest::File;
use pgBackRest::FileCommon;

use pgBackRestTest::Common::ExecuteTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    ################################################################################################################################
    if ($self->begin("FileCommon::fileManifestStat()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strFile = $self->testPath() . '/test.txt';

        $self->testResult(sub {pgBackRest::FileCommon::fileManifestStat($strFile)}, '[undef]', 'ignore missing file');

        #---------------------------------------------------------------------------------------------------------------------------
        fileStringWrite($strFile, "TEST");
        utime(1111111111, 1111111111, $strFile);
        executeTest('chmod 1640 ' . $strFile);

        $self->testResult(
            sub {pgBackRest::FileCommon::fileManifestStat($strFile)},
            '{group => ' . $self->group() .
                ', mode => 1640, modification_time => 1111111111, size => 4, type => f, user => ' . $self->pgUser() . '}',
            'stat file');

        #---------------------------------------------------------------------------------------------------------------------------
        my $strSocketFile = $self->testPath() . '/test.socket';

        # Create a socket to test invalid files
        my $oSocket = IO::Socket::UNIX->new(Type => SOCK_STREAM(), Local => $strSocketFile, Listen => 1);

        $self->testException(
            sub {pgBackRest::FileCommon::fileManifestStat($strSocketFile)}, ERROR_FILE_INVALID,
            "${strSocketFile} is not of type directory, file, or link");

        # Cleanup socket
        $oSocket->close();
        fileRemove($strSocketFile);

        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestPath = $self->testPath() . '/public_dir';
        filePathCreate($strTestPath, '0750');

        $self->testResult(
            sub {pgBackRest::FileCommon::fileManifestStat($strTestPath)},
            '{group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}',
            'stat directory');

        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestLink = $self->testPath() . '/public_dir_link';

        symlink($strTestPath, $strTestLink)
            or confess &log(ERROR, "unable to create symlink from ${strTestPath} to ${strTestLink}");

        $self->testResult(
            sub {pgBackRest::FileCommon::fileManifestStat($strTestLink)},
            '{group => ' . $self->group() . ", link_destination => ${strTestPath}, type => l, user => " . $self->pgUser() . '}',
            'stat link');
    }

    ################################################################################################################################
    if ($self->begin("FileCommon::fileManifestList()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my @stryFile = ('.', 'test.txt');

        $self->testResult(
            sub {pgBackRest::FileCommon::fileManifestList($self->testPath(), \@stryFile)},
            '{. => {group => ' . $self->group() . ', mode => 0770, type => d, user => ' . $self->pgUser() . '}}',
            'skip missing file');
    }

    ################################################################################################################################
    if ($self->begin("FileCommon::fileManifestRecurse()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestPath = $self->testPath() . '/public_dir';
        my $strTestFile = "${strTestPath}/test.txt";

        $self->testException(
            sub {my $hManifest = {}; pgBackRest::FileCommon::fileManifestRecurse($strTestFile, undef, 0, $hManifest); $hManifest},
            ERROR_FILE_MISSING, "unable to stat ${strTestFile}: No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        filePathCreate($strTestPath, '0750');

        $self->testResult(
            sub {my $hManifest = {}; pgBackRest::FileCommon::fileManifestRecurse($strTestPath, undef, 0, $hManifest); $hManifest},
            '{. => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}}',
            'empty directory manifest');

        #---------------------------------------------------------------------------------------------------------------------------
        fileStringWrite($strTestFile, "TEST");
        utime(1111111111, 1111111111, $strTestFile);
        executeTest('chmod 0750 ' . $strTestFile);

        filePathCreate("${strTestPath}/sub", '0750');

        $self->testResult(
            sub {my $hManifest = {}; pgBackRest::FileCommon::fileManifestRecurse(
                $self->testPath(), basename($strTestPath), 1, $hManifest); $hManifest},
            '{public_dir => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'public_dir/sub => {group => ' . $self->group() . ', mode => 0750, type => d, user => ' . $self->pgUser() . '}, ' .
            'public_dir/' . basename($strTestFile) . ' => {group => ' . $self->group() .
                ', mode => 0750, modification_time => 1111111111, size => 4, type => f, user => ' . $self->pgUser() . '}}',
            'directory and file manifest');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {my $hManifest = {}; pgBackRest::FileCommon::fileManifestRecurse($strTestFile, undef, 0, $hManifest); $hManifest},
            '{' . basename($strTestFile) . ' => {group => ' . $self->group() .
                ', mode => 0750, modification_time => 1111111111, size => 4, type => f, user => ' . $self->pgUser() . '}}',
            'single file manifest');
    }

    # Loop through local/remote
    for (my $bRemote = false; $bRemote <= true; $bRemote++)
    {
        if (!$self->begin('File->manifest() => ' . ($bRemote ? 'remote' : 'local'))) {next}

        # Create the file object
        my $oFile = new pgBackRest::File
        (
            $self->stanza(),
            $self->testPath(),
            $bRemote ? $self->remote() : $self->local()
        );

        #---------------------------------------------------------------------------------------------------------------------------
        my $strMissingFile = $self->testPath() . '/missing';

        $self->testException(
            sub {$oFile->manifest(PATH_BACKUP_ABSOLUTE, $strMissingFile)},
            ERROR_FILE_MISSING, "unable to stat ${strMissingFile}: No such file or directory");

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
            sub {$oFile->manifest(PATH_BACKUP_ABSOLUTE, $self->testPath())},
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
}

1;
