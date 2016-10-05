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

    my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $oDb->info(optionGet(OPTION_DB_PATH));

    # If the backup path does not exist, create it
    if (!$oFile->fileExists($oFile->pathGet(PATH_BACKUP_CLUSTER)))
    {
        # Create the cluster backup path
        $oFile->pathCreate(PATH_BACKUP_CLUSTER, undef, undef, true, true);
    }
#CSHANG This will return empty list even if data in the dir
    my @stryBackupList = $oFile->fileList($oFile->pathGet(PATH_BACKUP_CLUSTER), undef, 'forward', true);
    # if the cluster backup path is empty then create the backup info file
    if (!@stryBackupList)
    {
&log(INFO, "Backuplist empty");
        # Create the backup.info file
        my $oBackupInfo = new pgBackRest::BackupInfo($oFile->pathGet(PATH_BACKUP_CLUSTER));
        $oBackupInfo->check($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId);
        $oBackupInfo->save();
    }
    else
    {
&log(INFO, "Backuplist not empty " . $stryBackupList[0]);
    }

    # If the archive path does not exist, create it
    if (!$oFile->fileExists($oFile->pathGet(PATH_BACKUP_ARCHIVE)))
    {
        # Create the cluster archive path
        $oFile->pathCreate(PATH_BACKUP_ARCHIVE, undef, undef, true, true);
    }
    my @stryArchiveList = $oFile->fileList($oFile->pathGet(PATH_BACKUP_ARCHIVE), undef, 'forward', true);

    if (!@stryArchiveList)
    {
&log(INFO,  "archivelist empty");
        my $oArchiveInfo = new pgBackRest::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), false);
        $oArchiveInfo->check($strDbVersion, $ullDbSysId);
        # $oArchiveInfo->save();
    }
    else
    {
&log(INFO, "archivelist not empty " . $stryArchiveList[0]);
    }

    # Check the archive and backup configuration
    my $iResult = 0;

    # Since restore points are only supported in PG>= 9.1 the check command may fail for older versions if there has been no write
    # activity since the last log rotation. Therefore, manually create the files to be sure the backup and archive.info files
    # get created.
    # if ($oDb->versionGet() >= PG_VERSION_91)
    # {
    #     $iResult = new pgBackRest::Archive()->check();
    # }

    # Log that the stanza was created successfully
    if ($iResult == 0)
    {
        &log(INFO, "successfully created stanza ". optionGet(OPTION_STANZA));
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult, trace => true}
    );
}

1;
