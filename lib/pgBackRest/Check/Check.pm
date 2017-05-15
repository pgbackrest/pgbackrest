####################################################################################################################################
# CHECK MODULE
####################################################################################################################################
package pgBackRest::Check::Check;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Archive::ArchiveGet;
use pgBackRest::BackupInfo;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::File;
use pgBackRest::Protocol::Common::Common;
use pgBackRest::Protocol::Helper;

####################################################################################################################################
# constructor
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# process
#
# Validates the database configuration and checks that the archive logs can be read by backup. This will alert the user to any
# misconfiguration, particularly of archiving, that would result in the inability of a backup to complete (e.g waiting at the end
# until it times out because it could not find the WAL file).
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my $strOperation = logDebugParam(__PACKAGE__ . '->process');

    # Initialize default file object
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(isRepoLocal() ? DB : BACKUP)
    );

    # Initialize the database object
    my $oDb = dbMasterGet();

    # Validate the database configuration
    $oDb->configValidate();

    # Get the timeout and error message to display - if it is 0 we are testing
    my $iArchiveTimeout = optionGet(OPTION_ARCHIVE_TIMEOUT);

    # Initialize the result variables
    my $iResult = 0;
    my $strResultMessage = undef;

    my $strArchiveId = undef;
    my $strArchiveFile = undef;
    my $strWalSegment = undef;

    # Turn off console logging to control when to display the error
    logLevelSet(undef, OFF);

    # Check backup.info - if the archive check fails below (e.g --no-archive-check set) then at least know backup.info succeeded
    eval
    {
        # Check that the backup info file is written and is valid for the current database of the stanza
        $self->backupInfoCheck($oFile);
        return true;
    }
    # If there is an unhandled error then confess
    or do
    {
        # Capture error information
        $iResult = exceptionCode($EVAL_ERROR);
        $strResultMessage = exceptionMessage($EVAL_ERROR->message());
    };

    # Check archive.info
    if ($iResult == 0)
    {
        eval
        {
            # Check that the archive info file is written and is valid for the current database of the stanza
            ($strArchiveId) = new pgBackRest::Archive::ArchiveGet()->getCheck($oFile);
            return true;
        }
        or do
        {
            # Capture error information
            $iResult = exceptionCode($EVAL_ERROR);
            $strResultMessage = exceptionMessage($EVAL_ERROR->message());
        };
    }

    # If able to get the archive id then force archiving and check the arrival of the archived WAL file with the time specified
    if ($iResult == 0 && !$oDb->isStandby())
    {
        $strWalSegment = $oDb->xlogSwitch();

        eval
        {
            $strArchiveFile = walSegmentFind($oFile, $strArchiveId, $strWalSegment, $iArchiveTimeout);
            return true;
        }
        # If this is a backrest error then capture the code and message else confess
        or do
        {
            # Capture error information
            $iResult = exceptionCode($EVAL_ERROR);
            $strResultMessage = exceptionMessage($EVAL_ERROR->message());
        };
    }

    # Reset the console logging
    logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));

    # If the archiving was successful and backup.info check did not error in an unexpected way, then indicate success
    # Else, log the error.
    if ($iResult == 0)
    {
        if (!$oDb->isStandby())
        {
            &log(INFO,
            "WAL segment ${strWalSegment} successfully stored in the archive at '" .
            $oFile->pathGet(PATH_BACKUP_ARCHIVE, "$strArchiveId/${strArchiveFile}") . "'");
        }
        else
        {
            &log(INFO, "switch xlog cannot be performed on the standby, all other checks passed successfully");
        }
    }
    else
    {
        &log(ERROR, $strResultMessage, $iResult);

        # If a switch xlog was attempted, then alert the user to the WAL that did not reach the archive
        if (defined($strWalSegment))
        {
            &log(WARN,
                "WAL segment ${strWalSegment} did not reach the archive:" . (defined($strArchiveId) ? $strArchiveId : '') . "\n" .
                "HINT: Check the archive_command to ensure that all options are correct (especialy --stanza).\n" .
                "HINT: Check the PostreSQL server log for errors.");
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
# backupInfoCheck
#
# Check the backup.info file, if it exists, to confirm the DB version, system-id, control and catalog numbers match the database.
####################################################################################################################################
sub backupInfoCheck
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strDbVersion,
        $iControlVersion,
        $iCatalogVersion,
        $ullDbSysId
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->backupInfoCheck', \@_,
        {name => 'oFile'},
        {name => 'strDbVersion', required => false},
        {name => 'iControlVersion', required => false},
        {name => 'iCatalogVersion', required => false},
        {name => 'ullDbSysId', required => false}
    );

    # If the db info are not passed, then we need to retrieve the database information
    my $iDbHistoryId;

    if (!defined($strDbVersion) || !defined($iControlVersion) || !defined($iCatalogVersion) || !defined($ullDbSysId))
    {
        # get DB info for comparison
        ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = dbMasterGet()->info();
    }

    if ($oFile->isRemote(PATH_BACKUP))
    {
        $iDbHistoryId = $oFile->{oProtocol}->cmdExecute(
            OP_CHECK_BACKUP_INFO_CHECK, [$strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId]);
    }
    else
    {
        $iDbHistoryId = (new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER)))->check(
            $strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iDbHistoryId', value => $iDbHistoryId, trace => true}
    );
}

1;
