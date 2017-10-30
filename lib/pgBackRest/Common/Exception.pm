####################################################################################################################################
# COMMON EXCEPTION MODULE
####################################################################################################################################
package pgBackRest::Common::Exception;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);

use Scalar::Util qw(blessed);

use Exporter qw(import);
    our @EXPORT = qw();

####################################################################################################################################
# Exception codes
####################################################################################################################################
use constant ERROR_MINIMUM                                          => 25;
    push @EXPORT, qw(ERROR_MINIMUM);
use constant ERROR_MAXIMUM                                          => 125;
    push @EXPORT, qw(ERROR_MAXIMUM);

use constant ERROR_ASSERT                                           => ERROR_MINIMUM;
    push @EXPORT, qw(ERROR_ASSERT);
use constant ERROR_CHECKSUM                                         => ERROR_MINIMUM + 1;
    push @EXPORT, qw(ERROR_CHECKSUM);
use constant ERROR_CONFIG                                           => ERROR_MINIMUM + 2;
    push @EXPORT, qw(ERROR_CONFIG);
use constant ERROR_FILE_INVALID                                     => ERROR_MINIMUM + 3;
    push @EXPORT, qw(ERROR_FILE_INVALID);
use constant ERROR_FORMAT                                           => ERROR_MINIMUM + 4;
    push @EXPORT, qw(ERROR_FORMAT);
use constant ERROR_COMMAND_REQUIRED                                 => ERROR_MINIMUM + 5;
    push @EXPORT, qw(ERROR_COMMAND_REQUIRED);
use constant ERROR_OPTION_INVALID                                   => ERROR_MINIMUM + 6;
    push @EXPORT, qw(ERROR_OPTION_INVALID);
use constant ERROR_OPTION_INVALID_VALUE                             => ERROR_MINIMUM + 7;
    push @EXPORT, qw(ERROR_OPTION_INVALID_VALUE);
use constant ERROR_OPTION_INVALID_RANGE                             => ERROR_MINIMUM + 8;
    push @EXPORT, qw(ERROR_OPTION_INVALID_RANGE);
use constant ERROR_OPTION_INVALID_PAIR                              => ERROR_MINIMUM + 9;
    push @EXPORT, qw(ERROR_OPTION_INVALID_PAIR);
use constant ERROR_OPTION_DUPLICATE_KEY                             => ERROR_MINIMUM + 10;
    push @EXPORT, qw(ERROR_OPTION_DUPLICATE_KEY);
use constant ERROR_OPTION_NEGATE                                    => ERROR_MINIMUM + 11;
    push @EXPORT, qw(ERROR_OPTION_NEGATE);
use constant ERROR_OPTION_REQUIRED                                  => ERROR_MINIMUM + 12;
    push @EXPORT, qw(ERROR_OPTION_REQUIRED);
use constant ERROR_POSTMASTER_RUNNING                               => ERROR_MINIMUM + 13;
    push @EXPORT, qw(ERROR_POSTMASTER_RUNNING);
use constant ERROR_PROTOCOL                                         => ERROR_MINIMUM + 14;
    push @EXPORT, qw(ERROR_PROTOCOL);
use constant ERROR_PATH_NOT_EMPTY                                   => ERROR_MINIMUM + 15;
    push @EXPORT, qw(ERROR_PATH_NOT_EMPTY);
use constant ERROR_FILE_OPEN                                        => ERROR_MINIMUM + 16;
    push @EXPORT, qw(ERROR_FILE_OPEN);
use constant ERROR_FILE_READ                                        => ERROR_MINIMUM + 17;
    push @EXPORT, qw(ERROR_FILE_READ);
use constant ERROR_PARAM_REQUIRED                                   => ERROR_MINIMUM + 18;
    push @EXPORT, qw(ERROR_PARAM_REQUIRED);
use constant ERROR_ARCHIVE_MISMATCH                                 => ERROR_MINIMUM + 19;
    push @EXPORT, qw(ERROR_ARCHIVE_MISMATCH);
use constant ERROR_ARCHIVE_DUPLICATE                                => ERROR_MINIMUM + 20;
    push @EXPORT, qw(ERROR_ARCHIVE_DUPLICATE);
use constant ERROR_VERSION_NOT_SUPPORTED                            => ERROR_MINIMUM + 21;
    push @EXPORT, qw(ERROR_VERSION_NOT_SUPPORTED);
use constant ERROR_PATH_CREATE                                      => ERROR_MINIMUM + 22;
    push @EXPORT, qw(ERROR_PATH_CREATE);
use constant ERROR_COMMAND_INVALID                                  => ERROR_MINIMUM + 23;
    push @EXPORT, qw(ERROR_COMMAND_INVALID);
use constant ERROR_HOST_CONNECT                                     => ERROR_MINIMUM + 24;
    push @EXPORT, qw(ERROR_HOST_CONNECT);
use constant ERROR_LOCK_ACQUIRE                                     => ERROR_MINIMUM + 25;
    push @EXPORT, qw(ERROR_LOCK_ACQUIRE);
use constant ERROR_BACKUP_MISMATCH                                  => ERROR_MINIMUM + 26;
    push @EXPORT, qw(ERROR_BACKUP_MISMATCH);
use constant ERROR_FILE_SYNC                                        => ERROR_MINIMUM + 27;
    push @EXPORT, qw(ERROR_FILE_SYNC);
use constant ERROR_PATH_OPEN                                        => ERROR_MINIMUM + 28;
    push @EXPORT, qw(ERROR_PATH_OPEN);
use constant ERROR_PATH_SYNC                                        => ERROR_MINIMUM + 29;
    push @EXPORT, qw(ERROR_PATH_SYNC);
use constant ERROR_FILE_MISSING                                     => ERROR_MINIMUM + 30;
    push @EXPORT, qw(ERROR_FILE_MISSING);
use constant ERROR_DB_CONNECT                                       => ERROR_MINIMUM + 31;
    push @EXPORT, qw(ERROR_DB_CONNECT);
use constant ERROR_DB_QUERY                                         => ERROR_MINIMUM + 32;
    push @EXPORT, qw(ERROR_DB_QUERY);
use constant ERROR_DB_MISMATCH                                      => ERROR_MINIMUM + 33;
    push @EXPORT, qw(ERROR_DB_MISMATCH);
use constant ERROR_DB_TIMEOUT                                       => ERROR_MINIMUM + 34;
    push @EXPORT, qw(ERROR_DB_TIMEOUT);
use constant ERROR_FILE_REMOVE                                      => ERROR_MINIMUM + 35;
    push @EXPORT, qw(ERROR_FILE_REMOVE);
use constant ERROR_PATH_REMOVE                                      => ERROR_MINIMUM + 36;
    push @EXPORT, qw(ERROR_PATH_REMOVE);
use constant ERROR_STOP                                             => ERROR_MINIMUM + 37;
    push @EXPORT, qw(ERROR_STOP);
use constant ERROR_TERM                                             => ERROR_MINIMUM + 38;
    push @EXPORT, qw(ERROR_TERM);
use constant ERROR_FILE_WRITE                                       => ERROR_MINIMUM + 39;
    push @EXPORT, qw(ERROR_FILE_WRITE);
use constant ERROR_PROTOCOL_TIMEOUT                                 => ERROR_MINIMUM + 41;
    push @EXPORT, qw(ERROR_PROTOCOL_TIMEOUT);
use constant ERROR_FEATURE_NOT_SUPPORTED                            => ERROR_MINIMUM + 42;
    push @EXPORT, qw(ERROR_FEATURE_NOT_SUPPORTED);
use constant ERROR_ARCHIVE_COMMAND_INVALID                          => ERROR_MINIMUM + 43;
    push @EXPORT, qw(ERROR_ARCHIVE_COMMAND_INVALID);
use constant ERROR_LINK_EXPECTED                                    => ERROR_MINIMUM + 44;
    push @EXPORT, qw(ERROR_LINK_EXPECTED);
use constant ERROR_LINK_DESTINATION                                 => ERROR_MINIMUM + 45;
    push @EXPORT, qw(ERROR_LINK_DESTINATION);
use constant ERROR_TABLESPACE_IN_PGDATA                             => ERROR_MINIMUM + 46;
    push @EXPORT, qw(ERROR_TABLESPACE_IN_PGDATA);
use constant ERROR_HOST_INVALID                                     => ERROR_MINIMUM + 47;
    push @EXPORT, qw(ERROR_HOST_INVALID);
use constant ERROR_PATH_MISSING                                     => ERROR_MINIMUM + 48;
    push @EXPORT, qw(ERROR_PATH_MISSING);
use constant ERROR_FILE_MOVE                                        => ERROR_MINIMUM + 49;
    push @EXPORT, qw(ERROR_FILE_MOVE);
use constant ERROR_BACKUP_SET_INVALID                               => ERROR_MINIMUM + 50;
    push @EXPORT, qw(ERROR_BACKUP_SET_INVALID);
use constant ERROR_TABLESPACE_MAP                                   => ERROR_MINIMUM + 51;
    push @EXPORT, qw(ERROR_TABLESPACE_MAP);
use constant ERROR_PATH_TYPE                                        => ERROR_MINIMUM + 52;
    push @EXPORT, qw(ERROR_PATH_TYPE);
use constant ERROR_LINK_MAP                                         => ERROR_MINIMUM + 53;
    push @EXPORT, qw(ERROR_LINK_MAP);
use constant ERROR_FILE_CLOSE                                       => ERROR_MINIMUM + 54;
    push @EXPORT, qw(ERROR_FILE_CLOSE);
use constant ERROR_DB_MISSING                                       => ERROR_MINIMUM + 55;
    push @EXPORT, qw(ERROR_DB_MISSING);
use constant ERROR_DB_INVALID                                       => ERROR_MINIMUM + 56;
    push @EXPORT, qw(ERROR_DB_INVALID);
use constant ERROR_ARCHIVE_TIMEOUT                                  => ERROR_MINIMUM + 57;
    push @EXPORT, qw(ERROR_ARCHIVE_TIMEOUT);
use constant ERROR_FILE_MODE                                        => ERROR_MINIMUM + 58;
    push @EXPORT, qw(ERROR_FILE_MODE);
use constant ERROR_OPTION_MULTIPLE_VALUE                            => ERROR_MINIMUM + 59;
    push @EXPORT, qw(ERROR_OPTION_MULTIPLE_VALUE);
use constant ERROR_PROTOCOL_OUTPUT_REQUIRED                         => ERROR_MINIMUM + 60;
    push @EXPORT, qw(ERROR_PROTOCOL_OUTPUT_REQUIRED);
use constant ERROR_LINK_OPEN                                        => ERROR_MINIMUM + 61;
    push @EXPORT, qw(ERROR_LINK_OPEN);
use constant ERROR_ARCHIVE_DISABLED                                 => ERROR_MINIMUM + 62;
    push @EXPORT, qw(ERROR_ARCHIVE_DISABLED);
use constant ERROR_FILE_OWNER                                       => ERROR_MINIMUM + 63;
    push @EXPORT, qw(ERROR_FILE_OWNER);
use constant ERROR_USER_MISSING                                     => ERROR_MINIMUM + 64;
    push @EXPORT, qw(ERROR_USER_MISSING);
use constant ERROR_OPTION_COMMAND                                   => ERROR_MINIMUM + 65;
    push @EXPORT, qw(ERROR_OPTION_COMMAND);
use constant ERROR_GROUP_MISSING                                    => ERROR_MINIMUM + 66;
    push @EXPORT, qw(ERROR_GROUP_MISSING);
use constant ERROR_PATH_EXISTS                                      => ERROR_MINIMUM + 67;
    push @EXPORT, qw(ERROR_PATH_EXISTS);
use constant ERROR_FILE_EXISTS                                      => ERROR_MINIMUM + 68;
    push @EXPORT, qw(ERROR_FILE_EXISTS);
use constant ERROR_MEMORY                                           => ERROR_MINIMUM + 69; # Thrown by C library
    push @EXPORT, qw(ERROR_CRYPT);

use constant ERROR_INVALID_VALUE                                    => ERROR_MAXIMUM - 2;
    push @EXPORT, qw(ERROR_INVALID_VALUE);
use constant ERROR_UNHANDLED                                        => ERROR_MAXIMUM - 1;
    push @EXPORT, qw(ERROR_UNHANDLED);
use constant ERROR_UNKNOWN                                          => ERROR_MAXIMUM;
    push @EXPORT, qw(ERROR_UNKNOWN);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name
    my $strLevel = shift;    # Log level
    my $iCode = shift;       # Error code
    my $strMessage = shift;  # ErrorMessage
    my $strTrace = shift;    # Stack trace
    my $rExtra = shift;      # Extra info used exclusively by the logging system

    if ($iCode < ERROR_MINIMUM || $iCode > ERROR_MAXIMUM)
    {
        $iCode = ERROR_INVALID_VALUE;
    }

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Initialize exception
    $self->{strLevel} = $strLevel;
    $self->{iCode} = $iCode;
    $self->{strMessage} = $strMessage;
    $self->{strTrace} = $strTrace;
    $self->{rExtra} = $rExtra;

    return $self;
}

####################################################################################################################################
# level
####################################################################################################################################
sub level
{
    my $self = shift;

    return $self->{strLevel};
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
# extra
####################################################################################################################################
sub extra
{
    my $self = shift;

    return $self->{rExtra};
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

####################################################################################################################################
# isException - is this a structured exception or a default Perl exception?
####################################################################################################################################
sub isException
{
    my $roException = shift;

    # Only check if defined
    if (defined($roException) && defined($$roException))
    {
        # If a standard Exception
        if (blessed($$roException))
        {
            return $$roException->isa('pgBackRest::Common::Exception') ? 1 : 0;
        }
        # Else if a specially formatted string from the C library
        elsif ($$roException =~ /^PGBRCLIB\:[0-9]+\:/)
        {
            my @stryException = split(/\:/, $$roException);
            $$roException = new pgBackRest::Common::Exception(
                "ERROR", $stryException[1] + 0, $stryException[4], $stryException[2] . qw{:} . $stryException[3]);

            return 1;
        }
    }

    return 0;
}

push @EXPORT, qw(isException);

####################################################################################################################################
# exceptionCode
#
# Extract the error code from an exception - if a Perl exception return ERROR_UNKNOWN.
####################################################################################################################################
sub exceptionCode
{
    my $oException = shift;

    return isException(\$oException) ? $oException->code() : ERROR_UNKNOWN;
}

push @EXPORT, qw(exceptionCode);

####################################################################################################################################
# exceptionMessage
#
# Extract the error message from an exception - if a Perl exception return bare exception.
####################################################################################################################################
sub exceptionMessage
{
    my $oException = shift;

    return isException(\$oException) ? $oException->message() : $oException;
}

push @EXPORT, qw(exceptionMessage);

1;
