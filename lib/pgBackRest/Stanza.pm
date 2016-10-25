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
    $self->{oDb} = new pgBackRest::Db(1);

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

    # Create the backup.info file
    my ($iResult, $strResultMessage) = $self->createInfoFile($oFile, PATH_BACKUP_CLUSTER, FILE_BACKUP_INFO, ERROR_BACKUP_DIR_INVALID);

    if ($iResult == 0)
    {
        # Create the archive.info file
        ($iResult, $strResultMessage) = $self->createInfoFile($oFile, PATH_BACKUP_ARCHIVE, ARCHIVE_INFO_FILE, ERROR_ARCHIVE_DIR_INVALID);
    }

    # The DB function xlogSwitch checks for the version >= 9.1 and only performs the restore point if met so check would fail here
    # on initial stanza-create for those systems, so only run the full check on systems > 9.1.
    if (($iResult == 0) && ($self->{oDb}{strDbVersion} >= PG_VERSION_91))
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

####################################################################################################################################
# createInfoFile
#
# Creates the info file based on the data passed to the function
####################################################################################################################################
sub createInfoFile
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
            __PACKAGE__ . '->createInfoFile', \@_,
            {name => 'oFile'},
            {name => 'strPathType'},
            {name => 'strInfoFile'},
            {name => 'iErrorCode'}
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

    # If the cluster repo path is empty then create the backup info file
    my @stryFileList = fileList($strParentPath, undef, 'forward', true);
    my $oInfo = ($strPathType eq PATH_BACKUP_CLUSTER) ? new pgBackRest::BackupInfo($strParentPath, false, false)
                                                      : new pgBackRest::ArchiveInfo($strParentPath, false);

    if (!@stryFileList)
    {
&log(WARN, "CREATING INFO FILE at ${strPathType}"); #CSHANG
        # Create and save the info file
        ($strPathType eq PATH_BACKUP_CLUSTER)
            ? $oInfo->fileCreate($self->{oDb}{strDbVersion}, $self->{oDb}{iControlVersion}, $self->{oDb}{iCatalogVersion},
                                 $self->{oDb}{ullDbSysId})
            : $oInfo->fileCreate($self->{oDb}{strDbVersion}, $self->{oDb}{ullDbSysId});
    }
    elsif (!grep(/^$strInfoFile/i, @stryFileList))
    {
        $iResult = $iErrorCode;
        $strResultMessage = "the " . (($strPathType eq PATH_BACKUP_CLUSTER) ? "backup" : "archive") . " directory is not empty " .
                            "but the ${strInfoFile} file is missing\n" .
                            "HINT: Has the directory been copied from another location and the copy has not completed?";
        #\n"."HINT: Use --force to force the backup.info to be created from the existing backup data in the directory.";
    }
    elsif ($self->{oDb}{strDbVersion} < PG_VERSION_91)
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
# Gets the database information and store it i $self
####################################################################################################################################
sub dbInfoGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->dbInfoGet');

    my $oDb = $self->{oDb};

    # Validate the database configuration - if the db-path in pgbackrest.conf does not match the pg_control
    # then this will error alerting the user to fix the pgbackrest.conf
    $self->{oDb}->configValidate(optionGet(OPTION_DB_PATH));

    (${$oDb}{strDbVersion}, ${$oDb}{iControlVersion}, ${$oDb}{iCatalogVersion}, ${$oDb}{ullDbSysId})
        = $self->{oDb}->info(optionGet(OPTION_DB_PATH));

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
