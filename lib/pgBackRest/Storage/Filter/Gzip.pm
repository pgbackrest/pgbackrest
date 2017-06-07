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
        $strCompressType,
        $iLevel,
        $lCompressBufferMax,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'self', trace => true},
            {name => 'bWantGzip', optional => true, default => true, trace => true},
            {name => 'strCompressType', optional => true, default => STORAGE_COMPRESS, trace => true},
            {name => 'iLevel', optional => true, default => 6, trace => true},
            {name => 'lCompressBufferMax', optional => true, default => COMMON_IO_BUFFER_MAX, trace => true},
        );

    # Bless with new class
    @ISA = $self->isA();                                            ## no critic (ClassHierarchies::ProhibitExplicitISA)
    bless $self, $class;

    # Set variables
    $self->{bWantGzip} = $bWantGzip;
    $self->{iLevel} = $iLevel;
    $self->{lCompressBufferMax} = $lCompressBufferMax;
    $self->{strCompressType} = $strCompressType;

    # Set read/write
    $self->{bWrite} = false;

    # Create the zlib object
    my $iZLibStatus;

    if ($self->{strCompressType} eq STORAGE_COMPRESS)
    {
        ($self->{oZLib}, $iZLibStatus) = new Compress::Raw::Zlib::Deflate(
            WindowBits => $self->{bWantGzip} ? WANT_GZIP : MAX_WBITS, Level => $self->{iLevel},
            Bufsize => $self->{lCompressBufferMax}, AppendOutput => 1);

        $self->{tCompressedBuffer} = undef;
    }
    else
    {
        ($self->{oZLib}, $iZLibStatus) = new Compress::Raw::Zlib::Inflate(
            WindowBits => $self->{bWantGzip} ? WANT_GZIP : MAX_WBITS, Bufsize => $self->{lCompressBufferMax},
            LimitOutput => 1, AppendOutput => 1);

        $self->{tUncompressedBuffer} = undef;
        $self->{lUncompressedBufferSize} = 0;
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
# read - compress/decompress data
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;
    my $iSize = shift;

    if ($self->{strCompressType} eq STORAGE_COMPRESS)
    {
        my $lSizeBegin = defined($$rtBuffer) ? length($$rtBuffer) : 0;
        my $lUncompressedSize;
        my $lCompressedSize;

        do
        {
            my $tUncompressedBuffer;
            $lUncompressedSize = $self->SUPER::read(\$tUncompressedBuffer, $iSize);

            if ($lUncompressedSize > 0)
            {
                $self->errorCheck($self->{oZLib}->deflate($tUncompressedBuffer, $$rtBuffer));
            }
            else
            {
                $self->errorCheck($self->{oZLib}->flush($$rtBuffer));
            }

            $lCompressedSize = length($$rtBuffer) - $lSizeBegin;
        }
        while ($lUncompressedSize > 0 && $lCompressedSize < $iSize);

        # Return the actual size read
        return $lCompressedSize;
    }
    else
    {
        # If the local buffer size is not large enough to satisfy the request and there is still data to decompress
        if ($self->{lUncompressedBufferSize} < $iSize)
        {
            while ($self->{lUncompressedBufferSize} < $iSize)
            {
                if (!defined($self->{tCompressedBuffer}) || length($self->{tCompressedBuffer}) == 0)
                {
                    $self->SUPER::read(\$self->{tCompressedBuffer}, $self->{lCompressBufferMax});
                }

                my $iZLibStatus = $self->{oZLib}->inflate($self->{tCompressedBuffer}, $self->{tUncompressedBuffer});
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
}

####################################################################################################################################
# write - compress/decompress data
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    $self->{bWrite} = true;

    if ($self->{strCompressType} eq STORAGE_COMPRESS)
    {
        # Compress the data
        $self->errorCheck($self->{oZLib}->deflate($$rtBuffer, $self->{tCompressedBuffer}));

        # Only write when buffer is full
        if (defined($self->{tCompressedBuffer}) && length($self->{tCompressedBuffer}) > $self->{lCompressBufferMax})
        {
            $self->SUPER::write(\$self->{tCompressedBuffer});
            $self->{tCompressedBuffer} = undef;
        }
    }
    else
    {
        my $tCompressedBuffer = $$rtBuffer;

        while (length($tCompressedBuffer) > 0)
        {
            my $tUncompressedBuffer;

            my $iZLibStatus = $self->{oZLib}->inflate($tCompressedBuffer, $tUncompressedBuffer);
            $self->SUPER::write(\$tUncompressedBuffer);

            last if $iZLibStatus == Z_STREAM_END;

            $self->errorCheck($iZLibStatus);
        }
    }

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
            $self->{bWrite} ? ERROR_FILE_WRITE : ERROR_FILE_READ,
            'unable to ' . ($self->{strCompressType} eq STORAGE_COMPRESS ? 'deflate' : 'inflate') . " '$self->{strName}'",
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
        if ($self->{bWrite})
        {
            if ($self->{strCompressType} eq STORAGE_COMPRESS)
            {
                # Flush out last compressed bytes
                $self->errorCheck($self->{oZLib}->flush($self->{tCompressedBuffer}));

                # Write last compressed bytes
                $self->SUPER::write(\$self->{tCompressedBuffer});
            }
        }

        undef($self->{oZLib});

        # Close io
        return $self->SUPER::close();
    }
}

1;
