####################################################################################################################################
# PROTOCOL COMMON MODULE
####################################################################################################################################
package BackRest::Protocol::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Compress::Raw::Zlib qw(WANT_GZIP Z_OK Z_BUF_ERROR Z_STREAM_END);
use File::Basename qw(dirname);
use IO::String qw();

use lib dirname($0) . '/../lib';
use BackRest::Config;
use BackRest::Exception;
use BackRest::Ini;
use BackRest::Protocol::IO;
use BackRest::Utility;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_PROTOCOL_COMMON                                     => 'Protocol::Common';

use constant OP_PROTOCOL_COMMON_NEW                                 => OP_PROTOCOL_COMMON . "->new";

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name
    my $iBlockSize = shift;             # Buffer size
    my $iCompressLevel = shift;         # Set compression level
    my $iCompressLevelNetwork = shift;  # Set compression level for network only compression
    my $strName = shift;                # Name to be used for gretting

    # Debug
    logTrace(OP_PROTOCOL_COMMON_NEW, DEBUG_CALL, undef,
             {iBlockSize => $iBlockSize, iCompressLevel => $iCompressLevel, iCompressNetworkLevel => $iCompressLevelNetwork});

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Set default block size
    $self->{iBlockSize} = $iBlockSize;

    if (!defined($self->{iBlockSize}))
    {
        confess &log(ASSERT, 'iBlockSize must be set');
    }

    # Set compress levels
    $self->{iCompressLevel} = $iCompressLevel;

    if (!defined($self->{iCompressLevel}))
    {
        confess &log(ASSERT, 'iCompressLevel must be set');
    }

    $self->{iCompressLevelNetwork} = $iCompressLevelNetwork;

    if (!defined($self->{iCompressLevelNetwork}))
    {
        confess &log(ASSERT, 'iCompressLevelNetwork must be set');
    }

    # Create the greeting that will be used to check versions with the remote
    if (defined($strName))
    {
        $self->{strName} = $strName;
        $self->{strGreeting} = 'PG_BACKREST_' . uc($strName) . ' ' . BACKREST_VERSION;
    }

    return $self;
}

####################################################################################################################################
# pipeToString
#
# Copies data from a file handle into a string.
####################################################################################################################################
sub pipeToString
{
    my $self = shift;
    my $hOut = shift;

    my $strBuffer;
    my $hString = IO::String->new($strBuffer);
    $self->binaryXfer($hOut, $hString);

    return $strBuffer;
}

####################################################################################################################################
# blockRead
#
# Read a block from the protocol layer.
####################################################################################################################################
sub blockRead
{
    my $self = shift;
    my $io = shift;
    my $strBlockRef = shift;
    my $bProtocol = shift;

    my $iBlockSize;
    my $strMessage;

    if ($bProtocol)
    {
        # Read the block header and make sure it's valid
        my $strBlockHeader = $self->{io}->lineRead();

        if ($strBlockHeader !~ /^block -{0,1}[0-9]+( .*){0,1}$/)
        {
            $self->{io}->waitPid();
            confess "unable to read block header ${strBlockHeader}";
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
            my $iBlockRead = 0;
            my $iBlockIn = 0;
            my $iOffset = defined($$strBlockRef) ? length($$strBlockRef) : 0;

            # !!! Would be nice to modify this with a non-blocking read
            # http://docstore.mik.ua/orelly/perl/cookbook/ch07_15.htm

            # Read as many chunks as it takes to get the full block
            while ($iBlockRead != $iBlockSize)
            {
                $iBlockIn = $io->bufferRead($strBlockRef, $iBlockSize - $iBlockRead, $iBlockRead + $iOffset);
                $iBlockRead += $iBlockIn;
            }
        }
    }
    else
    {
        $iBlockSize = $io->bufferRead($strBlockRef, $self->{iBlockSize}, defined($$strBlockRef) ? length($$strBlockRef) : undef);
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
    my $io = shift;
    my $tBlockRef = shift;
    my $iBlockSize = shift;
    my $bProtocol = shift;
    my $strMessage = shift;

    # If block size is not defined, get it from buffer length
    $iBlockSize = defined($iBlockSize) ? $iBlockSize : length($$tBlockRef);

    # Write block header to the protocol stream
    if ($bProtocol)
    {
        $io->lineWrite("block ${iBlockSize}" . (defined($strMessage) ? " ${strMessage}" : ''));
    }

    # Write block if size > 0
    if ($iBlockSize > 0)
    {
        $io->bufferWrite($tBlockRef, $iBlockSize);
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
    if (!defined($hIn))
    {
        if (defined($self->{io}))
        {
            $hIn = $self->{io}->hInGet();
        }
        else
        {
            confess &log(ASSERT, 'hIn is not defined');
        }
    }

    # The output stream must be defined unless 'none' is passed
    if (!defined($hOut))
    {
        if (defined($self->{io}))
        {
            $hOut = $self->{io}->hOutGet();
        }
        else
        {
            confess &log(ASSERT, 'hOut is not defined');
        }
    }
    elsif ($hOut eq 'none')
    {
        undef($hOut);
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

    # Create IO object for this transfer
    my $io = new BackRest::Protocol::IO($hIn, $hOut, $self->{io}->{hErr}, $self->{io}->{pid});

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
                                                 Bufsize => $self->{iBlockSize}, LimitOutput => 1);

            if ($iZLibStatus != Z_OK)
            {
                confess &log(ERROR, "unable create a inflate object: ${iZLibStatus}");
            }

            # Read all input
            do
            {
                # Read a block from the input stream
                ($iBlockSize, $strMessage) = $self->blockRead($io, \$tCompressedBuffer, $bProtocol);

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
                                if (defined($hOut))
                                {
                                    $io->bufferWrite(\$tUncompressedBuffer, $iUncompressedBufferSize);
                                }
                            }
                        }
                        # Else error, exit so it can be handled
                        else
                        {
                            $iBlockSize = 0;
                            last;
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
                ($iBlockSize, $strMessage) = $self->blockRead($io, \$tBuffer, $bProtocol);

                # If the block contains data, write it
                if ($iBlockSize > 0)
                {
                    # Add data to checksum and size
                    if (!$bProtocol)
                    {
                        $oSHA->add($tBuffer);
                        $iFileSize += $iBlockSize;
                    }

                    $io->bufferWrite(\$tBuffer, $iBlockSize);
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
            if ($bProtocol && defined($hOut))
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
                                                 Bufsize => $self->{iBlockSize}, AppendOutput => 1);

            if ($iZLibStatus != Z_OK)
            {
                confess &log(ERROR, "unable create a deflate object: ${iZLibStatus}");
            }

            do
            {
                # Read a block from the stream
                $iBlockSize = $io->bufferRead(\$tUncompressedBuffer, $self->{iBlockSize});

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
                        if ($iCompressedBufferSize > $self->{iBlockSize})
                        {
                            $self->blockWrite($io, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $strMessage);
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
            if (defined($hOut))
            {
                $iCompressedBufferSize = length($tCompressedBuffer);

                if ($iCompressedBufferSize > 0)
                {
                    $self->blockWrite($io, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $strMessage);
                    undef($strMessage);
                }

                $self->blockWrite($io, undef, 0, $bProtocol, "${strChecksum}-${iFileSize}");
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
                    new Compress::Raw::Zlib::Inflate(WindowBits => WANT_GZIP, Bufsize => $self->{iBlockSize}, LimitOutput => 1);

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
                $iBlockSize = $io->bufferRead(\$tBuffer, $self->{iBlockSize});

                # Write a block if size > 0
                if ($iBlockSize > 0)
                {
                    $self->blockWrite($io, \$tBuffer, $iBlockSize, $bProtocol, $strMessage);
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
                $self->blockWrite($io, undef, 0, $bProtocol, $strMessage);
            }
        }
    }

    # If message is defined the the checksum and size should be in it
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
