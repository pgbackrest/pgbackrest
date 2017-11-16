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

        # db-version, db-sys-id passed but combination doesn't exist in archive.info history
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {$oArchiveBase->getCheck(PG_VERSION_95, WAL_VERSION_94_SYS_ID, undef, false)}, ERROR_UNKNOWN,
            "unable to retrieve the archive id for database version '" . PG_VERSION_95 . "' and system-id '" .
            WAL_VERSION_94_SYS_ID . "'");

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
        my $strWalSegmentName = "${strWalSegment}-${strFileHash}";

        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentName}");

        ($strArchiveId, $strArchiveFile, $strCipherPass) = $oArchiveBase->getCheck(undef, undef, $strWalSegment, false);

        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'undef db-version, db-sys-id with a requested wal not in current db archive returns only current db archive-id');

        # Pass db-version and db-sys-id where WAL is actually located
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) =
            $oArchiveBase->getCheck(PG_VERSION_92, WAL_VERSION_92_SYS_ID, $strWalSegment, false);

        $self->testResult(sub {($strArchiveId eq PG_VERSION_92 . '-1') && ($strArchiveFile eq $strWalSegmentName) && !defined($strCipherPass)},
            true, 'db-version, db-sys-id with a requested wal in requested db archive');

        # Put same WAL segment in more recent archive for same DB
        #---------------------------------------------------------------------------------------------------------------------------
        $strArchivePath = $self->{strArchivePath} . "/" . PG_VERSION_92 . "-3/";
        $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        $strWalSegmentName = "${strWalSegment}-${strFileHash}";

        # Store with actual data that will match the hash check
        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentName}", $strFileContent);

        ($strArchiveId, $strArchiveFile, $strCipherPass) =
            $oArchiveBase->getCheck(PG_VERSION_92, WAL_VERSION_92_SYS_ID, $strWalSegment, false);

        # Using the returned values, confirm the correct file is read
        $self->testResult(sub {sha1_hex(${storageRepo()->get($self->{strArchivePath} . "/" . $strArchiveId . "/" .
            substr($strWalSegment, 0, 16) . "/" . $strArchiveFile)})}, $strFileHash,
            'check correct WAL archiveID when in multiple locations');
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

        storageRepo()->pathCreate($strArchivePath);
        storageRepo()->put($strArchivePath . BOGUS, BOGUS);
        my $strBogusHash = sha1_hex(BOGUS);

        # Create path to copy file
        storageRepo()->pathCreate($strDestinationPath);

        $self->testResult(sub {new pgBackRest::Archive::Get::Get()->get(BOGUS, $strDestinationFile)}, 0,
            "non-WAL segment copied");

        # Confirm the correct file is copied
        $self->testResult(sub {sha1_hex(${storageRepo()->get($strDestinationFile)})}, $strBogusHash,
            '   check correct non-WAL copied from older archiveId');

        # create same WAL segment in same DB but different archives and different has values. Confirm latest one copied.
        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        my $strWalSegmentName = "${strWalSegment}-${strFileHash}";

        # Put zero byte file in old archive
        storageRepo()->pathCreate($strWalMajorPath);
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentName}");

        # Create newest archive path
        $strArchivePath = $self->{strArchivePath} . "/" . PG_VERSION_94 . "-12/";
        $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        $strWalSegmentName = "${strWalSegment}-${strFileHash}";

        # Store with actual data that will match the hash check
        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentName}", $strFileContent);

        $self->testResult(sub {new pgBackRest::Archive::Get::Get()->get($strWalSegmentName, $strDestinationFile)}, 0,
            "WAL segment copied");

        # Using the returned values, confirm the correct file is copied
        $self->testResult(sub {sha1_hex(${storageRepo()->get($strDestinationFile)})}, $strFileHash,
            '    check correct WAL copied when in multiple locations');
    }
}

1;
