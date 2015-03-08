####################################################################################################################################
# EXCEPTION MODULE
####################################################################################################################################
package BackRest::Exception;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

####################################################################################################################################
# Exports
####################################################################################################################################
use Exporter qw(import);
our @EXPORT = qw(ERROR_ASSERT ERROR_CHECKSUM ERROR_CONFIG ERROR_OPERATION_REQUIRED ERROR_OPTION_REQUIRED ERROR_OPTION_INVALID
                 ERROR_OPTION_INVALID_VALUE ERROR_POSTMASTER_RUNNING ERROR_PROTOCOL ERROR_RESTORE_PATH_NOT_EMPTY ERROR_FORMAT);

####################################################################################################################################
# Exception Codes
####################################################################################################################################
use constant
{
    ERROR_ASSERT                       => 100,
    ERROR_CHECKSUM                     => 101,
    ERROR_CONFIG                       => 102,
    ERROR_FORMAT                       => 103,
    ERROR_OPERATION_REQUIRED           => 104,
    ERROR_OPTION_REQUIRED              => 105,
    ERROR_OPTION_INVALID               => 106,
    ERROR_OPTION_INVALID_VALUE         => 107,
    ERROR_POSTMASTER_RUNNING           => 108,
    ERROR_PROTOCOL                     => 109,
    ERROR_RESTORE_PATH_NOT_EMPTY       => 110
};

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
