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

use pgBackRest::Backup::Common;
use pgBackRest::Common::Cipher;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Archive::Info;
use pgBackRest::Backup::Info;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;

####################################################################################################################################
# Global variables
####################################################################################################################################
my $strHintForce = "\nHINT: use stanza-create --force to force the stanza data to be recreated.";
my $strInfoMissing = " information missing";
my $strStanzaCreateErrorMsg = "not empty";
my $strRepoEncryptedMsg = " and repo is encrypted and info file(s) are missing, --force cannot be used";

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

    # Initialize the database object if not performing a stanza-delete
    if (!cfgCommandTest(CFGCMD_STANZA_DELETE))
    {
        ($self->{oDb}) = dbObjectGet();
        $self->dbInfoGet();
    }

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
    if (cfgCommandTest(CFGCMD_STANZA_CREATE))
    {
        $iResult = $self->stanzaCreate();
    }
    # Process stanza upgrade
    elsif (cfgCommandTest(CFGCMD_STANZA_UPGRADE))
    {
        $self->stanzaUpgrade();
    }
    # Process stanza delete
    else
    {
        $self->stanzaDelete();
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

    my $bContinue = true;

    # Get the parent paths (create if not exist)
    my $strParentPathArchive = $self->parentPathGet(STORAGE_REPO_ARCHIVE);
    my $strParentPathBackup = $self->parentPathGet(STORAGE_REPO_BACKUP);

    # Get a listing of files in the top-level repo directories, ignoring if any are missing
    my @stryFileListArchive = storageRepo()->list($strParentPathArchive, {bIgnoreMissing => true});
    my @stryFileListBackup = storageRepo()->list($strParentPathBackup, {bIgnoreMissing => true});

    # If files exist force should be required since create must have already occurred and reissuing a create needs to be a consciuos
    # effort to rewrite the files.
    # If at least one directory is not empty, then check to see if the info files exist
    if (@stryFileListArchive || @stryFileListBackup)
    {
        my $strBackupInfoFile = &FILE_BACKUP_INFO;
        my $strArchiveInfoFile = &ARCHIVE_INFO_FILE;

        # If .info exists, set to true.
        my $bBackupInfoFileExists = grep(/^$strBackupInfoFile$/i, @stryFileListBackup);
        my $bArchiveInfoFileExists = grep(/^$strArchiveInfoFile$/i, @stryFileListArchive);

        # If .info does not exist, check for .info.copy
        if (!$bBackupInfoFileExists)
        {
            $strBackupInfoFile .= &INI_COPY_EXT;
            $bBackupInfoFileExists = grep(/^$strBackupInfoFile$/i, @stryFileListBackup);
        }

        if (!$bArchiveInfoFileExists)
        {
            $strArchiveInfoFile .= &INI_COPY_EXT;
            $bArchiveInfoFileExists = grep(/^$strArchiveInfoFile$/i, @stryFileListArchive);
        }

        # Determine if a file exists other than the info files
        my $strExistingFile = $self->existingFileName(STORAGE_REPO_BACKUP, $strParentPathBackup, &FILE_BACKUP_INFO);
        if (!defined($strExistingFile))
        {
            $strExistingFile = $self->existingFileName(STORAGE_REPO_ARCHIVE, $strParentPathArchive, &ARCHIVE_INFO_FILE);
        }

        # If something other than the info files exist in the repo (maybe a backup is in progress) and the user is attempting to
        # change the repo encryption in anyway, then error
        if (defined($strExistingFile) && (!storageRepo()->encryptionValid(storageRepo()->encrypted($strExistingFile))))
        {
            confess &log(ERROR, 'files exist - the encryption type or passphrase cannot be changed', ERROR_PATH_NOT_EMPTY);
        }

        # If the .info file exists in one directory but is missing from the other directory then there is clearly a mismatch
        # which requires force option so throw an error to indicate force is needed. If the repo is encrypted and something other
        # than the info files exist, then error that force cannot be used (the info files cannot be reconstructed since we no longer
        # have the passphrase that was in the info file to open the other files in the repo)
        if (!$bArchiveInfoFileExists && $bBackupInfoFileExists)
        {
            $self->errorForce('archive' . $strInfoMissing, ERROR_FILE_MISSING, $strExistingFile, $bArchiveInfoFileExists,
                $strParentPathArchive, $strParentPathBackup);
        }
        elsif (!$bBackupInfoFileExists && $bArchiveInfoFileExists)
        {
            $self->errorForce('backup' . $strInfoMissing, ERROR_FILE_MISSING, $strExistingFile, $bBackupInfoFileExists,
                $strParentPathArchive, $strParentPathBackup);
        }
        # If we get here then either both exist or neither exist so if neither file exists and something else exists in the
        # directories then need to use force option to recreate the missing info files - unless the repo is encrypted, then force
        # cannot be used if other than the info files exist. If only the info files exist then force must be used to overwrite the
        # files.
        else
        {
            $self->errorForce(
                (@stryFileListBackup ? 'backup directory ' : '') .
                ((@stryFileListBackup && @stryFileListArchive) ? 'and/or ' : '') .
                (@stryFileListArchive ? 'archive directory ' : '') .
                $strStanzaCreateErrorMsg, ERROR_PATH_NOT_EMPTY,
                $strExistingFile, $bArchiveInfoFileExists, $strParentPathArchive, $strParentPathBackup);

            # If no error was thrown, then do not continue without --force
            if (!cfgOption(CFGOPT_FORCE))
            {
                $bContinue = false;
            }
        }
    }

    my $iResult = 0;

    if ($bContinue)
    {
        # Instantiate the info objects. Throws an error and aborts if force not used and an error occurs during instantiation.
        my $oArchiveInfo =
            $self->infoObject(STORAGE_REPO_ARCHIVE, $strParentPathArchive, {bRequired => false, bIgnoreMissing => true});
        my $oBackupInfo =
            $self->infoObject(STORAGE_REPO_BACKUP, $strParentPathBackup, {bRequired => false, bIgnoreMissing => true});

        # Create the archive info object
        ($iResult, my $strResultMessage) = $self->infoFileCreate($oArchiveInfo);

        if ($iResult == 0)
        {
            # Create the backup.info file
            ($iResult, $strResultMessage) = $self->infoFileCreate($oBackupInfo);
        }

        if ($iResult != 0)
        {
            &log(WARN, "unable to create stanza '" . cfgOption(CFGOPT_STANZA) . "'");
            confess &log(ERROR, $strResultMessage, $iResult);
        }
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

    # Get the archive info and backup info files; if either does not exist or cannot be opened an error will be thrown
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
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# stanzaDelete
#
# Delete a stanza. The stop file must exist and the db must be offline unless --force is used.
####################################################################################################################################
sub stanzaDelete
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->stanzaDelete');

    my $strStanza = cfgOption(CFGOPT_STANZA);
    my $oStorageRepo = storageRepo();

    # If at least an archive or backup directory exists for the stanza, then continue, else nothing to do
    if ($oStorageRepo->pathExists(STORAGE_REPO_ARCHIVE) || $oStorageRepo->pathExists(STORAGE_REPO_BACKUP))
    {
        # If the stop file does not exist, then error
        if (!lockStopTest({bStanzaStopRequired => true}))
        {
            confess &log(ERROR, "stop file does not exist for stanza '${strStanza}'" .
                "\nHINT: has the pgbackrest stop command been run on this server?", ERROR_FILE_MISSING);
        }

        # If a force has not been issued, then check the database
        if (!cfgOption(CFGOPT_FORCE))
        {
            # Get the master database object and index
            my ($oDbMaster, $iMasterRemoteIdx) = dbObjectGet({bMasterOnly => true});

            # Initialize the master file object and path
            my $oStorageDbMaster = storageDb({iRemoteIdx => $iMasterRemoteIdx});

            # Check if Postgres is running and if so only continue when forced
            if ($oStorageDbMaster->exists(DB_FILE_POSTMASTERPID))
            {
                confess &log(ERROR, DB_FILE_POSTMASTERPID . " exists - looks like the postmaster is running. " .
                    "To delete stanza '${strStanza}', shutdown the postmaster for stanza '${strStanza}' and try again, " .
                    "or use --force.", ERROR_POSTMASTER_RUNNING);
            }
        }

        # Delete the archive info files
        $oStorageRepo->remove(STORAGE_REPO_ARCHIVE . '/' . ARCHIVE_INFO_FILE, {bIgnoreMissing => true});
        $oStorageRepo->remove(STORAGE_REPO_ARCHIVE . '/' . ARCHIVE_INFO_FILE . INI_COPY_EXT, {bIgnoreMissing => true});

        # Delete the backup info files
        $oStorageRepo->remove(STORAGE_REPO_BACKUP . '/' . FILE_BACKUP_INFO, {bIgnoreMissing => true});
        $oStorageRepo->remove(STORAGE_REPO_BACKUP . '/' . FILE_BACKUP_INFO . INI_COPY_EXT, {bIgnoreMissing => true});

        # Invalidate the backups by removing the manifest files
        foreach my $strBackup ($oStorageRepo->list(
            STORAGE_REPO_BACKUP, {strExpression => backupRegExpGet(true, true, true), strSortOrder => 'reverse',
            bIgnoreMissing => true}))
        {
            $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strBackup}/" . FILE_MANIFEST, {bIgnoreMissing => true});
            $oStorageRepo->remove(STORAGE_REPO_BACKUP . "/${strBackup}/" . FILE_MANIFEST_COPY, {bIgnoreMissing => true});
        }

        # Recursively remove the stanza archive and backup directories
        $oStorageRepo->remove(STORAGE_REPO_ARCHIVE, {bRecurse => true, bIgnoreMissing => true});
        $oStorageRepo->remove(STORAGE_REPO_BACKUP, {bRecurse => true, bIgnoreMissing => true});

        # Remove the stop file so processes can run.
        lockStart();
    }
    else
    {
        &log(INFO, "stanza ${strStanza} already deleted");
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
# existingFileName
#
# Get a name of a file in the specified storage
####################################################################################################################################
sub existingFileName
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strPathType,
        $strParentPath,
        $strExcludeFile,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->existingFileName', \@_,
            {name => 'strPathType'},
            {name => 'strParentPath'},
            {name => 'strExcludeFile'},
        );

    my $hFullList = storageRepo()->manifest(storageRepo()->pathGet($strPathType), {bIgnoreMissing => true});
    my $strExistingFile = undef;

    foreach my $strName (keys(%{$hFullList}))
    {
        if (($hFullList->{$strName}{type} eq 'f') &&
            (substr($strName, 0, length($strExcludeFile)) ne $strExcludeFile))
        {
            $strExistingFile = $strParentPath . "/" . $strName;
            last;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strExistingFile', value => $strExistingFile},
    );
}

####################################################################################################################################
# errorForce
#
# Determines based on encryption or not and errors when force is required but not set.
####################################################################################################################################
sub errorForce
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strMessage,
        $iErrorCode,
        $strFileName,
        $bInfoFileExists,
        $strParentPathArchive,
        $strParentPathBackup,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->errorForce', \@_,
            {name => 'strMessage', trace => true},
            {name => 'iErrorCode', trace => true},
            {name => 'strFileName', required => false, trace => true},
            {name => 'bInfoFileExists', trace => true},
            {name => 'strParentPathArchive', trace => true},
            {name => 'strParentPathBackup', trace => true},
        );

    my $bRepoEncrypted = false;

    # Check if the file passed is encrypted (if one exists) else check the repo setting.
    if (defined($strFileName))
    {
        $bRepoEncrypted = storageRepo()->encrypted($strFileName);
    }
    elsif (defined(storageRepo()->cipherType()))
    {
        $bRepoEncrypted = true;
    }

    # If the repo is encrypted and a file other than the info files exist yet the info file is missing then the --force option
    # cannot be used
    if ($bRepoEncrypted && defined($strFileName) && !$bInfoFileExists)
    {
        confess &log(ERROR, $strMessage . $strRepoEncryptedMsg, $iErrorCode);
    }
    elsif (!cfgOption(CFGOPT_FORCE))
    {
        # If info files exist, check to see if the DB sections match the database - else an upgrade is required
        if ($bInfoFileExists && $iErrorCode == ERROR_PATH_NOT_EMPTY)
        {
            if ($self->upgradeCheck(new pgBackRest::Backup::Info($strParentPathBackup), STORAGE_REPO_BACKUP,
                ERROR_BACKUP_MISMATCH) ||
                $self->upgradeCheck(new pgBackRest::Archive::Info($strParentPathArchive), STORAGE_REPO_ARCHIVE,
                ERROR_ARCHIVE_MISMATCH))
            {
                confess &log(ERROR, "backup info file or archive info file invalid\n" .
                    'HINT: use stanza-upgrade if the database has been upgraded or use --force', ERROR_FILE_INVALID);
            }
            else
            {
                &log(INFO, "stanza-create was already performed");
            }
        }
        else
        {
            # If get here something is wrong so indicate force is required
            confess &log(ERROR, $strMessage . $strHintForce, $iErrorCode);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
        # attempt to be loaded. If the info file is missing and the repo is encrypted, we can only get to this function if nothing
        # else existed in the repo so a passphrase is generated to store in the file. If it exists and the repo is encrypted then
        # the generated passphrase passed will not be used - the one from the info file will be read.
        my $oParamRef =
            {bIgnoreMissing => $bIgnoreMissing, strCipherPassSub => defined(storageRepo()->cipherType()) ? cipherPassGen() : undef};

        $oInfo = ($strPathType eq STORAGE_REPO_BACKUP ?
            new pgBackRest::Backup::Info($strParentPath, false, $bRequired, $oParamRef) :
            new pgBackRest::Archive::Info($strParentPath, $bRequired, $oParamRef));

        # Reset the console logging
        logEnable();
        return true;
    }
    or do
    {
        # Reset console logging and capture error information
        logEnable();
        $iResult = exceptionCode($EVAL_ERROR);
        $strResultMessage = exceptionMessage($EVAL_ERROR);
    };

    if ($iResult != 0)
    {
        # If force was not used, and the file is missing, then confess the error with hint to use force if the option is
        # configurable (force is not configurable for stanza-upgrade so this will always confess errors on stanza-upgrade)
        # else confess all other errors
        if ((cfgOptionValid(CFGOPT_FORCE) && !cfgOption(CFGOPT_FORCE)) ||
            (!cfgOptionValid(CFGOPT_FORCE)))
        {
            if ($iResult == ERROR_FILE_MISSING)
            {
                confess &log(ERROR, cfgOptionValid(CFGOPT_FORCE) ? $strResultMessage . $strHintForce : $strResultMessage, $iResult);
            }
            else
            {
                confess &log(ERROR, $strResultMessage, $iResult);
            }
        }
        # Else instatiate the object without loading it so we can reconstruct and overwrite the invalid files
        else
        {
            # Confess unhandled exception
            if (($iResult != ERROR_FILE_MISSING) && ($iResult != ERROR_CRYPTO))
            {
                confess &log(ERROR, $strResultMessage, $iResult);
            }

            my $oParamRef =
                {bLoad => false, strCipherPassSub => defined(storageRepo()->cipherType()) ? cipherPassGen() : undef};

            $oInfo = ($strPathType eq STORAGE_REPO_BACKUP ?
                new pgBackRest::Backup::Info($strParentPath, false, false, $oParamRef) :
                new pgBackRest::Archive::Info($strParentPath, false, $oParamRef));
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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoFileCreate', \@_,
            {name => 'oInfo', trace => true},
        );

    my $iResult = 0;
    my $strResultMessage = undef;
    my $strWarningMsgArchive = undef;

    # Turn off console logging to control when to display the error
    logDisable();

    eval
    {
        # Reconstruct the file from the data in the directory if there is any else initialize the file
        if (defined($oInfo->{strBackupClusterPath}))
        {
            $oInfo->reconstruct(false, false, $self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId}, $self->{oDb}{iControlVersion},
                $self->{oDb}{iCatalogVersion});
        }
        # If this is the archive.info reconstruction then catch any warnings
        else
        {
            $strWarningMsgArchive = $oInfo->reconstruct($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId});
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
        $strResultMessage = exceptionMessage($EVAL_ERROR);
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
    if (cfgOption(CFGOPT_ONLINE))
    {
        # If the pg-path in pgbackrest.conf does not match the pg_control then this will error alert the user to fix pgbackrest.conf
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
        $strResultMessage = exceptionMessage($EVAL_ERROR);
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => ($iResult == $iExpectedError ? true : false)},
    );
}

1;
