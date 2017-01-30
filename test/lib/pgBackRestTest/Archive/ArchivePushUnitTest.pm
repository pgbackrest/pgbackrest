####################################################################################################################################
# ArchivePushUnitTest.pm - Unit tests for ArchivePush and ArchivePush Async
####################################################################################################################################
package pgBackRestTest::Archive::ArchivePushUnitTest;
use parent 'pgBackRestTest::Common::Env::EnvHostTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Archive::ArchivePush;
use pgBackRest::Archive::ArchivePushAsync;
use pgBackRest::Archive::ArchivePushFile;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock    ;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;

use pgBackRestTest::Common::Env::EnvHostTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::Host::HostBackupTest;
use pgBackRestTest::Common::RunTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strWalPath} = "$self->{strDbPath}/pg_xlog";
    $self->{strWalStatusPath} = "$self->{strWalPath}/archive_status";
    $self->{strWalHash} = "1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strArchivePath} = "$self->{strRepoPath}/archive/" . $self->stanza();
    $self->{strSpoolPath} = "$self->{strArchivePath}/out";

    # Create the local file object
    $self->{oFile} =
        new pgBackRest::File
        (
            $self->stanza(),
            $self->{strRepoPath},
            new pgBackRest::Protocol::Common
            (
                OPTION_DEFAULT_BUFFER_SIZE,                 # Buffer size
                OPTION_DEFAULT_COMPRESS_LEVEL,              # Compress level
                OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,      # Compress network level
                HOST_PROTOCOL_TIMEOUT                       # Protocol timeout
            )
        );
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create WAL path
    filePathCreate($self->{strWalStatusPath}, undef, true, true);

    # Create archive info
    filePathCreate($self->{strArchivePath}, undef, true, true);

    my $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($self->{strArchivePath}, false);
    $oArchiveInfo->create(PG_VERSION_94, WAL_VERSION_94_SYS_ID, true);

    $self->{strArchiveId} = $oArchiveInfo->archiveId();
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    my $oOption = {};

    $self->optionSetTest($oOption, OPTION_STANZA, $self->stanza());
    $self->optionSetTest($oOption, OPTION_DB_PATH, $self->{strDbPath});
    $self->optionSetTest($oOption, OPTION_REPO_PATH, $self->{strRepoPath});
    $self->optionSetTest($oOption, OPTION_LOG_PATH, $self->testPath());
    $self->optionBoolSetTest($oOption, OPTION_COMPRESS, false);

    $self->optionSetTest($oOption, OPTION_DB_TIMEOUT, 5);
    $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 6);
    $self->optionSetTest($oOption, OPTION_ARCHIVE_TIMEOUT, 3);

    ################################################################################################################################
    if ($self->begin("ArchivePushFile::archivePushCheck"))
    {
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalSegment = '000000010000000100000001';

        $self->testResult(sub {archivePushCheck(
            $self->{oFile}, $strWalSegment, PG_VERSION_94, WAL_VERSION_94_SYS_ID, "$self->{strWalPath}/${strWalSegment}")},
            '(9.4-1, [undef], [undef])', "${strWalSegment} WAL not found");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strWalMajorPath = "$self->{strArchivePath}/9.4-1/" . substr($strWalSegment, 0, 16);
        my $strWalSegmentHash = "${strWalSegment}-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";

        $self->walGenerate(
            $self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strWalSegment);

        filePathCreate($strWalMajorPath, undef, false, true);
        fileStringWrite("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testResult(sub {archivePushCheck(
            $self->{oFile}, $strWalSegment, PG_VERSION_94, WAL_VERSION_94_SYS_ID, "$self->{strWalPath}/${strWalSegment}")},
            '(9.4-1, 1e34fa1c833090d94b9bb14f2a8d3153dca6ea27,' .
                " WAL segment ${strWalSegment} already exists in the archive with the same checksum\n" .
                'HINT: this is valid in some recovery scenarios but may also indicate a problem.)',
            "${strWalSegment} WAL found");

        fileRemove("${strWalMajorPath}/${strWalSegmentHash}");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegmentHash = "${strWalSegment}-10be15a0ab8e1653dfab18c83180e74f1507cab1";

        fileStringWrite("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testException(sub {archivePushCheck(
            $self->{oFile}, $strWalSegment, PG_VERSION_94, WAL_VERSION_94_SYS_ID, "$self->{strWalPath}/${strWalSegment}")},
            ERROR_ARCHIVE_DUPLICATE, "WAL segment ${strWalSegment} already exists in the archive");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegment = "${strWalSegment}.partial";
        $strWalSegmentHash = "${strWalSegment}-1e34fa1c833090d94b9bb14f2a8d3153dca6ea27";

        $self->walGenerate(
            $self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strWalSegment);

        fileStringWrite("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testResult(sub {archivePushCheck(
            $self->{oFile}, $strWalSegment, PG_VERSION_94, WAL_VERSION_94_SYS_ID, "$self->{strWalPath}/${strWalSegment}")},
            '(9.4-1, 1e34fa1c833090d94b9bb14f2a8d3153dca6ea27,' .
                " WAL segment ${strWalSegment} already exists in the archive with the same checksum\n" .
                'HINT: this is valid in some recovery scenarios but may also indicate a problem.)',
            "${strWalSegment} WAL found");

        fileRemove("${strWalMajorPath}/${strWalSegmentHash}");

        #---------------------------------------------------------------------------------------------------------------------------
        $strWalSegmentHash = "${strWalSegment}-10be15a0ab8e1653dfab18c83180e74f1507cab1";

        fileStringWrite("${strWalMajorPath}/${strWalSegmentHash}");

        $self->testException(sub {archivePushCheck(
            $self->{oFile}, $strWalSegment, PG_VERSION_94, WAL_VERSION_94_SYS_ID, "$self->{strWalPath}/${strWalSegment}")},
            ERROR_ARCHIVE_DUPLICATE, "WAL segment ${strWalSegment} already exists in the archive");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testException(sub {archivePushCheck(
            $self->{oFile}, $strWalSegment, PG_VERSION_94, WAL_VERSION_94_SYS_ID)},
            ERROR_ASSERT, "strFile is required in File->hash");

        #---------------------------------------------------------------------------------------------------------------------------
        my $strHistoryFile = "00000001.history";

        fileStringWrite("$self->{strArchivePath}/9.4-1/${strHistoryFile}");

        $self->testResult(sub {archivePushCheck(
            $self->{oFile}, $strHistoryFile, PG_VERSION_94, WAL_VERSION_94_SYS_ID, "$self->{strWalPath}/${strHistoryFile}")},
            '(9.4-1, [undef], [undef])', "history file ${strHistoryFile} found");
    }

    ################################################################################################################################
    if ($self->begin("ArchivePushFile::archivePushFile"))
    {
        my $iWalTimeline = 1;
        my $iWalMajor = 1;
        my $iWalMinor = 1;

        $self->optionSetTest($oOption, OPTION_BACKUP_HOST, 'localhost');
        $self->optionSetTest($oOption, OPTION_BACKUP_USER, $self->pgUser());
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        # Create the file object
        my $oRemoteFile = new pgBackRest::File(
            $self->stanza(), $self->{strRepoPath}, protocolGet(BACKUP, undef, {strBackRestBin => $self->backrestExe()}));

        # Generate a normal segment
        my $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        $self->testResult(
            sub {archivePushFile($oRemoteFile, $self->{strWalPath}, $strSegment, false, false)}, '[undef]',
            "${strSegment} WAL segment to remote");

        $self->testResult(
            sub {archivePushFile($oRemoteFile, $self->{strWalPath}, $strSegment, false, false)},
            "WAL segment 000000010000000100000001 already exists in the archive with the same checksum\n" .
                'HINT: this is valid in some recovery scenarios but may also indicate a problem.',
            "${strSegment} WAL duplicate segment to remote");

        # Destroy protocol object
        protocolDestroy();

        $self->optionReset($oOption, OPTION_BACKUP_HOST);
        $self->optionReset($oOption, OPTION_BACKUP_USER);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();
    }

    ################################################################################################################################
    if ($self->begin("ArchivePush->readyList()"))
    {
        my $oPushAsync = new pgBackRest::Archive::ArchivePushAsync($self->{strWalPath}, $self->{strSpoolPath});
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();
        $oPushAsync->initServer();

        my $iWalTimeline = 1;
        my $iWalMajor = 1;
        my $iWalMinor = 1;

        #---------------------------------------------------------------------------------------------------------------------------
        fileStringWrite(
            "$self->{strWalStatusPath}/" . $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++) . '.done');

        $self->testResult(
            sub {$oPushAsync->readyList()}, '()',
            'ignore files without .ready extension');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->walGenerate(
            $self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++));
        $self->walGenerate(
            $self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++));

        $self->testResult(
            sub {$oPushAsync->readyList()}, '(000000010000000100000002, 000000010000000100000003)',
            '.ready files are found');

        fileStringWrite("$self->{strSpoolPath}/000000010000000100000002.ok");
        fileStringWrite("$self->{strSpoolPath}/000000010000000100000003.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->walGenerate(
            $self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++));

        $self->testResult(
            sub {$oPushAsync->readyList()}, '(000000010000000100000004)',
            'new .ready files are found and duplicates ignored');

        fileStringWrite("$self->{strSpoolPath}/000000010000000100000004.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(
            sub {$oPushAsync->readyList()}, '()',
            'no new .ready files returns empty list');

        #---------------------------------------------------------------------------------------------------------------------------
        $iWalTimeline++;
        $iWalMinor = 1;

        fileStringWrite("$self->{strWalStatusPath}/00000002.history.ready");

        $self->testResult(
            sub {$oPushAsync->readyList()}, '(00000002.history)',
            'history .ready file');

        fileStringWrite("$self->{strSpoolPath}/00000002.history.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        fileStringWrite(
            "$self->{strWalStatusPath}/" . $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++) . '.00000028.backup.ready');

        $self->testResult(
            sub {$oPushAsync->readyList()}, '(000000020000000100000001.00000028.backup)',
            'backup .ready file');

        fileStringWrite("$self->{strSpoolPath}/000000020000000100000001.00000028.backup.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        fileRemove("$self->{strWalStatusPath}/00000002.history.ready");

        $self->testResult(
            sub {$oPushAsync->readyList()}, '()', 'remove 00000002.history.ok file');

        $self->testResult(
            sub {fileExists("$self->{strWalStatusPath}/00000002.history.ready")}, false, '00000002.history.ok is removed');
    }

    ################################################################################################################################
    if ($self->begin("ArchivePush->dropList()"))
    {
        my $oPushAsync = new pgBackRest::Archive::ArchivePushAsync($self->{strWalPath}, $self->{strSpoolPath});
        $self->optionSetTest($oOption, OPTION_ARCHIVE_QUEUE_MAX, PG_WAL_SIZE * 4);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        my $iWalTimeline = 1;
        my $iWalMajor = 1;
        my $iWalMinor = 1;

        #---------------------------------------------------------------------------------------------------------------------------
        fileStringWrite(
            "$self->{strWalStatusPath}/" . $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++) . '.ready');
        fileStringWrite(
            "$self->{strWalStatusPath}/" . $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++) . '.ready');
        fileStringWrite(
            "$self->{strWalStatusPath}/" . $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++) . '.ready');

        $self->testResult(
            sub {$oPushAsync->dropList($oPushAsync->readyList())}, '()',
            'WAL files not dropped');

        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionSetTest($oOption, OPTION_ARCHIVE_QUEUE_MAX, PG_WAL_SIZE * 2);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testResult(
            sub {$oPushAsync->dropList($oPushAsync->readyList())},
            '(000000010000000100000001, 000000010000000100000002, 000000010000000100000003)', 'WAL files that exceed queue max');

        # Reset queue max
        $self->optionReset($oOption, OPTION_ARCHIVE_QUEUE_MAX);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();
    }

    ################################################################################################################################
    if ($self->begin("ArchivePushAsync->walStatusWrite() & ArchivePush->walStatus()"))
    {
        my $oPush = new pgBackRest::Archive::ArchivePush();

        my $oPushAsync = new pgBackRest::Archive::ArchivePushAsync($self->{strWalPath}, $self->{strSpoolPath});
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();
        $oPushAsync->initServer();

        my $iWalTimeline = 1;
        my $iWalMajor = 1;
        my $iWalMinor = 1;

        #---------------------------------------------------------------------------------------------------------------------------
        my $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);

        $self->testResult(sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment)}, 0, "${strSegment} WAL no status");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a normal ok
        $oPushAsync->walStatusWrite(WAL_STATUS_OK, $strSegment);

        # Check status
        $self->testResult(sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment)}, 1, "${strSegment} WAL ok");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a bogus warning ok (if content is present there must be two lines)
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        fileStringWrite("$self->{strSpoolPath}/${strSegment}.ok", "Test Warning");

        # Check status
        $self->testException(
            sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment)}, ERROR_ASSERT,
            "${strSegment}.ok content must have at least two lines:\nTest Warning");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a valid warning ok
        $oPushAsync->walStatusWrite(WAL_STATUS_OK, $strSegment, 0, 'Test Warning');

        # Check status
        $self->testResult(sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment)}, 1, "${strSegment} WAL warning ok");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate an invalid error
        $self->testException(
            sub {$oPushAsync->walStatusWrite(WAL_STATUS_ERROR, $strSegment)}, ERROR_ASSERT,
            "error status must have iCode and strMessage set");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate an invalid error
        $self->testException(
            sub {$oPushAsync->walStatusWrite(WAL_STATUS_ERROR, $strSegment, ERROR_ASSERT)}, ERROR_ASSERT,
            "strMessage must be set when iCode is set");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate an invalid error
        fileStringWrite("$self->{strSpoolPath}/${strSegment}.error");

        # Check status (will error because there are now two status files)
        $self->testException(
            sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment);}, ERROR_ASSERT,
            "multiple status files found in " . $self->testPath() . "/repo/archive/db/out for ${strSegment}:" .
            " ${strSegment}.error, ${strSegment}.ok");

        #---------------------------------------------------------------------------------------------------------------------------
        # Remove the ok file
        fileRemove("$self->{strSpoolPath}/${strSegment}.ok");

        # Check status
        $self->testException(
            sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment);}, ERROR_ASSERT, "${strSegment}.error has no content");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a valid error
        $oPushAsync->walStatusWrite(
            WAL_STATUS_ERROR, $strSegment, ERROR_ARCHIVE_DUPLICATE, "WAL segment ${strSegment} already exists in the archive");

        # Check status
        $self->testException(sub {
            $oPush->walStatus($self->{strSpoolPath}, $strSegment)}, ERROR_ARCHIVE_DUPLICATE,
            "WAL segment ${strSegment} already exists in the archive");

        #---------------------------------------------------------------------------------------------------------------------------
        # Change the error file to an ok file
        fileMove("$self->{strSpoolPath}/${strSegment}.error", "$self->{strSpoolPath}/${strSegment}.ok");

        # Check status
        $self->testResult(
            sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment);}, 1,
            "${strSegment} WAL warning ok (converted from .error)");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a normal ok
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $oPushAsync->walStatusWrite(WAL_STATUS_OK, $strSegment);

        #---------------------------------------------------------------------------------------------------------------------------
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);

        # Check status
        $self->testResult(sub {$oPush->walStatus($self->{strSpoolPath}, $strSegment)}, 0, "${strSegment} WAL no status");
    }

    ################################################################################################################################
    if ($self->begin("ArchivePushAsync->process()"))
    {
        my $oPushAsync = new pgBackRest::Archive::ArchivePushAsync(
            $self->{strWalPath}, $self->{strSpoolPath}, $self->backrestExe());
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();
        $oPushAsync->initServer();

        my $iWalTimeline = 1;
        my $iWalMajor = 1;
        my $iWalMinor = 1;

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a normal segment
        my $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        # Generate an error (.ready file withough a corresponding WAL file)
        my $strSegmentError = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        fileStringWrite("$self->{strWalStatusPath}/$strSegmentError.ready");

        # Process and check results
        $self->testResult(sub {$oPushAsync->processQueue()}, '(2, 0, 1, 1)', "process ${strSegment}, ${strSegmentError}");

        $self->testResult(
            sub {fileList($self->{strSpoolPath})}, "(${strSegment}.ok, ${strSegmentError}.error)",
            "${strSegment} pushed, ${strSegmentError} errored");

        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, "${strSegment}-$self->{strWalHash}",
            "${strSegment} WAL in archive");

        $self->testResult(
            sub {fileStringRead("$self->{strSpoolPath}/$strSegmentError.error")},
            ERROR_FILE_OPEN . "\nraised on local-1 host: unable to open $self->{strWalPath}/${strSegmentError}",
            "test ${strSegmentError}.error contents");

        # Remove pushed WAL file
        $self->walRemove($self->{strWalPath}, $strSegment);

        #---------------------------------------------------------------------------------------------------------------------------
        # Fix errored WAL file by providing a valid segment
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegmentError);

        # Process and check results
        $self->testResult(sub {$oPushAsync->processQueue()}, '(1, 0, 1, 0)', "process ${strSegment}, ${strSegmentError}");

        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegmentError)}, "${strSegmentError}-$self->{strWalHash}",
            "${strSegmentError} WAL in archive");

        $self->testResult(sub {fileList($self->{strSpoolPath})}, "${strSegmentError}.ok", "${strSegmentError} pushed");

        #---------------------------------------------------------------------------------------------------------------------------
        # Remove previously errored WAL file
        $self->walRemove($self->{strWalPath}, $strSegmentError);

        # Process and check results
        $self->testResult(sub {$oPushAsync->processQueue()}, '(0, 0, 0, 0)', "remove ${strSegmentError}.ready");

        $self->testResult(sub {fileList($self->{strSpoolPath})}, "[undef]", "${strSegmentError} removed");

        #---------------------------------------------------------------------------------------------------------------------------
        # Enable compression
        $self->optionBoolSetTest($oOption, OPTION_COMPRESS, true);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        # Create history file
        my $strHistoryFile = "00000001.history";

        fileStringWrite("$self->{strWalPath}/${strHistoryFile}");
        fileStringWrite("$self->{strWalStatusPath}/$strHistoryFile.ready");

        # Create backup file
        my $strBackupFile = "${strSegment}.00000028.backup";

        fileStringWrite("$self->{strWalPath}/${strBackupFile}");
        fileStringWrite("$self->{strWalStatusPath}/$strBackupFile.ready");

        # Process and check results
        $self->testResult(sub {$oPushAsync->processQueue()}, '(2, 0, 2, 0)', "end processing ${strHistoryFile}, ${strBackupFile}");

        $self->testResult(
            sub {fileList($self->{strSpoolPath})}, "(${strHistoryFile}.ok, ${strBackupFile}.ok)",
            "${strHistoryFile}, ${strBackupFile} pushed");

        $self->testResult(
            sub {$self->{oFile}->exists(PATH_BACKUP_ARCHIVE, "$self->{strArchiveId}/${strHistoryFile}")}, true,
            "${strHistoryFile} in archive");

        $self->testResult(
            sub {$self->{oFile}->exists(PATH_BACKUP_ARCHIVE, "$self->{strArchiveId}/${strBackupFile}")}, true,
            "${strBackupFile} in archive");

        # Remove history and backup files
        fileRemove("$self->{strWalPath}/${strHistoryFile}");
        fileRemove("$self->{strWalStatusPath}/$strHistoryFile.ready");
        fileRemove("$self->{strWalPath}/${strBackupFile}");
        fileRemove("$self->{strWalStatusPath}/$strBackupFile.ready");

        #---------------------------------------------------------------------------------------------------------------------------
        # Generate a normal segment
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        # Process and check results
        $self->testResult(sub {$oPushAsync->processQueue()}, '(1, 0, 1, 0)', "processing ${strSegment}.gz");

        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, "${strSegment}-$self->{strWalHash}.gz",
            "${strSegment} WAL in archive");

        # Remove the WAL and process so the .ok file is removed
        $self->walRemove($self->{strWalPath}, $strSegment);

        $self->testResult(sub {$oPushAsync->processQueue()}, '(0, 0, 0, 0)', "remove ${strSegment}.ready");

        $self->testResult(sub {fileList($self->{strSpoolPath})}, "[undef]", "${strSegment}.ok removed");

        # Generate the same WAL again
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        # Process and check results
        $self->testResult(sub {$oPushAsync->processQueue()}, '(1, 0, 1, 0)', "processed duplicate ${strSegment}.gz");

        $self->testResult(sub {fileList($self->{strSpoolPath})}, "${strSegment}.ok", "${strSegment} pushed");

        $self->testResult(
            sub {fileStringRead("$self->{strSpoolPath}/${strSegment}.ok")},
            "0\nWAL segment ${strSegment} already exists in the archive with the same checksum\n" .
                'HINT: this is valid in some recovery scenarios but may also indicate a problem.',
            "${strSegment}.ok warning status");

        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, "${strSegment}-$self->{strWalHash}.gz",
            "${strSegment} WAL in archive");

        # Remove the WAL
        $self->walRemove($self->{strWalPath}, $strSegment);

        # Disable compression
        $self->optionBoolSetTest($oOption, OPTION_COMPRESS, false);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->optionSetTest($oOption, OPTION_ARCHIVE_QUEUE_MAX, PG_WAL_SIZE * 2);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        # Generate WAL to test queue limits
        my @strySegment =
        (
            $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++),
            $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++),
            $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++)
        );

        foreach my $strSegment (@strySegment)
        {
            $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);
        }

        # Process and check results
        $self->testResult(sub {$oPushAsync->processQueue()}, '(3, 3, 1, 0)', "process and drop files");

        $self->testResult(
            sub {fileList($self->{strSpoolPath})}, '(' . join('.ok, ', @strySegment) . '.ok)',
            join(', ', @strySegment) . " ok drop files written");

        foreach my $strSegment (@strySegment)
        {
            $self->testResult(
                sub {fileStringRead("$self->{strSpoolPath}/${strSegment}.ok")},
                $strSegment eq $strySegment[0] ? '' :
                    "0\ndropped WAL file ${strSegment} because archive queue exceeded " . optionGet(OPTION_ARCHIVE_QUEUE_MAX) .
                        ' bytes',
                "verify ${strSegment} status");

            $self->walRemove($self->{strWalPath}, $strSegment);
        }

        $self->optionReset($oOption, OPTION_ARCHIVE_QUEUE_MAX);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        #---------------------------------------------------------------------------------------------------------------------------
        $self->testResult(sub {$oPushAsync->processQueue()}, '(0, 0, 0, 0)', "final process to remove ok files");

        $self->testResult(sub {fileList($self->{strSpoolPath})}, "[undef]", "ok files removed");
    }

    ################################################################################################################################
    if ($self->begin("ArchivePush->process()"))
    {
        my $oPush = new pgBackRest::Archive::ArchivePush($self->backrestExe());

        my $iWalTimeline = 1;
        my $iWalMajor = 1;
        my $iWalMinor = 1;

        my $iProcessId = $PID;

        #---------------------------------------------------------------------------------------------------------------------------
        # Set db-host to trick archive-push into thinking it is running on the backup server
        $self->optionSetTest($oOption, OPTION_DB_HOST, BOGUS);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testException(sub {$oPush->process(undef)}, ERROR_HOST_INVALID, 'archive-push operation must run on db host');

        #---------------------------------------------------------------------------------------------------------------------------
        # Reset db-host
        $self->optionReset($oOption, OPTION_DB_HOST);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testException(sub {$oPush->process(undef)}, ERROR_PARAM_REQUIRED, 'WAL file to push required');

        #---------------------------------------------------------------------------------------------------------------------------
        my $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        $self->testResult(sub {$oPush->process("pg_xlog/${strSegment}")}, 0, "${strSegment} WAL pushed (with relative path)");

        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, "${strSegment}-$self->{strWalHash}",
            "${strSegment} WAL in archive");

        $self->walRemove($self->{strWalPath}, $strSegment);

        #---------------------------------------------------------------------------------------------------------------------------
        # Set unrealistic queue max to make synchronous push drop a WAL
        $self->optionSetTest($oOption, OPTION_ARCHIVE_QUEUE_MAX, 0);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        $self->testResult(sub {$oPush->process("$self->{strWalPath}/${strSegment}")}, 0, "${strSegment} WAL dropped");
        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, '[undef]',
            "${strSegment} WAL in archive");

        # Set more realistic queue max and allow segment to push
        $self->optionSetTest($oOption, OPTION_ARCHIVE_QUEUE_MAX, PG_WAL_SIZE * 4);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testResult(sub {$oPush->process("$self->{strWalPath}/${strSegment}")}, 0, "${strSegment} WAL pushed");
        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, "${strSegment}-$self->{strWalHash}",
            "${strSegment} WAL in archive");

        $self->walRemove($self->{strWalPath}, $strSegment);

        # Reset queue max
        $self->optionReset($oOption, OPTION_ARCHIVE_QUEUE_MAX);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        #---------------------------------------------------------------------------------------------------------------------------
        # Enable async archiving
        $self->optionBoolSetTest($oOption, OPTION_ARCHIVE_ASYNC, true);
        $self->optionSetTest($oOption, OPTION_SPOOL_PATH, $self->{strRepoPath});
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        # Write an OK file so the async process is not actually started
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        filePathCreate($self->{strSpoolPath}, undef, undef, true);
        fileStringWrite("$self->{strSpoolPath}/${strSegment}.ok");

        $self->testResult(
            sub {$oPush->process("$self->{strWalPath}/${strSegment}")}, 0,
            "${strSegment} WAL pushed async from synthetic ok file");

        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, '[undef]',
            "${strSegment} WAL not in archive");

        #---------------------------------------------------------------------------------------------------------------------------
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        $self->testResult(sub {$oPush->process("$self->{strWalPath}/${strSegment}")}, 0, "${strSegment} WAL pushed async");
        exit if ($iProcessId != $PID);

        $self->testResult(
            sub {walSegmentFind($self->{oFile}, $self->{strArchiveId}, $strSegment)}, "${strSegment}-$self->{strWalHash}",
            "${strSegment} WAL in archive");

        $self->walRemove($self->{strWalPath}, $strSegment);

        #---------------------------------------------------------------------------------------------------------------------------
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);

        $self->optionSetTest($oOption, OPTION_ARCHIVE_TIMEOUT, 1);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testException(
            sub {$oPush->process("$self->{strWalPath}/${strSegment}")}, ERROR_ARCHIVE_TIMEOUT,
            "unable to push WAL ${strSegment} asynchronously after 1 second(s)");
        exit if ($iProcessId != $PID);

        #---------------------------------------------------------------------------------------------------------------------------
        $strSegment = $self->walSegment($iWalTimeline, $iWalMajor, $iWalMinor++);
        $self->walGenerate($self->{oFile}, $self->{strWalPath}, WAL_VERSION_94, 1, $strSegment);

        $self->optionSetTest($oOption, OPTION_BACKUP_HOST, BOGUS);
        $self->optionSetTest($oOption, OPTION_PROTOCOL_TIMEOUT, 60);
        $self->optionSetTest($oOption, OPTION_ARCHIVE_TIMEOUT, 5);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();

        $self->testException(
            sub {$oPush->process("$self->{strWalPath}/${strSegment}")}, ERROR_HOST_CONNECT,
            "remote process terminated on local-1 host: remote process terminated on bogus host.*");
        exit if ($iProcessId != $PID);

        # Disable async archiving
        $self->optionReset($oOption, OPTION_ARCHIVE_ASYNC);
        $self->optionReset($oOption, OPTION_SPOOL_PATH);
        logDisable(); $self->configLoadExpect(dclone($oOption), CMD_ARCHIVE_PUSH); logEnable();
    }
}

1;
