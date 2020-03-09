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
# Error Definitions
####################################################################################################################################
use constant ERROR_MINIMUM                                          => 25;
push @EXPORT, qw(ERROR_MINIMUM);
use constant ERROR_MAXIMUM                                          => 125;
push @EXPORT, qw(ERROR_MAXIMUM);

use constant ERROR_ASSERT                                           => 25;
push @EXPORT, qw(ERROR_ASSERT);
use constant ERROR_CHECKSUM                                         => 26;
push @EXPORT, qw(ERROR_CHECKSUM);
use constant ERROR_CONFIG                                           => 27;
push @EXPORT, qw(ERROR_CONFIG);
use constant ERROR_FILE_INVALID                                     => 28;
push @EXPORT, qw(ERROR_FILE_INVALID);
use constant ERROR_FORMAT                                           => 29;
push @EXPORT, qw(ERROR_FORMAT);
use constant ERROR_OPTION_INVALID_VALUE                             => 32;
push @EXPORT, qw(ERROR_OPTION_INVALID_VALUE);
use constant ERROR_POSTMASTER_RUNNING                               => 38;
push @EXPORT, qw(ERROR_POSTMASTER_RUNNING);
use constant ERROR_PATH_NOT_EMPTY                                   => 40;
push @EXPORT, qw(ERROR_PATH_NOT_EMPTY);
use constant ERROR_FILE_OPEN                                        => 41;
push @EXPORT, qw(ERROR_FILE_OPEN);
use constant ERROR_FILE_READ                                        => 42;
push @EXPORT, qw(ERROR_FILE_READ);
use constant ERROR_ARCHIVE_MISMATCH                                 => 44;
push @EXPORT, qw(ERROR_ARCHIVE_MISMATCH);
use constant ERROR_ARCHIVE_DUPLICATE                                => 45;
push @EXPORT, qw(ERROR_ARCHIVE_DUPLICATE);
use constant ERROR_PATH_CREATE                                      => 47;
push @EXPORT, qw(ERROR_PATH_CREATE);
use constant ERROR_LOCK_ACQUIRE                                     => 50;
push @EXPORT, qw(ERROR_LOCK_ACQUIRE);
use constant ERROR_BACKUP_MISMATCH                                  => 51;
push @EXPORT, qw(ERROR_BACKUP_MISMATCH);
use constant ERROR_PATH_OPEN                                        => 53;
push @EXPORT, qw(ERROR_PATH_OPEN);
use constant ERROR_PATH_SYNC                                        => 54;
push @EXPORT, qw(ERROR_PATH_SYNC);
use constant ERROR_FILE_MISSING                                     => 55;
push @EXPORT, qw(ERROR_FILE_MISSING);
use constant ERROR_DB_CONNECT                                       => 56;
push @EXPORT, qw(ERROR_DB_CONNECT);
use constant ERROR_DB_QUERY                                         => 57;
push @EXPORT, qw(ERROR_DB_QUERY);
use constant ERROR_DB_MISMATCH                                      => 58;
push @EXPORT, qw(ERROR_DB_MISMATCH);
use constant ERROR_PATH_REMOVE                                      => 61;
push @EXPORT, qw(ERROR_PATH_REMOVE);
use constant ERROR_STOP                                             => 62;
push @EXPORT, qw(ERROR_STOP);
use constant ERROR_FILE_WRITE                                       => 64;
push @EXPORT, qw(ERROR_FILE_WRITE);
use constant ERROR_FEATURE_NOT_SUPPORTED                            => 67;
push @EXPORT, qw(ERROR_FEATURE_NOT_SUPPORTED);
use constant ERROR_ARCHIVE_COMMAND_INVALID                          => 68;
push @EXPORT, qw(ERROR_ARCHIVE_COMMAND_INVALID);
use constant ERROR_LINK_EXPECTED                                    => 69;
push @EXPORT, qw(ERROR_LINK_EXPECTED);
use constant ERROR_LINK_DESTINATION                                 => 70;
push @EXPORT, qw(ERROR_LINK_DESTINATION);
use constant ERROR_PATH_MISSING                                     => 73;
push @EXPORT, qw(ERROR_PATH_MISSING);
use constant ERROR_FILE_MOVE                                        => 74;
push @EXPORT, qw(ERROR_FILE_MOVE);
use constant ERROR_PATH_TYPE                                        => 77;
push @EXPORT, qw(ERROR_PATH_TYPE);
use constant ERROR_DB_MISSING                                       => 80;
push @EXPORT, qw(ERROR_DB_MISSING);
use constant ERROR_DB_INVALID                                       => 81;
push @EXPORT, qw(ERROR_DB_INVALID);
use constant ERROR_ARCHIVE_TIMEOUT                                  => 82;
push @EXPORT, qw(ERROR_ARCHIVE_TIMEOUT);
use constant ERROR_ARCHIVE_DISABLED                                 => 87;
push @EXPORT, qw(ERROR_ARCHIVE_DISABLED);
use constant ERROR_FILE_OWNER                                       => 88;
push @EXPORT, qw(ERROR_FILE_OWNER);
use constant ERROR_PATH_EXISTS                                      => 92;
push @EXPORT, qw(ERROR_PATH_EXISTS);
use constant ERROR_FILE_EXISTS                                      => 93;
push @EXPORT, qw(ERROR_FILE_EXISTS);
use constant ERROR_CRYPTO                                           => 95;
push @EXPORT, qw(ERROR_CRYPTO);
use constant ERROR_INVALID                                          => 123;
push @EXPORT, qw(ERROR_INVALID);
use constant ERROR_UNHANDLED                                        => 124;
push @EXPORT, qw(ERROR_UNHANDLED);
use constant ERROR_UNKNOWN                                          => 125;
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
    my $bErrorC = shift;     # Is this a C error?

    if ($iCode < ERROR_MINIMUM || $iCode > ERROR_MAXIMUM)
    {
        $iCode = ERROR_INVALID;
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
    $self->{bErrorC} = $bErrorC ? 1 : 0;

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
# Is this a C error?
####################################################################################################################################
sub errorC
{
    my $self = shift;

    return $self->{bErrorC};
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
            # Split message and discard the first part used for identification
            my @stryException = split(/\:/, $$roException);
            shift(@stryException);

            # Construct exception fields
            my $iCode = shift(@stryException) + 0;
            my $strTrace = shift(@stryException) . qw{:} . shift(@stryException);
            my $strMessage = join(':', @stryException);

            # Create exception
            $$roException = new pgBackRest::Common::Exception("ERROR", $iCode, $strMessage, $strTrace, undef, 1);

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
