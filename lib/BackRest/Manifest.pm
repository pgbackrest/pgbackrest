####################################################################################################################################
# MANIFEST MODULE
####################################################################################################################################
package BackRest::Manifest;

use threads;
use strict;
use warnings;
use Carp;

use File::Basename qw(dirname);
use Time::Local qw(timelocal);

use lib dirname($0);
use BackRest::Utility;

# Exports
use Exporter qw(import);
our @EXPORT = qw(MANIFEST_SECTION_BACKUP MANIFEST_SECTION_BACKUP_OPTION MANIFEST_SECTION_BACKUP_PATH
                 MANIFEST_SECTION_BACKUP_TABLESPACE

                 MANIFEST_KEY_COMPRESS MANIFEST_KEY_HARDLINK MANIFEST_KEY_LABEL MANIFEST_KEY_TIMESTAMP_COPY_START

                 MANIFEST_SUBKEY_CHECKSUM MANIFEST_SUBKEY_DESTINATION MANIFEST_SUBKEY_FUTURE MANIFEST_SUBKEY_GROUP
                 MANIFEST_SUBKEY_LINK MANIFEST_SUBKEY_MODE MANIFEST_SUBKEY_MODIFICATION_TIME MANIFEST_SUBKEY_PATH
                 MANIFEST_SUBKEY_REFERENCE MANIFEST_SUBKEY_SIZE MANIFEST_SUBKEY_USER);

####################################################################################################################################
# MANIFEST Constants
####################################################################################################################################
use constant
{
    MANIFEST_SECTION_BACKUP             => 'backup',
    MANIFEST_SECTION_BACKUP_OPTION      => 'backup:option',
    MANIFEST_SECTION_BACKUP_PATH        => 'backup:path',
    MANIFEST_SECTION_BACKUP_TABLESPACE  => 'backup:tablespace',

    MANIFEST_KEY_COMPRESS               => 'compress',
    MANIFEST_KEY_HARDLINK               => 'hardlink',
    MANIFEST_KEY_LABEL                  => 'label',
    MANIFEST_KEY_TIMESTAMP_COPY_START   => 'timestamp-copy-start',

    MANIFEST_SUBKEY_CHECKSUM            => 'checksum',
    MANIFEST_SUBKEY_DESTINATION         => 'link_destination',
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

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Create the manifest hash
    $self->{oManifest} = {};

    # Load the manifest if a filename is provided
    if (defined($strFileName))
    {
        ini_load($strFileName, $self->{oManifest});
    }

    return $self;
}

####################################################################################################################################
# SAVE
#
# Save the config file.
####################################################################################################################################
# sub save
# {
#     my $self = shift;
#     my $strFileName = shift; # Filename to save manifest to
#
#     # Save the config file
#     ini_save($strFileName, $self);
#
#     return $self;
# }

####################################################################################################################################
# GET
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
    $self->valid($strSection, $strSubKey, $strValue);

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

    # Section and key must always be defined
    if (!defined($strSection) || !defined($strKey))
    {
        confess &log(ASSERT, 'section or key is not defined');
    }

    if ($strSection eq MANIFEST_SECTION_BACKUP_PATH)
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

    if ($self->test($strSection))
    {
        return sort(keys $self->get($strSection));
    }

    confess 'nothing was returned';

    return [];
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

1;
