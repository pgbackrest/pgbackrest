####################################################################################################################################
# Archive Get and Base Tests
####################################################################################################################################
package pgBackRestTest::Module::Command::CommandArchiveGetPerlTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Storable qw(dclone);

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Get::File;
use pgBackRest::Archive::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::LibC qw(:crypto);
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

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
    $self->{strLockPath} = $self->testPath() . '/lock';
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
    $self->{strSpoolPath} = "$self->{strArchivePath}/in";
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Clear cache from the previous test
    storageRepoCacheClear();

    # Load options
    $self->configTestClear();
    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->testPath() . '/repo');
    $self->optionTestSet(CFGOPT_PG_PATH, $self->{strDbPath});
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());
    $self->optionTestSet(CFGOPT_LOCK_PATH, $self->{strLockPath});
    $self->configTestLoad(CFGCMD_ARCHIVE_GET);

    # Create archive info path
    storageTest()->pathCreate($self->{strArchivePath}, {bIgnoreExists => true, bCreateParent => true});

    # Create backup info path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});

    # Create spool path
    storageTest()->pathCreate($self->{strSpoolPath}, {bIgnoreExists => true, bCreateParent => true});

    # Create pg_control path
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});

    # Generate pg_control file
    $self->controlGenerate($self->{strDbPath}, PG_VERSION_94);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    # Define test file
    my $strFileContent = 'TESTDATA';
    my $strFileHash = cryptoHashOne('sha1', $strFileContent);
    my $iFileSize = length($strFileContent);

    my $strDestinationPath = $self->{strDbPath} . "/pg_xlog";
    my $strDestinationFile = $strDestinationPath . "/RECOVERYXLOG";
    my $strWalSegment = '000000010000000100000001';
    my $strArchivePath;

    ################################################################################################################################
    if ($self->begin("Archive::Common::archiveGetCheck()"))
    {
        # Create and save archive.info file
        my $oArchiveInfo = new pgBackRest::Archive::Info(storageRepo()->pathGet(STORAGE_REPO_ARCHIVE), false,
            {bLoad => false, bIgnoreMissing => true});
        $oArchiveInfo->create(PG_VERSION_92, $self->dbSysId(PG_VERSION_92), false);
        $oArchiveInfo->dbSectionSet(PG_VERSION_93, $self->dbSysId(PG_VERSION_93), $oArchiveInfo->dbHistoryIdGet(false) + 1);
        $oArchiveInfo->dbSectionSet(PG_VERSION_92, $self->dbSysId(PG_VERSION_92), $oArchiveInfo->dbHistoryIdGet(false) + 1);
        $oArchiveInfo->dbSectionSet(PG_VERSION_94, $self->dbSysId(PG_VERSION_94), $oArchiveInfo->dbHistoryIdGet(false) + 10);
        $oArchiveInfo->save();

        # db-version, db-sys-id passed but combination doesn't exist in archive.info history
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {archiveGetCheck(
            PG_VERSION_95, $self->dbSysId(PG_VERSION_94), undef, false)}, ERROR_ARCHIVE_MISMATCH,
            "unable to retrieve the archive id for database version '" . PG_VERSION_95 . "' and system-id '" .
            $self->dbSysId(PG_VERSION_94) . "'");

        # db-version, db-sys-id and wal passed all undefined
        #---------------------------------------------------------------------------------------------------------------------------
        my ($strArchiveId, $strArchiveFile, $strCipherPass) = archiveGetCheck(undef, undef, undef, false);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'undef db-version, db-sys-id and wal returns only current db archive-id');

        # db-version defined, db-sys-id and wal undefined
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) = archiveGetCheck(PG_VERSION_92, undef, undef, false);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'old db-version, db-sys-id and wal undefined returns only current db archive-id');

        # db-version undefined, db-sys-id defined and wal undefined
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) = archiveGetCheck(
            undef, $self->dbSysId(PG_VERSION_93), undef, false);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'undef db-version, old db-sys-id and wal undef returns only current db archive-id');

        # old db-version, db-sys-id and wal undefined, check = true (default)
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) = archiveGetCheck(PG_VERSION_92, undef, undef);
        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'old db-version, db-sys-id and wal undefined, check = true returns only current db archive-id');

        # old db-version, old db-sys-id and wal undefined, check = true (default)
        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {archiveGetCheck(
            PG_VERSION_93, $self->dbSysId(PG_VERSION_93), undef)}, ERROR_ARCHIVE_MISMATCH,
            "WAL segment version " . PG_VERSION_93 . " does not match archive version " . PG_VERSION_94 . "\n" .
            "WAL segment system-id " . $self->dbSysId(PG_VERSION_93) . " does not match archive system-id " .
            $self->dbSysId(PG_VERSION_94) . "\nHINT: are you archiving to the correct stanza?");

        # db-version, db-sys-id undefined, wal requested is stored in old archive
        #---------------------------------------------------------------------------------------------------------------------------
        $strArchivePath = $self->{strArchivePath} . "/" . PG_VERSION_92 . "-1/";
        my $strWalMajorPath = "${strArchivePath}/" . substr($strWalSegment, 0, 16);
        my $strWalSegmentName = "${strWalSegment}-${strFileHash}";

        storageRepo()->pathCreate($strWalMajorPath, {bCreateParent => true});
        storageRepo()->put("${strWalMajorPath}/${strWalSegmentName}");

        ($strArchiveId, $strArchiveFile, $strCipherPass) = archiveGetCheck(undef, undef, $strWalSegment, false);

        $self->testResult(sub {($strArchiveId eq PG_VERSION_94 . '-13') && !defined($strArchiveFile) && !defined($strCipherPass)},
            true, 'undef db-version, db-sys-id with a requested wal not in current db archive returns only current db archive-id');

        # Pass db-version and db-sys-id where WAL is actually located
        #---------------------------------------------------------------------------------------------------------------------------
        ($strArchiveId, $strArchiveFile, $strCipherPass) =
            archiveGetCheck(PG_VERSION_92, $self->dbSysId(PG_VERSION_92), $strWalSegment, false);

        $self->testResult(
            sub {($strArchiveId eq PG_VERSION_92 . '-1') && ($strArchiveFile eq $strWalSegmentName) && !defined($strCipherPass)},
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
            archiveGetCheck(PG_VERSION_92, $self->dbSysId(PG_VERSION_92), $strWalSegment, false);

        # Using the returned values, confirm the correct file is read
        $self->testResult(sub {cryptoHashOne('sha1', ${storageRepo()->get($self->{strArchivePath} . "/" . $strArchiveId . "/" .
            substr($strWalSegment, 0, 16) . "/" . $strArchiveFile)})}, $strFileHash,
            'check correct WAL archiveID when in multiple locations');
    }
}

1;
