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

                 MANIFEST_VALUE_LABEL

                 MANIFEST_SUBVALUE_CHECKSUM MANIFEST_SUBVALUE_DESTINATION MANIFEST_SUBVALUE_FUTURE MANIFEST_SUBVALUE_GROUP
                 MANIFEST_SUBVALUE_MODE MANIFEST_SUBVALUE_MODIFICATION_TIME MANIFEST_SUBVALUE_REFERENCE MANIFEST_SUBVALUE_SIZE
                 MANIFEST_SUBVALUE_USER);

####################################################################################################################################
# MANIFEST Constants
####################################################################################################################################
use constant
{
    MANIFEST_SECTION_BACKUP             => 'backup',
    MANIFEST_SECTION_BACKUP_OPTION      => 'backup:option',
    MANIFEST_SECTION_BACKUP_PATH        => 'backup:path',

    MANIFEST_VALUE_LABEL                => 'label',

    MANIFEST_SUBVALUE_CHECKSUM          => 'checksum',
    MANIFEST_SUBVALUE_DESTINATION       => 'link_destination',
    MANIFEST_SUBVALUE_FUTURE            => 'future',
    MANIFEST_SUBVALUE_GROUP             => 'group',
    MANIFEST_SUBVALUE_MODE              => 'permission',
    MANIFEST_SUBVALUE_MODIFICATION_TIME => 'modification_time',
    MANIFEST_SUBVALUE_REFERENCE         => 'reference',
    MANIFEST_SUBVALUE_SIZE              => 'size',
    MANIFEST_SUBVALUE_USER              => 'user'
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
# keys
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
