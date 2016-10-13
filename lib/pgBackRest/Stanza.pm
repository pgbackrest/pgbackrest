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

# # GOAL: to update the archive.info and backup.info files to the new database version  (the archive PGversion dir should be created automatically)
# # For example, the /var/lib/pgbackrest/archive/<STANZA>/ directory would go from:
# #   9.4-1 to having 2 directories: 9.4-1 and 9.5-2
# #   The archive.info file will need to change
#     #     [db]
#     #     db-id=1
#     #     db-system-id=6317250445863755990
#     #     db-version="9.4"
#     #
#     #     [db:history]
#     #     1={"db-id":6317250445863755990,"db-version":"9.4"}
#     #
#     # To say:
#     #     [db]
#     #     db-id=2
#     #     db-system-id=1234567890123456789
#     #     db-version="9.5"
#     #
#     #     [db:history]
#     #     1={"db-id":6317250445863755990,"db-version":"9.4"}
#     #     2={"db-id":1234567890123456789,"db-version":"9.5"}
#
# # For backup, /var/lib/pgbackrest/backup/<STANZA>/ directory does not have subdirectories based on the database version
# #   the file has more data so all values would change
#     # [db]
#     # db-catalog-version=201409291
#     # db-control-version=942
#     # db-id=1
#     # db-system-id=6317250445863755990
#     # db-version="9.4"
#     #
#     # [db:history]
#     # 1={"db-catalog-version":201409291,"db-control-version":942,"db-system-id":6317250445863755990,"db-version":"9.4"}
#
# # QUESTIONS:
# # backup.info -- do I need to do anything with the [backup:current] section?
# -- no
# # See Config.pm line 1748. If I have sys1 with backrest and DB + REPO combo and sys2 with just backrest
# # installed and have the DB and REPO remote, then the first IF will set remote type to BACKUP and be done without erroring.
# -- Dave has refactored this code and if backup
#
# # But there is ths issue of remote/local using the
# # Archive->check command - if backup repo is local, then we check the backup.info, but if it is not local
# # we just ignore it and move on - but why didn't we pass the information over the protocol layer?
# -- need to make the $oBackupInfo->check work remotely but need to succeed on missing backup.info (in backup.pm, if current backup is not full it does a bunch of tests so we will need to update that) -- this is a bug in the archive->check function so we update remote minion and backupinfo check
#
# # What happens if the current backup was based on the prior DB version?
# -- this is for later and there is code to check the backup and maybe force a full backup after an upgrade
#
# # Does the db:history in the backup.info have anything to do with the backup.history directory?
# -- no
#
# # After an upgrade, the pg_control file will contain the DB version, system, etc for the new PG version.
# # However, it is possible that the database version and system ID in the backup.info/archive.info files could be the same as the
# # upgraded DB ver/sys - which is OK and we need to handle since system id will be different. If they really did not upgrade then output INFO that no upgrade happened.
# -- system ID will change (it is based on timestamp and process ID)
#
# # Is it the checksum that can tell us if the version and system id are the same but an upgrade has occurred? If so, would it be different in the case the version numbers are the same?
#
# # I think it would also be helpful to output that we're upgrading from x version to y version.
# -- yes, info level only
#
# -- NOTE: for now we will check if remote for stanza-upgrade, then tell them they need to run it locally
# # What to do if the system crashes during a stanza upgrade and backup and archive info are not in sync? Can
# # we automatically run the check command after the upgrade to confirm?
# -- If only one gets updated, then the other operation will fail, so they dont have to be consistent with each other.
# -- See IniSave function and pass bTemp as true to make the update atomic.
#
# # Can I assume they can upgrade multiple stanzas in one command? How will that work?
# -- No - if require stanza then it does not allow them to have more than one.
#
# # I'm still confused by OPTION_RULE_COMMAND and how I know what options the stanza-upgrade must have? Like why is backup-host not an option for backup command but backup-host is?
# # How is this:
# #         &OPTION_RULE_COMMAND =>
# #         {
# #             &CMD_CHECK =>
# #             {
# #                 &OPTION_RULE_REQUIRED => true  -- must be specified  (if this is outside, then it is required for all commands
# #             },
# # different than this:
# #         &OPTION_RULE_COMMAND =>
# #         {
# #             &CMD_CHECK => true,  -- can be specified
# # and what does this mean:
# #         &OPTION_RULE_COMMAND =>
# #         {
# #             &CMD_CHECK => false, -- can't be specified so can't use it for this option
#
# # Once we add in the new history, we need to update the Expire command so that it works on the history for backups that exceed retention.
#
# # Need to test the backup info command?
# -- Yes, test it after an upgrade
#
#   - check if REPO is local - if not, log ERROR message that they must run the command locally (pgbackrest file)
#   - get the DB info from the database
#   - compare the systemID with what is in the backup file
#       - if not different, no upgrade was performed so move to archive.info and check that  - if also same, then log INFO and exit
#       - else
#           - update the backup and archive info files
#           - save the files atomically
#           - log info to the user the upgrade occurred
#TODO - then add code to expire so it expires from the history for backups and archives that exceed retention. Per Dave, backup expiration should work fine but the paths for the archive will need to be addressed and when all archive for the old DB version are removed from the dir (e.g. 9.4-1) then we need new code to remove the empty directory.

#CSHANG - SOMEHOW when we do a switchXlog, the archive.info file is being created but it is NOT being created through the archiveInfo->check command. I took out the call to archiveInfo->check in the archive->getCheck and just to be sure, I added print statements in the archiveInfo->check function - and these never get printed. I also added staements to the INISAVE and SAVE of ini.pm but still nothing! so I can't figure out where in the code the archive.info file is being written.
#CSHANG Per Dave, we should be calling archive->check from stanza-upgrade but need a flag to indicate if we should be switching an xlog or not because if we already know that nothing has changed or the file does not exist and something has changed, we don't want the stanza-upgrade to create a file but we do want it to make sure everything is configured properly before we upgrade the info files. But the question is how do we ensure everything is configured properly WITHOUT doing a switchXlog? The whole point is to determine if the archive_command is set up properly -- but we KNOW it will fail if we are calling it from stanza-upgrade BEFORE updating the archive.info file. So I see stanza-upgrade as doing the following:
# 1) Check for the existence of backup.info and archive.info - if neither exist do nothing - do not create the files during stanza-upgrade
# 2) If only backup.info file exists, then check the system_id (the getBackupInfoCheck checks version/id and then catalog/control - if either condition is not a match with the DB it errors out) and upgrade the backup.info file.
# 3) If only archive.info file exists (this is the normal case) we still wouldn't want to call this function knowing that the switch would still fail - the archive.info file is checked against the DB data (but for sysID and DB version) resulting in a failure in this function (after 60 seconds) - it's a waste of time because we KNOW it will fail until the archive.info file is in sync with the DB.
# 4) If both backup.info and archive.info exist then we still would want to not waste time running this function until we upgraded the files because, again, it would just fail - as stated in all the reasons above.
# So I think we should check it AFTER we upgrade the info files after determining they need upgrading. We can check the archive.info against the backup.info in the other function BEFORE updating them, once we know the 2 files exist, and throw an error. We can do that here as well, near the end.
#
# IF archive file is missing AND backup info file is missing
# THEN
#     do nothing;
# ELSE
#     IF archive info exists
#     THEN
#         load the archive.info file;
#         update the archive.info DB section and save the file;
#     FI
#     IF backup info exists
#     THEN
#         load the backup.info file but do not validate;
#         update the backup.info DB section and save the file;
#     FI
#     call archive->check(archiveinfo->bExists, backupinfo->bExists)
#     -- any errors now should be logged


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

    my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $oDb->info(optionGet(OPTION_DB_PATH));

    # Initialize the result variables
    my $iResult = 0;
    my $strResultMessage = undef;

    # Since restore points are only supported in PG>= 9.1 the check command may fail for older versions if there has been no write
    # activity since the last log rotation. Therefore, manually create the files to be sure the backup and archive.info files
    # get created. If they run the command twice, just check the files if the DB version is older than 9.1.

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
        #\n"."HINT: Use --force to force the backup.info to be created from the existing backup data in the directory.";
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
            #TODO: need to remove the save in the archive.info check or have a default so it does not get created unless forced?
            # $oArchiveInfo->save();
        }
        elsif (!grep(/^$strArchiveInfo/i, @stryArchiveList ))
        {
            $iResult = &ERROR_ARCHIVE_DIR_INVALID;
            $strResultMessage = "the archive directory is not empty but the archive.info file is missing\n" .
                                "HINT: Has the directory been copied from another location and the copy has not completed?"
            #\n"."HINT: Use --force to force the archive.info to be created from the existing archive data in the directory.";
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

sub stanzaUpgrade
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
# CSHANG why are we needing to pass a 1 now? What is iRemoteIdx?
    my $oDb = new pgBackRest::Db(1);

    # Validate the database configuration - if the db-path in pgbackrest.conf does not match the pg_control then this will error
    # alerting the user to fix the pgbackrest.conf
    $oDb->configValidate(optionGet(OPTION_DB_PATH));

    my $oBackupInfo = undef;

    # Turn off console logging to control when to display the error
#     logLevelSet(undef, OFF);
#
#     # Load or build backup.info
#     eval
#     {
#         $oBackupInfo = new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER));
#         return true;
#     }
#     # If there is an error but it is not that the file is missing then confess
#     or do
#     {
#         if (!isException($EVAL_ERROR) || $EVAL_ERROR->code() != ERROR_PATH_MISSING)
#         {
#             confess $EVAL_ERROR;
#         }
#     };
#
#     # Reset the console logging
#     logLevelSet(undef, optionGet(OPTION_LOG_LEVEL_CONSOLE));
#
    # Get archiveInfo object
    my $oArchiveInfo = new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), false);
#CSHANG In reality, if it does not exist we would want to do a stanza-create? No - probably just call validate here in an eval block and if the cluster path is missing, then tell them they need to do a stanza create. So maybe this should not be "validate" unless we have an else statement in the validate function to confirm an existing file is valid before we continue...
        $oArchiveInfo->validate($oFile->{strCompressExtension});
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

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
