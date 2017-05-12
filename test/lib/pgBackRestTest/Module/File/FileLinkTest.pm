####################################################################################################################################
# FileLinkTest.pm - Tests for FileCommon::fileLinkDestination
####################################################################################################################################
package pgBackRestTest::Module::File::FileLinkTest;
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
    if ($self->begin("FileCommon::fileLinkDestination()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestLink = $self->testPath() . '/public_dir_link';

        $self->testException(
            sub {fileLinkDestination($strTestLink)}, ERROR_FILE_MISSING,
            "unable to get destination for link ${strTestLink}: No such file or directory");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strTestPath = $self->testPath() . '/public_dir';
        filePathCreate($strTestPath);

        $self->testException(
            sub {fileLinkDestination($strTestPath)}, ERROR_FILE_OPEN,
            "unable to get destination for link ${strTestPath}: Invalid argument");

        #---------------------------------------------------------------------------------------------------------------------------
        symlink($strTestPath, $strTestLink)
            or confess &log(ERROR, "unable to create symlink from ${strTestPath} to ${strTestLink}");

        $self->testResult(sub {fileLinkDestination($strTestLink)}, $strTestPath, 'get link destination');
    }
}

1;
