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
use pgBackRest::Archive;
use pgBackRest::ArchiveInfo;
use pgBackRest::BackupInfo;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;
use IO::Uncompress::Gunzip qw($GunzipError);

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

    my $iResult;

    # Process stanza create
    if (commandTest(CMD_STANZA_CREATE))
    {
        $iResult = $self->stanzaCreate();
    }
    # Else error if any other command is found
    else
    {
        confess &log(ASSERT, "Stanza->process() called with invalid command: " . commandGet());
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

    # Initialize the database object
    my $oDb = dbMasterGet();

    # Validate the database configuration - if the db-path in pgbackrest.conf does not match the pg_control
    # then this will error alerting the user to fix the pgbackrest.conf
    $oDb->configValidate();

    my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $oDb->info();

    # Initialize the result variables
    my $iResult = 0;
    my $strResultMessage = undef;

    # Since restore points are only supported in PG>= 9.1 the check command may fail for older versions if there has been no write
    # activity since the last log rotation. Therefore, manually create the files to be sure the backup and archive.info files
    # get created. If the command is twice, just check the files if the DB version is older than 9.1.

    # If the backup path does not exist, create it
    if (!fileExists($oFile->pathGet(PATH_BACKUP_CLUSTER)))
    {
        # Create the cluster backup path
        $oFile->pathCreate(PATH_BACKUP_CLUSTER, undef, undef, true, true);
    }

    # If the cluster backup path is empty then create the backup info file
    my @stryBackupList = fileList($oFile->pathGet(PATH_BACKUP_CLUSTER), undef, 'forward', true);
    my $strBackupInfo = &FILE_BACKUP_INFO;
    my $oBackupInfo = new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER));

    if (!@stryBackupList)
    {
        # Create the backup.info file
        $oBackupInfo->check($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);
        $oBackupInfo->save();
    }
    elsif (!grep(/^$strBackupInfo$/i, @stryBackupList))
    {
        $iResult = &ERROR_BACKUP_DIR_INVALID;
        $strResultMessage = "the backup directory is not empty but the backup.info file is missing\n" .
                            "HINT: Has the directory been copied from another location and the copy has not completed?"
    }
    elsif ($strDbVersion < PG_VERSION_91)
    {
        # Turn off console logging to control when to display the error
        logLevelSet(undef, OFF);

        eval
        {
            # Check that the backup info file is valid for the current database of the stanza
            $oBackupInfo->check($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);
            return true;
        }
        or do
        {
            # Confess unhandled errors
            if (!isException($EVAL_ERROR))
            {
                confess $EVAL_ERROR;
            }

            # If this is a backrest error then capture the last code and message
            $iResult = $EVAL_ERROR->code();
            $strResultMessage = $EVAL_ERROR->message();
        };

        # Reset the console logging
        logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));
    }

    if ($iResult == 0)
    {
        # If the archive path does not exist, create it
        if (!fileExists($oFile->pathGet(PATH_BACKUP_ARCHIVE)))
        {
            # Create the cluster archive path
            $oFile->pathCreate(PATH_BACKUP_ARCHIVE, undef, undef, true, true);
        }

        my @stryArchiveList = fileList($oFile->pathGet(PATH_BACKUP_ARCHIVE), undef, 'forward', true);
        my $strArchiveInfo = &ARCHIVE_INFO_FILE;
        my $oArchiveInfo = new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), false);

        if (!@stryArchiveList)
        {
            $oArchiveInfo->check($strDbVersion, $ullDbSysId);
        }
        elsif (!grep(/^$strArchiveInfo/i, @stryArchiveList ))
        {
            $iResult = &ERROR_ARCHIVE_DIR_INVALID;
            $strResultMessage = "the archive directory is not empty but the archive.info file is missing\n" .
                                "HINT: Has the directory been copied from another location and the copy has not completed?"
        }
        elsif ($strDbVersion < PG_VERSION_91)
        {
            # Turn off console logging to control when to display the error
            logLevelSet(undef, OFF);

            eval
            {
                # check that the archive info file is valid for the current database of the stanza
                $oArchiveInfo->check($strDbVersion, $ullDbSysId);
                return true;
            }
            or do
            {
                # Confess unhandled errors
                if (!isException($EVAL_ERROR))
                {
                    confess $EVAL_ERROR;
                }

                # If this is a backrest error then capture the last code and message
                $iResult = $EVAL_ERROR->code();
                $strResultMessage = $EVAL_ERROR->message();
            };

            # Reset the console logging
            logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));
        }
    }

    # The DB function xlogSwitch checks for the version >= 9.1 and only performs the restore point if met so check would fail here
    # on initial stanza-create for those systems, so only run the full check on systems > 9.1.
    if (($iResult == 0) && ($strDbVersion >= PG_VERSION_91))
    {
        $iResult = new pgBackRest::Archive()->check();
    }

    if ($iResult == 0)
    {
        &log(INFO, "successfully created stanza " . optionGet(OPTION_STANZA));
    }
    else
    {
        &log(ERROR, $strResultMessage, $iResult);
        &log(WARN, "the stanza " . optionGet(OPTION_STANZA) . " could not be created");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true}
    );
}

1;
