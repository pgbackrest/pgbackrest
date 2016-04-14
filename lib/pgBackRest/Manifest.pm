####################################################################################################################################
# MANIFEST MODULE
####################################################################################################################################
package pgBackRest::Manifest;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use Digest::SHA;
use Time::Local qw(timelocal);

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::File;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_MANIFEST                                            => 'Manifest';

use constant OP_MANIFEST_BUILD                                      => OP_MANIFEST . '->build';
use constant OP_MANIFEST_NEW                                        => OP_MANIFEST . '->new';
use constant OP_MANIFEST_SAVE                                       => OP_MANIFEST . '->save';

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant FILE_MANIFEST                                          => 'backup.manifest';
    push @EXPORT, qw(FILE_MANIFEST);
use constant PATH_PG_TBLSPC                                         => 'pg_tblspc';
    push @EXPORT, qw(PATH_PG_TBLSPC);
use constant FILE_PG_CONTROL                                        => 'global/pg_control';
    push @EXPORT, qw(FILE_PG_CONTROL);

####################################################################################################################################
# MANIFEST Constants
####################################################################################################################################
use constant MANIFEST_PATH                                          => 'path';
    push @EXPORT, qw(MANIFEST_PATH);
use constant MANIFEST_FILE                                          => 'file';
    push @EXPORT, qw(MANIFEST_FILE);
use constant MANIFEST_LINK                                          => 'link';
    push @EXPORT, qw(MANIFEST_LINK);
use constant MANIFEST_TABLESPACE                                    => 'tablespace';
    push @EXPORT, qw(MANIFEST_TABLESPACE);

use constant MANIFEST_KEY_BASE                                      => 'base';
    push @EXPORT, qw(MANIFEST_KEY_BASE);

# Manifest sections
use constant MANIFEST_SECTION_BACKUP                                => 'backup';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP);
use constant MANIFEST_SECTION_BACKUP_DB                             => 'backup:db';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_DB);
use constant MANIFEST_SECTION_BACKUP_INFO                           => 'backup:info';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_INFO);
use constant MANIFEST_SECTION_BACKUP_OPTION                         => 'backup:option';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_OPTION);
use constant MANIFEST_SECTION_BACKUP_PATH                           => 'backup:path';
    push @EXPORT, qw(MANIFEST_SECTION_BACKUP_PATH);

# Backup metadata required for restores
use constant MANIFEST_KEY_ARCHIVE_START                             => 'backup-archive-start';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_START);
use constant MANIFEST_KEY_ARCHIVE_STOP                              => 'backup-archive-stop';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_STOP);
use constant MANIFEST_KEY_LABEL                                     => 'backup-label';
    push @EXPORT, qw(MANIFEST_KEY_LABEL);
use constant MANIFEST_KEY_PRIOR                                     => 'backup-prior';
    push @EXPORT, qw(MANIFEST_KEY_PRIOR);
use constant MANIFEST_KEY_TIMESTAMP_COPY_START                      => 'backup-timestamp-copy-start';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_COPY_START);
use constant MANIFEST_KEY_TIMESTAMP_START                           => 'backup-timestamp-start';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_START);
use constant MANIFEST_KEY_TIMESTAMP_STOP                            => 'backup-timestamp-stop';
    push @EXPORT, qw(MANIFEST_KEY_TIMESTAMP_STOP);
use constant MANIFEST_KEY_TYPE                                      => 'backup-type';
    push @EXPORT, qw(MANIFEST_KEY_TYPE);

# Options that were set when the backup was made
use constant MANIFEST_KEY_HARDLINK                                  => 'option-hardlink';
    push @EXPORT, qw(MANIFEST_KEY_HARDLINK);
use constant MANIFEST_KEY_ARCHIVE_CHECK                             => 'option-archive-check';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_CHECK);
use constant MANIFEST_KEY_ARCHIVE_COPY                              => 'option-archive-copy';
    push @EXPORT, qw(MANIFEST_KEY_ARCHIVE_COPY);
use constant MANIFEST_KEY_COMPRESS                                  => 'option-compress';
    push @EXPORT, qw(MANIFEST_KEY_COMPRESS);
use constant MANIFEST_KEY_ONLINE                                    => 'option-online';
    push @EXPORT, qw(MANIFEST_KEY_ONLINE);

# Information about the database that was backed up
use constant MANIFEST_KEY_SYSTEM_ID                                 => 'db-system-id';
    push @EXPORT, qw(MANIFEST_KEY_SYSTEM_ID);
use constant MANIFEST_KEY_CATALOG                                   => 'db-catalog-version';
    push @EXPORT, qw(MANIFEST_KEY_CATALOG);
use constant MANIFEST_KEY_CONTROL                                   => 'db-control-version';
    push @EXPORT, qw(MANIFEST_KEY_CONTROL);
use constant MANIFEST_KEY_DB_VERSION                                => 'db-version';
    push @EXPORT, qw(MANIFEST_KEY_DB_VERSION);

# Subkeys used for path/file/link info
use constant MANIFEST_SUBKEY_CHECKSUM                               => 'checksum';
    push @EXPORT, qw(MANIFEST_SUBKEY_CHECKSUM);
use constant MANIFEST_SUBKEY_DESTINATION                            => 'destination';
    push @EXPORT, qw(MANIFEST_SUBKEY_DESTINATION);
use constant MANIFEST_SUBKEY_FUTURE                                 => 'future';
    push @EXPORT, qw(MANIFEST_SUBKEY_FUTURE);
use constant MANIFEST_SUBKEY_GROUP                                  => 'group';
    push @EXPORT, qw(MANIFEST_SUBKEY_GROUP);
use constant MANIFEST_SUBKEY_LINK                                   => 'link';
    push @EXPORT, qw(MANIFEST_SUBKEY_LINK);
use constant MANIFEST_SUBKEY_MODE                                   => 'mode';
    push @EXPORT, qw(MANIFEST_SUBKEY_MODE);
use constant MANIFEST_SUBKEY_TIMESTAMP                              => 'timestamp';
    push @EXPORT, qw(MANIFEST_SUBKEY_TIMESTAMP);
use constant MANIFEST_SUBKEY_PATH                                   => 'path';
    push @EXPORT, qw(MANIFEST_SUBKEY_PATH);
use constant MANIFEST_SUBKEY_REFERENCE                              => 'reference';
    push @EXPORT, qw(MANIFEST_SUBKEY_REFERENCE);
use constant MANIFEST_SUBKEY_SIZE                                   => 'size';
    push @EXPORT, qw(MANIFEST_SUBKEY_SIZE);
use constant MANIFEST_SUBKEY_USER                                   => 'user';
    push @EXPORT, qw(MANIFEST_SUBKEY_USER);

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strFileName,                               # Manifest filename
        $bLoad                                      # Load the manifest?
    ) =
        logDebugParam
        (
            OP_MANIFEST_NEW, \@_,
            {name => 'strFileName', trace => true},
            {name => 'bLoad', required => false, trace => true}
        );

    # Set defaults
    $bLoad = defined($bLoad) ? $bLoad : true;

    # Init object and store variables
    my $self = $class->SUPER::new($strFileName, $bLoad);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# save
#
# Save the manifest.
####################################################################################################################################
sub save
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
    ) =
        logDebugParam
        (
            OP_MANIFEST_SAVE
        );

    # !!! Add section comments here
    # $self->commentSet(MANIFEST_SECTION_BACKUP_INFO,
    #     #################################################################################
    #     "Information about the backup:\n" .
    #     "    backup-size       = total size of original files.\n" .
    #     "    backup-size-delta = difference in total file size from the prior backup.\n".
    #     "                        backup-size-delta will be equal to backup-size when\n" .
    #     "                        backup-type = full, otherwise this is not possible\n" .
    #     "                        unless option-start-stop = true.\n" .
    #     "\n" .
    #     "Human-readable output:\n" .
    #     "    backup-repo-size       = " . fileSizeFormat($lBackupRepoSize) . "\n" .
    #     "    backup-repo-size-delta = " . fileSizeFormat($lBackupRepoSizeDelta) . "\n" .
    #     "    backup-size            = " . fileSizeFormat($lBackupSize) . "\n" .
    #     "    backup-size-delta      = " . fileSizeFormat($lBackupSizeDelta)
    #     );

    # Call inherited save
    $self->SUPER::save();

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

####################################################################################################################################
# set
#
# Set a value.
####################################################################################################################################
sub set
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $strValue = shift;

    # Make sure the keys are valid
    $self->valid($strSection, $strKey, $strSubKey);

    # Call inherited set
    $self->SUPER::set($strSection, $strKey, $strSubKey, $strValue);
}

####################################################################################################################################
# remove
#
# Remove a value.
####################################################################################################################################
sub remove
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $strValue = shift;

    # Make sure the keys are valid
    $self->valid($strSection, $strKey, $strSubKey, undef, true);

    # Call inherited remove
    $self->SUPER::remove($strSection, $strKey, $strSubKey, $strValue);
}

####################################################################################################################################
# valid
#
# Determine if section, key, subkey combination is valid.
####################################################################################################################################
sub valid
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;
    my $strValue = shift;
    my $bDelete = shift;

    # Section and key must always be defined
    if (!defined($strSection) || !defined($strKey))
    {
        confess &log(ASSERT, 'section or key is not defined');
    }

    # Default bDelete
    $bDelete = defined($bDelete) ? $bDelete : false;

    if ($strSection =~ /^.*\:(file|path|link)$/ && $strSection !~ /^backup\:path$/)
    {
        if (!defined($strSubKey) && $bDelete)
        {
            return true;
        }

        my $strPath = (split(':', $strSection))[0];
        my $strType = (split(':', $strSection))[1];

        if ($strPath eq MANIFEST_TABLESPACE)
        {
            $strPath = (split(':', $strSection))[1];
            $strType = (split(':', $strSection))[2];
        }

        if (($strType eq 'path' || $strType eq 'file' || $strType eq 'link') &&
            ($strSubKey eq MANIFEST_SUBKEY_USER ||
             $strSubKey eq MANIFEST_SUBKEY_GROUP))
        {
            return true;
        }
        elsif (($strType eq 'path' || $strType eq 'file') &&
               ($strSubKey eq MANIFEST_SUBKEY_MODE))
        {
            return true;
        }
        elsif ($strType eq 'file' &&
               ($strSubKey eq MANIFEST_SUBKEY_CHECKSUM ||
                $strSubKey eq MANIFEST_SUBKEY_FUTURE ||
                $strSubKey eq MANIFEST_SUBKEY_TIMESTAMP ||
                $strSubKey eq MANIFEST_SUBKEY_REFERENCE ||
                $strSubKey eq MANIFEST_SUBKEY_SIZE))
        {
            return true;
        }
        elsif ($strType eq 'link' &&
               $strSubKey eq MANIFEST_SUBKEY_DESTINATION)
        {
            return true;
        }
    }
    elsif ($strSection eq INI_SECTION_BACKREST)
    {
        return true;
    }
    elsif ($strSection eq MANIFEST_SECTION_BACKUP)
    {
        if ($strKey eq MANIFEST_KEY_ARCHIVE_START ||
            $strKey eq MANIFEST_KEY_ARCHIVE_STOP ||
            $strKey eq MANIFEST_KEY_LABEL ||
            $strKey eq MANIFEST_KEY_PRIOR ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_COPY_START ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_START ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_STOP ||
            $strKey eq MANIFEST_KEY_TYPE)
        {
            return true;
        }
    }
    elsif ($strSection eq MANIFEST_SECTION_BACKUP_DB)
    {
        if ($strKey eq MANIFEST_KEY_CATALOG ||
            $strKey eq MANIFEST_KEY_CONTROL ||
            $strKey eq MANIFEST_KEY_SYSTEM_ID ||
            $strKey eq MANIFEST_KEY_DB_VERSION)
        {
            return true;
        }
    }
    elsif ($strSection eq MANIFEST_SECTION_BACKUP_OPTION)
    {
        if ($strKey eq MANIFEST_KEY_ARCHIVE_CHECK ||
            $strKey eq MANIFEST_KEY_ARCHIVE_COPY ||
            $strKey eq MANIFEST_KEY_COMPRESS ||
            $strKey eq MANIFEST_KEY_HARDLINK ||
            $strKey eq MANIFEST_KEY_ONLINE)
        {
            return true;
        }
    }
    elsif ($strSection eq MANIFEST_SECTION_BACKUP_PATH)
    {
        if ($strKey eq MANIFEST_KEY_BASE &&
            $strSubKey eq MANIFEST_SUBKEY_PATH)
        {
            return true;
        }

        if ($strKey =~ /^tablespace\// &&
            ($strSubKey eq MANIFEST_SUBKEY_LINK ||
             $strSubKey eq MANIFEST_SUBKEY_PATH))
        {
            return true;
        }
    }

    confess &log(ASSERT, "manifest section '${strSection}', key '${strKey}'" .
                          (defined($strSubKey) ? ", subkey '$strSubKey'" : '') . ' is not valid');
}

####################################################################################################################################
# tablespacePathGet
#
# Get the unique path assigned by Postgres for the tablespace.
####################################################################################################################################
sub tablespacePathGet
{
    my $self = shift;

    return('PG_' . $self->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) .
           '_' . $self->get(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_CATALOG));
}

####################################################################################################################################
# build
#
# Build the manifest object.
####################################################################################################################################
sub build
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strDbClusterPath,
        $oLastManifest,
        $bOnline,
        $oTablespaceMapRef,
        $strLevel
    ) =
        logDebugParam
        (
            OP_MANIFEST_BUILD, \@_,
            {name => 'oFile'},
            {name => 'strDbClusterPath'},
            {name => 'oLastManifest', required => false},
            {name => 'bOnline'},
            {name => 'oTablespaceMapRef', required => false},
            {name => 'strLevel', required => false}
        );

    # If no level is defined then it must be base
    my $strTablespacePath;

    if (!defined($strLevel))
    {
        $strLevel = MANIFEST_KEY_BASE;

        if (defined($oLastManifest))
        {
            $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef,
                       $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
        }

        # If not online then build the tablespace map from pg_tblspc path
        if (!$bOnline && !defined($oTablespaceMapRef))
        {
            $oTablespaceMapRef = {};

            my %oTablespaceManifestHash;
            $oFile->manifest(PATH_DB_ABSOLUTE, $strDbClusterPath . '/' . PATH_PG_TBLSPC, \%oTablespaceManifestHash);

            foreach my $strName (sort(CORE::keys(%{$oTablespaceManifestHash{name}})))
            {
                if ($strName eq '.' or $strName eq '..')
                {
                    next;
                }

                logDebugMisc($strOperation, "found tablespace ${strName}");

                ${$oTablespaceMapRef}{oid}{$strName}{name} = $strName;
            }
        }
    }
    elsif ($self->numericGet(MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION) >= 9.0)
    {
        $strTablespacePath = $self->tablespacePathGet();
    }

    # Get the manifest for this level
    my %oManifestHash;
    $oFile->manifest(PATH_DB_ABSOLUTE, $strDbClusterPath .
                     (defined($strTablespacePath) ? "/${strTablespacePath}" : ''), \%oManifestHash);

    $self->set(MANIFEST_SECTION_BACKUP_PATH, $strLevel, MANIFEST_SUBKEY_PATH, $strDbClusterPath);

    # Loop though all paths/files/links in the manifest
    foreach my $strName (sort(CORE::keys(%{$oManifestHash{name}})))
    {
        # Skip certain files during backup
        if (($strName =~ /^pg\_xlog\/.*/ && $bOnline) ||       # pg_xlog/ - this will be reconstructed
            $strName =~ /^postmaster\.pid$/ ||                 # postmaster.pid - to avoid confusing postgres when restoring
            $strName =~ /^backup\_label\.old$/ ||              # backup_label.old - old backup labels are not useful
            $strName =~ /^recovery\.done$/ ||                  # recovery.done - doesn't make sense to backup this file
            $strName =~ /^recovery\.conf$/)                    # recovery.conf - doesn't make sense to backup this file
        {
            next;
        }

        my $cType = $oManifestHash{name}{$strName}{type};
        my $strLinkDestination = $oManifestHash{name}{$strName}{link_destination};
        my $strSection = "${strLevel}:path";

        if ($cType eq 'f')
        {
            $strSection = "${strLevel}:file";
        }
        elsif ($cType eq 'l')
        {
            $strSection = "${strLevel}:link";
        }
        elsif ($cType ne 'd')
        {
            confess &log(ASSERT, "unrecognized file type $cType for file $strName");
        }

        # Make sure that pg_tblspc contains only absolute links that do not point inside PGDATA
        if (index($strName, PATH_PG_TBLSPC . '/') == 0 && $strLevel eq MANIFEST_KEY_BASE)
        {
            # Check for files in pg_tblspc that are not links
            if ($oManifestHash{name}{$strName}{type} ne 'l')
            {
                confess &log(ERROR, "/${strName} is not a symlink - pg_tblspc should contain only symlinks", ERROR_LINK_EXPECTED);
            }

            # Check for relative link targets
            if (index($oManifestHash{name}{$strName}{link_destination}, '/') != 0)
            {
                confess &log(ERROR, 'tablespace symlink ' . $oManifestHash{name}{$strName}{link_destination} .
                             ' must be absolute', ERROR_ABSOLUTE_LINK_EXPECTED);
            }

            # Check for tablespaces in PGDATA
            if (index($oManifestHash{name}{$strName}{link_destination}, $strDbClusterPath) == 0)
            {
                confess &log(ERROR, 'tablespace symlink ' . $oManifestHash{name}{$strName}{link_destination} .
                             ' must not be in $PGDATA', ERROR_TABLESPACE_IN_PGDATA);
            }
        }

        # User and group required for all types
        $self->set($strSection, $strName, MANIFEST_SUBKEY_USER, $oManifestHash{name}{$strName}{user});
        $self->set($strSection, $strName, MANIFEST_SUBKEY_GROUP, $oManifestHash{name}{$strName}{group});

        # Mode for required file and path type only
        if ($cType eq 'f' || $cType eq 'd')
        {
            $self->set($strSection, $strName, MANIFEST_SUBKEY_MODE, $oManifestHash{name}{$strName}{mode});
        }

        # Modification time and size required for file type only
        if ($cType eq 'f')
        {
            $self->set($strSection, $strName, MANIFEST_SUBKEY_TIMESTAMP,
                       $oManifestHash{name}{$strName}{modification_time} + 0);
            $self->set($strSection, $strName, MANIFEST_SUBKEY_SIZE, $oManifestHash{name}{$strName}{size} + 0);
        }

        # Link destination required for link type only
        if ($cType eq 'l')
        {
            $self->set($strSection, $strName, MANIFEST_SUBKEY_DESTINATION,
                       $oManifestHash{name}{$strName}{link_destination});

            # If this is a tablespace then follow the link
            if (index($strName, PATH_PG_TBLSPC . '/') == 0 && $strLevel eq MANIFEST_KEY_BASE)
            {
                my $strTablespaceOid = basename($strName);
                my $strTablespaceName = MANIFEST_TABLESPACE . '/' . ${$oTablespaceMapRef}{oid}{$strTablespaceOid}{name};

                $self->set(MANIFEST_SECTION_BACKUP_PATH, $strTablespaceName,
                           MANIFEST_SUBKEY_LINK, $strTablespaceOid);
                $self->set(MANIFEST_SECTION_BACKUP_PATH, $strTablespaceName,
                           MANIFEST_SUBKEY_PATH, $strLinkDestination);

                $self->build($oFile, $strLinkDestination, $oLastManifest, $bOnline, $oTablespaceMapRef,
                             $strTablespaceName);
            }
        }
    }

    # If this is the base level then do post-processing
    if ($strLevel eq MANIFEST_KEY_BASE)
    {
        my $bTimeInFuture = false;

        my $lTimeBegin = $oFile->wait(PATH_DB_ABSOLUTE);

        # Loop through all backup paths (base and tablespaces)
        foreach my $strPathKey ($self->keys(MANIFEST_SECTION_BACKUP_PATH))
        {
            my $strSection = "${strPathKey}:file";

            # Make sure file section exists
            if ($self->test($strSection))
            {
                # Loop though all files
                foreach my $strName ($self->keys($strSection))
                {
                    # If modification time is in the future (in this backup OR the last backup) set warning flag and do not
                    # allow a reference
                    if ($self->numericGet($strSection, $strName, MANIFEST_SUBKEY_TIMESTAMP) > $lTimeBegin ||
                        (defined($oLastManifest) && $oLastManifest->test($strSection, $strName, MANIFEST_SUBKEY_FUTURE, 'y')))
                    {
                        $bTimeInFuture = true;

                        # Only mark as future if still in the future in the current backup
                        if ($self->numericGet($strSection, $strName, MANIFEST_SUBKEY_TIMESTAMP) > $lTimeBegin)
                        {
                            $self->set($strSection, $strName, MANIFEST_SUBKEY_FUTURE, 'y');
                        }
                    }
                    # Else check if modification time and size are unchanged since last backup
                    elsif (defined($oLastManifest) && $oLastManifest->test($strSection, $strName) &&
                           $self->numericGet($strSection, $strName, MANIFEST_SUBKEY_SIZE) ==
                               $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_SIZE) &&
                           $self->numericGet($strSection, $strName, MANIFEST_SUBKEY_TIMESTAMP) ==
                               $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_TIMESTAMP))
                    {
                        # Copy reference from previous backup if possible
                        if ($oLastManifest->test($strSection, $strName, MANIFEST_SUBKEY_REFERENCE))
                        {
                            $self->set($strSection, $strName, MANIFEST_SUBKEY_REFERENCE,
                                       $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_REFERENCE));
                        }
                        # Otherwise the reference is to the previous backup
                        else
                        {
                            $self->set($strSection, $strName, MANIFEST_SUBKEY_REFERENCE,
                                       $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
                        }

                        # Copy the checksum from previous manifest
                        if ($oLastManifest->test($strSection, $strName, MANIFEST_SUBKEY_CHECKSUM))
                        {
                            $self->set($strSection, $strName, MANIFEST_SUBKEY_CHECKSUM,
                                       $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_CHECKSUM));
                        }
                    }
                }
            }
        }

        # Warn if any files in the current backup are in the future
        if ($bTimeInFuture)
        {
            &log(WARN, "some files have timestamps in the future - they will be copied to prevent possible race conditions");
        }

        # Record the time when copying will start
        $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef, $lTimeBegin + 1);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation
    );
}

1;
