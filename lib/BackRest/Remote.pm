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
has pId => (is => 'bare');               # SSH object
has hIn => (is => 'bare');               # SSH object
has hOut => (is => 'bare');               # SSH object
has hErr => (is => 'bare');               # SSH object

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

        # if ($self->{bNoCompression})
        # {
        #     $strOptionSSHCompression = "Compression=yes";
        # }

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
        strUser => $self->{strHost}
    );
}

####################################################################################################################################
# GREETING_READ
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
    my $bCompress = shift;

    $strRemote = defined($strRemote) ? $strRemote : 'none';

    my $iBlockSize = $self->{iBlockSize};
    my $iHeaderSize = 12;
    my $iBlockIn;
    my $iBlockOut;
    my $strBlockHeader;
    my $strBlock;

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
                # 
                # if ($strRemote eq 'in')
                # {
                #     confess &log(ERROR, "block size $iBlockSize");
                # }
                $iBlockIn = 0;
            }
        }
        else
        {
            if (!defined($bCompress))
            {
                $iBlockIn = sysread($hIn, $strBlock, $iBlockSize);
                
                if (!defined($iBlockIn))
                {
                    confess "unable to read ${iBlockSize} bytes from remote: " . $!;
                }
            }
        }

        if ($strRemote eq 'out')
        {
            $strBlockHeader = "block ${iBlockIn}\n";
            
#            print "wrote block header: ${strBlockHeader}"; # REMOVE!
            
            $iBlockOut = syswrite($hOut, $strBlockHeader);
            
            if (!defined($iBlockOut) || $iBlockOut != length($strBlockHeader))
            {
                confess "unable to write block header";
            }
        }

        if ($iBlockIn > 0)
        {
            # Write to the output handle
#            print "writing: ${strBlock}\n"; # REMOVE!

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
}

####################################################################################################################################
# OUTPUT_READ
####################################################################################################################################
sub output_read
{
    my $self = shift;
    my $bOutputRequired = shift;
    my $strErrorPrefix = shift;

#    &log(TRACE, "Remote->output_read");

    my $strLine;
    my $strOutput;
    my $bError = false;
    my $iErrorCode;
    my $strError;

    if (waitpid($self->{pId}, WNOHANG) != 0)
    {
        print "process exited\n";
        
        my $strError = $self->pipe_to_string($self->{hErr});
        
        if (defined($strError))
        {
            $bError = true;
            $strOutput = $strError;
        }
    
        # Capture any errors
        if ($bError)
        {
#            print "error: " . $strOutput->message();
            
            confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}" : "") .
                                (defined($strOutput) ? ": ${strOutput}" : ""));
        }
    }

    # print "error read wait\n";
    # 
    # if (!eof($self->{hErr}))
    # {
    #     $strError = $self->pipe_to_string($self->{hErr});
    # 
    #     if (defined($strError))
    #     {
    #         $bError = true;
    #         $strOutput = $strError;
    #         $iErrorCode = undef;
    #     }
    # }

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

    # Capture any errors
    if ($bError)
    {
        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}" : "") .
                            (defined($strOutput) ? ": ${strOutput}" : ""), $iErrorCode);
    }
    
    if ($bOutputRequired && !defined($strOutput))
    {
        my $strError = $self->pipe_to_string($self->{hErr});
        
        if (defined($strError))
        {
            $bError = true;
            $strOutput = $strError;
        }
    
        # Capture any errors
        if ($bError)
        {
            confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}" : "") .
                                (defined($strOutput) ? ": ${strOutput}" : ""));
        }

        confess &log(ERROR, (defined($strErrorPrefix) ? "${strErrorPrefix}: " : "") . "output is not defined");
    }

    return $strOutput;
}

####################################################################################################################################
# OUTPUT_WRITE
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
####################################################################################################################################
sub command_read
{
    my $self = shift;
    my $oParamHashRef = shift;

#    &log(TRACE, "Remote->command_read");

    my $strLine;
    my $strCommand;

    while ($strLine = $self->read_line(*STDIN))
    {
#        $strLine = trim($strLine);

        if (!defined($strCommand))
        {
            if ($strLine =~ /:$/)
            {
                $strCommand = substr($strLine, 0, length($strLine) - 1);
#                print "command ${strCommand} continues\n";
            }
            else
            {
                $strCommand = $strLine;
#                print "command ${strCommand}\n";
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

#            print "${strParam}=${strValue}\n";
        }
    }

    return $strCommand;
}

####################################################################################################################################
# COMMAND_WRITE
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
