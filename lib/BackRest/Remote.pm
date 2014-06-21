####################################################################################################################################
# REMOTE MODULE
####################################################################################################################################
package BackRest::Remote;

use threads;
use strict;
use warnings;
use Carp;

use Moose;
use Net::OpenSSH;
use File::Basename;
use POSIX ":sys_wait_h";
use IO::Compress::Gzip qw(gzip $GzipError);
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);

use lib dirname($0) . "/../lib";
use BackRest::Exception;
use BackRest::Utility;

####################################################################################################################################
# Remote xfer default block size constant
####################################################################################################################################
use constant
{
    DEFAULT_BLOCK_SIZE  => 8192
};

####################################################################################################################################
# Module variables
####################################################################################################################################
# Protocol strings
has strGreeting => (is => 'ro', default => 'PG_BACKREST_REMOTE');

# Command strings
has strCommand => (is => 'bare');

# Module variables
has strHost => (is => 'bare');            # Host host
has strUser => (is => 'bare');            # User user
has oSSH => (is => 'bare');               # SSH object

# Process variables
has pId => (is => 'bare');                # Process Id
has hIn => (is => 'bare');                # Input stream
has hOut => (is => 'bare');               # Output stream
has hErr => (is => 'bare');               # Error stream

# Block size
has iBlockSize => (is => 'bare', default => DEFAULT_BLOCK_SIZE);  # Set block size to default

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub BUILD
{
    my $self = shift;

    $self->{strGreeting} .= " " . version_get();

    if (defined($self->{strHost}))
    {
        # User must be defined
        if (!defined($self->{strUser}))
        {
            confess &log(ASSERT, "strUser must be defined");
        }

        # User must be defined
        if (!defined($self->{strCommand}))
        {
            confess &log(ASSERT, "strCommand must be defined");
        }

        # Set SSH Options
        my $strOptionSSHRequestTTY = "RequestTTY=yes";
        my $strOptionSSHCompression = "Compression=no";

        &log(TRACE, "connecting to remote ssh host " . $self->{strHost});

        # Make SSH connection
        $self->{oSSH} = Net::OpenSSH->new($self->{strHost}, timeout => 300, user => $self->{strUser},
                                  master_opts => [-o => $strOptionSSHCompression, -o => $strOptionSSHRequestTTY]);

        $self->{oSSH}->error and confess &log(ERROR, "unable to connect to $self->{strHost}: " . $self->{oSSH}->error);

        # Execute remote command
        ($self->{hIn}, $self->{hOut}, $self->{hErr}, $self->{pId}) = $self->{oSSH}->open3($self->{strCommand});

        $self->greeting_read();
    }
    else
    {

    }
}

####################################################################################################################################
# CLONE
####################################################################################################################################
sub clone
{
    my $self = shift;

    return pg_backrest_remote->new
    (
        strCommand => $self->{strCommand},
        strHost => $self->{strUser},
        strUser => $self->{strHost},
        iBlockSize => $self->{iBlockSize}
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
        confess &log(ERROR, "remote version mismatch");
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
        confess "unable to write greeting";
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

    if (!syswrite($hOut, "." . $strBuffer))
    {
        confess "unable to write string";
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
        if ($oMessage->isa("BackRest::Exception"))
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

    if (!syswrite(*STDOUT, "\nERROR" . (defined($iCode) ? " $iCode" : "") . "\n"))
    {
        confess "unable to write error";
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

            confess &log(ERROR, "unable to read 1 byte" . (defined($!) ? ": " . $! : ""));
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
        confess "unable to write " . length($strBuffer) . " byte(s)";
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
        my $strError = "no error on stderr";

        if (!defined($self->{hErr}))
        {
            $strError = "no error captured because stderr is already closed";
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
# Copies data from one file handle to another, optionally compressing or decompressing the data in stream.
####################################################################################################################################
sub binary_xfer
{
    my $self = shift;
    my $hIn = shift;
    my $hOut = shift;
    my $strRemote = shift;
    my $bSourceCompressed = shift;
    my $bDestinationCompress = shift;

    if (!defined($strRemote))
    {
        $strRemote = 'none';
    }
    else
    {
        $bSourceCompressed = defined($bSourceCompressed) ? $bSourceCompressed : false;
        $bDestinationCompress = defined($bDestinationCompress) ? $bDestinationCompress : false;
    }

    my $iBlockSize = $self->{iBlockSize};
    my $iBlockIn;
    my $iBlockOut;
    my $strBlockHeader;
    my $strBlock;
    my $oGzip;
    my $hPipeIn;
    my $hPipeOut;
    my $pId;

    # Both the in and out streams must be defined
    if (!defined($hIn) || !defined($hOut))
    {
        confess &log(ASSERT, "hIn or hOut is not defined");
    }

    # !!! Convert this to a thread when there is time
    # Spawn a child process to do compression
    if ($strRemote eq "out" && !$bSourceCompressed)
    {
        pipe $hPipeOut, $hPipeIn;

        # fork and exit the parent process
        $pId = fork();

        if (!$pId)
        {
            close($hPipeOut);

            gzip($hIn => $hPipeIn)
                or exit 1;
                #or die confess &log(ERROR, "unable to compress: " . $GzipError);

            close($hPipeIn);

            exit 0;
        }

        close($hPipeIn);

        $hIn = $hPipeOut;
    }
    # Spawn a child process to do decompression
    elsif ($strRemote eq "in" && !$bDestinationCompress)
    {
        pipe $hPipeOut, $hPipeIn;

        # fork and exit the parent process
        $pId = fork();

        if (!$pId)
        {
            close($hPipeIn);

            gunzip($hPipeOut => $hOut)
                or exit 1;
                #or die confess &log(ERROR, "unable to uncompress: " . $GunzipError);

            close($hPipeOut);

            exit 0;
        }

        close($hPipeOut);

        $hOut = $hPipeIn;
    }

    while (1)
    {
        if ($strRemote eq 'in')
        {

            $strBlockHeader = $self->read_line($hIn);

            if ($strBlockHeader !~ /^block [0-9]+$/)
            {
                confess "unable to read block header ${strBlockHeader}";
            }

            $iBlockSize = trim(substr($strBlockHeader, index($strBlockHeader, " ") + 1));

            if ($iBlockSize != 0)
            {
                $iBlockIn = sysread($hIn, $strBlock, $iBlockSize);

                if (!defined($iBlockIn) || $iBlockIn != $iBlockSize)
                {
                    confess "unable to read ${iBlockSize} bytes from remote" . (defined($!) ? ": " . $! : "");
                }
            }
            else
            {
                $iBlockIn = 0;
            }
        }
        else
        {
            $iBlockIn = sysread($hIn, $strBlock, $iBlockSize);

            if (!defined($iBlockIn))
            {
                confess &log(ERROR, "unable to read");
            }
        }

        if ($strRemote eq 'out')
        {
            $strBlockHeader = "block ${iBlockIn}\n";

            $iBlockOut = syswrite($hOut, $strBlockHeader);

            if (!defined($iBlockOut) || $iBlockOut != length($strBlockHeader))
            {
                confess "unable to write block header";
            }
        }

        if ($iBlockIn > 0)
        {
            $iBlockOut = syswrite($hOut, $strBlock, $iBlockIn);

            if (!defined($iBlockOut) || $iBlockOut != $iBlockIn)
            {
                confess "unable to write ${iBlockIn} bytes" . (defined($!) ? ": " . $! : "");
            }
        }
        else
        {
            last;
        }
    }

    # Make sure the child process doing compress/decompress exited successfully
    if (defined($pId))
    {
        if ($strRemote eq "out")
        {
            close($hPipeOut);
        }
        elsif ($strRemote eq "in" && !$bDestinationCompress)
        {
            close($hPipeIn);
        }

        waitpid($pId, 0);
        my $iExitStatus = ${^CHILD_ERROR_NATIVE} >> 8;

        if ($iExitStatus != 0)
        {
            confess &log(ERROR, "compression/decompression child process returned " . $iExitStatus);
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

        $strOutput .= (defined($strOutput) ? "\n" : "") . substr($strLine, 1);
    }

    # Check if the process has exited abnormally
    $self->wait_pid();

    # Raise any errors
    if ($bError)
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}" : "") .
                            (defined($strOutput) ? ": ${strOutput}" : ""), $iErrorCode);
    }

    # If output is required and there is no output, raise exception
    if ($bOutputRequired && !defined($strOutput))
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}: " : "") . "output is not defined");
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
            confess "unable to write output";
        }
    }

    if (!syswrite(*STDOUT, "OK\n"))
    {
        confess "unable to write output";
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

    foreach my $strParam (sort(keys $oParamHashRef))
    {
        $strParamList .= (defined($strParamList) ? "," : "") . "${strParam}=" .
                         (defined(${$oParamHashRef}{"${strParam}"}) ? ${$oParamHashRef}{"${strParam}"} : "[undef]");
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

            my $iPos = index($strLine, "=");

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

        $strOutput .= "end";
    }

    &log(TRACE, "Remote->command_write:\n" . $strOutput);

    if (!syswrite($self->{hIn}, "${strOutput}\n"))
    {
        confess "unable to write command";
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

no Moose;
__PACKAGE__->meta->make_immutable;
