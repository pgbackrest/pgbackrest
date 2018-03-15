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

use pgBackRest::Common::ExceptionAuto;

####################################################################################################################################
# Export error constants
####################################################################################################################################
push(@EXPORT, @pgBackRest::Common::ExceptionAuto::EXPORT);

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
            # Split message and discard the first part used for identification
            my @stryException = split(/\:/, $$roException);
            shift(@stryException);

            # Construct exception fields
            my $iCode = shift(@stryException) + 0;
            my $strTrace = shift(@stryException) . qw{:} . shift(@stryException);
            my $strMessage = join(':', @stryException);

            # Create exception
            $$roException = new pgBackRest::Common::Exception("ERROR", $iCode, $strMessage, $strTrace);

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
