####################################################################################################################################
# STANZA MODULE
#
# Contains functions for adding, upgrading and removing a stanza.
####################################################################################################################################
package pgBackRest::Stanza;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::BackupInfo;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::InfoCommon;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;

####################################################################################################################################
# Global variables
####################################################################################################################################
my $strStanzaCreateErrorMsg = "not empty\n" .
    "HINT: Use --force to force the stanza data to be created.";

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->new');

    # Initialize the database object
    $self->{oDb} = dbMasterGet();
    $self->dbInfoGet();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# Process Stanza Commands
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    my $iResult = 0;

    # Process stanza create
    if (commandTest(CMD_STANZA_CREATE))
    {
        $iResult = $self->stanzaCreate();
    }
    # Process stanza upgrade
    elsif (commandTest(CMD_STANZA_UPGRADE))
    {
        $iResult = $self->stanzaUpgrade();
    }
    # Else error if any other command is found
    else
    {
        confess &log(ASSERT, "stanza->process() called with invalid command: " . commandGet());
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true}
    );
}

####################################################################################################################################
# stanzaCreate
#
# Creates the required data for the stanza.
####################################################################################################################################
sub stanzaCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->stanzaCreate');

    # Initialize default file object with protocol set to NONE meaning strictly local
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(NONE)
    );

    # Get the parent paths (create if not exist)
    my $strParentPathArchive = $self->parentPathGet($oFile, PATH_BACKUP_ARCHIVE);
    my $strParentPathBackup = $self->parentPathGet($oFile, PATH_BACKUP_CLUSTER);

    # Get a listing of files in the directory, ignoring if any are missing
    my @stryFileListArchive = fileList($strParentPathArchive, undef, 'forward', true);
    my @stryFileListBackup = fileList($strParentPathBackup, undef, 'forward', true);

    # If force not used and at least one directory is not empty, then check to see if the info files exist
    if (!optionGet(OPTION_FORCE) && (@stryFileListArchive || @stryFileListBackup))
    {
        my $strBackupInfoFile = &FILE_BACKUP_INFO;
        my $strArchiveInfoFile = &ARCHIVE_INFO_FILE;

        # If either info file is not in the file list, then something exists in the directories so need to use force option
        if (@stryFileListBackup && !grep(/^$strBackupInfoFile/i, @stryFileListBackup)
            || @stryFileListArchive && !grep(/^$strArchiveInfoFile/i, @stryFileListArchive))
        {
            confess &log(ERROR,
                (@stryFileListBackup ? 'backup directory ' : '') .
                ((@stryFileListBackup && @stryFileListArchive) ? 'and/or ' : '') .
                (@stryFileListArchive ? 'archive directory ' : '') .
                $strStanzaCreateErrorMsg, ERROR_PATH_NOT_EMPTY);
        }
    }

    # Create the archive.info file and local variables
    my ($iResult, $strResultMessage) =
        $self->infoFileCreate((new pgBackRest::Archive::ArchiveInfo($strParentPathArchive, false)), $oFile,
            PATH_BACKUP_ARCHIVE, $strParentPathArchive, \@stryFileListArchive);

    if ($iResult == 0)
    {
        # Create the backup.info file
        ($iResult, $strResultMessage) =
            $self->infoFileCreate((new pgBackRest::BackupInfo($strParentPathBackup, false, false)), $oFile,
                PATH_BACKUP_CLUSTER, $strParentPathBackup, \@stryFileListBackup);
    }

    if ($iResult != 0)
    {
        &log(WARN, "unable to create stanza '" . optionGet(OPTION_STANZA) . "'");
        &log(ERROR, $strResultMessage, $iResult);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true}
    );
}

####################################################################################################################################
# stanzaUpgrade
#
#
####################################################################################################################################
sub stanzaUpgrade
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->stanzaUpgrade');

    # Initialize default file object with protocol set to NONE meaning strictly local
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(NONE)
    );

    # Get the archive info and backup info files; if either does not exist an error will be thrown
    my $oArchiveInfo = new pgBackRest::Archive::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE));
    my $oBackupInfo = new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER));

    # If the DB section does not match, then upgrade
    if ($self->upgradeCheck($oBackupInfo, PATH_BACKUP_CLUSTER, ERROR_BACKUP_MISMATCH) ||
        $self->upgradeCheck($oArchiveInfo, PATH_BACKUP_ARCHIVE, ERROR_ARCHIVE_MISMATCH))
    {
        $oBackupInfo->dbSectionSet($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion},
            $self->{oDb}{ullDbSysId}, $oBackupInfo->dbHistoryIdGet() + 1);
        $oBackupInfo->save();

        $oArchiveInfo->dbSectionSet($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId}, $oArchiveInfo->dbHistoryIdGet() + 1);
        $oArchiveInfo->save();
    }
    else
    {
        &log(INFO, "the stanza data is already up to date");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => 0, trace => true}
    );
}

####################################################################################################################################
# parentPathGet
#
# Creates the parent path if it doesn't exist and returns the path.
####################################################################################################################################
sub parentPathGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strPathType,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->parentPathGet', \@_,
            {name => 'oFile', trace => true},
            {name => 'strPathType', trace => true},
        );

    my $strParentPath = $oFile->pathGet($strPathType);

    # If the info path does not exist, create it
    if (!fileExists($strParentPath))
    {
        # Create the cluster repo path
        $oFile->pathCreate($strPathType, undef, undef, true, true);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strParentPath', value => $strParentPath},
    );
}

####################################################################################################################################
# infoFileCreate
#
# Creates the info file based on the data passed to the function
####################################################################################################################################
sub infoFileCreate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oInfo,
        $oFile,
        $strPathType,
        $strParentPath,
        $stryFileList,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoFileCreate', \@_,
            {name => 'oInfo', trace => true},
            {name => 'oFile', trace => true},
            {name => 'strPathType'},
            {name => 'strParentPath'},
            {name => 'stryFileList'},
        );

    my $iResult = 0;
    my $strResultMessage = undef;
    my $strWarningMsgArchive = undef;
    my $bSave = true;


    # If force was not used and the info file does not exist and the directory is not empty, then error
    # This should also be performed by the calling routine before this function is called, so this is just a safety check
    if (!optionGet(OPTION_FORCE) && !$oInfo->{bExists} && @$stryFileList)
    {
        confess &log(ERROR, ($strPathType eq PATH_BACKUP_CLUSTER ? 'backup directory ' : 'archive directory ') .
            $strStanzaCreateErrorMsg, ERROR_PATH_NOT_EMPTY);
    }

    # Turn off console logging to control when to display the error
    logLevelSet(undef, OFF);

    eval
    {
        # Reconstruct the file from the data in the directory if there is any
        if ($strPathType eq PATH_BACKUP_CLUSTER)
        {
            $oInfo->reconstruct(false, false);
        }
        # If this is the archive.info reconstruction then catch any warnings
        else
        {
            $strWarningMsgArchive = $oInfo->reconstruct($oFile, $self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId});
        }

        my $hDbList = $oInfo->dbHistoryList();
        my $iDbId = 0;
        my $iDbIdMax = 0;

        # Get the db-id from the reconstructed history that corresponds to the current db-version and db-system-id
        foreach my $iDbHistoryId (keys %{$hDbList})
        {
            if ($$hDbList{$iDbHistoryId}{&INFO_SYSTEM_ID} == $self->{oDb}{ullDbSysId} &&
                $$hDbList{$iDbHistoryId}{&INFO_DB_VERSION} eq $self->{oDb}{strDbVersion})
            {
                $iDbId = $iDbHistoryId;
            }

            # If the current history ID is greater than the running max, then set it to the current id
            if ($iDbHistoryId > $iDbIdMax)
            {
                $iDbIdMax = $iDbHistoryId;
            }

        }

        # If a corresponding db-id was not found, then check to see if there was any history
        if ($iDbId == 0)
        {
            # If there is history then get the last key and increment it to set the proper key for the DB section
            if ($iDbIdMax > 0)
            {
                $iDbId = $iDbIdMax + 1;
            }
            # Else initialize the db-id
            else
            {
                $iDbId = 1;
            }
        }

        # Make sure the db section is properly up to date
        ($strPathType eq PATH_BACKUP_CLUSTER)
            ? $oInfo->dbSectionSet($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion},
                $self->{oDb}{ullDbSysId}, $iDbId)
            : $oInfo->dbSectionSet($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId}, $iDbId);


        # If the file exists on disk, then check if the reconstructed data is the same as what is on disk
        if ($oInfo->{bExists})
        {
            my $oInfoOnDisk =
                ($strPathType eq PATH_BACKUP_CLUSTER ? new pgBackRest::BackupInfo($strParentPath)
                : new pgBackRest::Archive::ArchiveInfo($strParentPath));

            # If force was not used and the hashes are different then error
            if ($oInfoOnDisk->hash() ne $oInfo->hash())
            {
                if (!optionGet(OPTION_FORCE))
                {
                    $iResult = ERROR_FILE_INVALID;
                    $strResultMessage =
                        ($strPathType eq PATH_BACKUP_CLUSTER ? 'backup file ' : 'archive file ') .
                        ' invalid; to correct, use --force';
                }
            }
            # If the hashes are the same, then don't save the file since it already exists and is valid
            else
            {
                $bSave = false;
            }
        }

        if ($iResult == 0)
        {
            # Save the reconstructed file
            if ($bSave)
            {
                $oInfo->save();
            }

            # Sync path if requested
            if (optionGet(OPTION_REPO_SYNC))
            {
                $oFile->pathSync(
                    PATH_BACKUP_ABSOLUTE,
                    defined($oInfo->{strArchiveClusterPath}) ? $oInfo->{strArchiveClusterPath} : $oInfo->{strBackupClusterPath});
            }
        }

        return true;
    }
    or do
    {
        # Confess unhandled errors
        confess $EVAL_ERROR if (!isException($EVAL_ERROR));

        # If this is a backrest error then capture the last code and message
        $iResult = $EVAL_ERROR->code();
        $strResultMessage = $EVAL_ERROR->message();
    };

    # Reset the console logging
    logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));

    # If a warning was issued, raise it
    if (defined($strWarningMsgArchive))
    {
        &log(WARN, $strWarningMsgArchive);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult},
        {name => 'strResultMessage', value => $strResultMessage},
    );
}

####################################################################################################################################
# dbInfoGet
#
# Gets the database information and store it in $self
####################################################################################################################################
sub dbInfoGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->dbInfoGet');

    # Validate the database configuration. Do not require the database to be online before creating a stanza because the
    # archive_command will attempt to push an achive before the archive.info file exists which will result in an error in the
    # postgres logs.
    if (optionGet(OPTION_ONLINE))
    {
        # If the db-path in pgbackrest.conf does not match the pg_control then this will error alert the user to fix pgbackrest.conf
        $self->{oDb}->configValidate();
    }

    ($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion}, $self->{oDb}{ullDbSysId})
        = $self->{oDb}->info();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# upgradeCheck
#
# Checks the info file to see if an upgrade is necessary.
####################################################################################################################################
sub upgradeCheck
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oInfo,
        $strPathType,
        $iExpectedError,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->upgradeCheck', \@_,
            {name => 'oInfo', trace => true},
            {name => 'strPathType'},
            {name => 'iExpectedError'},
        );

    my $iResult = 0;
    my $strResultMessage = undef;

    # Turn off console logging to control when to display the error
    logLevelSet(undef, OFF);

    eval
    {
        ($strPathType eq PATH_BACKUP_CLUSTER)
            ? $oInfo->check($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion},
                $self->{oDb}{ullDbSysId}, true)
            : $oInfo->check($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId}, true);
        return true;
    }
    or do
    {
        # Confess unhandled errors
        confess $EVAL_ERROR if (!isException($EVAL_ERROR) || $EVAL_ERROR->code() != $iExpectedError);

        # If this is a backrest error then capture the last code and message
        $iResult = $EVAL_ERROR->code();
        $strResultMessage = $EVAL_ERROR->message();
    };

    # Reset the console logging
    logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));

    # Throw an error if the result is not the expected value passed
    if ($iResult != 0 && $iResult != $iExpectedError)
    {
        confess &log(ERROR, $strResultMessage, $iResult);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => ($iResult != 0 ? true : false)},
    );
}

1;
