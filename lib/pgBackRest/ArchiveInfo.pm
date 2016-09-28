####################################################################################################################################
# ARCHIVE INFO MODULE
#
# The archive.info file is created when archiving begins. It is located under the stanza directory. The file contains information
# regarding the stanza database version, database WAL segment system id and other information to ensure that archiving is being
# performed on the proper database.
####################################################################################################################################
package pgBackRest::ArchiveInfo;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname basename);
use File::stat;

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::BackupInfo;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant ARCHIVE_INFO_FILE                                      => 'archive.info';
    our @EXPORT = qw(ARCHIVE_INFO_FILE);

####################################################################################################################################
# Backup info Constants
####################################################################################################################################
use constant INFO_ARCHIVE_SECTION_DB                                => INFO_BACKUP_SECTION_DB;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB);
use constant INFO_ARCHIVE_SECTION_DB_HISTORY                        => INFO_BACKUP_SECTION_DB_HISTORY;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB);

use constant INFO_ARCHIVE_KEY_DB_VERSION                            => MANIFEST_KEY_DB_VERSION;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_VERSION);
use constant INFO_ARCHIVE_KEY_DB_ID                                 => INFO_BACKUP_KEY_HISTORY_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_ID);
use constant INFO_ARCHIVE_KEY_DB_SYSTEM_ID                          => MANIFEST_KEY_SYSTEM_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_SYSTEM_ID);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                              # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchiveClusterPath,                     # Backup cluster path
        $bRequired,                                 # Is archive info required?
        $bValidate
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strArchiveClusterPath'},
            {name => 'bRequired', default => false},
            {name => 'bValidate', default => true}
        );

    # Build the archive info path/file name
    my $strArchiveInfoFile = "${strArchiveClusterPath}/" . ARCHIVE_INFO_FILE;
    my $bExists = fileExists($strArchiveInfoFile);

    if (!$bExists && $bRequired)
    {
        confess &log(ERROR, ARCHIVE_INFO_FILE . " does not exist but is required to get WAL segments\n" .
                     "HINT: is archive_command configured in postgresql.conf?\n" .
                     "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving" .
                     " scheme.", ERROR_FILE_MISSING);
    }

    # Init object and store variables
    my $self = $class->SUPER::new($strArchiveInfoFile, $bExists);

    $self->{bExists} = $bExists;
    $self->{strArchiveClusterPath} = $strArchiveClusterPath;

    # Validate the backup info
    if ($bValidate)
    {
        $self->validate();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}


####################################################################################################################################
# validate
#
# Compare the backup info against the actual backups in the repository.
####################################################################################################################################
sub validate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->validate');

    # If the archive directory exists then validate the archive info file against the directories
    if (fileExists($self->{strArchiveClusterPath}))
    {

        foreach my $strArchiveDir (fileList($self->{strArchiveClusterPath}, '^[0-9]+\.[0-9]+-[0-9]+$'))
        {
print "DIR= $strArchiveDir\n";
    # TODO:
    # *  get a list of the directories in the archive/<stanza> -- the above regex gets this list and nothing more:
    #     DIR= 9.4-1
    #     DIR= 9.5-10
    #     DIR= 10.1-3
    # * split the directories to version and history
    # * unpack the first file of the first subdir in each strArchiveDir to a temporary location (or is there a way to pull into memory without unpacking -- can do with gzip -dc FILE | head -10 but need to look if we do this anywhere)
    # * instantiate an Archive object and pass file in the temp location to Archive->walInfo (not sure if that will work) to get the db system id
    # * update the archive info file if necessary (figure out criteria for doing so)
    # * remove temp files if we needed to create them
        }
    }

    #     foreach my $strArchiveDir (fileList($self->{strArchiveClusterPath}, backupRegExpGet(true, true, true)))
    #     {

#     # Check for backups that are not in FILE_BACKUP_INFO
#     my $strPattern = backupRegExpGet(true, true, true);
# #CSHANG strPattern is not used - should be passed to the filelist function - or removed
#
#     foreach my $strBackup (fileList($self->{strBackupClusterPath}, backupRegExpGet(true, true, true)))
#     {
#         my $strManifestFile = "$self->{strBackupClusterPath}/${strBackup}/" . FILE_MANIFEST;
#
#         # ??? Check for and move history files that were not moved before and maybe don't consider it to be an error when they
#         # can't be moved.  This would also be true for the first move attempt in Backup->process();
#
#         if (!$self->current($strBackup) && fileExists($strManifestFile))
#         {
#             &log(WARN, "backup ${strBackup} found in repository added to " . FILE_BACKUP_INFO);
#             my $oManifest = pgBackRest::Manifest->new($strManifestFile);
# #CSHANG This add updates the $oContent and saves the data to the backup.info file but then how come removing backups below does NOT save to the backup.info file?
#             $self->add($oManifest);
#         }
#     }
#
#     # Remove backups from FILE_BACKUP_INFO that are no longer in the repository
#     foreach my $strBackup ($self->keys(INFO_BACKUP_SECTION_BACKUP_CURRENT))
#     {
#         my $strManifestFile = "$self->{strBackupClusterPath}/${strBackup}/" . FILE_MANIFEST;
#         my $strBackupPath = "$self->{strBackupClusterPath}/${strBackup}";
#
#         if (!fileExists($strBackupPath))
#         {
#             &log(WARN, "backup ${strBackup} missing in repository removed from " . FILE_BACKUP_INFO);
#             $self->delete($strBackup);
#         }
#         elsif (!fileExists($strManifestFile))
#         {
#             &log(WARN, "backup ${strBackup} missing manifest removed from " . FILE_BACKUP_INFO);
#             $self->delete($strBackup);
#         }
#     }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# check
#
# Check archive info file and make sure it is compatible with the current version of the database for the stanza. If the file does
# not exist it will be created with the values passed.
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'}
        );

    my $bSave = false;

    if ($self->test(INFO_ARCHIVE_SECTION_DB))
    {
        my $strError = undef;

        if (!$self->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersion))
        {
            $strError = "WAL segment version ${strDbVersion} does not match archive version " .
                        $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION);
        }

        if (!$self->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef, $ullDbSysId))
        {
            $strError = (defined($strError) ? ($strError . "\n") : "") .
                        "WAL segment system-id ${ullDbSysId} does not match archive system-id " .
                        $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID);
        }

        if (defined($strError))
        {
            confess &log(ERROR, "${strError}\nHINT: are you archiving to the correct stanza?", ERROR_ARCHIVE_MISMATCH);
        }
    }
    # Else create the info file from the parameters passed which are usually derived from the current WAL segment
    else
    {
        # Initialize the DB id/history id
        my $iDbId = $self->getDbHistoryId();

        # Fill db section and db history section
        $self->setDb($strDbVersion, $ullDbSysId, $iDbId);

        $bSave = true;
    }

    # Save if changes have been made
    if ($bSave)
    {
        $self->save();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $self->archiveId()}
    );
}


####################################################################################################################################
# archiveId
#
# Get the archive id which is a combination of the DB version and the db-id setting (e.g. 9.4-1)
####################################################################################################################################
sub archiveId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->archiveId');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION) . "-" .
                                          $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID)}
    );
}

####################################################################################################################################
# getDbHistoryId
#
# Get the db history ID
####################################################################################################################################
sub getDbHistoryId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->getDbHistoryId');

    # If the DB section does not exist, initialize the history to one, else return the latest ID
    my $iDbHistoryId = (!$self->test(INFO_ARCHIVE_SECTION_DB))
                        ? 1 : $self->numericGet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iDbHistoryId', value => $iDbHistoryId}
    );
}

####################################################################################################################################
# setDb
#
# Set the db and db:history sections.
####################################################################################################################################
sub setDb
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $iDbHistoryId,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->setDb', \@_,
            {name => 'strDbVersion', trace => true},
            {name => 'ullDbSysId', trace => true},
            {name => 'iDbHistoryId', trace => true}
        );

    # Fill db section
    $self->numericSet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef, $ullDbSysId);
    $self->set(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersion);
    $self->numericSet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID, undef, $iDbHistoryId);

    # Fill db history
    $self->numericSet(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbHistoryId, INFO_ARCHIVE_KEY_DB_ID, $ullDbSysId);
    $self->set(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbHistoryId, INFO_ARCHIVE_KEY_DB_VERSION, $strDbVersion);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
