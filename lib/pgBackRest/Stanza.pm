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
use pgBackRest::Archive;
use pgBackRest::Config::Config;
use pgBackRest::Db;
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
    my $oDb = new pgBackRest::Db(1);

    # Validate the database configuration - if the db-path in pgbackrest.conf does not match the pg_control
    # then this will error alerting the user to fix the pgbackrest.conf
    $oDb->configValidate(optionGet(OPTION_DB_PATH));

    # If the backup path does not exist, create it
    if (!$oFile->fileExists($oFile->pathGet(PATH_BACKUP_CLUSTER)))
    {
        # Create the cluster backup and history path
        $oFile->pathCreate(PATH_BACKUP_CLUSTER, undef, undef, true, true);
    }

    # Check the archive and backup configuration
    my $iResult = new pgBackRest::Archive()->check();

    # Log that the stanza was created successfully
    if ($iResult == 0)
    {
        &log(INFO, "stanza ". optionGet(OPTION_STANZA) . " was created successfully");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true}
    );
}

    # my $oBackupInfo = undef;
    #
    # # Turn off console logging to control when to display the error
    # logLevelSet(undef, OFF);
    #
    # # Load or build backup.info
    # eval
    # {
    #     $oBackupInfo = new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER));
    #     return true;
    # }
    # # If there is an error but it is not that the file is missing then confess
    # or do
    # {
    #     if (!isException($EVAL_ERROR) || $EVAL_ERROR->code() != ERROR_PATH_MISSING)
    #     {
    #         confess $EVAL_ERROR;
    #     }
    #
    #     if (($EVAL_ERROR->code() == ERROR_PATH_MISSING) ||
    #         (defined($oBackupInfo) && !$oBackupInfo->{bExists}))
    #     {
    #         # Create the backup.info file if it does not exist
    #         my $iDbHistoryId = $oBackupInfo->check(
    #     }
    #
    # };
#
#     # Reset the console logging
#     logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));
#
    # Get archiveInfo object
    # my $oArchiveInfo = new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), false);
#CSHANG In reality, if it does not exist we would want to do a stanza-create? No - probably just call validate here in an eval block and if the cluster path is missing, then tell them they need to do a stanza create. So maybe this should not be "validate" unless we have an else statement in the validate function to confirm an existing file is valid before we continue...
#        $oArchiveInfo->validate($oFile->{strCompressExtension});
#
#     # if neither backup nor archive info files exist then there would be nothing to do except check the db-path against the
#     # pg_control file which was performed above
#     if ((!defined($oBackupInfo) || !$oBackupInfo->{bExists}) && !$oArchiveInfo->{bExists})
#     {
#         &log(INFO, "neither archive.info nor backup.info exist - upgrade is not necessary.");
#     }
#     else
#     {
#         # get DB info for comparison
#         my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = ($oDb->info(optionGet(OPTION_DB_PATH)));
#
#         my $strResultInfo = "stanza " . optionGet(OPTION_STANZA) . " has been upgraded to DB version ${strDbVersion}";
#         my $strResultMessage = undef;
#         my $bUpgradedArchiveInfo = false;
#         my $bUpgradedBackupInfo = false;
#
#         # If we have a backup.info file, then check that the stanza backup info db system id is compatible with the current version
#         # of the database and if not, upgrade the DB data in backupinfo object and save the file.
#         if ($oBackupInfo->{bExists})
#         {
#             my ($bVersionSystemMatch, $bCatalogControlMatch) =
#                 $oBackupInfo->checkSectionDb($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);
# # CSHANG here we need to decide if the catalog/control doesn't match - is that truly an error instead?
#             # If something doesn't match, then perform the upgrade
#             if (!$bVersionSystemMatch || !$bCatalogControlMatch)
#             {
#                 # Update the backupinfo object
#                 $oBackupInfo->setDb($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId,
#                                     ($oBackupInfo->getDbHistoryId() + 1));
#                 # atomic save
#                 $oBackupInfo->save();
#                 $bUpgradedBackupInfo = true;
#                 $strResultMessage = "backup info for ". $strResultInfo;
#             }
#         }
#
#         # Turn off console logging to control when to display the error
#         logLevelSet(undef, OFF);
#
#         # # If archive.info exists, then check that the stanza archive info db system id is compatible with the current version of the
#         # # database and if not, upgrade the DB data in archive.info file.
#         # if ($oArchiveInfo->{bExists})
#         # {
#         #     eval
#         #     {
#         #         my $strArchiveId = $oArchiveInfo->check($strDbVersion, $ullDbSysId);
#         #         return true;
#         #     }
#         #     or do
#         #     {
#         #         # Confess unhandled errors
#         #         if (!isException($EVAL_ERROR) || $EVAL_ERROR->code() != ERROR_ARCHIVE_MISMATCH)
#         #         {
#         #             confess $EVAL_ERROR;
#         #         }
#         #
#         #         # Update the archive.info file
#         #         $oArchiveInfo->setDb($strDbVersion, $ullDbSysId, ($oArchiveInfo->getDbHistoryId() + 1));
#         #
#         #         # atomic save
#         #         $oArchiveInfo->save();
#         #         $bUpgradedArchiveInfo = true;
#         #         $strResultMessage = (defined($strResultMessage) ? "\n" : "") . "archive info for ". $strResultInfo;
#         #     };
#         # }
#
#         # Reset the console logging
#         logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));
#
#         if ($bUpgradedBackupInfo || $bUpgradedArchiveInfo)
#         {
#             # Confirm archive and backup are configured properly
#             my $iResult = new pgBackRest::Archive()->check($oArchiveInfo->{bExists}, $oBackupInfo->{bExists});
#
#             &log(INFO, "${strResultMessage}");
#         }
#         else
#         {
#             &log(INFO, "upgrade of " . optionGet(OPTION_STANZA) . "is not necessary");
#         }
#
#     }
#
#     # Return from function and log return values if any
#     return logDebugReturn($strOperation);
# }

1;
