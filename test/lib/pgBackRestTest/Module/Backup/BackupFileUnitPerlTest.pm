####################################################################################################################################
# Tests for Backup File module
####################################################################################################################################
package pgBackRestTest::Module::Backup::BackupFileUnitPerlTest;
use parent 'pgBackRestTest::Env::HostEnvTest';

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Storable qw(dclone);

use pgBackRest::Backup::File;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;
use pgBackRest::Storage::Helper;

use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::RunTest;
use pgBackRestTest::Env::Host::HostBackupTest;

####################################################################################################################################
# initModule
####################################################################################################################################
sub initModule
{
    my $self = shift;

    $self->{strDbPath} = $self->testPath() . '/db';
    $self->{strRepoPath} = $self->testPath() . '/repo';
    $self->{strBackupPath} = "$self->{strRepoPath}/backup/" . $self->stanza();
}

####################################################################################################################################
# initTest
####################################################################################################################################
sub initTest
{
    my $self = shift;

    # Create backup path
    storageTest()->pathCreate($self->{strBackupPath}, {bIgnoreExists => true, bCreateParent => true});

    # Generate pg_control file
    storageTest()->pathCreate($self->{strDbPath} . '/' . DB_PATH_GLOBAL, {bCreateParent => true});
    $self->controlGenerate($self->{strDbPath}, PG_VERSION_94);
}

####################################################################################################################################
# run
####################################################################################################################################
sub run
{
    my $self = shift;

    $self->optionTestSet(CFGOPT_STANZA, $self->stanza());
    $self->optionTestSet(CFGOPT_PG_PATH, $self->{strDbPath});
    $self->optionTestSet(CFGOPT_REPO_PATH, $self->{strRepoPath});
    $self->optionTestSet(CFGOPT_LOG_PATH, $self->testPath());

    $self->optionTestSetBool(CFGOPT_ONLINE, false);

    $self->optionTestSet(CFGOPT_DB_TIMEOUT, 5);
    $self->optionTestSet(CFGOPT_PROTOCOL_TIMEOUT, 6);
	$self->optionTestSet(CFGOPT_COMPRESS_LEVEL, 3);

	$self->configTestLoad(CFGCMD_BACKUP);

    ################################################################################################################################
    if ($self->begin('backupFile()'))
    {
# $strDbFile,                                 # Database file to backup
# $strRepoFile,                               # Location in the repository to copy to
# $lSizeFile,                                 # File size
# $strChecksum,                               # File checksum to be checked
# $bChecksumPage,                             # Should page checksums be calculated?
# $strBackupLabel,                            # Label of current backup
# $bCompress,                                 # Compress destination file
# $iCompressLevel,                            # Compress level
# $lModificationTime,                         # File modification time
# $bIgnoreMissing,                            # Is it OK if the file is missing?
# $hExtraParam,                               # Parameter to pass to the extra function
# $bDelta,                                    # Is the detla option on?
# $bHasReference,                             # Does a reference exist to the file in a prior backup in the set?
# $strCipherPass,                             # Passphrase to access the repo file (undefined if repo not encrypted). This
#                                             # parameter must always be last in the parameter list to this function.

		# Result variables
        my $iResultCopyResult;
        my $lResultCopySize;
        my $lResultRepoSize;
        my $strResultCopyChecksum;
        my $rResultExtra;

		# Repo
		my $strRepoBackupPath = storageRepo()->pathGet(STORAGE_REPO_BACKUP);
        my $strBackupLabel = "20180724-182750F";

		# File
        my $strFileName = "12345";
        my $strFileDb = $self->{strDbPath} . "/$strFileName";
        my $strFileHash = '1c7e00fd09b9dd11fc2966590b3e3274645dd031';
		my $strFileRepo = storageRepo()->pathGet(
			STORAGE_REPO_BACKUP . "/$strBackupLabel/" . MANIFEST_TARGET_PGDATA . "/$strFileName");

		# Copy file to db path
        executeTest('cp ' . $self->dataPath() . "/filecopy.archive2.bin ${strFileDb}");

		# Get size and data info for the file in the db pat
        my $hManifest = storageDb()->manifest($self->{strDbPath});
        my $lFileSize = $hManifest->{$strFileName}{size} + 0;
        my $lFileTime = $hManifest->{$strFileName}{modification_time} + 0;

		#---------------------------------------------------------------------------------------------------------------------------
		# No prior checksum, no compression, no page checksum, no extra, no delta, no hasReference
		($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
			backupFile($strFileDb, MANIFEST_TARGET_PGDATA . "/$strFileName", $lFileSize, undef, false, $strBackupLabel, false,
			cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, false, false, undef);

		$self->testResult(sub {storageTest()->exists($strFileRepo)}, true, 'non-compressed file exists in repo');

		$self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
			$lResultRepoSize == $lFileSize), true, 'file copied to repo successfully');

        $self->testException(sub {storageRepo()->openRead("$strFileRepo.gz")}, ERROR_FILE_MISSING,
            "unable to open '$strFileRepo.gz': No such file or directory");

		storageTest()->remove($strFileRepo);

		#---------------------------------------------------------------------------------------------------------------------------
		# No prior checksum, yes compression, yes page checksum, no extra, no delta, no hasReference
		$self->testException(sub {backupFile($strFileDb, MANIFEST_TARGET_PGDATA . "/$strFileName", $lFileSize, undef, true,
			$strBackupLabel, true, cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, false, false, undef)}, ERROR_ASSERT,
			"iWalId is required in Backup::Filter::PageChecksum->new");

	    # Build the lsn start parameter to pass to the extra function
	    my $hStartLsnParam =
	    {
	        iWalId => 0xFFFF,
	        iWalOffset => 0xFFFF,
	    };

		#---------------------------------------------------------------------------------------------------------------------------
		# No prior checksum, yes compression, yes page checksum, yes extra, no delta, no hasReference
		($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
			backupFile($strFileDb, MANIFEST_TARGET_PGDATA . "/$strFileName", $lFileSize, undef, true, $strBackupLabel, true,
			cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, $hStartLsnParam, false, false, undef);

		$self->testResult(sub {storageTest()->exists("$strFileRepo.gz")}, true, 'compressed file exists in repo');

		$self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
			$lResultRepoSize < $lFileSize && $rResultExtra->{bValid}), true, 'file copied to repo successfully');

        $self->testException(sub {storageRepo()->openRead("$strFileRepo")}, ERROR_FILE_MISSING,
            "unable to open '$strFileRepo': No such file or directory");

		storageTest()->remove("$strFileRepo.gz");

		#---------------------------------------------------------------------------------------------------------------------------
		# Add a segment number for bChecksumPage code coverage
		executeTest('cp ' . "$strFileDb $strFileDb.1");

		# No prior checksum, no compression, yes page checksum, yes extra, no delta, no hasReference
		($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
			backupFile("$strFileDb.1", MANIFEST_TARGET_PGDATA . "/$strFileName.1", $lFileSize, undef, true, $strBackupLabel, false,
			cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, $hStartLsnParam, false, false, undef);

		$self->testResult(sub {storageTest()->exists("$strFileRepo.1")}, true, 'non-compressed segment file exists in repo');

		$self->testResult(($iResultCopyResult == BACKUP_FILE_COPY && $strResultCopyChecksum eq $strFileHash &&
			$lResultRepoSize == $lFileSize && $rResultExtra->{bValid}), true, 'segment file copied to repo successfully');

		# Remove the db file and try to back it up
		storageTest()->remove("$strFileDb.1");

		# No prior checksum, no compression, no page checksum, no extra, no delta, no hasReference
		($iResultCopyResult, $lResultCopySize, $lResultRepoSize, $strResultCopyChecksum, $rResultExtra) =
			backupFile("$strFileDb.1", MANIFEST_TARGET_PGDATA . "/$strFileName.1", $lFileSize, undef, false, $strBackupLabel, false,
			cfgOption(CFGOPT_COMPRESS_LEVEL), $lFileTime, true, undef, false, false, undef);

		$self->testResult(($iResultCopyResult == BACKUP_FILE_SKIP && !defined($strResultCopyChecksum) &&
			!defined($lResultRepoSize)), true, 'backup file skipped');
    }

    ################################################################################################################################
    # if ($self->begin('backupManifestUpdate()'))
    # {
    #
    # }
}

1;
