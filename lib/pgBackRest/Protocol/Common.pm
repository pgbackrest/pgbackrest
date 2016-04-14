####################################################################################################################################
# PROTOCOL COMMON MODULE
####################################################################################################################################
package pgBackRest::Protocol::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Compress::Raw::Zlib qw(WANT_GZIP Z_OK Z_BUF_ERROR Z_STREAM_END);
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::IO;
use pgBackRest::Version;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_PROTOCOL_COMMON                                     => 'Protocol::Common';

use constant OP_PROTOCOL_COMMON_NEW                                 => OP_PROTOCOL_COMMON . "->new";

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_NOOP                                                => 'noop';
    push @EXPORT, qw(OP_NOOP);
use constant OP_EXIT                                                => 'exit';
    push @EXPORT, qw(OP_EXIT);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{iBufferMax},
        $self->{iCompressLevel},
        $self->{iCompressLevelNetwork},
        $self->{iProtocolTimeout},
        $self->{strName}
    ) =
        logDebugParam
        (
            OP_PROTOCOL_COMMON_NEW, \@_,
            {name => 'iBufferMax', trace => true},
            {name => 'iCompressLevel', trace => true},
            {name => 'iCompressNetworkLevel', trace => true},
            {name => 'iProtocolTimeout', trace => true},
            {name => 'strName', required => false, trace => true}
        );

    # Create the greeting that will be used to check versions with the remote
    if (defined($self->{strName}))
    {
        $self->{strGreeting} = uc(BACKREST_NAME) . uc($self->{strName}) . ' ' . BACKREST_VERSION;
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# keepAlive
#
# Don't do anything for keep alive if there is no remote.
####################################################################################################################################
sub keepAlive
{
}

####################################################################################################################################
# blockRead
#
# Read a block from the protocol layer.
####################################################################################################################################
sub blockRead
{
    my $self = shift;
    my $oIn = shift;
    my $strBlockRef = shift;
    my $bProtocol = shift;

    my $iBlockSize;
    my $strMessage;

    if ($bProtocol)
    {
        # Read the block header and make sure it's valid
        my $strBlockHeader = $oIn->lineRead();

        if ($strBlockHeader !~ /^block -{0,1}[0-9]+( .*){0,1}$/)
        {
            confess &log(ERROR, "invalid block header ${strBlockHeader}", ERROR_FILE_READ);
        }

        # Get block size from the header
        my @stryToken = split(/ /, $strBlockHeader);
        $iBlockSize = $stryToken[1];
        $strMessage = $stryToken[2];

        # If block size is 0 or an error code then undef the buffer
        if ($iBlockSize <= 0)
        {
            undef($$strBlockRef);
        }
        # Else read the block
        else
        {
            $oIn->bufferRead($strBlockRef, $iBlockSize, undef, true);
        }
    }
    else
    {
        $iBlockSize = $oIn->bufferRead($strBlockRef, $self->{iBufferMax}, defined($$strBlockRef) ? length($$strBlockRef) : undef);
    }

    # Return the block size
    return $iBlockSize, $strMessage;
}

####################################################################################################################################
# blockWrite
#
# Write a block to the protocol layer.
####################################################################################################################################
sub blockWrite
{
    my $self = shift;
    my $oOut = shift;
    my $tBlockRef = shift;
    my $iBlockSize = shift;
    my $bProtocol = shift;
    my $strMessage = shift;

    # If block size is not defined, get it from buffer length
    $iBlockSize = defined($iBlockSize) ? $iBlockSize : length($$tBlockRef);

    # Write block header to the protocol stream
    if ($bProtocol)
    {
        $oOut->lineWrite("block ${iBlockSize}" . (defined($strMessage) ? " ${strMessage}" : ''));
    }

    # Write block if size > 0
    if ($iBlockSize > 0)
    {
        $oOut->bufferWrite($tBlockRef, $iBlockSize);
    }
}

####################################################################################################################################
# binaryXfer
#
# Copies data from one file handle to another, optionally compressing or decompressing the data in stream.  If $strRemote != none
# then one side is a protocol stream, though this can be controlled with the bProtocol param.
####################################################################################################################################
sub binaryXfer
{
    my $self = shift;
    my $hIn = shift;
    my $hOut = shift;
    my $strRemote = shift;
    my $bSourceCompressed = shift;
    my $bDestinationCompress = shift;
    my $bProtocol = shift;

    # The input stream must be defined
    my $oIn;

    if (!defined($hIn))
    {
        $oIn = $self->{io};
    }
    else
    {
        $oIn = new pgBackRest::Protocol::IO($hIn, undef, $self->{io}->{hErr}, $self->{io}->{pid},
                                          $self->{iProtocolTimeout}, $self->{iBufferMax});
    }

    # The output stream must be defined unless 'none' is passed
    my $oOut;

    if (!defined($hOut))
    {
        $oOut = $self->{io};
    }
    elsif ($hOut ne 'none')
    {
        $oOut = new pgBackRest::Protocol::IO(undef, $hOut, $self->{io}->{hErr}, $self->{io}->{pid},
                                           $self->{iProtocolTimeout}, $self->{iBufferMax});
    }

    # If no remote is defined then set to none
    if (!defined($strRemote))
    {
        $strRemote = 'none';
    }
    # Only set compression defaults when remote is defined
    else
    {
        $bSourceCompressed = defined($bSourceCompressed) ? $bSourceCompressed : false;
        $bDestinationCompress = defined($bDestinationCompress) ? $bDestinationCompress : false;
    }

    # Default protocol to true
    $bProtocol = defined($bProtocol) ? $bProtocol : true;
    my $strMessage = undef;

    # Checksum and size
    my $strChecksum = undef;
    my $iFileSize = undef;

    # Read from the protocol stream
    if ($strRemote eq 'in')
    {
        # If the destination should not be compressed then decompress
        if (!$bDestinationCompress)
        {
            my $iBlockSize;
            my $tCompressedBuffer;
            my $tUncompressedBuffer;
            my $iUncompressedBufferSize;

            # Initialize SHA
            my $oSHA;

            if (!$bProtocol)
            {
                $oSHA = Digest::SHA->new('sha1');
            }

            # Initialize inflate object and check for errors
            my ($oZLib, $iZLibStatus) =
                new Compress::Raw::Zlib::Inflate(WindowBits => 15 & $bSourceCompressed ? WANT_GZIP : 0,
                                                 Bufsize => $self->{iBufferMax}, LimitOutput => 1);

            if ($iZLibStatus != Z_OK)
            {
                confess &log(ERROR, "unable create a inflate object: ${iZLibStatus}");
            }

            # Read all input
            do
            {
                # Read a block from the input stream
                ($iBlockSize, $strMessage) = $self->blockRead($oIn, \$tCompressedBuffer, $bProtocol);

                # Process protocol messages
                if (defined($strMessage) && $strMessage eq 'nochecksum')
                {
                    $oSHA = Digest::SHA->new('sha1');
                    undef($strMessage);
                }

                # If the block contains data, decompress it
                if ($iBlockSize > 0)
                {
                    # Keep looping while there is more to decompress
                    do
                    {
                        # Decompress data
                        $iZLibStatus = $oZLib->inflate($tCompressedBuffer, $tUncompressedBuffer);
                        $iUncompressedBufferSize = length($tUncompressedBuffer);

                        # If status is ok, write the data
                        if ($iZLibStatus == Z_OK || $iZLibStatus == Z_BUF_ERROR || $iZLibStatus == Z_STREAM_END)
                        {
                            if ($iUncompressedBufferSize > 0)
                            {
                                # Add data to checksum
                                if (defined($oSHA))
                                {
                                    $oSHA->add($tUncompressedBuffer);
                                }

                                # Write data if hOut is defined
                                if (defined($oOut))
                                {
                                    $oOut->bufferWrite(\$tUncompressedBuffer, $iUncompressedBufferSize);
                                }
                            }
                        }
                        # Else error, exit so it can be handled
                        else
                        {
                            $iBlockSize = 0;
                        }
                    }
                    while ($iZLibStatus != Z_STREAM_END && $iUncompressedBufferSize > 0 && $iBlockSize > 0);
                }
            }
            while ($iBlockSize > 0);

            # Make sure the decompression succeeded (iBlockSize < 0 indicates remote error, handled later)
            if ($iBlockSize == 0 && $iZLibStatus != Z_STREAM_END)
            {
                confess &log(ERROR, "unable to inflate stream: ${iZLibStatus}");
            }

            # Get checksum and total uncompressed bytes written
            if (defined($oSHA))
            {
                $strChecksum = $oSHA->hexdigest();
                $iFileSize = $oZLib->total_out();
            };
        }
        # If the destination should be compressed then just write out the already compressed stream
        else
        {
            my $iBlockSize;
            my $tBuffer;

            # Initialize checksum and size
            my $oSHA;

            if (!$bProtocol)
            {
                $oSHA = Digest::SHA->new('sha1');
                $iFileSize = 0;
            }

            do
            {
                # Read a block from the protocol stream
                ($iBlockSize, $strMessage) = $self->blockRead($oIn, \$tBuffer, $bProtocol);

                # If the block contains data, write it
                if ($iBlockSize > 0)
                {
                    # Add data to checksum and size
                    if (!$bProtocol)
                    {
                        $oSHA->add($tBuffer);
                        $iFileSize += $iBlockSize;
                    }

                    $oOut->bufferWrite(\$tBuffer, $iBlockSize);
                    undef($tBuffer);
                }
            }
            while ($iBlockSize > 0);

            # Get checksum
            if (!$bProtocol)
            {
                $strChecksum = $oSHA->hexdigest();
            };
        }
    }
    # Read from file input stream
    else
    {
        # If source is not already compressed then compress it
        if ($strRemote eq 'out' && !$bSourceCompressed)
        {
            my $iBlockSize;
            my $tCompressedBuffer;
            my $iCompressedBufferSize;
            my $tUncompressedBuffer;

            # Initialize message to indicate that a checksum will be sent
            if ($bProtocol && defined($oOut))
            {
                $strMessage = 'checksum';
            }

            # Initialize checksum
            my $oSHA = Digest::SHA->new('sha1');

            # Initialize inflate object and check for errors
            my ($oZLib, $iZLibStatus) =
                new Compress::Raw::Zlib::Deflate(WindowBits => 15 & $bDestinationCompress ? WANT_GZIP : 0,
                                                 Level => $bDestinationCompress ? $self->{iCompressLevel} :
                                                                                  $self->{iCompressLevelNetwork},
                                                 Bufsize => $self->{iBufferMax}, AppendOutput => 1);

            if ($iZLibStatus != Z_OK)
            {
                confess &log(ERROR, "unable create a deflate object: ${iZLibStatus}");
            }

            do
            {
                # Read a block from the stream
                $iBlockSize = $oIn->bufferRead(\$tUncompressedBuffer, $self->{iBufferMax});

                # If block size > 0 then compress
                if ($iBlockSize > 0)
                {
                    # Update checksum and filesize
                    $oSHA->add($tUncompressedBuffer);

                    # Compress the data
                    $iZLibStatus = $oZLib->deflate($tUncompressedBuffer, $tCompressedBuffer);
                    $iCompressedBufferSize = length($tCompressedBuffer);

                    # If compression was successful
                    if ($iZLibStatus == Z_OK)
                    {
                        # The compressed data is larger than block size, then write
                        if ($iCompressedBufferSize > $self->{iBufferMax})
                        {
                            $self->blockWrite($oOut, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $strMessage);
                            undef($tCompressedBuffer);
                            undef($strMessage);
                        }
                    }
                    # Else if error
                    else
                    {
                        $iBlockSize = 0;
                    }
                }
            }
            while ($iBlockSize > 0);

            # If good so far flush out the last bytes
            if ($iZLibStatus == Z_OK)
            {
                $iZLibStatus = $oZLib->flush($tCompressedBuffer);
            }

            # Make sure the compression succeeded
            if ($iZLibStatus != Z_OK)
            {
                confess &log(ERROR, "unable to deflate stream: ${iZLibStatus}");
            }

            # Get checksum and total uncompressed bytes written
            $strChecksum = $oSHA->hexdigest();
            $iFileSize = $oZLib->total_in();

            # Write out the last block
            if (defined($oOut))
            {
                $iCompressedBufferSize = length($tCompressedBuffer);

                if ($iCompressedBufferSize > 0)
                {
                    $self->blockWrite($oOut, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $strMessage);
                    undef($strMessage);
                }

                $self->blockWrite($oOut, undef, 0, $bProtocol, "${strChecksum}-${iFileSize}");
            }
        }
        # If source is already compressed or transfer is not compressed then just read the stream
        else
        {
            my $iBlockSize;
            my $tBuffer;
            my $tCompressedBuffer;
            my $tUncompressedBuffer;
            my $iUncompressedBufferSize;
            my $oSHA;
            my $oZLib;
            my $iZLibStatus;

            # If the destination will be compressed setup deflate
            if ($bDestinationCompress)
            {
                if ($bProtocol)
                {
                    $strMessage = 'checksum';
                }

                # Initialize checksum and size
                $oSHA = Digest::SHA->new('sha1');
                $iFileSize = 0;

                # Initialize inflate object and check for errors
                ($oZLib, $iZLibStatus) =
                    new Compress::Raw::Zlib::Inflate(WindowBits => WANT_GZIP, Bufsize => $self->{iBufferMax}, LimitOutput => 1);

                if ($iZLibStatus != Z_OK)
                {
                    confess &log(ERROR, "unable create a inflate object: ${iZLibStatus}");
                }
            }
            # Initialize message to indicate that a checksum will not be sent
            elsif ($bProtocol)
            {
                $strMessage = 'nochecksum';
            }

            # Read input
            do
            {
                $iBlockSize = $oIn->bufferRead(\$tBuffer, $self->{iBufferMax});

                # Write a block if size > 0
                if ($iBlockSize > 0)
                {
                    $self->blockWrite($oOut, \$tBuffer, $iBlockSize, $bProtocol, $strMessage);
                    undef($strMessage);
                }

                # Decompress the buffer to calculate checksum/size
                if ($bDestinationCompress)
                {
                    # If the block contains data, decompress it
                    if ($iBlockSize > 0)
                    {
                        # Copy file buffer to compressed buffer
                        if (defined($tCompressedBuffer))
                        {
                            $tCompressedBuffer .= $tBuffer;
                        }
                        else
                        {
                            $tCompressedBuffer = $tBuffer;
                        }

                        # Keep looping while there is more to decompress
                        do
                        {
                            # Decompress data
                            $iZLibStatus = $oZLib->inflate($tCompressedBuffer, $tUncompressedBuffer);
                            $iUncompressedBufferSize = length($tUncompressedBuffer);

                            # If status is ok, write the data
                            if ($iZLibStatus == Z_OK || $iZLibStatus == Z_BUF_ERROR || $iZLibStatus == Z_STREAM_END)
                            {
                                if ($iUncompressedBufferSize > 0)
                                {
                                    $oSHA->add($tUncompressedBuffer);
                                    $iFileSize += $iUncompressedBufferSize;
                                }
                            }
                            # Else error, exit so it can be handled
                            else
                            {
                                $iBlockSize = 0;
                            }
                        }
                        while ($iZLibStatus != Z_STREAM_END && $iUncompressedBufferSize > 0 && $iBlockSize > 0);
                    }
                }
            }
            while ($iBlockSize > 0);

            # Check decompression get checksum
            if ($bDestinationCompress)
            {
                # Make sure the decompression succeeded (iBlockSize < 0 indicates remote error, handled later)
                if ($iBlockSize == 0 && $iZLibStatus != Z_STREAM_END)
                {
                    confess &log(ERROR, "unable to inflate stream: ${iZLibStatus}");
                }

                # Get checksum
                $strChecksum = $oSHA->hexdigest();

                # Set protocol message
                if ($bProtocol)
                {
                    $strMessage = "${strChecksum}-${iFileSize}";
                }
            }

            # If protocol write
            if ($bProtocol)
            {
                # Write 0 block to indicate end of stream
                $self->blockWrite($oOut, undef, 0, $bProtocol, $strMessage);
            }
        }
    }

    # If message is defined then the checksum and size should be in it
    if (defined($strMessage))
    {
        my @stryToken = split(/-/, $strMessage);
        $strChecksum = $stryToken[0];
        $iFileSize = $stryToken[1];
    }

    # Return the checksum and size if they are available
    return $strChecksum, $iFileSize;
}

1;
