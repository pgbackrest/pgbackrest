####################################################################################################################################
# Compress/decompress using gzip.
####################################################################################################################################
package pgBackRest::Storage::Filter::Gzip;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Compress::Raw::Zlib qw(WANT_GZIP MAX_WBITS Z_OK Z_BUF_ERROR Z_DATA_ERROR Z_STREAM_END);
use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Base;
use pgBackRest::Common::Log;
use pgBackRest::Storage::Base;

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant STORAGE_FILTER_GZIP                                    => __PACKAGE__;
    push @EXPORT, qw(STORAGE_FILTER_GZIP);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
our @ISA = ();                                                      ## no critic (ClassHierarchies::ProhibitExplicitISA)

sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $self,
        $bWantGzip,
        $iLevel,
        $lBufferMax,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'self', trace => true},
            {name => 'bWantGzip', optional => true, default => true, trace => true},
            {name => 'iLevel', optional => true, default => 6, trace => true},
            {name => 'lBufferMax', optional => true, default => COMMON_IO_BUFFER_MAX, trace => true},
        );

    # Bless with the new class
    @ISA = $self->isA();                                            ## no critic (ClassHierarchies::ProhibitExplicitISA)
    bless $self, $class;

    # Set variables
    $self->{bWantGzip} = $bWantGzip;
    $self->{iLevel} = $iLevel;
    $self->{lBufferMax} = $lBufferMax;

    # Create the zlib object
    my $iZLibStatus;

    if ($self->SUPER::type() eq STORAGE_FILE_READ)
    {
        ($self->{oZLib}, $iZLibStatus) = new Compress::Raw::Zlib::Inflate(
            WindowBits => $self->{bWantGzip} ? WANT_GZIP : MAX_WBITS, Bufsize => $self->{lBufferMax},
            LimitOutput => 1, AppendOutput => 1);

        $self->{tUncompressedBuffer} = undef;
        $self->{lUncompressedBufferSize} = 0;
    }
    else
    {
        ($self->{oZLib}, $iZLibStatus) = new Compress::Raw::Zlib::Deflate(
            WindowBits => $self->{bWantGzip} ? WANT_GZIP : MAX_WBITS, Level => $self->{iLevel},
            Bufsize => $self->{lBufferMax}, AppendOutput => 1);
    }

    $self->errorCheck($iZLibStatus);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# read - decompress data
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;
    my $iSize = shift;

    # If the local buffer size is not large enough to satisfy the request and there is still data to decompress
    if ($self->{lUncompressedBufferSize} < $iSize)
    {
        my $iZLibStatus = Z_OK;

        while ($self->{lUncompressedBufferSize} < $iSize)
        {
            if (!defined($self->{tCompressedBuffer}) || length($self->{tCompressedBuffer}) == 0)
            {
                $self->SUPER::read(\$self->{tCompressedBuffer}, $self->{lBufferMax} );
            }

            $iZLibStatus = $self->{oZLib}->inflate($self->{tCompressedBuffer}, $self->{tUncompressedBuffer});
            $self->{lUncompressedBufferSize} = length($self->{tUncompressedBuffer});

            last if $iZLibStatus == Z_STREAM_END;

            $self->errorCheck($iZLibStatus);
        }
    }

    # Actual size is the lesser of the local buffer size or requested size - if the local buffer is smaller than the requested size
    # it means that there was nothing more to be read.
    my $iActualSize = $self->{lUncompressedBufferSize} < $iSize ? $self->{lUncompressedBufferSize} : $iSize;

    # Append the to the request buffer
    $$rtBuffer .= substr($self->{tUncompressedBuffer}, 0, $iActualSize);

    # Truncate local buffer
    $self->{tUncompressedBuffer} = substr($self->{tUncompressedBuffer}, $iActualSize);
    $self->{lUncompressedBufferSize} -= $iActualSize;

    # Return the actual size read
    return $iActualSize;
}

####################################################################################################################################
# write - compress data
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    # Compress the data
    my $tCompressedBuffer;
    $self->errorCheck($self->{oZLib}->deflate($$rtBuffer, $tCompressedBuffer));

    # Call the io method
    $self->SUPER::write(\$tCompressedBuffer);

    # Return bytes written
    return length($$rtBuffer);
}

####################################################################################################################################
# errorCheck - check status code for errors
####################################################################################################################################
sub errorCheck
{
    my $self = shift;
    my $iZLibStatus = shift;

    if (!($iZLibStatus == Z_OK || $iZLibStatus == Z_BUF_ERROR))
    {
        logErrorResult(
            $self->SUPER::type() eq STORAGE_FILE_READ ? ERROR_FILE_READ : ERROR_FILE_WRITE,
            'unable to ' . ($self->SUPER::type() eq STORAGE_FILE_READ ? 'inflate' : 'deflate') . " '$self->{strName}'",
            $self->{oZLib}->msg());
    }

    return Z_OK;
}

####################################################################################################################################
# close - close the file
####################################################################################################################################
sub close
{
    my $self = shift;

    if (defined($self->{oZLib}))
    {
        # Flush the write buffer
        if ($self->SUPER::type() eq STORAGE_FILE_WRITE)
        {
            # Flush out last compressed bytes
            my $tCompressedBuffer;
            $self->errorCheck($self->{oZLib}->flush($tCompressedBuffer));

            # Write last compressed bytes
            $self->SUPER::write(\$tCompressedBuffer, length($tCompressedBuffer));
        }

        undef($self->{oZLib});

        # Close io
        return $self->SUPER::close();
    }
}

1;
