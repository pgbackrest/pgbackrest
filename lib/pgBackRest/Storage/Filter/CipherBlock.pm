####################################################################################################################################
# Block Cipher Filter
####################################################################################################################################
package pgBackRest::Storage::Filter::CipherBlock;
use parent 'pgBackRest::Common::Io::Filter';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();

use pgBackRest::Common::Exception;
use pgBackRest::Common::Io::Base;
use pgBackRest::Common::Log;
use pgBackRest::LibC qw(:crypto);
use pgBackRest::Storage::Base;

####################################################################################################################################
# Package name constant
####################################################################################################################################
use constant STORAGE_FILTER_CIPHER_BLOCK                            => __PACKAGE__;
    push @EXPORT, qw(STORAGE_FILTER_CIPHER_BLOCK);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oParent,
        $strCipherType,
        $tCipherPass,
        $strMode,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'oParent', trace => true},
            {name => 'strCipherType', trace => true},
            {name => 'tCipherPass', trace => true},
            {name => 'strMode', optional => true, default => STORAGE_ENCRYPT, trace => true},
        );

    # Bless with new class
    my $self = $class->SUPER::new($oParent);
    bless $self, $class;

    # Check mode is valid
    $self->{strMode} = $strMode;

    if (!($self->{strMode} eq STORAGE_ENCRYPT || $self->{strMode} eq STORAGE_DECRYPT))
    {
        confess &log(ASSERT, "unknown cipher mode: $self->{strMode}");
    }

    # Set read/write
    $self->{bWrite} = false;

    # Create cipher object
    $self->{oCipher} = new pgBackRest::LibC::Cipher::Block(
        $self->{strMode} eq STORAGE_ENCRYPT ? CIPHER_MODE_ENCRYPT : CIPHER_MODE_DECRYPT, $strCipherType, $tCipherPass,
        length($tCipherPass));

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# read - encrypt/decrypt data
####################################################################################################################################
sub read
{
    my $self = shift;
    my $rtBuffer = shift;
    my $iSize = shift;

    # Return 0 if all data has been read
    return 0 if $self->eof();

    # Loop until required bytes have been read
    my $tBufferRead = '';
    my $iBufferReadSize = 0;

    do
    {
        # Read data
        my $tCipherBuffer;
        my $iActualSize = $self->SUPER::read(\$tCipherBuffer, $iSize);

        # If something was read, then process it
        if ($iActualSize > 0)
        {
            $tBufferRead .= $self->{oCipher}->process($tCipherBuffer);
        }

        # If eof then flush the remaining data
        if ($self->eof())
        {
            $tBufferRead .= $self->{oCipher}->flush();
        }

        # Get the current size of the read buffer
        $iBufferReadSize = length($tBufferRead);
    }
    while ($iBufferReadSize < $iSize && !$self->eof());

    # Append to the read buffer
    $$rtBuffer .= $tBufferRead;

    # Return the actual size read
    return $iBufferReadSize;
}

####################################################################################################################################
# write - encrypt/decrypt data
####################################################################################################################################
sub write
{
    my $self = shift;
    my $rtBuffer = shift;

    # Set write flag so close will flush buffer
    $self->{bWrite} = true;

    # Write the buffer if defined
    my $tCipherBuffer;

    if (defined($$rtBuffer))
    {
        $tCipherBuffer = $self->{oCipher}->process($$rtBuffer);
    }

    # Call the io method. If $rtBuffer is undefined, then this is expected to error.
    $self->SUPER::write(\$tCipherBuffer);

    return length($$rtBuffer);
}

####################################################################################################################################
# close - close the file
####################################################################################################################################
sub close
{
    my $self = shift;

    # Only close the object if not already closed
    if ($self->{oCipher})
    {
        # Flush the write buffer
        if ($self->{bWrite})
        {
            my $tCipherBuffer = $self->{oCipher}->flush();
            $self->SUPER::write(\$tCipherBuffer);
        }

        undef($self->{oCipher});

        # Close io
        return $self->SUPER::close();
    }

    return false;
}

1;
