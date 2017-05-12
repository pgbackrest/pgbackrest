####################################################################################################################################
# ArchiveUnitTest.pm - Tests for ArchiveCommon module
####################################################################################################################################
package pgBackRestTest::Module::Archive::ArchiveUnitTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;

use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;
    my $strModule = 'ArchiveCommon';

    ################################################################################################################################
    if ($self->begin("${strModule}::walPath()"))
    {
        my $strDbPath = '/db';
        my $strWalFileRelative = 'pg_xlog/000000010000000100000001';
        my $strWalFileAbsolute = "${strDbPath}/${strWalFileRelative}";

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {walPath($strWalFileRelative, undef, CMD_ARCHIVE_GET)}, ERROR_OPTION_REQUIRED,
            "option '" . OPTION_DB_PATH . "' must be specified when relative xlog paths are used\n" .
            "HINT: Is \%f passed to " . CMD_ARCHIVE_GET . " instead of \%p?\n" .
            "HINT: PostgreSQL may pass relative paths even with \%p depending on the environment.");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileRelative, $strDbPath, CMD_ARCHIVE_PUSH)}, $strWalFileAbsolute, 'relative path is contructed');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileAbsolute, $strDbPath, CMD_ARCHIVE_PUSH)}, $strWalFileAbsolute,
            'path is not relative and db-path is still specified');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileAbsolute, $strDbPath, CMD_ARCHIVE_PUSH)}, $strWalFileAbsolute,
            'path is not relative and db-path is undef');
    }

    ################################################################################################################################
    if ($self->begin("${strModule}::walIsSegment()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {walIsSegment('0000000200ABCDEF0000001')}, false, 'invalid segment');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {walIsSegment('0000000200ABCDEF00000001')}, true, 'valid segment');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {walIsSegment('000000010000000100000001.partial')}, true, 'valid partial segment');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {walIsSegment('00000001.history')}, false, 'valid history file');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {walIsSegment('000000020000000100000001.00000028.backup')}, false, 'valid backup file');
    }

    ################################################################################################################################
    if ($self->begin("${strModule}::walIsPartial()"))
    {
        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegment = '0000000200ABCDEF00000001';

        $self->testResult(sub {walIsPartial($strWalSegment)}, false, "${strWalSegment} WAL is not partial");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegment = $strWalSegment . '.partial';

        $self->testResult(sub {walIsPartial($strWalSegment)}, true, "${strWalSegment} WAL is partial");
    }

    ################################################################################################################################
    if ($self->begin("${strModule}::walSegmentFind()"))
    {
        my $strArchiveId = '9.4-1';
        my $oFile = new pgBackRest::File(
            $self->stanza(),
            $self->testPath(),
            new pgBackRest::Protocol::Common(
                OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
                OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
                OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
                HOST_PROTOCOL_TIMEOUT                       # Protocol timeout
            ));

        my $strArchivePath = $oFile->pathGet(PATH_BACKUP_ARCHIVE, $strArchiveId);;

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegment = '000000010000000100000001ZZ';

        $self->testException(
            sub {walSegmentFind($oFile, $strArchiveId, $strWalSegment)}, ERROR_ASSERT, "${strWalSegment} is not a WAL segment");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegment = '000000010000000100000001';

        $self->testResult(
            sub {walSegmentFind($oFile, $strArchiveId, $strWalSegment)}, undef, "${strWalSegment} WAL not found");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {walSegmentFind($oFile, $strArchiveId, $strWalSegment, .1)}, ERROR_ARCHIVE_TIMEOUT,
            "could not find WAL segment ${strWalSegment} after 0.1 second(s)");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        my $strWalSegmentHash = "${strWalSegment}-53aa5d59515aa7288ae02ba414c009aed1ca73ad";

        filePathCreate($strWalMajorPath, undef, false, true);
        fileStringWrite("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testResult(
            sub {walSegmentFind($oFile, $strArchiveId, $strWalSegment)}, $strWalSegmentHash, "${strWalSegment} WAL found");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walSegmentFind($oFile, $strArchiveId, substr($strWalSegment, 8, 16))}, $strWalSegmentHash,
            "${strWalSegment} WAL found without timeline");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegmentHash2 = "${strWalSegment}-a0b0d38b8aa263e25b8ff52a0a4ba85b6be97f9b.gz";

        fileStringWrite("${strWalMajorPath}/${strWalSegmentHash2}");

        $self->testException(
            sub {walSegmentFind($oFile, $strArchiveId, $strWalSegment)}, ERROR_ARCHIVE_DUPLICATE,
            "duplicates found in archive for WAL segment ${strWalSegment}: ${strWalSegmentHash}, ${strWalSegmentHash2}");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegment3 = '00000002' . substr($strWalSegment, 8, 16);
        my $strWalSegmentHash3 = "${strWalSegment3}-dcdd09246e1918e88c67cf44b35edc23b803d879";
        my $strWalMajorPath3 = "${strArchivePath}/" . substr($strWalSegment3, 0, 16);

        filePathCreate($strWalMajorPath3, undef, false, true);
        fileStringWrite("${strWalMajorPath3}/${strWalSegmentHash3}");

        $self->testException(
            sub {walSegmentFind($oFile, $strArchiveId, substr($strWalSegment, 8, 16))}, ERROR_ARCHIVE_DUPLICATE,
            "duplicates found in archive for WAL segment XXXXXXXX" . substr($strWalSegment, 8, 16) .
            ": ${strWalSegmentHash}, ${strWalSegmentHash2}, ${strWalSegmentHash3}");

        fileRemove("${strWalMajorPath}/${strWalSegmentHash}");
        fileRemove("${strWalMajorPath3}/${strWalSegmentHash3}");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walSegmentFind($oFile, $strArchiveId, $strWalSegment)}, $strWalSegmentHash2,
            "${strWalSegment} WAL found with compressed extension");

        fileRemove("${strWalMajorPath}/${strWalSegmentHash2}");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegment = $strWalSegment . '.partial';
        $strWalSegmentHash = "${strWalSegment}-996195c807713ef9262170043e7222cb150aef70";
        fileStringWrite("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testResult(
            sub {walSegmentFind($oFile, $strArchiveId, $strWalSegment)}, $strWalSegmentHash, "${strWalSegment} WAL found");
    }
}

1;
