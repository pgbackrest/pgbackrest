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
use IO::Uncompress::Gunzip qw(gunzip $GunzipError) ;

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
# Archive info Constants
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
# Global variables
####################################################################################################################################
my $strArchiveInfoMissingMsg = ARCHIVE_INFO_FILE . " does not exist but is required to get WAL segments\n" .
             "HINT: is archive_command configured in postgresql.conf?\n" .
             "HINT: has a stanza-create been performed?\n " .
             "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving" .
             " scheme.";

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
        $bRequired                                  # Is archive info required?
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strArchiveClusterPath'},
            {name => 'bRequired', default => false}
        );

    # Build the archive info path/file name
    my $strArchiveInfoFile = "${strArchiveClusterPath}/" . ARCHIVE_INFO_FILE;
    my $bExists = fileExists($strArchiveInfoFile);

    if (!$bExists && $bRequired)
    {
        confess &log(ERROR, $strArchiveInfoMissingMsg, ERROR_FILE_MISSING);
    }

    # Init object and store variables
    my $self = $class->SUPER::new($strArchiveInfoFile, $bExists);

    $self->{bExists} = $bExists;
    $self->{strArchiveClusterPath} = $strArchiveClusterPath;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# check
#
# Check archive info file and make sure it is compatible with the current version of the database for the stanza. If the file does
# not exist an error will occur.
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $bRequired,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
            {name => 'bRequired', default => true},
        );

    # ??? remove bRequired after stanza-upgrade
    if ($bRequired)
    {
        # Confirm the info file exists with the DB section
        $self->confirmExists();
    }

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
# create
#
# Creates the archive.info file.
####################################################################################################################################
sub create
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $strDbHistoryId,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->create', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
            {name => 'strDbHistoryId', required => false},
            {name => 'bSave', default => true},
        );

    # Fill db section and db history section
    $self->dbSectionSet($strDbVersion, $ullDbSysId, (defined($strDbHistoryId) ? $strDbHistoryId : $self->dbHistoryIdGet(false)));

    $self->save();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# reconstruct
#
# Reconstruct the info file from the existing directory and files
####################################################################################################################################
sub reconstruct
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strChkDbVersion,
        $ullChkDbSysId,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->reconstruct', \@_,
        {name => 'oFile'},
        {name => 'strChkDbVersion', trace => true},
        {name => 'ullChkDbSysId', trace => true},
    );

    # Get the upper level directory names, e.g. 9.4-1
    foreach my $strVersionDir (fileList($self->{strArchiveClusterPath}, REGEX_DB_VERSION . '-[0-9]+$'))
    {
        # Get the db-version and db-id (history id) from the directory name
        my ($strDbVersion, $strDbHistoryId) = split("-", $strVersionDir);

        # Get the name of the first archive directory
        my $strArchiveDir = (fileList($self->{strArchiveClusterPath}."/${strVersionDir}", REGEX_ARCHIVE_DIR))[0];

#CSHANG Create a constant for the regexs below - or maybe have a function to provide regex for archive stuff like the backupregex function?
# CAN MAKE FUNCTION in archiveCommon.pm
        my $strArchiveFile =
            (fileList($self->{strArchiveClusterPath}."/${strVersionDir}/${strArchiveDir}",
            "^[0-F]{24}(\\.partial){0,1}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$"))[0];

        # Get the full path for the file
        my $strArchiveFilePath = $self->{strArchiveClusterPath}."/${strVersionDir}/${strArchiveDir}/${strArchiveFile}";

        my $tBlock;
        if ($strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$")
        {
            gunzip $strArchiveFilePath => \$tBlock
                or confess "gunzip failed: $GunzipError\n";
        }

# CSHANG may need to put these constants in a common location: PG_VERSION_93 ? PG_WAL_SYSTEM_ID_OFFSET_GTE_93 : PG_WAL_SYSTEM_ID_OFFSET_LT_93;
# MOVE TO COMMON FOR PG_WAL...
        my $iSysIdOffset = $strDbVersion >= '9.3' ? 20 : 12;
        my ($iMagic, $iFlag, $junk, $ullDbSysId) = unpack('SSa' . $iSysIdOffset . 'Q', $tBlock);

        if (!defined($ullDbSysId))
        {
            confess &log(ERROR, "unable to read database system identifier");
        }

        # ??? Until stanza-upgrade, make sure the current DB info matches what should be stored in the file before saving
        # Fill db section and db history section
        $self->dbSectionSet($strDbVersion, $ullDbSysId, $strDbHistoryId);
        $self->check($strChkDbVersion, $ullChkDbSysId, false);
        $self->save();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# dbHistoryIdGet
#
# Get the db history ID
####################################################################################################################################
sub dbHistoryIdGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bFileRequired,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->dbHistoryIdGet', \@_,
        {name => 'bFileRequired', default => true},
    );

    # Confirm the info file exists if it is required
    if ($bFileRequired)
    {
        $self->confirmExists();
    }

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
# dbSectionSet
#
# Set the db and db:history sections.
####################################################################################################################################
sub dbSectionSet
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
            __PACKAGE__ . '->dbSectionSet', \@_,
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

####################################################################################################################################
# Helper functions
####################################################################################################################################
sub confirmExists
{
    my $self = shift;

    # Confirm the file exists and the DB section is filled out
    if (!$self->test(INFO_ARCHIVE_SECTION_DB) || !$self->{bExists})
    {
        confess &log(ERROR, $strArchiveInfoMissingMsg, ERROR_FILE_MISSING);
    }
}

1;
