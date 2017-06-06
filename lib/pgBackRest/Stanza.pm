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
use pgBackRest::Backup::Info;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::InfoCommon;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;

####################################################################################################################################
# Global variables
####################################################################################################################################
my $strHintForce = "\nHINT: use stanza-create --force to force the stanza data to be created.";
my $strInfoMissing = " information missing";
my $strStanzaCreateErrorMsg = "not empty" . $strHintForce;

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

    # Get the parent paths (create if not exist)
    my $strParentPathArchive = $self->parentPathGet(STORAGE_REPO_ARCHIVE);
    my $strParentPathBackup = $self->parentPathGet(STORAGE_REPO_BACKUP);

    # Get a listing of files in the directory, ignoring if any are missing
    my @stryFileListArchive = storageRepo()->list($strParentPathArchive, {bIgnoreMissing => true});
    my @stryFileListBackup = storageRepo()->list($strParentPathBackup, {bIgnoreMissing => true});

    # If force not used, then if files exist force should be required since create must have already occurred and reissuing a create
    # needs to be a consciuos effort to rewrite the files
    if (!optionGet(OPTION_FORCE))
    {
        # At least one directory is not empty, then check to see if the info files exist
        if (@stryFileListArchive || @stryFileListBackup)
        {
            my $strBackupInfoFile = &FILE_BACKUP_INFO;
            my $strArchiveInfoFile = &ARCHIVE_INFO_FILE;

            my $bBackupInfoFileExists = grep(/^$strBackupInfoFile$/i, @stryFileListBackup);
            my $bArchiveInfoFileExists = grep(/^$strArchiveInfoFile$/i, @stryFileListArchive);

            # If the info file exists in one directory but is missing from the other directory then there is clearly a mismatch
            # which requires force option
            if (!$bArchiveInfoFileExists && $bBackupInfoFileExists)
            {
                confess &log(ERROR, 'archive' . $strInfoMissing . $strHintForce, ERROR_FILE_MISSING);
            }
            elsif (!$bBackupInfoFileExists && $bArchiveInfoFileExists)
            {
                confess &log(ERROR, 'backup' . $strInfoMissing . $strHintForce, ERROR_FILE_MISSING);
            }
            # If we get here then either both exist or neither exist so if neither file exists then something still exists in the
            # directories since one or both of them are not empty so need to use force option
            elsif (!$bArchiveInfoFileExists)
            {
                confess &log(ERROR,
                    (@stryFileListBackup ? 'backup directory ' : '') .
                    ((@stryFileListBackup && @stryFileListArchive) ? 'and/or ' : '') .
                    (@stryFileListArchive ? 'archive directory ' : '') .
                    $strStanzaCreateErrorMsg, ERROR_PATH_NOT_EMPTY);
            }
        }
    }

    # Instantiate the info objects. Throws an error and aborts if force not used and an error occurs during instantiation.
    my $oArchiveInfo = $self->infoObject(STORAGE_REPO_ARCHIVE, $strParentPathArchive, {bRequired => false, bIgnoreMissing => true});
    my $oBackupInfo = $self->infoObject(STORAGE_REPO_BACKUP, $strParentPathBackup, {bRequired => false, bIgnoreMissing => true});

    # Create the archive info object
    my ($iResult, $strResultMessage) =
        $self->infoFileCreate($oArchiveInfo, STORAGE_REPO_ARCHIVE, $strParentPathArchive, \@stryFileListArchive);

    if ($iResult == 0)
    {
        # Create the backup.info file
        ($iResult, $strResultMessage) =
            $self->infoFileCreate($oBackupInfo, STORAGE_REPO_BACKUP, $strParentPathBackup, \@stryFileListBackup);
    }

    if ($iResult != 0)
    {
        &log(WARN, "unable to create stanza '" . optionGet(OPTION_STANZA) . "'");
        confess &log(ERROR, $strResultMessage, $iResult);
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
# Updates stanza information to reflect new cluster information.  Normally used for version upgrades, but could be used after a
# cluster has been dumped and restored to the same version.
####################################################################################################################################
sub stanzaUpgrade
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->stanzaUpgrade');

    # Get the archive info and backup info files; if either does not exist an error will be thrown
    my $oArchiveInfo = $self->infoObject(STORAGE_REPO_ARCHIVE, storageRepo()->pathGet(STORAGE_REPO_ARCHIVE));
    my $oBackupInfo = $self->infoObject(STORAGE_REPO_BACKUP, storageRepo()->pathGet(STORAGE_REPO_BACKUP));
    my $bBackupUpgraded = false;
    my $bArchiveUpgraded = false;

    # If the DB section does not match, then upgrade
    if ($self->upgradeCheck($oBackupInfo, STORAGE_REPO_BACKUP, ERROR_BACKUP_MISMATCH))
    {
        # Reconstruct the file and save it
        my ($bReconstruct, $strWarningMsgArchive) = $oBackupInfo->reconstruct(false, false, $self->{oDb}{strDbVersion},
            $self->{oDb}{ullDbSysId}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion});
        $oBackupInfo->save();
        $bBackupUpgraded = true;
    }

    if ($self->upgradeCheck($oArchiveInfo, STORAGE_REPO_ARCHIVE, ERROR_ARCHIVE_MISMATCH))
    {
        # Reconstruct the file and save it
        my ($bReconstruct, $strWarningMsgArchive) = $oArchiveInfo->reconstruct($self->{oDb}{strDbVersion},
            $self->{oDb}{ullDbSysId});
        $oArchiveInfo->save();
        $bArchiveUpgraded = true;
    }

    # If neither file needed upgrading then provide informational message that an upgrade was not necessary
    if (!($bBackupUpgraded || $bArchiveUpgraded))
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
        $strPathType,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->parentPathGet', \@_,
            {name => 'strPathType', trace => true},
        );

    my $strParentPath = storageRepo()->pathGet($strPathType);

    # If the info path does not exist, create it
    if (!storageRepo()->pathExists($strParentPath))
    {
        # Create the cluster repo path
        storageRepo()->pathCreate($strPathType, {bIgnoreExists => true, bCreateParent => true});
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strParentPath', value => $strParentPath},
    );
}

####################################################################################################################################
# infoObject
#
# Attempt to load an info object. Ignores missing files if directed. Throws an error and aborts if force not used and an error
# occurs during loading, else instatiates the object without loading it.
####################################################################################################################################
sub infoObject
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strParentPath,
        $bRequired,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoObject', \@_,
            {name => 'strPathType'},
            {name => 'strParentPath'},
            {name => 'bRequired', optional => true, default => true},
            {name => 'bIgnoreMissing', optional => true, default => false},
        );

    my $iResult = 0;
    my $strResultMessage;
    my $oInfo;

    # Turn off console logging to control when to display the error
    logDisable();

    # Instantiate the info object in an eval block to trap errors. If force is not used and an error occurs, throw the error
    # along with a directive that force will need to be used to attempt to correct the issue
    eval
    {
        # Ignore missing files if directed but if the info or info.copy file exists the exists flag will still be set and data will
        # attempt to be loaded
        $oInfo = ($strPathType eq STORAGE_REPO_BACKUP ?
            new pgBackRest::Backup::Info($strParentPath, false, $bRequired, {bIgnoreMissing => $bIgnoreMissing}) :
            new pgBackRest::Archive::ArchiveInfo($strParentPath, $bRequired, {bIgnoreMissing => $bIgnoreMissing}));

        # Reset the console logging
        logEnable();
        return true;
    }
    or do
    {
        # Reset console logging and capture error information
        logEnable();
        $iResult = exceptionCode($EVAL_ERROR);
        $strResultMessage = exceptionMessage($EVAL_ERROR->message());
    };

    if ($iResult != 0)
    {
        # If force was not used, and the file is missing, then confess the error with hint to use force if the option is
        # configurable (force is not configurable for stanza-upgrade so this will always confess errors on stanza-upgrade)
        # else confess all other errors
        if ((optionValid(OPTION_FORCE) && !optionGet(OPTION_FORCE)) ||
            (!optionValid(OPTION_FORCE)))
        {
            if ($iResult == ERROR_FILE_MISSING)
            {
                confess &log(ERROR, (optionValid(OPTION_FORCE) ? $strResultMessage . $strHintForce : $strResultMessage), $iResult);
            }
            else
            {
                confess &log(ERROR, $strResultMessage, $iResult);
            }
        }
        # Else instatiate the object without loading it so we can reconstruct and overwrite the invalid files
        else
        {
            $oInfo = ($strPathType eq STORAGE_REPO_BACKUP ?
                new pgBackRest::Backup::Info($strParentPath, false, false, {bLoad => false}) :
                new pgBackRest::Archive::ArchiveInfo($strParentPath, false, {bLoad => false}));
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oInfo', value => $oInfo},
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
        $strPathType,
        $strParentPath,
        $stryFileList,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoFileCreate', \@_,
            {name => 'oInfo', trace => true},
            {name => 'strPathType'},
            {name => 'strParentPath'},
            {name => 'stryFileList'},
        );

    my $iResult = 0;
    my $strResultMessage = undef;
    my $strWarningMsgArchive = undef;

    # If force was not used and the info file does not exist and the directory is not empty, then error
    # This should also be performed by the calling routine before this function is called, so this is just a safety check
    if (!optionGet(OPTION_FORCE) && !$oInfo->{bExists} && @$stryFileList)
    {
        confess &log(ERROR, ($strPathType eq STORAGE_REPO_BACKUP ? 'backup directory ' : 'archive directory ') .
            $strStanzaCreateErrorMsg, ERROR_PATH_NOT_EMPTY);
    }

    # Turn off console logging to control when to display the error
    logDisable();

    eval
    {
        # Reconstruct the file from the data in the directory if there is any else initialize the file
        if ($strPathType eq STORAGE_REPO_BACKUP)
        {
            $oInfo->reconstruct(false, false, $self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId}, $self->{oDb}{iControlVersion},
                $self->{oDb}{iCatalogVersion});
        }
        # If this is the archive.info reconstruction then catch any warnings
        else
        {
            $strWarningMsgArchive = $oInfo->reconstruct($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId});
        }

        # If the file exists on disk, then check if the reconstructed data is the same as what is on disk
        if ($oInfo->exists())
        {
            my $oInfoOnDisk =
                ($strPathType eq STORAGE_REPO_BACKUP ?
                    new pgBackRest::Backup::Info($strParentPath) : new pgBackRest::Archive::ArchiveInfo($strParentPath));

            # If the hashes are not the same
            if ($oInfoOnDisk->hash() ne $oInfo->hash())
            {
                # If force was not used and the hashes are different then error
                if (!optionGet(OPTION_FORCE))
                {
                    $iResult = ERROR_FILE_INVALID;
                    $strResultMessage =
                        ($strPathType eq STORAGE_REPO_BACKUP ? 'backup info file ' : 'archive info file ') . "invalid\n" .
                        'HINT: use stanza-upgrade if the database has been upgraded or use --force';
                }
            }
        }

        # Reset the console logging
        logEnable();
        return true;
    }
    or do
    {
        # Reset console logging and capture error information
        logEnable();
        $iResult = exceptionCode($EVAL_ERROR);
        $strResultMessage = exceptionMessage($EVAL_ERROR->message());
    };

    # If we got here without error then save the reconstructed file
    if ($iResult == 0)
    {
        $oInfo->save();

        # Sync path
        storageRepo()->pathSync(
            defined($oInfo->{strArchiveClusterPath}) ? $oInfo->{strArchiveClusterPath} : $oInfo->{strBackupClusterPath});
    }

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
            {name => 'oInfo'},
            {name => 'strPathType'},
            {name => 'iExpectedError'},
        );

    my $iResult = 0;
    my $strResultMessage = undef;

    # Turn off console logging to control when to display the error
    logDisable();

    eval
    {
        ($strPathType eq STORAGE_REPO_BACKUP)
            ? $oInfo->check($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion},
                $self->{oDb}{ullDbSysId}, true)
            : $oInfo->check($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId}, true);
        logEnable();
        return true;
    }
    or do
    {
        logEnable();

        # Confess unhandled errors
        confess $EVAL_ERROR if (exceptionCode($EVAL_ERROR) != $iExpectedError);

        # Capture the result which will be the expected error, meaning an upgrade is needed
        $iResult = exceptionCode($EVAL_ERROR);
        $strResultMessage = exceptionMessage($EVAL_ERROR->message());
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => ($iResult == $iExpectedError ? true : false)},
    );
}

1;
