####################################################################################################################################
# ARCHIVE INFO MODULE
####################################################################################################################################
package BackRest::ArchiveInfo;
use parent 'BackRest::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname basename);
use File::stat;

use lib dirname($0);
use BackRest::BackupInfo;
use BackRest::Config;
use BackRest::Exception;
use BackRest::File;
use BackRest::Ini;
use BackRest::Manifest;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_ARCHIVE_INFO                                        => 'ArchiveInfo';

use constant OP_ARCHIVE_INFO_NEW                                    => OP_ARCHIVE_INFO . "->new";

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
    my $class = shift;                  # Class name
    my $strArchiveClusterPath = shift;  # Backup cluster path

    &log(DEBUG, OP_ARCHIVE_INFO_NEW . "(): archiveClusterPath = ${strArchiveClusterPath}");

    # Build the archive info path/file name
    my $strArchiveInfoFile = "${strArchiveClusterPath}/" . ARCHIVE_INFO_FILE;
    my $bExists = -e $strArchiveInfoFile ? true : false;

    # Init object and store variables
    my $self = $class->SUPER::new($strArchiveInfoFile, $bExists);

    $self->{bExists} = $bExists;
    $self->{strArchiveClusterPath} = $strArchiveClusterPath;

    return $self;
}

####################################################################################################################################
# check
#
# Check archive info and make it is compatible.
####################################################################################################################################
sub check
{
    my $self = shift;
    my $strDbVersion = shift;
    my $ullDbSysId = shift;

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
            $strError = "WAL segment system-id ${ullDbSysId} does not match archive system-id " .
                        $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID);
        }

        if (defined($strError))
        {
            confess &log(ERROR, "${strError}\nHINT: are you archiving to the correct stanza?", ERROR_ARCHIVE_MISMATCH);
        }
    }
    # Else create the info file from the current WAL segment
    else
    {
        my $iDbId = 1;

        # Fill db section
        $self->setNumeric(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef, $ullDbSysId);
        $self->set(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersion);
        $self->setNumeric(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID, undef, $iDbId);

        # Fill db history
        $self->setNumeric(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbId, INFO_ARCHIVE_KEY_DB_ID, $ullDbSysId);
        $self->set(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbId, INFO_ARCHIVE_KEY_DB_VERSION, $strDbVersion);

        $bSave = true;
    }

    # Save if changes have been made
    if ($bSave)
    {
        $self->save();
    }

    return $self->archiveId();
}


####################################################################################################################################
# archiveId
#
# Get the archive id.
####################################################################################################################################
sub archiveId
{
    my $self = shift;

    return $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION) . "-" .
           $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID);
}

1;
