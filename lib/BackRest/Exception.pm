####################################################################################################################################
# EXCEPTION MODULE
####################################################################################################################################
package BackRest::Exception;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);

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
    ERROR_COMMAND_REQUIRED             => 105,
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
    ERROR_PATH_CREATE                  => 122,
    ERROR_COMMAND_INVALID              => 123,
    ERROR_HOST_CONNECT                 => 124,
    ERROR_LOCK_ACQUIRE                 => 125,
    ERROR_BACKUP_MISMATCH              => 126,
    ERROR_FILE_SYNC                    => 127,
    ERROR_PATH_OPEN                    => 128,
    ERROR_PATH_SYNC                    => 129,
    ERROR_FILE_MISSING                 => 130,

    ERROR_UNKNOWN                      => 199
};

our @EXPORT = qw(ERROR_ASSERT ERROR_CHECKSUM ERROR_CONFIG ERROR_FILE_INVALID ERROR_FORMAT ERROR_COMMAND_REQUIRED
                 ERROR_OPTION_INVALID ERROR_OPTION_INVALID_VALUE ERROR_OPTION_INVALID_RANGE ERROR_OPTION_INVALID_PAIR
                 ERROR_OPTION_DUPLICATE_KEY ERROR_OPTION_NEGATE ERROR_OPTION_REQUIRED ERROR_POSTMASTER_RUNNING ERROR_PROTOCOL
                 ERROR_RESTORE_PATH_NOT_EMPTY ERROR_FILE_OPEN ERROR_FILE_READ ERROR_PARAM_REQUIRED ERROR_ARCHIVE_MISMATCH
                 ERROR_ARCHIVE_DUPLICATE ERROR_VERSION_NOT_SUPPORTED ERROR_PATH_CREATE ERROR_COMMAND_INVALID ERROR_HOST_CONNECT
                 ERROR_UNKNOWN ERROR_LOCK_ACQUIRE ERROR_BACKUP_MISMATCH ERROR_FILE_SYNC ERROR_PATH_OPEN ERROR_PATH_SYNC
                 ERROR_FILE_MISSING);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name
    my $iCode = shift;       # Error code
    my $strMessage = shift;  # ErrorMessage
    my $strTrace = shift;    # Stack trace

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize exception
    $self->{iCode} = $iCode;
    $self->{strMessage} = $strMessage;
    $self->{strTrace} = $strTrace;

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

####################################################################################################################################
# TRACE
####################################################################################################################################
sub trace
{
    my $self = shift;

    return $self->{strTrace};
}

1;
