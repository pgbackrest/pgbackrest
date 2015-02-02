####################################################################################################################################
# MANIFEST MODULE
####################################################################################################################################
package BackRest::Manifest;

use threads;
use strict;
use warnings;
use Carp;

use File::Basename qw(dirname basename);
use Time::Local qw(timelocal);
use Digest::SHA;

use lib dirname($0);
use BackRest::Utility;
use BackRest::File;

# Exports
use Exporter qw(import);
our @EXPORT = qw(MANIFEST_PATH MANIFEST_FILE MANIFEST_LINK

                 MANIFEST_SECTION_BACKUP MANIFEST_SECTION_BACKUP_OPTION MANIFEST_SECTION_BACKUP_PATH
                 MANIFEST_SECTION_BACKUP_TABLESPACE

                 MANIFEST_KEY_ARCHIVE_START MANIFEST_KEY_ARCHIVE_STOP MANIFEST_KEY_BASE MANIFEST_KEY_CHECKSUM MANIFEST_KEY_COMPRESS
                 MANIFEST_KEY_HARDLINK MANIFEST_KEY_LABEL MANIFEST_KEY_PRIOR MANIFEST_KEY_REFERENCE MANIFEST_KEY_TIMESTAMP_DB_START
                 MANIFEST_KEY_TIMESTAMP_DB_STOP MANIFEST_KEY_TIMESTAMP_COPY_START MANIFEST_KEY_TIMESTAMP_START
                 MANIFEST_KEY_TIMESTAMP_STOP MANIFEST_KEY_TYPE MANIFEST_KEY_VERSION

                 MANIFEST_SUBKEY_CHECKSUM MANIFEST_SUBKEY_DESTINATION MANIFEST_SUBKEY_EXISTS MANIFEST_SUBKEY_FUTURE
                 MANIFEST_SUBKEY_GROUP MANIFEST_SUBKEY_LINK MANIFEST_SUBKEY_MODE MANIFEST_SUBKEY_MODIFICATION_TIME
                 MANIFEST_SUBKEY_PATH MANIFEST_SUBKEY_REFERENCE MANIFEST_SUBKEY_SIZE MANIFEST_SUBKEY_USER);

####################################################################################################################################
# MANIFEST Constants
####################################################################################################################################
use constant
{
    MANIFEST_PATH                       => 'path',
    MANIFEST_FILE                       => 'file',
    MANIFEST_LINK                       => 'link',

    MANIFEST_SECTION_BACKUP             => 'backup',
    MANIFEST_SECTION_BACKUP_OPTION      => 'backup:option',
    MANIFEST_SECTION_BACKUP_PATH        => 'backup:path',
    MANIFEST_SECTION_BACKUP_TABLESPACE  => 'backup:tablespace',

    MANIFEST_KEY_ARCHIVE_START          => 'archive-start',
    MANIFEST_KEY_ARCHIVE_STOP           => 'archive-stop',
    MANIFEST_KEY_BASE                   => 'base',
    MANIFEST_KEY_CHECKSUM               => 'checksum',
    MANIFEST_KEY_COMPRESS               => 'compress',
    MANIFEST_KEY_HARDLINK               => 'hardlink',
    MANIFEST_KEY_LABEL                  => 'label',
    MANIFEST_KEY_PRIOR                  => 'prior',
    MANIFEST_KEY_REFERENCE              => 'reference',
    MANIFEST_KEY_TIMESTAMP_DB_START     => 'timestamp-db-start',
    MANIFEST_KEY_TIMESTAMP_DB_STOP      => 'timestamp-db-stop',
    MANIFEST_KEY_TIMESTAMP_COPY_START   => 'timestamp-copy-start',
    MANIFEST_KEY_TIMESTAMP_START        => 'timestamp-start',
    MANIFEST_KEY_TIMESTAMP_STOP         => 'timestamp-stop',
    MANIFEST_KEY_TYPE                   => 'type',
    MANIFEST_KEY_VERSION                => 'version',

    MANIFEST_SUBKEY_CHECKSUM            => 'checksum',
    MANIFEST_SUBKEY_DESTINATION         => 'link_destination',
    MANIFEST_SUBKEY_EXISTS              => 'exists',
    MANIFEST_SUBKEY_FUTURE              => 'future',
    MANIFEST_SUBKEY_GROUP               => 'group',
    MANIFEST_SUBKEY_LINK                => 'link',
    MANIFEST_SUBKEY_MODE                => 'permission',
    MANIFEST_SUBKEY_MODIFICATION_TIME   => 'modification_time',
    MANIFEST_SUBKEY_PATH                => 'path',
    MANIFEST_SUBKEY_REFERENCE           => 'reference',
    MANIFEST_SUBKEY_SIZE                => 'size',
    MANIFEST_SUBKEY_USER                => 'user'
};

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name
    my $strFileName = shift; # Filename to load manifest from
    my $bLoad = shift;       # Load the manifest?

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Filename must be specified
    if (!defined($strFileName))
    {
        confess &log(ASSERT, 'filename must be provided');
    }

    # Set variables
    my $oManifest = {};
    $self->{oManifest} = $oManifest;
    $self->{strFileName} = $strFileName;

    # Load the manifest if specified
    if (!(defined($bLoad) && $bLoad == false))
    {
        ini_load($strFileName, $oManifest);

        # Make sure the manifest is valid by testing checksum
        my $strChecksum = $self->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_CHECKSUM);
        my $strTestChecksum = $self->hash();

        if ($strChecksum ne $strTestChecksum)
        {
            confess &log(ERROR, "backup.manifest checksum is invalid, should be ${strTestChecksum}");
        }
    }

    return $self;
}

####################################################################################################################################
# SAVE
#
# Save the manifest.
####################################################################################################################################
sub save
{
    my $self = shift;

    # Create the checksum
    $self->hash();

    # Save the config file
    ini_save($self->{strFileName}, $self->{oManifest});
}

####################################################################################################################################
# HASH
#
# Generate hash for the manifest.
####################################################################################################################################
sub hash
{
    my $self = shift;

    my $oManifest = $self->{oManifest};

    # Remove the old checksum
    $self->remove(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_CHECKSUM);

    my $oSHA = Digest::SHA->new('sha1');

    foreach my $strSection ($self->keys())
    {
        $oSHA->add($strSection);

        foreach my $strKey ($self->keys($strSection))
        {
            $oSHA->add($strKey);

            my $strValue = $self->get($strSection, $strKey);

            if (!defined($strValue))
            {
                confess &log(ASSERT, "section ${strSection}, key ${$strKey} has undef value");
            }

            if (ref($strValue) eq "HASH")
            {
                foreach my $strSubKey ($self->keys($strSection, $strKey))
                {
                    my $strSubValue = $self->get($strSection, $strKey, $strSubKey);

                    if (!defined($strSubValue))
                    {
                        confess &log(ASSERT, "section ${strSection}, key ${strKey}, subkey ${strSubKey} has undef value");
                    }

                    $oSHA->add($strSubValue);
                }
            }
            else
            {
                $oSHA->add($strValue);
            }
        }
    }

    # Set the new checksum
    my $strHash = $oSHA->hexdigest();

    $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_CHECKSUM, undef, $strHash);

    return $strHash;
}

####################################################################################################################################
# HASH
#
# Get a value.
####################################################################################################################################
sub get
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $bRequired = shift;
    my $oDefault = shift;

    my $oManifest = $self->{oManifest};

    # Section must always be defined
    if (!defined($strSection))
    {
        confess &log(ASSERT, 'section is not defined');
    }

    # Set default for required
    $bRequired = defined($bRequired) ? $bRequired : true;

    # Store the result
    my $oResult = undef;

    if (defined($strSubValue))
    {
        if (!defined($strValue))
        {
            confess &log(ASSERT, 'subvalue requested bu value is not defined');
        }

        if (defined(${$oManifest}{$strSection}{$strValue}))
        {
            $oResult = ${$oManifest}{$strSection}{$strValue}{$strSubValue};
        }
    }
    elsif (defined($strValue))
    {
        if (defined(${$oManifest}{$strSection}))
        {
            $oResult = ${$oManifest}{$strSection}{$strValue};
        }
    }
    else
    {
        $oResult = ${$oManifest}{$strSection};
    }

    if (!defined($oResult) && $bRequired)
    {
        confess &log(ASSERT, "manifest section '$strSection'" . (defined($strValue) ? ", value '$strValue'" : '') .
                              (defined($strSubValue) ? ", subvalue '$strSubValue'" : '') . ' is required but not defined');
    }

    if (!defined($oResult) && defined($oDefault))
    {
        $oResult = $oDefault;
    }

    return $oResult
}

####################################################################################################################################
# SET
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

    my $oManifest = $self->{oManifest};

    # Make sure the keys are valid
    $self->valid($strSection, $strKey, $strSubKey);

    if (defined($strSubKey))
    {
        ${$oManifest}{$strSection}{$strKey}{$strSubKey} = $strValue;
    }
    else
    {
        ${$oManifest}{$strSection}{$strKey} = $strValue;
    }
}

####################################################################################################################################
# REMOVE
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

    my $oManifest = $self->{oManifest};

    # Make sure the keys are valid
    $self->valid($strSection, $strKey, $strSubKey, undef, true);

    if (defined($strSubKey))
    {
        delete(${$oManifest}{$strSection}{$strKey}{$strSubKey});
    }
    else
    {
        delete(${$oManifest}{$strSection}{$strKey});
    }
}

####################################################################################################################################
# VALID
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

        if ($strPath eq 'tablespace')
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
                $strSubKey eq MANIFEST_SUBKEY_EXISTS ||
                $strSubKey eq MANIFEST_SUBKEY_FUTURE ||
                $strSubKey eq MANIFEST_SUBKEY_MODIFICATION_TIME ||
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
    if ($strSection eq MANIFEST_SECTION_BACKUP)
    {
        if ($strKey eq MANIFEST_KEY_ARCHIVE_START ||
            $strKey eq MANIFEST_KEY_ARCHIVE_STOP ||
            $strKey eq MANIFEST_KEY_CHECKSUM ||
            $strKey eq MANIFEST_KEY_LABEL ||
            $strKey eq MANIFEST_KEY_PRIOR ||
            $strKey eq MANIFEST_KEY_REFERENCE ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_DB_START ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_DB_STOP ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_COPY_START ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_START ||
            $strKey eq MANIFEST_KEY_TIMESTAMP_STOP ||
            $strKey eq MANIFEST_KEY_TYPE ||
            $strKey eq MANIFEST_KEY_VERSION)
        {
            return true;
        }
    }
    elsif ($strSection eq MANIFEST_SECTION_BACKUP_OPTION)
    {
        if ($strKey eq MANIFEST_KEY_CHECKSUM ||
            $strKey eq MANIFEST_KEY_COMPRESS ||
            $strKey eq MANIFEST_KEY_HARDLINK)
        {
            return true;
        }
    }
    elsif ($strSection eq MANIFEST_SECTION_BACKUP_TABLESPACE)
    {
        if ($strSubKey eq 'link' ||
            $strSubKey eq 'path')
        {
            return true;
        }
    }
    elsif ($strSection eq MANIFEST_SECTION_BACKUP_PATH)
    {
        if ($strKey eq 'base' || $strKey =~ /^tablespace\:.*$/)
        {
            return true;
        }
    }

    confess &log(ASSERT, "manifest section '${strSection}', key '${strKey}'" .
                          (defined($strSubKey) ? ", subkey '$strSubKey'" : '') . ' is not valid');
}

####################################################################################################################################
# epoch
#
# Retrieves a value in the format YYYY-MM-DD HH24:MI:SS and converts to epoch time.
####################################################################################################################################
sub epoch
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;
    my $strSubKey = shift;

    my $strValue = $self->get($strSection, $strKey, $strSubKey);

    my ($iYear, $iMonth, $iDay, $iHour, $iMinute, $iSecond) = split(/[\s\-\:]+/, $strValue);

    return timelocal($iSecond, $iMinute, $iHour, $iDay , $iMonth - 1, $iYear);
}

####################################################################################################################################
# KEYS
#
# Get a list of keys.
####################################################################################################################################
sub keys
{
    my $self = shift;
    my $strSection = shift;
    my $strKey = shift;

    if (defined($strSection))
    {
        if ($self->test($strSection, $strKey))
        {
            return sort(keys $self->get($strSection, $strKey));
        }

        return [];
    }

    return sort(keys $self->{oManifest});
}

####################################################################################################################################
# TEST
#
# Test a value to see if it equals the supplied test value.  If no test value is given, tests that it is defined.
####################################################################################################################################
sub test
{
    my $self = shift;
    my $strSection = shift;
    my $strValue = shift;
    my $strSubValue = shift;
    my $strTest = shift;

    my $strResult = $self->get($strSection, $strValue, $strSubValue, false);

    if (defined($strResult))
    {
        if (defined($strTest))
        {
            return $strResult eq $strTest ? true : false;
        }

        return true;
    }

    return false;
}

####################################################################################################################################
# BUILD
#
# Build the manifest object.
####################################################################################################################################
sub build
{
    my $self = shift;
    my $oFile = shift;
    my $strDbClusterPath = shift;
    my $oLastManifest = shift;
    my $bNoStartStop = shift;
    my $oTablespaceMapRef = shift;
    my $strLevel = shift;

    &log(DEBUG, 'Manifest->build');

    # If no level is defined then it must be base
    if (!defined($strLevel))
    {
        $strLevel = 'base';

        if (defined($oLastManifest))
        {
            $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef,
                       $oLastManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LABEL));
        }

        # If bNoStartStop then build the tablespace map from pg_tblspc path
        if ($bNoStartStop)
        {
            $oTablespaceMapRef = {};

            my %oTablespaceManifestHash;
            $oFile->manifest(PATH_DB_ABSOLUTE, $strDbClusterPath . '/pg_tblspc', \%oTablespaceManifestHash);

            foreach my $strName (sort(CORE::keys $oTablespaceManifestHash{name}))
            {
                if ($strName eq '.' or $strName eq '..')
                {
                    next;
                }

                if ($oTablespaceManifestHash{name}{$strName}{type} ne 'l')
                {
                    confess &log(ERROR, "pg_tblspc/${strName} is not a link");
                }

                &log(DEBUG, "Found tablespace ${strName}");

                ${$oTablespaceMapRef}{oid}{$strName}{name} = $strName;
            }
        }
    }

    # Get the manifest for this level
    my %oManifestHash;
    $oFile->manifest(PATH_DB_ABSOLUTE, $strDbClusterPath, \%oManifestHash);

    $self->set(MANIFEST_SECTION_BACKUP_PATH, $strLevel, undef, $strDbClusterPath);

    # Loop though all paths/files/links in the manifest
    foreach my $strName (sort(CORE::keys $oManifestHash{name}))
    {
        # Skip certain files during backup
        if (($strName =~ /^pg\_xlog\/.*/ && !$bNoStartStop) || # pg_xlog/ - this will be reconstructed
            $strName =~ /^postmaster\.pid$/ ||                 # postmaster.pid - to avoid confusing postgres when restoring
            $strName =~ /^recovery\.conf$/)                    # recovery.conf - doesn't make sense to backup this file
        {
            next;
        }

        my $cType = $oManifestHash{name}{"${strName}"}{type};
        my $strLinkDestination = $oManifestHash{name}{"${strName}"}{link_destination};
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

        # User and group required for all types
        $self->set($strSection, $strName, MANIFEST_SUBKEY_USER, $oManifestHash{name}{"${strName}"}{user});
        $self->set($strSection, $strName, MANIFEST_SUBKEY_GROUP, $oManifestHash{name}{"${strName}"}{group});

        # Mode for required file and path type only
        if ($cType eq 'f' || $cType eq 'd')
        {
            $self->set($strSection, $strName, MANIFEST_SUBKEY_MODE, $oManifestHash{name}{"${strName}"}{permission});
        }

        # Modification time and size required for file type only
        if ($cType eq 'f')
        {
            $self->set($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME,
                       $oManifestHash{name}{"${strName}"}{modification_time} + 0);
            $self->set($strSection, $strName, MANIFEST_SUBKEY_SIZE, $oManifestHash{name}{"${strName}"}{size} + 0);
        }

        # Link destination required for link type only
        if ($cType eq 'l')
        {
            $self->set($strSection, $strName, MANIFEST_SUBKEY_DESTINATION,
                       $oManifestHash{name}{"${strName}"}{link_destination});

            # If this is a tablespace then follow the link
            if (index($strName, 'pg_tblspc/') == 0 && $strLevel eq 'base')
            {
                my $strTablespaceOid = basename($strName);
                my $strTablespaceName = ${$oTablespaceMapRef}{oid}{$strTablespaceOid}{name};

                $self->set(MANIFEST_SECTION_BACKUP_TABLESPACE, $strTablespaceName,
                           MANIFEST_SUBKEY_LINK, $strTablespaceOid);
                $self->set(MANIFEST_SECTION_BACKUP_TABLESPACE, $strTablespaceName,
                           MANIFEST_SUBKEY_PATH, $strLinkDestination);

                $self->build($oFile, $strLinkDestination, $oLastManifest, $bNoStartStop, $oTablespaceMapRef,
                             "tablespace:${strTablespaceName}");
            }
        }
    }

    # If this is the base level then do post-processing
    if ($strLevel eq 'base')
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
                    if ($self->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME) > $lTimeBegin ||
                        (defined($oLastManifest) && $oLastManifest->test($strSection, $strName, MANIFEST_SUBKEY_FUTURE, 'y')))
                    {
                        $bTimeInFuture = true;

                        # Only mark as future if still in the future in the current backup
                        if ($self->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME) > $lTimeBegin)
                        {
                            $self->set($strSection, $strName, MANIFEST_SUBKEY_FUTURE, 'y');
                        }
                    }
                    # Else check if modification time and size are unchanged since last backup
                    elsif (defined($oLastManifest) && $oLastManifest->test($strSection, $strName) &&
                           $self->get($strSection, $strName, MANIFEST_SUBKEY_SIZE) ==
                               $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_SIZE) &&
                           $self->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME) ==
                               $oLastManifest->get($strSection, $strName, MANIFEST_SUBKEY_MODIFICATION_TIME))
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

                        # Build the manifest reference list - not used for processing but is useful for debugging
                        my $strFileReference = $self->get($strSection, $strName, MANIFEST_SUBKEY_REFERENCE);

                        my $strManifestReference = $self->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_REFERENCE,
                                                              undef, false);

                        if (!defined($strManifestReference))
                        {
                            $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_REFERENCE, undef, $strFileReference);
                        }
                        else
                        {
                            if ($strManifestReference !~ /^$strFileReference|,$strFileReference/)
                            {
                                $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_REFERENCE, undef,
                                           $strManifestReference . ",${strFileReference}");
                            }
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
        $self->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TIMESTAMP_COPY_START, undef,
                   timestamp_string_get(undef, $lTimeBegin + 1));
    }

}

1;
