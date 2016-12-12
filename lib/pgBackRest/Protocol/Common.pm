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
use JSON::PP;

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Protocol::IO;

####################################################################################################################################
# DB/BACKUP Constants
####################################################################################################################################
use constant DB                                                     => 'db';
    push @EXPORT, qw(DB);
use constant BACKUP                                                 => 'backup';
    push @EXPORT, qw(BACKUP);
use constant NONE                                                   => 'none';
    push @EXPORT, qw(NONE);

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_NOOP                                                => 'noop';
    push @EXPORT, qw(OP_NOOP);
use constant OP_EXIT                                                => 'exit';
    push @EXPORT, qw(OP_EXIT);

# Backup module
use constant OP_BACKUP_FILE                                          => 'backupFile';
    push @EXPORT, qw(OP_BACKUP_FILE);

# Archive Module
use constant OP_ARCHIVE_GET_ARCHIVE_ID                              => 'archiveId';
    push @EXPORT, qw(OP_ARCHIVE_GET_ARCHIVE_ID);
use constant OP_ARCHIVE_GET_BACKUP_INFO_CHECK                       => 'backupInfoCheck';
    push @EXPORT, qw(OP_ARCHIVE_GET_BACKUP_INFO_CHECK);
use constant OP_ARCHIVE_GET_CHECK                                   => 'archiveCheck';
    push @EXPORT, qw(OP_ARCHIVE_GET_CHECK);
use constant OP_ARCHIVE_PUSH_CHECK                                  => 'archivePushCheck';
    push @EXPORT, qw(OP_ARCHIVE_PUSH_CHECK);

# Db Module
use constant OP_DB_CONNECT                                          => 'dbConnect';
    push @EXPORT, qw(OP_DB_CONNECT);
use constant OP_DB_EXECUTE_SQL                                      => 'dbExecSql';
    push @EXPORT, qw(OP_DB_EXECUTE_SQL);
use constant OP_DB_INFO                                             => 'dbInfo';
    push @EXPORT, qw(OP_DB_INFO);

# File Module
use constant OP_FILE_COPY                                           => 'fileCopy';
    push @EXPORT, qw(OP_FILE_COPY);
use constant OP_FILE_COPY_IN                                        => 'fileCopyIn';
    push @EXPORT, qw(OP_FILE_COPY_IN);
use constant OP_FILE_COPY_OUT                                       => 'fileCopyOut';
    push @EXPORT, qw(OP_FILE_COPY_OUT);
use constant OP_FILE_EXISTS                                         => 'fileExists';
    push @EXPORT, qw(OP_FILE_EXISTS);
use constant OP_FILE_LIST                                           => 'fileList';
    push @EXPORT, qw(OP_FILE_LIST);
use constant OP_FILE_MANIFEST                                       => 'fileManifest';
    push @EXPORT, qw(OP_FILE_MANIFEST);
use constant OP_FILE_PATH_CREATE                                    => 'pathCreate';
    push @EXPORT, qw(OP_FILE_PATH_CREATE);
use constant OP_FILE_WAIT                                           => 'wait';
    push @EXPORT, qw(OP_FILE_WAIT);

# Info module
use constant OP_INFO_STANZA_LIST                                    => 'infoStanzList';
    push @EXPORT, qw(OP_INFO_STANZA_LIST);

# Restore module
use constant OP_RESTORE_FILE                                         => 'restoreFile';
    push @EXPORT, qw(OP_RESTORE_FILE);

# To be run after each command
use constant OP_POST                                                 => 'post';
    push @EXPORT, qw(OP_POST);

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
            __PACKAGE__ . '->new', \@_,
            {name => 'iBufferMax', trace => true},
            {name => 'iCompressLevel', trace => true},
            {name => 'iCompressNetworkLevel', trace => true},
            {name => 'iProtocolTimeout', trace => true},
            {name => 'strName', required => false, trace => true}
        );

    # By default remote type is NONE
    $self->{strRemoteType} = NONE;

    # Create JSON object
    $self->{oJSON} = JSON::PP->new()->allow_nonref();

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
# noop
#
# Don't do anything for noop if there is no remote.
####################################################################################################################################
sub noOp
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
    my $hMessage;

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

        if (defined($stryToken[2]))
        {
            $hMessage = $self->{oJSON}->decode($stryToken[2]);
        }

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
    return $iBlockSize, $hMessage;
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
    my $hMessage = shift;

    # If block size is not defined, get it from buffer length
    $iBlockSize = defined($iBlockSize) ? $iBlockSize : length($$tBlockRef);

    # Write block header to the protocol stream
    if ($bProtocol)
    {
        $oOut->lineWrite("block ${iBlockSize}" . (defined($hMessage) ? " " . $self->{oJSON}->encode($hMessage) : ''));
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
    my $fnExtra = shift;

    # The input stream must be defined
    my $oIn;

    if (!defined($hIn))
    {
        $oIn = $self->{io};
    }
    else
    {
        $oIn = new pgBackRest::Protocol::IO(
            $hIn, undef, $self->{io}->{hErr}, $self->{io}->{pid}, $self->{io}->{strId}, $self->{iProtocolTimeout},
            $self->{iBufferMax});
    }

    # The output stream must be defined unless 'none' is passed
    my $oOut;

    if (!defined($hOut))
    {
        $oOut = $self->{io};
    }
    elsif ($hOut ne 'none')
    {
        $oOut = new pgBackRest::Protocol::IO(
            undef, $hOut, $self->{io}->{hErr}, $self->{io}->{pid}, $self->{io}->{strId}, $self->{iProtocolTimeout},
            $self->{iBufferMax});
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
    my $hMessage = undef;

    # Checksum, size, and extra
    my $strChecksum = undef;
    my $iFileSize = undef;
    my $rExtra = undef;

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
                ($iBlockSize, $hMessage) = $self->blockRead($oIn, \$tCompressedBuffer, $bProtocol);

                # Process protocol messages
                if (defined($hMessage) && defined($hMessage->{bChecksum}) && !$hMessage->{bChecksum})
                {
                    $oSHA = Digest::SHA->new('sha1');
                    undef($hMessage);
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
                $rExtra = defined($fnExtra) ? {} : undef;
            }

            do
            {
                # Read a block from the protocol stream
                ($iBlockSize, $hMessage) = $self->blockRead($oIn, \$tBuffer, $bProtocol);

                # Add data to checksum and size
                if ($iBlockSize > 0 && !$bProtocol)
                {
                    $oSHA->add($tBuffer);
                    $iFileSize += $iBlockSize;
                }

                # Do extra processing on the buffer if requested
                if (!$bProtocol && defined($fnExtra))
                {
                    $fnExtra->(\$tBuffer, $iBlockSize, $iFileSize - $iBlockSize, $rExtra);
                }

                # Write buffer
                if ($iBlockSize > 0)
                {
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
                $hMessage->{bChecksum} = true;
            }

            # Initialize checksum
            my $oSHA = Digest::SHA->new('sha1');
            $rExtra = defined($fnExtra) ? {} : undef;

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

                # If block size > 0 then update checksum and size
                if ($iBlockSize > 0)
                {
                    # Update checksum and filesize
                    $oSHA->add($tUncompressedBuffer);
                }

                # Do extra processing on the buffer if requested
                if (defined($fnExtra))
                {
                    $fnExtra->(\$tUncompressedBuffer, $iBlockSize, $oZLib->total_in(), $rExtra);
                }

                # If block size > 0 then compress
                if ($iBlockSize > 0)
                {
                    # Compress the data
                    $iZLibStatus = $oZLib->deflate($tUncompressedBuffer, $tCompressedBuffer);
                    $iCompressedBufferSize = length($tCompressedBuffer);

                    # If compression was successful
                    if ($iZLibStatus == Z_OK)
                    {
                        # The compressed data is larger than block size, then write
                        if ($iCompressedBufferSize > $self->{iBufferMax})
                        {
                            $self->blockWrite($oOut, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $hMessage);
                            undef($tCompressedBuffer);
                            undef($hMessage);
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
                    $self->blockWrite($oOut, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $hMessage);
                    undef($hMessage);
                }

                $self->blockWrite(
                    $oOut, undef, 0, $bProtocol, {strChecksum => $strChecksum, iFileSize => $iFileSize, rExtra => $rExtra});
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
                    $hMessage->{bChecksum} = true;
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
                $hMessage->{bChecksum} = false;
            }

            # Read input
            do
            {
                $iBlockSize = $oIn->bufferRead(\$tBuffer, $self->{iBufferMax});

                # Write a block if size > 0
                if ($iBlockSize > 0)
                {
                    $self->blockWrite($oOut, \$tBuffer, $iBlockSize, $bProtocol, $hMessage);
                    undef($hMessage);
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

            # Check decompression, get checksum
            if ($bDestinationCompress)
            {
                # Make sure the decompression succeeded (iBlockSize < 0 indicates remote error, handled later)
                if ($iBlockSize == 0 && $iZLibStatus != Z_STREAM_END)
                {
                    confess &log(ERROR, "unable to inflate stream: ${iZLibStatus}");
                }

                # If protocol then create the message
                if ($bProtocol)
                {
                    $hMessage = {strChecksum => $oSHA->hexdigest(), iFileSize => $iFileSize, rExtra => $rExtra};
                }
                # Otherwise just set checksum
                else
                {
                    $strChecksum = $oSHA->hexdigest();
                }
            }

            # If protocol write
            if ($bProtocol)
            {
                # Write 0 block to indicate end of stream
                $self->blockWrite($oOut, undef, 0, $bProtocol, $hMessage);
            }
        }
    }

    # If message is defined then the checksum, size, and extra should be in it
    if (defined($hMessage))
    {
        return $hMessage->{strChecksum}, $hMessage->{iFileSize}, $hMessage->{rExtra};
    }

    # Return the checksum and size if they are available
    return $strChecksum, $iFileSize, $rExtra;
}

####################################################################################################################################
# remoteType
#
# Return the remote type.
####################################################################################################################################
sub remoteType
{
    return shift->{strRemoteType};
}

####################################################################################################################################
# remoteTypeTest
#
# Determine if the remote matches the specified remote.
####################################################################################################################################
sub remoteTypeTest
{
    my $self = shift;
    my $strRemoteType = shift;

    return $self->{strRemoteType} eq $strRemoteType ? true : false;
}

####################################################################################################################################
# isRemote
#
# Determine if the protocol object is communicating with a remote.
####################################################################################################################################
sub isRemote
{
    return shift->{strRemoteType} ne NONE ? true : false;
}

1;
