####################################################################################################################################
# Archive Get and Base Tests
####################################################################################################################################
package pgBackRestTest::Module::Archive::ArchiveGetTest;
use parent 'pgBackRestTest::Env::ConfigEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Storable qw(dclone);
use Digest::SHA qw(sha1_hex);

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Get::Get;
use pgBackRest::Archive::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Storage::Helper;

use pgBackRestTest::Env::HostEnvTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Clear cache from the previous test
    storageRepoCacheClear($self->stanza());

    # Load options
    $self->configTestClear();
    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
    $self->optionTestSet(CFGOPT_DB_PATH, $self->{strDbPath});
    $self->configTestLoad(CFGCMD_ARCHIVE_GET);

    # Create archive info path
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});

    # Create backup info path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});

    # Create pg_control path
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});

    # Copy a pg_control file into the pg_control path
    executeTest(
        'cp ' . $self->dataPath() . '/backup.pg_control_' . WAL_VERSION_94 . '.bin ' . $self->{strDbPath} . '/' .
        DB_FILE_PGCONTROL);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;
    my $oArchiveBase = new pgBackRest::Archive::Base();

    # Define test file
    my $strFileContent = 'TESTDATA';
    my $strFileHash = sha1_hex($strFileContent);
    my $iFileSize = length($strFileContent);

    my $strDestinationPath = $self->{strDbPath} . "/pg_xlog";
    my $strDestinationFile = $strDestinationPath . "/RECOVERYXLOG";
    my $strWalSegment = '000000010000000100000001';
    my $strArchivePath;


# CSHANG:
# Archive->get - Make sure the files get copied
# - Store a history file and send that to the get process
# - Store a valid wal uncompressed segment and send that to the get process
# - Store a valid wal uncompressed segment and send that to the get process
# Archive->getArchiveId - How to simulate Remote in unit test
# - Make sure archive ID is returned - or undefined?
# ->getCheck
# - Make sure if same name and same db/sys ids that wal found is most recent
# - Wal name valid but can't find wal in current DB / in previous db


    ################################################################################################################################
    if ($self->begin("Archive::Base::getCheck()"))
    {
        # Create and save archive.info file
        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true});
        $oArchiveInfo->create(PG_VERSION_92, WAL_VERSION_92_SYS_ID, false);
        $oArchiveInfo->dbSectionSet(PG_VERSION_93, WAL_VERSION_93_SYS_ID, $oArchiveInfo->dbHistoryIdGet(false) + 1);
        $oArchiveInfo->dbSectionSet(PG_VERSION_92, WAL_VERSION_92_SYS_ID, $oArchiveInfo->dbHistoryIdGet(false) + 1);
        $oArchiveInfo->dbSectionSet(PG_VERSION_94, WAL_VERSION_94_SYS_ID, $oArchiveInfo->dbHistoryIdGet(false) + 10);
        $oArchiveInfo->save();

        # db-version, db-sys-id and wal passed all undefined
        #---------------------------------------------------------------------------------------------------------------------------
        my ($strArchiveId, $strArchiveFile, $strCipherPass) = $oArchiveBase->getCheck(undef, undef, undef, false);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'undef db-version, db-sys-id and wal returns only current db archive-id');

        # db-version defined, db-sys-id and wal undefined
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) = $oArchiveBase->getCheck(PG_VERSION_92, undef, undef, false);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'old db-version, db-sys-id and wal undefined returns only current db archive-id');

        # db-version undefined, db-sys-id defined and wal undefined
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) = $oArchiveBase->getCheck(undef, WAL_VERSION_93_SYS_ID, undef, false);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'undef db-version, old db-sys-id and wal undef returns only current db archive-id');

        # old db-version, db-sys-id and wal undefined, check = true (default)
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) = $oArchiveBase->getCheck(PG_VERSION_92, undef, undef);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'old db-version, db-sys-id and wal undefined, check = true returns only current db archive-id');

        # old db-version, old db-sys-id and wal undefined, check = true (default)
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oArchiveBase->getCheck(PG_VERSION_93, WAL_VERSION_93_SYS_ID, undef)}, ERROR_ARCHIVE_MISMATCH,
            "WAL segment version " . PG_VERSION_93 . " does not match archive version " . PG_VERSION_94 . "\n" .
            "WAL segment system-id " . WAL_VERSION_93_SYS_ID . " does not match archive system-id " . WAL_VERSION_94_SYS_ID .
            "\nHINT: are you archiving to the correct stanza?");

        # db-version, db-sys-id undefined, wal requested is stored in old archive
        #---------------------------------------------------------------------------------------------------------------------------
        $strArchivePath = $self->{strArchivePath} . "/" . PG_VERSION_92 . "-1/";
        my $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        my $strWalSegmentHash = "${strWalSegment}-${strFileHash}";

        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash}");

        ($strArchiveId, $strArchiveFile, $strCipherPass) = $oArchiveBase->getCheck(undef, undef, $strWalSegment, false);

        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'undef db-version, db-sys-id with a requested wal not in current db archive returns only current db archive-id');

        # Pass db-version and db-sys-id where WAL is actually located
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) =
            $oArchiveBase->getCheck(PG_VERSION_92, WAL_VERSION_92_SYS_ID, $strWalSegment, false);

        $self->testResult(sub {($strArchiveId eq PG_VERSION_92 . '-1') && ($strArchiveFile eq $strWalSegmentHash) && !defined($strCipherPass)},
            true, 'db-version, db-sys-id with a requested wal in requested db archive');

        # Put same WAL segment in more recent archive for same DB
        #---------------------------------------------------------------------------------------------------------------------------
        $strArchivePath = $self->{strArchivePath} . "/" . PG_VERSION_92 . "-3/";
        $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        $strWalSegmentHash = "${strWalSegment}-${strFileHash}";

        # Store with actual data that will match the hash check
        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash}", $strFileContent);

        ($strArchiveId, $strArchiveFile, $strCipherPass) =
            $oArchiveBase->getCheck(PG_VERSION_92, WAL_VERSION_92_SYS_ID, $strWalSegment, false);

        # Using the returned values, confirm the correct file is read
        $self->testResult(sub {sha1_hex(${storageRepo()->get($self->{strArchivePath} . "/" . $strArchiveId . "/" .
            substr($strWalSegment, 0, 16) . "/" . $strArchiveFile)})}, $strFileHash,
            'check correct WAL retrieved when in multiple locations');
    }

    ################################################################################################################################
    if ($self->begin("Archive::Get::Get::get()"))
    {
        # archive.info missing
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {new pgBackRest::Archive::Get::Get()->get($strWalSegment, $strDestinationFile)},
            ERROR_FILE_MISSING,
            ARCHIVE_INFO_FILE . " does not exist but is required to push/get WAL segments\n" .
            "HINT: is archive_command configured in postgresql.conf?\n" .
            "HINT: has a stanza-create been performed?\n" .
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.");

        # Create and save archive.info file
        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true});
        $oArchiveInfo->create(PG_VERSION_94, WAL_VERSION_94_SYS_ID, false);
        $oArchiveInfo->dbSectionSet(PG_VERSION_93, WAL_VERSION_93_SYS_ID, $oArchiveInfo->dbHistoryIdGet(false) + 1);
        $oArchiveInfo->dbSectionSet(PG_VERSION_94, WAL_VERSION_94_SYS_ID, $oArchiveInfo->dbHistoryIdGet(false) + 10);
        $oArchiveInfo->save();

        # file not found
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {new pgBackRest::Archive::Get::Get()->get($strWalSegment, $strDestinationFile)}, 1,
            "unable to find ${strWalSegment} in the archive");

        # file found but is not a WAL segment
        #---------------------------------------------------------------------------------------------------------------------------
        $strArchivePath = $self->{strArchivePath} . "/" . PG_VERSION_94 . "-1/";
# CSHANG Need to put a WAL into the old archive dir, make sure it gets it, then add same wal in new archive and see that it gets that?
        # storageRepo()->pathCreate($strArchivePath);
        # storageRepo()->put($strArchivePath . BOGUS, BOGUS);
        # my $strBogusHash = sha1_hex(BOGUS);
        #
        # # Create path to copy file
        # storageRepo()->pathCreate($strDestinationPath);
        #
        # $self->testResult(sub {new pgBackRest::Archive::Get::Get()->get(BOGUS, $strDestinationFile)}, 0,
        #     "non-WAL segment copied");
        #
        # # Confirm the correct file is copied
        # $self->testResult(sub {sha1_hex(${storageRepo()->get($self->{strArchivePath} . "/" . $strArchiveId . "/" .
        #     substr($strWalSegment, 0, 16) . "/" . $strArchiveFile)})}, $strFileHash,
        #     'check correct WAL retrieved when in multiple locations');

    }
    #
    #     my $strDbPath = '/db';
    #     my $strWalFileRelative = 'pg_wal/000000010000000100000001';
    #     my $strWalFileAbsolute = "${strDbPath}/${strWalFileRelative}";
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testException(
    #         sub {walPath($strWalFileRelative, undef, cfgCommandName(CFGCMD_ARCHIVE_GET))}, ERROR_OPTION_REQUIRED,
    #         "option 'db-path' must be specified when relative wal paths are used\n" .
    #         "HINT: Is \%f passed to " . cfgCommandName(CFGCMD_ARCHIVE_GET) . " instead of \%p?\n" .
    #         "HINT: PostgreSQL may pass relative paths even with \%p depending on the environment.");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(
    #         sub {walPath($strWalFileRelative, $strDbPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
    #         'relative path is contructed');
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(
    #         sub {walPath($strWalFileAbsolute, $strDbPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
    #         'path is not relative and db-path is still specified');
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(
    #         sub {walPath($strWalFileAbsolute, $strDbPath, cfgCommandName(CFGCMD_ARCHIVE_PUSH))}, $strWalFileAbsolute,
    #         'path is not relative and db-path is undef');
    # }
    #
    # ################################################################################################################################
    # if ($self->begin("${strModule}::walIsSegment()"))
    # {
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(sub {walIsSegment('0000000200ABCDEF0000001')}, false, 'invalid segment');
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(sub {walIsSegment('0000000200ABCDEF00000001')}, true, 'valid segment');
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(sub {walIsSegment('000000010000000100000001.partial')}, true, 'valid partial segment');
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(sub {walIsSegment('00000001.history')}, false, 'valid history file');
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(sub {walIsSegment('000000020000000100000001.00000028.backup')}, false, 'valid backup file');
    # }
    #
    # ################################################################################################################################
    # if ($self->begin("${strModule}::walIsPartial()"))
    # {
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     my $strWalSegment = '0000000200ABCDEF00000001';
    #
    #     $self->testResult(sub {walIsPartial($strWalSegment)}, false, "${strWalSegment} WAL is not partial");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $strWalSegment = $strWalSegment . '.partial';
    #
    #     $self->testResult(sub {walIsPartial($strWalSegment)}, true, "${strWalSegment} WAL is partial");
    # }
    #
    # ################################################################################################################################
    # if ($self->begin("${strModule}::walSegmentFind()"))
    # {
    #     $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    #     $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath());
    #     $self->configTestLoad(CFGCMD_ARCHIVE_PUSH);
    #
    #     my $strArchiveId = '9.4-1';
    #     my $strArchivePath = storageRepo()->pathGet(STORAGE_REPO_ARCHIVE . "/${strArchiveId}");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     my $strWalSegment = '000000010000000100000001ZZ';
    #
    #     $self->testException(
    #         sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, ERROR_ASSERT,
    #         "${strWalSegment} is not a WAL segment");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $strWalSegment = '000000010000000100000001';
    #
    #     $self->testResult(
    #         sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, undef, "${strWalSegment} WAL not found");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testException(
    #         sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment, .1)}, ERROR_ARCHIVE_TIMEOUT,
    #         "could not find WAL segment ${strWalSegment} after 0.1 second(s)");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     my $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
    #     my $strWalSegmentHash = "${strWalSegment}-53aa5d59515aa7288ae02ba414c009aed1ca73ad";
    #
    #     storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
    #     storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash}");
    #
    #     $self->testResult(
    #         sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, $strWalSegmentHash, "${strWalSegment} WAL found");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     my $strWalSegmentHash2 = "${strWalSegment}-a0b0d38b8aa263e25b8ff52a0a4ba85b6be97f9b.gz";
    #
    #     storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash2}");
    #
    #     $self->testException(
    #         sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, ERROR_ARCHIVE_DUPLICATE,
    #         "duplicates found in archive for WAL segment ${strWalSegment}: ${strWalSegmentHash}, ${strWalSegmentHash2}");
    #
    #     storageRepo()->remove("${strWalMajorPath}/${strWalSegmentHash}");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $self->testResult(
    #         sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, $strWalSegmentHash2,
    #         "${strWalSegment} WAL found with compressed extension");
    #
    #     storageRepo()->remove("${strWalMajorPath}/${strWalSegmentHash2}");
    #
    #     #---------------------------------------------------------------------------------------------------------------------------
    #     $strWalSegment = $strWalSegment . '.partial';
    #     $strWalSegmentHash = "${strWalSegment}-996195c807713ef9262170043e7222cb150aef70";
    #     storageRepo()->put("${strWalMajorPath}/${strWalSegmentHash}");
    #
    #     $self->testResult(
    #         sub {walSegmentFind(storageRepo(), $strArchiveId, $strWalSegment)}, $strWalSegmentHash, "${strWalSegment} WAL found");
    # }
}

1;
