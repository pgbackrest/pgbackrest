####################################################################################################################################
# CHECK MODULE
####################################################################################################################################
package pgBackRest::Check::Check;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use pgBackRest::Archive::Common;
use pgBackRest::Archive::Get::File;
use pgBackRest::Backup::Info;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::Manifest;
use pgBackRest::Protocol::Helper;
use pgBackRest::Protocol::Storage::Helper;

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

    # Initialize the local variables
    my $iArchiveTimeout = cfgOption(CFGOPT_ARCHIVE_TIMEOUT);

    my $iResult = 0;
    my $strResultMessage = undef;

    my $strArchiveId = undef;
    my $strArchiveFile = undef;
    my $strWalSegment = undef;

    # Get the master database object to test to see if the manifest can be built
    my ($oDb) = dbMasterGet();

    # Get the database version to pass to the manifest constructor and the system-id in the event of a failure
    my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $oDb->info();

    # Turn off console logging to control when to display the error
    logLevelSet(undef, OFF);

    # Loop through all defined databases and attempt to build a manifest
    for (my $iRemoteIdx = 1; $iRemoteIdx <= cfgOptionIndexTotal(CFGOPT_PG_HOST); $iRemoteIdx++)
    {
        # Make sure a db is defined for this index
        if (cfgOptionTest(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iRemoteIdx)) ||
            cfgOptionTest(cfgOptionIdFromIndex(CFGOPT_PG_HOST, $iRemoteIdx)))
        {
            eval
            {
                # Passing file location dev/null so that the save will fail if it is ever attempted. Pass a miscellaneus value for
                # encryption key since the file will not be saved.
                my $oBackupManifest = new pgBackRest::Manifest("/dev/null/manifest.chk",
                    {bLoad => false, strDbVersion => $strDbVersion, iDbCatalogVersion => $iCatalogVersion,
                    strCipherPass => 'x', strCipherPassSub => 'x'});

                # Set required settings not set during manifest instantiation
                $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID, undef, 1);
                $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CONTROL, undef, $iControlVersion);
                $oBackupManifest->numericSet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_SYSTEM_ID, undef, $ullDbSysId);

                $oBackupManifest->build(
                    storageDb({iRemoteIdx => $iRemoteIdx}), cfgOption(cfgOptionIdFromIndex(CFGOPT_PG_PATH, $iRemoteIdx)), undef,
                    cfgOptionValid(CFGOPT_ONLINE) && cfgOption(CFGOPT_ONLINE), false, $oDb->tablespaceMapGet());

                return true;
            }
            or do
            {
                # Capture error information
                $strResultMessage = "Database: ${strDbVersion} ${ullDbSysId} " . exceptionMessage($EVAL_ERROR) .
                    (($iResult != 0) ? "\n[$iResult] : $strResultMessage" : "");
                $iResult = exceptionCode($EVAL_ERROR);
            };
        }
    }

    # Reset the console logging
    logLevelSet(undef, cfgOption(CFGOPT_LOG_LEVEL_CONSOLE));

    # If the manifest builds are ok, then proceed with the other checks
    if ($iResult == 0)
    {
        # Reinitialize the database object in order to check the configured replicas. This will throw an error if at least one is not
        # able to be connected to and warnings for any that cannot be properly connected to.
        ($oDb) = dbObjectGet();

        # Validate the database configuration
        $oDb->configValidate();

        # Turn off console logging to control when to display the error
        logLevelSet(undef, OFF);

        # Check backup.info - if the archive check fails below (e.g --no-archive-check set) then at least know backup.info succeeded
        eval
        {
            # Check that the backup info file is written and is valid for the current database of the stanza
            $self->backupInfoCheck();
            return true;
        }
        # If there is an unhandled error then confess
        or do
        {
            # Capture error information
            $iResult = exceptionCode($EVAL_ERROR);
            $strResultMessage = exceptionMessage($EVAL_ERROR);
        };

        # Check archive.info
        if ($iResult == 0)
        {
            eval
            {
                # Check that the archive info file is written and is valid for the current database of the stanza
                ($strArchiveId) = archiveGetCheck();
                return true;
            }
            or do
            {
                # Capture error information
                $iResult = exceptionCode($EVAL_ERROR);
                $strResultMessage = exceptionMessage($EVAL_ERROR);
            };
        }

        # Reset the console logging
        logLevelSet(undef, cfgOption(CFGOPT_LOG_LEVEL_CONSOLE));
    }

    # Log the captured error
    if ($iResult != 0)
    {
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
        $strDbVersion,
        $iControlVersion,
        $iCatalogVersion,
        $ullDbSysId,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->backupInfoCheck', \@_,
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

    if (!isRepoLocal())
    {
        $iDbHistoryId = protocolGet(CFGOPTVAL_REMOTE_TYPE_BACKUP)->cmdExecute(
            OP_CHECK_BACKUP_INFO_CHECK, [$strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId]);
    }
    else
    {
        $iDbHistoryId = (new pgBackRest::Backup::Info(storageRepo()->pathGet(STORAGE_REPO_BACKUP)))->check(
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
