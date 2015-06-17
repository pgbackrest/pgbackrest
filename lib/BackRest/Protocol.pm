####################################################################################################################################
# PROTOCOL MODULE
####################################################################################################################################
package BackRest::Protocol;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Compress::Raw::Zlib qw(WANT_GZIP Z_OK Z_BUF_ERROR Z_STREAM_END);
use File::Basename qw(dirname);
use IO::String qw();
use Net::OpenSSH qw();
use POSIX qw(:sys_wait_h);
use Scalar::Util qw(blessed);

use lib dirname($0) . '/../lib';
use BackRest::Config qw(optionGet OPTION_STANZA OPTION_REPO_REMOTE_PATH);
use BackRest::Exception qw(ERROR_PROTOCOL ERROR_HOST_CONNECT);
use BackRest::Utility qw(log version_get trim TRACE ERROR ASSERT DEBUG true false waitInit waitMore);

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_PROTOCOL                                            => 'Protocol';

use constant OP_PROTOCOL_NEW                                        => OP_PROTOCOL . "->new";
use constant OP_PROTOCOL_COMMAND_WRITE                              => OP_PROTOCOL . "->commandWrite";

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name
    my $strHost = shift;                # Host to connect to for remote (optional as this can also be used on the remote)
    my $strUser = shift;                # User to connect to for remote (must be set if strHost is set)
    my $strCommand = shift;             # Command to execute on remote ('remote' if this is the remote)
    my $strStanza = shift;              # Stanza
    my $strRepoPath = shift;            # Remote Repository Path
    my $iBlockSize = shift;             # Buffer size
    my $iCompressLevel = shift;         # Set compression level
    my $iCompressLevelNetwork = shift;  # Set compression level for network only compression

    # Debug
    &log(defined($strHost) ? DEBUG : TRACE, OP_PROTOCOL_NEW . '()' . 
         ': host = ' . (defined($strHost) ? $strHost : '[undef]') .
         ', user = ' . (defined($strUser) ? $strUser : '[undef]') .
         ', stanza = ' . (defined($strStanza) ? $strStanza : '[undef]') .
         ', remote-repo-path = ' . (defined($strRepoPath) ? $strRepoPath : '[undef]') .
         ', command = ' . (defined($strCommand) ? $strCommand : '[undef]'));

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Create the greeting that will be used to check versions with the remote
    $self->{strGreeting} = 'PG_BACKREST_REMOTE ' . version_get();

    # Set stanza and repo path
    $self->{strStanza} = $strStanza;
    $self->{strRepoPath} = $strRepoPath;

    # Set default block size
    $self->{iBlockSize} = $iBlockSize;

    # Set compress levels
    $self->{iCompressLevel} = $iCompressLevel;
    $self->{iCompressLevelNetwork} = $iCompressLevelNetwork;

    # If host is defined then make a connnection
    if (defined($strHost))
    {
        # User must be defined
        if (!defined($strUser))
        {
            confess &log(ASSERT, 'strUser must be defined');
        }

        # Command must be defined
        if (!defined($strCommand))
        {
            confess &log(ASSERT, 'strCommand must be defined');
        }

        $self->{strHost} = $strHost;
        $self->{strUser} = $strUser;
        $self->{strCommand} = $strCommand . ' remote';

        # Set SSH Options
        my $strOptionSSHRequestTTY = 'RequestTTY=yes';
        my $strOptionSSHCompression = 'Compression=no';

        &log(TRACE, 'connecting to remote ssh host ' . $self->{strHost});

        # Make SSH connection
        $self->{oSSH} = Net::OpenSSH->new($self->{strHost}, timeout => 600, user => $self->{strUser},
                                          master_opts => [-o => $strOptionSSHCompression, -o => $strOptionSSHRequestTTY]);

        $self->{oSSH}->error and confess &log(ERROR, "unable to connect to $self->{strHost}: " . $self->{oSSH}->error,
                                              ERROR_HOST_CONNECT);
        &log(TRACE, 'connected to remote ssh host ' . $self->{strHost});

        # Execute remote command
        ($self->{hIn}, $self->{hOut}, $self->{hErr}, $self->{pId}) = $self->{oSSH}->open3($self->{strCommand});

        $self->greeting_read();
        $self->setting_write($self->{strStanza}, $self->{strRepoPath},
                             $self->{iBlockSize}, $self->{iCompressLevel}, $self->{iCompressLevelNetwork});
    }
    elsif (defined($strCommand) && $strCommand eq 'remote')
    {
        # Write the greeting so master process knows who we are
        $self->greeting_write();

        # Read settings from master
        ($self->{strStanza}, $self->{strRepoPath}, $self->{iBlockSize}, $self->{iCompressLevel},
         $self->{iCompressLevelNetwork}) = $self->setting_read();
    }

    # Check block size
    if (!defined($self->{iBlockSize}))
    {
        confess &log(ASSERT, 'iBlockSize must be set');
    }

    # Check compress levels
    if (!defined($self->{iCompressLevel}))
    {
        confess &log(ASSERT, 'iCompressLevel must be set');
    }

    if (!defined($self->{iCompressLevelNetwork}))
    {
        confess &log(ASSERT, 'iCompressLevelNetwork must be set');
    }

    return $self;
}

####################################################################################################################################
# DESTROY
####################################################################################################################################
sub DESTROY
{
    my $self = shift;

    # Only send the exit command if the process is running
    if (defined($self->{pId}))
    {
        &log(TRACE, "sending exit command to process");
        $self->command_write('exit');

        # &log(TRACE, "waiting for remote process");
        # if (!$self->wait_pid(5, false))
        # {
        #     &log(TRACE, "killed remote process");
        #     kill('KILL', $self->{pId});
        # }
    }
}

####################################################################################################################################
# repoPath
####################################################################################################################################
sub repoPath
{
    my $self = shift;

    return $self->{strRepoPath};
}

####################################################################################################################################
# stanza
####################################################################################################################################
sub stanza
{
    my $self = shift;

    return $self->{strStanza};
}

####################################################################################################################################
# CLONE
####################################################################################################################################
sub clone
{
    my $self = shift;

    return BackRest::Protocol->new
    (
        $self->{strHost},
        $self->{strUser},
        $self->{strCommand},
        $self->{strStanza},
        $self->{strRepoPath},
        $self->{iBlockSize},
        $self->{iCompressLevel},
        $self->{iCompressLevelNetwork}
    );
}

####################################################################################################################################
# GREETING_READ
#
# Read the greeting and make sure it is as expected.
####################################################################################################################################
sub greeting_read
{
    my $self = shift;

    # Make sure that the remote is running the right version
    if ($self->read_line($self->{hOut}) ne $self->{strGreeting})
    {
        confess &log(ERROR, 'protocol version mismatch');
    }
}

####################################################################################################################################
# GREETING_WRITE
#
# Send a greeting to the master process.
####################################################################################################################################
sub greeting_write
{
    my $self = shift;

    $self->write_line(*STDOUT, $self->{strGreeting});
}

####################################################################################################################################
# SETTING_READ
#
# Read the settings from the master process.
####################################################################################################################################
sub setting_read
{
    my $self = shift;

    # Get Stanza
    my $strStanza = $self->read_line(*STDIN);

    # Get Repo Path
    my $strRepoPath = $self->read_line(*STDIN);

    # Tokenize the settings
    my @stryToken = split(/ /, $self->read_line(*STDIN));

    # Make sure there are the correct number of tokens
    if (@stryToken != 4)
    {
        confess &log(ASSERT, 'settings token count is invalid', ERROR_PROTOCOL);
    }

    # Check for the setting token just to be sure
    if ($stryToken[0] ne 'setting')
    {
        confess &log(ASSERT, 'settings token 0 must be \'setting\'');
    }

    # Return the settings
    return $strStanza, $strRepoPath, $stryToken[1], $stryToken[2], $stryToken[3];
}

####################################################################################################################################
# SETTING_WRITE
#
# Send settings to the remote process.
####################################################################################################################################
sub setting_write
{
    my $self = shift;
    my $strStanza = shift;              # Database stanza
    my $strRepoPath = shift;            # Path to the repository on the remote
    my $iBlockSize = shift;             # Optionally, set the block size (defaults to DEFAULT_BLOCK_SIZE)
    my $iCompressLevel = shift;         # Set compression level
    my $iCompressLevelNetwork = shift;  # Set compression level for network only compression

    $self->write_line($self->{hIn}, $strStanza);
    $self->write_line($self->{hIn}, $strRepoPath);
    $self->write_line($self->{hIn}, "setting ${iBlockSize} ${iCompressLevel} ${iCompressLevelNetwork}");
}

####################################################################################################################################
# STRING_WRITE
#
# Write a string.
####################################################################################################################################
sub string_write
{
    my $self = shift;
    my $hOut = shift;
    my $strBuffer = shift;

    $strBuffer =~ s/\n/\n\./g;

    if (!syswrite($hOut, '.' . $strBuffer))
    {
        confess 'unable to write string';
    }
}

####################################################################################################################################
# PIPE_TO_STRING Function
#
# Copies data from a file handle into a string.
####################################################################################################################################
sub pipe_to_string
{
    my $self = shift;
    my $hOut = shift;

    my $strBuffer;
    my $hString = IO::String->new($strBuffer);
    $self->binary_xfer($hOut, $hString);

    return $strBuffer;
}

####################################################################################################################################
# ERROR_WRITE
#
# Write errors with error codes in protocol format, otherwise write to stderr and exit with error.
####################################################################################################################################
sub error_write
{
    my $self = shift;
    my $oMessage = shift;

    my $iCode;
    my $strMessage;

    if (blessed($oMessage))
    {
        if ($oMessage->isa('BackRest::Exception'))
        {
            $iCode = $oMessage->code();
            $strMessage = $oMessage->message();
        }
        else
        {
            syswrite(*STDERR, 'unknown error object: ' . $oMessage);
            exit 1;
        }
    }
    else
    {
        syswrite(*STDERR, $oMessage);
        exit 1;
    }

    if (defined($strMessage))
    {
        $self->string_write(*STDOUT, trim($strMessage));
    }

    if (!syswrite(*STDOUT, "\nERROR" . (defined($iCode) ? " $iCode" : '') . "\n"))
    {
        confess 'unable to write error';
    }
}

####################################################################################################################################
# READ_LINE
#
# Read a line.
####################################################################################################################################
sub read_line
{
    my $self = shift;
    my $hIn = shift;
    my $bError = shift;

    my $strLine;
    my $strChar;
    my $iByteIn;

    while (1)
    {
        $iByteIn = sysread($hIn, $strChar, 1);

        if (!defined($iByteIn) || $iByteIn != 1)
        {
            $self->wait_pid();

            if (defined($bError) and !$bError)
            {
                return undef;
            }

            confess &log(ERROR, 'unable to read 1 byte' . (defined($!) ? ': ' . $! : ''));
        }

        if ($strChar eq "\n")
        {
            last;
        }

        $strLine .= $strChar;
    }

    return $strLine;
}

####################################################################################################################################
# WRITE_LINE
#
# Write a line data
####################################################################################################################################
sub write_line
{
    my $self = shift;
    my $hOut = shift;
    my $strBuffer = shift;

    my $iLineOut = syswrite($hOut, (defined($strBuffer) ? $strBuffer : '') . "\n");

    if (!defined($iLineOut) || $iLineOut != (defined($strBuffer) ? length($strBuffer) : 0) + 1)
    {
        confess &log(ERROR, "unable to write ${strBuffer}: $!", ERROR_PROTOCOL);
    }
}

####################################################################################################################################
# WAIT_PID
#
# See if the remote process has terminated unexpectedly.
####################################################################################################################################
sub wait_pid
{
    my $self = shift;
    my $fWaitTime = shift;
    my $bReportError = shift;

    # Record the start time and set initial sleep interval
    my $oWait = waitInit($fWaitTime);

    if (defined($self->{pId}))
    {
        do
        {
            my $iResult = waitpid($self->{pId}, WNOHANG);

            if (defined($fWaitTime))
            {
                confess &log(TRACE, "waitpid result = $iResult");
            }

            # If there is no such process
            if ($iResult == -1)
            {
                return true;
            }

            if ($iResult > 0)
            {
                if (!defined($bReportError) || $bReportError)
                {
                    my $strError = 'no error on stderr';

                    if (!defined($self->{hErr}))
                    {
                        $strError = 'no error captured because stderr is already closed';
                    }
                    else
                    {
                        $strError = $self->pipe_to_string($self->{hErr});
                    }

                    $self->{pId} = undef;
                    $self->{hIn} = undef;
                    $self->{hOut} = undef;
                    $self->{hErr} = undef;

                    confess &log(ERROR, "remote process terminated: ${strError}");
                }

                return true;
            }

            &log(TRACE, "waiting for pid");
        }
        while (waitMore($oWait));
    }

    return false;
}

####################################################################################################################################
# BLOCK_READ
#
# Read a block from the protocol layer.
####################################################################################################################################
sub block_read
{
    my $self = shift;
    my $hIn = shift;
    my $strBlockRef = shift;
    my $bProtocol = shift;

    my $iBlockSize;
    my $strMessage;

    if ($bProtocol)
    {
        # Read the block header and make sure it's valid
        my $strBlockHeader = $self->read_line($hIn);

        if ($strBlockHeader !~ /^block -{0,1}[0-9]+( .*){0,1}$/)
        {
            $self->wait_pid();
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
                $iBlockIn = sysread($hIn, $$strBlockRef, $iBlockSize - $iBlockRead, $iBlockRead + $iOffset);

                if (!defined($iBlockIn))
                {
                    my $strError = $!;

                    $self->wait_pid();
                    confess "only read ${iBlockRead}/${iBlockSize} block bytes from remote" .
                            (defined($strError) ? ": ${strError}" : '');
                }

                $iBlockRead += $iBlockIn;
            }
        }
    }
    else
    {
        $iBlockSize = $self->stream_read($hIn, $strBlockRef, $self->{iBlockSize},
                                         defined($$strBlockRef) ? length($$strBlockRef) : 0);
    }

    # Return the block size
    return $iBlockSize, $strMessage;
}

####################################################################################################################################
# BLOCK_WRITE
#
# Write a block to the protocol layer.
####################################################################################################################################
sub block_write
{
    my $self = shift;
    my $hOut = shift;
    my $tBlockRef = shift;
    my $iBlockSize = shift;
    my $bProtocol = shift;
    my $strMessage = shift;

    # If block size is not defined, get it from buffer length
    $iBlockSize = defined($iBlockSize) ? $iBlockSize : length($$tBlockRef);

    # Write block header to the protocol stream
    if ($bProtocol)
    {
        $self->write_line($hOut, "block ${iBlockSize}" . (defined($strMessage) ? " ${strMessage}" : ''));
    }

    # Write block if size > 0
    if ($iBlockSize > 0)
    {
        $self->stream_write($hOut, $tBlockRef, $iBlockSize);
    }
}

####################################################################################################################################
# STREAM_READ
#
# Read data from a stream.
####################################################################################################################################
sub stream_read
{
    my $self = shift;
    my $hIn = shift;
    my $tBlockRef = shift;
    my $iBlockSize = shift;
    my $bOffset = shift;

    # Read a block from the stream
    my $iBlockIn = sysread($hIn, $$tBlockRef, $iBlockSize, $bOffset ? length($$tBlockRef) : false);

    if (!defined($iBlockIn))
    {
        $self->wait_pid();
        confess &log(ERROR, 'unable to read');
    }

    return $iBlockIn;
}

####################################################################################################################################
# STREAM_WRITE
#
# Write data to a stream.
####################################################################################################################################
sub stream_write
{
    my $self = shift;
    my $hOut = shift;
    my $tBlockRef = shift;
    my $iBlockSize = shift;

    # If block size is not defined, get it from buffer length
    $iBlockSize = defined($iBlockSize) ? $iBlockSize : length($$tBlockRef);

    # Write the block
    my $iBlockOut = syswrite($hOut, $$tBlockRef, $iBlockSize);

    # Report any errors
    if (!defined($iBlockOut) || $iBlockOut != $iBlockSize)
    {
        my $strError = $!;

        $self->wait_pid();
        confess "unable to write ${iBlockSize} bytes" . (defined($strError) ? ': ' . $strError : '');
    }
}

####################################################################################################################################
# BINARY_XFER
#
# Copies data from one file handle to another, optionally compressing or decompressing the data in stream.  If $strRemote != none
# then one side is a protocol stream, though this can be controlled with the bProtocol param.
####################################################################################################################################
sub binary_xfer
{
    my $self = shift;
    my $hIn = shift;
    my $hOut = shift;
    my $strRemote = shift;
    my $bSourceCompressed = shift;
    my $bDestinationCompress = shift;
    my $bProtocol = shift;

    # The input stream must be defined (output is optional)
    if (!defined($hIn))
    {
        confess &log(ASSERT, 'hIn is not defined');
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
                                                 Bufsize => $self->{iBlockSize}, LimitOutput => 1);

            if ($iZLibStatus != Z_OK)
            {
                confess &log(ERROR, "unable create a inflate object: ${iZLibStatus}");
            }

            # Read all input
            do
            {
                # Read a block from the input stream
                ($iBlockSize, $strMessage) = $self->block_read($hIn, \$tCompressedBuffer, $bProtocol);

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
                                    $self->stream_write($hOut, \$tUncompressedBuffer, $iUncompressedBufferSize);
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
                ($iBlockSize, $strMessage) = $self->block_read($hIn, \$tBuffer, $bProtocol);

                # If the block contains data, write it
                if ($iBlockSize > 0)
                {
                    # Add data to checksum and size
                    if (!$bProtocol)
                    {
                        $oSHA->add($tBuffer);
                        $iFileSize += $iBlockSize;
                    }

                    $self->stream_write($hOut, \$tBuffer, $iBlockSize);
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
                $iBlockSize = $self->stream_read($hIn, \$tUncompressedBuffer, $self->{iBlockSize});

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
                            $self->block_write($hOut, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $strMessage);
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
                    $self->block_write($hOut, \$tCompressedBuffer, $iCompressedBufferSize, $bProtocol, $strMessage);
                    undef($strMessage);
                }

                $self->block_write($hOut, undef, 0, $bProtocol, "${strChecksum}-${iFileSize}");
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
                $iBlockSize = $self->stream_read($hIn, \$tBuffer, $self->{iBlockSize});

                # Write a block if size > 0
                if ($iBlockSize > 0)
                {
                    $self->block_write($hOut, \$tBuffer, $iBlockSize, $bProtocol, $strMessage);
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
                $self->block_write($hOut, undef, 0, $bProtocol, $strMessage);
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

####################################################################################################################################
# OUTPUT_READ
#
# Read output from the remote process.
####################################################################################################################################
sub output_read
{
    my $self = shift;
    my $bOutputRequired = shift;
    my $strErrorPrefix = shift;
    my $bSuppressLog = shift;

    my $strLine;
    my $strOutput;
    my $bError = false;
    my $iErrorCode;
    my $strError;

    # Read output lines
    while ($strLine = $self->read_line($self->{hOut}, false))
    {
        if ($strLine =~ /^ERROR.*/)
        {
            $bError = true;

            $iErrorCode = (split(' ', $strLine))[1];

            last;
        }

        if ($strLine =~ /^OK$/)
        {
            last;
        }

        $strOutput .= (defined($strOutput) ? "\n" : '') . substr($strLine, 1);
    }

    # Check if the process has exited abnormally
    $self->wait_pid();

    # Raise any errors
    if ($bError)
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}: " : '') .
                            (defined($strOutput) ? "${strOutput}" : ''), $iErrorCode, $bSuppressLog);
    }

    # If output is required and there is no output, raise exception
    if (defined($bOutputRequired) && $bOutputRequired && !defined($strOutput))
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}: " : '') . 'output is not defined');
    }

    # Return output
    return $strOutput;
}

####################################################################################################################################
# OUTPUT_WRITE
#
# Write output for the master process.
####################################################################################################################################
sub output_write
{
    my $self = shift;
    my $strOutput = shift;

    if (defined($strOutput))
    {
        $self->string_write(*STDOUT, "${strOutput}");

        if (!syswrite(*STDOUT, "\n"))
        {
            confess 'unable to write output';
        }
    }

    if (!syswrite(*STDOUT, "OK\n"))
    {
        confess 'unable to write output';
    }
}

####################################################################################################################################
# COMMAND_PARAM_STRING
#
# Output command parameters in the hash as a string (used for debugging).
####################################################################################################################################
sub command_param_string
{
    my $self = shift;
    my $oParamHashRef = shift;

    my $strParamList = '';

    if (defined($oParamHashRef))
    {
        foreach my $strParam (sort(keys $oParamHashRef))
        {
            $strParamList .= (defined($strParamList) ? ',' : '') . "${strParam}=" .
                             (defined(${$oParamHashRef}{"${strParam}"}) ? ${$oParamHashRef}{"${strParam}"} : '[undef]');
        }
    }

    return $strParamList;
}

####################################################################################################################################
# COMMAND_READ
#
# Read command sent by the master process.
####################################################################################################################################
sub command_read
{
    my $self = shift;
    my $oParamHashRef = shift;

    my $strLine;
    my $strCommand;

    while ($strLine = $self->read_line(*STDIN))
    {
        if (!defined($strCommand))
        {
            if ($strLine =~ /:$/)
            {
                $strCommand = substr($strLine, 0, length($strLine) - 1);
            }
            else
            {
                $strCommand = $strLine;
                last;
            }
        }
        else
        {
            if ($strLine eq 'end')
            {
                last;
            }

            my $iPos = index($strLine, '=');

            if ($iPos == -1)
            {
                confess "param \"${strLine}\" is missing = character";
            }

            my $strParam = substr($strLine, 0, $iPos);
            my $strValue = substr($strLine, $iPos + 1);

            ${$oParamHashRef}{"${strParam}"} = ${strValue};
        }
    }

    return $strCommand;
}

####################################################################################################################################
# COMMAND_WRITE
#
# Send command to remote process.
####################################################################################################################################
sub command_write
{
    my $self = shift;
    my $strCommand = shift;
    my $oParamRef = shift;

    my $strOutput = $strCommand;

    if (defined($oParamRef))
    {
        $strOutput = "${strCommand}:\n";

        foreach my $strParam (sort(keys $oParamRef))
        {
            if ($strParam =~ /=/)
            {
                confess &log(ASSERT, "param \"${strParam}\" cannot contain = character");
            }

            my $strValue = ${$oParamRef}{"${strParam}"};

            if ($strParam =~ /\n\$/)
            {
                confess &log(ASSERT, "param \"${strParam}\" value cannot end with LF");
            }

            if (defined(${strValue}))
            {
                $strOutput .= "${strParam}=${strValue}\n";
            }
        }

        $strOutput .= 'end';
    }

    &log(TRACE, OP_PROTOCOL_COMMAND_WRITE . "=>:Protocol->command_write:\n" . $strOutput);

    if (!syswrite($self->{hIn}, "${strOutput}\n"))
    {
        confess 'unable to write command';
    }
}

####################################################################################################################################
# COMMAND_EXECUTE
#
# Send command to remote process and wait for output.
####################################################################################################################################
sub command_execute
{
    my $self = shift;
    my $strCommand = shift;
    my $oParamRef = shift;
    my $bOutputRequired = shift;
    my $strErrorPrefix = shift;

    $self->command_write($strCommand, $oParamRef);

    return $self->output_read($bOutputRequired, $strErrorPrefix);
}

####################################################################################################################################
# paramGet
#
# Helper function that returns the param or an error if required and it does not exist.
####################################################################################################################################
sub paramGet
{
    my $oParamHashRef = shift;
    my $strParam = shift;
    my $bRequired = shift;

    my $strValue = ${$oParamHashRef}{$strParam};

    if (!defined($strValue) && (!defined($bRequired) || $bRequired))
    {
        confess "${strParam} must be defined";
    }

    return $strValue;
}

####################################################################################################################################
# run
####################################################################################################################################
# sub run
# {
#     my $self = shift;
#
#     # Create the file object
#     my $oFile = new BackRest::File
#     (
#         $self->stanza(),
#         $self->repoPath(),
#         undef,
#         $self,
#     );
#
#     # Create objects
#     my $oArchive = new BackRest::Archive();
#     my $oInfo = new BackRest::Info();
#     my $oJSON = JSON::PP->new();
#     my $oDb = new BackRest::Db(false);
#
#     # Command string
#     my $strCommand = OP_NOOP;
#
#     # Loop until the exit command is received
#     while ($strCommand ne OP_EXIT)
#     {
#         my %oParamHash;
#
#         $strCommand = $self->command_read(\%oParamHash);
#
#         eval
#         {
#             # Copy file
#             if ($strCommand eq OP_FILE_COPY ||
#                 $strCommand eq OP_FILE_COPY_IN ||
#                 $strCommand eq OP_FILE_COPY_OUT)
#             {
#                 my $bResult;
#                 my $strChecksum;
#                 my $iFileSize;
#
#                 # Copy a file locally
#                 if ($strCommand eq OP_FILE_COPY)
#                 {
#                     ($bResult, $strChecksum, $iFileSize) =
#                         $oFile->copy(PATH_ABSOLUTE, paramGet(\%oParamHash, 'source_file'),
#                                      PATH_ABSOLUTE, paramGet(\%oParamHash, 'destination_file'),
#                                      paramGet(\%oParamHash, 'source_compressed'),
#                                      paramGet(\%oParamHash, 'destination_compress'),
#                                      paramGet(\%oParamHash, 'ignore_missing_source', false),
#                                      undef,
#                                      paramGet(\%oParamHash, 'mode', false),
#                                      paramGet(\%oParamHash, 'destination_path_create') ? 'Y' : 'N',
#                                      paramGet(\%oParamHash, 'user', false),
#                                      paramGet(\%oParamHash, 'group', false),
#                                      paramGet(\%oParamHash, 'append_checksum', false));
#                 }
#                 # Copy a file from STDIN
#                 elsif ($strCommand eq OP_FILE_COPY_IN)
#                 {
#                     ($bResult, $strChecksum, $iFileSize) =
#                         $oFile->copy(PIPE_STDIN, undef,
#                                      PATH_ABSOLUTE, paramGet(\%oParamHash, 'destination_file'),
#                                      paramGet(\%oParamHash, 'source_compressed'),
#                                      paramGet(\%oParamHash, 'destination_compress'),
#                                      undef, undef,
#                                      paramGet(\%oParamHash, 'mode', false),
#                                      paramGet(\%oParamHash, 'destination_path_create'),
#                                      paramGet(\%oParamHash, 'user', false),
#                                      paramGet(\%oParamHash, 'group', false),
#                                      paramGet(\%oParamHash, 'append_checksum', false));
#                 }
#                 # Copy a file to STDOUT
#                 elsif ($strCommand eq OP_FILE_COPY_OUT)
#                 {
#                     ($bResult, $strChecksum, $iFileSize) =
#                         $oFile->copy(PATH_ABSOLUTE, paramGet(\%oParamHash, 'source_file'),
#                                      PIPE_STDOUT, undef,
#                                      paramGet(\%oParamHash, 'source_compressed'),
#                                      paramGet(\%oParamHash, 'destination_compress'));
#                 }
#
#                 $self->output_write(($bResult ? 'Y' : 'N') . " " . (defined($strChecksum) ? $strChecksum : '?') . " " .
#                                        (defined($iFileSize) ? $iFileSize : '?'));
#             }
#             # List files in a path
#             elsif ($strCommand eq OP_FILE_LIST)
#             {
#                 my $strOutput;
#
#                 foreach my $strFile ($oFile->list(PATH_ABSOLUTE, paramGet(\%oParamHash, 'path'),
#                                                   paramGet(\%oParamHash, 'expression', false),
#                                                   paramGet(\%oParamHash, 'sort_order'),
#                                                   paramGet(\%oParamHash, 'ignore_missing')))
#                 {
#                     if (defined($strOutput))
#                     {
#                         $strOutput .= "\n";
#                     }
#
#                     $strOutput .= $strFile;
#                 }
#
#                 $self->output_write($strOutput);
#             }
#             # Create a path
#             elsif ($strCommand eq OP_FILE_PATH_CREATE)
#             {
#                 $oFile->path_create(PATH_ABSOLUTE, paramGet(\%oParamHash, 'path'), paramGet(\%oParamHash, 'mode', false));
#                 $self->output_write();
#             }
#             # Check if a file/path exists
#             elsif ($strCommand eq OP_FILE_EXISTS)
#             {
#                 $self->output_write($oFile->exists(PATH_ABSOLUTE, paramGet(\%oParamHash, 'path')) ? 'Y' : 'N');
#             }
#             # Wait
#             elsif ($strCommand eq OP_FILE_WAIT)
#             {
#                 $self->output_write($oFile->wait(PATH_ABSOLUTE));
#             }
#             # Generate a manifest
#             elsif ($strCommand eq OP_FILE_MANIFEST)
#             {
#                 my %oManifestHash;
#
#                 $oFile->manifest(PATH_ABSOLUTE, paramGet(\%oParamHash, 'path'), \%oManifestHash);
#
#                 my $strOutput = "name\ttype\tuser\tgroup\tmode\tmodification_time\tinode\tsize\tlink_destination";
#
#                 foreach my $strName (sort(keys $oManifestHash{name}))
#                 {
#                     $strOutput .= "\n${strName}\t" .
#                         $oManifestHash{name}{"${strName}"}{type} . "\t" .
#                         (defined($oManifestHash{name}{"${strName}"}{user}) ? $oManifestHash{name}{"${strName}"}{user} : "") . "\t" .
#                         (defined($oManifestHash{name}{"${strName}"}{group}) ? $oManifestHash{name}{"${strName}"}{group} : "") . "\t" .
#                         (defined($oManifestHash{name}{"${strName}"}{mode}) ? $oManifestHash{name}{"${strName}"}{mode} : "") . "\t" .
#                         (defined($oManifestHash{name}{"${strName}"}{modification_time}) ?
#                             $oManifestHash{name}{"${strName}"}{modification_time} : "") . "\t" .
#                         (defined($oManifestHash{name}{"${strName}"}{inode}) ? $oManifestHash{name}{"${strName}"}{inode} : "") . "\t" .
#                         (defined($oManifestHash{name}{"${strName}"}{size}) ? $oManifestHash{name}{"${strName}"}{size} : "") . "\t" .
#                         (defined($oManifestHash{name}{"${strName}"}{link_destination}) ?
#                             $oManifestHash{name}{"${strName}"}{link_destination} : "");
#                 }
#
#                 $self->output_write($strOutput);
#             }
#             # Archive push checks
#             elsif ($strCommand eq OP_ARCHIVE_PUSH_CHECK)
#             {
#                 my ($strArchiveId, $strChecksum) = $oArchive->pushCheck($oFile,
#                                                                         paramGet(\%oParamHash, 'wal-segment'),
#                                                                         undef,
#                                                                         paramGet(\%oParamHash, 'db-version'),
#                                                                         paramGet(\%oParamHash, 'db-sys-id'));
#
#                 $self->output_write("${strArchiveId}\t" . (defined($strChecksum) ? $strChecksum : 'Y'));
#             }
#             elsif ($strCommand eq OP_ARCHIVE_GET_CHECK)
#             {
#                 $self->output_write($oArchive->getCheck($oFile));
#             }
#             # Info list stanza
#             elsif ($strCommand eq OP_INFO_LIST_STANZA)
#             {
#                 $self->output_write(
#                     $oJSON->encode(
#                         $oInfo->listStanza($oFile,
#                                        paramGet(\%oParamHash, 'stanza', false))));
#             }
#             elsif ($strCommand eq OP_DB_INFO)
#             {
#                 my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) =
#                     $oDb->info($oFile, paramGet(\%oParamHash, 'db-path'));
#
#                 $self->output_write("${strDbVersion}\t${iControlVersion}\t${iCatalogVersion}\t${ullDbSysId}");
#             }
#             # Continue if noop or exit
#             elsif ($strCommand ne OP_NOOP && $strCommand ne OP_EXIT)
#             {
#                 confess "invalid command: ${strCommand}";
#             }
#         };
#
#         # Process errors
#         if ($@)
#         {
#             $self->error_write($@);
#         }
#     }
# }

1;
