####################################################################################################################################
# ARCHIVE INFO MODULE
#
# The archive.info file is created when archiving begins. It is located under the stanza directory. The file contains information
# regarding the stanza database version, database WAL segment system id and other information to ensure that archiving is being
# performed on the proper database.
####################################################################################################################################
package pgBackRest::Archive::Info;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);

use pgBackRest::Archive::Common;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::DbVersion;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;
use pgBackRest::Storage::Base;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant ARCHIVE_INFO_FILE                                      => 'archive.info';
    push @EXPORT, qw(ARCHIVE_INFO_FILE);

####################################################################################################################################
# Archive info constants
####################################################################################################################################
use constant INFO_ARCHIVE_SECTION_DB                                => INFO_BACKUP_SECTION_DB;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB);
use constant INFO_ARCHIVE_SECTION_DB_HISTORY                        => INFO_BACKUP_SECTION_DB_HISTORY;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB_HISTORY);

use constant INFO_ARCHIVE_KEY_DB_VERSION                            => MANIFEST_KEY_DB_VERSION;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_VERSION);
use constant INFO_ARCHIVE_KEY_DB_ID                                 => MANIFEST_KEY_DB_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_ID);
use constant INFO_ARCHIVE_KEY_DB_SYSTEM_ID                          => MANIFEST_KEY_SYSTEM_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_SYSTEM_ID);

####################################################################################################################################
# Global variables
####################################################################################################################################
my $strArchiveInfoMissingMsg =
    ARCHIVE_INFO_FILE . " does not exist but is required to push/get WAL segments\n" .
    "HINT: is archive_command configured in postgresql.conf?\n" .
    "HINT: has a stanza-create been performed?\n" .
    "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.";

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
        $strArchiveClusterPath,                     # Archive cluster path
        $bRequired,                                 # Is archive info required?
        $bLoad,                                     # Should the file attempt to be loaded?
        $bIgnoreMissing,                            # Don't error on missing files
        $strCipherPassSub,                          # Passphrase to encrypt the subsequent archive files if repo is encrypted
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strArchiveClusterPath'},
            {name => 'bRequired', default => true},
            {name => 'bLoad', optional => true, default => true},
            {name => 'bIgnoreMissing', optional => true, default => false},
            {name => 'strCipherPassSub', optional => true},
        );

    # Build the archive info path/file name
    my $strArchiveInfoFile = "${strArchiveClusterPath}/" . ARCHIVE_INFO_FILE;
    my $self = {};
    my $iResult = 0;
    my $strResultMessage;

    # Init object and store variables
    eval
    {
        $self = $class->SUPER::new(
            storageRepo(), $strArchiveInfoFile,
                {bLoad => $bLoad, bIgnoreMissing => $bIgnoreMissing, strCipherPassSub => $strCipherPassSub});
        return true;
    }
    or do
    {
        # Capture error information
        $iResult = exceptionCode($EVAL_ERROR);
        $strResultMessage = exceptionMessage($EVAL_ERROR);
    };

    if ($iResult != 0)
    {
        # If the file does not exist but is required to exist, then error
        # The archive info is only allowed not to exist when running a stanza-create on a new install
        if ($iResult == ERROR_FILE_MISSING)
        {
            if ($bRequired)
            {
                confess &log(ERROR, $strArchiveInfoMissingMsg, ERROR_FILE_MISSING);
            }
        }
        elsif ($iResult == ERROR_CRYPTO && $strResultMessage =~ "^unable to flush")
        {
            confess &log(ERROR, "unable to parse '$strArchiveInfoFile'\nHINT: is or was the repo encrypted?", $iResult);
        }
        else
        {
            confess $EVAL_ERROR;
        }
    }

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
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
    ) = logDebugParam
        (
            __PACKAGE__ . '->archiveId', \@_,
            {name => 'strDbVersion', optional => true},
            {name => 'ullDbSysId', optional => true},
        );

    my $strArchiveId = undef;

    # If neither optional version and system-id are passed then set the archive id to the current one
    if (!defined($strDbVersion) && !defined($ullDbSysId))
    {
        $strArchiveId = $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION) . "-" .
            $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID);
    }
    # If both the optional version and system-id are passed
    elsif (defined($strDbVersion) && defined($ullDbSysId))
    {
        # Get the newest archiveId for the version/system-id passed
        $strArchiveId = ($self->archiveIdList($strDbVersion, $ullDbSysId))[0];
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId}
    );
}

####################################################################################################################################
# archiveIdList
#
# Get a sorted list of the archive ids for the db-version and db-system-id passed.
####################################################################################################################################
sub archiveIdList
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
    ) = logDebugParam
        (
            __PACKAGE__ . '->archiveIdList', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
        );

    my @stryArchiveId;

    # Get the version and system-id for all known databases
    my $hDbList = $self->dbHistoryList();

    foreach my $iDbHistoryId (sort  {$a <=> $b} keys %$hDbList)
    {
        # If the version and system-id match then construct the archive id so that the constructed array has the newest match first
        if (($hDbList->{$iDbHistoryId}{&INFO_DB_VERSION} eq $strDbVersion) &&
            ($hDbList->{$iDbHistoryId}{&INFO_SYSTEM_ID} eq $ullDbSysId))
        {
            unshift(@stryArchiveId, $strDbVersion . "-" . $iDbHistoryId);
        }
    }

    # If the archive id has still not been found, then error
    if (@stryArchiveId == 0)
    {
        confess &log(
            ERROR, "unable to retrieve the archive id for database version '$strDbVersion' and system-id '$ullDbSysId'",
            ERROR_ARCHIVE_MISMATCH);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'stryArchiveId', value => \@stryArchiveId}
    );
}

####################################################################################################################################
# create
#
# Creates the archive.info file. WARNING - this function should only be called from stanza-create or tests.
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
        $bSave,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->create', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
            {name => 'bSave', default => true},
        );

    # Fill db section and db history section
    $self->dbSectionSet($strDbVersion, $ullDbSysId, $self->dbHistoryIdGet(false));

    if ($bSave)
    {
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
# dbHistoryList
#
# Get the data from the db history section.
####################################################################################################################################
sub dbHistoryList
{
    my $self = shift;
    my
    (
        $strOperation,
    ) = logDebugParam
        (
            __PACKAGE__ . '->dbHistoryList',
        );

    my %hDbHash;

    foreach my $iHistoryId ($self->keys(INFO_ARCHIVE_SECTION_DB_HISTORY))
    {
        $hDbHash{$iHistoryId}{&INFO_DB_VERSION} =
            $self->get(INFO_ARCHIVE_SECTION_DB_HISTORY, $iHistoryId, INFO_ARCHIVE_KEY_DB_VERSION);
        $hDbHash{$iHistoryId}{&INFO_SYSTEM_ID} =
            $self->get(INFO_ARCHIVE_SECTION_DB_HISTORY, $iHistoryId, INFO_ARCHIVE_KEY_DB_ID);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hDbHash', value => \%hDbHash}
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
    # Force the version to a string since newer versions of JSON::PP lose track of the fact that it is one
    $self->set(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersion . '');
    $self->numericSet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID, undef, $iDbHistoryId);

    # Fill db history
    $self->numericSet(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbHistoryId, INFO_ARCHIVE_KEY_DB_ID, $ullDbSysId);
    # Force the version to a string since newer versions of JSON::PP lose track of the fact that it is one
    $self->set(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbHistoryId, INFO_ARCHIVE_KEY_DB_VERSION, $strDbVersion . '');

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# confirmExists
#
# Ensure that the archive.info file and the db section exist.
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
