####################################################################################################################################
# REMOTE MODULE
####################################################################################################################################
package BackRest::Remote;

use threads;
use strict;
use warnings;
use Carp;

use Scalar::Util;
use Net::OpenSSH;
use File::Basename;
use POSIX ':sys_wait_h';
use Scalar::Util 'blessed';
use IO::Compress::Gzip qw($GzipError);
use IO::Uncompress::Gunzip qw($GunzipError);

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;

use Exporter qw(import);
our @EXPORT = qw(DB BACKUP NONE);

####################################################################################################################################
# DB/BACKUP Constants
####################################################################################################################################
use constant
{
    DB              => 'db',
    BACKUP          => 'backup',
    NONE            => 'none'
};

####################################################################################################################################
# Remote xfer default block size constant
####################################################################################################################################
use constant
{
    DEFAULT_BLOCK_SIZE  => 8192
};

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;       # Class name
    my $strHost = shift;     # Host to connect to for remote (optional as this can also be used on the remote)
    my $strUser = shift;     # User to connect to for remote (must be set if strHost is set)
    my $strCommand = shift;  # Command to execute on remote
    my $iBlockSize = shift;  # Optionally, set the block size (defaults to DEFAULT_BLOCK_SIZE)

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Create the greeting that will be used to check versions with the remote
    $self->{strGreeting} = 'PG_BACKREST_REMOTE ' . version_get();

    # Set default block size
    if (!defined($iBlockSize))
    {
        $self->{iBlockSize} = DEFAULT_BLOCK_SIZE;
    }
    else
    {
        $self->{iBlockSize} = $iBlockSize;
    }

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
        $self->{strCommand} = $strCommand;

        # Set SSH Options
        my $strOptionSSHRequestTTY = 'RequestTTY=yes';
        my $strOptionSSHCompression = 'Compression=no';

        &log(TRACE, 'connecting to remote ssh host ' . $self->{strHost});

        # Make SSH connection
        $self->{oSSH} = Net::OpenSSH->new($self->{strHost}, timeout => 600, user => $self->{strUser},
                                          master_opts => [-o => $strOptionSSHCompression, -o => $strOptionSSHRequestTTY]);

        $self->{oSSH}->error and confess &log(ERROR, "unable to connect to $self->{strHost}: " . $self->{oSSH}->error);

        # Execute remote command
        ($self->{hIn}, $self->{hOut}, $self->{hErr}, $self->{pId}) = $self->{oSSH}->open3($self->{strCommand});

        $self->greeting_read();
    }

    return $self;
}

####################################################################################################################################
# THREAD_KILL
####################################################################################################################################
sub thread_kill
{
    my $self = shift;
}

####################################################################################################################################
# DESTRUCTOR
####################################################################################################################################
sub DEMOLISH
{
    my $self = shift;

    $self->thread_kill();
}

####################################################################################################################################
# CLONE
####################################################################################################################################
sub clone
{
    my $self = shift;

    return BackRest::Remote->new
    (
        $self->{strHost},
        $self->{strUser},
        $self->{strCommand},
        $self->{iBlockSize}
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
        confess &log(ERROR, 'remote version mismatch');
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

    if (!syswrite(*STDOUT, "$self->{strGreeting}\n"))
    {
        confess 'unable to write greeting';
    }
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

    $strBuffer = $strBuffer . "\n";

    my $iLineOut = syswrite($hOut, $strBuffer, length($strBuffer));

    if (!defined($iLineOut) || $iLineOut != length($strBuffer))
    {
        confess 'unable to write ' . length($strBuffer) . ' byte(s)';
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

    if (defined($self->{pId}) && waitpid($self->{pId}, WNOHANG) != 0)
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

    # Read the block header and make sure it's valid
    my $strBlockHeader = $self->read_line($hIn);

    if ($strBlockHeader !~ /^block -{0,1}[0-9]+$/)
    {
        $self->wait_pid();
        confess "unable to read block header ${strBlockHeader}";
    }

    # Get block size from the header
    my $iBlockSize = trim(substr($strBlockHeader, index($strBlockHeader, ' ') + 1));

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

    # Return the block size
    return $iBlockSize;
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

    # If block size is not defined, get it from buffer length
    $iBlockSize = defined($iBlockSize) ? $iBlockSize : length($$tBlockRef);

    # Write block header to the protocol stream
    $self->write_line($hOut, "block ${iBlockSize}");
#    &log(INFO, "block ${iBlockSize}");

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
# then one side is a protocol stream.
####################################################################################################################################
sub binary_xfer
{
    my $self = shift;
    my $hIn = shift;
    my $hOut = shift;
    my $strRemote = shift;
    my $bSourceCompressed = shift;
    my $bDestinationCompress = shift;

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

    # Working variables
    my $iBlockSize = $self->{iBlockSize};
    my $iBlockIn;
    my $iBlockInTotal = $iBlockSize;
    my $iBlockBufferIn;
    my $iBlockOut;
    my $iBlockTotal = 0;
    my $strBlockHeader;
    my $strBlock;
    my $strBlockBuffer;
    my $oGzip = undef;
    my $oSHA = undef;
    my $iFileSize = undef;

    my $bFirst = true;

    # Both the in and out streams must be defined
    if (!defined($hIn) || !defined($hOut))
    {
        confess &log(ASSERT, 'hIn or hOut is not defined');
    }

    while (1)
    {
        # Read from the protocol stream
        if ($strRemote eq 'in')
        {
            # If the destination should not be compressed then decompress
            if (!$bDestinationCompress)
            {
                # Read a block from the protocol stream
                $iBlockSize = $self->block_read($hIn, \$strBlockBuffer);

                # If block size = -1 it means an error happened on the remote we need to exit so it can be returned.
                if ($iBlockSize == -1)
                {
                    last;
                }

                # If this is the first block then initialize Gunzip
                if ($bFirst)
                {
                    $oSHA = Digest::SHA->new('sha1');
                    $iFileSize = 0;

                    if ($iBlockSize == 0)
                    {
                        &log(ASSERT, 'first protocol block is zero');
                    }

                    # Gunzip doesn't like to be initialized with just the header, so if the first block is 10 bytes then fetch
                    # another another block to make sure so is at least some payload.
                    if ($iBlockSize <= 10)
                    {
                        $iBlockSize = $self->block_read($hIn, \$strBlockBuffer);
                    }

                    # Initialize Gunzip
                    $oGzip = new IO::Uncompress::Gunzip(\$strBlockBuffer, Append => 1, Transparent => 0,
                                                                          BlockSize => $self->{iBlockSize})
                        or confess "IO::Uncompress::Gunzip failed (${iBlockSize}): $GunzipError";

                    # Clear first block flag
                    $bFirst = false;
                }

                # If the block contains data, decompress it
                if ($iBlockSize > 0)
                {
                    my $iUncompressedTotal = 0;

                    # Loop while there is more data to uncompress
                    while (!$oGzip->eof())
                    {
                        # Decompress the block
                        $iBlockIn = $oGzip->read($strBlock);

                        if ($iBlockIn < 0)
                        {
                            confess &log(ERROR, "unable to decompress stream ($iBlockIn): ${GunzipError}");
                        }

                        $iUncompressedTotal += $iBlockIn;
                    }

                    # Write out the uncompressed bytes if there are any
                    if ($iUncompressedTotal > 0)
                    {
                        $oSHA->add($strBlock);
                        $iFileSize += $iUncompressedTotal;

                        $self->stream_write($hOut, \$strBlock, $iUncompressedTotal);
                        undef($strBlock);
                    }
                }
                # Else close gzip
                else
                {
                    $iBlockIn = $oGzip->close();
                    last;
                }
            }
            # If the destination should be compressed then just write out the already compressed stream
            else
            {
                # Read a block from the protocol stream
                $iBlockSize = $self->block_read($hIn, \$strBlock);

                # If the block contains data, write it
                if ($iBlockSize > 0)
                {
                    $self->stream_write($hOut, \$strBlock, $iBlockSize);
                    undef($strBlock);
                }
                # Else done
                else
                {
                    last;
                }
            }
        }
        # Read from file input stream
        else
        {
            # If source is not already compressed then compress it
            if ($strRemote eq 'out' && !$bSourceCompressed)
            {
                # Create the gzip object
                if ($bFirst)
                {
                    $oSHA = Digest::SHA->new('sha1');
                    $iFileSize = 0;

                    $oGzip = new IO::Compress::Gzip(\$strBlock, Append => 1)
                        or confess "IO::Compress::Gzip failed: $GzipError";

                    # Clear first block flag
                    $bFirst = false;
                }

                # Read a block from the stream
                $iBlockBufferIn = $self->stream_read($hIn, \$strBlockBuffer, $iBlockSize);
                $oSHA->add($strBlockBuffer);
                $iFileSize += $iBlockBufferIn;

                # If block size > 0 then compress
                if ($iBlockBufferIn > 0)
                {
                    $iBlockIn = $oGzip->syswrite($strBlockBuffer, $iBlockBufferIn);

                    if (!defined($iBlockIn) || $iBlockIn != $iBlockBufferIn)
                    {
                        $self->wait_pid();
                        confess &log(ERROR, "IO::Compress::Gzip failed: $GzipError");
                    }

                    if (defined($strBlock) && length($strBlock) > $self->{iBlockSize})
                    {
                        $self->block_write($hOut, \$strBlock);
                        undef($strBlock);
                    }
                }
                # If there was nothing new to compress then close
                else
                {
                    $oGzip->close();

                    $self->block_write($hOut, \$strBlock);
                    $self->block_write($hOut, undef, 0);
                    last;
                }
            }
            # If source is already compressed or transfer is not compressed then just read the stream
            else
            {
                $iBlockIn = $self->stream_read($hIn, \$strBlock, $iBlockSize);

                if ($iBlockIn > 0)
                {
                    $self->block_write($hOut, \$strBlock, $iBlockIn);
                }
                else
                {
                    $self->block_write($hOut, undef, 0);
                    last;
                }
            }
        }
    }

    # Return the checksum and size if they are available
    return (defined($oSHA) ? $oSHA->hexdigest() : undef), $iFileSize;
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
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}" : '') .
                            (defined($strOutput) ? ": ${strOutput}" : ''), $iErrorCode, $bSuppressLog);
    }

    # If output is required and there is no output, raise exception
    if ($bOutputRequired && !defined($strOutput))
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

    my $strParamList;

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

    &log(TRACE, "Remote->command_write:\n" . $strOutput);

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

1;
