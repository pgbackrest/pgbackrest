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

    # Initialize the database object
    ($self->{oDb}) = dbObjectGet();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->DESTROY');

    undef($self->{oDb});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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

    $self->dbInfoGet();

    # Create the archive.info file
    my ($iResult, $strResultMessage) = $self->infoFileCreate($oFile, PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE,
                                                             ERROR_ARCHIVE_DIR_INVALID);

    if ($iResult == 0)
    {
        # Create the backup.info file
        ($iResult, $strResultMessage) = $self->infoFileCreate($oFile, PATH_BACKUP_CLUSTER, FILE_BACKUP_INFO,
                                                              ERROR_BACKUP_DIR_INVALID);
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
        $oFile,
        $strPathType,
        $strInfoFile,
        $iErrorCode,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->infoFileCreate', \@_,
            {name => 'oFile', trace => true},
            {name => 'strPathType', trace => true},
            {name => 'strInfoFile', trace => true},
            {name => 'iErrorCode', trace => true}
        );

    my $iResult = 0;
    my $strResultMessage = undef;
    my $strParentPath = $oFile->pathGet($strPathType);

    # If the info path does not exist, create it
    if (!fileExists($strParentPath))
    {
        # Create the cluster repo path
        $oFile->pathCreate($strPathType, undef, undef, true, true);
    }
# CSHANG - Need to put in checking the DB against the archive.info/backup.info file - we don't want to create a DB=2 scenario so we should be checking against the database.
    # If the cluster repo path is empty then don't validate the info files, just create them
    my @stryFileList = fileList($strParentPath, undef, 'forward', true);
    my $oInfo = ($strPathType eq PATH_BACKUP_CLUSTER) ? new pgBackRest::BackupInfo($strParentPath, false, false)
                                                      : new pgBackRest::ArchiveInfo($strParentPath, false);

    if (!@stryFileList)
    {
        # Create and save the info file
        ($strPathType eq PATH_BACKUP_CLUSTER)
            ? $oInfo->create($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion},
                                 $self->{oDb}{ullDbSysId})
            : $oInfo->create($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId});
    }
    elsif (!grep(/^$strInfoFile$/i, @stryFileList))
    {
        if (!optionGet(OPTION_FORCE))
        {
            $iResult = $iErrorCode;
            $strResultMessage =
                "the " . (($strPathType eq PATH_BACKUP_CLUSTER) ? "backup" : "archive") . " directory is not empty " .
                "but the ${strInfoFile} file is missing\n" .
                "HINT: Has the directory been copied from another location and the copy has not completed?\n" .
                "HINT: Use --force to force the ${strInfoFile} to be created from the existing data in the directory.";
        }
        # Force the recreation of the info file
        else
        {
            # Turn off console logging to control when to display the error
            logLevelSet(undef, OFF);

            eval
            {
                ($strPathType eq PATH_BACKUP_CLUSTER)
                    ? $oInfo->reconstruct($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion},
                                          $self->{oDb}{ullDbSysId})
                    : $oInfo->reconstruct($oFile, $self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId});
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
    # Check the validity of the existing info file
    else
    {
        # Turn off console logging to control when to display the error
        logLevelSet(undef, OFF);

        eval
        {
            # Check that the info file is valid for the current database of the stanza
            ($strPathType eq PATH_BACKUP_CLUSTER)
                ? $oInfo->check($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion}, $self->{oDb}{ullDbSysId})
                : $oInfo->check($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId});
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

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true},
        {name => 'strResultMessage', value => $strResultMessage, trace => true}
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
        $self->{oDb}->configValidate(optionGet(OPTION_DB_PATH));
    }

    ($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion}, $self->{oDb}{ullDbSysId})
        = $self->{oDb}->info(optionGet(OPTION_DB_PATH));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
