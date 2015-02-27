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
    DEFAULT_BLOCK_SIZE  => 8388606
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
    my $oGzip;
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
            if (!$bDestinationCompress && $bFirst)
            {
                $strBlockHeader = $self->read_line($hIn);

                if ($strBlockHeader !~ /^block [0-9]+$/)
                {
                    $self->wait_pid();
                    confess "unable to read block header ${strBlockHeader}";
                }

                # Parse the block size
                $iBlockSize = trim(substr($strBlockHeader, index($strBlockHeader, ' ') + 1));

                if ($iBlockSize != 10)
                {
                    confess &log(ERROR, 'gzip header length should be 10 bytes');
                }

                $iBlockBufferIn = sysread($hIn, $strBlockBuffer, $iBlockSize);

                &log(INFO, "header block buffer in = ${iBlockBufferIn}");

                if (!defined($iBlockBufferIn))
                {
                    my $strError = $!;

                    $self->wait_pid();
                    confess "unable to read block #${iBlockTotal}/${iBlockSize} bytes from remote" .
                            (defined($strError) ? ": ${strError}" : '');
                }

                $oGzip = new IO::Uncompress::Gunzip(\$strBlockBuffer, Transparent => 0)
                    or confess "IO::Uncompress::Gunzip failed: $GunzipError";

                $bFirst = false;
                $iBlockSize = $self->{iBlockSize};
            }

            # Read the block header (at start or after the previous block is complete)
            if ($iBlockInTotal == $iBlockSize)
            {
                $strBlockHeader = $self->read_line($hIn);

                if ($strBlockHeader !~ /^block [0-9]+$/)
                {
                    $self->wait_pid();
                    confess "unable to read block header ${strBlockHeader}";
                }

                # Parse the block size
                $iBlockSize = trim(substr($strBlockHeader, index($strBlockHeader, ' ') + 1));
                $iBlockTotal += 1;
                $iBlockInTotal = 0;

                &log(INFO, "block size = ${iBlockSize}");
            }

            # If block size > 0 then read from protocol stream.  The entire block may not be read on the first try so keep
            # reading until we get it all.
            if ($iBlockSize != 0)
            {
                if ($strRemote eq 'in' && !$bDestinationCompress)
                {
                    $iBlockBufferIn = sysread($hIn, $strBlockBuffer, $iBlockSize - $iBlockInTotal);

                    &log(INFO, "block buffer in = ${iBlockBufferIn}");

                    if (!defined($iBlockBufferIn))
                    {
                        my $strError = $!;

                        $self->wait_pid();
                        confess "unable to read block #${iBlockTotal}/${iBlockSize} bytes from remote" .
                                (defined($strError) ? ": ${strError}" : '');
                    }

                    $iBlockInTotal += $iBlockBufferIn;

                    # Decompress the block
                    $iBlockIn = $oGzip->read($strBlock);

                    if ($iBlockIn < 0)
                    {
                        confess &log(ERROR, "unable to decompress stream: ${GunzipError}");
                    }

#                    &log(INFO, "block in = ${iBlockIn} - " . length($strBlock));
                }
                else
                {
                    $iBlockIn = sysread($hIn, $strBlock, $iBlockSize - $iBlockInTotal);

                    if (!defined($iBlockIn))
                    {
                        my $strError = $!;

                        $self->wait_pid();
                        confess "unable to read block #${iBlockTotal}/${iBlockSize} bytes from remote" .
                                (defined($strError) ? ": ${strError}" : '');
                    }

                    $iBlockInTotal += $iBlockIn;
                }
            }
            # Else indicate the stream is complete
            else
            {
                $iBlockIn = 0;
            }
        }
        # Read from file input stream
        else
        {
            # If source is not already compressed then compress it
            if ($strRemote eq 'out' && !$bSourceCompressed)
            {
                # Write the header
                if ($bFirst)
                {
                    $bFirst = false;

                    $oGzip = new IO::Compress::Gzip(\$strBlock, Append => 0)
                        or confess "IO::Compress::Gzip failed: $GzipError";

                    if (!defined($strBlock))
                    {
                        confess &log(ERROR, "gzip header block does not exist");
                    }

                    $iBlockIn = length($strBlock);
                }
                else
                {
                    # Read a block from the stream
                    $iBlockBufferIn = sysread($hIn, $strBlockBuffer, $iBlockSize);

                    if (!defined($iBlockBufferIn))
                    {
                        $self->wait_pid();
                        confess &log(ERROR, 'unable to read');
                    }

                    # If block size > 0 then compress
                    if ($iBlockBufferIn > 0)
                    {

                        $iBlockIn = $oGzip->syswrite($strBlockBuffer, $iBlockBufferIn);

                        if (!defined($iBlockIn) || $iBlockIn != $iBlockBufferIn)
                        {
                            $self->wait_pid();
                            confess &log(ERROR, 'unable to read');
                        }
                    }

                    # If there was nothing new to compress then close
                    if (!defined($strBlock))
                    {
                        $oGzip->close();
                    }

                    # If there is data in the compressed buffer, then output
                    if (defined($strBlock))
                    {
                        $iBlockIn = length($strBlock);
                    }
                    # Else indicate that the stream is complete
                    else
                    {
                        $iBlockIn = 0;
                    }
                }
            }
            # If source is already compressed or transfer is not compressed then just read the stream
            else
            {
                $iBlockIn = sysread($hIn, $strBlockBuffer, $iBlockSize);

                if (!defined($iBlockIn))
                {
                    $self->wait_pid();
                    confess &log(ERROR, 'unable to read');
                }
            }
        }

        # Write block header to the protocol stream
        if ($strRemote eq 'out')
        {
            $self->write_line($hOut, "block ${iBlockIn}");
        }

        # Write block to output stream
        if ($iBlockIn > 0)
        {
            $iBlockOut = syswrite($hOut, $strBlock, $iBlockIn);

            if (!defined($iBlockOut) || $iBlockOut != $iBlockIn)
            {
                $self->wait_pid();
                confess "unable to write ${iBlockIn} bytes" . (defined($!) ? ': ' . $! : '');
            }

            undef($strBlock);
        }
        # When there is no more data to write then exit
        else
        {
            last;
        }
    }
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
