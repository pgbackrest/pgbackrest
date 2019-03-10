####################################################################################################################################
# Archive Common Tests
####################################################################################################################################
package pgBackRestTest::Module::Command::CommandArchiveCommonPerlTest;
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
use pgBackRest::DbVersion;
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
    if ($self->begin("${strModule}::lsnFileRange()"))
    {
        $self->testResult(sub {lsnFileRange("1/60", "1/60", PG_VERSION_92, 16 * 1024 * 1024)}, "0000000100000000", 'get single');
        $self->testResult(
            sub {lsnFileRange("1/FD000000", "2/1000000", PG_VERSION_92, 16 * 1024 * 1024)},
            "(00000001000000FD, 00000001000000FE, 0000000200000000, 0000000200000001)", 'get range < 9.3');
        $self->testResult(
            sub {lsnFileRange("1/FD000000", "2/60", PG_VERSION_93, 16 * 1024 * 1024)},
            "(00000001000000FD, 00000001000000FE, 00000001000000FF, 0000000200000000)", 'get range >= 9.3');
        $self->testResult(
            sub {lsnFileRange("A/800", "B/C0000000", PG_VERSION_11, 1024 * 1024 * 1024)},
            '(0000000A00000000, 0000000A00000001, 0000000A00000002, 0000000A00000003, 0000000B00000000, 0000000B00000001, ' .
                '0000000B00000002, 0000000B00000003)',
            'get range >= 11/1GB');
        $self->testResult(
            sub {lsnFileRange("7/FFEFFFFF", "8/001AAAAA", PG_VERSION_11, 1024 * 1024)},
            '(0000000700000FFE, 0000000700000FFF, 0000000800000000, 0000000800000001)', 'get range >= 11/1MB');
    }

    ################################################################################################################################
    if ($self->begin("${strModule}::walPath()"))
    {
        my $strPgPath = '/db';
        my $strWalFileRelative = 'pg_wal/000000010000000100000001';
        my $strWalFileAbsolute = "${strPgPath}/${strWalFileRelative}";

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(
            sub {walPath($strWalFileRelative, undef, cfgCommandName(CFGCMD_ARCHIVE_GET))}, ERROR_OPTION_REQUIRED,
            "option 'pg1-path' must be specified when relative wal paths are used\n" .
            "HINT: Is \%f passed to " . cfgCommandName(CFGCMD_ARCHIVE_GET) . " instead of \%p?\n" .
            "HINT: PostgreSQL may pass relative paths even with \%p depending on the environment.");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileRelative, $strPgPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
            'relative path is contructed');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileAbsolute, $strPgPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
            'path is not relative and pg-path is still specified');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {walPath($strWalFileAbsolute, $strPgPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
            'path is not relative and pg-path is undef');
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
            "could not find WAL segment ${strWalSegment} after 0.1 second(s)" .
                "\nHINT: is archive_command configured correctly?" .
                "\nHINT: use the check command to verify that PostgreSQL is archiving.");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        my $strWalSegmentHash = "${strWalSegment}-53aa5d59515aa7288ae02ba414c009aed1ca73ad";

        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testResult(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, $strWalSegmentHash, "${strWalSegment} WAL found");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegmentHash2 = "${strWalSegment}-a0b0d38b8aa263e25b8ff52a0a4ba85b6be97f9b.gz";

        storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash2}");

        $self->testException(
            sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, ERROR_ARCHIVE_DUPLICATE,
            "duplicates found in archive for WAL segment ${strWalSegment}: ${strWalSegmentHash}, ${strWalSegmentHash2}");

        storageRepo()->remove("${strWalMajorPath}/${strWalSegmentHash}");

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

    ################################################################################################################################
    if ($self->begin("archiveAsyncStatusWrite()"))
    {
        my $iWalTimeline = 1;
        my $iWalMajor = 1;
        my $iWalMinor = 1;

        # Create the spool path
        my $strSpoolPath = $self->testPath() . "/spool/out";
        $self->storageTest()->pathCreate($strSpoolPath, {bIgnoreExists => true, bCreateParent => true});

        #---------------------------------------------------------------------------------------------------------------------------
        my $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);

        # Generate a normal ok
        archiveAsyncStatusWrite(WAL_STATUS_OK, $strSpoolPath, $strSegment);

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a valid warning ok
        archiveAsyncStatusWrite(WAL_STATUS_OK, $strSpoolPath, $strSegment, 0, 'Test Warning');

        # Skip error when an ok file already exists
        #---------------------------------------------------------------------------------------------------------------------------
        archiveAsyncStatusWrite(
            WAL_STATUS_ERROR, $strSpoolPath, $strSegment, ERROR_ARCHIVE_DUPLICATE,
            "WAL segment ${strSegment} already exists in the archive", true);

        $self->testResult(
            $self->storageTest()->exists("${strSpoolPath}/${strSegment}.error"), false, "error file should not exist");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate an invalid error
        $self->testException(
            sub {archiveAsyncStatusWrite(WAL_STATUS_ERROR, $strSpoolPath, $strSegment)}, ERROR_ASSERT,
            "error status must have iCode and strMessage set");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate an invalid error
        $self->testException(
            sub {archiveAsyncStatusWrite(WAL_STATUS_ERROR, $strSpoolPath, $strSegment, ERROR_ASSERT)},
            ERROR_ASSERT, "strMessage must be set when iCode is set");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a valid error
        archiveAsyncStatusWrite(
            WAL_STATUS_ERROR, $strSpoolPath, $strSegment, ERROR_ARCHIVE_DUPLICATE,
            "WAL segment ${strSegment} already exists in the archive");
    }
}

1;
