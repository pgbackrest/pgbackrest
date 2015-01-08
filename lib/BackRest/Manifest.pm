####################################################################################################################################
# MANIFEST MODULE
####################################################################################################################################
package BackRest::Manifest;

use threads;
use strict;
use warnings;
use Carp;

use File::Basename qw(dirname);

use lib dirname($0);
use BackRest::Utility;

# Exports
use Exporter qw(import);
our @EXPORT = qw(MANIFEST_SECTION_BACKUP MANIFEST_SECTION_BACKUP_OPTION MANIFEST_SECTION_BACKUP_PATH
                 MANIFEST_SECTION_BACKUP_TABLESPACE

                 MANIFEST_KEY_LABEL

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

    MANIFEST_KEY_LABEL                  => 'label',

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

    return $self;
}

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

    # Section must always be defined
    if (!defined($strSection))
    {
        confess &log(ASSERT, 'section is not defined');
    }

    # Set default for required
    $bRequired = defined($bRequired) ? $bRequired : true;

    # Store the result
    my $oResult;

    if (defined($strSubValue))
    {
        if (!defined($strValue))
        {
            confess &log(ASSERT, 'subvalue requested bu value is not defined');
        }

        if (defined(${$self}{$strSection}{$strValue}))
        {
            $oResult = ${$self}{$strSection}{$strValue}{$strSubValue};
        }
    }
    elsif (defined($strValue))
    {
        if (defined(${$self}{$strSection}))
        {
            $oResult = ${$self}{$strSection}{$strValue};
        }
    }
    else
    {
        $oResult = ${$self}{$strSection};
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

    # Make sure the keys are valid
    $self->valid($strSection, $strSubKey, $strValue);

    if (defined($strSubKey))
    {
        ${$self}{$strSection}{$strKey}{$strSubKey} = $strValue;
    }
    else
    {
        ${$self}{$strSection}{$strKey} = $strValue;
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
