####################################################################################################################################
# EXCEPTION MODULE
####################################################################################################################################
package BackRest::Exception;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use Exporter qw(import);

####################################################################################################################################
# Exception Codes
####################################################################################################################################
use constant
{
    ERROR_ASSERT                       => 100,
    ERROR_CHECKSUM                     => 101,
    ERROR_CONFIG                       => 102,
    ERROR_FILE_INVALID                 => 103,
    ERROR_FORMAT                       => 104,
    ERROR_OPERATION_REQUIRED           => 105,
    ERROR_OPTION_INVALID               => 106,
    ERROR_OPTION_INVALID_VALUE         => 107,
    ERROR_OPTION_INVALID_RANGE         => 108,
    ERROR_OPTION_INVALID_PAIR          => 109,
    ERROR_OPTION_DUPLICATE_KEY         => 110,
    ERROR_OPTION_NEGATE                => 111,
    ERROR_OPTION_REQUIRED              => 112,
    ERROR_POSTMASTER_RUNNING           => 113,
    ERROR_PROTOCOL                     => 114,
    ERROR_RESTORE_PATH_NOT_EMPTY       => 115,
    ERROR_FILE_OPEN                    => 116,
    ERROR_FILE_READ                    => 117,
    ERROR_PARAM_REQUIRED               => 118,
    ERROR_ARCHIVE_MISMATCH             => 119,
    ERROR_ARCHIVE_DUPLICATE            => 120,
    ERROR_VERSION_NOT_SUPPORTED        => 121,
    ERROR_PATH_CREATE                  => 122
};

our @EXPORT = qw(ERROR_ASSERT ERROR_CHECKSUM ERROR_CONFIG ERROR_FILE_INVALID ERROR_FORMAT ERROR_OPERATION_REQUIRED
                 ERROR_OPTION_INVALID ERROR_OPTION_INVALID_VALUE ERROR_OPTION_INVALID_RANGE ERROR_OPTION_INVALID_PAIR
                 ERROR_OPTION_DUPLICATE_KEY ERROR_OPTION_NEGATE ERROR_OPTION_REQUIRED ERROR_POSTMASTER_RUNNING ERROR_PROTOCOL
                 ERROR_RESTORE_PATH_NOT_EMPTY ERROR_FILE_OPEN ERROR_FILE_READ ERROR_PARAM_REQUIRED ERROR_ARCHIVE_MISMATCH
                 ERROR_ARCHIVE_DUPLICATE ERROR_VERSION_NOT_SUPPORTED ERROR_PATH_CREATE);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name
    my $iCode = shift;       # Error code
    my $strMessage = shift;  # ErrorMessage

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize exception
    $self->{iCode} = $iCode;
    $self->{strMessage} = $strMessage;

    return $self;
}

####################################################################################################################################
# CODE
####################################################################################################################################
sub code
{
    my $self = shift;

    return $self->{iCode};
}

####################################################################################################################################
# MESSAGE
####################################################################################################################################
sub message
{
    my $self = shift;

    return $self->{strMessage};
}

1;
