####################################################################################################################################
# BackupArchiveUnitTest.pm - Tests for ArchiveCommon module
####################################################################################################################################
package pgBackRestTest::Backup::BackupArchiveUnitTest;
use parent 'pgBackRestTest::Backup::BackupCommonTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::ArchiveCommon;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Increment the run, log, and decide whether this unit test should be run
    if (!$self->begin('unit')) {return}

    # Unit tests for walPath()
    #-----------------------------------------------------------------------------------------------------------------------
    {
        my $strDbPath = '/db';
        my $strWalFileRelative = 'pg_xlog/000000010000000100000001';
        my $strWalFileAbsolute = "${strDbPath}/${strWalFileRelative}";

        # Error is thrown if the wal file is relative and there is no db path
        $self->testException(
            sub {walPath($strWalFileRelative, undef, CMD_ARCHIVE_GET)}, ERROR_OPTION_REQUIRED,
            "option '" . OPTION_DB_PATH . "' must be specified when relative xlog paths are used\n" .
            "HINT: Is \%f passed to " . CMD_ARCHIVE_GET . " instead of \%p?\n" .
            "HINT: PostgreSQL may pass relative paths even with \%p depending on the environment.");

        # Relative path is contructed
        $self->testResult(sub {walPath($strWalFileRelative, $strDbPath, CMD_ARCHIVE_PUSH)}, $strWalFileAbsolute);

        # Path is not relative and db-path is still specified
        $self->testResult(sub {walPath($strWalFileAbsolute, $strDbPath, CMD_ARCHIVE_PUSH)}, $strWalFileAbsolute);

        # Path is not relative and db-path is undef
        $self->testResult(sub {walPath($strWalFileAbsolute, $strDbPath, CMD_ARCHIVE_PUSH)}, $strWalFileAbsolute);
    }
}

1;
