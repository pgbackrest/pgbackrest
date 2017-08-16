####################################################################################################################################
# Archive Common Tests
####################################################################################################################################
package pgBackRestTest::Module::Archive::ArchiveCommonTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Storable qw(dclone);

use pgBackRest::Archive::Common;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::LibC qw(:config);
use pgBackRest::Protocol::Storage::Helper;

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
            sub {walPath($strWalFileRelative, undef, cfgCommandName(CFGCMD_ARCHIVE_GET))}, ERROR_OPTION_REQUIRED,
            "option 'db-path' must be specified when relative xlog paths are used\n" .
            "HINT: Is \%f passed to " . cfgCommandName(CFGCMD_ARCHIVE_GET) . " instead of \%p?\n" .
            "HINT: PostgreSQL may pass relative paths even with \%p depending on the environment.");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileRelative, $strDbPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
            'relative path is contructed');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileAbsolute, $strDbPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
            'path is not relative and db-path is still specified');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileAbsolute, $strDbPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
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
        $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
        $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath());
        $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);

        my $strArchiveId = '9.4-1';
        my $strArchivePath = storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/${strArchiveId}");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegment = '000000010000000100000001ZZ';

        $self->testException(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, ERROR_ASSERT,
            "${strWalSegment} is not a WAL segment");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegment = '000000010000000100000001';

        $self->testResult(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, undef, "${strWalSegment} WAL not found");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment, .1)}, ERROR_ARCHIVE_TIMEOUT,
            "could not find WAL segment ${strWalSegment} after 0.1 second(s)");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        my $strWalSegmentHash = "${strWalSegment}-53aa5d59515aa7288ae02ba414c009aed1ca73ad";

        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testResult(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, $strWalSegmentHash, "${strWalSegment} WAL found");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walSegmentFind(storageRepo(), $strArchiveId, substr($strWalSegment, 8, 16))}, $strWalSegmentHash,
            "${strWalSegment} WAL found without timeline");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegmentHash2 = "${strWalSegment}-a0b0d38b8aa263e25b8ff52a0a4ba85b6be97f9b.gz";

        storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash2}");

        $self->testException(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, ERROR_ARCHIVE_DUPLICATE,
            "duplicates found in archive for WAL segment ${strWalSegment}: ${strWalSegmentHash}, ${strWalSegmentHash2}");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegment3 = '00000002' . substr($strWalSegment, 8, 16);
        my $strWalSegmentHash3 = "${strWalSegment3}-dcdd09246e1918e88c67cf44b35edc23b803d879";
        my $strWalMajorPath3 = "${strArchivePath}/" . substr($strWalSegment3, 0, 16);

        storageRepo()->pathCreate($strWalMajorPath3, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath3}/${strWalSegmentHash3}");

        $self->testException(
            sub {walSegmentFind(storageRepo(), $strArchiveId, substr($strWalSegment, 8, 16))}, ERROR_ARCHIVE_DUPLICATE,
            "duplicates found in archive for WAL segment XXXXXXXX" . substr($strWalSegment, 8, 16) .
            ": ${strWalSegmentHash}, ${strWalSegmentHash2}, ${strWalSegmentHash3}");

        storageRepo()->remove("${strWalMajorPath}/${strWalSegmentHash}");
        storageRepo()->remove("${strWalMajorPath3}/${strWalSegmentHash3}");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, $strWalSegmentHash2,
            "${strWalSegment} WAL found with compressed extension");

        storageRepo()->remove("${strWalMajorPath}/${strWalSegmentHash2}");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegment = $strWalSegment . '.partial';
        $strWalSegmentHash = "${strWalSegment}-996195c807713ef9262170043e7222cb150aef70";
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testResult(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, $strWalSegmentHash, "${strWalSegment} WAL found");
    }
}

1;
